#include "theme.h"
#include <commctrl.h>
#include <uxtheme.h>

/* PreferredAppMode enum values used by uxtheme.dll ordinal 135 on Windows 10 1903+ */
typedef enum tagPREFERRED_APP_MODE {
    PAM_DEFAULT    = 0,
    PAM_ALLOW_DARK = 1,
    PAM_FORCE_DARK = 2,
    PAM_FORCE_LIGHT = 3
} PREFERRED_APP_MODE;

typedef VOID (WINAPI *PFN_RTL_GET_NT_VERSION_NUMBERS)(LPDWORD pMajor, LPDWORD pMinor, LPDWORD pBuild);
typedef BOOL (WINAPI *PFN_ALLOW_DARK_MODE_FOR_APP)(BOOL bAllow);
typedef INT  (WINAPI *PFN_SET_PREFERRED_APP_MODE)(INT nMode);
typedef BOOL (WINAPI *PFN_ALLOW_DARK_MODE_FOR_WINDOW)(HWND hWnd, BOOL bAllow);
typedef VOID (WINAPI *PFN_FLUSH_MENU_THEMES)(VOID);
typedef HRESULT (WINAPI *PFN_DWM_SET_WINDOW_ATTRIBUTE)(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

static BOOL    g_bDarkMode     = FALSE;
static DWORD   g_dwBuildNumber = 0;
static HMODULE g_hUxTheme      = NULL;
static HMODULE g_hDwmapi       = NULL;
static HBRUSH  g_hBrushDlgBg   = NULL;
static HBRUSH  g_hBrushEditBg  = NULL;

static PFN_ALLOW_DARK_MODE_FOR_APP    g_pfnAllowDarkForApp    = NULL;
static PFN_SET_PREFERRED_APP_MODE     g_pfnSetPreferredAppMode = NULL;
static PFN_ALLOW_DARK_MODE_FOR_WINDOW g_pfnAllowDarkForWindow = NULL;
static PFN_FLUSH_MENU_THEMES          g_pfnFlushMenuThemes    = NULL;
static PFN_DWM_SET_WINDOW_ATTRIBUTE   g_pfnDwmSetWindowAttr   = NULL;

/* Dark theme color palette */
#define DARK_COLOR_DLG_BG   RGB(32, 32, 32)
#define DARK_COLOR_EDIT_BG  RGB(45, 45, 45)
#define DARK_COLOR_TEXT     RGB(240, 240, 240)
#define DARK_COLOR_TREE_LN  RGB(90, 90, 90)

/* DWMWA_USE_IMMERSIVE_DARK_MODE attributes for Windows 10/11 */
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static BOOL QuerySystemDarkMode(VOID)
{
    HKEY hKey = NULL;
    DWORD dwLightTheme = 1;
    DWORD dwSize = sizeof(DWORD);
    DWORD dwType = REG_DWORD;
    LONG lResult;

    lResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_READ,
        &hKey
    );

    if (lResult == ERROR_SUCCESS) {
        lResult = RegQueryValueExW(
            hKey,
            L"AppsUseLightTheme",
            NULL,
            &dwType,
            (LPBYTE)&dwLightTheme,
            &dwSize
        );
        RegCloseKey(hKey);
        if (lResult == ERROR_SUCCESS && dwType == REG_DWORD) {
            return (dwLightTheme == 0) ? TRUE : FALSE;
        }
    }

    return FALSE;
}

static VOID ApplyGlobalMenuTheme(VOID)
{
    if (g_dwBuildNumber >= 18362 && g_pfnSetPreferredAppMode != NULL) {
        g_pfnSetPreferredAppMode(g_bDarkMode ? PAM_FORCE_DARK : PAM_FORCE_LIGHT);
    } else if (g_dwBuildNumber >= 17763 && g_pfnAllowDarkForApp != NULL) {
        g_pfnAllowDarkForApp(g_bDarkMode);
    }

    if (g_pfnFlushMenuThemes != NULL) {
        g_pfnFlushMenuThemes();
    }
}

