# LAN Messenger

Рассылка сообщений по диапазону IP-адресов в локальной сети. На компьютерах пользователей работает фоновый агент
(MSI, устанавливается через групповую политику), у отправителя — таблица: **доставлено / не доставлено**,
**прочитано / не прочитано**. Отправлять можно с ПК или со смартфона.

## Документация

* **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** — установка, GPO, работа с отправителем и телефоном, статусы, ключ, порты, диагностика.
* [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — устройство, протокол, структура кода.

## Быстрый старт

1. Открыть `CMakeLists.txt` в Qt Creator (комплект Desktop Qt 6.11.2 MinGW 64-bit) → собрать.
2. Распаковать WiX Toolset 3.14 в `installer\wix314\` и выполнить `.\installer\build-msi.ps1 -SharedKey "ВашКлюч"`.
3. Установить `dist\LanMessengerAgent.msi` на компьютеры (GPO: «Установка программного обеспечения → Назначенный»).
4. Запустить `dist\LanMessengerSender\LanMessengerSender.exe`, ввести ключ, диапазон и текст.

Проверка без установки: `set LM_SHARED_KEY=testkey`, `LanMessengerAgent.exe --console`, затем `lm_selftest.exe`.

## Состав

| Папка | Результат |
|---|---|
| `agent/` | `LanMessengerAgent.exe` — Windows-служба |
| `agentui/` | `LanMessengerUI.exe` — окно сообщения (только чтение, кнопка «Прочитано») |
| `sender/` | `LanMessengerSender.exe` — отправитель для ПК |
| `sendermobile/` | `LanMessengerMobile` — отправитель для Android (Qt Quick) |
| `common/`, `sendercore/` | общие библиотеки: протокол, движок отправителя |
| `installer/` | WiX-описание и скрипт сборки MSI |
| `tests/` | самотест полного цикла |
# messager
