#include "manager_dlg.h"
#include "i18n.h"
#include "resource.h"
#include "theme.h"
#include "tray_menu.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>

typedef struct tagMANAGER_STATE {
    HINSTANCE   hInstance;
    MENU_CONFIG workingCfg;
    HIMAGELIST  hImageList;
    DWORD       dwSelectedId;
} MANAGER_STATE;

typedef struct tagEDIT_STATE {
    PMENU_CONFIG pCfg;
    BOOL         bIsNew;
    DWORD        dwEditId;
    ENTRY_TYPE   eType;
    DWORD        dwParentId;
    WCHAR        szLabel[MAX_LABEL_LEN];
    WCHAR        szTarget[MAX_PATH];
    WCHAR        szArgs[MAX_ARGS_LEN];
} EDIT_STATE;

static HWND g_hActiveManageDlg = NULL;

static VOID TrimEditStringW(LPWSTR pszText)
{
    SIZE_T len;
    SIZE_T start = 0;

    if (pszText == NULL || pszText[0] == L'\0') {
        return;
    }

    len = lstrlenW(pszText);
    while (start < len && (pszText[start] == L' ' || pszText[start] == L'\t')) {
        start++;
    }
    while (len > start && (pszText[len - 1] == L' ' || pszText[len - 1] == L'\t')) {
        len--;
    }

    if (start > 0 && len > start) {
        MoveMemory(pszText, pszText + start, (len - start) * sizeof(WCHAR));
    }
    pszText[len - start] = L'\0';
}

/* Creates a subtle 16x16 horizontal line icon used for separator entries in the TreeView */
static HICON CreateSeparatorIcon(INT cx, INT cy)
{
    BITMAPINFO bmi;
    LPDWORD pBits = NULL;
    HDC hdcScreen;
    HDC hdcMem;
    HBITMAP hColor;
    HBITMAP hMask;
    ICONINFO ii;
    HICON hIcon;
    INT yMid = cy / 2;
    INT x;

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = cx;
    bmi.bmiHeader.biHeight      = -cy;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hdcScreen = GetDC(NULL);
    hdcMem = CreateCompatibleDC(hdcScreen);
    hColor = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (VOID **)&pBits, NULL, 0);
    hMask = CreateBitmap(cx, cy, 1, 1, NULL);
    ReleaseDC(NULL, hdcScreen);

    if (hColor == NULL || hMask == NULL || pBits == NULL) {
        if (hColor != NULL) DeleteObject(hColor);
        if (hMask != NULL) DeleteObject(hMask);
        if (hdcMem != NULL) DeleteDC(hdcMem);
        return NULL;
    }

    ZeroMemory(pBits, (SIZE_T)(cx * cy) * sizeof(DWORD));
    for (x = 2; x < cx - 2; x++) {
        pBits[yMid * cx + x] = 0xFF888888U;
    }

    DeleteDC(hdcMem);

    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon    = TRUE;
    ii.hbmMask  = hMask;
    ii.hbmColor = hColor;
    hIcon = CreateIconIndirect(&ii);

    DeleteObject(hColor);
    DeleteObject(hMask);
    return hIcon;
}

static BOOL BrowseForFolderModern(HWND hOwner, LPWSTR pszOutPath, DWORD cchOutPath)
{
    IFileOpenDialog *pfd = NULL;
    HRESULT hr;
    BOOL bPicked = FALSE;

    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (VOID **)&pfd);
    if (SUCCEEDED(hr) && pfd != NULL) {
        DWORD dwOptions = 0;
        pfd->lpVtbl->GetOptions(pfd, &dwOptions);
        pfd->lpVtbl->SetOptions(pfd, dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        pfd->lpVtbl->SetTitle(pfd, I18nGetString(STR_BROWSE_FOLDER_TITLE));

        if (SUCCEEDED(pfd->lpVtbl->Show(pfd, hOwner))) {
            IShellItem *psi = NULL;
            if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd, &psi)) && psi != NULL) {
                LPWSTR pszPath = NULL;
                if (SUCCEEDED(psi->lpVtbl->GetDisplayName(psi, SIGDN_FILESYSPATH, &pszPath)) && pszPath != NULL) {
                    lstrcpynW(pszOutPath, pszPath, (INT)cchOutPath);
                    CoTaskMemFree(pszPath);
                    bPicked = TRUE;
                }
                psi->lpVtbl->Release(psi);
            }
        }
        pfd->lpVtbl->Release(pfd);
        return bPicked;
    }

    /* Classic fallback if IFileOpenDialog is unavailable */
    {
        BROWSEINFOW bi;
        LPITEMIDLIST pidl;
        WCHAR szDisplay[MAX_PATH] = { 0 };

        ZeroMemory(&bi, sizeof(bi));
        bi.hwndOwner = hOwner;
        bi.pszDisplayName = szDisplay;
        bi.lpszTitle = I18nGetString(STR_BROWSE_FOLDER_TITLE);
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;

        pidl = SHBrowseForFolderW(&bi);
        if (pidl != NULL) {
            if (SHGetPathFromIDListW(pidl, pszOutPath)) {
                bPicked = TRUE;
            }
            CoTaskMemFree(pidl);
        }
    }

    return bPicked;
}

