#include "download_manager.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QFileInfo>
#include <cstring>
#include <iostream>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent) {
}

DownloadManager::~DownloadManager() {
    cancelDownload();
    killService();
}

void DownloadManager::setSelectedFilePath(const QString& v) {
    QString local = QUrl(v).toLocalFile();
    if (local.isEmpty()) local = v;
    { std::lock_guard<std::mutex> l(mutex_); selectedFilePath_ = local; }
    emit selectedFilePathChanged();
    startServiceAndConnect();
}

void DownloadManager::killService() {
    // Kill any existing ota_service binary to avoid routing conflicts
    QProcess killProc;
    killProc.start("pkill", {"-9", "-x", "ota_service"});
    killProc.waitForFinished(2000);

    if (serviceProcess_) {
        serviceProcess_->terminate();
        if (!serviceProcess_->waitForFinished(3000))
            serviceProcess_->kill();
        serviceProcess_->deleteLater();
        serviceProcess_ = nullptr;
    }
}

void DownloadManager::launchService() {
    if (selectedFilePath_.isEmpty()) {
        setStatus("No file selected");
        return;
    }

    setStatus("Starting service...");

    QString serviceBin = QCoreApplication::applicationDirPath() + "/ota_service";
    serviceProcess_ = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("VSOMEIP_CONFIGURATION", QDir::currentPath() + "/service.json");
    env.insert("VSOMEIP_APPLICATION_NAME", "ota_service");
    serviceProcess_->setProcessEnvironment(env);
    serviceProcess_->setProcessChannelMode(QProcess::ForwardedChannels);
    serviceProcess_->setReadChannel(QProcess::StandardOutput);

    QStringList svcArgs = {selectedFilePath_};
    if (!versionOverride_.isEmpty())
        svcArgs << versionOverride_;
    serviceProcess_->start(serviceBin, svcArgs);

    if (!serviceProcess_->waitForStarted(5000)) {
        setStatus("Failed to start service");
        return;
    }

    QFileInfo fi(selectedFilePath_);
    setFileName(fi.fileName());

    setStatus("Service started — connecting...");
    QTimer::singleShot(2000, this, [this]() {
        connectProxy();
    });
}

void DownloadManager::connectProxy() {
    // Use a distinct app name so the GUI proxy doesn't conflict
    // with the ota_service child process (routing manager).
    qputenv("VSOMEIP_APPLICATION_NAME", "ota_gui_proxy");
    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        setStatus("Failed to get CommonAPI runtime");
        return;
    }

    proxy_ = runtime_->buildProxy<v1::manager::updater::UpdaterProxy>(
        "local", "manager.updater.Updater");

    if (!proxy_) {
        setStatus("Failed to build proxy");
        return;
    }

    if (proxy_->isAvailable()) {
        setConnected(true);
        setStatus("Connected — fetching update info...");
        checkForUpdate([this](bool ok) {
            if (ok) startDownload();
        });
    } else {
        proxy_->getProxyStatusEvent().subscribe(
            [this](const CommonAPI::AvailabilityStatus& av) {
                if (av == CommonAPI::AvailabilityStatus::AVAILABLE) {
                    setConnected(true);
                    setStatus("Connected — fetching update info...");
                    checkForUpdate([this](bool ok) {
                        if (ok) startDownload();
                    });
                } else {
                    setConnected(false);
                    setStatus("Service unavailable");
                }
            });
        setStatus("Waiting for service...");
    }
}

void DownloadManager::startServiceAndConnect() {
    if (downloading_) return;
    if (connected_ && proxy_) {
        // Already connected — just start download if not already
        startDownload();
        return;
    }
    killService();
    launchService();
}

