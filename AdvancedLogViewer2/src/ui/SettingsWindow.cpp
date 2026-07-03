#include "SettingsWindow.h"
#include "ProfileManager.h"
#include "Settings.h"

#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSerialPortInfo>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

SettingsWindow::SettingsWindow(ProfileManager *pm, Settings *settings,
                               QWidget *parent)
    : QDialog(parent)
    , m_profileMgr(pm)
    , m_settings(settings)
{
    setWindowTitle(tr("Параметры"));
    // Профиль-вкладка обёрнута в скролл (ниже), поэтому окно можно свободно
    // уменьшать без наложения виджетов и ресайза за любой край.
    setMinimumSize(560, 360);
    resize(820, 760);
    setSizeGripEnabled(true); // уголок для надёжного изменения размера

    auto *tabs = new QTabWidget(this);
    auto *profileScroll = new QScrollArea;
    profileScroll->setWidgetResizable(true);
    profileScroll->setFrameShape(QFrame::NoFrame);
    profileScroll->setWidget(createProfileTab());
    tabs->addTab(profileScroll, tr("Профиль"));
    tabs->addTab(createSettingsTab(), tr("Настройки"));
    tabs->addTab(createAboutTab(), tr("О программе"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabs);

    refreshProfileList();
    loadSettings();

    // Автовыбор профиля при открытии, иначе действия молча ничего не делают.
    if (m_profileList->count() > 0) {
        QString want = m_profileMgr->activeProfileName();
        if (want.isEmpty()) {
            want = m_settings->lastProfile();
        }
        const auto names = m_profileMgr->profileNames();
        int row = names.indexOf(want);
        m_profileList->setCurrentRow(row >= 0 ? row : 0);
    }
}

// =============================================================================
// Вкладка «Профиль»
// =============================================================================

QWidget *SettingsWindow::createProfileTab()
{
    auto *page = new QWidget;
    auto *hLayout = new QHBoxLayout(page);

    // ======================= Левая колонка: профили =======================
    auto *leftPanel = new QVBoxLayout;
    leftPanel->addWidget(new QLabel(tr("Профили")));
    m_profileList = new QListWidget;
    m_profileList->setObjectName(QStringLiteral("profileList"));
    m_profileList->setMinimumWidth(160);
    connect(m_profileList, &QListWidget::currentRowChanged,
            this, &SettingsWindow::onProfileSelected);
    leftPanel->addWidget(m_profileList, 1);

    auto *btnCreate = new QPushButton(tr("Создать"));
    connect(btnCreate, &QPushButton::clicked, this, &SettingsWindow::onCreateProfile);
    auto *btnDup = new QPushButton(tr("Дублировать"));
    connect(btnDup, &QPushButton::clicked, this, &SettingsWindow::onDuplicateProfile);
    auto *btnRen = new QPushButton(tr("Переименовать"));
    connect(btnRen, &QPushButton::clicked, this, &SettingsWindow::onRenameProfile);
    auto *btnDel = new QPushButton(tr("Удалить"));
    connect(btnDel, &QPushButton::clicked, this, &SettingsWindow::onDeleteProfile);
    for (auto *b : {btnCreate, btnDup, btnRen, btnDel}) {
        leftPanel->addWidget(b);
    }
    hLayout->addLayout(leftPanel, 0);

    // ======================= Правая колонка =======================
    auto *rightPanel = new QVBoxLayout;

    // ------------------------- Соединение -------------------------
    auto *connGroup = new QGroupBox(tr("Соединение"));
    auto *connForm = new QFormLayout(connGroup);
    connForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_connType = new QComboBox;
    m_connType->addItems({QStringLiteral("uart"), QStringLiteral("udp")});
    connForm->addRow(tr("Тип:"), m_connType);

    // Порт + кнопка обновления
    m_primaryPort = new QComboBox;
    m_primaryPort->setObjectName(QStringLiteral("primaryPort"));
    m_primaryPort->setEditable(true);
    m_primaryPort->setInsertPolicy(QComboBox::NoInsert);
    m_primaryPort->setToolTip(
        tr("Выберите доступный порт или введите вручную "
           "(для UDP — номер порта)"));
    auto *portRefresh = new QToolButton;
    portRefresh->setText(QString::fromUtf8("↻")); // ↻
    portRefresh->setToolTip(tr("Обновить список портов"));
    connect(portRefresh, &QToolButton::clicked, this,
            &SettingsWindow::refreshSerialPorts);
    auto *portBox = new QHBoxLayout;
    portBox->setContentsMargins(0, 0, 0, 0);
    portBox->addWidget(m_primaryPort, 1);
    portBox->addWidget(portRefresh);
    connForm->addRow(tr("Порт:"), portBox);
    refreshSerialPorts();
    // Список COM-портов относится только к UART; для UDP поле — номер порта.
    connect(m_connType, &QComboBox::currentTextChanged, this,
            [this](const QString &type) {
                if (type == QLatin1String("uart")) {
                    refreshSerialPorts();
                }
            });

    // Фоллбек-порты
    m_fallbackList = new QListWidget;
    m_fallbackList->setMaximumHeight(56);
    m_fallbackList->setToolTip(
        tr("Резервные порты: перебираются, если основной молчит 5 с"));
    auto *fbAdd = new QToolButton;
    fbAdd->setText(QStringLiteral("+"));
    fbAdd->setToolTip(tr("Добавить фоллбек-порт"));
    connect(fbAdd, &QToolButton::clicked, this, [this]() {
        bool ok = false;
        QString port = QInputDialog::getText(this, tr("Фоллбек-порт"),
                                             tr("Порт:"), QLineEdit::Normal,
                                             QString(), &ok);
        if (ok && !port.isEmpty()) {
            m_fallbackList->addItem(port);
        }
    });
    auto *fbRem = new QToolButton;
    fbRem->setText(QString::fromUtf8("−")); // −
    fbRem->setToolTip(tr("Убрать выбранный фоллбек-порт"));
    connect(fbRem, &QToolButton::clicked, this, [this]() {
        delete m_fallbackList->takeItem(m_fallbackList->currentRow());
    });
    auto *fbBtns = new QVBoxLayout;
    fbBtns->setContentsMargins(0, 0, 0, 0);
    fbBtns->addWidget(fbAdd);
    fbBtns->addWidget(fbRem);
    fbBtns->addStretch();
    auto *fbBox = new QHBoxLayout;
    fbBox->setContentsMargins(0, 0, 0, 0);
    fbBox->addWidget(m_fallbackList, 1);
    fbBox->addLayout(fbBtns);
    connForm->addRow(tr("Фоллбек:"), fbBox);

    // Бодрейт (включая высокоскоростные, отсутствующие в стандартном списке)
    m_baudrate = new QComboBox;
    m_baudrate->setObjectName(QStringLiteral("baudrate"));
    m_baudrate->setEditable(true);
    QList<int> bauds;
    for (int br : QSerialPortInfo::standardBaudRates()) {
        bauds.append(br);
    }
    for (int extra : {230400, 460800, 500000, 921600, 1000000, 1500000,
                      2000000, 3000000}) {
        if (!bauds.contains(extra)) {
            bauds.append(extra);
        }
    }
    std::sort(bauds.begin(), bauds.end());
    for (int br : bauds) {
        m_baudrate->addItem(QString::number(br));
    }
    connForm->addRow(tr("Бодрейт:"), m_baudrate);

    m_encoding = new QComboBox;
    m_encoding->addItems({QStringLiteral("utf8"), QStringLiteral("cp1251"),
                          QStringLiteral("cp866")});
    connForm->addRow(tr("Кодировка:"), m_encoding);

    rightPanel->addWidget(connGroup);

    // ------------------------- Окна -------------------------
    auto *winGroup = new QGroupBox(tr("Окна (отфильтрованные виды)"));
    auto *winLayout = new QVBoxLayout(winGroup);
    auto *winHint = new QLabel(
        tr("Каждое окно — отдельный вид логов со своим фильтром. "
           "Фильтр: ^E — по уровню (начало строки), *текст* — маска, "
           "a|b — ИЛИ, пусто — все строки. Двойной клик по цвету — палитра."));
    winHint->setWordWrap(true);
    winHint->setEnabled(false); // приглушённый пояснительный текст
    winLayout->addWidget(winHint);

    m_windowTable = new QTableWidget(0, 5);
    m_windowTable->setObjectName(QStringLiteral("windowTable"));
    m_windowTable->setHorizontalHeaderLabels(
        {tr("ID"), tr("Заголовок"), tr("Фильтр"), tr("Текст"), tr("Панель")});
    auto *hh = m_windowTable->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::Stretch);
    hh->setSectionResizeMode(2, QHeaderView::Stretch);
    hh->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_windowTable->verticalHeader()->setVisible(false);
    m_windowTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_windowTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_windowTable->setMinimumHeight(120);
    // Двойной клик по ячейке цвета открывает палитру.
    connect(m_windowTable, &QTableWidget::cellDoubleClicked, this,
            &SettingsWindow::pickWindowColor);
    winLayout->addWidget(m_windowTable);

    auto *winBtns = new QHBoxLayout;
    auto *winAdd = new QPushButton(tr("Добавить окно"));
    connect(winAdd, &QPushButton::clicked, this, &SettingsWindow::onAddWindow);
    winBtns->addWidget(winAdd);
    auto *winRem = new QPushButton(tr("Удалить"));
    connect(winRem, &QPushButton::clicked, this, &SettingsWindow::onRemoveWindow);
    winBtns->addWidget(winRem);
    winBtns->addStretch();
    auto *winOpen = new QPushButton(tr("Открыть выбранное"));
    winOpen->setObjectName(QStringLiteral("btnOpenWindow"));
    winOpen->setToolTip(
        tr("Открыть выделенное окно немедленно, не закрывая параметры"));
    connect(winOpen, &QPushButton::clicked, this,
            &SettingsWindow::onOpenSelectedWindow);
    winBtns->addWidget(winOpen);
    winLayout->addLayout(winBtns);

    rightPanel->addWidget(winGroup);
    rightPanel->addStretch(); // прижать кнопки действий книзу, таблицу — компактно

    // ------------------------- Нижняя панель действий -------------------------
    auto *actionRow = new QHBoxLayout;
    auto *btnRememberLayout = new QPushButton(tr("Запомнить расположение"));
    btnRememberLayout->setToolTip(
        tr("Сохранить текущее положение открытых окон в профиль"));
    connect(btnRememberLayout, &QPushButton::clicked, this, [this]() {
        emit saveLayoutRequested();
        refreshProfileList();
    });
    actionRow->addWidget(btnRememberLayout);
    actionRow->addStretch();

    auto *btnSave = new QPushButton(tr("Сохранить"));
    btnSave->setObjectName(QStringLiteral("btnSaveProfile"));
    btnSave->setToolTip(tr("Сохранить профиль, не открывая окна"));
    connect(btnSave, &QPushButton::clicked, this, &SettingsWindow::onSaveProfile);
    actionRow->addWidget(btnSave);

    auto *btnRun = new QPushButton(tr("Запустить"));
    btnRun->setObjectName(QStringLiteral("btnRun"));
    btnRun->setDefault(true);
    btnRun->setToolTip(
        tr("Сохранить, применить профиль и открыть все его окна"));
    connect(btnRun, &QPushButton::clicked, this, &SettingsWindow::onApplyProfile);
    actionRow->addWidget(btnRun);

    auto *btnClose = new QPushButton(tr("Закрыть"));
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    actionRow->addWidget(btnClose);

    rightPanel->addLayout(actionRow);

    hLayout->addLayout(rightPanel, 1);
    return page;
}

