#include "core/telegram_client.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

#include <utility>

#if NIMARKOGRAM_HAS_TDLIB
#include <td/telegram/td_json_client.h>
#endif

namespace nimarkogram::core {

namespace {

QString defaultDataDirectory() {
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath() + QStringLiteral("/.nimarkogram");
    }
    return QDir::cleanPath(directory + QStringLiteral("/tdlib"));
}

QJsonObject emptySendOptions() {
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
        {QStringLiteral("suggested_post_info"), QJsonValue::Null},
        {QStringLiteral("disable_notification"), false},
        {QStringLiteral("from_background"), false},
        {QStringLiteral("protect_content"), false},
        {QStringLiteral("allow_paid_broadcast"), false},
        {QStringLiteral("paid_message_star_count"), QStringLiteral("0")},
        {QStringLiteral("update_order_of_installed_sticker_sets"), false},
        {QStringLiteral("scheduling_state"), QJsonValue::Null},
        {QStringLiteral("effect_id"), QStringLiteral("0")},
        {QStringLiteral("sending_id"), 0},
        {QStringLiteral("only_preview"), false},
    };
}

} // namespace

ClientConfig ClientConfig::fromEnvironment() {
    ClientConfig config;
    bool ok = false;
    const int apiId = qEnvironmentVariable("NIMARKOGRAM_API_ID").toInt(&ok);
    config.apiId = ok ? apiId : 0;
    config.apiHash = qEnvironmentVariable("NIMARKOGRAM_API_HASH");
    config.dataDirectory = qEnvironmentVariable("NIMARKOGRAM_DATA_DIR");
    if (config.dataDirectory.isEmpty()) {
        config.dataDirectory = defaultDataDirectory();
    }
    return config;
}

bool ClientConfig::isValid(QString *error) const {
    if (apiId <= 0) {
        if (error) {
            *error = QStringLiteral("NIMARKOGRAM_API_ID не задан или имеет неверное значение");
        }
        return false;
    }
    if (apiHash.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("NIMARKOGRAM_API_HASH не задан");
        }
        return false;
    }
    return true;
}

TelegramClient::TelegramClient(ClientConfig config, QObject *parent)
    : QObject(parent), m_config(std::move(config)) {
    qRegisterMetaType<ClientState>("nimarkogram::core::ClientState");
}

TelegramClient::~TelegramClient() {
    stop();
}

ClientState TelegramClient::state() const noexcept {
    return m_state.load();
}

QString TelegramClient::lastError() const {
    std::lock_guard lock(m_errorMutex);
    return m_lastError;
}

bool TelegramClient::hasTransport() const noexcept {
#if NIMARKOGRAM_HAS_TDLIB
    return true;
#else
    return false;
#endif
}

void TelegramClient::start() {
    if (m_running.exchange(true)) {
        return;
    }

    QString configError;
    if (!m_config.isValid(&configError)) {
        m_running = false;
        fail(configError);
        return;
    }

#if !NIMARKOGRAM_HAS_TDLIB
    m_running = false;
    fail(QStringLiteral("Эта сборка создана без TDLib. Включите NIMARKOGRAM_FETCH_TDLIB или укажите установленный TDLib."));
    return;
#else
    QDir().mkpath(m_config.dataDirectory);
    QDir().mkpath(m_config.dataDirectory + QStringLiteral("/files"));
    m_stopRequested = false;
    setState(ClientState::Connecting);
    m_tdClient = td_json_client_create();
    if (!m_tdClient) {
        m_running = false;
        fail(QStringLiteral("Не удалось создать TDLib client"));
        return;
    }

    m_receiveThread = std::thread([this] { receiveLoop(); });
    sendRequest(QJsonObject{{QStringLiteral("@type"), QStringLiteral("getAuthorizationState")}});
#endif
}

void TelegramClient::stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    m_stopRequested = true;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

#if NIMARKOGRAM_HAS_TDLIB
    if (m_tdClient) {
        td_json_client_destroy(m_tdClient);
        m_tdClient = nullptr;
    }
#endif
    setState(ClientState::Disconnected);
}

void TelegramClient::setPhoneNumber(const QString &phoneNumber) {
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setAuthenticationPhoneNumber")},
        {QStringLiteral("phone_number"), phoneNumber.trimmed()},
    });
}

