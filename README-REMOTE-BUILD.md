# NimarkoGram Windows remote build

Этот пакет предназначен для сборки через GitHub Actions. Локально устанавливать Qt, CMake, Ninja и Git не требуется.

## Как запустить сборку

1. Создайте новый пустой публичный репозиторий на GitHub.
2. Распакуйте этот архив.
3. Загрузите в репозиторий содержимое распакованной папки, включая `.github/workflows/windows-desktop.yml`.
4. Откройте вкладку **Actions**.
5. Выберите **Native Windows client** и нажмите **Run workflow**.
6. Дождитесь окончания сборки.
7. В результате job скачайте artifact `NimarkoGram-windows-portable`.

Workflow сам устанавливает Qt, зависимости TDLib и собирает portable ZIP на Windows runner.

Для Telegram при первом запуске приложения потребуются собственные API ID и API hash.
