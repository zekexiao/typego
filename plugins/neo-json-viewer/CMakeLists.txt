cmake_minimum_required(VERSION 3.20)
project(NeoJsonViewerPlugin LANGUAGES C CXX)
if(NOT TARGET TypeGoPlugin::SDK)
    find_package(TypeGoPluginSDK 1.0 REQUIRED CONFIG)
endif()
find_package(Qt6 REQUIRED COMPONENTS LinguistTools Concurrent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用统一管理的依赖；独立构建时可指定 yyjson submodule 路径。
set(TYPEGO_YYJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/yyjson" CACHE PATH "yyjson source directory")
add_library(neo_json_model STATIC JsonTreeItem.cpp JsonTreeModel.cpp "${TYPEGO_YYJSON_DIR}/src/yyjson.c")
target_include_directories(neo_json_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${TYPEGO_YYJSON_DIR}/src")
target_link_libraries(neo_json_model PUBLIC Qt6::Core)
target_compile_options(neo_json_model PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

set(NeoJsonViewer_TS_FILES translations/NeoJsonViewer_zh_CN.ts)
qt_add_translation(NeoJsonViewer_QM_FILES ${NeoJsonViewer_TS_FILES})
typego_add_plugin(neo_json_viewer METADATA "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" SOURCES
    NeoJsonViewerPlugin.cpp NeoJsonViewerPlugin.h
    AboutDialog.cpp AboutDialog.h
    AddItemDialog.cpp AddItemDialog.h
    MainWindow.cpp MainWindow.h
    SearchDialog.cpp SearchDialog.h
    SettingsDialog.cpp SettingsDialog.h
    resources.qrc ${NeoJsonViewer_QM_FILES})
target_compile_options(neo_json_viewer PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(neo_json_viewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neo_json_viewer PRIVATE Qt6::Gui Qt6::Concurrent neo_json_model)
set(NEO_JSON_VIEWER_PACKAGE_DIR "${CMAKE_BINARY_DIR}/bin/plugins/json-viewer")
set_target_properties(neo_json_viewer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${NEO_JSON_VIEWER_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NEO_JSON_VIEWER_PACKAGE_DIR}")
add_custom_command(TARGET neo_json_viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:neo_json_viewer>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NeoJsonViewer_QM_FILES}
        "$<TARGET_FILE_DIR:neo_json_viewer>/translations/NeoJsonViewer_zh_CN.qm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TYPEGO_YYJSON_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:neo_json_viewer>/yyjson-LICENSE.txt"
    VERBATIM)
set_property(TARGET neo_json_viewer APPEND PROPERTY LINK_DEPENDS "${NeoJsonViewer_QM_FILES}")
install(FILES "${TYPEGO_YYJSON_DIR}/LICENSE" DESTINATION plugins/json-viewer RENAME yyjson-LICENSE.txt)
install(TARGETS neo_json_viewer LIBRARY DESTINATION plugins/json-viewer
    RUNTIME DESTINATION plugins/json-viewer)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/neo_json_viewer-metadata/$<CONFIG>/typego-plugin.json"
    DESTINATION plugins/json-viewer)
install(FILES ${NeoJsonViewer_QM_FILES} DESTINATION plugins/json-viewer/translations)
