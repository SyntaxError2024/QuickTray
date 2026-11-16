#ifndef TRAY_MENU_H
#define TRAY_MENU_H

#include "config.h"
#include <windows.h>

/* Extracts a small shell HICON for the given entry (caller must DestroyIcon) */
HICON TrayMenuExtractEntryIcon(const MENU_ENTRY *pEntry);

/* Displays the complete quick-launch popup menu at cursor and returns chosen command ID */
UINT TrayMenuShow(HWND hOwnerWnd, const MENU_CONFIG *pCfg);

/* Executes a clicked application or folder entry via ShellExecuteW */
VOID TrayMenuExecuteItem(HWND hOwnerWnd, const MENU_ENTRY *pEntry);

#endif /* TRAY_MENU_H */
