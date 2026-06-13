#include "gui/download_manager.hpp"
#include <QUrl>
#include <QFileInfo>
#include <iostream>
#include <shared/command_types.hpp>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent) {
}

DownloadManager::~DownloadManager() {
    stopServing();
}

void DownloadManager::setSelectedFilePath(const QString& v) {
    QString local = QUrl(v).toLocalFile();
    if (local.isEmpty()) local = v;
    { std::lock_guard<std::mutex> l(mutex_); selectedFilePath_ = local; }
    emit selectedFilePathChanged();
}

void DownloadManager::startServing() {
    if (selectedFilePath_.isEmpty()) {
        setStatus("Select a file first");
        return;
    }
    if (serving_) {
        setStatus("Already serving");
        return;
    }

    QFileInfo fi(selectedFilePath_);
    if (!fi.exists()) {
        setStatus("File not found: " + fi.fileName());
        return;
    }

    setStatus("Loading firmware...");

    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        setStatus("Failed to get CommonAPI runtime");
        return;
    }

    updaterStub_ = std::make_shared<GuiUpdaterStub>();
    std::string ver = versionOverride_.toStdString();
    if (!updaterStub_->loadFile(selectedFilePath_.toStdString(), ver)) {
        setStatus("Failed to load firmware file");
        updaterStub_.reset();
        return;
    }

    const auto& meta = updaterStub_->getManager()->getMetadata();
    QString info = QString("Version 0x%1  |  %2 MB  |  %3 chunks")
        .arg(meta.versionId, 8, 16, QLatin1Char('0'))
        .arg(meta.fileSize / 1'048'576.0, 0, 'f', 1)
        .arg((meta.fileSize + ::CHUNK_SIZE - 1) / ::CHUNK_SIZE);
    if (!meta.md5Hash.empty())
        info += QString("  |  MD5: %1").arg(QString::fromStdString(meta.md5Hash.substr(0, 16)));

    setFileName(fi.fileName());
    setFileInfo(info);

    bool registered = runtime_->registerService("local", "manager.updater.Updater", updaterStub_);
    if (!registered) {
        setFileInfo(info + "  |  (Serving disabled \u2014 another instance has the service)");
        setStatus("Another instance already serves firmware; use as controller only");
    } else {
        setServing(true);
        setStatus("Serving firmware via SOME/IP");
        updaterStub_->fireNotifyUpdateAvailableEvent(meta.versionId);
    }

    connectToRelay();
}

void DownloadManager::stopServing() {
    if (runtime_ && updaterStub_) {
        runtime_->unregisterService("local", "manager.updater.Updater", "manager.updater.Updater");
    }
    updaterStub_.reset();
    setServing(false);
    setFileInfo("");
    setFileName("");
    setStatus("Stopped");
}

void DownloadManager::connectToRelay() {
    if (!runtime_) {
        runtime_ = CommonAPI::Runtime::get();
        if (!runtime_) {
            std::cerr << "[GUI] Failed to get CommonAPI runtime" << std::endl;
            setRelayOutput("Failed to get CommonAPI runtime");
            return;
        }
    }

    std::cout << "[GUI] Building relay proxy..." << std::endl;
    relayProxy_ = runtime_->buildProxy<v1::manager::updater::RelayControlProxy>(
        "local", "manager.updater.RelayControl");

    if (!relayProxy_) {
        std::cerr << "[GUI] buildProxy returned null!" << std::endl;
        setRelayOutput("Failed to build relay proxy");
        return;
    }
    std::cout << "[GUI] relayProxy built successfully" << std::endl;

    auto subscribeToStateEvents = [this]() {
        std::cout << "[GUI] Subscribing to state changed events..." << std::endl;
        relayProxy_->getStateChangedEvent().subscribe(
            [this](const std::string& state, const uint32_t& progress,
                   const uint32_t& versionId, const std::string& message) {
                std::cout << "[GUI] State event: state=" << state
                          << ", progress=" << progress
                          << ", v=0x" << std::hex << versionId << std::dec
                          << ", msg=" << message << std::endl;
                QString info = QString("[%1] %2% v=0x%3 \u2014 %4")
                    .arg(QString::fromStdString(state))
                    .arg(progress)
                    .arg(versionId, 0, 16)
                    .arg(QString::fromStdString(message));
                setRelayState(info);
                setRelayProgress(static_cast<double>(progress) / 100.0);
                setStatus(info);
            }
        );
        std::cout << "[GUI] Subscribed to state events" << std::endl;
    };

    relayProxy_->getProxyStatusEvent().subscribe(
        [this, subscribeToStateEvents](const CommonAPI::AvailabilityStatus& av) {
            if (av == CommonAPI::AvailabilityStatus::AVAILABLE) {
                setRelayConnected(true);
                setRelayState("Connected");
                subscribeToStateEvents();
                setRelayOutput("Relay connected");
            } else {
                setRelayConnected(false);
                setRelayState("Unavailable");
            }
        });

    if (relayProxy_->isAvailable()) {
        setRelayConnected(true);
        setRelayState("Connected");
        subscribeToStateEvents();
        setRelayOutput("Relay connected");
    } else {
        setRelayState("Waiting...");
    }
}

