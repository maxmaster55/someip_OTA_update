#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdint>
#include <chrono>
#include <CommonAPI/CommonAPI.hpp>
#include <v1/manager/updater/UpdaterProxy.hpp>
#include <v1/manager/updater/RelayControlProxy.hpp>
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
    Q_PROPERTY(QString relayState READ relayState NOTIFY relayStateChanged)
    Q_PROPERTY(QString relayOutput READ relayOutput NOTIFY relayOutputChanged)
    Q_PROPERTY(bool relayConnected READ relayConnected NOTIFY relayConnectedChanged)
    Q_PROPERTY(bool serviceRunning READ serviceRunning NOTIFY serviceRunningChanged)

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
    QString relayState() const { std::lock_guard<std::mutex> l(mutex_); return relayState_; }
    QString relayOutput() const { std::lock_guard<std::mutex> l(mutex_); return relayOutput_; }
    bool relayConnected() const { std::lock_guard<std::mutex> l(mutex_); return relayConnected_; }
    bool serviceRunning() const { std::lock_guard<std::mutex> l(mutex_); return serviceRunning_; }

    Q_INVOKABLE void startService();
    Q_INVOKABLE void stopService();
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void connectToRelay();
    Q_INVOKABLE void sendRelayCommand(int commandCode, int parameter = 0);
    Q_INVOKABLE void getRelayVersion();
    Q_INVOKABLE void installUpdate();

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
    void relayStateChanged();
    void relayOutputChanged();
    void relayConnectedChanged();
    void serviceRunningChanged();

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
    void setRelayState(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); relayState_ = v; } emit relayStateChanged(); }
    void setRelayOutput(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); relayOutput_ = v; } emit relayOutputChanged(); }
    void setRelayConnected(bool v) { { std::lock_guard<std::mutex> l(mutex_); relayConnected_ = v; } emit relayConnectedChanged(); }
    void setServiceRunning(bool v) { { std::lock_guard<std::mutex> l(mutex_); serviceRunning_ = v; } emit serviceRunningChanged(); }

    void launchService();
    void killService();
    void connectProxy();
    void retryConnectProxy();
    void checkForUpdate(std::function<void(bool)> onResult = nullptr);

    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<v1::manager::updater::UpdaterProxy<>> proxy_;
    std::shared_ptr<v1::manager::updater::RelayControlProxy<>> relayProxy_;
    QProcess* serviceProcess_ = nullptr;
    QTimer* connectRetryTimer_ = nullptr;
    int connectRetryCount_ = 0;
    static constexpr int MAX_CONNECT_RETRIES = 10;

    mutable std::mutex mutex_;
    double progress_ = 0.0;
    QString status_ = "Idle";
    QString speedText_ = "";
    bool connected_ = false;
    QString fileInfo_ = "";
    bool downloading_ = false;
    QString fileName_ = "";
    QString selectedFilePath_;
    QString versionOverride_;
    QString relayState_ = "Not connected";
    QString relayOutput_ = "";
    bool relayConnected_ = false;
    bool serviceRunning_ = false;

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