static BOOL BrowseForAppFile(HWND hOwner, LPWSTR pszOutPath, DWORD cchOutPath)
{
    OPENFILENAMEW ofn;
    WCHAR szFile[MAX_PATH] = { 0 };

    if (pszOutPath[0] != L'\0') {
        lstrcpynW(szFile, pszOutPath, MAX_PATH);
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner   = hOwner;
    ofn.lpstrFilter = I18nGetString(STR_BROWSE_FILTER_APPS);
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = I18nGetString(STR_BROWSE_APP_TITLE);
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        lstrcpynW(pszOutPath, szFile, (INT)cchOutPath);
        return TRUE;
    }
    return FALSE;
}

static VOID EditDlgUpdateFieldStates(HWND hDlg)
{
    INT nSel = (INT)SendDlgItemMessageW(hDlg, IDC_COMBO_TYPE, CB_GETCURSEL, 0, 0);
    ENTRY_TYPE eType = (ENTRY_TYPE)SendDlgItemMessageW(hDlg, IDC_COMBO_TYPE, CB_GETITEMDATA, (WPARAM)nSel, 0);
    BOOL bNeedsTarget = (eType == ENTRY_APP || eType == ENTRY_FOLDER);
    BOOL bIsApp = (eType == ENTRY_APP);

    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_TARGET), bNeedsTarget);
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_BROWSE), bNeedsTarget);
    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_ARGS), bIsApp);
}

static VOID EditDlgPopulateParents(HWND hDlg, const EDIT_STATE *pState)
{
    HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_PARENT);
    INT nIdx;
    INT nSelectIdx = 0;
    DWORD i;

    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);

    nIdx = (INT)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)I18nGetString(STR_ROOT_MENU));
    SendMessageW(hCombo, CB_SETITEMDATA, (WPARAM)nIdx, (LPARAM)0);

    for (i = 0; i < pState->pCfg->dwCount; i++) {
        const MENU_ENTRY *pItem = &pState->pCfg->pItems[i];
        if (pItem->eType != ENTRY_SUBMENU) {
            continue;
        }
        /* Prevent circular submenu nesting */
        if (!pState->bIsNew && ConfigIsDescendant(pState->pCfg, pItem->dwId, pState->dwEditId)) {
            continue;
        }
        nIdx = (INT)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)pItem->szLabel);
        SendMessageW(hCombo, CB_SETITEMDATA, (WPARAM)nIdx, (LPARAM)pItem->dwId);
        if (pItem->dwId == pState->dwParentId) {
            nSelectIdx = nIdx;
        }
    }

    SendMessageW(hCombo, CB_SETCURSEL, (WPARAM)nSelectIdx, 0);
}

