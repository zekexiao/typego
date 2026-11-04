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

# Windows 默认 Qt 路径；其他平台通过 CMAKE_PREFIX_PATH 或系统 Qt 查找。
if(WIN32)
    list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.3/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network Concurrent LinguistTools)

add_subdirectory(plugins/neo-json-viewer)
add_subdirectory(plugins/todo)
add_subdirectory(plugins/clipboard)
add_subdirectory(plugins/screenshot)

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

# 纯逻辑层：表达式计算、单位换算插件、动作分类、拼音、版本与更新检查解析、配置读写。
# 独立成静态库让可执行程序和单元测试共用同一份实现，避免源码清单两处重复。
add_library(typego_core STATIC
    src/Settings.cpp
    src/plugins/IPlugin.cpp
    src/plugins/CalcPlugin.cpp
    src/plugins/ConvertPlugin.cpp
    src/plugins/ExpressionParser.cpp
    src/plugins/PluginAction.cpp
    src/plugins/PluginManager.cpp
    src/plugins/TerminalLauncher.cpp
    src/plugins/UnitConversion.cpp
    src/utils/AppMatcher.cpp
    src/utils/AppUsageStore.cpp
    src/utils/DirectoryListing.cpp
    src/utils/FileSearchCommand.cpp
    src/utils/IconExtractor.cpp
    src/utils/PinStorage.cpp
    src/utils/ScreenPosition.cpp
    src/utils/Restart.cpp
    src/utils/PinyinConverter.cpp
    src/utils/ReleaseParser.cpp
    src/utils/ShellOpen.cpp
    src/utils/UpdateCheckGate.cpp
    src/utils/Version.cpp
)
target_include_directories(typego_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_options(typego_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
target_link_libraries(typego_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_link_libraries(typego_core PUBLIC neo_json_model)

# IconExtractor 在 Windows 分支里使用 Shell COM API
if(WIN32)
    target_link_libraries(typego_core PUBLIC shell32 shlwapi gdi32 user32)
endif()

qt_add_executable(TypeGo
    src/main.cpp
    src/UiSnapshots.cpp
    src/plugins/IntegratedFeatures.cpp
    src/MainWindow.cpp
    src/SettingsView.cpp
    src/AppLauncher.cpp
    src/GlobalHotkey.cpp
    src/Logger.cpp
    src/ThemeManager.cpp
    src/PluginSettingsDialog.cpp
    src/plugins/AppPlugin.cpp
    src/plugins/BuiltinPlugins.cpp
    src/plugins/PluginWindowHost.cpp
    src/plugins/DirectoryPlugin.cpp
    src/plugins/SettingsPlugin.cpp
    src/plugins/FilePlugin.cpp
    src/plugins/ShellPlugin.cpp
    src/UpdateDialog.cpp
    src/Updater.cpp
    resources.qrc
)

if(WIN32)
    target_sources(TypeGo PRIVATE assets/app_icon.rc)
    # 图标更新后强制重新编译 Windows 资源。
    set_source_files_properties(assets/app_icon.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/icon.ico"
    )
endif()

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
    clipboard_plugin screenshot_plugin neo_json_viewer todo_plugin
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
)

# Everything SDK 只提供 Windows 导入库，文件搜索在其他平台退化为提示项。
if(WIN32)
    target_link_libraries(TypeGo PRIVATE "${EVERYTHING_SDK_DIR}/lib/Everything64.lib" dwmapi ole32)
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

# 内置功能译文随主程序一同发布。
qt_add_translation(INTEGRATED_QM_FILES
    plugins/clipboard/translations/Clipboard_zh_CN.ts
    plugins/screenshot/translations/Screenshot_zh_CN.ts
    plugins/neo-json-viewer/translations/NeoJsonViewer_zh_CN.ts
    plugins/todo/translations/Todo_zh_CN.ts)
qt_add_resources(TypeGo integrated_translations PREFIX /integrated
    BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${INTEGRATED_QM_FILES})
qt_add_resources(TypeGo json_viewer_assets PREFIX /jsonviewer
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/plugins/neo-json-viewer
    FILES plugins/neo-json-viewer/icon.png)

# Translations
get_filename_component(TYPEGO_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
qt_add_resources(TypeGo qt_translations
    PREFIX "/i18n"
    BASE "${TYPEGO_QT_PREFIX}/translations"
    FILES "${TYPEGO_QT_PREFIX}/translations/qtbase_zh_CN.qm"
)
qt_add_translations(TypeGo
    TS_FILES
        i18n/TypeGo_zh_CN.ts
    LUPDATE_OPTIONS -no-obsolete
)

# Linux 安装与 deb 打包：Qt 运行库随包部署到 /opt/typego，不依赖发行版自带的旧版 Qt。
# 用法：cmake --install 或 cpack -G DEB（在构建目录执行）。
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    set_target_properties(TypeGo PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS TypeGo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    qt_generate_deploy_app_script(TARGET TypeGo OUTPUT_SCRIPT TYPEGO_DEPLOY_SCRIPT NO_TRANSLATIONS)
    install(SCRIPT "${TYPEGO_DEPLOY_SCRIPT}")

    # 桌面入口、图标和命令行入口放到系统目录，程序本体留在 /opt/typego。
    file(CREATE_LINK /opt/typego/${CMAKE_INSTALL_BINDIR}/TypeGo "${CMAKE_CURRENT_BINARY_DIR}/typego" SYMBOLIC)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/typego" DESTINATION /usr/bin)
    install(FILES assets/typego.desktop DESTINATION /usr/share/applications)
    install(FILES assets/icon.png DESTINATION /usr/share/pixmaps RENAME typego.png)

    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_NAME typego)
    set(CPACK_PACKAGE_VENDOR "Linger Me, Inc.")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Application launcher with built-in tools")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://typego.app")
    set(CPACK_PACKAGING_INSTALL_PREFIX /opt/typego)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Linger Me, Inc. <xiao@linger.me>")
    set(CPACK_DEBIAN_PACKAGE_SECTION utils)
    # 依赖由 dpkg-shlibdeps 从 ELF 推导；随包 Qt 库不是系统包，要告诉它在哪找。
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${CMAKE_INSTALL_LIBDIR}")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fd-find, fonts-noto-cjk")
    include(CPack)
endif()

if(TYPEGO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
