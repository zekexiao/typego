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
typego_add_test(tst_settings tst_settings.cpp)
typego_add_test(tst_release_parser tst_release_parser.cpp)
typego_add_test(tst_app_matcher tst_app_matcher.cpp)
typego_add_test(tst_app_usage tst_app_usage.cpp)
typego_add_test(tst_terminal_launcher tst_terminal_launcher.cpp)
typego_add_test(tst_task_repository tst_task_repository.cpp)
typego_add_test(tst_json_task_storage tst_json_task_storage.cpp)
typego_add_test(tst_task_service tst_task_service.cpp)