static INT_PTR CALLBACK EditDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    EDIT_STATE *pState = (EDIT_STATE *)GetWindowLongPtrW(hDlg, DWLP_USER);

    switch (uMsg) {
        case WM_INITDIALOG: {
            HWND hTypeCombo = GetDlgItem(hDlg, IDC_COMBO_TYPE);
            INT nIdx;
            INT nSelType = 0;

            pState = (EDIT_STATE *)lParam;
            SetWindowLongPtrW(hDlg, DWLP_USER, (LONG_PTR)pState);

            SetWindowTextW(hDlg, I18nGetString(pState->bIsNew ? STR_EDIT_TITLE_ADD : STR_EDIT_TITLE_EDIT));
            SetDlgItemTextW(hDlg, IDC_LBL_TYPE, I18nGetString(STR_LBL_TYPE));
            SetDlgItemTextW(hDlg, IDC_LBL_NAME, I18nGetString(STR_LBL_NAME));
            SetDlgItemTextW(hDlg, IDC_LBL_TARGET, I18nGetString(STR_LBL_TARGET));
            SetDlgItemTextW(hDlg, IDC_BTN_BROWSE, I18nGetString(STR_BTN_BROWSE));
            SetDlgItemTextW(hDlg, IDC_LBL_ARGS, I18nGetString(STR_LBL_ARGS));
            SetDlgItemTextW(hDlg, IDC_LBL_PARENT, I18nGetString(STR_LBL_PARENT));
            SetDlgItemTextW(hDlg, IDOK, I18nGetString(STR_BTN_OK));
            SetDlgItemTextW(hDlg, IDCANCEL, I18nGetString(STR_BTN_CANCEL));

            nIdx = (INT)SendMessageW(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)I18nGetString(STR_TYPE_APP));
            SendMessageW(hTypeCombo, CB_SETITEMDATA, (WPARAM)nIdx, (LPARAM)ENTRY_APP);
            if (pState->eType == ENTRY_APP) nSelType = nIdx;

            nIdx = (INT)SendMessageW(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)I18nGetString(STR_TYPE_FOLDER));
            SendMessageW(hTypeCombo, CB_SETITEMDATA, (WPARAM)nIdx, (LPARAM)ENTRY_FOLDER);
            if (pState->eType == ENTRY_FOLDER) nSelType = nIdx;

            nIdx = (INT)SendMessageW(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)I18nGetString(STR_TYPE_SUBMENU));
            SendMessageW(hTypeCombo, CB_SETITEMDATA, (WPARAM)nIdx, (LPARAM)ENTRY_SUBMENU);
            if (pState->eType == ENTRY_SUBMENU) nSelType = nIdx;

            SendMessageW(hTypeCombo, CB_SETCURSEL, (WPARAM)nSelType, 0);

            SendDlgItemMessageW(hDlg, IDC_EDIT_NAME, EM_SETLIMITTEXT, MAX_LABEL_LEN - 1, 0);
            SendDlgItemMessageW(hDlg, IDC_EDIT_TARGET, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
            SendDlgItemMessageW(hDlg, IDC_EDIT_ARGS, EM_SETLIMITTEXT, MAX_ARGS_LEN - 1, 0);

            SetDlgItemTextW(hDlg, IDC_EDIT_NAME, pState->szLabel);
            SetDlgItemTextW(hDlg, IDC_EDIT_TARGET, pState->szTarget);
            SetDlgItemTextW(hDlg, IDC_EDIT_ARGS, pState->szArgs);

            EditDlgPopulateParents(hDlg, pState);
            EditDlgUpdateFieldStates(hDlg);
            if (!pState->bIsNew && pState->eType == ENTRY_SUBMENU && ConfigHasChildren(pState->pCfg, pState->dwEditId)) {
                EnableWindow(hTypeCombo, FALSE);
            }
            ThemeApplyToDialog(hDlg);

            SetFocus(GetDlgItem(hDlg, IDC_EDIT_NAME));
            return FALSE;
        }

        case WM_SETTINGCHANGE:
            if (ThemeRefresh()) {
                ThemeApplyToDialog(hDlg);
            }
            return TRUE;

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN: {
            INT_PTR nRes = ThemeHandleCtlColor(uMsg, (HDC)wParam);
            if (nRes != 0) {
                return nRes;
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_COMBO_TYPE:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        EditDlgUpdateFieldStates(hDlg);
                    }
                    return TRUE;

                case IDC_BTN_BROWSE: {
                    INT nTypeSel = (INT)SendDlgItemMessageW(hDlg, IDC_COMBO_TYPE, CB_GETCURSEL, 0, 0);
                    ENTRY_TYPE eType = (ENTRY_TYPE)SendDlgItemMessageW(hDlg, IDC_COMBO_TYPE, CB_GETITEMDATA, (WPARAM)nTypeSel, 0);
                    WCHAR szPath[MAX_PATH] = { 0 };
                    WCHAR szName[MAX_LABEL_LEN] = { 0 };
                    BOOL bOk = FALSE;

                    GetDlgItemTextW(hDlg, IDC_EDIT_TARGET, szPath, MAX_PATH);

                    if (eType == ENTRY_APP) {
                        bOk = BrowseForAppFile(hDlg, szPath, MAX_PATH);
                    } else if (eType == ENTRY_FOLDER) {
                        bOk = BrowseForFolderModern(hDlg, szPath, MAX_PATH);
                    }

                    if (bOk) {
                        SetDlgItemTextW(hDlg, IDC_EDIT_TARGET, szPath);
                        GetDlgItemTextW(hDlg, IDC_EDIT_NAME, szName, MAX_LABEL_LEN);
                        TrimEditStringW(szName);
                        if (szName[0] == L'\0') {
                            WCHAR szAuto[MAX_PATH] = { 0 };
                            LPCWSTR pszLeaf;
                            lstrcpynW(szAuto, szPath, MAX_PATH);
                            PathRemoveBackslashW(szAuto);
                            pszLeaf = PathFindFileNameW(szAuto);
                            if (pszLeaf != NULL && pszLeaf[0] != L'\0') {
                                WCHAR szLabelBuf[MAX_LABEL_LEN];
                                lstrcpynW(szLabelBuf, pszLeaf, MAX_LABEL_LEN);
                                if (eType == ENTRY_APP) {
                                    PathRemoveExtensionW(szLabelBuf);
                                }
                                SetDlgItemTextW(hDlg, IDC_EDIT_NAME, szLabelBuf);
                            }
                        }
                    }
                    return TRUE;
                }

                case IDOK: {
                    INT nTypeSel = (INT)SendDlgItemMessageW(hDlg, IDC_COMBO_TYPE, CB_GETCURSEL, 0, 0);
                    INT nParentSel = (INT)SendDlgItemMessageW(hDlg, IDC_COMBO_PARENT, CB_GETCURSEL, 0, 0);
                    ENTRY_TYPE eType = (ENTRY_TYPE)SendDlgItemMessageW(hDlg, IDC_COMBO_TYPE, CB_GETITEMDATA, (WPARAM)nTypeSel, 0);
                    DWORD dwParentId = (DWORD)SendDlgItemMessageW(hDlg, IDC_COMBO_PARENT, CB_GETITEMDATA, (WPARAM)nParentSel, 0);
                    WCHAR szLabel[MAX_LABEL_LEN] = { 0 };
                    WCHAR szTarget[MAX_PATH] = { 0 };
                    WCHAR szArgs[MAX_ARGS_LEN] = { 0 };

                    GetDlgItemTextW(hDlg, IDC_EDIT_NAME, szLabel, MAX_LABEL_LEN);
                    GetDlgItemTextW(hDlg, IDC_EDIT_TARGET, szTarget, MAX_PATH);
                    GetDlgItemTextW(hDlg, IDC_EDIT_ARGS, szArgs, MAX_ARGS_LEN);

                    TrimEditStringW(szLabel);
                    TrimEditStringW(szTarget);
                    TrimEditStringW(szArgs);

                    if (szLabel[0] == L'\0') {
                        MessageBoxW(hDlg, I18nGetString(STR_ERR_NAME_REQUIRED), I18nGetString(STR_APP_TITLE), MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hDlg, IDC_EDIT_NAME));
                        return TRUE;
                    }

                    if ((eType == ENTRY_APP || eType == ENTRY_FOLDER) && szTarget[0] == L'\0') {
                        MessageBoxW(hDlg, I18nGetString(STR_ERR_TARGET_REQUIRED), I18nGetString(STR_APP_TITLE), MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hDlg, IDC_EDIT_TARGET));
                        return TRUE;
                    }

                    pState->eType = eType;
                    pState->dwParentId = dwParentId;
                    lstrcpynW(pState->szLabel, szLabel, MAX_LABEL_LEN);

                    if (eType == ENTRY_SUBMENU) {
                        pState->szTarget[0] = L'\0';
                        pState->szArgs[0] = L'\0';
                    } else if (eType == ENTRY_FOLDER) {
                        lstrcpynW(pState->szTarget, szTarget, MAX_PATH);
                        pState->szArgs[0] = L'\0';
                    } else {
                        lstrcpynW(pState->szTarget, szTarget, MAX_PATH);
                        lstrcpynW(pState->szArgs, szArgs, MAX_ARGS_LEN);
                    }

                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}

