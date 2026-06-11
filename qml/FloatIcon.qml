import QtQuick
import QtQuick.Window
import QtQuick.Controls

Item {
    id: floatRoot
    width: 48
    height: 48

    property var mainWindow: null

    Rectangle {
        id: floatButton
        width: 48
        height: 48
        radius: 24
        color: themeManager.surface

        Image {
            anchors.centerIn: parent
            source: "qrc:/assets/icon.svg"
            width: 32
            height: 32
            sourceSize.width: 32
            sourceSize.height: 32
        }

        MouseArea {
            id: dragArea
            property bool dragging: false
            property point dragStartPos
            property point windowStartPos

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onPressed: {
                dragging = false
                dragStartPos = mapToGlobal(Qt.point(mouse.x, mouse.y))
                windowStartPos = Qt.point(floatRoot.Window.window.x, floatRoot.Window.window.y)
            }

            onPositionChanged: {
                if (pressed) {
                    var globalPos = mapToGlobal(Qt.point(mouse.x, mouse.y))
                    var dx = globalPos.x - dragStartPos.x
                    var dy = globalPos.y - dragStartPos.y
                    if (Math.abs(dx) > 2 || Math.abs(dy) > 2) {
                        dragging = true
                    }
                    floatRoot.Window.window.x = windowStartPos.x + dx
                    floatRoot.Window.window.y = windowStartPos.y + dy
                }
            }

            onReleased: {
                if (mouse.button === Qt.RightButton && !dragging) {
                    floatMenu.popup()
                } else if (!dragging && mainWindow) {
                    mainWindow.show()
                    mainWindow.raise()
                    mainWindow.requestActivate()
                }
                if (dragging) {
                    settings.setFloatIconPos(Qt.point(floatRoot.Window.window.x, floatRoot.Window.window.y))
                }
                dragging = false
            }

            Menu {
                id: floatMenu
                MenuItem {
                    text: qsTr("Hide float icon")
                    onTriggered: settings.showFloatIcon = false
                }
            }
        }
    }
}
