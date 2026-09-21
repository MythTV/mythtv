
#include "libmythbase/mythlogging.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include "stdinnotifier.h"
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

StdinNotifier::StdinNotifier(QObject *parent)
    : QObject(parent)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(
                                     fileno(stdin),
                                     QSocketNotifier::Read,
                                     this);

    connect(m_notifier,
            &QSocketNotifier::activated,
            this,
            &StdinNotifier::handleReadyRead);
}

void StdinNotifier::handleReadyRead()
{
    m_notifier->setEnabled(false);

    char buffer[4096];

    for (;;)
    {
        ssize_t count = ::read(STDIN_FILENO, buffer, sizeof(buffer));

        if (count > 0)
        {
            m_buffer.append(buffer, count);
            continue;
        }

        if (count == 0)
        {
            // EOF
            emit finished();
            return;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN)
            break;

        LOG(VB_RECORD, LOG_ERR,
            QString("stdin read failed: %1")
            .arg(QString::fromLocal8Bit(strerror(errno))));

        emit finished();
        return;
    }

    for (;;)
    {
        qsizetype newline = m_buffer.indexOf('\n');

        if (newline < 0)
            break;

        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);

        line = line.trimmed();

        if (line.isEmpty())
            continue;

        LOG(VB_RECORD, LOG_DEBUG,
            QString("StdinNotifier received: [%1]")
            .arg(QString::fromUtf8(line)));

        if (line == "APIVersion?")
        {
            emit apiVersionRequested();
            continue;
        }

        QJsonParseError jsonError;
        QJsonDocument jsonDoc =
            QJsonDocument::fromJson(line, &jsonError);

        if (jsonDoc.isNull() || !jsonDoc.isObject())
        {
            LOG(VB_RECORD, LOG_ERR,
                QString("Invalid JSON message received: %1. Details: %2")
                .arg(QString::fromUtf8(line),
                     jsonError.errorString()));
            continue;
        }

        emit commandReceived(jsonDoc.object());
    }

    m_notifier->setEnabled(true);
}