WindowDef SettingsWindow::windowDefFromRow(int row) const
{
    WindowDef w;
    if (row < 0 || row >= m_windowTable->rowCount()) {
        return w;
    }
    const auto cell = [this, row](int col) -> QString {
        auto *item = m_windowTable->item(row, col);
        return item ? item->text() : QString();
    };
    w.id = cell(0);
    w.title = cell(1);
    w.globalFilter = cell(2);
    w.textColor = QColor(cell(3));
    w.headerColor = QColor(cell(4));
    w.visible = true;
    return w;
}

void SettingsWindow::onOpenSelectedWindow()
{
    if (m_currentProfileName.isEmpty()) {
        QMessageBox::information(this, tr("Открыть окно"),
                                 tr("Сначала выберите профиль слева."));
        return;
    }
    int row = m_windowTable->currentRow();
    if (row < 0) {
        QMessageBox::information(
            this, tr("Открыть окно"),
            tr("Выберите окно в таблице или создайте его кнопкой "
               "«Добавить окно»."));
        return;
    }
    // Зафиксировать состояние редактора в активный профиль, чтобы контроллер
    // поднял конвейер с актуальным соединением.
    Profile p = editorToProfile();
    m_profileMgr->saveProfile(p);
    m_profileMgr->setActiveProfile(m_currentProfileName);
    refreshProfileList();
    emit windowOpenRequested(windowDefFromRow(row));
}

