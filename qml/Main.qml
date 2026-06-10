import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import OtaGui 1.0

ApplicationWindow {
    id: window
    width: 600
    height: 760
    minimumWidth: 520
    minimumHeight: 700
    visible: true
    title: "OTA Update Manager"

    Material.theme: Material.Light
    Material.accent: Material.DeepPurple
    Material.primary: Material.Indigo

    font.pixelSize: 13

    DownloadManager { id: manager }

    FileDialog {
        id: fileDialog
        title: "Select OTA Update File"
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
        nameFilters: ["Update Files (*.wic.bz2 *.wic *.bin *.bz2 *.gz)", "All Files (*)"]
        onAccepted: manager.selectedFilePath = selectedFile
    }

    // ── Menu Bar ──────────────────────────────────────────────
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
            Action { text: "About OTA Update Manager"; onTriggered: aboutDialog.open() }
        }
    }

    Dialog {
        id: aboutDialog
        title: "About"
        standardButtons: Dialog.Close
        modal: true
        anchors.centerIn: parent
        width: 300

        ColumnLayout {
            spacing: 8
            Label { text: "OTA Update Manager"; font.pixelSize: 16; font.weight: Font.Bold }
            Label { text: "SOME/IP based firmware update tool"; color: Material.color(Material.Grey) }
            Label { text: "Service-Relay-Daemon 3-node architecture"; font.pixelSize: 11; color: Material.color(Material.Grey) }
        }
    }

    // ── Content ───────────────────────────────────────────────
    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: parent.width - 32
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 16

            // ════════════════════════════════════════════════════
            // Section: Service - file + status merged
            // ════════════════════════════════════════════════════
            GroupBox {
                Layout.fillWidth: true
                title: "1. Service"
                Material.elevation: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            Layout.fillWidth: true
                            readOnly: true
                            placeholderText: "No file selected — click Browse..."
                            text: {
                                if (manager.selectedFilePath.length === 0) return ""
                                var parts = manager.selectedFilePath.split("/")
                                return parts.slice(-3).join("/")
                            }
                            color: manager.selectedFilePath.length > 0
                                   ? Material.foreground : Material.color(Material.Grey)
                            leftPadding: 8
                            background: Rectangle {
                                radius: 4
                                color: Material.color(Material.Grey, Material.Shade100)
                                border.color: Material.color(Material.Grey, Material.Shade300)
                                border.width: 1
                            }
                        }

                        Button {
                            text: "Browse..."
                            highlighted: true
                            Material.background: Material.accent
                            onClicked: fileDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "Ver:"
                            color: Material.color(Material.Grey)
                            Layout.alignment: Qt.AlignVCenter
                        }

                        TextField {
                            Layout.preferredWidth: 80
                            placeholderText: "Auto"
                            leftPadding: 8
                            verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle {
                                radius: 4
                                color: Material.color(Material.Grey, Material.Shade100)
                                border.color: Material.color(Material.Grey, Material.Shade300)
                                border.width: 1
                            }
                            onTextChanged: manager.versionOverride = text.trim()
                        }

                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: manager.serviceRunning ? "#4ade80" : "#9ca3af"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: "Svc"
                            font.pixelSize: 11
                            color: manager.serviceRunning ? "#4ade80" : "#9ca3af"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: manager.connected ? "#4ade80" : (manager.serviceRunning ? "#f59e0b" : "#9ca3af")
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: manager.connected ? "IP" : (manager.serviceRunning ? "..." : "N/A")
                            font.pixelSize: 11
                            color: manager.connected ? "#4ade80" : (manager.serviceRunning ? "#f59e0b" : "#9ca3af")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: manager.serviceRunning ? "Stop" : "Launch"
                            enabled: manager.selectedFilePath.length > 0
                            implicitHeight: 30
                            Material.background: manager.serviceRunning ? Material.Red : Material.Green
                            Material.foreground: "white"
                            onClicked: {
                                if (manager.serviceRunning)
                                    manager.stopService()
                                else
                                    manager.startService()
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: manager.fileInfo
                        font.pixelSize: 11
                        color: Material.color(Material.Grey)
                        wrapMode: Text.WordWrap
                        visible: manager.fileInfo.length > 0
                    }
                }
            }

            // ════════════════════════════════════════════════════
            // Section: Update Pipeline - single unified flow
            // ════════════════════════════════════════════════════
            GroupBox {
                Layout.fillWidth: true
                title: "2. Update Pipeline"
                Material.elevation: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    // Connection to relay
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 10; height: 10; radius: 5
                            color: manager.relayConnected ? "#4ade80" : "#ef4444"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: manager.relayConnected ? "Relay Connected" : "Relay Disconnected"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            color: manager.relayConnected ? "#4ade80" : "#ef4444"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Connect"
                            implicitHeight: 28
                            enabled: !manager.relayConnected
                            onClicked: manager.connectToRelay()
                        }
                    }

                    // Current state
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: stateLabel.height + 12
                        radius: 4
                        color: Material.color(Material.Grey, Material.Shade100)
                        visible: manager.relayState !== "Not connected"

                        Label {
                            id: stateLabel
                            anchors.fill: parent
                            anchors.margins: 8
                            text: manager.relayState
                            font.pixelSize: 11
                            color: Material.color(Material.Grey, Material.Shade700)
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // Single progress bar
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: manager.relayProgress > 0 || manager.relayState.indexOf("downloading") >= 0
                                 || manager.relayState.indexOf("installing") >= 0

                        ProgressBar {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 8
                            from: 0.0; to: 1.0
                            value: manager.relayProgress
                            indeterminate: manager.relayProgress === 0
                                           && (manager.relayState.indexOf("downloading") >= 0
                                            || manager.relayState.indexOf("installing") >= 0)
                            Material.accent: Material.DeepPurple
                        }

                        Label {
                            text: Math.floor(manager.relayProgress * 100) + "%"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            color: Material.accent
                        }
                    }

                    // Action buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "\u2776 Fetch Firmware"
                            enabled: manager.relayConnected
                            implicitHeight: 36
                            Layout.fillWidth: true
                            Material.background: Material.Green
                            Material.foreground: "white"
                            onClicked: manager.sendRelayCommand(0)
                        }

                        Button {
                            text: "\u2777 Deploy to Device"
                            enabled: manager.relayConnected && manager.relayState.indexOf("ready") >= 0
                            implicitHeight: 36
                            Layout.fillWidth: true
                            Material.background: Material.DeepOrange
                            Material.foreground: "white"
                            onClicked: manager.installUpdate()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "Status"
                            enabled: manager.relayConnected
                            implicitHeight: 34
                            Layout.fillWidth: true
                            onClicked: manager.sendRelayCommand(4)
                        }

                        Button {
                            text: "Version"
                            enabled: manager.relayConnected
                            implicitHeight: 34
                            Layout.fillWidth: true
                            onClicked: manager.getRelayVersion()
                        }

                        Button {
                            text: "Cancel"
                            enabled: manager.relayConnected
                            implicitHeight: 34
                            Layout.fillWidth: true
                            Material.background: Material.Red
                            Material.foreground: "white"
                            onClicked: manager.sendRelayCommand(3)
                        }
                    }

                    // Command output
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: outputLabel.height + 12
                        radius: 4
                        color: Material.color(Material.DeepPurple, Material.Shade50)
                        visible: manager.relayOutput.length > 0

                        Label {
                            id: outputLabel
                            anchors.fill: parent
                            anchors.margins: 8
                            text: manager.relayOutput
                            font.pixelSize: 12
                            color: Material.accent
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // Spacer
            Item { Layout.fillHeight: true }
        }
    }

    // ── Status Bar ────────────────────────────────────────────
    footer: Pane {
        Material.elevation: 2
        padding: 8
        Material.background: Material.color(Material.Grey, Material.Shade100)

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 8

            Label {
                text: "Status:"
                font.pixelSize: 11
                color: Material.color(Material.Grey)
            }

            Label {
                text: manager.status
                font.pixelSize: 11
                font.weight: Font.Medium
                color: manager.connected ? "#16a34a" :
                       manager.relayConnected ? "#7c3aed" :
                       Material.color(Material.Grey)
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Mini indicators
            RowLayout {
                spacing: 4
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: manager.serviceRunning ? "#4ade80" : "#9ca3af"
                }
                Label {
                    text: "Svc"
                    font.pixelSize: 9
                    color: Material.color(Material.Grey)
                }
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: manager.relayConnected ? "#4ade80" : "#9ca3af"
                }
                Label {
                    text: "Pipe"
                    font.pixelSize: 9
                    color: Material.color(Material.Grey)
                }
            }
        }
    }
}
