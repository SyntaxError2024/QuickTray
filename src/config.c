#include "config.h"
#include "i18n.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <wchar.h>

static BOOL EnsureCapacity(PMENU_CONFIG pCfg, DWORD dwNeeded)
{
    DWORD dwNewCap;
    PMENU_ENTRY pNewItems;

    if (dwNeeded <= pCfg->dwCapacity) {
        return TRUE;
    }

    dwNewCap = (pCfg->dwCapacity == 0) ? 32 : (pCfg->dwCapacity * 2);
    while (dwNewCap < dwNeeded) {
        dwNewCap *= 2;
    }

    if (pCfg->pItems == NULL) {
        pNewItems = (PMENU_ENTRY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNewCap * sizeof(MENU_ENTRY));
    } else {
        pNewItems = (PMENU_ENTRY)HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pCfg->pItems, dwNewCap * sizeof(MENU_ENTRY));
    }

    if (pNewItems == NULL) {
        return FALSE;
    }

    pCfg->pItems = pNewItems;
    pCfg->dwCapacity = dwNewCap;
    return TRUE;
}

static VOID SafeCopyW(LPWSTR pszDst, DWORD cchDst, LPCWSTR pszSrc)
{
    if (cchDst == 0) {
        return;
    }
    if (pszSrc == NULL) {
        pszDst[0] = L'\0';
        return;
    }
    lstrcpynW(pszDst, pszSrc, (INT)cchDst);
}

static LPCWSTR EntryTypeToString(ENTRY_TYPE eType)
{
    switch (eType) {
        case ENTRY_APP:       return L"app";
        case ENTRY_FOLDER:    return L"folder";
        case ENTRY_SUBMENU:   return L"submenu";
        case ENTRY_SEPARATOR: return L"separator";
        default:              return L"app";
    }
}

static ENTRY_TYPE StringToEntryType(LPCWSTR pszType)
{
    if (pszType == NULL) {
        return ENTRY_APP;
    }
    if (lstrcmpiW(pszType, L"folder") == 0) {
        return ENTRY_FOLDER;
    }
    if (lstrcmpiW(pszType, L"submenu") == 0) {
        return ENTRY_SUBMENU;
    }
    if (lstrcmpiW(pszType, L"separator") == 0) {
        return ENTRY_SEPARATOR;
    }
    return ENTRY_APP;
}

static VOID TrimWhitespaceW(LPWSTR pszText)
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
    while (len > start && (pszText[len - 1] == L' ' || pszText[len - 1] == L'\t' ||
                           pszText[len - 1] == L'\r' || pszText[len - 1] == L'\n')) {
        len--;
    }

    if (start > 0 && len > start) {
        MoveMemory(pszText, pszText + start, (len - start) * sizeof(WCHAR));
    }
    pszText[len - start] = L'\0';
}

static VOID PopulateDefaults(PMENU_CONFIG pCfg)
{
    WCHAR szDesktop[MAX_PATH] = { 0 };
    WCHAR szDocs[MAX_PATH] = { 0 };
    WCHAR szSysDir[MAX_PATH] = { 0 };
    WCHAR szNotepad[MAX_PATH] = { 0 };
    WCHAR szCmd[MAX_PATH] = { 0 };
    DWORD dwToolsId;

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, szDesktop))) {
        ConfigAddEntry(pCfg, ENTRY_FOLDER, 0, I18nGetString(STR_DEFAULT_DESKTOP), szDesktop, L"");
    }

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, szDocs))) {
        ConfigAddEntry(pCfg, ENTRY_FOLDER, 0, I18nGetString(STR_DEFAULT_DOCUMENTS), szDocs, L"");
    }

    ConfigAddEntry(pCfg, ENTRY_SEPARATOR, 0, L"", L"", L"");

    dwToolsId = ConfigAddEntry(pCfg, ENTRY_SUBMENU, 0, I18nGetString(STR_DEFAULT_SUBMENU_TOOLS), L"", L"");

    if (GetSystemDirectoryW(szSysDir, MAX_PATH) > 0) {
        PathCombineW(szNotepad, szSysDir, L"notepad.exe");
        PathCombineW(szCmd, szSysDir, L"cmd.exe");
        ConfigAddEntry(pCfg, ENTRY_APP, dwToolsId, I18nGetString(STR_DEFAULT_NOTEPAD), szNotepad, L"");
        ConfigAddEntry(pCfg, ENTRY_APP, dwToolsId, I18nGetString(STR_DEFAULT_CMD), szCmd, L"");
    }
}

