#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <mutex>
#include <cstdint>
#include <CommonAPI/CommonAPI.hpp>
#include <v1/manager/updater/UpdaterStubDefault.hpp>
#include <v1/manager/updater/RelayControlProxy.hpp>
#include <shared/transfer_config.hpp>
#include <shared/file_manager.hpp>

class GuiUpdaterStub : public v1::manager::updater::UpdaterStubDefault {
public:
    GuiUpdaterStub() = default;

    bool loadFile(const std::string& path, const std::string& versionOverride) {
        manager_ = std::make_shared<UpdateManager>();
        return manager_->loadUpdateFile(path, versionOverride);
    }

    std::shared_ptr<UpdateManager> getManager() const { return manager_; }

    void getUpdateInfo(const std::shared_ptr<CommonAPI::ClientId> _client,
                       getUpdateInfoReply_t _reply) override {
        (void)_client;
        const auto& meta = manager_->getMetadata();
        _reply(meta.versionId, meta.fileSize, meta.md5Hash, meta.isCompressed);
    }

    void getDownloadStatus(const std::shared_ptr<CommonAPI::ClientId> _client,
                           uint32_t _versionId, bool _success, bool _retry,
                           std::string _message,
                           getDownloadStatusReply_t _reply) override {
        (void)_client; (void)_versionId; (void)_success; (void)_retry; (void)_message;
        _reply();
    }

    void getInstallationStatus(const std::shared_ptr<CommonAPI::ClientId> _client,
                               uint32_t _versionId, bool _success,
                               std::string _message,
                               getInstallationStatusReply_t _reply) override {
        (void)_client; (void)_versionId; (void)_success; (void)_message;
        _reply();
    }

    void requestData(const std::shared_ptr<CommonAPI::ClientId> _client,
                     uint32_t _versionId, uint32_t _chunkIndex,
                     requestDataReply_t _reply) override {
        (void)_client; (void)_versionId;
        static constexpr uint32_t CHUNK_SIZE = ::CHUNK_SIZE;
        std::string data = manager_->getChunk(_chunkIndex, CHUNK_SIZE);
        bool last = !manager_->hasMoreChunks(_chunkIndex, CHUNK_SIZE);
        _reply(_chunkIndex, data, last);
    }

private:
    std::shared_ptr<UpdateManager> manager_;
};

class DownloadManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString fileInfo READ fileInfo NOTIFY fileInfoChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString selectedFilePath READ selectedFilePath WRITE setSelectedFilePath NOTIFY selectedFilePathChanged)
    Q_PROPERTY(QString versionOverride READ versionOverride WRITE setVersionOverride NOTIFY versionOverrideChanged)
    Q_PROPERTY(QString relayState READ relayState NOTIFY relayStateChanged)
    Q_PROPERTY(QString relayOutput READ relayOutput NOTIFY relayOutputChanged)
    Q_PROPERTY(double relayProgress READ relayProgress NOTIFY relayProgressChanged)
    Q_PROPERTY(bool relayConnected READ relayConnected NOTIFY relayConnectedChanged)
    Q_PROPERTY(bool serving READ serving NOTIFY servingChanged)

public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    QString status() const { std::lock_guard<std::mutex> l(mutex_); return status_; }
    QString fileInfo() const { std::lock_guard<std::mutex> l(mutex_); return fileInfo_; }
    QString fileName() const { std::lock_guard<std::mutex> l(mutex_); return fileName_; }
    QString selectedFilePath() const { std::lock_guard<std::mutex> l(mutex_); return selectedFilePath_; }
    QString versionOverride() const { std::lock_guard<std::mutex> l(mutex_); return versionOverride_; }
    QString relayState() const { std::lock_guard<std::mutex> l(mutex_); return relayState_; }
    QString relayOutput() const { std::lock_guard<std::mutex> l(mutex_); return relayOutput_; }
    double relayProgress() const { std::lock_guard<std::mutex> l(mutex_); return relayProgress_; }
    bool relayConnected() const { std::lock_guard<std::mutex> l(mutex_); return relayConnected_; }
    bool serving() const { std::lock_guard<std::mutex> l(mutex_); return serving_; }

    Q_INVOKABLE void startServing();
    Q_INVOKABLE void stopServing();
    Q_INVOKABLE void connectToRelay();
    Q_INVOKABLE void disconnectFromRelay();
    Q_INVOKABLE void sendRelayCommand(int commandCode, int parameter = 0);
    Q_INVOKABLE void getRelayVersion();
    Q_INVOKABLE void sendToDaemon();
    Q_INVOKABLE void triggerDaemonInstall();

signals:
    void statusChanged();
    void fileInfoChanged();
    void fileNameChanged();
    void selectedFilePathChanged();
    void versionOverrideChanged();
    void relayStateChanged();
    void relayOutputChanged();
    void relayProgressChanged();
    void relayConnectedChanged();
    void servingChanged();

private:
    void setStatus(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); status_ = v; } emit statusChanged(); }
    void setFileInfo(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); fileInfo_ = v; } emit fileInfoChanged(); }
    void setFileName(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); fileName_ = v; } emit fileNameChanged(); }
    void setSelectedFilePath(const QString& v);
    void setVersionOverride(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); versionOverride_ = v; } emit versionOverrideChanged(); }
    void setRelayState(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); relayState_ = v; } emit relayStateChanged(); }
    void setRelayOutput(const QString& v) { { std::lock_guard<std::mutex> l(mutex_); relayOutput_ = v; } emit relayOutputChanged(); }
    void setRelayProgress(double v) { { std::lock_guard<std::mutex> l(mutex_); relayProgress_ = v; } emit relayProgressChanged(); }
    void setRelayConnected(bool v) { { std::lock_guard<std::mutex> l(mutex_); relayConnected_ = v; } emit relayConnectedChanged(); }
    void setServing(bool v) { { std::lock_guard<std::mutex> l(mutex_); serving_ = v; } emit servingChanged(); }

    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<GuiUpdaterStub> updaterStub_;
    std::shared_ptr<v1::manager::updater::RelayControlProxy<>> relayProxy_;

    mutable std::mutex mutex_;
    QString status_ = "Idle";
    QString fileInfo_ = "";
    QString fileName_ = "";
    QString selectedFilePath_;
    QString versionOverride_;
    QString relayState_ = "Not connected";
    QString relayOutput_ = "";
    double relayProgress_ = 0.0;
    bool relayConnected_ = false;
    bool serving_ = false;
};