void SettingsWindow::onSaveProfile()
{
    if (m_currentProfileName.isEmpty()) {
        QMessageBox::information(this, tr("Сохранить"),
                                 tr("Сначала выберите профиль слева."));
        return;
    }
    Profile p = editorToProfile();
    m_profileMgr->saveProfile(p);
    m_profileMgr->clearDirty();
    saveSettings();
    m_settings->save();
    refreshProfileList();
}

void SettingsWindow::selectProfileByName(const QString &name)
{
    const auto names = m_profileMgr->profileNames();
    int idx = names.indexOf(name);
    if (idx >= 0) {
        m_profileList->setCurrentRow(idx);
    }
}

void SettingsWindow::refreshProfileList()
{
    m_profileList->clear();
    const auto names = m_profileMgr->profileNames();
    for (const auto &name : names) {
        QString display = name;
        if (name == m_profileMgr->activeProfileName() && m_profileMgr->isDirty()) {
            display += QStringLiteral(" \u25CF"); // ●
        }
        m_profileList->addItem(display);
    }
}

void SettingsWindow::onProfileSelected(int row)
{
    if (row < 0) {
        return;
    }
    const auto names = m_profileMgr->profileNames();
    if (row >= names.size()) {
        return;
    }
    const QString newName = names[row];
    if (newName == m_currentProfileName) {
        return;
    }

    // Автосохранение правок предыдущего профиля — иначе введённые вручную
    // порт/скорость/окна терялись бы молча при переключении.
    flushEditor();

    m_currentProfileName = newName;
    loadProfileToEditor(m_profileMgr->profile(m_currentProfileName));
}