static BOOL CALLBACK ThemeChildEnumProc(HWND hCtrl, LPARAM lParam)
{
    WCHAR szClass[64] = { 0 };
    BOOL bDark = (BOOL)lParam;

    GetClassNameW(hCtrl, szClass, ARRAYSIZE(szClass));

    if (g_pfnAllowDarkForWindow != NULL) {
        g_pfnAllowDarkForWindow(hCtrl, bDark);
    }

    if (lstrcmpiW(szClass, L"ComboBox") == 0) {
        SetWindowTheme(hCtrl, bDark ? L"DarkMode_CFD" : L"Explorer", NULL);
    } else if (lstrcmpiW(szClass, L"SysTreeView32") == 0) {
        ThemeApplyToTreeView(hCtrl);
    } else if (lstrcmpiW(szClass, L"Edit") == 0) {
        SetWindowTheme(hCtrl, bDark ? L"DarkMode_CFD" : L"Explorer", NULL);
    } else {
        SetWindowTheme(hCtrl, bDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    }

    InvalidateRect(hCtrl, NULL, TRUE);
    return TRUE;
}

VOID ThemeInit(VOID)
{
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (hNtDll != NULL) {
        PFN_RTL_GET_NT_VERSION_NUMBERS pfnGetVer =
            (PFN_RTL_GET_NT_VERSION_NUMBERS)GetProcAddress(hNtDll, "RtlGetNtVersionNumbers");
        if (pfnGetVer != NULL) {
            DWORD dwMajor = 0, dwMinor = 0, dwBuild = 0;
            pfnGetVer(&dwMajor, &dwMinor, &dwBuild);
            g_dwBuildNumber = dwBuild & ~0xF0000000;
        }
    }

    g_hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (g_hUxTheme != NULL) {
        g_pfnAllowDarkForWindow = (PFN_ALLOW_DARK_MODE_FOR_WINDOW)GetProcAddress(g_hUxTheme, MAKEINTRESOURCEA(133));
        if (g_dwBuildNumber >= 18362) {
            g_pfnSetPreferredAppMode = (PFN_SET_PREFERRED_APP_MODE)GetProcAddress(g_hUxTheme, MAKEINTRESOURCEA(135));
        } else {
            g_pfnAllowDarkForApp = (PFN_ALLOW_DARK_MODE_FOR_APP)GetProcAddress(g_hUxTheme, MAKEINTRESOURCEA(135));
        }
        g_pfnFlushMenuThemes = (PFN_FLUSH_MENU_THEMES)GetProcAddress(g_hUxTheme, MAKEINTRESOURCEA(136));
    }

    g_hDwmapi = LoadLibraryW(L"dwmapi.dll");
    if (g_hDwmapi != NULL) {
        g_pfnDwmSetWindowAttr = (PFN_DWM_SET_WINDOW_ATTRIBUTE)GetProcAddress(g_hDwmapi, "DwmSetWindowAttribute");
    }

    g_hBrushDlgBg  = CreateSolidBrush(DARK_COLOR_DLG_BG);
    g_hBrushEditBg = CreateSolidBrush(DARK_COLOR_EDIT_BG);

    g_bDarkMode = QuerySystemDarkMode();
    ApplyGlobalMenuTheme();
}

VOID ThemeCleanup(VOID)
{
    if (g_hBrushDlgBg != NULL) {
        DeleteObject(g_hBrushDlgBg);
        g_hBrushDlgBg = NULL;
    }
    if (g_hBrushEditBg != NULL) {
        DeleteObject(g_hBrushEditBg);
        g_hBrushEditBg = NULL;
    }
    if (g_hDwmapi != NULL) {
        FreeLibrary(g_hDwmapi);
        g_hDwmapi = NULL;
    }
    if (g_hUxTheme != NULL) {
        FreeLibrary(g_hUxTheme);
        g_hUxTheme = NULL;
    }
}

BOOL ThemeRefresh(VOID)
{
    BOOL bNewDark = QuerySystemDarkMode();
    BOOL bChanged = (bNewDark != g_bDarkMode);
    g_bDarkMode = bNewDark;
    ApplyGlobalMenuTheme();
    return bChanged;
}

BOOL ThemeIsDark(VOID)
{
    return g_bDarkMode;
}

VOID ThemeApplyToTreeView(HWND hTree)
{
    if (hTree == NULL) {
        return;
    }

    if (g_pfnAllowDarkForWindow != NULL) {
        g_pfnAllowDarkForWindow(hTree, g_bDarkMode);
    }

    SetWindowTheme(hTree, g_bDarkMode ? L"DarkMode_Explorer" : L"Explorer", NULL);

    if (g_bDarkMode) {
        TreeView_SetBkColor(hTree, DARK_COLOR_DLG_BG);
        TreeView_SetTextColor(hTree, DARK_COLOR_TEXT);
        TreeView_SetLineColor(hTree, DARK_COLOR_TREE_LN);
    } else {
        TreeView_SetBkColor(hTree, CLR_DEFAULT);
        TreeView_SetTextColor(hTree, CLR_DEFAULT);
        TreeView_SetLineColor(hTree, CLR_DEFAULT);
    }
}

VOID ThemeApplyToDialog(HWND hDlg)
{
    BOOL bDark = g_bDarkMode;

    if (hDlg == NULL) {
        return;
    }

    if (g_pfnAllowDarkForWindow != NULL) {
        g_pfnAllowDarkForWindow(hDlg, bDark);
    }

    if (g_pfnDwmSetWindowAttr != NULL) {
        HRESULT hr = g_pfnDwmSetWindowAttr(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &bDark, sizeof(bDark));
        if (FAILED(hr)) {
            g_pfnDwmSetWindowAttr(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &bDark, sizeof(bDark));
        }
    }

    EnumChildWindows(hDlg, ThemeChildEnumProc, (LPARAM)bDark);
    InvalidateRect(hDlg, NULL, TRUE);
}

INT_PTR ThemeHandleCtlColor(UINT uMsg, HDC hdc)
{
    if (!g_bDarkMode || hdc == NULL) {
        return (INT_PTR)FALSE;
    }

    switch (uMsg) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetTextColor(hdc, DARK_COLOR_TEXT);
            SetBkColor(hdc, DARK_COLOR_DLG_BG);
            return (INT_PTR)g_hBrushDlgBg;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            SetTextColor(hdc, DARK_COLOR_TEXT);
            SetBkColor(hdc, DARK_COLOR_EDIT_BG);
            return (INT_PTR)g_hBrushEditBg;

        default:
            return (INT_PTR)FALSE;
    }
}