void TelegramClient::setAuthenticationCode(const QString &code) {
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("checkAuthenticationCode")},
        {QStringLiteral("code"), code.trimmed()},
    });
}

void TelegramClient::setPassword(const QString &password) {
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("checkAuthenticationPassword")},
        {QStringLiteral("password"), password},
    });
}

void TelegramClient::registerUser(const QString &firstName, const QString &lastName) {
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("registerUser")},
        {QStringLiteral("first_name"), firstName.trimmed()},
        {QStringLiteral("last_name"), lastName.trimmed()},
        {QStringLiteral("disable_notification"), false},
    });
}

void TelegramClient::openChat(qint64 chatId) {
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChat")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("@extra"), QStringLiteral("chat:%1").arg(chatId)},
    });
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatHistory")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("from_message_id"), QStringLiteral("0")},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("limit"), 50},
        {QStringLiteral("only_local"), false},
        {QStringLiteral("@extra"), QStringLiteral("history:%1").arg(chatId)},
    });
}

void TelegramClient::sendDocument(qint64 chatId, const QString &filePath) {
    if (chatId == 0 || filePath.trimmed().isEmpty()) {
        return;
    }

    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("input_message_content"), QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputMessageDocument")},
            {QStringLiteral("document"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("inputDocument")},
                {QStringLiteral("document"), QJsonObject{
                    {QStringLiteral("@type"), QStringLiteral("inputFileLocal")},
                    {QStringLiteral("path"), filePath},
                }},
                {QStringLiteral("thumbnail"), QJsonValue::Null},
                {QStringLiteral("disable_content_type_detection"), false},
            }},
            {QStringLiteral("caption"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("formattedText")},
                {QStringLiteral("text"), QString()},
                {QStringLiteral("entities"), QJsonArray{}},
            }},
        }},
    });
}

void TelegramClient::sendPhoto(qint64 chatId, const QString &filePath) {
    if (chatId == 0 || filePath.trimmed().isEmpty()) {
        return;
    }

    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("input_message_content"), QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputMessagePhoto")},
            {QStringLiteral("photo"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("inputPhoto")},
                {QStringLiteral("photo"), QJsonObject{
                    {QStringLiteral("@type"), QStringLiteral("inputFileLocal")},
                    {QStringLiteral("path"), filePath},
                }},
                {QStringLiteral("thumbnail"), QJsonValue::Null},
                {QStringLiteral("video"), QJsonValue::Null},
                {QStringLiteral("added_sticker_file_ids"), QJsonArray{}},
                {QStringLiteral("width"), 0},
                {QStringLiteral("height"), 0},
            }},
            {QStringLiteral("caption"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("formattedText")},
                {QStringLiteral("text"), QString()},
                {QStringLiteral("entities"), QJsonArray{}},
            }},
            {QStringLiteral("show_caption_above_media"), false},
            {QStringLiteral("self_destruct_type"), QJsonValue::Null},
            {QStringLiteral("has_spoiler"), false},
        }},
    });
}

void TelegramClient::sendVideo(qint64 chatId, const QString &filePath) {
    if (chatId == 0 || filePath.trimmed().isEmpty()) {
        return;
    }

    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("input_message_content"), QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputMessageVideo")},
            {QStringLiteral("video"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("inputVideo")},
                {QStringLiteral("video"), QJsonObject{
                    {QStringLiteral("@type"), QStringLiteral("inputFileLocal")},
                    {QStringLiteral("path"), filePath},
                }},
                {QStringLiteral("thumbnail"), QJsonValue::Null},
                {QStringLiteral("cover"), QJsonValue::Null},
                {QStringLiteral("start_timestamp"), 0},
                {QStringLiteral("added_sticker_file_ids"), QJsonArray{}},
                {QStringLiteral("duration"), 0},
                {QStringLiteral("width"), 0},
                {QStringLiteral("height"), 0},
                {QStringLiteral("supports_streaming"), true},
            }},
            {QStringLiteral("caption"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("formattedText")},
                {QStringLiteral("text"), QString()},
                {QStringLiteral("entities"), QJsonArray{}},
            }},
            {QStringLiteral("show_caption_above_media"), false},
            {QStringLiteral("self_destruct_type"), QJsonValue::Null},
            {QStringLiteral("has_spoiler"), false},
        }},
    });
}