void SettingsWindow::flushEditor()
{
    if (m_currentProfileName.isEmpty()) {
        return;
    }
    if (!m_profileMgr->hasProfile(m_currentProfileName)) {
        return; // профиль удалён — сохранять некуда
    }
    m_profileMgr->saveProfile(editorToProfile());
}

void SettingsWindow::loadProfileToEditor(const Profile &profile)
{
    m_connType->setCurrentText(profile.connection.type);
    // Порт: если совпадает с доступным — выбрать пункт (покажет подпись с
    // описанием), иначе показать как введённый вручную текст.
    {
        int pidx = m_primaryPort->findData(profile.connection.primaryPort);
        if (pidx >= 0) {
            m_primaryPort->setCurrentIndex(pidx);
        } else {
            m_primaryPort->setCurrentText(profile.connection.primaryPort);
        }
    }
    m_fallbackList->clear();
    m_fallbackList->addItems(profile.connection.fallbackPorts);
    const int baud = profile.connection.baudrate > 0
                         ? profile.connection.baudrate
                         : 115200;
    m_baudrate->setCurrentText(QString::number(baud));
    m_encoding->setCurrentText(profile.connection.encoding);
    refreshWindowTable(profile.windows);
}

Profile SettingsWindow::editorToProfile() const
{
    Profile p;
    p.name = m_currentProfileName;

    p.connection.type = m_connType->currentText();
    p.connection.primaryPort = currentPortName();
    p.connection.fallbackPorts.clear();
    for (int i = 0; i < m_fallbackList->count(); ++i) {
        p.connection.fallbackPorts.append(m_fallbackList->item(i)->text());
    }
    p.connection.baudrate = m_baudrate->currentText().toInt();
    p.connection.encoding = m_encoding->currentText();

    for (int row = 0; row < m_windowTable->rowCount(); ++row) {
        p.windows.append(windowDefFromRow(row));
    }

    return p;
}

