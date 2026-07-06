#include "LogFileWriter.h"
#include "LogStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QReadLocker>

LogFileWriter::LogFileWriter(LogStore *store, const QString &globalFilter,
                             const QString &logDir, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_globalFilter(globalFilter)
    , m_logDir(logDir)
{
    QString dt = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString safe = safeFileName(globalFilter);
    if (safe.isEmpty()) {
        safe = QStringLiteral("all");
    }
    m_filePath = QDir(m_logDir).filePath(
        QStringLiteral("%1_%2.log").arg(dt, safe));
}

LogFileWriter::~LogFileWriter()
{
    stop();
}

void LogFileWriter::start()
{
    if (m_running.load()) {
        return;
    }

    m_running.store(true);
    moveToThread(&m_thread);
    m_thread.start();

    QMetaObject::invokeMethod(this, [this]() {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(kPollIntervalMs);
        connect(m_pollTimer, &QTimer::timeout, this, &LogFileWriter::pollAndWrite);
        m_pollTimer->start();
    }, Qt::QueuedConnection);
}

void LogFileWriter::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_pollTimer) {
                m_pollTimer->stop();
                delete m_pollTimer;
                m_pollTimer = nullptr;
            }
            // Финальный сброс
            pollAndWrite();
            if (m_file) {
                m_file->close();
                delete m_file;
                m_file = nullptr;
            }
            // Возврат аффинности выполняем из рабочего потока (иначе Qt
            // печатает "Cannot move to target thread").
            moveToThread(QCoreApplication::instance()->thread());
        }, Qt::BlockingQueuedConnection);

        m_thread.quit();
        m_thread.wait();
    }
}

void LogFileWriter::pollAndWrite()
{
    QReadLocker locker(&m_store->lock());

    size_t storeSize = m_store->lineCountLocked();
    if (storeSize <= m_lastWrittenLine) {
        return;
    }

    for (size_t i = m_lastWrittenLine; i < storeSize; ++i) {
        QString line = m_store->lineLocked(i);
        m_writeBuffer.append(line.toUtf8());
        m_writeBuffer.append('\n');
    }
    m_lastWrittenLine = storeSize;

    locker.unlock();

    if (!m_writeBuffer.isEmpty()) {
        ensureFileOpen();
        if (m_file && m_file->isOpen()) {
            m_file->write(m_writeBuffer);
            m_file->flush();
        }
        m_writeBuffer.clear();
    }
}

QString LogFileWriter::safeFileName(const QString &filter)
{
    QString safe = filter;
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    for (const QChar &ch : forbidden) {
        safe.replace(ch, QLatin1Char('_'));
    }
    safe = safe.trimmed();
    if (safe.length() > 64) {
        safe.truncate(64);
    }
    return safe;
}

void LogFileWriter::ensureFileOpen()
{
    if (m_file && m_file->isOpen()) {
        return;
    }

    QDir dir(m_logDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    m_file = new QFile(m_filePath, this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete m_file;
        m_file = nullptr;
    }
}
