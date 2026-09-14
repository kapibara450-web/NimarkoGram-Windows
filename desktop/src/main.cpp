#include <QApplication>
#include <QAudioInput>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QMediaCaptureSession>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QTextCursor>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "core/telegram_client.h"

using nimarkogram::core::ClientConfig;
using nimarkogram::core::ClientState;
using nimarkogram::core::TelegramClient;

namespace {

int mediaFileId(const QJsonObject &message) {
    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    const QString type = content.value(QStringLiteral("@type")).toString();
    if (type == QStringLiteral("messagePhoto")) {
        const QJsonArray sizes = content.value(QStringLiteral("photo")).toObject()
            .value(QStringLiteral("sizes")).toArray();
        if (!sizes.isEmpty()) {
            return sizes.last().toObject().value(QStringLiteral("photo")).toObject()
                .value(QStringLiteral("id")).toInt();
        }
    } else if (type == QStringLiteral("messageVideo")) {
        return content.value(QStringLiteral("video")).toObject()
            .value(QStringLiteral("video")).toObject().value(QStringLiteral("id")).toInt();
    } else if (type == QStringLiteral("messageDocument")) {
        return content.value(QStringLiteral("document")).toObject()
            .value(QStringLiteral("document")).toObject().value(QStringLiteral("id")).toInt();
    } else if (type == QStringLiteral("messageAudio")) {
        return content.value(QStringLiteral("audio")).toObject()
            .value(QStringLiteral("audio")).toObject().value(QStringLiteral("id")).toInt();
    } else if (type == QStringLiteral("messageVoiceNote")) {
        return content.value(QStringLiteral("voice_note")).toObject()
            .value(QStringLiteral("voice")).toObject().value(QStringLiteral("id")).toInt();
    }
    return 0;
}

QString darkStyleSheet() {
    return QStringLiteral(
        "QMainWindow, QWidget { background: #202124; color: #e8eaed; }"
        "QLineEdit, QTextEdit, QListWidget { background: #292a2d; color: #e8eaed; "
        "border: 1px solid #4a4d52; border-radius: 6px; padding: 5px; }"
        "QListWidget::item:selected { background: #3f51b5; color: white; }"
        "QPushButton { background: #3f51b5; color: white; border: 0; "
        "border-radius: 6px; padding: 7px 12px; }"
        "QPushButton:disabled { background: #44464b; color: #9aa0a6; }"
        "QStatusBar { background: #292a2d; color: #bdc1c6; }"
    );
}

QString textFromMessage(const QJsonObject &message) {
    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    const QString contentType = content.value(QStringLiteral("@type")).toString();
    if (contentType == QStringLiteral("messageText")) {
        return content.value(QStringLiteral("text")).toObject().value(QStringLiteral("text")).toString();
    }
    if (contentType == QStringLiteral("messagePhoto")) {
        return QStringLiteral("[Фото]");
    }
    if (contentType == QStringLiteral("messageVideo")) {
        return QStringLiteral("[Видео]");
    }
    if (contentType == QStringLiteral("messageDocument")) {
        return QStringLiteral("[Файл]");
    }
    if (contentType == QStringLiteral("messageAudio")) {
        return QStringLiteral("[Аудио]");
    }
    if (contentType == QStringLiteral("messageVoiceNote")) {
        return QStringLiteral("[Голосовое сообщение]");
    }
    return QStringLiteral("[%1]").arg(contentType.isEmpty() ? QStringLiteral("сообщение") : contentType);
}

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(TelegramClient &client, QWidget *parent = nullptr)
        : QMainWindow(parent), m_client(client) {
        setWindowTitle(QStringLiteral("NimarkoGram — Windows"));
        resize(1180, 760);
        setMinimumSize(900, 600);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(1);

        auto *leftPanel = new QWidget(splitter);
        auto *leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(14, 14, 10, 14);
        leftLayout->setSpacing(10);

        auto *brand = new QLabel(QStringLiteral("NimarkoGram"), leftPanel);
        QFont brandFont = brand->font();
        brandFont.setPointSize(18);
        brandFont.setBold(true);
        brand->setFont(brandFont);
        leftLayout->addWidget(brand);

        auto *search = new QLineEdit(leftPanel);
        search->setPlaceholderText(QStringLiteral("Поиск чатов"));
        leftLayout->addWidget(search);
        connect(search, &QLineEdit::textChanged, this, [this](const QString &query) {
            const QString normalized = query.trimmed();
            for (auto it = m_chatItems.cbegin(); it != m_chatItems.cend(); ++it) {
                it.value()->setHidden(!normalized.isEmpty() &&
                    !it.value()->text().contains(normalized, Qt::CaseInsensitive));
            }
        });

        m_chatList = new QListWidget(leftPanel);
        m_chatList->addItem(QStringLiteral("Избранное"));
        m_chatList->addItem(QStringLiteral("Подключение к Telegram…"));
        m_chatList->setAlternatingRowColors(true);
        leftLayout->addWidget(m_chatList, 1);

        auto *rightPanel = new QWidget(splitter);
        auto *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(18, 14, 18, 14);
        rightLayout->setSpacing(12);

        m_header = new QLabel(QStringLiteral("Нативный Windows-клиент"), rightPanel);
        QFont headerFont = m_header->font();
        headerFont.setPointSize(16);
        headerFont.setBold(true);
        m_header->setFont(headerFont);
        rightLayout->addWidget(m_header);

        m_conversation = new QTextEdit(rightPanel);
        m_conversation->setReadOnly(true);
        m_conversation->setPlaceholderText(
            QStringLiteral("Здесь появятся сообщения после подключения к Telegram."));
        rightLayout->addWidget(m_conversation, 1);

        auto *composeRow = new QHBoxLayout();
        m_compose = new QLineEdit(rightPanel);
        m_compose->setPlaceholderText(QStringLiteral("Сообщение"));
        m_compose->setEnabled(false);
        m_attach = new QPushButton(QStringLiteral("Файл"), rightPanel);
        m_attach->setEnabled(false);
        m_download = new QPushButton(QStringLiteral("Скачать медиа"), rightPanel);
        m_download->setEnabled(false);
        m_record = new QPushButton(QStringLiteral("Голос"), rightPanel);
        m_record->setEnabled(false);
        m_send = new QPushButton(QStringLiteral("Отправить"), rightPanel);
        m_send->setEnabled(false);
        composeRow->addWidget(m_compose, 1);
        composeRow->addWidget(m_attach);
        composeRow->addWidget(m_download);
        composeRow->addWidget(m_record);
        composeRow->addWidget(m_send);
        rightLayout->addLayout(composeRow);

        splitter->addWidget(leftPanel);
        splitter->addWidget(rightPanel);
        splitter->setSizes({320, 860});
        setCentralWidget(splitter);

        m_audioInput = new QAudioInput(this);
        m_mediaSession = new QMediaCaptureSession(this);
        m_recorder = new QMediaRecorder(this);
        m_mediaSession->setAudioInput(m_audioInput);
        m_mediaSession->setRecorder(m_recorder);
        QMediaFormat recordingFormat;
        recordingFormat.setFileFormat(QMediaFormat::MPEG4);
        recordingFormat.setAudioCodec(QMediaFormat::AudioCodec::AAC);
        m_recorder->setMediaFormat(recordingFormat);

        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            m_tray = new QSystemTrayIcon(
                QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), this);
            m_tray->setToolTip(QStringLiteral("NimarkoGram"));
            auto *trayMenu = new QMenu(this);
            auto *showAction = trayMenu->addAction(QStringLiteral("Открыть NimarkoGram"));
            auto *themeAction = trayMenu->addAction(QStringLiteral("Тёмная тема"));
            QSettings appearance(QSettings::IniFormat, QSettings::UserScope,
                                 QStringLiteral("NimarkoGram"), QStringLiteral("desktop"));
            themeAction->setCheckable(true);
            themeAction->setChecked(appearance.value(QStringLiteral("appearance/dark"), false).toBool());
            trayMenu->addSeparator();
            auto *quitAction = trayMenu->addAction(QStringLiteral("Выйти"));
            connect(showAction, &QAction::triggered, this, [this] {
                showNormal();
                raise();
                activateWindow();
            });
            connect(themeAction, &QAction::toggled, this, [](bool enabled) {
                qApp->setStyleSheet(enabled ? darkStyleSheet() : QString());
                QSettings appearance(QSettings::IniFormat, QSettings::UserScope,
                                     QStringLiteral("NimarkoGram"), QStringLiteral("desktop"));
                appearance.setValue(QStringLiteral("appearance/dark"), enabled);
            });
            connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
            m_tray->setContextMenu(trayMenu);
            m_tray->show();
        }

        statusBar()->showMessage(QStringLiteral("Подготовка подключения…"));

        connect(&client, &TelegramClient::stateChanged, this,
                [this](ClientState state) {
                    switch (state) {
                    case ClientState::Connecting:
                        statusBar()->showMessage(QStringLiteral("Подключение…"));
                        break;
                    case ClientState::WaitingForAuthorization:
                        statusBar()->showMessage(QStringLiteral("Ожидание авторизации"));
                        break;
                    case ClientState::Ready:
                        statusBar()->showMessage(QStringLiteral("Подключено"));
                        break;
                    case ClientState::Disconnected:
                        statusBar()->showMessage(QStringLiteral("Отключено"));
                        break;
                    case ClientState::Error:
                        statusBar()->showMessage(QStringLiteral("Ошибка подключения"));
                        break;
                    }
                });

        connect(&client, &TelegramClient::errorOccurred, this,
                [this](const QString &message) {
                    QMessageBox::critical(this, QStringLiteral("NimarkoGram"), message);
                });

        connect(&client, &TelegramClient::authorizationPhoneRequired, this,
                [this] { askPhoneNumber(); });
        connect(&client, &TelegramClient::authorizationCodeRequired, this,
                [this] { askCode(); });
        connect(&client, &TelegramClient::authorizationPasswordRequired, this,
                [this] { askPassword(); });
        connect(&client, &TelegramClient::registrationRequired, this,
                [this] { askRegistration(); });
        connect(&client, &TelegramClient::authorized, this,
                [this] {
                    m_conversation->setPlainText(
                        QStringLiteral("Авторизация выполнена. Список чатов загружается…"));
                });

        connect(m_chatList, &QListWidget::itemClicked, this,
                [this](QListWidgetItem *item) {
                    const QVariant value = item->data(Qt::UserRole);
                    if (!value.isValid()) {
                        return;
                    }
                    m_currentChatId = value.toLongLong();
                    m_chatUnread.insert(m_currentChatId, 0);
                    refreshChatItem(m_currentChatId);
                    m_header->setText(m_chatTitles.value(m_currentChatId, item->text()));
                    m_compose->setEnabled(true);
                    m_attach->setEnabled(true);
                    m_download->setEnabled(m_lastMediaFileId > 0);
                    m_record->setEnabled(true);
                    m_send->setEnabled(true);
                    m_conversation->clear();
                    m_oldestMessageId = 0;
                    m_loadingOlder = false;
                    m_client.openChat(m_currentChatId);
                });

        connect(m_send, &QPushButton::clicked, this, [this] { sendCurrentMessage(); });
        connect(m_compose, &QLineEdit::returnPressed, this, [this] { sendCurrentMessage(); });
        connect(m_attach, &QPushButton::clicked, this, [this] { sendCurrentFile(); });
        connect(m_download, &QPushButton::clicked, this, [this] {
            if (m_lastMediaFileId > 0) {
                m_pendingDownloadId = m_lastMediaFileId;
                m_client.downloadFile(m_pendingDownloadId);
                statusBar()->showMessage(QStringLiteral("Загрузка медиа…"));
            }
        });
        connect(m_record, &QPushButton::clicked, this, [this] { toggleRecording(); });
        connect(m_recorder, &QMediaRecorder::recorderStateChanged, this,
                [this](QMediaRecorder::RecorderState state) { handleRecorderState(state); });
        connect(m_recorder, &QMediaRecorder::errorOccurred, this,
                [this](QMediaRecorder::Error, const QString &message) {
                    m_record->setText(QStringLiteral("Голос"));
                    m_record->setEnabled(m_currentChatId != 0);
                    QMessageBox::warning(this, QStringLiteral("Запись голоса"), message);
                });

        connect(m_conversation->verticalScrollBar(), &QScrollBar::valueChanged, this,
                [this](int value) {
                    if (value == 0 && !m_loadingOlder && m_currentChatId != 0 && m_oldestMessageId != 0) {
                        m_loadingOlder = true;
                        m_client.loadMoreHistory(m_currentChatId, m_oldestMessageId);
                    }
                });

        connect(&client, &TelegramClient::chatListReceived, this,
                [this](const QJsonObject &) {
                    m_chatList->clear();
                    m_chatItems.clear();
                    m_chatTitles.clear();
                    m_chatUnread.clear();
                });
        connect(&client, &TelegramClient::chatReceived, this,
                [this](const QJsonObject &chat) { addOrUpdateChat(chat); });
        connect(&client, &TelegramClient::chatUnreadUpdated, this,
                [this](qint64 chatId, int unreadCount) {
                    m_chatUnread.insert(chatId, unreadCount);
                    refreshChatItem(chatId);
                });
        connect(&client, &TelegramClient::messageHistoryReceived, this,
                [this](const QJsonObject &messages) {
                    const QJsonArray items = messages.value(QStringLiteral("messages")).toArray();
                    const bool older = messages.value(QStringLiteral("@extra")).toString().endsWith(QStringLiteral(":older"));
                    if (items.isEmpty()) {
                        m_loadingOlder = false;
                        return;
                    }
                    if (!older) {
                        m_conversation->clear();
                        m_lastMediaFileId = 0;
                        m_download->setEnabled(false);
                        m_oldestMessageId = items.first().toObject().value(QStringLiteral("id")).toVariant().toLongLong();
                        for (const QJsonValue &item : items) {
                            appendMessage(item.toObject());
                        }
                    } else {
                        const int oldScrollValue = m_conversation->verticalScrollBar()->value();
                        QString olderText;
                        for (const QJsonValue &item : items) {
                            const QJsonObject message = item.toObject();
                            if (olderText.isEmpty()) {
                                olderText = formattedMessage(message);
                            } else {
                                olderText += QStringLiteral("\n") + formattedMessage(message);
                            }
                        }
                        m_conversation->moveCursor(QTextCursor::Start);
                        m_conversation->insertPlainText(olderText + QStringLiteral("\n"));
                        m_conversation->verticalScrollBar()->setValue(oldScrollValue + items.size());
                        m_oldestMessageId = items.first().toObject().value(QStringLiteral("id")).toVariant().toLongLong();
                    }
                    m_loadingOlder = false;
                });
        connect(&client, &TelegramClient::messageReceived, this,
                [this](const QJsonObject &message) {
                    const qint64 chatId = message.value(QStringLiteral("chat_id")).toVariant().toLongLong();
                    const bool currentChat = chatId == m_currentChatId;
                    if (currentChat) {
                        appendMessage(message);
                    }
                    if (!message.value(QStringLiteral("is_outgoing")).toBool() && !currentChat && m_tray) {
                        const QString body = textFromMessage(message);
                        if (!body.isEmpty()) {
                            m_tray->showMessage(QStringLiteral("Новое сообщение"), body,
                                                QSystemTrayIcon::Information, 5000);
                        }
                    }
                });
        connect(&client, &TelegramClient::fileUpdated, this,
                [this](const QJsonObject &file) { handleFileUpdate(file); });
        connect(&client, &TelegramClient::messageSendSucceeded, this,
                [this](qint64, qint64) {
                    statusBar()->showMessage(QStringLiteral("Сообщение отправлено"), 2500);
                });
        connect(&client, &TelegramClient::messageSendFailed, this,
                [this](qint64, const QString &error) {
                    statusBar()->showMessage(QStringLiteral("Ошибка отправки: %1").arg(error), 6000);
                });
    }