static QTableWidgetItem *makeColorItem(const QColor &color)
{
    auto *item = new QTableWidgetItem(color.name());
    item->setBackground(color);
    // Контрастный текст поверх плашки цвета.
    item->setForeground(color.lightnessF() > 0.5 ? Qt::black : Qt::white);
    item->setToolTip(SettingsWindow::tr("Двойной клик — выбрать цвет"));
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

void SettingsWindow::refreshWindowTable(const QVector<WindowDef> &windows)
{
    m_windowTable->setRowCount(static_cast<int>(windows.size()));
    for (int i = 0; i < windows.size(); ++i) {
        const auto &w = windows[i];
        m_windowTable->setItem(i, 0, new QTableWidgetItem(w.id));
        m_windowTable->setItem(i, 1, new QTableWidgetItem(w.title));
        m_windowTable->setItem(i, 2, new QTableWidgetItem(w.globalFilter));
        m_windowTable->setItem(i, 3, makeColorItem(w.textColor));
        m_windowTable->setItem(i, 4, makeColorItem(w.headerColor));
    }
}

QString SettingsWindow::currentPortName() const
{
    int idx = m_primaryPort->currentIndex();
    // Если выбран пункт из списка (текст совпадает с подписью пункта) — берём
    // реальное имя порта из data: в подписи есть описание, слать его нельзя.
    if (idx >= 0 && m_primaryPort->currentText() == m_primaryPort->itemText(idx)) {
        const QString data = m_primaryPort->itemData(idx).toString();
        if (!data.isEmpty()) {
            return data;
        }
    }
    return m_primaryPort->currentText().trimmed();
}

void SettingsWindow::refreshSerialPorts()
{
    const QString current = currentPortName(); // реальное имя, не подпись
    m_primaryPort->clear();
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        QString label = info.portName();
        if (!info.description().isEmpty()) {
            label += QStringLiteral(" — ") + info.description();
        }
        // Данные — только имя порта; подпись с описанием для наглядности.
        m_primaryPort->addItem(label, info.portName());
    }
    if (!current.isEmpty()) {
        // Сохранить ранее выбранное значение (в т.ч. введённое вручную).
        int idx = m_primaryPort->findData(current);
        if (idx >= 0) {
            m_primaryPort->setCurrentIndex(idx);
        } else {
            m_primaryPort->setCurrentText(current);
        }
    }
}

void SettingsWindow::pickWindowColor(int row, int column)
{
    if (column != 3 && column != 4) {
        return;
    }
    auto *item = m_windowTable->item(row, column);
    if (!item) {
        return;
    }
    QColor initial(item->text());
    QColor chosen = QColorDialog::getColor(initial, this, tr("Выбор цвета"));
    if (chosen.isValid()) {
        m_windowTable->setItem(row, column, makeColorItem(chosen));
    }
}

void SettingsWindow::onAddWindow()
{
    int row = m_windowTable->rowCount();
    m_windowTable->insertRow(row);
    QString id = QStringLiteral("window_%1").arg(row);
    m_windowTable->setItem(row, 0, new QTableWidgetItem(id));
    m_windowTable->setItem(row, 1, new QTableWidgetItem(tr("Новое окно")));
    m_windowTable->setItem(row, 2, new QTableWidgetItem(QString()));
    m_windowTable->setItem(row, 3, makeColorItem(QColor(QStringLiteral("#d0d0d0"))));
    m_windowTable->setItem(row, 4, makeColorItem(QColor(QStringLiteral("#303030"))));
    m_windowTable->setCurrentCell(row, 1); // выделить новую строку
}

void SettingsWindow::onRemoveWindow()
{
    int row = m_windowTable->currentRow();
    if (row >= 0) {
        m_windowTable->removeRow(row);
    }
}

