@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "BUILD_DIR=%PROJECT_DIR%\build"
set "QT_DIR=C:\Qt\6.10.3\msvc2022_64"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

call "%VCVARS%" x64
if not "%errorlevel%"=="0" exit /b 1

cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=Release
if not "%errorlevel%"=="0" exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if not "%errorlevel%"=="0" exit /b 1

echo Build completed successfully!
