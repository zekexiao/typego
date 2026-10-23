cmake_minimum_required(VERSION 3.20)
project(TypeGoPluginDemo LANGUAGES CXX)
find_package(TypeGoPluginSDK 1.0 REQUIRED CONFIG)
typego_add_plugin(typego_demo METADATA "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" SOURCES DemoPlugin.cpp)
