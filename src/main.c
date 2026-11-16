#include "config.h"
#include "i18n.h"
#include "manager_dlg.h"
#include "resource.h"
#include "theme.h"
#include "tray_menu.h"
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#define QUICKTRAY_WND_CLASS     L"QuickTrayHiddenHostWndClass"
#define QUICKTRAY_MUTEX_NAME    L"Local\\QuickTray_SingleInstance_Mutex_v1"
#define TRAY_ICON_ID            1

static HINSTANCE   g_hInstance          = NULL;
static HICON       g_hTrayIcon          = NULL;
static MENU_CONFIG g_cfg;
static UINT        g_uTaskbarCreatedMsg = 0;
static UINT        g_uShowMenuMsg       = 0;
static BOOL        g_bMenuOpen          = FALSE;

static BOOL AddTrayIcon(HWND hWnd)
{
    NOTIFYICONDATAW nid;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize           = sizeof(NOTIFYICONDATAW);
    nid.hWnd             = hWnd;
    nid.uID              = TRAY_ICON_ID;
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = g_hTrayIcon;
    lstrcpynW(nid.szTip, I18nGetString(STR_TRAY_TOOLTIP), ARRAYSIZE(nid.szTip));

    return Shell_NotifyIconW(NIM_ADD, &nid);
}

static VOID RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATAW nid;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd   = hWnd;
    nid.uID    = TRAY_ICON_ID;

    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static VOID OpenQuickMenu(HWND hWnd)
{
    UINT uCmd;

    if (ManagerDialogIsOpen()) {
        ManagerDialogBringToFront();
        return;
    }

    if (g_bMenuOpen) {
        return;
    }

    g_bMenuOpen = TRUE;
    uCmd = TrayMenuShow(hWnd, &g_cfg);
    g_bMenuOpen = FALSE;

    if (uCmd == IDM_TRAY_MANAGE) {
        ManagerDialogShow(hWnd, g_hInstance, &g_cfg);
    } else if (uCmd == IDM_TRAY_EXIT) {
        DestroyWindow(hWnd);
    } else if (uCmd >= IDM_DYNAMIC_BASE) {
        DWORD dwIdx = uCmd - IDM_DYNAMIC_BASE;
        if (dwIdx < g_cfg.dwCount) {
            TrayMenuExecuteItem(hWnd, &g_cfg.pItems[dwIdx]);
        }
    }
}

static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == g_uTaskbarCreatedMsg && g_uTaskbarCreatedMsg != 0) {
        AddTrayIcon(hWnd);
        return 0;
    }

    if (uMsg == g_uShowMenuMsg && g_uShowMenuMsg != 0) {
        OpenQuickMenu(hWnd);
        return 0;
    }

    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
                OpenQuickMenu(hWnd);
            }
            return 0;

        case WM_SETTINGCHANGE:
            ThemeRefresh();
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon(hWnd);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
    HANDLE hMutex;
    WNDCLASSEXW wc;
    HWND hWnd;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    INT cxSm;
    INT cySm;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    g_hInstance = hInstance;
    g_uShowMenuMsg = RegisterWindowMessageW(L"QuickTray_ShowMenu_Msg");

    /* Enforce single instance and trigger existing instance's menu if already running */
    hMutex = CreateMutexW(NULL, TRUE, QUICKTRAY_MUTEX_NAME);
    if (hMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(QUICKTRAY_WND_CLASS, NULL);
        if (hExisting != NULL && g_uShowMenuMsg != 0) {
            PostMessageW(hExisting, g_uShowMenuMsg, 0, 0);
        }
        CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    ZeroMemory(&icc, sizeof(icc));
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC  = ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    I18nInit();
    ThemeInit();
    ConfigInit(&g_cfg);
    ConfigLoad(&g_cfg);

    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    cxSm = GetSystemMetrics(SM_CXSMICON);
    cySm = GetSystemMetrics(SM_CYSMICON);
    g_hTrayIcon = (HICON)LoadImageW(
        hInstance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        cxSm,
        cySm,
        LR_DEFAULTCOLOR
    );
    if (g_hTrayIcon == NULL) {
        g_hTrayIcon = LoadIconW(NULL, IDI_APPLICATION);
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = TrayWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = QUICKTRAY_WND_CLASS;

    if (!RegisterClassExW(&wc)) {
        if (hMutex != NULL) CloseHandle(hMutex);
        ConfigFree(&g_cfg);
        ThemeCleanup();
        CoUninitialize();
        return 1;
    }

    /* Use a hidden top-level popup window so broadcast messages (WM_SETTINGCHANGE / TaskbarCreated) arrive */
    hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        QUICKTRAY_WND_CLASS,
        I18nGetString(STR_APP_TITLE),
        WS_POPUP,
        0, 0, 0, 0,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hWnd == NULL) {
        if (hMutex != NULL) CloseHandle(hMutex);
        ConfigFree(&g_cfg);
        ThemeCleanup();
        CoUninitialize();
        return 1;
    }

    AddTrayIcon(hWnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hTrayIcon != NULL) {
        DestroyIcon(g_hTrayIcon);
        g_hTrayIcon = NULL;
    }

    ConfigFree(&g_cfg);
    ThemeCleanup();
    CoUninitialize();

    if (hMutex != NULL) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return (INT)msg.wParam;
}
