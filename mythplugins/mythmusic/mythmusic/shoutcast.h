/*
  Shoutcast decoder for MythTV.
  Eskil Heyn Olsen, 2005, distributed under the GPL as part of mythtv.
  Paul Harrison July 2010, Updated for Qt4
*/

#ifndef SHOUTCAST_H_
#define SHOUTCAST_H_

// c
#include <sys/time.h>
#include <time.h>

// qt
#include <QObject>
#include <QTcpSocket>
#include <QBuffer>
#include <QUrl>
#include <QMutex>

//mythtv
#include "config.h"

// mythmusic
#include "decoder.h"
#include "decoderhandler.h"

class ShoutCastRequest;
class ShoutCastResponse;

typedef QMap<QString,QString> ShoutCastMetaMap;

class ShoutCastIODevice : public MusicIODevice
{
  Q_OBJECT

  public:
    enum State
    {
        NOT_CONNECTED,
        RESOLVING,
        CONNECTING,
        CANT_RESOLVE,
        CANT_CONNECT,
        CONNECTED,
        WRITING_HEADER,
        READING_HEADER,
        PLAYING,
        STREAMING,
        STREAMING_META,
        STOPPED
    };
    static const char* stateString(const State &s);

    ShoutCastIODevice(void);
    ~ShoutCastIODevice(void);

    State getState(void) { return m_state; }

    void connectToUrl(const QUrl &url);
    void close(void);
    bool flush(void);

    qint64 size(void) const;
    qint64 pos(void) const { return 0; }
    qint64 bytesAvailable(void) const;
    bool   isSequential(void) const { return true; }

    qint64 readData(char *data, qint64 sz);
    qint64 writeData(const char *data, qint64 sz);

    bool getResponse(ShoutCastResponse &response);

  signals:
    void meta(const QString &metadata);
    void changedState(ShoutCastIODevice::State newstate);
    void bufferStatus(int available, int max);

  private slots:
    void socketHostFound(void);
    void socketConnected(void);
    void socketConnectionClosed(void);
    void socketReadyRead(void);
    void socketBytesWritten(qint64 );
    void socketError(QAbstractSocket::SocketError error);

  private:
    void switchToState(const State &s);
    bool parseHeader(void);
    bool parseMeta(void);

    // Our tools
    ShoutCastResponse *m_response;
    int                m_redirects;
    QTcpSocket        *m_socket;

    // Our scratchpad
    QByteArray         m_scratchpad;
    qint64             m_scratchpad_pos;

    // Our state info
    QUrl           m_url;
    qint64         m_bytesTillNextMeta;
    State          m_state;
    QString        m_last_metadata;
    qint64         m_bytesDownloaded;
    bool           m_response_gotten;
    bool           m_started;
};

class DecoderIOFactoryShoutCast : public DecoderIOFactory
{
  Q_OBJECT

  public:
    DecoderIOFactoryShoutCast(DecoderHandler *parent);
    ~DecoderIOFactoryShoutCast(void);

    void start(void);
    void stop(void);
    QIODevice *getInput(void);

  protected slots:
    void periodicallyCheckResponse(void);
    void periodicallyCheckBuffered(void);
    void shoutcastMeta(const QString &metadata);
    void shoutcastChangedState(ShoutCastIODevice::State newstate);
    void shoutcastBufferStatus(int available, int maxSize);

  private:
    int checkResponseOK(void);

    void makeIODevice(void);
    void closeIODevice(void);

    QTimer *m_timer;

    ShoutCastIODevice *m_input;
    uint m_prebuffer;

    QTime m_lastStatusTime;
};

#endif /* SHOUTCAST_H_ */
