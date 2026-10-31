add_library(screenshot_plugin STATIC ScreenshotPlugin.cpp ScreenshotSession.cpp ScrollCaptureSession.cpp ScrollStitcher.cpp ScrollResultWindow.cpp ScreenOverlay.cpp CaptureToolbar.cpp Annotation.cpp ShotIcons.cpp PinWindow.cpp PinEditor.cpp ScreenshotStore.cpp ScreenshotSettingsWidget.cpp WinCapture.cpp)
target_link_libraries(screenshot_plugin PUBLIC Qt6::Widgets Qt6::Gui)
target_include_directories(screenshot_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(screenshot_plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
