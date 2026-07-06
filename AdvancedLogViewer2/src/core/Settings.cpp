#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

Settings::Settings()
    : m_logFont(QStringLiteral("Monospace"), 10)
{
    m_logFont.setStyleHint(QFont::Monospace);

    m_logDir = QDir::homePath() + QStringLiteral("/AdvancedLogViewer2_logs");
}

void Settings::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    fromJson(doc.object());
}

void Settings::save() const
{
    QString path = settingsFilePath();
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(toJson());
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

QString Settings::settingsFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/AdvancedLogViewer2/settings.json");
}

QJsonObject Settings::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("language")] = m_language;
    obj[QStringLiteral("theme")] = m_theme;
    obj[QStringLiteral("log_dir")] = m_logDir;
    obj[QStringLiteral("max_lines")] = m_maxLines;
    obj[QStringLiteral("font_family")] = m_logFont.family();
    obj[QStringLiteral("font_size")] = m_logFont.pointSize();
    obj[QStringLiteral("last_profile")] = m_lastProfile;
    return obj;
}

void Settings::fromJson(const QJsonObject &obj)
{
    m_language = obj[QStringLiteral("language")].toString(m_language);
    m_theme = obj[QStringLiteral("theme")].toString(m_theme);
    m_logDir = obj[QStringLiteral("log_dir")].toString(m_logDir);
    m_maxLines = obj[QStringLiteral("max_lines")].toInt(m_maxLines);

    QString family = obj[QStringLiteral("font_family")].toString(m_logFont.family());
    int size = obj[QStringLiteral("font_size")].toInt(m_logFont.pointSize());
    m_logFont = QFont(family, size);
    m_logFont.setStyleHint(QFont::Monospace);

    m_lastProfile = obj[QStringLiteral("last_profile")].toString();
}
