#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <atomic>
#include <mutex>
#include <thread>

namespace nimarkogram::core {

enum class ClientState {
    Disconnected,
    Connecting,
    WaitingForAuthorization,
    Ready,
    Error
};

struct ClientConfig {
    int apiId = 0;
    QString apiHash;
    QString dataDirectory;

    static ClientConfig fromEnvironment();
    bool isValid(QString *error = nullptr) const;
};

class TelegramClient final : public QObject {
    Q_OBJECT

public:
    explicit TelegramClient(ClientConfig config, QObject *parent = nullptr);
    ~TelegramClient() override;

    ClientState state() const noexcept;
    QString lastError() const;
    bool hasTransport() const noexcept;

public slots:
    void start();
    void stop();
    void setPhoneNumber(const QString &phoneNumber);
    void setAuthenticationCode(const QString &code);
    void setPassword(const QString &password);
    void registerUser(const QString &firstName, const QString &lastName);
    void openChat(qint64 chatId);
    void loadMoreHistory(qint64 chatId, qint64 fromMessageId);
    void sendTextMessage(qint64 chatId, const QString &text);
    void sendDocument(qint64 chatId, const QString &filePath);
    void sendPhoto(qint64 chatId, const QString &filePath);
    void sendVideo(qint64 chatId, const QString &filePath);
    void sendVoiceNote(qint64 chatId, const QString &filePath);
    void downloadFile(int fileId);

signals:
    void stateChanged(nimarkogram::core::ClientState state);
    void errorOccurred(const QString &message);
    void authorizationPhoneRequired();
    void authorizationCodeRequired();
    void authorizationPasswordRequired();
    void registrationRequired();
    void authorized();
    void chatListReceived(const QJsonObject &chats);
    void chatReceived(const QJsonObject &chat);
    void chatUnreadUpdated(qint64 chatId, int unreadCount);
    void messageHistoryReceived(const QJsonObject &messages);
    void messageReceived(const QJsonObject &message);
    void messageSendSucceeded(qint64 oldMessageId, qint64 messageId);
    void messageSendFailed(qint64 oldMessageId, const QString &error);
    void fileUpdated(const QJsonObject &file);

private:
    void receiveLoop();
    void handleResponse(const QJsonObject &response);
    void sendRequest(const QJsonObject &request);
    void setState(ClientState state);
    void fail(const QString &message);

    ClientConfig m_config;
    std::atomic<ClientState> m_state = ClientState::Disconnected;
    std::atomic_bool m_stopRequested = false;
    std::atomic_bool m_running = false;
    mutable std::mutex m_errorMutex;
    QString m_lastError;
    void *m_tdClient = nullptr;
    std::thread m_receiveThread;
};

} // namespace nimarkogram::core

Q_DECLARE_METATYPE(nimarkogram::core::ClientState)