void DownloadManager::disconnectFromRelay() {
    relayProxy_.reset();
    setRelayConnected(false);
    setRelayState("Not connected");
    setRelayOutput("Disconnected");
    setRelayProgress(0.0);
}

void DownloadManager::sendRelayCommand(int commandCode, int parameter) {
    std::cout << "[GUI] sendRelayCommand called: code=" << commandCode << ", param=" << parameter << std::endl;
    if (!relayProxy_) {
        std::cout << "[GUI] relayProxy_ is null" << std::endl;
        setRelayOutput("Relay proxy not built");
        return;
    }
    if (!relayProxy_->isAvailable()) {
        std::cout << "[GUI] relayProxy_ is not available (isAvailable=false)" << std::endl;
        setRelayOutput("Relay not connected");
        return;
    }

    std::cout << "[GUI] Sending sendCommandAsync: code=" << commandCode << ", param=" << parameter << std::endl;
    relayProxy_->sendCommandAsync(
        static_cast<uint32_t>(commandCode),
        0,
        static_cast<uint32_t>(parameter),
        [this, commandCode](const CommonAPI::CallStatus& status, const bool& accepted, const std::string& message) {
            std::cout << "[GUI] sendCommandAsync callback: status=" << static_cast<int>(status)
                      << " (" << (status == CommonAPI::CallStatus::SUCCESS ? "SUCCESS" :
                                  status == CommonAPI::CallStatus::REMOTE_ERROR ? "REMOTE_ERROR" :
                                  status == CommonAPI::CallStatus::NOT_AVAILABLE ? "NOT_AVAILABLE" :
                                  status == CommonAPI::CallStatus::UNKNOWN ? "UNKNOWN" : "OTHER")
                      << "), accepted=" << (accepted ? "true" : "false")
                      << ", message=\"" << message << "\"" << std::endl;
            if (status == CommonAPI::CallStatus::SUCCESS) {
                QString result = QString(accepted ? "Accepted: %1" : "Rejected: %1")
                    .arg(QString::fromStdString(message));
                setRelayOutput(result);
                setStatus(result);
            } else {
                QString err = QString("Command %1 failed: status=%2, msg=\"%3\"")
                    .arg(commandCode)
                    .arg(static_cast<int>(status))
                    .arg(QString::fromStdString(message));
                setRelayOutput(err);
                setStatus("Relay command failed");
            }
        }
    );
}

