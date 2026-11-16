#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

#define MAX_LABEL_LEN   128
#define MAX_ARGS_LEN    512

/* Supported menu entry types */
typedef enum tagENTRY_TYPE {
    ENTRY_APP = 0,
    ENTRY_FOLDER = 1,
    ENTRY_SUBMENU = 2,
    ENTRY_SEPARATOR = 3
} ENTRY_TYPE;

/* Single menu item definition */
typedef struct tagMENU_ENTRY {
    DWORD      dwId;
    DWORD      dwParentId;  /* 0 = root menu, otherwise parent submenu dwId */
    ENTRY_TYPE eType;
    WCHAR      szLabel[MAX_LABEL_LEN];
    WCHAR      szTarget[MAX_PATH];
    WCHAR      szArgs[MAX_ARGS_LEN];
} MENU_ENTRY, *PMENU_ENTRY;

/* Dynamic list of menu entries */
typedef struct tagMENU_CONFIG {
    PMENU_ENTRY pItems;
    DWORD       dwCount;
    DWORD       dwCapacity;
    DWORD       dwNextId;
} MENU_CONFIG, *PMENU_CONFIG;

/* Initializes an empty configuration structure */
VOID ConfigInit(PMENU_CONFIG pCfg);

/* Frees memory owned by a configuration structure */
VOID ConfigFree(PMENU_CONFIG pCfg);

/* Deep-copies a configuration structure for transactional dialog editing */
BOOL ConfigClone(PMENU_CONFIG pDst, const MENU_CONFIG *pSrc);

/* Loads configuration from the UTF-16 LE INI file beside the executable */
BOOL ConfigLoad(PMENU_CONFIG pCfg);

/* Saves configuration to the UTF-16 LE INI file beside the executable */
BOOL ConfigSave(const MENU_CONFIG *pCfg);

/* Appends a new entry and returns its assigned dwId (0 on failure) */
DWORD ConfigAddEntry(
    PMENU_CONFIG pCfg,
    ENTRY_TYPE eType,
    DWORD dwParentId,
    LPCWSTR pszLabel,
    LPCWSTR pszTarget,
    LPCWSTR pszArgs
);

/* Finds array index by entry dwId; returns -1 if not found */
INT ConfigFindIndexById(const MENU_CONFIG *pCfg, DWORD dwId);

/* Returns TRUE if the specified submenu has at least one child entry */
BOOL ConfigHasChildren(const MENU_CONFIG *pCfg, DWORD dwSubmenuId);

/* Returns TRUE if dwCandidateId is equal to or nested inside dwAncestorId */
BOOL ConfigIsDescendant(const MENU_CONFIG *pCfg, DWORD dwCandidateId, DWORD dwAncestorId);

/* Deletes an entry and all nested children recursively */
VOID ConfigDeleteEntry(PMENU_CONFIG pCfg, DWORD dwId);

/* Moves an entry up (nDirection < 0) or down (nDirection > 0) among siblings */
BOOL ConfigMoveEntry(PMENU_CONFIG pCfg, DWORD dwId, INT nDirection);

/* Returns the full path to the portable INI file beside the executable */
BOOL ConfigGetIniPath(LPWSTR pszOutPath, DWORD cchOutPath);

#endif /* CONFIG_H */
