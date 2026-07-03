#pragma once

#include <QColor>
#include <QJsonObject>
#include <QRect>
#include <QString>
#include <QVector>

struct ConnectionDef {
    QString type;             // "uart" | "udp"
    QString primaryPort;      // e.g. "/dev/ttyACM0" or "5000"
    QStringList fallbackPorts;
    int baudrate = 115200;
    QString encoding = QStringLiteral("utf8"); // utf8 | cp1251 | cp866

    QJsonObject toJson() const;
    static ConnectionDef fromJson(const QJsonObject &obj);
};

struct WindowDef {
    QString id;
    QString title;
    QString globalFilter;
    QColor textColor = QColor(0xD0, 0xD0, 0xD0);
    QColor headerColor = QColor(0x30, 0x30, 0x30);
    bool visible = true;

    QJsonObject toJson() const;
    static WindowDef fromJson(const QJsonObject &obj);
};

struct WindowLayout {
    QString windowId;
    QRect geometry;

    QJsonObject toJson() const;
    static WindowLayout fromJson(const QJsonObject &obj);
};

struct Profile {
    QString name;
    ConnectionDef connection;
    QVector<WindowDef> windows;
    QVector<WindowLayout> layout;

    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject &obj);

    static QString safeFileName(const QString &name);
};