static BOOL CanMoveSibling(const MENU_CONFIG *pCfg, DWORD dwId, INT nDirection)
{
    INT nIdx = ConfigFindIndexById(pCfg, dwId);
    DWORD dwParentId;
    INT i;

    if (nIdx < 0) {
        return FALSE;
    }

    dwParentId = pCfg->pItems[nIdx].dwParentId;
    if (nDirection < 0) {
        for (i = nIdx - 1; i >= 0; i--) {
            if (pCfg->pItems[i].dwParentId == dwParentId) {
                return TRUE;
            }
        }
    } else {
        for (i = nIdx + 1; i < (INT)pCfg->dwCount; i++) {
            if (pCfg->pItems[i].dwParentId == dwParentId) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static VOID ManageDlgUpdateSelectionUI(HWND hDlg, MANAGER_STATE *pState)
{
    HWND hTree = GetDlgItem(hDlg, IDC_TREE_ITEMS);
    HTREEITEM hSel = TreeView_GetSelection(hTree);
    DWORD dwSelId = 0;
    INT nIdx = -1;
    WCHAR szPreview[768] = { 0 };

    if (hSel != NULL) {
        TVITEMW tvi;
        ZeroMemory(&tvi, sizeof(tvi));
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hSel;
        if (TreeView_GetItem(hTree, &tvi)) {
            dwSelId = (DWORD)tvi.lParam;
        }
    }

    pState->dwSelectedId = dwSelId;
    nIdx = ConfigFindIndexById(&pState->workingCfg, dwSelId);

    if (nIdx < 0) {
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_EDIT), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_UP), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_DOWN), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_DELETE), FALSE);
        SetDlgItemTextW(hDlg, IDC_LBL_PREVIEW, L"");
        return;
    }

    {
        const MENU_ENTRY *pItem = &pState->workingCfg.pItems[nIdx];
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_EDIT), (pItem->eType != ENTRY_SEPARATOR));
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_UP), CanMoveSibling(&pState->workingCfg, dwSelId, -1));
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_DOWN), CanMoveSibling(&pState->workingCfg, dwSelId, +1));
        EnableWindow(GetDlgItem(hDlg, IDC_BTN_DELETE), TRUE);

        if (pItem->eType == ENTRY_APP) {
            lstrcpynW(szPreview, I18nGetString(STR_PREVIEW_TARGET), ARRAYSIZE(szPreview));
            lstrcpynW(szPreview + lstrlenW(szPreview), pItem->szTarget, (INT)(ARRAYSIZE(szPreview) - lstrlenW(szPreview)));
            if (pItem->szArgs[0] != L'\0') {
                lstrcpynW(szPreview + lstrlenW(szPreview), L"\r\n", (INT)(ARRAYSIZE(szPreview) - lstrlenW(szPreview)));
                lstrcpynW(szPreview + lstrlenW(szPreview), I18nGetString(STR_PREVIEW_ARGS), (INT)(ARRAYSIZE(szPreview) - lstrlenW(szPreview)));
                lstrcpynW(szPreview + lstrlenW(szPreview), pItem->szArgs, (INT)(ARRAYSIZE(szPreview) - lstrlenW(szPreview)));
            }
        } else if (pItem->eType == ENTRY_FOLDER) {
            lstrcpynW(szPreview, I18nGetString(STR_PREVIEW_TARGET), ARRAYSIZE(szPreview));
            lstrcpynW(szPreview + lstrlenW(szPreview), pItem->szTarget, (INT)(ARRAYSIZE(szPreview) - lstrlenW(szPreview)));
        } else if (pItem->eType == ENTRY_SUBMENU) {
            lstrcpynW(szPreview, I18nGetString(STR_PREVIEW_SUBMENU), ARRAYSIZE(szPreview));
        } else {
            lstrcpynW(szPreview, I18nGetString(STR_PREVIEW_SEPARATOR), ARRAYSIZE(szPreview));
        }

        SetDlgItemTextW(hDlg, IDC_LBL_PREVIEW, szPreview);
    }
}

