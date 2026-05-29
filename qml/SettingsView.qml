import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: themeManager.bg

    signal saved()
    signal cancelled()

    property int _originalModifiers: settings.hotkeyModifiers
    property int _originalKey: settings.hotkeyKey
    property string _originalSearchEngine: settings.searchEngine
    property string _originalTheme: settings.theme

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
        autoEnterCheck.checked = settings.pluginAutoEnter
        autoStartCheck.checked = settings.autoStart
        floatIconCheck.checked = settings.showFloatIcon
        autoHideCheck.checked = settings.autoHideOnBlur
        var themes = ["system", "light", "dark"]
        var tidx = themes.indexOf(_originalTheme)
        themeCombo.currentIndex = tidx >= 0 ? tidx : 0
    }

    Component.onCompleted: restoreState()

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: width
        contentHeight: settingsLayout.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: settingsLayout
            width: parent.width
            spacing: 12

        Text { 
            font.pixelSize: 16
            text: qsTr("Settings")
            color: themeManager.text
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themeManager.border
        }

        Text { 
            font.pixelSize: 16
            text: qsTr("Global Hotkey")
            color: themeManager.accent
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
                    color: altCheck.down ? themeManager.surfacePressed : themeManager.surface
                    border.color: altCheck.checked ? themeManager.borderFocus : themeManager.border
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: themeManager.accent
                        font.pixelSize: 10
                        visible: altCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: altCheck.text
                    color: themeManager.text
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
                    color: ctrlCheck.down ? themeManager.surfacePressed : themeManager.surface
                    border.color: ctrlCheck.checked ? themeManager.borderFocus : themeManager.border
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: themeManager.accent
                        font.pixelSize: 10
                        visible: ctrlCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: ctrlCheck.text
                    color: themeManager.text
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
                    color: shiftCheck.down ? themeManager.surfacePressed : themeManager.surface
                    border.color: shiftCheck.checked ? themeManager.borderFocus : themeManager.border
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: themeManager.accent
                        font.pixelSize: 10
                        visible: shiftCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: shiftCheck.text
                    color: themeManager.text
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
                color: themeManager.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }

            background: Rectangle {
                color: themeManager.surface
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
                    color: themeManager.surface
                    radius: 6
                }
            }

            delegate: ItemDelegate {
                width: keyCombo.width
                highlighted: keyCombo.highlightedIndex === index

                contentItem: Text {
                    font.pixelSize: 16
                    text: modelData
                    color: themeManager.text
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: highlighted ? themeManager.selection : "transparent"
                }
            }
        }

        CheckBox {
            id: autoStartCheck
            text: qsTr("Start on boot")
            padding: 0
            spacing: 6
            checked: settings.autoStart
            indicator: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                x: autoStartCheck.leftPadding
                y: autoStartCheck.topPadding + (autoStartCheck.availableHeight - height) / 2
                radius: 4
                color: autoStartCheck.down ? themeManager.surfacePressed : themeManager.surface
                border.color: autoStartCheck.checked ? themeManager.borderFocus : themeManager.border
                border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    color: themeManager.accent
                    font.pixelSize: 10
                    visible: autoStartCheck.checked
                }
            }
            contentItem: Text {
                font.pixelSize: 16
                text: autoStartCheck.text
                color: themeManager.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: autoStartCheck.indicator.width + autoStartCheck.spacing
            }
        }

        CheckBox {
            id: floatIconCheck
            text: qsTr("Show float icon")
            padding: 0
            spacing: 6
            checked: settings.showFloatIcon
            indicator: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                x: floatIconCheck.leftPadding
                y: floatIconCheck.topPadding + (floatIconCheck.availableHeight - height) / 2
                radius: 4
                color: floatIconCheck.down ? themeManager.surfacePressed : themeManager.surface
                border.color: floatIconCheck.checked ? themeManager.borderFocus : themeManager.border
                border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    color: themeManager.accent
                    font.pixelSize: 10
                    visible: floatIconCheck.checked
                }
            }
            contentItem: Text {
                font.pixelSize: 16
                text: floatIconCheck.text
                color: themeManager.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: floatIconCheck.indicator.width + floatIconCheck.spacing
            }
        }

        CheckBox {
            id: autoHideCheck
            text: qsTr("Auto hide on blur")
            padding: 0
            spacing: 6
            checked: settings.autoHideOnFocusLoss
            indicator: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                x: autoHideCheck.leftPadding
                y: autoHideCheck.topPadding + (autoHideCheck.availableHeight - height) / 2
                radius: 4
                color: autoHideCheck.down ? themeManager.surfacePressed : themeManager.surface
                border.color: autoHideCheck.checked ? themeManager.borderFocus : themeManager.border
                border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    color: themeManager.accent
                    font.pixelSize: 10
                    visible: autoHideCheck.checked
                }
            }
            contentItem: Text {
                font.pixelSize: 16
                text: autoHideCheck.text
                color: themeManager.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: autoHideCheck.indicator.width + autoHideCheck.spacing
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themeManager.border
        }

        Text {
            font.pixelSize: 16
            text: qsTr("Theme")
            color: themeManager.accent
            font.bold: true
        }

        ComboBox {
            id: themeCombo
            Layout.fillWidth: true
            model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]

            contentItem: Text {
                font.pixelSize: 16
                text: themeCombo.displayText
                color: themeManager.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }

            background: Rectangle {
                color: themeManager.surface
                radius: 6
            }

            popup: Popup {
                y: themeCombo.height
                width: themeCombo.width
                implicitHeight: Math.min(contentItem.implicitHeight, 200)
                padding: 1

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: themeCombo.delegateModel
                    currentIndex: themeCombo.highlightedIndex
                    ScrollIndicator.vertical: ScrollIndicator {}
                }

                background: Rectangle {
                    color: themeManager.surface
                    radius: 6
                }
            }

            delegate: ItemDelegate {
                width: themeCombo.width
                highlighted: themeCombo.highlightedIndex === index

                contentItem: Text {
                    font.pixelSize: 16
                    text: modelData
                    color: themeManager.text
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: highlighted ? themeManager.selection : "transparent"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themeManager.border
        }

        Text {
            font.pixelSize: 16
            text: qsTr("Search Engine")
            color: themeManager.accent
            font.bold: true
        }

        ComboBox {
            id: engineCombo
            Layout.fillWidth: true
            model: ["Bing", "Google", "Baidu", "DuckDuckGo"]

            contentItem: Text {
                font.pixelSize: 16
                text: engineCombo.displayText
                color: themeManager.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }

            background: Rectangle {
                color: themeManager.surface
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
                    color: themeManager.surface
                    radius: 6
                }
            }

            delegate: ItemDelegate {
                width: engineCombo.width
                highlighted: engineCombo.highlightedIndex === index

                contentItem: Text {
                    font.pixelSize: 16
                    text: modelData
                    color: themeManager.text
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: highlighted ? themeManager.selection : "transparent"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themeManager.border
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                font.pixelSize: 16
                text: qsTr("Plugin Management")
                color: themeManager.accent
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            CheckBox {
                id: autoEnterCheck
                text: qsTr("Auto enter on empty args")
                padding: 0
                spacing: 6
                checked: settings.pluginAutoEnter()
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: autoEnterCheck.leftPadding
                    y: autoEnterCheck.topPadding + (autoEnterCheck.availableHeight - height) / 2
                    radius: 4
                    color: autoEnterCheck.down ? themeManager.surfacePressed : themeManager.surface
                    border.color: autoEnterCheck.checked ? themeManager.borderFocus : themeManager.border
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: themeManager.accent
                        font.pixelSize: 10
                        visible: autoEnterCheck.checked
                    }
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: autoEnterCheck.text
                    color: themeManager.text
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: autoEnterCheck.indicator.width + autoEnterCheck.spacing
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: ListModel { id: pluginModel }

                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        id: pluginIndicator
                        implicitWidth: 18
                        implicitHeight: 18
                        Layout.alignment: Qt.AlignVCenter
                        radius: 4
                        color: checkMouse.pressed ? themeManager.surfacePressed : themeManager.surface
                        border.color: model.enabled ? themeManager.borderFocus : themeManager.border
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            color: themeManager.accent
                            font.pixelSize: 10
                            visible: model.enabled
                        }
                        MouseArea {
                            id: checkMouse
                            anchors.fill: parent
                            onClicked: pluginModel.setProperty(index, "enabled", !model.enabled)
                        }
                    }

                    Text {
                        font.pixelSize: 16
                        text: model.cmd
                        color: themeManager.accent
                        font.bold: true
                        Layout.minimumWidth: 48
                        Layout.maximumWidth: 48
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        font.pixelSize: 16
                        text: "|"
                        color: themeManager.border
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        font.pixelSize: 16
                        text: model.desc
                        color: themeManager.text
                        Layout.fillWidth: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: pluginModel.setProperty(index, "enabled", !model.enabled)
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
                    color: parent.down ? themeManager.surfacePressed : themeManager.surface
                    radius: 6
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: parent.text
                    color: themeManager.text
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
                    settings.theme = ["system", "light", "dark"][themeCombo.currentIndex]
                    settings.autoStart = autoStartCheck.checked
                    settings.showFloatIcon = floatIconCheck.checked
                    settings.autoHideOnFocusLoss = autoHideCheck.checked
                    root._originalModifiers = mods
                    root._originalKey = settings.hotkeyKey
                    root._originalSearchEngine = settings.searchEngine
                    settings.pluginAutoEnter = autoEnterCheck.checked
                    for (var i = 0; i < pluginModel.count; i++) {
                        var item = pluginModel.get(i)
                        settings.setPluginEnabled(item.cmd, item.enabled)
                    }
                    root.saved()
                }

                background: Rectangle {
                    color: parent.down ? themeManager.accentPressed : themeManager.accent
                    radius: 6
                }
                contentItem: Text {
                    font.pixelSize: 16
                    text: parent.text
                    color: themeManager.bg
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    }
}
