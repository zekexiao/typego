function(typego_add_plugin target)
    cmake_parse_arguments(PLUGIN "" "METADATA" "SOURCES" ${ARGN})
    if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "TypeGo plugins require Windows x64 and MSVC")
    endif()
    if(NOT PLUGIN_METADATA OR NOT PLUGIN_SOURCES)
        message(FATAL_ERROR "typego_add_plugin requires METADATA and SOURCES")
    endif()
    # SDK 在客户工程中生成兼容标识，避免沿用发布 SDK 时的编译器信息。
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PLUGIN_METADATA}")
    file(READ "${PLUGIN_METADATA}" metadata)
    string(JSON metadata SET "${metadata}" apiMajor 1)
    string(JSON metadata SET "${metadata}" apiMinor 0)
    string(JSON metadata SET "${metadata}" abi
        "\"windows-qt-${Qt6Core_VERSION}-msvc-${MSVC_VERSION}-ptr-8-md-$<IF:$<CONFIG:Debug>,debug,release>\"")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}-metadata/$<CONFIG>/typego-plugin.json"
        CONTENT "${metadata}\n")
    add_library(${target} MODULE ${PLUGIN_SOURCES})
    set_target_properties(${target} PROPERTIES AUTOMOC ON PREFIX "" OUTPUT_NAME plugin
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_link_libraries(${target} PRIVATE TypeGoPlugin::SDK)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${target}-metadata/$<CONFIG>")
endfunction()
