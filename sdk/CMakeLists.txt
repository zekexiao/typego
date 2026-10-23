cmake_minimum_required(VERSION 3.20)
project(TypeGoPluginSDK VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "TypeGo Plugin SDK 1.0 supports Windows x64 with MSVC only")
endif()

add_library(typego_plugin_sdk INTERFACE)
add_library(TypeGoPlugin::SDK ALIAS typego_plugin_sdk)
set_target_properties(typego_plugin_sdk PROPERTIES EXPORT_NAME SDK)
target_compile_features(typego_plugin_sdk INTERFACE cxx_std_17)
target_compile_options(typego_plugin_sdk INTERFACE /utf-8)
target_include_directories(typego_plugin_sdk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(typego_plugin_sdk INTERFACE Qt6::Core Qt6::Widgets)
include(cmake/TypeGoPlugin.cmake)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/TypeGoPluginSDKConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/TypeGoPluginSDKConfig.cmake"
    INSTALL_DESTINATION lib/cmake/TypeGoPluginSDK)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/TypeGoPluginSDKConfigVersion.cmake"
    VERSION 1.0.0 COMPATIBILITY SameMajorVersion)
install(TARGETS typego_plugin_sdk EXPORT TypeGoPluginSDKTargets)
install(EXPORT TypeGoPluginSDKTargets NAMESPACE TypeGoPlugin:: DESTINATION lib/cmake/TypeGoPluginSDK)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../examples/plugin-demo" DESTINATION examples)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../docs/plugin-api.md" DESTINATION docs)
install(FILES cmake/TypeGoPlugin.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/TypeGoPluginSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TypeGoPluginSDKConfigVersion.cmake"
    DESTINATION lib/cmake/TypeGoPluginSDK)
