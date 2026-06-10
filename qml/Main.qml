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
            // Section: File & Service
            // ════════════════════════════════════════════════════
            GroupBox {
                Layout.fillWidth: true
                title: "1. Select Firmware File"
                Material.elevation: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

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
                            text: "Version:"
                            color: Material.color(Material.Grey)
                            Layout.alignment: Qt.AlignVCenter
                        }

                        TextField {
                            id: versionField
                            Layout.preferredWidth: 100
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

                        Label {
                            text: "(optional)"
                            color: Material.color(Material.Grey)
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: manager.serviceRunning ? "Stop Service" : "Launch Service"
                            enabled: manager.selectedFilePath.length > 0
                            implicitHeight: 32
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
                }
            }

            // ════════════════════════════════════════════════════
            // Section: Service Status & Direct Download
            // ════════════════════════════════════════════════════
            GroupBox {
                Layout.fillWidth: true
                title: "2. Service Status"
                Material.elevation: 1
                visible: manager.serviceRunning || manager.connected || manager.downloading

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 4
                        columnSpacing: 12

                        Label { text: "Process:"; color: Material.color(Material.Grey); font.pixelSize: 12 }
                        RowLayout {
                            spacing: 6
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: manager.serviceRunning ? "#4ade80" : "#9ca3af"
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                text: manager.serviceRunning ? "Running" : "Stopped"
                                font.pixelSize: 12; font.weight: Font.Bold
                                color: manager.serviceRunning ? "#4ade80" : "#9ca3af"
                            }
                        }

                        Label { text: "SOME/IP:"; color: Material.color(Material.Grey); font.pixelSize: 12 }
                        RowLayout {
                            spacing: 6
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: manager.connected ? "#4ade80" : (manager.serviceRunning ? "#f59e0b" : "#9ca3af")
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Label {
                                text: manager.connected ? "Connected" :
                                      manager.serviceRunning ? "Connecting..." : "N/A"
                                font.pixelSize: 12; font.weight: Font.Bold
                                color: manager.connected ? "#4ade80" : (manager.serviceRunning ? "#f59e0b" : "#9ca3af")
                            }
                        }

                        Label {
                            text: "File:"
                            color: Material.color(Material.Grey); font.pixelSize: 12
                            visible: manager.fileName.length > 0
                        }
                        Label {
                            text: manager.fileName
                            font.pixelSize: 12
                            visible: manager.fileName.length > 0
                            elide: Text.ElideRight
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

                    // Direct download button (legacy)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: manager.connected

                        Button {
                            text: manager.downloading ? "Cancel Download" : "Download Direct"
                            implicitHeight: 36
                            Layout.fillWidth: true
                            Material.background: manager.downloading ? Material.Red : Material.accent
                            Material.foreground: "white"
                            onClicked: {
                                if (manager.downloading)
                                    manager.cancelDownload()
                                else
                                    manager.startDownload()
                            }
                        }
                    }

                    // Direct download progress
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: manager.downloading || manager.progress > 0

                        ProgressBar {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            from: 0.0; to: 1.0
                            value: manager.progress
                            indeterminate: manager.downloading && manager.progress === 0
                            Material.accent: Material.DeepPurple
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: Math.floor(manager.progress * 100) + "%"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: Material.accent
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: manager.speedText
                                font.pixelSize: 12
                                color: "#4ade80"
                            }

                            Label {
                                text: {
                                    if (manager.progress < 0.05 || elapsedTime < 1) return ""
                                    var remaining = (1.0 - manager.progress) / manager.progress
                                    var seconds = Math.round(remaining * elapsedTime)
                                    if (seconds < 1) return ""
                                    if (seconds < 60) return seconds + "s remaining"
                                    return Math.floor(seconds / 60) + "m " + (seconds % 60) + "s"
                                }
                                font.pixelSize: 12
                                color: Material.color(Material.Grey)
                            }
                        }
                    }
                }
            }

            // ════════════════════════════════════════════════════
            // Section: Daemon Control — sends commands through relay to daemon
            // ════════════════════════════════════════════════════
            GroupBox {
                Layout.fillWidth: true
                title: "3. Daemon Control"
                Material.elevation: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    // Connection status to relay (gateway to daemon)
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
                        Label {
                            text: "(relay forwards commands to daemon)"
                            font.pixelSize: 10
                            color: Material.color(Material.Grey)
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

                    // Daemon state display
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: relayStateLabel.height + 16
                        radius: 4
                        color: Material.color(Material.Grey, Material.Shade100)
                        visible: manager.relayState !== "Not connected"

                        Label {
                            id: relayStateLabel
                            anchors.fill: parent
                            anchors.margins: 8
                            text: manager.relayState
                            font.pixelSize: 11
                            color: Material.color(Material.Grey, Material.Shade700)
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // ── Firmware source (prerequisite for daemon deployment) ──
                    Label {
                        text: "Step 1: Get firmware from service"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: Material.color(Material.Grey, Material.Shade700)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "Fetch Firmware from Service"
                            enabled: manager.relayConnected
                            implicitHeight: 36
                            Layout.fillWidth: true
                            Material.background: Material.Green
                            Material.foreground: "white"
                            onClicked: manager.sendRelayCommand(0)
                        }

                        Button {
                            text: "Fetch on Schedule (30s)"
                            enabled: manager.relayConnected
                            implicitHeight: 36
                            Layout.fillWidth: true
                            onClicked: manager.sendRelayCommand(1, 30)
                        }
                    }

                    // ── Daemon commands ──
                    Label {
                        text: "Step 2: Control the daemon"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: Material.color(Material.Grey, Material.Shade700)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "Deploy to Daemon"
                            enabled: manager.relayConnected && manager.relayState.indexOf("ready") >= 0
                            implicitHeight: 36
                            Layout.fillWidth: true
                            Material.background: Material.DeepOrange
                            Material.foreground: "white"
                            onClicked: manager.installUpdate()
                        }

                        Button {
                            text: "Cancel Deploy"
                            enabled: manager.relayConnected
                            implicitHeight: 36
                            Layout.fillWidth: true
                            Material.background: Material.Red
                            Material.foreground: "white"
                            onClicked: manager.sendRelayCommand(3)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: "Daemon Status"
                            enabled: manager.relayConnected
                            implicitHeight: 36
                            Layout.fillWidth: true
                            onClicked: manager.sendRelayCommand(4)
                        }

                        Button {
                            text: "Installed Version"
                            enabled: manager.relayConnected
                            implicitHeight: 36
                            Layout.fillWidth: true
                            onClicked: manager.getRelayVersion()
                        }
                    }

                    // Command output
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: relayOutputLabel.height + 16
                        radius: 4
                        color: Material.color(Material.DeepPurple, Material.Shade50)
                        visible: manager.relayOutput.length > 0

                        Label {
                            id: relayOutputLabel
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

    // ETA timer
    property double elapsedTime: 0.0
    Timer {
        interval: 1000
        repeat: true
        running: manager.downloading
        onTriggered: elapsedTime += 1.0
        onRunningChanged: if (!running) elapsedTime = 0.0
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
                color: manager.downloading ? "#d97706" :
                       manager.connected ? "#16a34a" :
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
                    text: "Daemon"
                    font.pixelSize: 9
                    color: Material.color(Material.Grey)
                }
            }
        }
    }
}
