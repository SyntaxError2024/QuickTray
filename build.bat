@echo off
setlocal
echo Compilation de QuickTray avec mingw32-make.exe...
where mingw32-make.exe >nul 2>&1
if errorlevel 1 (
    echo Erreur : mingw32-make.exe est introuvable dans le PATH.
    exit /b 1
)
mingw32-make.exe %*
if errorlevel 1 (
    echo Echec de la compilation.
    exit /b %errorlevel%
)
echo Compilation terminée avec succès : QuickTray.exe
endlocal