BOOL ConfigGetIniPath(LPWSTR pszOutPath, DWORD cchOutPath)
{
    WCHAR szExePath[MAX_PATH] = { 0 };
    DWORD dwLen;

    if (pszOutPath == NULL || cchOutPath == 0) {
        return FALSE;
    }

    dwLen = GetModuleFileNameW(NULL, szExePath, MAX_PATH);
    if (dwLen == 0 || dwLen >= MAX_PATH) {
        return FALSE;
    }

    PathRemoveFileSpecW(szExePath);
    if (!PathCombineW(pszOutPath, szExePath, L"QuickTray.ini")) {
        return FALSE;
    }

    return TRUE;
}

VOID ConfigInit(PMENU_CONFIG pCfg)
{
    ZeroMemory(pCfg, sizeof(MENU_CONFIG));
    pCfg->dwNextId = 1;
}

VOID ConfigFree(PMENU_CONFIG pCfg)
{
    if (pCfg == NULL) {
        return;
    }
    if (pCfg->pItems != NULL) {
        HeapFree(GetProcessHeap(), 0, pCfg->pItems);
    }
    ZeroMemory(pCfg, sizeof(MENU_CONFIG));
    pCfg->dwNextId = 1;
}

BOOL ConfigClone(PMENU_CONFIG pDst, const MENU_CONFIG *pSrc)
{
    ConfigInit(pDst);
    if (pSrc == NULL || pSrc->dwCount == 0) {
        if (pSrc != NULL) {
            pDst->dwNextId = pSrc->dwNextId;
        }
        return TRUE;
    }

    if (!EnsureCapacity(pDst, pSrc->dwCount)) {
        return FALSE;
    }

    CopyMemory(pDst->pItems, pSrc->pItems, pSrc->dwCount * sizeof(MENU_ENTRY));
    pDst->dwCount = pSrc->dwCount;
    pDst->dwNextId = pSrc->dwNextId;
    return TRUE;
}