void SettingsWindow::onCreateProfile()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Новый профиль"),
                                         tr("Имя профиля:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (!ok || name.isEmpty()) {
        return;
    }

    Profile p;
    p.name = name;
    // Новый профиль наследует текущее соединение из редактора, чтобы уже
    // выставленные порт/скорость не исчезали при создании (окна — с нуля).
    if (!m_currentProfileName.isEmpty()) {
        p.connection = editorToProfile().connection;
    }
    m_profileMgr->saveProfile(p);
    refreshProfileList();
    selectProfileByName(name);
}

void SettingsWindow::onDuplicateProfile()
{
    if (m_currentProfileName.isEmpty()) {
        QMessageBox::information(this, tr("Дублировать профиль"),
                                 tr("Сначала выберите профиль слева."));
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(
        this, tr("Дублировать профиль"), tr("Имя нового профиля:"),
        QLineEdit::Normal, m_currentProfileName + QStringLiteral(" (копия)"), &ok);
    if (!ok || name.isEmpty()) {
        return;
    }

    Profile p = editorToProfile();
    p.name = name;
    m_profileMgr->saveProfile(p);
    refreshProfileList();
    selectProfileByName(name);
}

void SettingsWindow::onDeleteProfile()
{
    if (m_currentProfileName.isEmpty()) {
        return;
    }

    auto res = QMessageBox::question(
        this, tr("Удалить профиль"),
        tr("Удалить профиль «%1»?").arg(m_currentProfileName));
    if (res != QMessageBox::Yes) {
        return;
    }

    m_profileMgr->deleteProfile(m_currentProfileName);
    m_currentProfileName.clear();
    refreshProfileList();
    // Выбрать другой профиль, чтобы редактор не остался «висеть» без выбора.
    if (m_profileList->count() > 0) {
        m_profileList->setCurrentRow(0);
    } else {
        m_windowTable->setRowCount(0);
    }
}

void SettingsWindow::onRenameProfile()
{
    if (m_currentProfileName.isEmpty()) {
        QMessageBox::information(this, tr("Переименовать профиль"),
                                 tr("Сначала выберите профиль слева."));
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(
        this, tr("Переименовать профиль"), tr("Новое имя:"),
        QLineEdit::Normal, m_currentProfileName, &ok);
    if (!ok || name.isEmpty() || name == m_currentProfileName) {
        return;
    }

    m_profileMgr->renameProfile(m_currentProfileName, name);
    m_currentProfileName = name;
    refreshProfileList();
    selectProfileByName(name);
}

void SettingsWindow::onApplyProfile()
{
    if (m_currentProfileName.isEmpty()) {
        QMessageBox::information(this, tr("Запустить"),
                                 tr("Сначала выберите профиль слева."));
        return;
    }
    if (m_windowTable->rowCount() == 0) {
        QMessageBox::information(
            this, tr("Запустить"),
            tr("В профиле нет окон. Добавьте хотя бы одно окно кнопкой "
               "«Добавить окно»."));
        return;
    }

    // Сохранить текущее состояние редактора в профиль
    Profile p = editorToProfile();
    m_profileMgr->saveProfile(p);
    m_profileMgr->setActiveProfile(m_currentProfileName);

    saveSettings();
    m_settings->setLastProfile(m_currentProfileName);
    m_settings->save();

    emit profileApplied(m_currentProfileName);
    accept();
}

// =============================================================================
// Вкладка «Настройки»
// =============================================================================

QWidget *SettingsWindow::createSettingsTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    // Язык
    auto *langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(tr("Язык:")));
    m_langCombo = new QComboBox;
    m_langCombo->addItem(tr("Русский"), QStringLiteral("ru_RU"));
    m_langCombo->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    langRow->addWidget(m_langCombo);
    langRow->addStretch();
    layout->addLayout(langRow);

    // Тема
    auto *themeRow = new QHBoxLayout;
    themeRow->addWidget(new QLabel(tr("Тема:")));
    m_themeCombo = new QComboBox;
    m_themeCombo->addItem(tr("Системная"), QStringLiteral("system"));
    m_themeCombo->addItem(tr("Тёмная"), QStringLiteral("dark"));
    m_themeCombo->addItem(tr("Светлая"), QStringLiteral("light"));
    themeRow->addWidget(m_themeCombo);
    themeRow->addStretch();
    layout->addLayout(themeRow);

    // Путь к логам
    auto *logDirRow = new QHBoxLayout;
    logDirRow->addWidget(new QLabel(tr("Путь к логам:")));
    m_logDirEdit = new QLineEdit;
    logDirRow->addWidget(m_logDirEdit);
    auto *browseBtn = new QPushButton(QStringLiteral("..."));
    browseBtn->setFixedWidth(32);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Выбрать директорию"), m_logDirEdit->text());
        if (!dir.isEmpty()) {
            m_logDirEdit->setText(dir);
        }
    });
    logDirRow->addWidget(browseBtn);
    layout->addLayout(logDirRow);

    // Макс. строк
    auto *maxRow = new QHBoxLayout;
    maxRow->addWidget(new QLabel(tr("Макс. строк:")));
    m_maxLinesSpin = new QSpinBox;
    m_maxLinesSpin->setRange(100000, 10000000);
    m_maxLinesSpin->setSingleStep(100000);
    maxRow->addWidget(m_maxLinesSpin);
    maxRow->addStretch();
    layout->addLayout(maxRow);

    // Шрифт
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(new QLabel(tr("Шрифт:")));
    m_fontLabel = new QLabel;
    fontRow->addWidget(m_fontLabel);
    auto *fontBtn = new QPushButton(tr("Выбрать"));
    connect(fontBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QFont font = QFontDialog::getFont(&ok, m_selectedFont, this);
        if (ok) {
            m_selectedFont = font;
            m_fontLabel->setText(QStringLiteral("%1, %2pt")
                                    .arg(font.family())
                                    .arg(font.pointSize()));
        }
    });
    fontRow->addWidget(fontBtn);
    fontRow->addStretch();
    layout->addLayout(fontRow);

    // Кнопка «Сохранить»
    auto *saveRow = new QHBoxLayout;
    saveRow->addStretch();
    auto *saveBtn = new QPushButton(tr("Сохранить"));
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        m_settings->save();
    });
    saveRow->addWidget(saveBtn);
    layout->addLayout(saveRow);

    layout->addStretch();
    return page;
}

