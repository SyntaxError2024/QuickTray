#include "tray_menu.h"
#include "i18n.h"
#include "resource.h"
#include <shellapi.h>
#include <shlwapi.h>

/* Pool of HBITMAP handles allocated for MENUITEMINFOW.hbmpItem during menu display */
typedef struct tagBITMAP_POOL {
    HBITMAP *pHandles;
    DWORD    dwCount;
    DWORD    dwCapacity;
} BITMAP_POOL;

static VOID BitmapPoolInit(BITMAP_POOL *pPool)
{
    pPool->pHandles = NULL;
    pPool->dwCount = 0;
    pPool->dwCapacity = 0;
}

static VOID BitmapPoolAdd(BITMAP_POOL *pPool, HBITMAP hBmp)
{
    HBITMAP *pNew;
    DWORD dwNewCap;

    if (hBmp == NULL) {
        return;
    }

    if (pPool->dwCount >= pPool->dwCapacity) {
        dwNewCap = (pPool->dwCapacity == 0) ? 32 : (pPool->dwCapacity * 2);
        if (pPool->pHandles == NULL) {
            pNew = (HBITMAP *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNewCap * sizeof(HBITMAP));
        } else {
            pNew = (HBITMAP *)HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pPool->pHandles, dwNewCap * sizeof(HBITMAP));
        }
        if (pNew == NULL) {
            DeleteObject(hBmp);
            return;
        }
        pPool->pHandles = pNew;
        pPool->dwCapacity = dwNewCap;
    }

    pPool->pHandles[pPool->dwCount++] = hBmp;
}

static VOID BitmapPoolDestroy(BITMAP_POOL *pPool)
{
    DWORD i;
    if (pPool->pHandles != NULL) {
        for (i = 0; i < pPool->dwCount; i++) {
            if (pPool->pHandles[i] != NULL) {
                DeleteObject(pPool->pHandles[i]);
            }
        }
        HeapFree(GetProcessHeap(), 0, pPool->pHandles);
    }
    BitmapPoolInit(pPool);
}

/* Converts an HICON into a 32-bit premultiplied ARGB HBITMAP suitable for HMENU */
static HBITMAP IconToMenuBitmap(HICON hIcon)
{
    INT cx = GetSystemMetrics(SM_CXSMICON);
    INT cy = GetSystemMetrics(SM_CYSMICON);
    BITMAPINFO bmi;
    LPDWORD pBits = NULL;
    HDC hdcScreen;
    HDC hdcMem;
    HBITMAP hBmp;
    HGDIOBJ hOldBmp;
    DWORD i;
    DWORD dwPixels;
    BOOL bHasAlpha = FALSE;

    if (hIcon == NULL || cx <= 0 || cy <= 0) {
        return NULL;
    }

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = cx;
    bmi.bmiHeader.biHeight      = -cy; /* Top-down DIB */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hdcScreen = GetDC(NULL);
    hdcMem = CreateCompatibleDC(hdcScreen);
    hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (VOID **)&pBits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);

    if (hBmp == NULL || hdcMem == NULL || pBits == NULL) {
        if (hBmp != NULL) DeleteObject(hBmp);
        if (hdcMem != NULL) DeleteDC(hdcMem);
        return NULL;
    }

    dwPixels = (DWORD)(cx * cy);
    ZeroMemory(pBits, dwPixels * sizeof(DWORD));

    hOldBmp = SelectObject(hdcMem, hBmp);
    DrawIconEx(hdcMem, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);

    /* Ensure legacy 1-bit masked icons without alpha channel stay opaque on RGB pixels */
    for (i = 0; i < dwPixels; i++) {
        if ((pBits[i] & 0xFF000000U) != 0) {
            bHasAlpha = TRUE;
            break;
        }
    }

    if (!bHasAlpha) {
        for (i = 0; i < dwPixels; i++) {
            if ((pBits[i] & 0x00FFFFFFU) != 0) {
                pBits[i] |= 0xFF000000U;
            }
        }
    }

    return hBmp;
}

static VOID ExpandPathW(LPCWSTR pszIn, LPWSTR pszOut, DWORD cchOut)
{
    DWORD dwRes;
    if (pszIn == NULL || pszIn[0] == L'\0') {
        if (cchOut > 0) pszOut[0] = L'\0';
        return;
    }
    dwRes = ExpandEnvironmentStringsW(pszIn, pszOut, cchOut);
    if (dwRes == 0 || dwRes > cchOut) {
        lstrcpynW(pszOut, pszIn, (INT)cchOut);
    }
}

