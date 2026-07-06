#pragma once

#include <QFont>
#include <QJsonObject>
#include <QString>

class Settings {
public:
    Settings();

    void load();
    void save() const;

    // Язык
    QString language() const { return m_language; }
    void setLanguage(const QString &lang) { m_language = lang; }

    // Тема: "system", "dark", "light"
    QString theme() const { return m_theme; }
    void setTheme(const QString &theme) { m_theme = theme; }

    // Путь к логам
    QString logDir() const { return m_logDir; }
    void setLogDir(const QString &dir) { m_logDir = dir; }

    // Макс. строк в LogStore
    int maxLines() const { return m_maxLines; }
    void setMaxLines(int n) { m_maxLines = n; }

    // Шрифт
    QFont logFont() const { return m_logFont; }
    void setLogFont(const QFont &font) { m_logFont = font; }

    // Последний активный профиль
    QString lastProfile() const { return m_lastProfile; }
    void setLastProfile(const QString &name) { m_lastProfile = name; }

    static QString settingsFilePath();

private:
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

    QString m_language = QStringLiteral("ru_RU");
    QString m_theme = QStringLiteral("system");
    QString m_logDir;
    int m_maxLines = 5000000;
    QFont m_logFont;
    QString m_lastProfile;
};
