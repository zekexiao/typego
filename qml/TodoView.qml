import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#1e1e2e"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: newTaskField
                Layout.fillWidth: true
                placeholderText: qsTr("Enter a new task...")
                font.pixelSize: 14
                color: "#cdd6f4"
                wrapMode: TextEdit.NoWrap
                topPadding: 8
                bottomPadding: 8
                leftPadding: 12
                rightPadding: 12

                background: Rectangle {
                    color: "#313244"
                    radius: 6
                    border.color: newTaskField.activeFocus ? "#89b4fa" : "#45475a"
                    border.width: 2
                }

                Keys.onReturnPressed: addButton.clicked()
                Keys.onEnterPressed: addButton.clicked()
            }

            Button {
                id: addButton
                text: qsTr("Add")
                implicitWidth: 60
                implicitHeight: 36
                onClicked: {
                    var t = newTaskField.text.trim()
                    if (t.length > 0) {
                        taskTreeModel.addTask(t)
                        newTaskField.text = ""
                        newTaskField.forceActiveFocus()
                    }
                }

                background: Rectangle {
                    color: parent.down ? "#74c7ec" : "#89b4fa"
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#1e1e2e"
                    font.bold: true
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        TreeView {
            id: treeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            clip: true
            model: taskTreeModel
            alternatingRows: false

            Component.onCompleted: {
                function expandRecursive(parentIndex) {
                    var count = taskTreeModel.rowCount(parentIndex)
                    for (var row = 0; row < count; row++) {
                        var idx = taskTreeModel.index(row, 0, parentIndex)
                        if (taskTreeModel.hasChildren(idx)) {
                            expand(idx)
                            expandRecursive(idx)
                        }
                    }
                }
                expandRecursive(undefined)
            }

            columnWidthProvider: function(column) { return width }

            delegate: TreeViewDelegate {
                id: treeDelegate
                implicitHeight: 42
                topPadding: 4
                bottomPadding: 4
                leftMargin: 8
                rightMargin: 8

                // 点击整行展开/折叠（子组件 CheckBox/TextField/Button 会优先处理自己的点击）
                TapHandler {
                    onTapped: {
                        var idx = treeView.index(treeDelegate.row, 0)
                        if (treeDelegate.hasChildren !== undefined && treeDelegate.hasChildren) {
                            if (treeDelegate.expanded !== undefined && treeDelegate.expanded)
                                treeView.collapse(idx)
                            else
                                treeView.expand(idx)
                        }
                    }
                }

                indicator: Item {
                    width: implicitWidth
                    height: implicitHeight
                    implicitWidth: 28
                    implicitHeight: 28
                    x: !treeDelegate.mirrored
                       ? (treeDelegate.leftMargin + treeDelegate.depth * treeDelegate.indentation)
                       : treeDelegate.width - (treeDelegate.leftMargin + treeDelegate.depth * treeDelegate.indentation) - width
                    y: (treeDelegate.height - height) / 2
                    visible: treeDelegate.hasChildren !== undefined && treeDelegate.hasChildren

                    Text {
                        anchors.centerIn: parent
                        text: (treeDelegate.expanded !== undefined && treeDelegate.expanded) ? "\u25bc" : "\u25b6"
                        color: "#6c7086"
                        font.pixelSize: 12
                    }
                }

                background: Rectangle {
                    color: (treeDelegate.current !== undefined && treeDelegate.current) ? "#313244" : (treeDelegate.hovered ? "#252537" : "transparent")
                    radius: 4
                }

                contentItem: RowLayout {
                    spacing: 10

                    CheckBox {
                        id: taskCheckBox
                        checked: model.checkState !== undefined && model.checkState === Qt.Checked
                        padding: 0
                        spacing: 0
                        onClicked: {
                            var tid = model.toolTip !== undefined ? model.toolTip : ""
                            if (tid !== "") taskTreeModel.toggleCompleted(tid)
                        }
                        indicator: Rectangle {
                            id: cbIndicator
                            implicitWidth: 18
                            implicitHeight: 18
                            x: taskCheckBox.leftPadding
                            y: taskCheckBox.topPadding + (taskCheckBox.availableHeight - height) / 2
                            radius: 4
                            color: taskCheckBox.down ? "#45475a" : "#313244"
                            border.color: taskCheckBox.checked ? "#89b4fa" : "#45475a"
                            border.width: 2
                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"
                                color: "#89b4fa"
                                font.pixelSize: 11
                                font.bold: true
                                visible: cbIndicator.parent.checked
                            }
                        }
                    }

                    TextField {
                        id: titleField
                        text: model.display !== undefined ? model.display : ""
                        color: (model.checkState !== undefined && model.checkState === Qt.Checked) ? "#6c7086" : "#cdd6f4"
                        font.pixelSize: 14
                        background: null
                        padding: 0
                        selectByMouse: true
                        Layout.fillWidth: true
                        verticalAlignment: Text.AlignVCenter
                        onEditingFinished: {
                            var tid = model.toolTip !== undefined ? model.toolTip : ""
                            if (tid !== "") taskTreeModel.updateTitle(tid, text)
                        }
                    }

                    Button {
                        text: "+"
                        implicitWidth: 28
                        implicitHeight: 28
                        visible: treeDelegate.depth !== undefined && treeDelegate.depth < 4
                        onClicked: {
                            var tid = model.toolTip !== undefined ? model.toolTip : ""
                            if (tid !== "") taskTreeModel.addSubTask(tid, "")
                        }
                        background: Rectangle {
                            color: parent.down ? "#45475a" : "transparent"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#89b4fa"
                            font.pixelSize: 16
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        text: "\u00d7"
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: {
                            var tid = model.toolTip !== undefined ? model.toolTip : ""
                            if (tid !== "") taskTreeModel.removeTask(tid)
                        }
                        background: Rectangle {
                            color: parent.down ? "#45475a" : "transparent"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#f38ba8"
                            font.pixelSize: 16
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
