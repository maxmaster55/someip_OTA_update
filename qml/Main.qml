import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import OtaGui 1.0

ApplicationWindow {
    id: window
    width: 560
    height: 620
    minimumWidth: 480
    minimumHeight: 540
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

        ColumnLayout {
            spacing: 8
            Label { text: "OTA Update Manager"; font.pixelSize: 16; font.weight: Font.Bold }
            Label { text: "SOME/IP based firmware update tool"; color: Material.color(Material.Grey) }
            Label { text: "Built with Qt"; font.pixelSize: 11; color: Material.color(Material.Grey) }
        }
    }

    // ── Content ───────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ── File Selection ────────────────────────────────────
        GroupBox {
            Layout.fillWidth: true
            title: "Update File"
            Material.elevation: 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        Layout.fillWidth: true
                        readOnly: true
                        placeholderText: "No file selected"
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

                Label {
                    text: manager.selectedFilePath.length > 0 ? manager.fileName : ""
                    color: Material.color(Material.Grey)
                    font.pixelSize: 11
                    visible: text.length > 0
                }

                // ── Version Override ──────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Version:"
                        color: Material.color(Material.Grey)
                        font.pixelSize: 13
                        Layout.alignment: Qt.AlignVCenter
                    }

                    TextField {
                        id: versionField
                        Layout.preferredWidth: 120
                        placeholderText: "Auto"
                        font.pixelSize: 13
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
                        text: "(optional — e.g. 2.5)"
                        color: Material.color(Material.Grey)
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ── Update Info ───────────────────────────────────────
        GroupBox {
            Layout.fillWidth: true
            title: "Update Info"
            Material.elevation: 0
            visible: manager.fileInfo.length > 0 || manager.connected

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 16

                    Label { text: "Status:"; color: Material.color(Material.Grey); font.pixelSize: 12 }
                    Label {
                        text: manager.connected ? "Connected" : "Disconnected"
                        color: manager.connected ? "#4ade80" : "#ef4444"
                        font.pixelSize: 12; font.weight: Font.Bold
                    }

                    Label { text: "File:"; color: Material.color(Material.Grey); font.pixelSize: 12; visible: manager.fileInfo.length > 0 }
                    Label {
                        text: manager.fileName; font.pixelSize: 12
                        visible: manager.fileName.length > 0
                        elide: Text.ElideRight
                    }

                    Label { text: "Details:"; color: Material.color(Material.Grey); font.pixelSize: 12; visible: manager.fileInfo.length > 0 }
                    Label {
                        Layout.fillWidth: true
                        text: manager.fileInfo; font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        visible: manager.fileInfo.length > 0
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // ── Main Action ───────────────────────────────────────
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            spacing: 12

            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 280
                Layout.preferredHeight: 48

                enabled: {
                    if (manager.downloading) return true
                    if (manager.connected) return true
                    if (manager.selectedFilePath.length > 0) return true
                    return false
                }

                highlighted: !manager.downloading
                Material.background: manager.downloading ? Material.Red : Material.accent
                Material.foreground: "white"

                contentItem: RowLayout {
                    anchors.centerIn: parent
                    spacing: 10

                    BusyIndicator {
                        implicitWidth: 20
                        implicitHeight: 20
                        running: manager.downloading
                        visible: manager.downloading
                        Material.accent: "white"
                    }

                    Label {
                        text: manager.downloading ? "  CANCEL DOWNLOAD" :
                              manager.connected ? "  START DOWNLOAD" :
                              manager.selectedFilePath.length > 0 ? "  START SERVICE" :
                              "  SELECT A FILE"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: "white"
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                onClicked: {
                    if (manager.downloading)
                        manager.cancelDownload()
                    else if (manager.connected)
                        manager.startDownload()
                    else
                        manager.startServiceAndConnect()
                }
            }

            // Hint text
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: {
                    if (manager.downloading) return "Click to cancel the download"
                    if (manager.connected && manager.fileInfo.length > 0) return "Ready to download the update"
                    if (manager.connected) return "Fetching update information..."
                    if (manager.selectedFilePath.length > 0) return "Click to start the service and send to RPi"
                    return "Browse to select an update file"
                }
                color: Material.color(Material.Grey)
                font.pixelSize: 11
            }
        }

        Item { Layout.fillHeight: true }

        // ── Progress ──────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            ProgressBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
                from: 0.0; to: 1.0
                value: manager.progress
                visible: manager.downloading || manager.progress > 0
                indeterminate: false
                Material.accent: Material.DeepPurple
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                visible: manager.downloading || manager.progress > 0

                Label {
                    text: Math.floor(manager.progress * 100) + "%"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: Material.accent
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: manager.speedText
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: "#4ade80"
                }

                Label {
                    text: {
                        if (manager.progress < 0.05) return ""
                        if (elapsedTime < 1) return ""
                        var remaining = (1.0 - manager.progress) / manager.progress
                        var seconds = Math.round(remaining * elapsedTime)
                        if (seconds < 1) return ""
                        if (seconds < 60) return seconds + "s remaining"
                        return Math.floor(seconds / 60) + "m " + (seconds % 60) + "s"
                    }
                    font.pixelSize: 12
                    color: Material.color(Material.Grey)
                    visible: manager.downloading
                }
            }
        }

    }

    // Track elapsed upload time for ETA
    property double elapsedTime: 0.0
    Timer {
        id: etaTimer
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

            RowLayout {
                spacing: 6
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: manager.connected ? "#4ade80" : "#ef4444"
                    Layout.alignment: Qt.AlignVCenter
                }
                Label {
                    text: manager.connected ? "Connected" : "Disconnected"
                    font.pixelSize: 11
                    color: Material.color(Material.Grey)
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                text: manager.status
                font.pixelSize: 11
                color: manager.downloading ? "#d97706" :
                       manager.connected ? "#16a34a" : Material.color(Material.Grey)
                elide: Text.ElideRight
            }
        }
    }
}

