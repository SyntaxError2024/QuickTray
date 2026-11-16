#ifndef THEME_H
#define THEME_H

#include <windows.h>

/* Initializes theme subsystem, detects system Dark/Light mode, and configures uxtheme */
VOID ThemeInit(VOID);

/* Releases cached GDI brushes and dynamic library handles */
VOID ThemeCleanup(VOID);

/* Re-reads the Windows personalization registry key and updates menu/app themes */
BOOL ThemeRefresh(VOID);

/* Returns TRUE when Windows Dark Mode is active for apps */
BOOL ThemeIsDark(VOID);

/* Applies current Dark/Light styling to a dialog, its title bar, and child controls */
VOID ThemeApplyToDialog(HWND hDlg);

/* Applies Dark/Light colors and visual styles to a SysTreeView32 control */
VOID ThemeApplyToTreeView(HWND hTree);

/* Handles WM_CTLCOLOR* messages for dialogs in Dark Mode; returns NULL in Light Mode */
INT_PTR ThemeHandleCtlColor(UINT uMsg, HDC hdc);

#endif /* THEME_H */