private:
    void askPhoneNumber() {
        bool ok = false;
        const QString phone = QInputDialog::getText(
            this, QStringLiteral("Вход в Telegram"),
            QStringLiteral("Номер телефона в международном формате:"),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !phone.trimmed().isEmpty()) {
            m_client.setPhoneNumber(phone);
        }
    }

    void askCode() {
        bool ok = false;
        const QString code = QInputDialog::getText(
            this, QStringLiteral("Код Telegram"),
            QStringLiteral("Введите код из Telegram:"),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !code.trimmed().isEmpty()) {
            m_client.setAuthenticationCode(code);
        }
    }

    void askPassword() {
        bool ok = false;
        const QString password = QInputDialog::getText(
            this, QStringLiteral("Пароль двухэтапной проверки"),
            QStringLiteral("Введите пароль Telegram:"),
            QLineEdit::Password, QString(), &ok);
        if (ok) {
            m_client.setPassword(password);
        }
    }

    void askRegistration() {
        bool ok = false;
        const QString firstName = QInputDialog::getText(
            this, QStringLiteral("Регистрация"), QStringLiteral("Имя:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || firstName.trimmed().isEmpty()) {
            return;
        }
        const QString lastName = QInputDialog::getText(
            this, QStringLiteral("Регистрация"), QStringLiteral("Фамилия (необязательно):"),
            QLineEdit::Normal, QString(), &ok);
        if (ok) {
            m_client.registerUser(firstName, lastName);
        }
    }

    void addOrUpdateChat(const QJsonObject &chat) {
        const qint64 chatId = chat.value(QStringLiteral("id")).toVariant().toLongLong();
        if (chatId == 0) {
            return;
        }
        const QString title = chat.value(QStringLiteral("title")).toString().isEmpty()
            ? QStringLiteral("Чат %1").arg(chatId)
            : chat.value(QStringLiteral("title")).toString();
        m_chatTitles.insert(chatId, title);
        if (chat.contains(QStringLiteral("unread_count"))) {
            m_chatUnread.insert(chatId, chat.value(QStringLiteral("unread_count")).toInt());
        }

        if (!m_chatItems.contains(chatId)) {
            auto *item = new QListWidgetItem(m_chatList);
            item->setData(Qt::UserRole, chatId);
            m_chatItems.insert(chatId, item);
        }
        refreshChatItem(chatId);
    }

    void refreshChatItem(qint64 chatId) {
        if (!m_chatItems.contains(chatId)) {
            return;
        }
        QString title = m_chatTitles.value(chatId, QStringLiteral("Чат %1").arg(chatId));
        const int unread = m_chatUnread.value(chatId, 0);
        if (unread > 0) {
            title += QStringLiteral("  (%1)").arg(unread);
        }
        m_chatItems.value(chatId)->setText(title);
    }

    QString formattedMessage(const QJsonObject &message) const {
        const QString text = textFromMessage(message);
        if (text.isEmpty()) {
            return QString();
        }
        const bool outgoing = message.value(QStringLiteral("is_outgoing")).toBool();
        const qint64 timestamp = message.value(QStringLiteral("date")).toVariant().toLongLong();
        const QString time = timestamp > 0
            ? QDateTime::fromSecsSinceEpoch(timestamp).toLocalTime().toString(QStringLiteral("HH:mm"))
            : QStringLiteral("--:--");
        return QStringLiteral("[%1] %2%3").arg(
            time, outgoing ? QStringLiteral("Вы: ") : QStringLiteral("Собеседник: "), text);
    }

    void appendMessage(const QJsonObject &message) {
        const int fileId = mediaFileId(message);
        if (fileId > 0) {
            m_lastMediaFileId = fileId;
            m_download->setEnabled(true);
        }
        const QString text = formattedMessage(message);
        if (!text.isEmpty()) {
            m_conversation->append(text);
        }
    }

    void toggleRecording() {
        if (m_recorder->recorderState() == QMediaRecorder::RecordingState) {
            m_recorder->stop();
            return;
        }
        if (m_currentChatId == 0) {
            return;
        }

        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/recordings");
        QDir().mkpath(directory);
        m_recordingPath = directory + QStringLiteral("/voice-%1.m4a")
            .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss")));
        m_recordingChatId = m_currentChatId;
        m_isRecording = true;
        m_recorder->setOutputLocation(QUrl::fromLocalFile(m_recordingPath));
        m_recorder->record();
        m_record->setText(QStringLiteral("Стоп"));
        statusBar()->showMessage(QStringLiteral("Идёт запись голоса…"));
    }

    void handleRecorderState(QMediaRecorder::RecorderState state) {
        if (state == QMediaRecorder::RecordingState) {
            m_record->setText(QStringLiteral("Стоп"));
            return;
        }
        if (state != QMediaRecorder::StoppedState || !m_isRecording) {
            return;
        }

        m_isRecording = false;
        m_record->setText(QStringLiteral("Голос"));
        if (m_recordingChatId != 0 && QFile::exists(m_recordingPath)) {
            m_client.sendVoiceNote(m_recordingChatId, m_recordingPath);
            statusBar()->showMessage(QStringLiteral("Голосовое сообщение отправляется…"));
        }
        m_recordingPath.clear();
        m_recordingChatId = 0;
    }

    void handleFileUpdate(const QJsonObject &file) {
        const int fileId = file.value(QStringLiteral("id")).toInt();
        const QJsonObject local = file.value(QStringLiteral("local")).toObject();
        if (fileId <= 0 || fileId != m_pendingDownloadId ||
            !local.value(QStringLiteral("is_downloading_completed")).toBool()) {
            return;
        }

        const QString sourcePath = local.value(QStringLiteral("path")).toString();
        if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
            statusBar()->showMessage(QStringLiteral("Файл ещё не готов"));
            return;
        }
        const QString defaultName = QFileInfo(sourcePath).fileName();
        const QString targetPath = QFileDialog::getSaveFileName(
            this, QStringLiteral("Сохранить медиа"), defaultName);
        if (!targetPath.isEmpty()) {
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            if (QFile::copy(sourcePath, targetPath)) {
                statusBar()->showMessage(QStringLiteral("Файл сохранён"));
            } else {
                statusBar()->showMessage(QStringLiteral("Не удалось сохранить файл"));
            }
        }
        m_pendingDownloadId = 0;
    }

    void sendCurrentFile() {
        if (m_currentChatId == 0) {
            return;
        }
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Выбрать файл"));
        if (path.isEmpty()) {
            return;
        }
        const QString suffix = QFileInfo(path).suffix().toLower();
        static const QStringList imageExtensions = {
            QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
            QStringLiteral("webp"), QStringLiteral("heic")};
        static const QStringList videoExtensions = {
            QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
            QStringLiteral("webm"), QStringLiteral("avi")};
        static const QStringList voiceExtensions = {
            QStringLiteral("ogg"), QStringLiteral("opus")};
        if (imageExtensions.contains(suffix)) {
            m_client.sendPhoto(m_currentChatId, path);
        } else if (videoExtensions.contains(suffix)) {
            m_client.sendVideo(m_currentChatId, path);
        } else if (voiceExtensions.contains(suffix)) {
            m_client.sendVoiceNote(m_currentChatId, path);
        } else {
            m_client.sendDocument(m_currentChatId, path);
        }
    }

    void sendCurrentMessage() {
        if (m_currentChatId == 0) {
            return;
        }
        const QString text = m_compose->text();
        if (text.trimmed().isEmpty()) {
            return;
        }
        m_client.sendTextMessage(m_currentChatId, text);
        m_compose->clear();
    }

    TelegramClient &m_client;
    QLabel *m_header = nullptr;
    QListWidget *m_chatList = nullptr;
    QTextEdit *m_conversation = nullptr;
    QLineEdit *m_compose = nullptr;
    QPushButton *m_send = nullptr;
    QPushButton *m_attach = nullptr;
    QPushButton *m_download = nullptr;
    QPushButton *m_record = nullptr;
    QAudioInput *m_audioInput = nullptr;
    QMediaCaptureSession *m_mediaSession = nullptr;
    QMediaRecorder *m_recorder = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_recordingPath;
    qint64 m_recordingChatId = 0;
    bool m_isRecording = false;
    QHash<qint64, QListWidgetItem *> m_chatItems;
    QHash<qint64, QString> m_chatTitles;
    QHash<qint64, int> m_chatUnread;
    qint64 m_currentChatId = 0;
    int m_lastMediaFileId = 0;
    int m_pendingDownloadId = 0;
    qint64 m_oldestMessageId = 0;
    bool m_loadingOlder = false;
};

