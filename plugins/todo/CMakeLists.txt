# 待办内置功能：存储与排序逻辑在 typego_core，这里只编译插件壳与界面。
add_library(todo_plugin STATIC
    TodoPlugin.cpp
    TodoWindow.cpp
    TodoCalendar.cpp)
target_link_libraries(todo_plugin PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
target_include_directories(todo_plugin PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(todo_plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
