@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "BUILD_DIR=%PROJECT_DIR%\build"
set "QT_DIR=C:\Qt\6.10.3\msvc2022_64"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

rem Test executables need Qt DLLs; PATH setup mirrors run.bat.
set "PATH=%QT_DIR%\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_DIR%\plugins"

call "%VCVARS%" x64
if not "%errorlevel%"=="0" exit /b 1

cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=Release -DTYPEGO_BUILD_TESTS=ON
if not "%errorlevel%"=="0" exit /b 1

cmake --build "%BUILD_DIR%" --target check
if not "%errorlevel%"=="0" exit /b 1

ctest --test-dir "%BUILD_DIR%" --output-on-failure
exit /b %errorlevel%
