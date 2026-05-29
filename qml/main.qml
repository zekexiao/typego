import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QWindowKit 1.0

Window {
    id: root
    objectName: "mainWindow"
    width: 640
    minimumWidth: 640
    minimumHeight: 140
    maximumHeight: 600
    height: titleBar.height + contentLayout.implicitHeight + 40
    visible: false
    title: "neoStart"

    color: themeManager.bg

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

    Timer {
        id: blurHideTimer
        interval: 50
        onTriggered: {
            if (!root.active && settings.autoHideOnFocusLoss) {
                root.hide()
            }
        }
    }

    onActiveChanged: {
        if (!active && settings.autoHideOnFocusLoss) {
            blurHideTimer.start()
        }
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
                if (t.trim().length === 0) {
                    results = t.length === 0 ? [] : appLauncher.search("")
                } else {
                    results = appLauncher.search(t)
                }
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

        Image {
            id: titleIcon
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 12
            }
            source: "qrc:/assets/icon.svg"
            width: 18
            height: 18
            sourceSize.width: 18
            sourceSize.height: 18
        }

        Text {
            anchors {
                verticalCenter: parent.verticalCenter
                left: titleIcon.right
                leftMargin: 8
            }
            text: root.title
            color: themeManager.text
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
                color: closeButtonArea.containsMouse ? themeManager.closeButtonHover : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    color: themeManager.text
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
        results = []
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
        id: contentLayout
        anchors.top: titleBar.bottom
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
                color: themeManager.text
                wrapMode: TextEdit.NoWrap
                topPadding: 12
                bottomPadding: 12
                leftPadding: 12
                rightPadding: 12

                background: Rectangle {
                    color: themeManager.surface
                    radius: 8
                    border.color: searchField.activeFocus ? themeManager.borderFocus : themeManager.border
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
            color: themeManager.hover
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
                    color: themeManager.accent
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    text: "|"
                    color: themeManager.border
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
                        if (!commandMode) return themeManager.textSecondary
                        var desc = pluginManager.commandDescription(currentCmd)
                        return desc ? themeManager.textSecondary : themeManager.error
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
                    color: themeManager.text
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        Flow {
            id: pinnedFlow
            Layout.fillWidth: true
            spacing: 12
            Layout.preferredHeight: (searchField.text.length === 0 && !commandMode && !customViewVisible) ? implicitHeight : 0
            visible: Layout.preferredHeight > 0
            clip: true

            Text {
                width: parent.width
                text: qsTr("Pinned")
                color: themeManager.textSecondary
                font.pixelSize: 12
                visible: appLauncher.pinnedApps.length > 0
            }

            Repeater {
                model: appLauncher.pinnedApps
                delegate: Rectangle {
                    width: 48
                    height: 48
                    radius: 24
                    color: themeManager.surface
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: modelData.icon ? "file:///" + modelData.icon : ""
                        visible: modelData.icon && modelData.icon.length > 0
                        fillMode: Image.PreserveAspectCrop
                    }

                    Text {
                        anchors.centerIn: parent
                        text: modelData.name.charAt(0).toUpperCase()
                        color: themeManager.text
                        font.pixelSize: 18
                        font.bold: true
                        visible: !modelData.icon || modelData.icon.length === 0
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: {
                            if (mouse.button === Qt.RightButton) {
                                pinContextMenu.popup()
                            } else {
                                appLauncher.launch(modelData.path)
                            }
                        }
                        ToolTip.visible: containsMouse
                        ToolTip.text: modelData.name
                        ToolTip.delay: 300

                        Menu {
                            id: pinContextMenu
                            MenuItem {
                                text: qsTr("Unpin from Start")
                                onTriggered: appLauncher.unpinApp(modelData.path)
                            }
                        }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: customViewVisible ? 320 : (results.length > 0 ? Math.min(listView.contentHeight, 400) : 0)
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
                    color: index === selectedIndex ? themeManager.selection : "transparent"

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
                            color: themeManager.iconPlaceholder
                            visible: !modelData.icon || modelData.icon.length === 0
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                anchors.centerIn: parent
                                text: (modelData.name !== undefined ? modelData.name : modelData.title).charAt(0).toUpperCase()
                                color: themeManager.text
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Text {
                            text: modelData.title !== undefined ? modelData.title : modelData.name
                            color: themeManager.text
                            font.pixelSize: 15
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.subtitle !== undefined ? modelData.subtitle : modelData.desc
                            color: themeManager.textSecondary
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: {
                            if (mouse.button === Qt.RightButton) {
                                contextMenu.popup()
                            } else {
                                selectedIndex = index
                                listView.launchCurrent()
                            }
                        }

                        Menu {
                            id: contextMenu
                            MenuItem {
                                text: {
                                    var p = modelData.path !== undefined ? modelData.path : ""
                                    if (p.length === 0) return qsTr("Cannot pin")
                                    return appLauncher.isPinned(p) ? qsTr("Unpin from Start") : qsTr("Pin to Start")
                                }
                                enabled: modelData.path !== undefined && modelData.path.length > 0
                                onTriggered: {
                                    var p = modelData.path
                                    if (appLauncher.isPinned(p)) {
                                        appLauncher.unpinApp(p)
                                    } else {
                                        var n = modelData.name !== undefined ? modelData.name : modelData.title
                                        var ic = modelData.icon !== undefined ? modelData.icon : ""
                                        appLauncher.pinApp(p, n, ic)
                                    }
                                }
                            }
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
