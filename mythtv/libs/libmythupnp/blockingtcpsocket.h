#ifndef LIBMYTHUPNP_BLOCKING_TCP_SOCKET
#define LIBMYTHUPNP_BLOCKING_TCP_SOCKET

#include <chrono>

#include <QHostAddress>
#include <QString>
#include <QTcpSocket>

/**
Wraps a QTcpSocket to provide a blocking connect(), readLine(), and write() with a timeout.
*/
class BlockingTcpSocket
{
  public:
    BlockingTcpSocket() = default;

    /** @brief Connect the socket to a host.
    @return True if a connection was established.
    */
    bool connect(const QHostAddress &address, quint16 port, std::chrono::milliseconds timeout);
    /** @brief Read a whole line from the socket.
    The data will remain buffered in the socket until QIODevice::canReadLine() returns true.
    @param timeout Total amount of time to wait.
    @return The entire line read from the socket, including '\n' per the QIODevice::readLine()
            documentation, or an empty string if no '\n' was read.
    */
    QString readLine(std::chrono::milliseconds timeout);
    qint64 write(const char* data, qint64 size, std::chrono::milliseconds timeout);

  private:
    QTcpSocket m_socket;
};

#endif // LIBMYTHUPNP_BLOCKING_TCP_SOCKET