static VOID PopulateTreeLevel(
    HWND hTree,
    HIMAGELIST hImgList,
    const MENU_CONFIG *pCfg,
    DWORD dwParentId,
    HTREEITEM hParentNode,
    DWORD dwSelectId,
    HTREEITEM *phOutSelectNode,
    DWORD dwDepth
)
{
    DWORD i;

    if (dwDepth > 16) {
        return;
    }

    for (i = 0; i < pCfg->dwCount; i++) {
        const MENU_ENTRY *pEntry = &pCfg->pItems[i];
        TVINSERTSTRUCTW tvis;
        HTREEITEM hInserted;
        INT nImgIdx = 0;
        WCHAR szLabel[MAX_LABEL_LEN];

        if (pEntry->dwParentId != dwParentId) {
            continue;
        }

        if (pEntry->eType == ENTRY_SEPARATOR) {
            lstrcpynW(szLabel, I18nGetString(STR_SEP_DISPLAY), MAX_LABEL_LEN);
            nImgIdx = 0;
        } else {
            HICON hIcon = TrayMenuExtractEntryIcon(pEntry);
            lstrcpynW(szLabel, pEntry->szLabel, MAX_LABEL_LEN);
            if (hIcon != NULL) {
                nImgIdx = ImageList_AddIcon(hImgList, hIcon);
                DestroyIcon(hIcon);
                if (nImgIdx < 0) {
                    nImgIdx = 0;
                }
            }
        }

        ZeroMemory(&tvis, sizeof(tvis));
        tvis.hParent      = (hParentNode != NULL) ? hParentNode : TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask    = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        tvis.item.pszText = szLabel;
        tvis.item.iImage  = nImgIdx;
        tvis.item.iSelectedImage = nImgIdx;
        tvis.item.lParam  = (LPARAM)pEntry->dwId;

        hInserted = TreeView_InsertItem(hTree, &tvis);
        if (pEntry->dwId == dwSelectId && phOutSelectNode != NULL) {
            *phOutSelectNode = hInserted;
        }

        if (pEntry->eType == ENTRY_SUBMENU && hInserted != NULL) {
            PopulateTreeLevel(hTree, hImgList, pCfg, pEntry->dwId, hInserted, dwSelectId, phOutSelectNode, dwDepth + 1);
            TreeView_Expand(hTree, hInserted, TVE_EXPAND);
        }
    }
}

