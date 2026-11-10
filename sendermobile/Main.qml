import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    visible: true
    width: 400
    height: 820
    title: "LAN Messenger"

    Material.theme: Material.System
    Material.accent: "#2F6FED"
    Material.primary: "#2F6FED"

    // Палитра статусов
    readonly property color okColor: "#1E9E5A"
    readonly property color warnColor: "#E08A00"
    readonly property color errColor: "#D6403A"
    readonly property color mutedColor: Material.theme === Material.Dark ? "#9AA0A6" : "#6B7280"
    readonly property color cardColor: Material.theme === Material.Dark ? "#23262B" : "#FFFFFF"
    readonly property color pageColor: Material.theme === Material.Dark ? "#17191C" : "#F2F4F8"
    readonly property color lineColor: Material.theme === Material.Dark ? "#34383E" : "#E3E7EE"

    color: pageColor

    function toast(text) {
        toastLabel.text = text
        toastBox.opacity = 1
        toastTimer.restart()
    }

    // ---------- Шапка ----------
    header: Rectangle {
        height: 64
        color: Material.primary
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12
            Rectangle {
                width: 36; height: 36; radius: 10
                color: "#33FFFFFF"
                Label { anchors.centerIn: parent; text: "✉"; color: "white"; font.pixelSize: 20 }
            }
            ColumnLayout {
                spacing: 0
                Label { text: "LAN Messenger"; color: "white"; font.pixelSize: 18; font.bold: true }
                Label {
                    text: sender.busy ? "Идёт отправка…" : "Рассылка сообщений по сети"
                    color: "#CCFFFFFF"; font.pixelSize: 12
                }
            }
            Item { Layout.fillWidth: true }
            BusyIndicator {
                running: sender.busy
                visible: running
                Layout.preferredWidth: 32; Layout.preferredHeight: 32
                Material.accent: "white"
            }
        }
    }

    // ---------- Карточка ----------
    component Card: Rectangle {
        default property alias content: inner.data
        property string heading: ""
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 32
        radius: 16
        color: win.cardColor
        border.color: win.lineColor
        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            Label {
                visible: heading !== ""
                text: heading
                font.pixelSize: 13; font.bold: true
                color: win.mutedColor
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.8
            }
        }
    }

    // ---------- Чип статуса ----------
    component Chip: Rectangle {
        property alias text: chipLabel.text
        property color tone: win.mutedColor
        implicitWidth: chipLabel.implicitWidth + 20
        implicitHeight: 24
        radius: 12
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
        Label { id: chipLabel; anchors.centerIn: parent; color: parent.tone; font.pixelSize: 12; font.bold: true }
    }

    // ---------- Плитка статистики ----------
    component Tile: Rectangle {
        property string caption
        property int value: 0
        property color tone: win.mutedColor
        Layout.fillWidth: true
        implicitHeight: 72
        radius: 14
        color: win.cardColor
        border.color: win.lineColor
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 0
            Label { text: value; color: tone; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
            Label { text: caption; color: win.mutedColor; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: nav.currentIndex

        // ================= Отправка =================
        Flickable {
            contentHeight: sendCol.implicitHeight + 32
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: sendCol
                width: parent.width - 32
                x: 16; y: 16
                spacing: 14

                Card {
                    heading: "Кому"
                    TextField {
                        id: rangeField
                        Layout.fillWidth: true
                        text: sender.range
                        placeholderText: "192.168.1.1-254"
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: win.mutedColor
                        font.pixelSize: 12
                        text: "Один адрес, диапазон (192.168.1.10-50 или 192.168.1.10-192.168.1.50) или подсеть (192.168.1.0/24). Можно несколько через запятую."
                    }
                }

                Card {
                    heading: "Сообщение"
                    TextField {
                        id: titleField
                        Layout.fillWidth: true
                        placeholderText: "Заголовок (необязательно)"
                        maximumLength: 120
                    }
                    TextArea {
                        id: bodyField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 170
                        wrapMode: TextEdit.Wrap
                        placeholderText: "Текст сообщения"
                    }
                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: bodyField.length + " / 20000"
                        color: bodyField.length > 20000 ? win.errColor : win.mutedColor
                        font.pixelSize: 12
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    highlighted: true
                    enabled: !sender.busy
                    text: sender.busy ? "Отправка…" : "Отправить"
                    font.pixelSize: 16
                    font.bold: true
                    onClicked: {
                        sender.range = rangeField.text
                        var err = sender.send(rangeField.text, titleField.text, bodyField.text)
                        if (err !== "") {
                            errorDialog.text = err
                            errorDialog.open()
                        } else {
                            bodyField.clear()
                            titleField.clear()
                            nav.currentIndex = 1
                        }
                    }
                }

                Label {
                    visible: sender.key === ""
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: win.warnColor
                    text: "⚠ Не задан общий ключ — укажите его на вкладке «Настройки»."
                }
                Item { Layout.preferredHeight: 8 }
            }
        }

        // ================= Статусы =================
        ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 8
                spacing: 8
                Tile { caption: "Всего"; value: sender.totalCount; tone: Material.accent }
                Tile { caption: "Доставлено"; value: sender.deliveredCount; tone: win.okColor }
                Tile { caption: "Прочитано"; value: sender.readCount; tone: win.okColor }
                Tile { caption: "Ошибки"; value: sender.failedCount; tone: win.errColor }
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                leftMargin: 16; rightMargin: 16; topMargin: 4; bottomMargin: 16
                model: sender.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    width: list.width - 32
                    height: rowCol.implicitHeight + 24
                    radius: 14
                    color: win.cardColor
                    border.color: win.lineColor

                    Rectangle {   // цветная полоска слева: итоговый статус
                        width: 5; radius: 3
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 10 }
                        color: delivery === 2 ? win.errColor
                             : delivery === 0 ? win.mutedColor
                             : isRead ? win.okColor : win.warnColor
                    }

                    ColumnLayout {
                        id: rowCol
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12; leftMargin: 24 }
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: ip; font.pixelSize: 16; font.bold: true }
                            Label {
                                text: machine
                                color: win.mutedColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label { text: sent; color: win.mutedColor; font.pixelSize: 11 }
                        }
                        Label {
                            text: title
                            visible: title !== ""
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: win.mutedColor
                            font.pixelSize: 13
                        }
                        RowLayout {
                            spacing: 8
                            Chip {
                                text: deliveryText
                                tone: delivery === 1 ? win.okColor : delivery === 2 ? win.errColor : win.mutedColor
                            }
                            Chip {
                                text: readText
                                tone: isRead ? win.okColor : (delivery === 1 ? win.warnColor : win.mutedColor)
                            }
                            Label {
                                visible: userName !== ""
                                text: "👤 " + userName
                                color: win.mutedColor
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            visible: readAt !== ""
                            text: "Прочитано: " + readAt
                            color: win.okColor
                            font.pixelSize: 12
                        }
                        Label {
                            visible: note !== ""
                            text: note
                            color: win.errColor
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    color: win.mutedColor
                    text: "Пока нет отправок\nПерейдите на вкладку «Отправка»"
                }
            }
        }

        // ================= Настройки =================
        Flickable {
            contentHeight: setCol.implicitHeight + 32
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: setCol
                width: parent.width - 32
                x: 16; y: 16
                spacing: 14

                Card {
                    heading: "Общий ключ"
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: keyField
                            Layout.fillWidth: true
                            text: sender.key
                            echoMode: showKey.checked ? TextInput.Normal : TextInput.Password
                            placeholderText: "Ключ, вшитый в MSI агента"
                            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                            onTextChanged: sender.key = text
                        }
                        CheckBox { id: showKey; text: "👁" }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: win.mutedColor
                        font.pixelSize: 12
                        text: "Ключ должен совпадать с ключом, который указан при сборке MSI (файл dist/sharedkey.txt). Если ключи разные, агенты отклоняют сообщения."
                    }
                }

                Card {
                    heading: "Приём «прочитано»"
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 12; height: 12; radius: 6
                            color: sender.listenError === "" ? win.okColor : win.errColor
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: sender.listenError === ""
                                  ? "Порт 5601 слушается — отметки о прочтении принимаются"
                                  : sender.listenError
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: win.mutedColor
                        font.pixelSize: 12
                        text: "Телефон должен быть в той же сети (Wi-Fi или VPN), откуда доступны компьютеры. Если телефон был недоступен, агенты повторяют отправку отметки каждые 20 секунд — статус обновится, как только приложение откроется."
                    }
                }

                Card {
                    heading: "История"
                    Button {
                        Layout.fillWidth: true
                        text: "Очистить историю отправок"
                        Material.foreground: win.errColor
                        flat: true
                        onClicked: confirmClear.open()
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    color: win.mutedColor
                    font.pixelSize: 12
                    text: "LAN Messenger " + appVersion
                }
            }
        }
    }

    // ---------- Нижняя навигация ----------
    footer: TabBar {
        id: nav
        Material.background: win.cardColor
        TabButton { text: "✉  Отправка" }
        TabButton { text: "📋  Статусы" }
        TabButton { text: "⚙  Настройки" }
    }

    // ---------- Диалоги ----------
    Dialog {
        id: errorDialog
        property alias text: errLabel.text
        title: "Не удалось отправить"
        modal: true
        standardButtons: Dialog.Ok
        anchors.centerIn: parent
        width: parent.width - 48
        Label { id: errLabel; wrapMode: Text.Wrap; width: parent.width }
    }

    Dialog {
        id: confirmClear
        title: "Очистить историю?"
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        anchors.centerIn: parent
        width: parent.width - 48
        Label { text: "Все записи об отправках будут удалены."; wrapMode: Text.Wrap; width: parent.width }
        onAccepted: { sender.clearHistory(); win.toast("История очищена") }
    }

    // ---------- Всплывающее уведомление ----------
    Rectangle {
        id: toastBox
        opacity: 0
        z: 10
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
        radius: 20
        color: "#CC202124"
        width: toastLabel.implicitWidth + 36
        height: 40
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Label { id: toastLabel; anchors.centerIn: parent; color: "white" }
        Timer { id: toastTimer; interval: 2000; onTriggered: toastBox.opacity = 0 }
    }
}
