# Qt Test 用例集：每个文件是一个独立可执行进程，互不影响状态。
find_package(Qt6 REQUIRED COMPONENTS Test)

# 统一测试目标配置：链接 core 库与 Qt Test，输出到 build/tests 便于 CTest 定位。
add_custom_target(check COMMENT "Building all unit tests")

function(typego_add_test target)
    add_executable(${target} ${ARGN})
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )
    target_link_libraries(${target} PRIVATE
        typego_core
        Qt6::Test
    )
    set_target_properties(${target} PROPERTIES
        WIN32_EXECUTABLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
    add_dependencies(check ${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

typego_add_test(tst_version tst_version.cpp)
typego_add_test(tst_pinyin tst_pinyin.cpp)
typego_add_test(tst_calc tst_calc.cpp)
typego_add_test(tst_convert tst_convert.cpp)
typego_add_test(tst_plugin_manager tst_plugin_manager.cpp)
typego_add_test(tst_plugin_runtime tst_plugin_runtime.cpp)
typego_add_test(tst_integrated_features tst_integrated_features.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/IntegratedFeatures.cpp)
target_link_libraries(tst_integrated_features PRIVATE clipboard_plugin screenshot_plugin neo_json_viewer todo_plugin Qt6::Concurrent)
if(WIN32)
    target_link_libraries(tst_integrated_features PRIVATE user32 gdi32 dwmapi)
endif()

typego_add_test(tst_settings tst_settings.cpp)
typego_add_test(tst_release_parser tst_release_parser.cpp)
typego_add_test(tst_update_check_gate tst_update_check_gate.cpp)
typego_add_test(tst_app_matcher tst_app_matcher.cpp)
typego_add_test(tst_app_usage tst_app_usage.cpp)
typego_add_test(tst_terminal_launcher tst_terminal_launcher.cpp)
typego_add_test(tst_directory_listing tst_directory_listing.cpp)
# fd/find 搜索只在非 Windows 平台启用；Windows 上 QFile::decodeName 按本地代码页解码，UTF-8 断言不成立。
if(NOT WIN32)
    typego_add_test(tst_file_search_command tst_file_search_command.cpp)
endif()
typego_add_test(tst_pin_storage tst_pin_storage.cpp)
typego_add_test(tst_screen_position tst_screen_position.cpp)
add_executable(restart_parent fixtures/RestartParent.cpp)
target_link_libraries(restart_parent PRIVATE Qt6::Core)
target_compile_options(restart_parent PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
typego_add_test(tst_restart tst_restart.cpp)
add_dependencies(tst_restart restart_parent)
target_compile_definitions(tst_restart PRIVATE TYPEGO_RESTART_PARENT="$<TARGET_FILE:restart_parent>")

typego_add_test(tst_json_tree_model tst_json_tree_model.cpp)

# 待办存储只依赖 Qt Core，在临时目录验证读写与排序。
typego_add_test(tst_todo_store tst_todo_store.cpp
    ${CMAKE_SOURCE_DIR}/plugins/todo/TodoStore.cpp)
target_include_directories(tst_todo_store PRIVATE "${CMAKE_SOURCE_DIR}/plugins/todo")

# 翻译完整性：所有 .ts 的译文必须非空，否则界面会直接露出英文原文。
typego_add_test(tst_translations tst_translations.cpp)
target_compile_definitions(tst_translations PRIVATE TYPEGO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# 剪贴板历史存储与图标只依赖 Qt Gui，不需要加载插件或访问系统剪贴板。
typego_add_test(tst_clipboard_store tst_clipboard_store.cpp
    ${CMAKE_SOURCE_DIR}/plugins/clipboard/ClipboardStore.cpp
    ${CMAKE_SOURCE_DIR}/plugins/clipboard/ClipboardIcons.cpp)
target_include_directories(tst_clipboard_store PRIVATE "${CMAKE_SOURCE_DIR}/plugins/clipboard")

# 截图历史存储只依赖 Qt Gui，可在临时目录中验证淘汰、缩略图和清理。
typego_add_test(tst_screenshot_store tst_screenshot_store.cpp
    ${CMAKE_SOURCE_DIR}/plugins/screenshot/ScreenshotStore.cpp)
target_include_directories(tst_screenshot_store PRIVATE "${CMAKE_SOURCE_DIR}/plugins/screenshot")

# 标注合成只依赖 Qt Gui：验证 HiDPI 下的标注落点与全局坐标到裁剪坐标的平移。
typego_add_test(tst_screenshot_annotation tst_screenshot_annotation.cpp
    ${CMAKE_SOURCE_DIR}/plugins/screenshot/Annotation.cpp)
target_include_directories(tst_screenshot_annotation PRIVATE "${CMAKE_SOURCE_DIR}/plugins/screenshot")

# 长截图拼接是纯图像算法，重点覆盖上下滚动、固定头尾和横向滚动。
typego_add_test(tst_scroll_stitcher tst_scroll_stitcher.cpp
    ${CMAKE_SOURCE_DIR}/plugins/screenshot/ScrollStitcher.cpp)
target_include_directories(tst_scroll_stitcher PRIVATE "${CMAKE_SOURCE_DIR}/plugins/screenshot")
