# TypeGo

Windows application launcher with built-in tools, built with Qt.

## Building on Linux

Windows builds use `build.bat` / `test.bat`. On Linux, install Qt 6.10 (gcc_64), CMake and Ninja, then:

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.3/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target check && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

On Linux, file search (`file`) uses `fd` (`fdfind` on Debian/Ubuntu, package `fd-find`) under the home folder and falls back to `find`; other Win32 integrations (screen capture, startup entry, installer updates, window effects) are unavailable. UI snapshots (`build/bin/TypeGo --ui-snapshots`) need a CJK font such as `fonts-noto-cjk`.
