import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true

    background: Rectangle {
        color: "#1e1e2e"
    }

    signal saved()
    signal cancelled()

    property int _originalModifiers: settings.hotkeyModifiers
    property int _originalKey: settings.hotkeyKey
    property string _originalSearchEngine: settings.searchEngine

    function keyToIndex(key) {
        var map = {
            0x20: 0,   0x41: 1,   0x42: 2,   0x43: 3,   0x44: 4,
            0x45: 5,   0x46: 6,   0x47: 7,   0x48: 8,   0x49: 9,
            0x4A: 10,  0x4B: 11,  0x4C: 12,  0x4D: 13,  0x4E: 14,
            0x4F: 15,  0x50: 16,  0x51: 17,  0x52: 18,  0x53: 19,
            0x54: 20,  0x55: 21,  0x56: 22,  0x57: 23,  0x58: 24,
            0x59: 25,  0x5A: 26,
            0x30: 27,  0x31: 28,  0x32: 29,  0x33: 30,  0x34: 31,
            0x35: 32,  0x36: 33,  0x37: 34,  0x38: 35,  0x39: 36,
            0x70: 37,  0x71: 38,  0x72: 39,  0x73: 40,  0x74: 41,
            0x75: 42,  0x76: 43,  0x77: 44,  0x78: 45,  0x79: 46,
            0x7A: 47,  0x7B: 48
        }
        return map[key] !== undefined ? map[key] : 0
    }

    function indexToKey(idx) {
        var keyMap = [
            0x20, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53,
            0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
            0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
            0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
            0x7A, 0x7B
        ]
        return keyMap[idx] || 0x20
    }

    function restoreState() {
        altCheck.checked = (_originalModifiers & Qt.AltModifier) !== 0
        ctrlCheck.checked = (_originalModifiers & Qt.ControlModifier) !== 0
        shiftCheck.checked = (_originalModifiers & Qt.ShiftModifier) !== 0
        keyCombo.currentIndex = keyToIndex(_originalKey)
        var engines = ["Bing", "Google", "Baidu", "DuckDuckGo"]
        var idx = engines.indexOf(_originalSearchEngine)
        engineCombo.currentIndex = idx >= 0 ? idx : 0
    }

    Component.onCompleted: restoreState()

    ColumnLayout {
        id: settingsLayout
        width: root.availableWidth
        anchors.margins: 20
        spacing: 20

        Text { 
            font.pixelSize: 16
            text: qsTr("Settings")
            color: "#cdd6f4"
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#45475a"
        }

        Text { 
            font.pixelSize: 16
            text: qsTr("Global Hotkey")
            color: "#89b4fa"
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            CheckBox {
                id: altCheck
                text: "Alt"
                padding: 0
                spacing: 6
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: altCheck.leftPadding
                    y: altCheck.topPadding + (altCheck.availableHeight - height) / 2
                    radius: 4
                    color: altCheck.down ? "#45475a" : "#313244"
                    border.color: altCheck.checked ? "#89b4fa" : "#45475a"
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: "#89b4fa"
                        font.pixelSize: 10
                        visible: altCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: altCheck.text
                    color: "#cdd6f4"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: altCheck.indicator.width + altCheck.spacing
                }
            }

            CheckBox {
                id: ctrlCheck
                text: "Ctrl"
                padding: 0
                spacing: 6
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: ctrlCheck.leftPadding
                    y: ctrlCheck.topPadding + (ctrlCheck.availableHeight - height) / 2
                    radius: 4
                    color: ctrlCheck.down ? "#45475a" : "#313244"
                    border.color: ctrlCheck.checked ? "#89b4fa" : "#45475a"
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: "#89b4fa"
                        font.pixelSize: 10
                        visible: ctrlCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: ctrlCheck.text
                    color: "#cdd6f4"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: ctrlCheck.indicator.width + ctrlCheck.spacing
                }
            }

            CheckBox {
                id: shiftCheck
                text: "Shift"
                padding: 0
                spacing: 6
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: shiftCheck.leftPadding
                    y: shiftCheck.topPadding + (shiftCheck.availableHeight - height) / 2
                    radius: 4
                    color: shiftCheck.down ? "#45475a" : "#313244"
                    border.color: shiftCheck.checked ? "#89b4fa" : "#45475a"
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: "#89b4fa"
                        font.pixelSize: 10
                        visible: shiftCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: shiftCheck.text
                    color: "#cdd6f4"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: shiftCheck.indicator.width + shiftCheck.spacing
                }
            }
        }

        ComboBox {
            id: keyCombo
            Layout.fillWidth: true
            model: ["Space","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","0","1","2","3","4","5","6","7","8","9","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"]

            contentItem: Text {
                font.pixelSize: 16
                text: keyCombo.displayText
                color: "#cdd6f4"
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }

            background: Rectangle {
                color: "#313244"
                radius: 6
            }

            popup: Popup {
                y: keyCombo.height
                width: keyCombo.width
                implicitHeight: Math.min(contentItem.implicitHeight, 200)
                padding: 1

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: keyCombo.delegateModel
                    currentIndex: keyCombo.highlightedIndex
                    ScrollIndicator.vertical: ScrollIndicator {}
                }

                background: Rectangle {
                    color: "#313244"
                    radius: 6
                }
            }

            delegate: ItemDelegate {
                width: keyCombo.width
                highlighted: keyCombo.highlightedIndex === index

                contentItem: Text {
                    font.pixelSize: 16
                    text: modelData
                    color: "#cdd6f4"
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: highlighted ? "#45475a" : "transparent"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#45475a"
        }

        Text { 
            font.pixelSize: 16
            text: qsTr("Search Engine")
            color: "#89b4fa"
            font.bold: true
        }

        ComboBox {
            id: engineCombo
            Layout.fillWidth: true
            model: ["Bing", "Google", "Baidu", "DuckDuckGo"]

            contentItem: Text {
                font.pixelSize: 16
                text: engineCombo.displayText
                color: "#cdd6f4"
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }

            background: Rectangle {
                color: "#313244"
                radius: 6
            }

            popup: Popup {
                y: engineCombo.height
                width: engineCombo.width
                implicitHeight: Math.min(contentItem.implicitHeight, 200)
                padding: 1

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: engineCombo.delegateModel
                    currentIndex: engineCombo.highlightedIndex
                    ScrollIndicator.vertical: ScrollIndicator {}
                }

                background: Rectangle {
                    color: "#313244"
                    radius: 6
                }
            }

            delegate: ItemDelegate {
                width: engineCombo.width
                highlighted: engineCombo.highlightedIndex === index

                contentItem: Text {
                    font.pixelSize: 16
                    text: modelData
                    color: "#cdd6f4"
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: highlighted ? "#45475a" : "transparent"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#45475a"
        }

        Text { 
            font.pixelSize: 16
            text: qsTr("Plugin Management")
            color: "#89b4fa"
            font.bold: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: ListModel { id: pluginModel }

                delegate: CheckBox {
                    Layout.fillWidth: true
                    checked: model.enabled
                    padding: 0
                    spacing: 6
                    onCheckedChanged: {
                        if (model.enabled !== checked) {
                            pluginModel.setProperty(index, "enabled", checked)
                        }
                    }
                    indicator: Rectangle {
                        id: pluginIndicator
                        implicitWidth: 18
                        implicitHeight: 18
                        x: parent.leftPadding
                        y: parent.topPadding + (parent.availableHeight - height) / 2
                        radius: 4
                        color: parent.down ? "#45475a" : "#313244"
                        border.color: parent.checked ? "#89b4fa" : "#45475a"
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            color: "#89b4fa"
                            font.pixelSize: 10
                            visible: pluginIndicator.parent.checked
                        }
                    }
                    contentItem: Text {
                        font.pixelSize: 16
                        text: model.cmd + "  |  " + model.desc
                        color: "#cdd6f4"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }
            }
        }

        function refreshPlugins() {
            pluginModel.clear()
            var cmds = pluginManager.commands()
            for (var i = 0; i < cmds.length; i++) {
                pluginModel.append({
                    cmd: cmds[i],
                    desc: pluginManager.commandDescription(cmds[i]),
                    enabled: settings.isPluginEnabled(cmds[i])
                })
            }
        }

        Component.onCompleted: refreshPlugins()

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Cancel")
                onClicked: {
                    restoreState()
                    settingsLayout.refreshPlugins()
                    root.cancelled()
                }

                background: Rectangle {
                    color: parent.down ? "#45475a" : "#313244"
                    radius: 6
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: parent.text
                    color: "#cdd6f4"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Save")
                onClicked: {
                    var mods = 0
                    if (altCheck.checked) mods |= Qt.AltModifier
                    if (ctrlCheck.checked) mods |= Qt.ControlModifier
                    if (shiftCheck.checked) mods |= Qt.ShiftModifier
                    settings.hotkeyModifiers = mods
                    settings.hotkeyKey = indexToKey(keyCombo.currentIndex)
                    settings.searchEngine = engineCombo.currentValue
                    root._originalModifiers = mods
                    root._originalKey = settings.hotkeyKey
                    root._originalSearchEngine = settings.searchEngine
                    for (var i = 0; i < pluginModel.count; i++) {
                        var item = pluginModel.get(i)
                        settings.setPluginEnabled(item.cmd, item.enabled)
                    }
                    root.saved()
                }

                background: Rectangle {
                    color: parent.down ? "#74c7ec" : "#89b4fa"
                    radius: 6
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: parent.text
                    color: "#1e1e2e"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
