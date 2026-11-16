#ifndef RESOURCE_H
#define RESOURCE_H

/* Application icon resource ID */
#define IDI_APP_ICON            101

/* Dialog resource IDs */
#define IDD_MANAGE_DLG          201
#define IDD_EDIT_DLG            202

/* Manage dialog control IDs */
#define IDC_TREE_ITEMS          1001
#define IDC_BTN_ADD_APP         1002
#define IDC_BTN_ADD_FOLDER      1003
#define IDC_BTN_ADD_SUBMENU     1004
#define IDC_BTN_ADD_SEP         1005
#define IDC_BTN_EDIT            1006
#define IDC_BTN_UP              1007
#define IDC_BTN_DOWN            1008
#define IDC_BTN_DELETE          1009
#define IDC_LBL_PREVIEW         1010

/* Edit item dialog control IDs */
#define IDC_LBL_TYPE            1101
#define IDC_COMBO_TYPE          1102
#define IDC_LBL_NAME            1103
#define IDC_EDIT_NAME           1104
#define IDC_LBL_TARGET          1105
#define IDC_EDIT_TARGET         1106
#define IDC_BTN_BROWSE          1107
#define IDC_LBL_ARGS            1108
#define IDC_EDIT_ARGS           1109
#define IDC_LBL_PARENT          1110
#define IDC_COMBO_PARENT        1111

/* Custom window messages and menu command IDs */
#define WM_TRAYICON             (WM_APP + 1)
#define IDM_TRAY_MANAGE         40001
#define IDM_TRAY_EXIT           40002
#define IDM_TRAY_EMPTY          40003
#define IDM_DYNAMIC_BASE        41000

#endif /* RESOURCE_H */