void DownloadManager::checkForUpdate(std::function<void(bool)> onResult) {
    if (!proxy_) {
        if (onResult) onResult(false);
        return;
    }

    auto gen = ++downloadGeneration_;
    proxy_->getUpdateInfoAsync(
        [this, gen, onResult](const CommonAPI::CallStatus& status,
                    const uint32_t& vid,
                    const int64_t& size,
                    const std::string& md5Hash,
                    const bool& isCompressed)
    {
        if (gen != downloadGeneration_) {
            if (onResult) onResult(false);
            return;
        }

        if (status != CommonAPI::CallStatus::SUCCESS) {
            setStatus("No update available");
            if (onResult) onResult(false);
            return;
        }

        fileSize_ = size;
        totalChunks_ = (static_cast<uint32_t>(size) + CHUNK_SIZE - 1) / CHUNK_SIZE;
        lastVersionProcessed_ = vid;

        QString info = QString("Version 0x%1  |  %2 MB  |  %3 chunks")
            .arg(vid, 8, 16, QLatin1Char('0'))
            .arg(size / 1'048'576.0, 0, 'f', 1)
            .arg(totalChunks_);

        if (!md5Hash.empty()) {
            info += QString("  |  MD5: %1").arg(QString::fromStdString(md5Hash.substr(0, 16)));
        }

        setFileInfo(info);
        setStatus("Ready to download");
        if (onResult) onResult(true);
    });
}

void DownloadManager::startDownload() {
    if (!proxy_ || downloading()) return;

    if (fileSize_ <= 0) {
        setStatus("Fetching update info...");
        checkForUpdate([this](bool ok) { if (ok) startDownload(); });
        return;
    }

    auto gen = ++downloadGeneration_;
    nextToSend_ = 0;
    outstanding_ = 0;
    chunksReceived_ = 0;
    bytesReceived_ = 0;

    setDownloading(true);
    setProgress(0.0);
    setSpeedText("");

    downloadedData_.resize(static_cast<size_t>(fileSize_));

    transferStartTime_ = std::chrono::steady_clock::now();

    auto cb = std::make_shared<v1::manager::updater::UpdaterProxyBase::RequestDataAsyncCallback>();
    *cb = [this, gen, cb](const CommonAPI::CallStatus& status,
                          const uint32_t& recvIdx,
                          const std::string& data,
                          const bool& lastChunk)
    {
        if (gen != downloadGeneration_) return;

        if (status != CommonAPI::CallStatus::SUCCESS) {
            setStatus("Download failed");
            setDownloading(false);
            outstanding_--;
            return;
        }

        size_t offset = static_cast<size_t>(recvIdx) * CHUNK_SIZE;
        if (offset + data.size() <= downloadedData_.size()) {
            std::memcpy(&downloadedData_[offset], data.data(), data.size());
        }

        chunksReceived_++;
        bytesReceived_ += static_cast<int64_t>(data.size());

        double p = std::min(
            static_cast<double>(chunksReceived_) / totalChunks_, 1.0);
        setProgress(p);

        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - transferStartTime_).count();
        if (elapsed < 0.001) elapsed = 0.001;
        double mbps = (bytesReceived_ / 1'048'576.0) / elapsed;
        setSpeedText(QString("%1 MB/s").arg(mbps, 0, 'f', 1));
        setStatus(QString("Downloading... %1%").arg(static_cast<int>(p * 100)));

        outstanding_--;

        uint32_t next = nextToSend_++;
        if (next < totalChunks_) {
            outstanding_++;
            proxy_->requestDataAsync(lastVersionProcessed_, next, *cb);
        }

        if (gen != downloadGeneration_) return;

        if (next >= totalChunks_ && outstanding_ == 0) {
            setProgress(1.0);
            setDownloading(false);
            auto t = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - transferStartTime_).count();
            double finalMbps = (fileSize_ / 1'048'576.0) / t;
            setSpeedText(QString("%1 MB/s").arg(finalMbps, 0, 'f', 1));
            setStatus("Download complete!");
        }
    };

    for (uint32_t i = 0; i < WINDOW_SIZE && nextToSend_ < totalChunks_; i++) {
        uint32_t idx = nextToSend_++;
        outstanding_++;
        proxy_->requestDataAsync(lastVersionProcessed_, idx, *cb);
    }

    setStatus("Downloading...");
}

void DownloadManager::cancelDownload() {
    downloadGeneration_++;
    setDownloading(false);
    setStatus("Cancelled");
    setSpeedText("");
}