BOOL ConfigLoad(PMENU_CONFIG pCfg)
{
    WCHAR szIniPath[MAX_PATH] = { 0 };
    HANDLE hFile;
    LARGE_INTEGER liSize;
    DWORD dwBytesRead = 0;
    LPBYTE pRaw = NULL;
    LPWSTR pText = NULL;
    DWORD cchTotal = 0;
    DWORD dwPos = 0;
    WCHAR szSection[64] = { 0 };
    MENU_ENTRY curItem;
    BOOL bInItem = FALSE;
    DWORD dwMaxId = 0;

    ConfigFree(pCfg);
    ConfigInit(pCfg);

    if (!ConfigGetIniPath(szIniPath, MAX_PATH)) {
        PopulateDefaults(pCfg);
        return FALSE;
    }

    hFile = CreateFileW(szIniPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        /* First launch: create starter menu and persist it */
        PopulateDefaults(pCfg);
        ConfigSave(pCfg);
        return TRUE;
    }

    if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart <= 0 || liSize.QuadPart > (4 * 1024 * 1024)) {
        CloseHandle(hFile);
        PopulateDefaults(pCfg);
        return FALSE;
    }

    pRaw = (LPBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)liSize.QuadPart + sizeof(WCHAR));
    if (pRaw == NULL) {
        CloseHandle(hFile);
        return FALSE;
    }

    if (!ReadFile(hFile, pRaw, (DWORD)liSize.QuadPart, &dwBytesRead, NULL)) {
        HeapFree(GetProcessHeap(), 0, pRaw);
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);

    /* Interpret UTF-16 LE with optional BOM (0xFEFF) */
    pText = (LPWSTR)pRaw;
    cchTotal = dwBytesRead / sizeof(WCHAR);
    if (cchTotal > 0 && pText[0] == 0xFEFF) {
        pText++;
        cchTotal--;
    }

    ZeroMemory(&curItem, sizeof(curItem));

    while (dwPos < cchTotal) {
        DWORD dwLineStart = dwPos;
        DWORD dwLineLen = 0;
        WCHAR szLine[1024];
        LPWSTR pszEq;

        while (dwPos < cchTotal && pText[dwPos] != L'\r' && pText[dwPos] != L'\n') {
            dwPos++;
        }
        dwLineLen = dwPos - dwLineStart;
        if (dwPos < cchTotal && pText[dwPos] == L'\r') {
            dwPos++;
        }
        if (dwPos < cchTotal && pText[dwPos] == L'\n') {
            dwPos++;
        }

        if (dwLineLen >= ARRAYSIZE(szLine)) {
            dwLineLen = ARRAYSIZE(szLine) - 1;
        }
        CopyMemory(szLine, pText + dwLineStart, dwLineLen * sizeof(WCHAR));
        szLine[dwLineLen] = L'\0';
        TrimWhitespaceW(szLine);

        if (szLine[0] == L'\0' || szLine[0] == L';' || szLine[0] == L'#') {
            continue;
        }

        /* Section header */
        if (szLine[0] == L'[') {
            LPWSTR pszClose = wcschr(szLine, L']');
            if (bInItem) {
                if (curItem.dwId == 0) {
                    curItem.dwId = pCfg->dwNextId++;
                }
                if (curItem.dwId > dwMaxId) {
                    dwMaxId = curItem.dwId;
                }
                if (EnsureCapacity(pCfg, pCfg->dwCount + 1)) {
                    pCfg->pItems[pCfg->dwCount++] = curItem;
                }
                ZeroMemory(&curItem, sizeof(curItem));
                bInItem = FALSE;
            }
            if (pszClose != NULL) {
                *pszClose = L'\0';
                SafeCopyW(szSection, ARRAYSIZE(szSection), szLine + 1);
                TrimWhitespaceW(szSection);
                if (wcsncmp(szSection, L"Item_", 5) == 0) {
                    bInItem = TRUE;
                    ZeroMemory(&curItem, sizeof(curItem));
                }
            }
            continue;
        }

        /* Key=Value pair */
        pszEq = wcschr(szLine, L'=');
        if (pszEq != NULL) {
            WCHAR szKey[64] = { 0 };
            LPCWSTR pszVal = pszEq + 1;
            SIZE_T keyLen = (SIZE_T)(pszEq - szLine);
            if (keyLen >= ARRAYSIZE(szKey)) {
                keyLen = ARRAYSIZE(szKey) - 1;
            }
            CopyMemory(szKey, szLine, keyLen * sizeof(WCHAR));
            szKey[keyLen] = L'\0';
            TrimWhitespaceW(szKey);

            if (lstrcmpiW(szSection, L"General") == 0) {
                if (lstrcmpiW(szKey, L"NextId") == 0) {
                    DWORD dwVal = (DWORD)_wtoi(pszVal);
                    if (dwVal > pCfg->dwNextId) {
                        pCfg->dwNextId = dwVal;
                    }
                }
            } else if (bInItem) {
                if (lstrcmpiW(szKey, L"Id") == 0) {
                    curItem.dwId = (DWORD)_wtoi(pszVal);
                } else if (lstrcmpiW(szKey, L"ParentId") == 0) {
                    curItem.dwParentId = (DWORD)_wtoi(pszVal);
                } else if (lstrcmpiW(szKey, L"Type") == 0) {
                    curItem.eType = StringToEntryType(pszVal);
                } else if (lstrcmpiW(szKey, L"Label") == 0) {
                    SafeCopyW(curItem.szLabel, MAX_LABEL_LEN, pszVal);
                } else if (lstrcmpiW(szKey, L"Target") == 0) {
                    SafeCopyW(curItem.szTarget, MAX_PATH, pszVal);
                } else if (lstrcmpiW(szKey, L"Args") == 0) {
                    SafeCopyW(curItem.szArgs, MAX_ARGS_LEN, pszVal);
                }
            }
        }
    }

    if (bInItem) {
        if (curItem.dwId == 0) {
            curItem.dwId = pCfg->dwNextId++;
        }
        if (curItem.dwId > dwMaxId) {
            dwMaxId = curItem.dwId;
        }
        if (EnsureCapacity(pCfg, pCfg->dwCount + 1)) {
            pCfg->pItems[pCfg->dwCount++] = curItem;
        }
    }

    if (pCfg->dwNextId <= dwMaxId) {
        pCfg->dwNextId = dwMaxId + 1;
    }

    HeapFree(GetProcessHeap(), 0, pRaw);
    return TRUE;
}

typedef struct tagWIDE_BUF {
    WCHAR szData[2048];
    DWORD cchLen;
} WIDE_BUF;

static VOID WideBufReset(WIDE_BUF *pBuf)
{
    pBuf->cchLen = 0;
    pBuf->szData[0] = L'\0';
}

static VOID WideBufAppendStr(WIDE_BUF *pBuf, LPCWSTR pszStr)
{
    DWORD cchMax = ARRAYSIZE(pBuf->szData) - 1;
    if (pszStr == NULL) {
        return;
    }
    while (*pszStr != L'\0' && pBuf->cchLen < cchMax) {
        pBuf->szData[pBuf->cchLen++] = *pszStr++;
    }
    pBuf->szData[pBuf->cchLen] = L'\0';
}

