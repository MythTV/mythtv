#ifndef DECODERHANDLER_H_
#define DECODERHANDLER_H_

// c++
#include <iostream>

// qt
#include <QObject>
#include <QIODevice>
#include <QFile>
#include <QUrl>
#include <QMutex>

// mythtv
#include <mythobservable.h>

// mythmusic
#include "metadata.h"
#include "pls.h"

class MusicBuffer;
class RemoteFile;

class QUrl;
class QNetworkAccessManager;
class QNetworkReply;

class Decoder;
class Metadata;
class DecoderIOFactory;
class DecoderHandler;
class MusicBuffer;
class MusicIODevice;

/** \brief Events sent by the \p DecoderHandler and it's helper classes.
 */
class DecoderHandlerEvent : public MythEvent
{
  public:
    DecoderHandlerEvent(Type t)
        : MythEvent(t), m_msg(0), m_meta(0), m_available(0), m_maxSize(0) {}

    DecoderHandlerEvent(Type t, QString *e)
        : MythEvent(t), m_msg(e), m_meta(0), m_available(0), m_maxSize(0) {}

    DecoderHandlerEvent(Type t, int available, int maxSize)
        : MythEvent(t), m_msg(0), m_meta(0), 
          m_available(available), m_maxSize(maxSize) {}

    DecoderHandlerEvent(Type t, const Metadata &m);
    ~DecoderHandlerEvent();

    QString *getMessage(void) const { return m_msg; }
    Metadata *getMetadata(void) const { return m_meta; }
    void getBufferStatus(int *available, int *maxSize) const;

    virtual MythEvent *clone(void) const;

    static Type Ready;
    static Type Meta;
    static Type BufferStatus;
    static Type OperationStart;
    static Type OperationStop;
    static Type Error;

  private:
    QString  *m_msg;
    Metadata *m_meta;
    int       m_available;
    int       m_maxSize;
};

/** \brief Class for starting stream decoding.

    This class handles starting the \p Decoder for the \p
    MusicPlayer via DecoderIOFactorys. 

    It operates on a playlist, either created with a single file, by
    loading a .pls or downloading it, and for each entry creates an
    appropriate DecoderIOFactory. The creator is simply a intermediate
    class that translates the next URL in the playlist to
    QIODevice. Ie. the DecoderIOFactoryFile just returns a QFile,
    whereas the DecoderIOFactoryShoutcast returns a QSocket subclass,
    where reads do the necessary translation of the shoutcast stream.
 */
class DecoderHandler : public QObject, public MythObservable
{
  Q_OBJECT
    friend class DecoderIOFactory;
  public:
    typedef enum 
    {
        ACTIVE,
        LOADING,
        STOPPED
    } State;

    DecoderHandler(void);
    virtual ~DecoderHandler(void);

    Decoder *getDecoder(void) { return m_decoder; }
    void start(Metadata *mdata);

    void stop(void);
    void customEvent(QEvent *e);
    bool done(void);
    bool next(void);
    void error(const QString &msg);

  protected:
    void doOperationStart(const QString &name);
    void doOperationStop(void);
    void doConnectDecoder(const QUrl &url, const QString &format);
    void doFailed(const QUrl &url, const QString &message);
    void doStart(bool result);

  private:
    void createPlaylist(const QUrl &url);
    void createPlaylistForSingleFile(const QUrl &url);
    void createPlaylistFromFile(const QUrl &url);
    void createPlaylistFromRemoteUrl(const QUrl &url);

    bool haveIOFactory(void) { return m_io_factory != NULL; }
    DecoderIOFactory *getIOFactory(void) { return m_io_factory; }
    void createIOFactory(const QUrl &url);
    void deleteIOFactory(void);

    int               m_state;
    int               m_playlist_pos;
    PlayListFile      m_playlist;
    DecoderIOFactory *m_io_factory;
    Decoder          *m_decoder;
    Metadata         *m_meta;
    bool              m_op;
    uint              m_redirects;

    static const uint MaxRedirects = 3;
};

/** \brief The glue between the DecoderHandler and the Decoder
    
    The DecoderIOFactory is responsible for opening the QIODevice that
    is given to the Decoder....
 */
class DecoderIOFactory : public QObject, public MythObservable
{
  public:
    DecoderIOFactory(DecoderHandler *parent);
    virtual ~DecoderIOFactory();

