#ifndef MANAGER_DLG_H
#define MANAGER_DLG_H

#include "config.h"
#include <windows.h>

/* Opens the modal menu management dialog; returns TRUE if changes were saved */
BOOL ManagerDialogShow(HWND hOwnerWnd, HINSTANCE hInstance, PMENU_CONFIG pCfg);

/* Returns TRUE if the management dialog is currently open */
BOOL ManagerDialogIsOpen(VOID);

/* Brings an already open management dialog to the foreground */
VOID ManagerDialogBringToFront(VOID);

#endif /* MANAGER_DLG_H */
