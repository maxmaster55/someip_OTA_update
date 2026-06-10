#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdint>
#include <chrono>
#include <CommonAPI/CommonAPI.hpp>
#include <v1/manager/updater/UpdaterProxy.hpp>
#include "transfer_config.hpp"

class DownloadManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString speedText READ speedText NOTIFY speedTextChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString fileInfo READ fileInfo NOTIFY fileInfoChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString selectedFilePath READ selectedFilePath WRITE setSelectedFilePath NOTIFY selectedFilePathChanged)
    Q_PROPERTY(QString versionOverride READ versionOverride WRITE setVersionOverride NOTIFY versionOverrideChanged)

public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    double progress() const { std::lock_guard<std::mutex> l(mutex_); return progress_; }
    QString status() const { std::lock_guard<std::mutex> l(mutex_); return status_; }
    QString speedText() const { std::lock_guard<std::mutex> l(mutex_); return speedText_; }
    bool connected() const { std::lock_guard<std::mutex> l(mutex_); return connected_; }
    QString fileInfo() const { std::lock_guard<std::mutex> l(mutex_); return fileInfo_; }
    bool downloading() const { std::lock_guard<std::mutex> l(mutex_); return downloading_; }
    QString fileName() const { std::lock_guard<std::mutex> l(mutex_); return fileName_; }
    QString selectedFilePath() const { std::lock_guard<std::mutex> l(mutex_); return selectedFilePath_; }
    QString versionOverride() const { std::lock_guard<std::mutex> l(mutex_); return versionOverride_; }

    Q_INVOKABLE void startServiceAndConnect();
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void cancelDownload();

signals:
    void progressChanged();
    void statusChanged();
    void speedTextChanged();
    void connectedChanged();
    void fileInfoChanged();
    void downloadingChanged();
    void fileNameChanged();
    void selectedFilePathChanged();
    void versionOverrideChanged();

private:
    void setProgress(double v) { { std::lock_guard<std::mutex> l(mutex_); progress_ = v; } emit progressChanged(); }
    void setStatus(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); status_ = v; } emit statusChanged(); }
    void setSpeedText(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); speedText_ = v; } emit speedTextChanged(); }
    void setConnected(bool v) { { std::lock_guard<std::mutex> l(mutex_); connected_ = v; } emit connectedChanged(); }
    void setFileInfo(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); fileInfo_ = v; } emit fileInfoChanged(); }
    void setDownloading(bool v) { { std::lock_guard<std::mutex> l(mutex_); downloading_ = v; } emit downloadingChanged(); }
    void setFileName(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); fileName_ = v; } emit fileNameChanged(); }
    void setSelectedFilePath(const QString& v);
    void setVersionOverride(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); versionOverride_ = v; } emit versionOverrideChanged(); }

    void launchService();
    void killService();
    void connectProxy();
    void checkForUpdate(std::function<void(bool)> onResult = nullptr);

    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<v1::manager::updater::UpdaterProxy<>> proxy_;
    QProcess* serviceProcess_ = nullptr;

    mutable std::mutex mutex_;
    double progress_ = 0.0;
    QString status_ = "Disconnected";
    QString speedText_ = "";
    bool connected_ = false;
    QString fileInfo_ = "";
    bool downloading_ = false;
    QString fileName_ = "";
    QString selectedFilePath_;
    QString versionOverride_;

    uint32_t lastVersionProcessed_ = 0;
    int64_t fileSize_ = 0;
    uint32_t totalChunks_ = 0;
    uint32_t downloadGeneration_ = 0;

    std::atomic<uint32_t> nextToSend_{0};
    std::atomic<uint32_t> outstanding_{0};

    std::vector<uint8_t> downloadedData_;

    std::chrono::steady_clock::time_point transferStartTime_;
    uint32_t chunksReceived_ = 0;
    int64_t bytesReceived_ = 0;

    static constexpr uint32_t CHUNK_SIZE = ::CHUNK_SIZE;
    static constexpr uint32_t WINDOW_SIZE = ::WINDOW_SIZE;
};