static VOID ManageDlgRebuildTree(HWND hDlg, MANAGER_STATE *pState, DWORD dwSelectId)
{
    HWND hTree = GetDlgItem(hDlg, IDC_TREE_ITEMS);
    INT cx = GetSystemMetrics(SM_CXSMICON);
    INT cy = GetSystemMetrics(SM_CYSMICON);
    HICON hSepIcon;
    HTREEITEM hToSelect = NULL;

    SendMessageW(hTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hTree);

    if (pState->hImageList != NULL) {
        ImageList_Destroy(pState->hImageList);
        pState->hImageList = NULL;
    }

    pState->hImageList = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, 16, 16);
    hSepIcon = CreateSeparatorIcon(cx, cy);
    if (hSepIcon != NULL) {
        ImageList_AddIcon(pState->hImageList, hSepIcon);
        DestroyIcon(hSepIcon);
    }

    TreeView_SetImageList(hTree, pState->hImageList, TVSIL_NORMAL);

    PopulateTreeLevel(hTree, pState->hImageList, &pState->workingCfg, 0, NULL, dwSelectId, &hToSelect, 0);

    SendMessageW(hTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hTree, NULL, TRUE);

    if (hToSelect != NULL) {
        TreeView_SelectItem(hTree, hToSelect);
    } else {
        HTREEITEM hFirst = TreeView_GetRoot(hTree);
        if (hFirst != NULL) {
            TreeView_SelectItem(hTree, hFirst);
        }
    }

    ManageDlgUpdateSelectionUI(hDlg, pState);
}

static DWORD GetContextualParentId(const MANAGER_STATE *pState)
{
    INT nIdx = ConfigFindIndexById(&pState->workingCfg, pState->dwSelectedId);
    if (nIdx < 0) {
        return 0;
    }
    if (pState->workingCfg.pItems[nIdx].eType == ENTRY_SUBMENU) {
        return pState->workingCfg.pItems[nIdx].dwId;
    }
    return pState->workingCfg.pItems[nIdx].dwParentId;
}

static VOID ManageDlgOpenAddDialog(HWND hDlg, MANAGER_STATE *pState, ENTRY_TYPE eInitialType)
{
    EDIT_STATE editState;
    ZeroMemory(&editState, sizeof(editState));
    editState.pCfg = &pState->workingCfg;
    editState.bIsNew = TRUE;
    editState.eType = eInitialType;
    editState.dwParentId = GetContextualParentId(pState);

    if (DialogBoxParamW(pState->hInstance, MAKEINTRESOURCEW(IDD_EDIT_DLG), hDlg, EditDlgProc, (LPARAM)&editState) == IDOK) {
        DWORD dwNewId = ConfigAddEntry(
            &pState->workingCfg,
            editState.eType,
            editState.dwParentId,
            editState.szLabel,
            editState.szTarget,
            editState.szArgs
        );
        ManageDlgRebuildTree(hDlg, pState, dwNewId);
    }
}

