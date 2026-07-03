#pragma once

#include "Profile.h"

#include <QMutex>
#include <QObject>

class ProfileManager : public QObject {
    Q_OBJECT

public:
    explicit ProfileManager(QObject *parent = nullptr);

    void loadAll();
    void saveProfile(const Profile &profile);
    void deleteProfile(const QString &name);
    void renameProfile(const QString &oldName, const QString &newName);

    QStringList profileNames() const;
    Profile profile(const QString &name) const;
    bool hasProfile(const QString &name) const;

    void setActiveProfile(const QString &name);
    QString activeProfileName() const;
    Profile activeProfile() const;

    void markDirty();
    void clearDirty();
    bool isDirty() const;

    static QString profilesDir();

signals:
    void profilesChanged();
    void dirtyChanged(bool dirty);

private:
    QString profileFilePath(const QString &name) const;
    Profile loadProfileFromFile(const QString &filePath) const;

    mutable QMutex m_mutex;
    QMap<QString, Profile> m_profiles;
    QString m_activeProfileName;
    bool m_dirty = false;
};
