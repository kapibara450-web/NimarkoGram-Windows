# NimarkoGram — Native Windows client

Это отдельный нативный клиент Windows, который создаётся рядом с Android-проектом. Android-сборка не изменяется и остаётся источником поведения и функций для поэтапного переноса.

> Текущий код — рабочий первый прототип: окно, конфигурация TDLib, запуск authorization flow и загрузка идентификаторов чатов. Это ещё не готовый Telegram-клиент.

## Что уже работает

- C++20 + Qt 6 + CMake.
- TDLib подключается через pinned revision или локальную копию.
- `setTdlibParameters` и локальное TDLib-хранилище.
- Вход по номеру телефона, коду Telegram и паролю 2FA.
- Первичная регистрация пользователя.
- Получение списка идентификаторов основных чатов.
- Базовое окно Windows-клиента.

## Архитектура

- `src/core` — кроссплатформенное ядро сессии, MTProto, хранилища и синхронизации.
- `src/ui` — интерфейс Qt 6 для Windows.
- `src/platform/windows` — уведомления, файловая система, интеграция с Windows и системные функции.
- `src/plugins` — отдельный безопасный API для плагинов NimarkoGram.

Android-зависимости (`Activity`, `Context`, CameraX, Android Keystore, Android notifications и Chaquopy) не переносятся как есть. Для них будут сделаны Windows-адаптеры.

## Сборка native Windows-версии

Требования Android из корневого README (`JDK`, `Android SDK`, `NDK`, `Chaquopy/Python`) относятся только к Android APK и для нативной Windows-версии не нужны.

Для Windows-клиента нужны:

| Компонент | Версия |
|---|---|
| Windows | 10/11 x64 |
| Visual Studio | 2022, workload Desktop development with C++ |
| MSVC | toolset из Visual Studio 2022 |
| Qt | 6.6+ с модулями Qt Base и Qt Multimedia |
| CMake | 3.24+ |
| Ninja | актуальная версия |
| vcpkg | для OpenSSL и zlib TDLib |
| Git | актуальная версия |

Если не хочется создавать Qt-аккаунт, Qt можно установить через vcpkg вместе с зависимостями:

```powershell
C:\vcpkg\vcpkg.exe install openssl:x64-windows zlib:x64-windows qtbase:x64-windows qtmultimedia:x64-windows
```

Это может занять больше времени и места на диске, но отдельный Qt Online Installer и Qt-аккаунт не понадобятся.

Если Git не устанавливается, можно скачать архивы vcpkg и TDLib напрямую через браузер/PowerShell. Скрипт поддерживает переменную `TDLIB_ROOT` и в этом режиме не использует Git для загрузки TDLib.

JDK, Android SDK/NDK, Gradle и Python нужны только если параллельно собирается Android APK.

Требуется Qt 6.6 или новее и CMake 3.24 или новее.

Для каркаса без TDLib:

```powershell
cmake -S desktop -B desktop/build -G Ninja
cmake --build desktop/build --config Release
```

Для прототипа с TDLib:

```powershell
cmake -S desktop -B desktop/build -G Ninja `
  -DNIMARKOGRAM_FETCH_TDLIB=ON
cmake --build desktop/build --config Release --parallel
```

Шаблон Windows workflow находится в `desktop/ci/windows-desktop.yml`; для GitHub Actions его нужно положить в `.github/workflows/windows-desktop.yml`. Он устанавливает зависимости TDLib, собирает приложение и создаёт `NimarkoGram-windows-portable.zip`.

## Конфигурация

Поддерживаются переменные окружения:

- `NIMARKOGRAM_API_ID`
- `NIMARKOGRAM_API_HASH`
- `NIMARKOGRAM_DATA_DIR`

Также приложение читает пользовательский INI-файл Qt Settings. Секреты не должны попадать в Git.

## Статус

- [x] Базовый CMake-проект Windows
- [x] Первое окно и компоновка клиента
- [x] Подключение TDLib JSON API
- [x] Авторизация по телефону, коду и паролю
- [x] Первичная загрузка списка чатов
- [x] Модель чатов и история переписки
- [x] Отправка текстовых сообщений
- [x] Выбор и отправка файлов как документов
- [x] Отправка изображений и видео через локальный файл
- [x] Запуск загрузки медиа и сохранение готового файла в выбранную папку
- [x] Отправка OGG/Opus как голосового сообщения
- [x] Запись голоса с микрофона Windows и отправка M4A как голосового сообщения
- [x] Уведомления Windows через системный трей
- [x] Фильтрация списка чатов поиском
- [x] Счётчики непрочитанных сообщений
- [x] Переключатель тёмной темы в системном трее
- [x] Подгрузка старых сообщений при прокрутке вверх
- [x] Статусы успешной и неудачной отправки
- [ ] Потоковая загрузка и полноценный предпросмотр медиа
- [ ] Уведомления Windows
- [ ] Темы, вкладки и расширения NimarkoGram
- [ ] Плагинная система
- [ ] Звонки и видеосообщения
- [ ] Установщик и автоматические обновления
