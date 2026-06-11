cmake_minimum_required(VERSION 3.20)
project(qingzhou VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.3/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network LinguistTools)

set(CMAKE_RC_FLAGS "-I ${CMAKE_SOURCE_DIR}")

# Everything SDK
set(EVERYTHING_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/everything-sdk")
include_directories("${EVERYTHING_SDK_DIR}/include")
set(QT_DEFAULT_MAJOR_VERSION 6)
set(QAPPLICATION_CLASS QApplication)
add_subdirectory(third_party/SingleApplication)

# QWindowKit 由 git submodule 提供，仅构建 Widgets 静态模块。
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_WIDGETS ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/qwindowkit EXCLUDE_FROM_ALL)

# spdlog 由 git submodule 提供，仅构建项目需要的静态库。
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

qt_add_executable(qingzhou
    src/main.cpp
    src/MainWindow.cpp
    src/SettingsView.cpp
    src/AppLauncher.cpp
    src/GlobalHotkey.cpp
    src/Settings.cpp
    src/Logger.cpp
    src/ThemeManager.cpp
    src/utils/PinyinConverter.cpp
    src/plugins/IPlugin.cpp
    src/plugins/PluginManager.cpp
    src/plugins/AppPlugin.cpp
    src/plugins/CalcPlugin.cpp
    src/plugins/DirectoryPlugin.cpp
    src/plugins/SettingsPlugin.cpp
    src/plugins/FilePlugin.cpp
    src/plugins/TodoPlugin.cpp
    src/todo/Task.cpp
    src/todo/TaskRepository.cpp
    src/todo/JsonTaskStorage.cpp
    src/todo/TaskService.cpp
    src/todo/TaskTreeModel.cpp
    src/Updater.cpp
    resources.qrc
    assets/app_icon.rc
)

target_compile_options(qingzhou PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(qingzhou PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    SingleApplication::SingleApplication
    QWindowKit::Widgets
    spdlog::spdlog
    "${EVERYTHING_SDK_DIR}/lib/Everything64.lib"
)

set_target_properties(qingzhou PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Translations
qt_add_translations(qingzhou
    TS_FILES
        i18n/qingzhou_zh_CN.ts
    LUPDATE_OPTIONS -no-obsolete
)