static VOID ManageDlgOpenEditSelected(HWND hDlg, MANAGER_STATE *pState)
{
    INT nIdx = ConfigFindIndexById(&pState->workingCfg, pState->dwSelectedId);
    EDIT_STATE editState;
    const MENU_ENTRY *pItem;

    if (nIdx < 0) {
        return;
    }

    pItem = &pState->workingCfg.pItems[nIdx];
    if (pItem->eType == ENTRY_SEPARATOR) {
        return;
    }

    ZeroMemory(&editState, sizeof(editState));
    editState.pCfg = &pState->workingCfg;
    editState.bIsNew = FALSE;
    editState.dwEditId = pItem->dwId;
    editState.eType = pItem->eType;
    editState.dwParentId = pItem->dwParentId;
    lstrcpynW(editState.szLabel, pItem->szLabel, MAX_LABEL_LEN);
    lstrcpynW(editState.szTarget, pItem->szTarget, MAX_PATH);
    lstrcpynW(editState.szArgs, pItem->szArgs, MAX_ARGS_LEN);

    if (DialogBoxParamW(pState->hInstance, MAKEINTRESOURCEW(IDD_EDIT_DLG), hDlg, EditDlgProc, (LPARAM)&editState) == IDOK) {
        /* Re-lookup in case anything shifted */
        INT nUpdateIdx = ConfigFindIndexById(&pState->workingCfg, editState.dwEditId);
        if (nUpdateIdx >= 0) {
            MENU_ENTRY *pDst = &pState->workingCfg.pItems[nUpdateIdx];
            pDst->eType = editState.eType;
            pDst->dwParentId = editState.dwParentId;
            lstrcpynW(pDst->szLabel, editState.szLabel, MAX_LABEL_LEN);
            lstrcpynW(pDst->szTarget, editState.szTarget, MAX_PATH);
            lstrcpynW(pDst->szArgs, editState.szArgs, MAX_ARGS_LEN);
        }
        ManageDlgRebuildTree(hDlg, pState, editState.dwEditId);
    }
}

static VOID ManageDlgAddSeparator(HWND hDlg, MANAGER_STATE *pState)
{
    DWORD dwParentId = GetContextualParentId(pState);
    DWORD dwAfterId = pState->dwSelectedId;
    DWORD dwNewId;
    INT nSelIdx = ConfigFindIndexById(&pState->workingCfg, dwAfterId);

    dwNewId = ConfigAddEntry(&pState->workingCfg, ENTRY_SEPARATOR, dwParentId, L"", L"", L"");
    if (dwNewId == 0) {
        return;
    }

    /* If a sibling in the same parent was selected, place separator right below it */
    if (nSelIdx >= 0 && pState->workingCfg.pItems[nSelIdx].dwParentId == dwParentId) {
        INT nNewIdx = ConfigFindIndexById(&pState->workingCfg, dwNewId);
        while (nNewIdx > nSelIdx + 1) {
            if (!ConfigMoveEntry(&pState->workingCfg, dwNewId, -1)) {
                break;
            }
            nNewIdx = ConfigFindIndexById(&pState->workingCfg, dwNewId);
        }
    }

    ManageDlgRebuildTree(hDlg, pState, dwNewId);
}

