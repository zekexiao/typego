cmake_minimum_required(VERSION 3.20)

# 版本号单一来源：仓库根目录的 VERSION 文件。
# 程序内显示、更新检查上报的版本都从这里派生，避免多处硬编码漂移。
file(READ "${CMAKE_SOURCE_DIR}/VERSION" TYPEGO_VERSION_RAW)
string(STRIP "${TYPEGO_VERSION_RAW}" TYPEGO_VERSION)
if(NOT TYPEGO_VERSION MATCHES "^[0-9]+\\.[0-9]+(\\.[0-9]+)?$")
    message(FATAL_ERROR "VERSION 文件内容非法: '${TYPEGO_VERSION}'，应为 主.次[.补丁]")
endif()

project(TypeGo VERSION ${TYPEGO_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.3/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network Concurrent LinguistTools)

add_subdirectory(sdk)
# 生成可直接交给客户的 SDK；示例从安装目录独立查找接口。
add_custom_target(plugin_sdk_package ALL
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}/sdk"
        --prefix "${CMAKE_BINARY_DIR}/plugin-sdk" --config $<CONFIG>
    COMMAND ${CMAKE_COMMAND} -E tar cf "${CMAKE_BINARY_DIR}/TypeGoPluginSDK-1.0.0-windows-x64.zip"
        --format=zip plugin-sdk
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM)

# 单元测试开关；关闭时不引入 Test 模块，保持发布构建的依赖不变。
option(TYPEGO_BUILD_TESTS "Build TypeGo unit tests" ON)

set(CMAKE_RC_FLAGS "-I ${CMAKE_SOURCE_DIR}")

# Everything SDK
set(EVERYTHING_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/everything-sdk")
include_directories("${EVERYTHING_SDK_DIR}/include")
set(QT_DEFAULT_MAJOR_VERSION 6)
set(QAPPLICATION_CLASS QApplication)
add_subdirectory(third_party/SingleApplication)

set(QHOTKEY_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QHOTKEY_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/QHotkey EXCLUDE_FROM_ALL)

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

# 纯逻辑层：表达式计算、单位换算插件、动作分类、拼音、待办数据与业务规则、
# 版本与更新检查解析、配置读写。Plugins/TodoView 等依赖窗口的部分不在此列。
# 独立成静态库让可执行程序和单元测试共用同一份实现，避免源码清单两处重复。
add_library(typego_core STATIC
    src/Settings.cpp
    src/plugins/IPlugin.cpp
    src/plugins/CalcPlugin.cpp
    src/plugins/ConvertPlugin.cpp
    src/plugins/ExpressionParser.cpp
    src/plugins/PluginAction.cpp
    src/plugins/PluginManager.cpp
    src/plugins/PluginManifest.cpp
    src/plugins/ExternalPluginRegistry.cpp
    src/plugins/TerminalLauncher.cpp
    src/plugins/UnitConversion.cpp
    src/todo/JsonTaskStorage.cpp
    src/todo/Task.cpp
    src/todo/TaskRepository.cpp
    src/todo/TaskService.cpp
    src/utils/AppMatcher.cpp
    src/utils/AppUsageStore.cpp
    src/utils/DirectoryListing.cpp
    src/utils/PinyinConverter.cpp
    src/utils/ReleaseParser.cpp
    src/utils/ShellOpen.cpp
    src/utils/Version.cpp
)
target_include_directories(typego_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_options(typego_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
target_link_libraries(typego_core PUBLIC
    TypeGoPlugin::SDK
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

qt_add_executable(TypeGo
    src/main.cpp
    src/MainWindow.cpp
    src/SettingsView.cpp
    src/AppLauncher.cpp
    src/GlobalHotkey.cpp
    src/Logger.cpp
    src/ThemeManager.cpp
    src/plugins/AppPlugin.cpp
    src/plugins/BuiltinPlugins.cpp
    src/plugins/PluginWindowHost.cpp
    src/plugins/DirectoryPlugin.cpp
    src/plugins/SettingsPlugin.cpp
    src/plugins/FilePlugin.cpp
    src/plugins/ShellPlugin.cpp
    src/plugins/TodoPlugin.cpp
    src/todo/TodoView.cpp
    src/Updater.cpp
    resources.qrc
    assets/app_icon.rc
)

# 图标更新后强制重新编译 Windows 资源。
set_source_files_properties(assets/app_icon.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/icon.ico"
)

target_compile_options(TypeGo PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# 把 VERSION 注入编译期，供应用内版本显示与更新检查使用。
target_compile_definitions(TypeGo PRIVATE TYPEGO_VERSION="${TYPEGO_VERSION}")

# Inno Setup 的安装包版本必须与 VERSION 一致，否则官网与安装程序会显示不同版本号。
# ISCC 不参与 CMake 构建，因此在配置阶段做一致性校验，漂移时直接报错。
file(READ "${CMAKE_SOURCE_DIR}/inno.iss" TYPEGO_INNO_CONTENT)
string(REGEX MATCH "#define[ \t]+MyAppVersion[ \t]+\"([^\"]+)\"" TYPEGO_INNO_MATCH "${TYPEGO_INNO_CONTENT}")
if(NOT CMAKE_MATCH_1 STREQUAL TYPEGO_VERSION)
    message(FATAL_ERROR "inno.iss 的 MyAppVersion='${CMAKE_MATCH_1}' 与 VERSION 文件 '${TYPEGO_VERSION}' 不一致，请把两者统一到同一个版本号后重新配置。")
endif()

target_link_libraries(TypeGo PRIVATE
    typego_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    Qt6::Concurrent
    SingleApplication::SingleApplication
    QHotkey::QHotkey
    QWindowKit::Widgets
    spdlog::spdlog
    "${EVERYTHING_SDK_DIR}/lib/Everything64.lib"
)

if(WIN32)
    target_link_libraries(TypeGo PRIVATE dwmapi ole32)
endif()

set_target_properties(TypeGo PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 每次构建前更新 TS，避免新增界面文本遗漏翻译。
add_custom_target(update_translation_sources ALL
    COMMAND Qt6::lupdate
        "${CMAKE_SOURCE_DIR}/src"
        -ts "${CMAKE_SOURCE_DIR}/i18n/TypeGo_zh_CN.ts"
        -no-obsolete
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Updating translation source files"
    VERBATIM
)
add_dependencies(TypeGo update_translation_sources)

# Translations
qt_add_translations(TypeGo
    TS_FILES
        i18n/TypeGo_zh_CN.ts
    LUPDATE_OPTIONS -no-obsolete
)

if(TYPEGO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
