# NimarkoGram Android → Native Windows

Цель — перенести поведение NimarkoGram, а не просто сделать новое окно Telegram. Каждая Android-зависимость должна получить отдельный Windows-адаптер или обоснованную замену.

| Область | Android-реализация | Windows-план | Статус |
|---|---|---|---|
| Telegram API / MTProto | Telegram Android core + JNI | TDLib JSON API в desktop core; затем перенос NimarkoGram-specific hooks | Прототип |
| Аккаунты | Android account/session services | Windows session manager + DPAPI | Запланировано |
| Чаты и сообщения | Android views/controllers | Qt model/view + virtualized message list | Запланировано |
| Медиа и файлы | Android media pipeline + JNI codecs | TDLib documents first; shared native codecs + Windows file layer next | Прототип |
| Уведомления | Android notification channels | Windows Toast notifications | Запланировано |
| Темы и цвета | Android resources / Monet | Qt theme engine | Запланировано |
| Вкладки и chat headers | NimarkoGram UI extensions | Recreate in desktop UI | Запланировано |
| Камера | CameraX | Windows Media Foundation / camera API | Запланировано |
| Биометрия | Android BiometricPrompt | Windows Hello, с fallback на пароль | Запланировано |
| Python plugins | Chaquopy + Android bridge | Embedded Python + capability-based desktop API | Запланировано |
| DEX plugins | Android/Dex loading | Не переносится напрямую; нужен desktop plugin ABI | Запланировано |
| Clipboard / share | Android intents | Windows Shell / clipboard | Запланировано |
| Calls | Android audio/video stack | Windows audio/video backend | Запланировано |
| Network tools | Android networking hooks | Windows networking adapter | Запланировано |
| Privacy controls | Android permissions / secure storage | Windows permissions / DPAPI / sandboxing | Запланировано |
| Обновления | Android APK/update flow | Signed Windows installer/update channel | Запланировано |
