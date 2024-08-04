#ifndef MYTHWEBSOCKET_H
#define MYTHWEBSOCKET_H

// Qt
#include <QTimer>
#include <QObject>

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif
#include <QElapsedTimer>

// MythTV
#include "http/mythwebsockettypes.h"

class QTcpSocket;

class MythWebSocket : public QObject
{
    Q_OBJECT

  public:
   ~MythWebSocket() override;
    static MythWebSocket* CreateWebsocket(bool Server, QTcpSocket* Socket,
                                          MythSocketProtocol Protocol, bool Testing);
  public slots:
    void Read();
    void Write(int64_t Written = 0);
    void Close();
    void SendTextFrame(const QString& Text);
    void SendBinaryFrame(const QByteArray& Data);

  signals:
    void NewTextMessage(const StringPayload& Text);
    void NewRawTextMessage(const DataPayloads& Payloads);
    void NewBinaryMessage(const DataPayloads& Payloads);

  protected:
    MythWebSocket(bool Server, QTcpSocket* Socket,
                  MythSocketProtocol Protocol, bool Testing);

  private:
    void PingReceived   (DataPayload Payload);
    void PongReceived   (const DataPayload& Payload);
    void CloseReceived  (const DataPayload& Payload);
    void CheckClose     ();
    void SendClose      (WSErrorCode Code, const QString& Message = {});
    void SendPing       ();
    void SendFrame      (WSOpCode Code, const DataPayloads& Payloads);

    enum ReadState : std::uint8_t
    {
        ReadHeader = 0,
        Read16BitLength,
        Read64BitLength,
        ReadMask,
        ReadPayload
    };

    QTcpSocket* m_socket            { nullptr };
    MythSocketProtocol m_protocol   { ProtFrame };
    WSOpCode    m_formatForProtocol { WSOpTextFrame };
    bool        m_preferRawText     { false };
    bool        m_testing           { false };
    QTimer*     m_timer             { nullptr };
    ReadState   m_readState         { ReadHeader };
    bool        m_serverSide        { false };
    bool        m_closeSent         { false };
    bool        m_closeReceived     { false };
    bool        m_closing           { false };

    WSFrame     m_currentFrame      { nullptr };
    WSOpCode    m_messageOpCode     { WSOpTextFrame };
    int64_t     m_messageSize       { 0 };
    DataPayloads m_dataFragments;
    StringPayload m_string          { nullptr };
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QTextCodec* m_utf8Codec         { QTextCodec::codecForName("UTF-8") };
    QTextCodec::ConverterState* m_utf8CodecState { new QTextCodec::ConverterState };
#else
    QStringDecoder *m_toUtf16       { new QStringDecoder };
#endif

    QByteArray  m_lastPingPayload;
    WSQueue     m_writeQueue;
    int64_t     m_writeTotal        { 0 };
    QElapsedTimer m_writeTime;
};

#endif