ClientConfig loadConfig(QWidget *parent) {
    ClientConfig config = ClientConfig::fromEnvironment();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("NimarkoGram"), QStringLiteral("desktop"));
    if (config.apiId <= 0) {
        config.apiId = settings.value(QStringLiteral("telegram/api_id"), 0).toInt();
    }
    if (config.dataDirectory.isEmpty()) {
        config.dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/tdlib");
    }

    if (config.apiId <= 0) {
        bool accepted = false;
        config.apiId = QInputDialog::getInt(
            parent, QStringLiteral("Настройка Telegram API"),
            QStringLiteral("Введите API ID из my.telegram.org:"),
            0, 1, 2147483647, 1, &accepted);
        if (!accepted) {
            config.apiId = 0;
        }
    }
    if (config.apiHash.isEmpty() && config.apiId > 0) {
        bool accepted = false;
        config.apiHash = QInputDialog::getText(
            parent, QStringLiteral("Настройка Telegram API"),
            QStringLiteral("Введите API hash из my.telegram.org:"),
            QLineEdit::Normal, QString(), &accepted);
        if (!accepted) {
            config.apiHash.clear();
        }
    }
    return config;
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NimarkoGram"));
    app.setApplicationVersion(QStringLiteral(NIMARKOGRAM_DESKTOP_VERSION));
    app.setOrganizationName(QStringLiteral("NimarkoGram"));

    QSettings appearance(QSettings::IniFormat, QSettings::UserScope,
                         QStringLiteral("NimarkoGram"), QStringLiteral("desktop"));
    if (appearance.value(QStringLiteral("appearance/dark"), false).toBool()) {
        app.setStyleSheet(darkStyleSheet());
    }

    const ClientConfig config = loadConfig(nullptr);
    TelegramClient client(config);
    MainWindow window(client);
    window.show();

    QTimer::singleShot(0, &client, &TelegramClient::start);
    return app.exec();
}
