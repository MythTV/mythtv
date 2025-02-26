#include "blockingtcpsocket.h"

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"

bool BlockingTcpSocket::connect(const QHostAddress &address, const quint16 port, const std::chrono::milliseconds timeout)
{
    m_socket.connectToHost(address, port);
    if (m_socket.waitForConnected(timeout.count()))
    {
        return true;
    }
    LOG(VB_UPNP, LOG_ERR, QString("Failed to connect a socket to %1:%2.  %3")
        .arg(address.toString(), QString::number(port), m_socket.errorString())
        );
    return false;
}

QString BlockingTcpSocket::readLine(const std::chrono::milliseconds timeout)
{
    MythTimer timer;
    timer.start();
    while (timer.elapsed() < timeout && !m_socket.canReadLine())
    {
        m_socket.waitForReadyRead((timeout - timer.elapsed()).count());
    }
    if (timer.elapsed() >= timeout)
    {
        LOG(VB_UPNP, LOG_ERR, QString("Exceeded timeout waiting to read line from %1:%2.")
            .arg(m_socket.peerAddress().toString(), QString::number(m_socket.peerPort()))
            );
    }

    if (m_socket.canReadLine())
    {
        return QString::fromUtf8(m_socket.readLine());
    }
    return {};
}

qint64 BlockingTcpSocket::write(const char* data, const qint64 size, const std::chrono::milliseconds timeout)
{
    MythTimer timer;
    timer.start();
    qint64 total = 0;
    do
    {
        auto written = m_socket.write(data + total, size - total);
        if (written == -1)
        {
            LOG(VB_UPNP, LOG_ERR, QString("Failed to write to %1:%2.  %3")
                .arg(m_socket.peerAddress().toString(),
                    QString::number(m_socket.peerPort()),
                    m_socket.errorString()
                    )
                );
            return -1;
        }
        total += written;
    }
    while (timer.elapsed() < timeout && total < size);

    do
    {
        m_socket.waitForBytesWritten((timeout - timer.elapsed()).count());
    }
    while (timer.elapsed() < timeout && m_socket.bytesToWrite() > 0);

    if (timer.elapsed() >= timeout)
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("Exceeded timeout waiting for write to %1:%2.  size: %3 total: %4 bytesToWrite: %5")
            .arg(m_socket.peerAddress().toString(), QString::number(m_socket.peerPort()),
                QString::number(size), QString::number(total), QString::number(m_socket.bytesToWrite())
                )
            );
    }
    return total;
}