static VOID WideBufAppendDword(WIDE_BUF *pBuf, DWORD dwVal)
{
    WCHAR szNum[16];
    INT nIdx = 0;

    if (dwVal == 0) {
        WideBufAppendStr(pBuf, L"0");
        return;
    }

    while (dwVal > 0 && nIdx < (INT)ARRAYSIZE(szNum)) {
        szNum[nIdx++] = (WCHAR)(L'0' + (dwVal % 10));
        dwVal /= 10;
    }

    while (nIdx > 0) {
        WCHAR szCh[2];
        szCh[0] = szNum[--nIdx];
        szCh[1] = L'\0';
        WideBufAppendStr(pBuf, szCh);
    }
}

static BOOL WriteWideString(HANDLE hFile, LPCWSTR pszText, DWORD cchLen)
{
    DWORD dwBytes = cchLen * sizeof(WCHAR);
    DWORD dwWritten = 0;
    if (dwBytes == 0) {
        return TRUE;
    }
    return WriteFile(hFile, pszText, dwBytes, &dwWritten, NULL) && (dwWritten == dwBytes);
}

BOOL ConfigSave(const MENU_CONFIG *pCfg)
{
    WCHAR szIniPath[MAX_PATH] = { 0 };
    WCHAR szTmpPath[MAX_PATH] = { 0 };
    HANDLE hFile;
    WORD wBom = 0xFEFF;
    DWORD dwWritten = 0;
    WIDE_BUF buf;
    DWORD i;
    BOOL bOk = TRUE;

    if (pCfg == NULL || !ConfigGetIniPath(szIniPath, MAX_PATH)) {
        return FALSE;
    }

    lstrcpynW(szTmpPath, szIniPath, MAX_PATH - 5);
    lstrcatW(szTmpPath, L".tmp");

    hFile = CreateFileW(szTmpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    /* Write UTF-16 LE BOM */
    if (!WriteFile(hFile, &wBom, sizeof(wBom), &dwWritten, NULL) || dwWritten != sizeof(wBom)) {
        CloseHandle(hFile);
        DeleteFileW(szTmpPath);
        return FALSE;
    }

    WideBufReset(&buf);
    WideBufAppendStr(&buf, L"[General]\r\nCount=");
    WideBufAppendDword(&buf, pCfg->dwCount);
    WideBufAppendStr(&buf, L"\r\nNextId=");
    WideBufAppendDword(&buf, pCfg->dwNextId);
    WideBufAppendStr(&buf, L"\r\n\r\n");
    bOk = WriteWideString(hFile, buf.szData, buf.cchLen);

    for (i = 0; i < pCfg->dwCount && bOk; i++) {
        const MENU_ENTRY *pItem = &pCfg->pItems[i];
        WideBufReset(&buf);
        WideBufAppendStr(&buf, L"[Item_");
        WideBufAppendDword(&buf, i);
        WideBufAppendStr(&buf, L"]\r\nId=");
        WideBufAppendDword(&buf, pItem->dwId);
        WideBufAppendStr(&buf, L"\r\nParentId=");
        WideBufAppendDword(&buf, pItem->dwParentId);
        WideBufAppendStr(&buf, L"\r\nType=");
        WideBufAppendStr(&buf, EntryTypeToString(pItem->eType));
        WideBufAppendStr(&buf, L"\r\nLabel=");
        WideBufAppendStr(&buf, pItem->szLabel);
        WideBufAppendStr(&buf, L"\r\nTarget=");
        WideBufAppendStr(&buf, pItem->szTarget);
        WideBufAppendStr(&buf, L"\r\nArgs=");
        WideBufAppendStr(&buf, pItem->szArgs);
        WideBufAppendStr(&buf, L"\r\n\r\n");
        bOk = WriteWideString(hFile, buf.szData, buf.cchLen);
    }

    CloseHandle(hFile);

    if (!bOk) {
        DeleteFileW(szTmpPath);
        return FALSE;
    }

    if (!MoveFileExW(szTmpPath, szIniPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(szTmpPath);
        return FALSE;
    }

    return TRUE;
}

DWORD ConfigAddEntry(
    PMENU_CONFIG pCfg,
    ENTRY_TYPE eType,
    DWORD dwParentId,
    LPCWSTR pszLabel,
    LPCWSTR pszTarget,
    LPCWSTR pszArgs
)
{
    MENU_ENTRY entry;

    if (pCfg == NULL || !EnsureCapacity(pCfg, pCfg->dwCount + 1)) {
        return 0;
    }

    ZeroMemory(&entry, sizeof(entry));
    entry.dwId = pCfg->dwNextId++;
    entry.dwParentId = dwParentId;
    entry.eType = eType;
    SafeCopyW(entry.szLabel, MAX_LABEL_LEN, pszLabel);
    SafeCopyW(entry.szTarget, MAX_PATH, pszTarget);
    SafeCopyW(entry.szArgs, MAX_ARGS_LEN, pszArgs);

    pCfg->pItems[pCfg->dwCount++] = entry;
    return entry.dwId;
}

INT ConfigFindIndexById(const MENU_CONFIG *pCfg, DWORD dwId)
{
    DWORD i;
    if (pCfg == NULL || dwId == 0) {
        return -1;
    }
    for (i = 0; i < pCfg->dwCount; i++) {
        if (pCfg->pItems[i].dwId == dwId) {
            return (INT)i;
        }
    }
    return -1;
}

BOOL ConfigHasChildren(const MENU_CONFIG *pCfg, DWORD dwSubmenuId)
{
    DWORD i;
    if (pCfg == NULL || dwSubmenuId == 0) {
        return FALSE;
    }
    for (i = 0; i < pCfg->dwCount; i++) {
        if (pCfg->pItems[i].dwParentId == dwSubmenuId) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ConfigIsDescendant(const MENU_CONFIG *pCfg, DWORD dwCandidateId, DWORD dwAncestorId)
{
    DWORD dwCur = dwCandidateId;
    DWORD dwGuard = 0;

    if (pCfg == NULL || dwAncestorId == 0) {
        return FALSE;
    }

    while (dwCur != 0 && dwGuard <= pCfg->dwCount) {
        INT nIdx;
        if (dwCur == dwAncestorId) {
            return TRUE;
        }
        nIdx = ConfigFindIndexById(pCfg, dwCur);
        if (nIdx < 0) {
            break;
        }
        dwCur = pCfg->pItems[nIdx].dwParentId;
        dwGuard++;
    }

    return FALSE;
}

VOID ConfigDeleteEntry(PMENU_CONFIG pCfg, DWORD dwId)
{
    INT nIdx;
    DWORD i;

    if (pCfg == NULL || dwId == 0) {
        return;
    }

    /* Delete children first when deleting a submenu */
    for (i = 0; i < pCfg->dwCount; ) {
        if (pCfg->pItems[i].dwParentId == dwId) {
            DWORD dwChildId = pCfg->pItems[i].dwId;
            ConfigDeleteEntry(pCfg, dwChildId);
            i = 0;
        } else {
            i++;
        }
    }

    nIdx = ConfigFindIndexById(pCfg, dwId);
    if (nIdx >= 0) {
        DWORD dwMoveCount = pCfg->dwCount - (DWORD)nIdx - 1;
        if (dwMoveCount > 0) {
            MoveMemory(&pCfg->pItems[nIdx], &pCfg->pItems[nIdx + 1], dwMoveCount * sizeof(MENU_ENTRY));
        }
        pCfg->dwCount--;
    }
}

BOOL ConfigMoveEntry(PMENU_CONFIG pCfg, DWORD dwId, INT nDirection)
{
    INT nIdx;
    INT nSwapIdx = -1;
    DWORD dwParentId;
    INT i;
    MENU_ENTRY temp;

    if (pCfg == NULL || dwId == 0 || nDirection == 0) {
        return FALSE;
    }

    nIdx = ConfigFindIndexById(pCfg, dwId);
    if (nIdx < 0) {
        return FALSE;
    }

    dwParentId = pCfg->pItems[nIdx].dwParentId;

    if (nDirection < 0) {
        for (i = nIdx - 1; i >= 0; i--) {
            if (pCfg->pItems[i].dwParentId == dwParentId) {
                nSwapIdx = i;
                break;
            }
        }
    } else {
        for (i = nIdx + 1; i < (INT)pCfg->dwCount; i++) {
            if (pCfg->pItems[i].dwParentId == dwParentId) {
                nSwapIdx = i;
                break;
            }
        }
    }

    if (nSwapIdx < 0) {
        return FALSE;
    }

    temp = pCfg->pItems[nIdx];
    pCfg->pItems[nIdx] = pCfg->pItems[nSwapIdx];
    pCfg->pItems[nSwapIdx] = temp;
    return TRUE;
}
