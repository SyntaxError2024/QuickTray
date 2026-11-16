#include "i18n.h"

static APP_LANGUAGE g_eCurrentLang = APP_LANG_EN;

/* Localization table indexed by [STRING_ID][APP_LANGUAGE] */
static LPCWSTR g_aszStrings[STR_COUNT][APP_LANG_COUNT] = {
    /* STR_APP_TITLE */
    { L"QuickTray", L"QuickTray", L"QuickTray" },
    /* STR_TRAY_TOOLTIP */
    { L"QuickTray - Click to open menu", L"QuickTray - Cliquer pour ouvrir le menu", L"QuickTray - Clic para abrir el menú" },
    /* STR_MENU_EMPTY */
    { L"(No entries)", L"(Aucune entrée)", L"(Sin elementos)" },
    /* STR_MENU_MANAGE */
    { L"Manage Menu...", L"Gérer le menu...", L"Gestionar menú..." },
    /* STR_MENU_EXIT */
    { L"Exit", L"Quitter", L"Salir" },
    /* STR_MANAGE_TITLE */
    { L"QuickTray - Manage Menu", L"QuickTray - Gérer le menu", L"QuickTray - Gestionar menú" },
    /* STR_BTN_ADD_APP */
    { L"Add Application...", L"Ajouter une appli...", L"Añadir aplicación..." },
    /* STR_BTN_ADD_FOLDER */
    { L"Add Folder...", L"Ajouter un dossier...", L"Añadir carpeta..." },
    /* STR_BTN_ADD_SUBMENU */
    { L"Add Submenu...", L"Ajouter sous-menu...", L"Añadir submenú..." },
    /* STR_BTN_ADD_SEP */
    { L"Add Separator", L"Ajouter séparateur", L"Añadir separador" },
    /* STR_BTN_EDIT */
    { L"Edit...", L"Modifier...", L"Editar..." },
    /* STR_BTN_UP */
    { L"Move Up", L"Monter", L"Subir" },
    /* STR_BTN_DOWN */
    { L"Move Down", L"Descendre", L"Bajar" },
    /* STR_BTN_DELETE */
    { L"Delete", L"Supprimer", L"Eliminar" },
    /* STR_BTN_OK */
    { L"OK", L"OK", L"Aceptar" },
    /* STR_BTN_CANCEL */
    { L"Cancel", L"Annuler", L"Cancelar" },
    /* STR_EDIT_TITLE_ADD */
    { L"Add Entry", L"Ajouter une entrée", L"Añadir entrada" },
    /* STR_EDIT_TITLE_EDIT */
    { L"Edit Entry", L"Modifier l'entrée", L"Editar entrada" },
    /* STR_LBL_TYPE */
    { L"Type:", L"Type :", L"Tipo:" },
    /* STR_LBL_NAME */
    { L"Name:", L"Nom :", L"Nombre:" },
    /* STR_LBL_TARGET */
    { L"Target:", L"Cible :", L"Destino:" },
    /* STR_BTN_BROWSE */
    { L"Browse...", L"Parcourir...", L"Examinar..." },
    /* STR_LBL_ARGS */
    { L"Arguments:", L"Arguments :", L"Argumentos:" },
    /* STR_LBL_PARENT */
    { L"Parent Menu:", L"Menu parent :", L"Menú padre:" },
    /* STR_TYPE_APP */
    { L"Application", L"Application", L"Aplicación" },
    /* STR_TYPE_FOLDER */
    { L"Folder", L"Dossier", L"Carpeta" },
    /* STR_TYPE_SUBMENU */
    { L"Submenu", L"Sous-menu", L"Submenú" },
    /* STR_TYPE_SEPARATOR */
    { L"Separator", L"Séparateur", L"Separador" },
    /* STR_ROOT_MENU */
    { L"[Root Menu]", L"[Menu racine]", L"[Menú raíz]" },
    /* STR_SEP_DISPLAY */
    { L"──── (Separator) ────", L"──── (Séparateur) ────", L"──── (Separador) ────" },
    /* STR_BROWSE_APP_TITLE */
    { L"Select an Application", L"Sélectionner une application", L"Seleccionar una aplicación" },
    /* STR_BROWSE_FOLDER_TITLE */
    { L"Select a Folder", L"Sélectionner un dossier", L"Seleccionar una carpeta" },
    /* STR_BROWSE_FILTER_APPS */
    {
        L"Applications (*.exe;*.bat;*.cmd;*.lnk)\0*.exe;*.bat;*.cmd;*.lnk\0All Files (*.*)\0*.*\0",
        L"Applications (*.exe;*.bat;*.cmd;*.lnk)\0*.exe;*.bat;*.cmd;*.lnk\0Tous les fichiers (*.*)\0*.*\0",
        L"Aplicaciones (*.exe;*.bat;*.cmd;*.lnk)\0*.exe;*.bat;*.cmd;*.lnk\0Todos los archivos (*.*)\0*.*\0"
    },
    /* STR_ERR_NAME_REQUIRED */
    { L"Please enter a display name.", L"Veuillez saisir un nom d'affichage.", L"Por favor, introduzca un nombre." },
    /* STR_ERR_TARGET_REQUIRED */
    { L"Please specify a target path.", L"Veuillez spécifier un chemin cible.", L"Por favor, especifique una ruta de destino." },
    /* STR_ERR_LAUNCH_FAILED */
    { L"Unable to open the selected item.", L"Impossible d'ouvrir l'élément sélectionné.", L"No se pudo abrir el elemento seleccionado." },
    /* STR_ERR_SAVE_FAILED */
    { L"Failed to save the configuration file.", L"Impossible d'enregistrer le fichier de configuration.", L"Error al guardar el archivo de configuración." },
    /* STR_CONFIRM_DELETE */
    { L"Delete the selected entry?", L"Supprimer l'entrée sélectionnée ?", L"¿Eliminar la entrada seleccionada?" },
    /* STR_CONFIRM_DELETE_SUB */
    { L"Delete this submenu and all its entries?", L"Supprimer ce sous-menu et toutes ses entrées ?", L"¿Eliminar este submenú y todas sus entradas?" },
    /* STR_PREVIEW_TARGET */
    { L"Target: ", L"Cible : ", L"Destino: " },
    /* STR_PREVIEW_ARGS */
    { L"Args: ", L"Args : ", L"Args: " },
    /* STR_PREVIEW_SUBMENU */
    { L"Submenu group", L"Groupe de sous-menu", L"Grupo de submenú" },
    /* STR_PREVIEW_SEPARATOR */
    { L"Menu separator", L"Séparateur de menu", L"Separador de menú" },
    /* STR_DEFAULT_DESKTOP */
    { L"Desktop", L"Bureau", L"Escritorio" },
    /* STR_DEFAULT_DOCUMENTS */
    { L"Documents", L"Documents", L"Documentos" },
    /* STR_DEFAULT_SUBMENU_TOOLS */
    { L"System Tools", L"Outils système", L"Herramientas del sistema" },
    /* STR_DEFAULT_NOTEPAD */
    { L"Notepad", L"Bloc-notes", L"Bloc de notas" },
    /* STR_DEFAULT_CMD */
    { L"Command Prompt", L"Invite de commandes", L"Símbolo del sistema" }
};

VOID I18nInit(VOID)
{
    LANGID wLangId = GetUserDefaultUILanguage();
    WORD wPrimary = PRIMARYLANGID(wLangId);

    if (wPrimary == LANG_FRENCH) {
        g_eCurrentLang = APP_LANG_FR;
    } else if (wPrimary == LANG_SPANISH) {
        g_eCurrentLang = APP_LANG_ES;
    } else {
        g_eCurrentLang = APP_LANG_EN;
    }
}

APP_LANGUAGE I18nGetLanguage(VOID)
{
    return g_eCurrentLang;
}

LPCWSTR I18nGetString(STRING_ID eId)
{
    if ((INT)eId < 0 || eId >= STR_COUNT) {
        return L"";
    }
    return g_aszStrings[eId][g_eCurrentLang];
}
