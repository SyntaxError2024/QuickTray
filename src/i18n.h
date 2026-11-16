#ifndef I18N_H
#define I18N_H

#include <windows.h>

/* Supported UI languages */
typedef enum tagAPP_LANGUAGE {
    APP_LANG_EN = 0,
    APP_LANG_FR = 1,
    APP_LANG_ES = 2,
    APP_LANG_COUNT = 3
} APP_LANGUAGE;

/* Localized string identifiers */
typedef enum tagSTRING_ID {
    STR_APP_TITLE = 0,
    STR_TRAY_TOOLTIP,
    STR_MENU_EMPTY,
    STR_MENU_MANAGE,
    STR_MENU_EXIT,
    STR_MANAGE_TITLE,
    STR_BTN_ADD_APP,
    STR_BTN_ADD_FOLDER,
    STR_BTN_ADD_SUBMENU,
    STR_BTN_ADD_SEP,
    STR_BTN_EDIT,
    STR_BTN_UP,
    STR_BTN_DOWN,
    STR_BTN_DELETE,
    STR_BTN_OK,
    STR_BTN_CANCEL,
    STR_EDIT_TITLE_ADD,
    STR_EDIT_TITLE_EDIT,
    STR_LBL_TYPE,
    STR_LBL_NAME,
    STR_LBL_TARGET,
    STR_BTN_BROWSE,
    STR_LBL_ARGS,
    STR_LBL_PARENT,
    STR_TYPE_APP,
    STR_TYPE_FOLDER,
    STR_TYPE_SUBMENU,
    STR_TYPE_SEPARATOR,
    STR_ROOT_MENU,
    STR_SEP_DISPLAY,
    STR_BROWSE_APP_TITLE,
    STR_BROWSE_FOLDER_TITLE,
    STR_BROWSE_FILTER_APPS,
    STR_ERR_NAME_REQUIRED,
    STR_ERR_TARGET_REQUIRED,
    STR_ERR_LAUNCH_FAILED,
    STR_ERR_SAVE_FAILED,
    STR_CONFIRM_DELETE,
    STR_CONFIRM_DELETE_SUB,
    STR_PREVIEW_TARGET,
    STR_PREVIEW_ARGS,
    STR_PREVIEW_SUBMENU,
    STR_PREVIEW_SEPARATOR,
    STR_DEFAULT_DESKTOP,
    STR_DEFAULT_DOCUMENTS,
    STR_DEFAULT_SUBMENU_TOOLS,
    STR_DEFAULT_NOTEPAD,
    STR_DEFAULT_CMD,
    STR_COUNT
} STRING_ID;

/* Initializes localization using the current Windows user UI language */
VOID I18nInit(VOID);

/* Returns the currently active application language */
APP_LANGUAGE I18nGetLanguage(VOID);

/* Returns the localized wide string for the specified identifier */
LPCWSTR I18nGetString(STRING_ID eId);

#endif /* I18N_H */
