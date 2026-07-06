#include "ProfileManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QStandardPaths>

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
}

void ProfileManager::loadAll()
{
    QMutexLocker locker(&m_mutex);
    m_profiles.clear();

    QDir dir(profilesDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const auto files = dir.entryInfoList(
        {QStringLiteral("*.profile.json")}, QDir::Files);

    for (const auto &fi : files) {
        Profile p = loadProfileFromFile(fi.absoluteFilePath());
        if (!p.name.isEmpty()) {
            m_profiles[p.name] = p;
        }
    }
}

void ProfileManager::saveProfile(const Profile &profile)
{
    QMutexLocker locker(&m_mutex);

    m_profiles[profile.name] = profile;

    QDir dir(profilesDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString filePath = profileFilePath(profile.name);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(profile.toJson());
        file.write(doc.toJson(QJsonDocument::Indented));
    }

    locker.unlock();
    emit profilesChanged();
}

void ProfileManager::deleteProfile(const QString &name)
{
    QMutexLocker locker(&m_mutex);

    m_profiles.remove(name);
    QFile::remove(profileFilePath(name));

    locker.unlock();
    emit profilesChanged();
}

void ProfileManager::renameProfile(const QString &oldName, const QString &newName)
{
    QMutexLocker locker(&m_mutex);

    if (!m_profiles.contains(oldName) || oldName == newName) {
        return;
    }

    Profile p = m_profiles.take(oldName);
    QFile::remove(profileFilePath(oldName));

    p.name = newName;
    m_profiles[newName] = p;

    locker.unlock();
    saveProfile(p);

    if (m_activeProfileName == oldName) {
        m_activeProfileName = newName;
    }
}

QStringList ProfileManager::profileNames() const
{
    QMutexLocker locker(&m_mutex);
    return m_profiles.keys();
}

Profile ProfileManager::profile(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    return m_profiles.value(name);
}

bool ProfileManager::hasProfile(const QString &name) const
{
    QMutexLocker locker(&m_mutex);
    return m_profiles.contains(name);
}

void ProfileManager::setActiveProfile(const QString &name)
{
    m_activeProfileName = name;
    m_dirty = false;
    emit dirtyChanged(false);
}

QString ProfileManager::activeProfileName() const
{
    return m_activeProfileName;
}

Profile ProfileManager::activeProfile() const
{
    QMutexLocker locker(&m_mutex);
    return m_profiles.value(m_activeProfileName);
}

void ProfileManager::markDirty()
{
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged(true);
    }
}

void ProfileManager::clearDirty()
{
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged(false);
    }
}

bool ProfileManager::isDirty() const
{
    return m_dirty;
}

QString ProfileManager::profilesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/AdvancedLogViewer2/profiles");
}

QString ProfileManager::profileFilePath(const QString &name) const
{
    return profilesDir() + QLatin1Char('/') + Profile::safeFileName(name)
           + QStringLiteral(".profile.json");
}

Profile ProfileManager::loadProfileFromFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }

    return Profile::fromJson(doc.object());
}
