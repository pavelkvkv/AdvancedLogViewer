#pragma once

#include "Profile.h"

#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

signals:
    void profileApplied(const QString &profileName);
    void windowOpenRequested(const WindowDef &def);
    void saveLayoutRequested();

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
    void refreshSerialPorts();
    void pickWindowColor(int row, int column);

    // --- Вкладка «Настройки» ---
    QWidget *createSettingsTab();
    void loadSettings();
    void saveSettings();

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

    // Профиль: правая панель — окна
    QTableWidget *m_windowTable;

    // Настройки
    QComboBox *m_langCombo;
    QComboBox *m_themeCombo;
    QLineEdit *m_logDirEdit;
    QSpinBox *m_maxLinesSpin;
    QLabel *m_fontLabel;
    QFont m_selectedFont;

    QString m_currentProfileName;
};
