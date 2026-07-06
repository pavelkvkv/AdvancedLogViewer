#pragma once

#include "Profile.h"

#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>

class ProfileManager;
class Settings;

class SettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit SettingsWindow(ProfileManager *pm, Settings *settings,
                            QWidget *parent = nullptr);

public slots:
    // Отразить состояние подключения на кнопке в настройках соединения.
    void setConnected(bool connected);

signals:
    void profileApplied(const QString &profileName);
    void windowOpenRequested(const WindowDef &def);
    void saveLayoutRequested();
    // Нажата кнопка «Отключить/Подключить источник» в настройках соединения.
    void connectionToggleRequested();

private:
    // --- Вкладка «Профиль» ---
    QWidget *createProfileTab();
    void refreshProfileList();
    void onProfileSelected(int row);
    void onCreateProfile();
    void onDuplicateProfile();
    void onDeleteProfile();
    void onRenameProfile();
    void onApplyProfile();
    void onSaveProfile();
    void onOpenSelectedWindow();
    void selectProfileByName(const QString &name);

    void loadProfileToEditor(const Profile &profile);
    Profile editorToProfile() const;
    QString currentPortName() const;
    void flushEditor();
    WindowDef windowDefFromRow(int row) const;
    void refreshWindowTable(const QVector<WindowDef> &windows);
    void onAddWindow();
    void onRemoveWindow();
    void onWindowTableItemChanged(QTableWidgetItem *item);
    void refreshSerialPorts();
    void pickWindowColor(int row, int column);

    // --- Вкладка «Настройки» ---
    QWidget *createSettingsTab();
    void loadSettings();
    void saveSettings();
    void updateLogDirStatus();

    // --- Вкладка «О программе» ---
    QWidget *createAboutTab();

    ProfileManager *m_profileMgr;
    Settings *m_settings;

    // Профиль: левая панель
    QListWidget *m_profileList;

    // Профиль: правая панель — соединение
    QComboBox *m_connType;
    QComboBox *m_primaryPort;
    QListWidget *m_fallbackList;
    QComboBox *m_baudrate;
    QComboBox *m_encoding;
    QPushButton *m_connToggleBtn;
    bool m_connected = false;

    // Профиль: правая панель — окна
    QTableWidget *m_windowTable;

    // Настройки
    QComboBox *m_langCombo;
    QComboBox *m_themeCombo;
    QLineEdit *m_logDirEdit;
    QLabel *m_logDirStatus;
    QSpinBox *m_maxLinesSpin;
    QLabel *m_fontLabel;
    QFont m_selectedFont;

    QString m_currentProfileName;
    bool m_updatingTable = false; // подавляет реакцию на программное изменение таблицы

    static constexpr int kColId = 0;
    static constexpr int kColTitle = 1;
    static constexpr int kColFilter = 2;
    static constexpr int kColText = 3;
    static constexpr int kColPanel = 4;
    static constexpr int kColCatchAll = 5;
};
