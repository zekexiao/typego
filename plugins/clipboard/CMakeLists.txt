cmake_minimum_required(VERSION 3.20)
project(ClipboardPlugin LANGUAGES CXX)
if(NOT TARGET TypeGoPlugin::SDK)
    find_package(TypeGoPluginSDK 1.0 REQUIRED CONFIG)
endif()
find_package(Qt6 REQUIRED COMPONENTS LinguistTools Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Clipboard_TS_FILES translations/Clipboard_zh_CN.ts)
qt_add_translation(Clipboard_QM_FILES ${Clipboard_TS_FILES})

typego_add_plugin(clipboard_plugin METADATA "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" SOURCES
    ClipboardPlugin.cpp ClipboardPlugin.h
    ClipboardIcons.cpp ClipboardIcons.h
    ClipboardStore.cpp ClipboardStore.h
    ClipboardSettingsWidget.cpp ClipboardSettingsWidget.h
    ${Clipboard_QM_FILES})
target_compile_options(clipboard_plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(clipboard_plugin PRIVATE Qt6::Gui Qt6::Widgets)

# 下游按插件 ID 查找包目录，目录名必须与元信息 id 一致。
set(CLIPBOARD_PACKAGE_DIR "${CMAKE_BINARY_DIR}/bin/plugins/clipboard")
set_target_properties(clipboard_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CLIPBOARD_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CLIPBOARD_PACKAGE_DIR}")
add_custom_command(TARGET clipboard_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:clipboard_plugin>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${Clipboard_QM_FILES}
        "$<TARGET_FILE_DIR:clipboard_plugin>/translations/Clipboard_zh_CN.qm"
    VERBATIM)
set_property(TARGET clipboard_plugin APPEND PROPERTY LINK_DEPENDS "${Clipboard_QM_FILES}")

install(TARGETS clipboard_plugin LIBRARY DESTINATION plugins/clipboard
    RUNTIME DESTINATION plugins/clipboard)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/clipboard_plugin-metadata/$<CONFIG>/typego-plugin.json"
    DESTINATION plugins/clipboard)
install(FILES ${Clipboard_QM_FILES} DESTINATION plugins/clipboard/translations)