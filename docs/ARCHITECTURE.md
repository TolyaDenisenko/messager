# LAN Messenger — архитектура и протокол

Для разработчиков. Руководство пользователя — [USER_GUIDE.md](USER_GUIDE.md).

## Структура проекта

```
CMakeLists.txt        корневой проект (Qt Creator открывает его)
common/               lm_common — протокол и сетевой ввод-вывод
sendercore/           lm_sendercore — движок отправителя (без GUI)
agent/                LanMessengerAgent.exe — Windows-служба
agentui/              LanMessengerUI.exe — окно сообщения у пользователя
sender/               LanMessengerSender.exe — отправитель для ПК (Widgets)
sendermobile/         LanMessengerMobile — отправитель для смартфона (Qt Quick, Material)
tests/                lm_selftest.exe — самотест полного цикла
installer/            Product.wxs (WiX 3) и build-msi.ps1
docs/                 документация
```

Зависимости: `agent`, `agentui` → `common`; `sender`, `sendermobile`, `tests` → `sendercore` → `common`.

## Компоненты

### common
* `protocol.h/.cpp` — структура `Packet`, подпись и проверка HMAC, кодирование строк JSON.
* `lineio.h/.cpp`
  * `serveLine()` — принять одну строку из `QIODevice` (TCP-сокет или named pipe), вызвать обработчик, записать ответ, закрыть. Лимит строки 256 КБ, таймаут 10 с.
  * `LineServer` — TCP-сервер на `serveLine`.
  * `LineRequest::send()` — асинхронный запрос «строка → строка» с таймаутом; удаляет себя по завершении.

### agent (служба)
* `main.cpp` — обёртка Windows-службы на WinAPI (`StartServiceCtrlDispatcher`, обработчик STOP/SHUTDOWN). Если процесс запущен не службой или с `--console` — работает как консольное приложение.
* `AgentCore` — один объект, одна нить (всё на цикле событий Qt):
  * `LineServer` на порту 5600 — приём сообщений от отправителя;
  * `QLocalServer` (named pipe `\\.\pipe\LanMessengerUI`, `WorldAccessOption`) — канал для UI-хелпера;
  * таймер 20 с — повторная отправка неподтверждённых отметок «прочитано».
* `MessageStore` — файл `%ProgramData%\LanMessenger\messages.json`, атомарная запись (`QSaveFile`).
* Ключ: реестр `HKLM\SOFTWARE\LanMessenger\SharedKey`; при пустом значении — переменная окружения `LM_SHARED_KEY` (только для отладки).

### agentui
Скрытое Qt Widgets приложение. Один экземпляр на сессию (`QLockFile` во временной папке пользователя). Таймер 3 с: запрос `get` к службе; для каждого непрочитанного сообщения показывает `MessageDialog` (модальный, поверх окон, без кнопки закрытия, `closeEvent` и `reject` игнорируются, `QTextEdit` в режиме `readOnly`). После нажатия «Прочитано» отправляет `read` и переходит к следующему. При завершении сеанса Windows (`commitDataRequest`) закрытие разрешается, но сообщение остаётся непрочитанным.

### sendercore
* `iprange.*` — разбор диапазонов (адрес, `a-b`, `a-N`, CIDR), лимит 65 536 адресов.
* `RowModel` — `QAbstractTableModel`: колонки для `QTableView`, именованные роли для QML.
* `SenderEngine` — публичное API для обоих интерфейсов:
  * `send(range, title, text)` — проверка, создание строк, рассылка (до 100 одновременных соединений, таймаут 3 с);
  * приём отметок на порту 5601 (`LineServer`), сопоставление со строкой по (id сообщения, IP отправителя отметки), запасной вариант — по имени компьютера;
  * свойства `key`, `range`, `summary`, `totalCount`, `deliveredCount`, `readCount`, `failedCount`, `busy`, `listenError`, `model`;
  * хранение в `AppDataLocation/sender.json`.
  * `model` отдаётся через `QSortFilterProxyModel`, новые отправки сверху.

## Протокол

Транспорт — TCP. Один запрос на соединение: клиент отправляет **одну строку JSON, заканчивающуюся `\n`**, сервер отвечает одной строкой и закрывает соединение.

### Пакет (`Packet`)

| Поле JSON | Тип | Описание |
|---|---|---|
| `type` | string | `message`, `read`, `ack` |
| `id` | string (UUID) | идентификатор сообщения |
| `title`, `text` | string | заголовок, текст (≤ 20 000 символов) |
| `sent` | int | время отправки, мс с эпохи UTC |
| `read` | int | время прочтения, мс с эпохи UTC (0 — нет) |
| `replyPort` | int | порт отправителя для отметок (5601) |
| `machine`, `user` | string | имя компьютера и пользователя |
| `ok` | bool | результат (в `ack`) |
| `error` | string | причина отказа |
| `sig` | string | подпись, Base64 |

### Подпись

`sig = Base64(HMAC-SHA256(key, canonical))`, где

```
canonical = type ␟ id ␟ title ␟ text ␟ sent ␟ read ␟ replyPort ␟ machine ␟ user ␟ ok(0|1)
```

разделитель `␟` — символ U+001F, UTF-8. Проверка сравнивает подписи за постоянное время.

### Обмен

| Шаг | Откуда → куда | Пакет |
|---|---|---|
| 1 | отправитель → агент:5600 | `message` (подписан) |
| 2 | агент → отправитель | `ack`, `ok=true`, `machine` (подписан) — **«доставлено»** |
| 3 | агент → отправитель:5601 | `read` с `id`, `read`, `machine`, `user` (подписан); адрес берётся из соединения, по которому пришло сообщение |
| 4 | отправитель → агент | `ack`, `ok=true` (подписан) — агент перестаёт повторять |

Повторная отправка того же `id` подтверждается, но не дублируется в хранилище.

### Канал агент ↔ UI (named pipe)

Тот же формат строк, без подписи (локальный канал):

| Запрос | Ответ |
|---|---|
| `{"cmd":"get"}` | `{"ok":true,"messages":[{"id","title","text","sent"}, …]}` — непрочитанные |
| `{"cmd":"read","id":"…","user":"имя"}` | `{"ok":true}`, после чего служба немедленно пытается отправить отметку |

## Установщик

`installer/Product.wxs` (WiX 3.x): компонент службы (`ServiceInstall` + `ServiceControl`, LocalSystem, автозапуск), правило брандмауэра (`FirewallException`, TCP 5600), реестр (ключ и `Run`), `MajorUpgrade`. Файлы Qt добавляются через `heat`. Публичное свойство `SHAREDKEY` (`Secure`, `Hidden`) задаётся из `-SharedKey` и может переопределяться в командной строке или MST.

`UpgradeCode` фиксирован: `{7B1D3C52-4E7A-4B7E-9C21-5A6F0E8D2A11}` — не меняйте, иначе обновления перестанут заменять старую версию.

## Идеи для развития

* Шифрование канала (TLS с общим ключом или сертификатами).
* Очередь повторной отправки недоставленных на стороне отправителя.
* Файловое вложение и форматирование.
* Центральный сервер-ретранслятор для мобильного отправителя: отметки «прочитано» доходили бы, даже если телефон спит или сменил сеть.
* Контроль источника: белый список IP отправителей в агенте.
