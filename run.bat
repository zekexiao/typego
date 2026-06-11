@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "QT_DIR=C:\Qt\6.10.3\msvc2022_64"
set "APP_PATH=%PROJECT_DIR%\build\bin\qingzhou.exe"

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
