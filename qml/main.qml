import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QWindowKit 1.0

Window {
    id: root
    objectName: "mainWindow"
    width: 640
    height: 400
    visible: false
    title: "neoStart"

    color: "#1e1e2e"

    property var results: []
    property int selectedIndex: 0
    property bool commandMode: false
    property string currentCmd: ""
    property bool customViewVisible: false
    property string customViewSource: ""

    Timer {
        id: searchDebounceTimer
        interval: 120
        onTriggered: performSearch()
    }

    function searchUrl(query) {
        var engine = settings.searchEngine
        if (engine === "Google")
            return "https://www.google.com/search?q=" + encodeURIComponent(query)
        if (engine === "Baidu")
            return "https://www.baidu.com/s?wd=" + encodeURIComponent(query)
        if (engine === "DuckDuckGo")
            return "https://duckduckgo.com/?q=" + encodeURIComponent(query)
        return "https://www.bing.com/search?q=" + encodeURIComponent(query)
    }

    function performSearch() {
        var t = searchField.text
        var firstSpace = t.indexOf(' ')
        if (firstSpace > 0) {
            commandMode = true
            currentCmd = t.substring(0, firstSpace)
            var args = t.substring(firstSpace + 1)
            customViewVisible = false
            customViewSource = ""
            // 空参数且插件支持 autoEnter，直接触发进入
            if (args.trim().length === 0 && pluginManager.autoEnterForCommand(currentCmd)) {
                results = []
                selectedIndex = 0
                launchCurrent()
                return
            }
            results = pluginManager.queryCommand(currentCmd, args)
        } else {
            var cmd = t.trim()
            if (cmd.length > 0 && pluginManager.commands().indexOf(cmd) >= 0) {
                commandMode = true
                currentCmd = cmd
                customViewVisible = false
                customViewSource = ""
                results = pluginManager.queryCommand(currentCmd, "")
            } else {
                commandMode = false
                currentCmd = ""
                customViewVisible = false
                customViewSource = ""
                results = appLauncher.search(t)
            }
        }
        // 无结果时提供浏览器搜索
        if (results.length === 0 && t.trim().length > 0) {
            var searchItem = {}
            searchItem.title = qsTr("Search \"") + t.trim() + "\""
            searchItem.subtitle = qsTr("Search with ") + settings.searchEngine
            searchItem.icon = ""
            var searchAction = {}
            searchAction.type = "openUrl"
            searchAction.data = searchUrl(t.trim())
            searchItem.action = searchAction
            results = [searchItem]
        }
        selectedIndex = 0
    }

    WindowAgent {
        id: windowAgent
    }

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        color: "transparent"
        Component.onCompleted: windowAgent.setTitleBar(titleBar)

        Text {
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 12
            }
            text: root.title
            color: "#cdd6f4"
            font.pixelSize: 13
        }

        Row {
            anchors {
                top: parent.top
                right: parent.right
            }
            height: parent.height

            Rectangle {
                id: closeButton
                width: 46
                height: parent.height
                color: closeButtonArea.containsMouse ? "#e81123" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    color: "#cdd6f4"
                    font.pixelSize: 16
                }

                MouseArea {
                    id: closeButtonArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.close()
                }

                Component.onCompleted: windowAgent.setSystemButton(WindowAgent.Close, closeButton)
            }
        }
    }

    function openSettings() {
        customViewVisible = true
        customViewSource = "qrc:/qml/SettingsView.qml"
        searchField.text = "set"
        searchField.forceActiveFocus()
    }

    function closeCustomView() {
        customViewVisible = false
        customViewSource = ""
        searchField.text = ""
        searchField.forceActiveFocus()
    }

    function moveSelection(delta) {
        var newIndex = selectedIndex + delta
        if (newIndex < 0) newIndex = 0
        if (newIndex >= results.length) newIndex = results.length - 1
        if (newIndex !== selectedIndex) {
            selectedIndex = newIndex
            listView.positionViewAtIndex(selectedIndex, ListView.Contain)
        }
    }

    Component.onCompleted: {
        windowAgent.setup(root)
        windowAgent.setWindowAttribute("dark-mode", true)
        results = appLauncher.search("")
        searchField.forceActiveFocus()
        root.visible = true
    }

    onClosing: function(closeEvent) {
        closeEvent.accepted = false
        root.hide()
    }

    // 全局快捷键：上下移动选中（不受焦点影响）
    Shortcut {
        sequence: "Down"
        onActivated: root.moveSelection(1)
    }
    Shortcut {
        sequence: "Up"
        onActivated: root.moveSelection(-1)
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: root.moveSelection(1)
    }
    Shortcut {
        sequence: "Ctrl+P"
        onActivated: root.moveSelection(-1)
    }
    Shortcut {
        sequence: "Return"
        onActivated: root.launchCurrent()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: root.hide()
    }

    ColumnLayout {
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 12

        TextArea {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                placeholderText: qsTr("Search apps or type cmd space args...")
                text: ""
                font.pixelSize: 18
                color: "#cdd6f4"
                wrapMode: TextEdit.NoWrap
                topPadding: 12
                bottomPadding: 12
                leftPadding: 12
                rightPadding: 12

                background: Rectangle {
                    color: "#313244"
                    radius: 8
                    border.color: searchField.activeFocus ? "#89b4fa" : "#45475a"
                    border.width: 2
                }

                Component.onCompleted: {
                    inputHighlighter.setup(searchField.textDocument)
                }

                onTextChanged: {
                    searchDebounceTimer.restart()
                }

                Keys.onDownPressed: {
                    if (selectedIndex < results.length - 1) selectedIndex++
                }
                Keys.onUpPressed: {
                    if (selectedIndex > 0) selectedIndex--
                }
                Keys.onReturnPressed: launchCurrent()
                Keys.onEnterPressed: launchCurrent()
            }

        // 命令模式提示栏：区分 cmd / args，高亮显示命令
        Rectangle {
            Layout.fillWidth: true
            height: commandMode ? 28 : 0
            color: "#252538"
            radius: 4
            visible: commandMode
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: currentCmd
                    color: "#89b4fa"
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    text: "|"
                    color: "#45475a"
                    font.pixelSize: 13
                    visible: cmdDesc.text.length > 0
                }

                Text {
                    id: cmdDesc
                    text: {
                        if (!commandMode) return ""
                        var desc = pluginManager.commandDescription(currentCmd)
                        return desc ? desc : qsTr("Unknown command")
                    }
                    color: {
                        if (!commandMode) return "#6c7086"
                        var desc = pluginManager.commandDescription(currentCmd)
                        return desc ? "#6c7086" : "#f38ba8"
                    }
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }

                Text {
                    text: {
                        if (!commandMode) return ""
                        var spaceIdx = searchField.text.indexOf(' ')
                        return spaceIdx > 0 ? searchField.text.substring(spaceIdx + 1) : ""
                    }
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: customViewVisible ? 1 : 0

            // 0: 默认列表视图
            ListView {
                id: listView
                clip: true
                model: results
                spacing: 4

                function launchCurrent() {
                    root.launchCurrent()
                }

                delegate: Rectangle {
                    width: listView.width
                    height: 48
                    radius: 6
                    color: index === selectedIndex ? "#45475a" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Image {
                            source: modelData.icon ? "file:///" + modelData.icon : ""
                            visible: modelData.icon && modelData.icon.length > 0
                            width: 32
                            height: 32
                            fillMode: Image.PreserveAspectFit
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Rectangle {
                            width: 32
                            height: 32
                            radius: 16
                            color: "#585b70"
                            visible: !modelData.icon || modelData.icon.length === 0
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                anchors.centerIn: parent
                                text: (modelData.name !== undefined ? modelData.name : modelData.title).charAt(0).toUpperCase()
                                color: "#cdd6f4"
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Text {
                            text: modelData.title !== undefined ? modelData.title : modelData.name
                            color: "#cdd6f4"
                            font.pixelSize: 15
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.subtitle !== undefined ? modelData.subtitle : modelData.desc
                            color: "#6c7086"
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            selectedIndex = index
                            listView.launchCurrent()
                        }
                    }
                }
            }

            // 1: 自定义视图容器
            Loader {
                id: customViewLoader
                source: customViewSource
                onLoaded: {
                    if (item.saved) {
                        item.saved.connect(function() {
                            root.closeCustomView()
                        })
                    }
                    if (item.cancelled) {
                        item.cancelled.connect(function() {
                            root.closeCustomView()
                        })
                    }
                }
            }
        }
    }

    function launchCurrent() {
        // 命令模式下优先检查是否有自定义视图
        if (commandMode && currentCmd.length > 0) {
            var viewUrl = pluginManager.viewUrlForCommand(currentCmd)
            if (viewUrl.length > 0) {
                customViewVisible = true
                customViewSource = viewUrl
                return
            }
        }
        if (selectedIndex >= 0 && selectedIndex < results.length) {
            var item = results[selectedIndex]
            if (item.action && item.action.type && item.action.type !== "none") {
                pluginManager.executeAction(item.action)
                root.hide()
            } else if (item.path) {
                appLauncher.launch(item.path)
                root.hide()
            }
        }
    }
}