void SettingsWindow::loadSettings()
{
    int langIdx = m_langCombo->findData(m_settings->language());
    if (langIdx >= 0) {
        m_langCombo->setCurrentIndex(langIdx);
    }

    int themeIdx = m_themeCombo->findData(m_settings->theme());
    if (themeIdx >= 0) {
        m_themeCombo->setCurrentIndex(themeIdx);
    }

    m_logDirEdit->setText(m_settings->logDir());
    m_maxLinesSpin->setValue(m_settings->maxLines());

    m_selectedFont = m_settings->logFont();
    m_fontLabel->setText(QStringLiteral("%1, %2pt")
                             .arg(m_selectedFont.family())
                             .arg(m_selectedFont.pointSize()));
}

void SettingsWindow::saveSettings()
{
    m_settings->setLanguage(m_langCombo->currentData().toString());
    m_settings->setTheme(m_themeCombo->currentData().toString());
    m_settings->setLogDir(m_logDirEdit->text());
    m_settings->setMaxLines(m_maxLinesSpin->value());
    m_settings->setLogFont(m_selectedFont);
}

// =============================================================================
// Вкладка «О программе»
// =============================================================================

QWidget *SettingsWindow::createAboutTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addStretch();

    auto *title = new QLabel(QStringLiteral("<h2>Advanced Log Viewer 2</h2>"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *version = new QLabel(
        tr("Версия %1").arg(QCoreApplication::applicationVersion()));
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);

    auto *license = new QLabel(tr("Внутренний инструмент. Все права защищены."));
    license->setAlignment(Qt::AlignCenter);
    layout->addWidget(license);

    layout->addStretch();
    return page;
}
