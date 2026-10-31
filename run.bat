@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "QT_DIR=C:\Qt\6.10.3\msvc2022_64"
set "APP_PATH=%PROJECT_DIR%\build\bin\TypeGo.exe"

set "PATH=%QT_DIR%\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_DIR%\plugins"
set "QML_IMPORT_PATH=%QT_DIR%\qml"
set "QML2_IMPORT_PATH=%QT_DIR%\qml"

if not exist "%APP_PATH%" (
    echo Application not found: "%APP_PATH%"
    echo Run build.bat first.
    exit /b 1
)

"%APP_PATH%" %*
set "APP_EXIT_CODE=%errorlevel%"
if "%~1"=="--ui-snapshots" if not "%APP_EXIT_CODE%"=="0" (
    echo UI snapshots failed with code %APP_EXIT_CODE%.
    echo Codes: 2=temp directory, 3=translations, 4=output directory already exists or is unwritable,
    echo 5=rendering or writing files - see FAILED.txt, 6=LATEST.txt, 7=fonts.
)
exit /b %APP_EXIT_CODE%