void DownloadManager::getRelayVersion() {
    std::cout << "[GUI] getRelayVersion called" << std::endl;
    if (!relayProxy_) {
        std::cout << "[GUI] relayProxy_ is null" << std::endl;
        setRelayOutput("Relay proxy not built");
        return;
    }
    if (!relayProxy_->isAvailable()) {
        std::cout << "[GUI] relayProxy_ is not available" << std::endl;
        setRelayOutput("Relay not connected");
        return;
    }

    std::cout << "[GUI] Sending getCurrentVersionAsync" << std::endl;
    relayProxy_->getCurrentVersionAsync(
        [this](const CommonAPI::CallStatus& status, const uint32_t& versionId,
               const std::string& versionString) {
            std::cout << "[GUI] getCurrentVersionAsync callback: status=" << static_cast<int>(status)
                      << ", versionId=0x" << std::hex << versionId << std::dec
                      << ", versionString=\"" << versionString << "\"" << std::endl;
            if (status == CommonAPI::CallStatus::SUCCESS) {
                QString result = QString("Current version: %1 (0x%2)")
                    .arg(QString::fromStdString(versionString))
                    .arg(versionId, 0, 16);
                setRelayOutput(result);
                setStatus(result);
            } else {
                QString err = QString("getCurrentVersion failed: status=%1")
                    .arg(static_cast<int>(status));
                setRelayOutput(err);
            }
        }
    );
}

void DownloadManager::sendToDaemon() {
    std::cout << "[GUI] sendToDaemon called" << std::endl;
    if (!relayProxy_) {
        std::cout << "[GUI] relayProxy_ is null" << std::endl;
        setRelayOutput("Relay proxy not built");
        return;
    }
    if (!relayProxy_->isAvailable()) {
        std::cout << "[GUI] relayProxy_ is not available" << std::endl;
        setRelayOutput("Relay not connected");
        return;
    }

    std::cout << "[GUI] Sending SEND_TO_DAEMON command" << std::endl;
    relayProxy_->sendCommandAsync(
        static_cast<uint32_t>(relay::SEND_TO_DAEMON),
        0,
        0,
        [this](const CommonAPI::CallStatus& status, const bool& accepted, const std::string& message) {
            std::cout << "[GUI] sendToDaemon callback: status=" << static_cast<int>(status)
                      << ", accepted=" << (accepted ? "true" : "false")
                      << ", message=\"" << message << "\"" << std::endl;
            if (status == CommonAPI::CallStatus::SUCCESS) {
                QString result = QString(accepted ? "File sent to daemon: %1" : "Send failed: %1")
                    .arg(QString::fromStdString(message));
                setRelayOutput(result);
                setStatus(result);
            } else {
                std::cout << "[GUI] sendToDaemon FAILED" << std::endl;
                setRelayOutput("Send to daemon command failed");
                setStatus("Send to daemon failed");
            }
        }
    );
}

void DownloadManager::triggerDaemonInstall() {
    std::cout << "[GUI] triggerDaemonInstall called" << std::endl;
    if (!relayProxy_) {
        std::cout << "[GUI] relayProxy_ is null" << std::endl;
        setRelayOutput("Relay proxy not built");
        return;
    }
    if (!relayProxy_->isAvailable()) {
        std::cout << "[GUI] relayProxy_ is not available" << std::endl;
        setRelayOutput("Relay not connected");
        return;
    }

    std::cout << "[GUI] Sending DAEMON_INSTALL command" << std::endl;
    relayProxy_->sendCommandAsync(
        static_cast<uint32_t>(relay::DAEMON_INSTALL),
        0,
        0,
        [this](const CommonAPI::CallStatus& status, const bool& accepted, const std::string& message) {
            std::cout << "[GUI] triggerDaemonInstall callback: status=" << static_cast<int>(status)
                      << ", accepted=" << (accepted ? "true" : "false")
                      << ", message=\"" << message << "\"" << std::endl;
            if (status == CommonAPI::CallStatus::SUCCESS) {
                QString result = QString(accepted ? "Install triggered: %1" : "Install rejected: %1")
                    .arg(QString::fromStdString(message));
                setRelayOutput(result);
                setStatus(result);
            } else {
                std::cout << "[GUI] triggerDaemonInstall FAILED" << std::endl;
                setRelayOutput("Install command failed");
                setStatus("Install command failed");
            }
        }
    );
}