static VOID EscapeMenuAmpersands(LPCWSTR pszIn, LPWSTR pszOut, DWORD cchOut)
{
    DWORD i = 0;
    if (cchOut == 0) {
        return;
    }
    if (pszIn == NULL) {
        pszOut[0] = L'\0';
        return;
    }

    while (*pszIn != L'\0' && i + 1 < cchOut) {
        if (*pszIn == L'&') {
            if (i + 2 < cchOut) {
                pszOut[i++] = L'&';
                pszOut[i++] = L'&';
            } else {
                break;
            }
        } else {
            pszOut[i++] = *pszIn;
        }
        pszIn++;
    }
    pszOut[i] = L'\0';
}

HICON TrayMenuExtractEntryIcon(const MENU_ENTRY *pEntry)
{
    SHFILEINFOW sfi;
    WCHAR szExpanded[MAX_PATH] = { 0 };

    if (pEntry == NULL) {
        return NULL;
    }

    ZeroMemory(&sfi, sizeof(sfi));

    if (pEntry->eType == ENTRY_SUBMENU) {
        if (SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
            return sfi.hIcon;
        }
        return NULL;
    }

    if (pEntry->eType == ENTRY_FOLDER) {
        ExpandPathW(pEntry->szTarget, szExpanded, MAX_PATH);
        if (szExpanded[0] != L'\0' &&
            SHGetFileInfoW(szExpanded, FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_SMALLICON)) {
            return sfi.hIcon;
        }
        if (SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
            return sfi.hIcon;
        }
        return NULL;
    }

    if (pEntry->eType == ENTRY_APP) {
        ExpandPathW(pEntry->szTarget, szExpanded, MAX_PATH);
        if (szExpanded[0] != L'\0' &&
            SHGetFileInfoW(szExpanded, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_SMALLICON)) {
            return sfi.hIcon;
        }
        if (SHGetFileInfoW(L"app.exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
            return sfi.hIcon;
        }
    }

    return NULL;
}

static VOID PopulateMenuLevel(
    HMENU hMenu,
    const MENU_CONFIG *pCfg,
    DWORD dwParentId,
    BITMAP_POOL *pPool,
    DWORD dwDepth
)
{
    DWORD i;
    DWORD dwAdded = 0;

    /* Guard against deep or malformed nesting */
    if (dwDepth > 16) {
        return;
    }

    for (i = 0; i < pCfg->dwCount; i++) {
        const MENU_ENTRY *pEntry = &pCfg->pItems[i];
        MENUITEMINFOW mii;
        WCHAR szSafeLabel[MAX_LABEL_LEN * 2];

        if (pEntry->dwParentId != dwParentId) {
            continue;
        }

        ZeroMemory(&mii, sizeof(mii));
        mii.cbSize = sizeof(MENUITEMINFOW);

        if (pEntry->eType == ENTRY_SEPARATOR) {
            mii.fMask = MIIM_FTYPE;
            mii.fType = MFT_SEPARATOR;
            InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
            dwAdded++;
            continue;
        }

        EscapeMenuAmpersands(pEntry->szLabel, szSafeLabel, ARRAYSIZE(szSafeLabel));

        if (pEntry->eType == ENTRY_SUBMENU) {
            HMENU hSub = CreatePopupMenu();
            HICON hIcon = TrayMenuExtractEntryIcon(pEntry);
            HBITMAP hBmp = NULL;

            if (hIcon != NULL) {
                hBmp = IconToMenuBitmap(hIcon);
                DestroyIcon(hIcon);
                BitmapPoolAdd(pPool, hBmp);
            }

            PopulateMenuLevel(hSub, pCfg, pEntry->dwId, pPool, dwDepth + 1);

            mii.fMask = MIIM_STRING | MIIM_SUBMENU;
            mii.hSubMenu = hSub;
            mii.dwTypeData = szSafeLabel;
            if (hBmp != NULL) {
                mii.fMask |= MIIM_BITMAP;
                mii.hbmpItem = hBmp;
            }
            InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
            dwAdded++;
        } else {
            HICON hIcon = TrayMenuExtractEntryIcon(pEntry);
            HBITMAP hBmp = NULL;

            if (hIcon != NULL) {
                hBmp = IconToMenuBitmap(hIcon);
                DestroyIcon(hIcon);
                BitmapPoolAdd(pPool, hBmp);
            }

            mii.fMask = MIIM_ID | MIIM_STRING;
            mii.wID = IDM_DYNAMIC_BASE + i;
            mii.dwTypeData = szSafeLabel;
            if (hBmp != NULL) {
                mii.fMask |= MIIM_BITMAP;
                mii.hbmpItem = hBmp;
            }
            InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
            dwAdded++;
        }
    }

    if (dwAdded == 0 && dwParentId != 0) {
        MENUITEMINFOW miiEmpty;
        WCHAR szEmpty[MAX_LABEL_LEN];
        lstrcpynW(szEmpty, I18nGetString(STR_MENU_EMPTY), MAX_LABEL_LEN);

        ZeroMemory(&miiEmpty, sizeof(miiEmpty));
        miiEmpty.cbSize = sizeof(MENUITEMINFOW);
        miiEmpty.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
        miiEmpty.fState = MFS_DISABLED;
        miiEmpty.wID = IDM_TRAY_EMPTY;
        miiEmpty.dwTypeData = szEmpty;
        InsertMenuItemW(hMenu, 0, TRUE, &miiEmpty);
    }
}

UINT TrayMenuShow(HWND hOwnerWnd, const MENU_CONFIG *pCfg)
{
    HMENU hRootMenu;
    BITMAP_POOL pool;
    POINT pt;
    UINT uCmd = 0;
    WCHAR szManage[MAX_LABEL_LEN];
    WCHAR szExit[MAX_LABEL_LEN];
    MENUITEMINFOW mii;

    hRootMenu = CreatePopupMenu();
    if (hRootMenu == NULL) {
        return 0;
    }

    BitmapPoolInit(&pool);
    PopulateMenuLevel(hRootMenu, pCfg, 0, &pool, 0);

    if (GetMenuItemCount(hRootMenu) > 0) {
        ZeroMemory(&mii, sizeof(mii));
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_FTYPE;
        mii.fType = MFT_SEPARATOR;
        InsertMenuItemW(hRootMenu, GetMenuItemCount(hRootMenu), TRUE, &mii);
    }

    lstrcpynW(szManage, I18nGetString(STR_MENU_MANAGE), MAX_LABEL_LEN);
    ZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_ID | MIIM_STRING;
    mii.wID = IDM_TRAY_MANAGE;
    mii.dwTypeData = szManage;
    InsertMenuItemW(hRootMenu, GetMenuItemCount(hRootMenu), TRUE, &mii);

    lstrcpynW(szExit, I18nGetString(STR_MENU_EXIT), MAX_LABEL_LEN);
    ZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_ID | MIIM_STRING;
    mii.wID = IDM_TRAY_EXIT;
    mii.dwTypeData = szExit;
    InsertMenuItemW(hRootMenu, GetMenuItemCount(hRootMenu), TRUE, &mii);

    GetCursorPos(&pt);

    /* Microsoft KB135788: ensure foreground focus so clicking outside dismisses the menu */
    SetForegroundWindow(hOwnerWnd);
    uCmd = (UINT)TrackPopupMenuEx(
        hRootMenu,
        TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x,
        pt.y,
        hOwnerWnd,
        NULL
    );
    PostMessageW(hOwnerWnd, WM_NULL, 0, 0);

    DestroyMenu(hRootMenu);
    BitmapPoolDestroy(&pool);

    return uCmd;
}

VOID TrayMenuExecuteItem(HWND hOwnerWnd, const MENU_ENTRY *pEntry)
{
    WCHAR szExpandedTarget[MAX_PATH] = { 0 };
    WCHAR szExpandedArgs[MAX_ARGS_LEN] = { 0 };
    WCHAR szWorkDir[MAX_PATH] = { 0 };
    LPCWSTR pszArgs = NULL;
    LPCWSTR pszWorkDir = NULL;
    HINSTANCE hRes;

    if (pEntry == NULL) {
        return;
    }

    ExpandPathW(pEntry->szTarget, szExpandedTarget, MAX_PATH);
    if (szExpandedTarget[0] == L'\0') {
        return;
    }

    if (pEntry->eType == ENTRY_APP) {
        ExpandPathW(pEntry->szArgs, szExpandedArgs, MAX_ARGS_LEN);
        if (szExpandedArgs[0] != L'\0') {
            pszArgs = szExpandedArgs;
        }

        lstrcpynW(szWorkDir, szExpandedTarget, MAX_PATH);
        if (PathRemoveFileSpecW(szWorkDir) && PathIsDirectoryW(szWorkDir)) {
            pszWorkDir = szWorkDir;
        }
    }

    hRes = ShellExecuteW(
        NULL,
        L"open",
        szExpandedTarget,
        pszArgs,
        pszWorkDir,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)hRes <= 32) {
        MessageBoxW(
            hOwnerWnd,
            I18nGetString(STR_ERR_LAUNCH_FAILED),
            I18nGetString(STR_APP_TITLE),
            MB_OK | MB_ICONERROR
        );
    }
}