void TelegramClient::downloadFile(int fileId) {
    if (fileId <= 0) {
        return;
    }
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("downloadFile")},
        {QStringLiteral("file_id"), fileId},
        {QStringLiteral("priority"), 32},
        {QStringLiteral("offset"), QStringLiteral("0")},
        {QStringLiteral("limit"), QStringLiteral("0")},
        {QStringLiteral("synchronous"), false},
    });
}

void TelegramClient::sendVoiceNote(qint64 chatId, const QString &filePath) {
    if (chatId == 0 || filePath.trimmed().isEmpty()) {
        return;
    }

    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("input_message_content"), QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputMessageVoiceNote")},
            {QStringLiteral("voice_note"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("inputVoiceNote")},
                {QStringLiteral("voice_note"), QJsonObject{
                    {QStringLiteral("@type"), QStringLiteral("inputFileLocal")},
                    {QStringLiteral("path"), filePath},
                }},
                {QStringLiteral("duration"), 0},
                {QStringLiteral("waveform"), QString()},
            }},
            {QStringLiteral("caption"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("formattedText")},
                {QStringLiteral("text"), QString()},
                {QStringLiteral("entities"), QJsonArray{}},
            }},
            {QStringLiteral("self_destruct_type"), QJsonValue::Null},
        }},
    });
}

void TelegramClient::loadMoreHistory(qint64 chatId, qint64 fromMessageId) {
    if (chatId == 0 || fromMessageId == 0) {
        return;
    }
    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatHistory")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("from_message_id"), QString::number(fromMessageId)},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("limit"), 50},
        {QStringLiteral("only_local"), false},
        {QStringLiteral("@extra"), QStringLiteral("history:%1:older").arg(chatId)},
    });
}

void TelegramClient::sendTextMessage(qint64 chatId, const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    sendRequest(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), QString::number(chatId)},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), QJsonValue::Null},
        {QStringLiteral("options"), emptySendOptions()},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
        {QStringLiteral("input_message_content"), QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputMessageText")},
            {QStringLiteral("text"), QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("formattedText")},
                {QStringLiteral("text"), trimmed},
                {QStringLiteral("entities"), QJsonArray{}},
            }},
            {QStringLiteral("link_preview_options"), QJsonValue::Null},
            {QStringLiteral("clear_draft"), true},
        }},
    });
}

void TelegramClient::receiveLoop() {
#if NIMARKOGRAM_HAS_TDLIB
    while (!m_stopRequested) {
        const char *raw = td_json_client_receive(m_tdClient, 0.25);
        if (!raw) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray(raw), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            fail(QStringLiteral("TDLib вернул некорректный JSON: %1").arg(parseError.errorString()));
            continue;
        }
        handleResponse(document.object());
    }
#endif
}

