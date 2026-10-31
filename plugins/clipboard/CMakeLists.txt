add_library(clipboard_plugin STATIC ClipboardPlugin.cpp ClipboardIcons.cpp ClipboardStore.cpp ClipboardSettingsWidget.cpp)
target_link_libraries(clipboard_plugin PUBLIC Qt6::Widgets Qt6::Gui)
target_include_directories(clipboard_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(clipboard_plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
