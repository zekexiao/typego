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

# 独立客户工程仅 find_package 安装后的 SDK，不接入宿主源码或 core 目标。
include(ExternalProject)
ExternalProject_Add(plugin_sdk_example
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/examples/plugin-demo"
    BINARY_DIR "${CMAKE_BINARY_DIR}/sdk-example"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DTypeGoPluginSDK_DIR=${CMAKE_BINARY_DIR}/plugin-sdk/lib/cmake/TypeGoPluginSDK"
        "-DQt6_DIR=${Qt6_DIR}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/sdk-example/plugin.dll"
    DEPENDS plugin_sdk_package)
typego_add_test(tst_plugin_sdk tst_plugin_sdk.cpp)
typego_add_plugin(plugin_contract
    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/plugin-contract.json"
    SOURCES fixtures/ContractPlugin.cpp)
set_target_properties(plugin_contract PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/contract"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/contract")
add_dependencies(tst_plugin_sdk plugin_contract)
add_dependencies(tst_plugin_sdk plugin_sdk_example)
target_compile_definitions(tst_plugin_sdk PRIVATE
    TYPEGO_EXAMPLE_PLUGIN="${CMAKE_BINARY_DIR}/sdk-example/plugin.dll"
    TYPEGO_CONTRACT_PLUGIN="$<TARGET_FILE:plugin_contract>")
typego_add_test(tst_settings tst_settings.cpp)
typego_add_test(tst_release_parser tst_release_parser.cpp)
typego_add_test(tst_app_matcher tst_app_matcher.cpp)
typego_add_test(tst_app_usage tst_app_usage.cpp)
typego_add_test(tst_terminal_launcher tst_terminal_launcher.cpp)
typego_add_test(tst_directory_listing tst_directory_listing.cpp)
typego_add_test(tst_pin_storage tst_pin_storage.cpp)
typego_add_test(tst_screen_position tst_screen_position.cpp)
add_executable(restart_parent fixtures/RestartParent.cpp)
target_link_libraries(restart_parent PRIVATE Qt6::Core)
target_compile_options(restart_parent PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
typego_add_test(tst_restart tst_restart.cpp)
add_dependencies(tst_restart restart_parent)
target_compile_definitions(tst_restart PRIVATE TYPEGO_RESTART_PARENT="$<TARGET_FILE:restart_parent>")

typego_add_test(tst_json_tree_model tst_json_tree_model.cpp)

# 剪贴板历史存储与图标只依赖 Qt Gui，不需要加载插件或访问系统剪贴板。
typego_add_test(tst_clipboard_store tst_clipboard_store.cpp
    ${CMAKE_SOURCE_DIR}/plugins/clipboard/ClipboardStore.cpp
    ${CMAKE_SOURCE_DIR}/plugins/clipboard/ClipboardIcons.cpp)
target_include_directories(tst_clipboard_store PRIVATE "${CMAKE_SOURCE_DIR}/plugins/clipboard")