    virtual void start(void) = 0;
    virtual void stop(void) = 0;
    virtual QIODevice *takeInput(void) = 0;

    void setUrl (const QUrl &url) { m_url = url; }
    void setMeta (Metadata *meta) { m_meta = *meta; }

    static const uint DefaultPrebufferSize = 128 * 1024;
    static const uint DefaultBufferSize = 256 * 1024;
    static const uint MaxRedirects = 3;

  protected:
    void doConnectDecoder(const QString &format);
    Decoder *getDecoder(void);
    void doFailed(const QString &message);
    void doOperationStart(const QString &name);
    void doOperationStop(void);
    Metadata& getMetadata() { return m_meta; }
    QUrl& getUrl() { return m_url; }

  private:
    DecoderHandler *m_handler;
    Metadata        m_meta;
    QUrl            m_url;
};

class DecoderIOFactoryFile : public DecoderIOFactory
{
  Q_OBJECT

  public:
    DecoderIOFactoryFile(DecoderHandler *parent);
    ~DecoderIOFactoryFile(void);
    void start(void);
    void stop() {}
    QIODevice *takeInput(void);

  private:
    QIODevice *m_input;
};

class DecoderIOFactorySG : public DecoderIOFactory
{
  Q_OBJECT

  public:
    DecoderIOFactorySG(DecoderHandler *parent);
    ~DecoderIOFactorySG(void);
    void start(void);
    void stop() {}
    QIODevice *takeInput(void);

  private:
    QIODevice *m_input;
};

class DecoderIOFactoryUrl : public DecoderIOFactory 
{
  Q_OBJECT

  public:
    DecoderIOFactoryUrl(DecoderHandler *parent);
    ~DecoderIOFactoryUrl(void);

    void start(void);
    void stop(void);
    QIODevice *takeInput(void);

  protected slots:
    void replyFinished(QNetworkReply *reply);
    void readyRead(void);

  private:
    void doStart(void);
    void doClose(void);

    bool m_started;
    QNetworkAccessManager *m_accessManager;
    QNetworkReply    *m_reply;
    MusicIODevice *m_input;
    QUrl   m_redirectedURL;
    uint   m_redirectCount;
    uint   m_bytesWritten;
};

class MusicBuffer
{
  public:
    MusicBuffer(void) { }
    ~MusicBuffer(void) { }

    qint64 read(char *data, qint64 max, bool doRemove = true);
    qint64 read(QByteArray &array, qint64 max, bool doRemove = true);

    void   write(const char *data, uint sz);
    void   write(QByteArray &array);

    void   remove(int index, int len);

    qint64 readBufAvail(void) const { return m_buffer.size(); }

  private:
    QByteArray m_buffer;
    QMutex     m_mutex;
};

class MusicIODevice : public QIODevice
{
  Q_OBJECT

  public:
    MusicIODevice(void);
    ~MusicIODevice(void);

    virtual bool open(OpenMode mode);
    virtual void close(void) { };
    bool flush(void);

    qint64 size(void) const;
    qint64 pos(void) const { return 0; }
    qint64 bytesAvailable(void) const;
    bool   isSequential(void) const { return true; }

    qint64 readData(char *data, qint64 sz);
    qint64 writeData(const char *data, qint64 sz);

    int getch(void);
    int putch(int c);
    int ungetch(int);

  signals:
    void freeSpaceAvailable(void);

  protected:
    MusicBuffer *m_buffer;
};

class MusicSGIODevice : public QIODevice
{
  Q_OBJECT

  public:
    MusicSGIODevice(const QString &url);
    ~MusicSGIODevice(void);

    virtual bool open(OpenMode mode);
    virtual void close(void) { };
    bool flush(void);

    qint64 size(void) const;
    qint64 pos(void) const { return 0; }
    qint64 bytesAvailable(void) const;
    bool   isSequential(void) const { return true; }
    bool   seek(qint64 pos);

    qint64 readData(char *data, qint64 sz);
    qint64 writeData(const char *data, qint64 sz);

    int getch(void);
    int putch(int c);
    int ungetch(int);

  protected:
    RemoteFile *m_remotefile;
};

#endif /* DECODERHANDLER_H_ */
