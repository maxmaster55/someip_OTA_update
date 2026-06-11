import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import OtaGui 1.0

ApplicationWindow {
    id: window
    width: 640
    height: 820
    minimumWidth: 540
    minimumHeight: 720
    visible: true
    title: "OTA Update Manager"

    Material.theme: Material.Light
    Material.accent: Material.DeepPurple
    Material.primary: Material.Indigo

    font.pixelSize: 14

    DownloadManager { id: manager }

    FileDialog {
        id: fileDialog
        title: "Select OTA Update File"
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
        nameFilters: ["Update Files (*.wic.bz2 *.wic *.bin *.bz2 *.gz)", "All Files (*)"]
        onAccepted: {
            manager.selectedFilePath = selectedFile
        }
    }

    function startServing() {
        if (manager.selectedFilePath.length === 0) return
        versionError.visible = false
        var ver = versionField.text.trim()
        if (ver.length === 0) {
            versionError.text = "Version is required"
            versionError.visible = true
            versionField.forceActiveFocus()
            return
        }
        manager.startServing()
    }

    menuBar: MenuBar {
        Material.background: Material.color(Material.Grey, Material.Shade100)

        Menu {
            title: "File"
            Action { text: "Open Update File..."; shortcut: "Ctrl+O"; onTriggered: fileDialog.open() }
            MenuSeparator { }
            Action { text: "Quit"; shortcut: "Ctrl+Q"; onTriggered: window.close() }
        }

        Menu {
            title: "Help"
            Action { text: "About"; onTriggered: aboutDialog.open() }
        }
    }

    Dialog {
        id: aboutDialog
        title: "About"
        standardButtons: Dialog.Close
        modal: true
        anchors.centerIn: parent
        width: 320

        ColumnLayout {
            spacing: 8
            Label { text: "OTA Update Manager"; font.pixelSize: 18; font.weight: Font.Bold }
            Label { text: "SOME/IP firmware update tool"; color: Material.color(Material.Grey) }
            Label { text: "Service \u2192 Relay \u2192 Daemon 3-node architecture"; font.pixelSize: 12; color: Material.color(Material.Grey) }
        }
    }

    ScrollView {
        id: scrollView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 0

            Pane {
                Layout.fillWidth: true
                implicitHeight: 72
                Material.elevation: 2
                Material.background: Material.Indigo

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Label {
                            text: "OTA Update Manager"
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: "white"
                            elide: Text.ElideMiddle
                        }
                        Label {
                            text: {
                                if (manager.fileName.length > 0) return manager.fileName
                                if (manager.selectedFilePath.length > 0) {
                                    var parts = manager.selectedFilePath.split("/")
                                    return parts[parts.length - 1]
                                }
                                return "No file selected"
                            }
                            font.pixelSize: 12
                            color: Material.color(Material.Grey, Material.Shade200)
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }

                    Item { Layout.fillWidth: true; Layout.minimumWidth: 8 }

                    RowLayout {
                        spacing: 8
                        Layout.alignment: Qt.AlignRight
                        ColumnLayout {
                            spacing: 1
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: manager.serving ? "#4ade80" : "#9ca3af"
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Label { text: "File"; font.pixelSize: 9; color: "#c7d2fe" }
                        }
                        ColumnLayout {
                            spacing: 1
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: manager.serving ? "#4ade80" : "#9ca3af"
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Label { text: "Srv"; font.pixelSize: 9; color: "#c7d2fe" }
                        }
                        ColumnLayout {
                            spacing: 1
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: manager.relayConnected ? "#4ade80" : "#ef4444"
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Label { text: "Relay"; font.pixelSize: 9; color: "#c7d2fe" }
                        }
                    }
                }
            }

            Pane {
                Layout.fillWidth: true
                Material.elevation: 0

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 20

                    // ─── SECTION: File Selection ───────────────────────────
                    Pane {
                        Layout.fillWidth: true
                        Material.elevation: 1
                        padding: 16

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Label {
                                text: "1. Select Update File"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: Material.color(Material.Grey, Material.Shade700)
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 48
                                radius: 6
                                color: manager.selectedFilePath.length > 0
                                       ? Material.color(Material.Green, Material.Shade50)
                                       : Material.color(Material.Grey, Material.Shade100)
                                border.color: manager.selectedFilePath.length > 0
                                              ? Material.color(Material.Green, Material.Shade300)
                                              : Material.color(Material.Grey, Material.Shade300)
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 4
                                    spacing: 8

                                    Label {
                                        text: manager.selectedFilePath.length > 0
                                              ? manager.selectedFilePath.split("/").slice(-2).join("/")
                                              : "Click Browse to select a firmware file"
                                        color: manager.selectedFilePath.length > 0
                                               ? Material.foreground : Material.color(Material.Grey)
                                        font.pixelSize: 13
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }

                                    Button {
                                        text: "Browse"
                                        implicitHeight: 36
                                        implicitWidth: 90
                                        Material.background: Material.accent
                                        Material.foreground: "white"
                                        onClicked: fileDialog.open()
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "Version:"
                                    color: Material.color(Material.Grey)
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                TextField {
                                    id: versionField
                                    Layout.preferredWidth: 100
                                    placeholderText: "Required"
                                    leftPadding: 8
                                    verticalAlignment: TextInput.AlignVCenter
                                    background: Rectangle {
                                        radius: 4
                                        color: Material.color(Material.Grey, Material.Shade100)
                                        border.color: versionError.visible
                                                   ? "#dc2626"
                                                   : Material.color(Material.Grey, Material.Shade300)
                                        border.width: versionError.visible ? 2 : 1
                                    }
                                    onTextChanged: {
                                        manager.versionOverride = text.trim()
                                        versionError.visible = false
                                    }
                                }

                                Label {
                                    id: versionError
                                    text: ""
                                    visible: false
                                    font.pixelSize: 11
                                    color: "#dc2626"
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    text: manager.serving ? "Serving..." : "Serve Firmware"
                                    enabled: manager.selectedFilePath.length > 0 && !manager.serving
                                    implicitHeight: 40
                                    implicitWidth: 120
                                    Material.background: Material.Green
                                    Material.foreground: "white"
                                    font.weight: Font.Medium
                                    onClicked: startServing()
                                }

                                Label {
                                    text: {
                                        if (manager.serving) return "Serving"
                                        if (manager.fileInfo.length > 0) return "Ready"
                                        return ""
                                    }
                                    font.pixelSize: 11
                                    color: manager.serving ? "#16a34a"
                                         : manager.fileInfo.length > 0 ? "#f59e0b"
                                         : Material.color(Material.Grey)
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }
                        }
                    }

                    // ─── SECTION: Download Progress ──────────────────────
                    Pane {
                        Layout.fillWidth: true
                        visible: manager.relayProgress > 0
                                 || manager.relayState.indexOf("downloading") >= 0
                                 || manager.fileInfo.length > 0
                        Material.elevation: 1
                        padding: 16

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            Label {
                                text: "2. Download to Relay"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: Material.color(Material.Grey, Material.Shade700)
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 20
                                radius: 10
                                color: Material.color(Material.Grey, Material.Shade200)

                                Rectangle {
                                    height: parent.height
                                    width: parent.width * Math.min(manager.relayProgress, 1.0)
                                    radius: 10
                                    color: Material.accent
                                    Behavior on width { NumberAnimation { duration: 300 } }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: {
                                        if (manager.relayState.indexOf("downloading") >= 0)
                                            return "Downloading..."
                                        if (manager.relayState.indexOf("installing") >= 0)
                                            return "Installing..."
                                        if (manager.relayProgress >= 1.0)
                                            return "Complete"
                                        return ""
                                    }
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: {
                                        if (manager.relayProgress >= 1.0) return "#16a34a"
                                        if (manager.relayState.indexOf("error") >= 0) return "#dc2626"
                                        return Material.accent
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: Math.floor(manager.relayProgress * 100) + "%"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    color: Material.accent
                                    visible: manager.relayProgress > 0
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: manager.fileInfo
                                font.pixelSize: 11
                                color: Material.color(Material.Grey, Material.Shade600)
                                wrapMode: Text.WordWrap
                                visible: manager.fileInfo.length > 0
                            }
                        }
                    }

                    // ─── SECTION: Pipeline (Fetch → Send → Install) ──────
                    Pane {
                        Layout.fillWidth: true
                        Material.elevation: 1
                        padding: 16

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Label {
                                text: "3. Pipeline"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: Material.color(Material.Grey, Material.Shade700)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Button {
                                    text: "1. Fetch to Relay"
                                    enabled: manager.relayConnected
                                    implicitHeight: 52
                                    Layout.fillWidth: true
                                    Material.background: Material.Indigo
                                    Material.foreground: "white"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    onClicked: manager.sendRelayCommand(0)
                                }

                                Button {
                                    text: "2. Send to Daemon"
                                    enabled: manager.relayConnected
                                    implicitHeight: 52
                                    Layout.fillWidth: true
                                    Material.background: Material.DeepOrange
                                    Material.foreground: "white"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    onClicked: manager.sendToDaemon()
                                }

                                Button {
                                    text: "3. Install"
                                    enabled: manager.relayConnected
                                    implicitHeight: 52
                                    Layout.fillWidth: true
                                    Material.background: Material.Green
                                    Material.foreground: "white"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    onClicked: manager.triggerDaemonInstall()
                                }

                                Button {
                                    text: "Cancel"
                                    enabled: manager.relayConnected
                                    implicitHeight: 52
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 80
                                    Material.background: Material.Red
                                    Material.foreground: "white"
                                    font.pixelSize: 14
                                    onClicked: manager.sendRelayCommand(3)
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(32, stateText.height + 16)
                                radius: 6
                                color: Material.color(Material.Grey, Material.Shade50)
                                border.color: Material.color(Material.Grey, Material.Shade200)
                                border.width: 1
                                visible: manager.relayState !== "Not connected"

                                Label {
                                    id: stateText
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    text: manager.relayState
                                    font.pixelSize: 11
                                    color: Material.color(Material.Grey, Material.Shade700)
                                    wrapMode: Text.WordWrap
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    // ─── SECTION: Utilities ──────────────────────────────
                    Pane {
                        Layout.fillWidth: true
                        Material.elevation: 1
                        padding: 16

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Label {
                                text: "4. Utilities"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: Material.color(Material.Grey, Material.Shade700)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Button {
                                    text: "Get Status"
                                    enabled: manager.relayConnected
                                    implicitHeight: 40
                                    Layout.fillWidth: true
                                    onClicked: manager.sendRelayCommand(4)
                                }

                                Button {
                                    text: "Installed Ver"
                                    enabled: manager.relayConnected
                                    implicitHeight: 40
                                    Layout.fillWidth: true
                                    onClicked: manager.getRelayVersion()
                                }

                                Button {
                                    text: "Schedule"
                                    enabled: manager.relayConnected
                                    implicitHeight: 40
                                    Layout.fillWidth: true
                                    onClicked: manager.sendRelayCommand(1, 30)
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(32, outputLabel.height + 16)
                                radius: 6
                                color: Material.color(Material.DeepPurple, Material.Shade50)
                                border.color: Material.color(Material.DeepPurple, Material.Shade200)
                                border.width: 1
                                visible: manager.relayOutput.length > 0

                                Flickable {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    contentHeight: outputLabel.height
                                    clip: true
                                    interactive: outputLabel.height > 40

                                    Label {
                                        id: outputLabel
                                        width: parent.width
                                        text: manager.relayOutput
                                        font.pixelSize: 11
                                        color: Material.accent
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true; Layout.minimumHeight: 8 }
                }
            }
        }
    }

    footer: Pane {
        Material.elevation: 3
        Material.background: Material.color(Material.Grey, Material.Shade100)
        padding: 10

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 8

            Label {
                text: manager.status
                font.pixelSize: 12
                font.weight: Font.Medium
                color: {
                    if (manager.relayState.indexOf("error") >= 0) return "#dc2626"
                    if (manager.relayProgress >= 1.0) return "#16a34a"
                    if (manager.relayConnected) return Material.accent
                    return Material.color(Material.Grey)
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 6
                Repeater {
                    model: [
                        { label: "File", color: manager.serving ? "#4ade80" : "#9ca3af" },
                        { label: "Srv", color: manager.serving ? "#4ade80" : "#9ca3af" },
                        { label: "Relay", color: manager.relayConnected ? "#4ade80" : "#ef4444" }
                    ]
                    delegate: RowLayout {
                        spacing: 3
                        Rectangle {
                            width: 7; height: 7; radius: 3
                            color: modelData.color
                        }
                        Label {
                            text: modelData.label
                            font.pixelSize: 10
                            color: Material.color(Material.Grey)
                        }
                    }
                }
            }
        }
    }
}

