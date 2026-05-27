@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

if not exist build mkdir build
cd build

cmake .. -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.10.3/msvc2022_64" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build . --config Release
if errorlevel 1 exit /b 1

echo Build completed successfully!
cd ..
