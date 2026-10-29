cmake_minimum_required(VERSION 3.20)
project(ScreenshotPlugin LANGUAGES CXX)
if(NOT TARGET TypeGoPlugin::SDK)
    find_package(TypeGoPluginSDK 1.0 REQUIRED CONFIG)
endif()
find_package(Qt6 REQUIRED COMPONENTS LinguistTools Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Screenshot_TS_FILES translations/Screenshot_zh_CN.ts)
qt_add_translation(Screenshot_QM_FILES ${Screenshot_TS_FILES})

typego_add_plugin(screenshot_plugin METADATA "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" SOURCES
    ScreenshotPlugin.cpp ScreenshotPlugin.h
    ScreenshotSession.cpp ScreenshotSession.h
    ScreenOverlay.cpp ScreenOverlay.h
    ScreenInfo.h
    CaptureToolbar.cpp CaptureToolbar.h
    Annotation.cpp Annotation.h
    ShotIcons.cpp ShotIcons.h
    PinWindow.cpp PinWindow.h
    ScreenshotStore.cpp ScreenshotStore.h
    ScreenshotSettingsWidget.cpp ScreenshotSettingsWidget.h
    WinCapture.cpp WinCapture.h
    ${Screenshot_QM_FILES})
target_compile_options(screenshot_plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(screenshot_plugin PRIVATE Qt6::Gui Qt6::Widgets user32 gdi32 dwmapi)

# 下游按插件 ID 查找包目录，目录名必须与元信息 id 一致。
set(SCREENSHOT_PACKAGE_DIR "${CMAKE_BINARY_DIR}/bin/plugins/screenshot")
set_target_properties(screenshot_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${SCREENSHOT_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${SCREENSHOT_PACKAGE_DIR}")
add_custom_command(TARGET screenshot_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:screenshot_plugin>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${Screenshot_QM_FILES}
        "$<TARGET_FILE_DIR:screenshot_plugin>/translations/Screenshot_zh_CN.qm"
    VERBATIM)
set_property(TARGET screenshot_plugin APPEND PROPERTY LINK_DEPENDS "${Screenshot_QM_FILES}")

install(TARGETS screenshot_plugin LIBRARY DESTINATION plugins/screenshot
    RUNTIME DESTINATION plugins/screenshot)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/screenshot_plugin-metadata/$<CONFIG>/typego-plugin.json"
    DESTINATION plugins/screenshot)
install(FILES ${Screenshot_QM_FILES} DESTINATION plugins/screenshot/translations)