void TelegramClient::handleResponse(const QJsonObject &response) {
    const QString type = response.value(QStringLiteral("@type")).toString();

    if (type == QStringLiteral("error")) {
        const int code = response.value(QStringLiteral("code")).toInt();
        const QString message = response.value(QStringLiteral("message")).toString();
        fail(QStringLiteral("Telegram error %1: %2").arg(code).arg(message));
        return;
    }

    if (type == QStringLiteral("updateAuthorizationState")) {
        const QJsonObject authorizationState = response.value(QStringLiteral("authorization_state")).toObject();
        const QString state = authorizationState.value(QStringLiteral("@type")).toString();

        if (state == QStringLiteral("authorizationStateWaitTdlibParameters")) {
            sendRequest(QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("setTdlibParameters")},
                {QStringLiteral("use_test_dc"), false},
                {QStringLiteral("database_directory"), m_config.dataDirectory},
                {QStringLiteral("files_directory"), m_config.dataDirectory + QStringLiteral("/files")},
                {QStringLiteral("database_encryption_key"), QString()},
                {QStringLiteral("use_file_database"), true},
                {QStringLiteral("use_chat_info_database"), true},
                {QStringLiteral("use_message_database"), true},
                {QStringLiteral("use_secret_chats"), true},
                {QStringLiteral("api_id"), m_config.apiId},
                {QStringLiteral("api_hash"), m_config.apiHash},
                {QStringLiteral("system_language_code"), QStringLiteral("en")},
                {QStringLiteral("device_model"), QStringLiteral("NimarkoGram Windows")},
                {QStringLiteral("system_version"), QStringLiteral("Windows")},
                {QStringLiteral("application_version"), QStringLiteral(NIMARKOGRAM_DESKTOP_VERSION)},
                {QStringLiteral("enable_storage_optimizer"), true},
            });
        } else if (state == QStringLiteral("authorizationStateWaitPhoneNumber")) {
            setState(ClientState::WaitingForAuthorization);
            emit authorizationPhoneRequired();
        } else if (state == QStringLiteral("authorizationStateWaitCode")) {
            setState(ClientState::WaitingForAuthorization);
            emit authorizationCodeRequired();
        } else if (state == QStringLiteral("authorizationStateWaitPassword")) {
            setState(ClientState::WaitingForAuthorization);
            emit authorizationPasswordRequired();
        } else if (state == QStringLiteral("authorizationStateWaitRegistration")) {
            setState(ClientState::WaitingForAuthorization);
            emit registrationRequired();
        } else if (state == QStringLiteral("authorizationStateReady")) {
            setState(ClientState::Ready);
            emit authorized();
            sendRequest(QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("getChats")},
                {QStringLiteral("chat_list"), QJsonObject{{QStringLiteral("@type"), QStringLiteral("chatListMain")}}},
                {QStringLiteral("limit"), 100},
            });
        } else if (state == QStringLiteral("authorizationStateClosed")) {
            setState(ClientState::Disconnected);
        }
        return;
    }

    if (type == QStringLiteral("chats")) {
        emit chatListReceived(response);
        const QJsonArray chatIds = response.value(QStringLiteral("chat_ids")).toArray();
        for (const QJsonValue &chatId : chatIds) {
            sendRequest(QJsonObject{
                {QStringLiteral("@type"), QStringLiteral("getChat")},
                {QStringLiteral("chat_id"), chatId.toVariant().toLongLong()},
            });
        }
        return;
    }

    if (type == QStringLiteral("chat")) {
        emit chatReceived(response);
        return;
    }

    if (type == QStringLiteral("messages")) {
        emit messageHistoryReceived(response);
        return;
    }

    if (type == QStringLiteral("file")) {
        emit fileUpdated(response);
        return;
    }

    if (type == QStringLiteral("updateFile")) {
        emit fileUpdated(response.value(QStringLiteral("file")).toObject());
        return;
    }

    if (type == QStringLiteral("updateChatUnreadCount")) {
        emit chatUnreadUpdated(
            response.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
            response.value(QStringLiteral("unread_count")).toInt());
        return;
    }

    if (type == QStringLiteral("updateNewChat")) {
        emit chatReceived(response.value(QStringLiteral("chat")).toObject());
        return;
    }

    if (type == QStringLiteral("updateNewMessage")) {
        emit messageReceived(response.value(QStringLiteral("message")).toObject());
        return;
    }

    if (type == QStringLiteral("updateMessageSendSucceeded")) {
        emit messageSendSucceeded(
            response.value(QStringLiteral("old_message_id")).toVariant().toLongLong(),
            response.value(QStringLiteral("message")).toObject().value(QStringLiteral("id")).toVariant().toLongLong());
        return;
    }

    if (type == QStringLiteral("updateMessageSendFailed")) {
        const QJsonObject error = response.value(QStringLiteral("error")).toObject();
        emit messageSendFailed(
            response.value(QStringLiteral("old_message_id")).toVariant().toLongLong(),
            error.value(QStringLiteral("message")).toString());
        return;
    }
}

void TelegramClient::sendRequest(const QJsonObject &request) {
#if NIMARKOGRAM_HAS_TDLIB
    if (!m_tdClient) {
        fail(QStringLiteral("TDLib client ещё не запущен"));
        return;
    }
    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    td_json_client_send(m_tdClient, payload.constData());
#else
    Q_UNUSED(request)
#endif
}

void TelegramClient::setState(ClientState state) {
    m_state = state;
    emit stateChanged(state);
}

void TelegramClient::fail(const QString &message) {
    {
        std::lock_guard lock(m_errorMutex);
        m_lastError = message;
    }
    m_state = ClientState::Error;
    emit stateChanged(ClientState::Error);
    emit errorOccurred(message);
}

} // namespace nimarkogram::core
