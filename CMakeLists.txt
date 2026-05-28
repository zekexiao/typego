cmake_minimum_required(VERSION 3.20)
project(neoStart VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.3/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Widgets LinguistTools)

set(QWINDOWKIT_BUILD_STATIC OFF)

# Everything SDK
set(EVERYTHING_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/everything-sdk")
include_directories("${EVERYTHING_SDK_DIR}/include")
set(QWINDOWKIT_BUILD_QUICK ON)
set(QWINDOWKIT_BUILD_WIDGETS OFF)
set(QWINDOWKIT_BUILD_EXAMPLES OFF)
set(QWINDOWKIT_INSTALL OFF)
add_subdirectory(third_party/qwindowkit)

set(QT_DEFAULT_MAJOR_VERSION 6)
set(QAPPLICATION_CLASS QApplication)
add_subdirectory(third_party/SingleApplication)

# spdlog
set(SPDLOG_BUILD_EXAMPLE OFF)
set(SPDLOG_BUILD_TESTS OFF)
add_subdirectory(third_party/spdlog)

qt_add_executable(neoStart
    src/main.cpp
    src/AppLauncher.cpp
    src/GlobalHotkey.cpp
    src/Settings.cpp
    src/Logger.cpp
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
    src/InputHighlighter.cpp
    resources.qrc
)

target_link_libraries(neoStart PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Widgets
    QWindowKit::Quick
    SingleApplication::SingleApplication
    spdlog::spdlog
    "${EVERYTHING_SDK_DIR}/lib/Everything64.lib"
)

set_target_properties(neoStart PROPERTIES
    WIN32_EXECUTABLE FALSE
)

# Translations
qt_add_translations(neoStart
    TS_FILES
        i18n/neoStart_zh_CN.ts
    LUPDATE_OPTIONS -no-obsolete
)