static INT_PTR CALLBACK ManageDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MANAGER_STATE *pState = (MANAGER_STATE *)GetWindowLongPtrW(hDlg, DWLP_USER);

    switch (uMsg) {
        case WM_INITDIALOG: {
            HICON hIcon;
            pState = (MANAGER_STATE *)lParam;
            SetWindowLongPtrW(hDlg, DWLP_USER, (LONG_PTR)pState);
            g_hActiveManageDlg = hDlg;

            hIcon = LoadIconW(pState->hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
            if (hIcon != NULL) {
                SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }

            SetWindowTextW(hDlg, I18nGetString(STR_MANAGE_TITLE));
            SetDlgItemTextW(hDlg, IDC_BTN_ADD_APP, I18nGetString(STR_BTN_ADD_APP));
            SetDlgItemTextW(hDlg, IDC_BTN_ADD_FOLDER, I18nGetString(STR_BTN_ADD_FOLDER));
            SetDlgItemTextW(hDlg, IDC_BTN_ADD_SUBMENU, I18nGetString(STR_BTN_ADD_SUBMENU));
            SetDlgItemTextW(hDlg, IDC_BTN_ADD_SEP, I18nGetString(STR_BTN_ADD_SEP));
            SetDlgItemTextW(hDlg, IDC_BTN_EDIT, I18nGetString(STR_BTN_EDIT));
            SetDlgItemTextW(hDlg, IDC_BTN_UP, I18nGetString(STR_BTN_UP));
            SetDlgItemTextW(hDlg, IDC_BTN_DOWN, I18nGetString(STR_BTN_DOWN));
            SetDlgItemTextW(hDlg, IDC_BTN_DELETE, I18nGetString(STR_BTN_DELETE));
            SetDlgItemTextW(hDlg, IDOK, I18nGetString(STR_BTN_OK));
            SetDlgItemTextW(hDlg, IDCANCEL, I18nGetString(STR_BTN_CANCEL));

            ThemeApplyToDialog(hDlg);
            ManageDlgRebuildTree(hDlg, pState, 0);
            return TRUE;
        }

        case WM_SETTINGCHANGE:
            if (ThemeRefresh()) {
                ThemeApplyToDialog(hDlg);
            }
            return TRUE;

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN: {
            INT_PTR nRes = ThemeHandleCtlColor(uMsg, (HDC)wParam);
            if (nRes != 0) {
                return nRes;
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR pNmh = (LPNMHDR)lParam;
            if (pNmh != NULL && pNmh->idFrom == IDC_TREE_ITEMS) {
                if (pNmh->code == TVN_SELCHANGEDW) {
                    ManageDlgUpdateSelectionUI(hDlg, pState);
                    return TRUE;
                }
                if (pNmh->code == NM_DBLCLK) {
                    ManageDlgOpenEditSelected(hDlg, pState);
                    return TRUE;
                }
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_ADD_APP:
                    ManageDlgOpenAddDialog(hDlg, pState, ENTRY_APP);
                    return TRUE;

                case IDC_BTN_ADD_FOLDER:
                    ManageDlgOpenAddDialog(hDlg, pState, ENTRY_FOLDER);
                    return TRUE;

                case IDC_BTN_ADD_SUBMENU:
                    ManageDlgOpenAddDialog(hDlg, pState, ENTRY_SUBMENU);
                    return TRUE;

                case IDC_BTN_ADD_SEP:
                    ManageDlgAddSeparator(hDlg, pState);
                    return TRUE;

                case IDC_BTN_EDIT:
                    ManageDlgOpenEditSelected(hDlg, pState);
                    return TRUE;

                case IDC_BTN_UP:
                    if (ConfigMoveEntry(&pState->workingCfg, pState->dwSelectedId, -1)) {
                        ManageDlgRebuildTree(hDlg, pState, pState->dwSelectedId);
                    }
                    return TRUE;

                case IDC_BTN_DOWN:
                    if (ConfigMoveEntry(&pState->workingCfg, pState->dwSelectedId, +1)) {
                        ManageDlgRebuildTree(hDlg, pState, pState->dwSelectedId);
                    }
                    return TRUE;

                case IDC_BTN_DELETE: {
                    INT nIdx = ConfigFindIndexById(&pState->workingCfg, pState->dwSelectedId);
                    if (nIdx >= 0) {
                        const MENU_ENTRY *pItem = &pState->workingCfg.pItems[nIdx];
                        BOOL bIsSubWithKids = (pItem->eType == ENTRY_SUBMENU) && ConfigHasChildren(&pState->workingCfg, pItem->dwId);
                        LPCWSTR pszMsg = I18nGetString(bIsSubWithKids ? STR_CONFIRM_DELETE_SUB : STR_CONFIRM_DELETE);
                        if (MessageBoxW(hDlg, pszMsg, I18nGetString(STR_APP_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                            ConfigDeleteEntry(&pState->workingCfg, pState->dwSelectedId);
                            ManageDlgRebuildTree(hDlg, pState, 0);
                        }
                    }
                    return TRUE;
                }

                case IDOK:
                    if (!ConfigSave(&pState->workingCfg)) {
                        MessageBoxW(hDlg, I18nGetString(STR_ERR_SAVE_FAILED), I18nGetString(STR_APP_TITLE), MB_OK | MB_ICONERROR);
                        return TRUE;
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_DESTROY:
            g_hActiveManageDlg = NULL;
            if (pState != NULL && pState->hImageList != NULL) {
                ImageList_Destroy(pState->hImageList);
                pState->hImageList = NULL;
            }
            return TRUE;
    }

    return FALSE;
}

BOOL ManagerDialogShow(HWND hOwnerWnd, HINSTANCE hInstance, PMENU_CONFIG pCfg)
{
    MANAGER_STATE state;
    INT_PTR nResult;

    if (g_hActiveManageDlg != NULL) {
        ManagerDialogBringToFront();
        return FALSE;
    }

    ZeroMemory(&state, sizeof(state));
    state.hInstance = hInstance;
    if (!ConfigClone(&state.workingCfg, pCfg)) {
        return FALSE;
    }

    nResult = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_MANAGE_DLG), hOwnerWnd, ManageDlgProc, (LPARAM)&state);
    if (nResult == IDOK) {
        ConfigFree(pCfg);
        *pCfg = state.workingCfg;
        return TRUE;
    }

    ConfigFree(&state.workingCfg);
    return FALSE;
}

BOOL ManagerDialogIsOpen(VOID)
{
    return (g_hActiveManageDlg != NULL && IsWindow(g_hActiveManageDlg));
}

VOID ManagerDialogBringToFront(VOID)
{
    if (g_hActiveManageDlg != NULL && IsWindow(g_hActiveManageDlg)) {
        ShowWindow(g_hActiveManageDlg, SW_SHOWNORMAL);
        SetForegroundWindow(g_hActiveManageDlg);
    }
}
