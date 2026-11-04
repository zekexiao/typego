# 待办内置功能：存储与排序逻辑随库导出供测试直接编译。
add_library(todo_plugin STATIC
    TodoPlugin.cpp
    TodoStore.cpp
    TodoWindow.cpp
    TodoEditorDialog.cpp)
target_link_libraries(todo_plugin PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
target_include_directories(todo_plugin PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(todo_plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
