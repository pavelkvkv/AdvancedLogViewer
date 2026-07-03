#include "Profile.h"

#include <QJsonArray>

// --- ConnectionDef ---

QJsonObject ConnectionDef::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("type")] = type;
    obj[QStringLiteral("primary_port")] = primaryPort;
    QJsonArray fb;
    for (const auto &p : fallbackPorts) {
        fb.append(p);
    }
    obj[QStringLiteral("fallback_ports")] = fb;
    obj[QStringLiteral("baudrate")] = baudrate;
    obj[QStringLiteral("encoding")] = encoding;
    return obj;
}

ConnectionDef ConnectionDef::fromJson(const QJsonObject &obj)
{
    ConnectionDef c;
    c.type = obj[QStringLiteral("type")].toString(QStringLiteral("uart"));
    c.primaryPort = obj[QStringLiteral("primary_port")].toString();
    const auto arr = obj[QStringLiteral("fallback_ports")].toArray();
    for (const auto &v : arr) {
        c.fallbackPorts.append(v.toString());
    }
    c.baudrate = obj[QStringLiteral("baudrate")].toInt(115200);
    c.encoding = obj[QStringLiteral("encoding")].toString(QStringLiteral("utf8"));
    return c;
}

// --- WindowDef ---

QJsonObject WindowDef::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("title")] = title;
    obj[QStringLiteral("global_filter")] = globalFilter;
    obj[QStringLiteral("text_color")] = textColor.name();
    obj[QStringLiteral("header_color")] = headerColor.name();
    obj[QStringLiteral("visible")] = visible;
    return obj;
}

WindowDef WindowDef::fromJson(const QJsonObject &obj)
{
    WindowDef w;
    w.id = obj[QStringLiteral("id")].toString();
    w.title = obj[QStringLiteral("title")].toString();
    w.globalFilter = obj[QStringLiteral("global_filter")].toString();
    w.textColor = QColor(obj[QStringLiteral("text_color")].toString(QStringLiteral("#D0D0D0")));
    w.headerColor = QColor(obj[QStringLiteral("header_color")].toString(QStringLiteral("#303030")));
    w.visible = obj[QStringLiteral("visible")].toBool(true);
    return w;
}

// --- WindowLayout ---

QJsonObject WindowLayout::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("window_id")] = windowId;
    obj[QStringLiteral("x")] = geometry.x();
    obj[QStringLiteral("y")] = geometry.y();
    obj[QStringLiteral("w")] = geometry.width();
    obj[QStringLiteral("h")] = geometry.height();
    return obj;
}

WindowLayout WindowLayout::fromJson(const QJsonObject &obj)
{
    WindowLayout l;
    l.windowId = obj[QStringLiteral("window_id")].toString();
    l.geometry = QRect(obj[QStringLiteral("x")].toInt(),
                       obj[QStringLiteral("y")].toInt(),
                       obj[QStringLiteral("w")].toInt(800),
                       obj[QStringLiteral("h")].toInt(600));
    return l;
}

// --- Profile ---

QJsonObject Profile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("connection")] = connection.toJson();

    QJsonArray wArr;
    for (const auto &w : windows) {
        wArr.append(w.toJson());
    }
    obj[QStringLiteral("windows")] = wArr;

    QJsonArray lArr;
    for (const auto &l : layout) {
        lArr.append(l.toJson());
    }
    obj[QStringLiteral("layout")] = lArr;

    return obj;
}

Profile Profile::fromJson(const QJsonObject &obj)
{
    Profile p;
    p.name = obj[QStringLiteral("name")].toString();
    p.connection = ConnectionDef::fromJson(obj[QStringLiteral("connection")].toObject());

    const auto wArr = obj[QStringLiteral("windows")].toArray();
    for (const auto &v : wArr) {
        p.windows.append(WindowDef::fromJson(v.toObject()));
    }

    const auto lArr = obj[QStringLiteral("layout")].toArray();
    for (const auto &v : lArr) {
        p.layout.append(WindowLayout::fromJson(v.toObject()));
    }

    return p;
}

QString Profile::safeFileName(const QString &name)
{
    QString safe = name;
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    for (auto &ch : safe) {
        if (forbidden.contains(ch)) {
            ch = QLatin1Char('_');
        }
    }
    return safe;
}
