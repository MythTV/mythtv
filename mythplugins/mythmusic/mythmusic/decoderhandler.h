#ifndef DECODERHANDLER_H_
#define DECODERHANDLER_H_

// qt
#include <QObject>
#include <QIODevice>
#include <QFile>
#include <QHttp>
#include <QUrl>

// mythtv
#include <mythtv/mythobservable.h>

// mythmusic
#include "metadata.h"
#include "pls.h"



class QUrl;
class QNetworkAccessManager;
class QNetworkReply;

class Decoder;
class Metadata;
class DecoderIOFactory;
class DecoderHandler;

/** \brief Events sent by the \p DecoderHandler and it's helper classes.
 */
class DecoderHandlerEvent : public MythEvent
{
  public:
    DecoderHandlerEvent(Type t)
        : MythEvent(t), m_msg(0), m_meta(0) {}

    DecoderHandlerEvent(Type t, QString *e)
        : MythEvent(t), m_msg(e), m_meta(0) {}

    DecoderHandlerEvent(Type t, const Metadata &m);
    ~DecoderHandlerEvent();

    QString *getMessage(void) const { return m_msg; }
    Metadata *getMetadata(void) const { return m_meta; }

    virtual MythEvent *clone(void) const;

    static Type Ready;
    static Type Meta;
    static Type Info;
    static Type OperationStart;
    static Type OperationStop;
    static Type Error;

  private:
    QString  *m_msg;
    Metadata *m_meta;
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
    void doInfo(const QString &message);

  private:
    bool createPlaylist(const QUrl &url);
    bool createPlaylistForSingleFile(const QUrl &url);
    bool createPlaylistFromFile(const QUrl &url);
    bool createPlaylistFromRemoteUrl(const QUrl &url);

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
    static const uint MaxRedirects = 3;

  protected:
    void doConnectDecoder(const QString &format);
    Decoder *getDecoder(void);
    void doFailed(const QString &message);
    void doInfo(const QString &message);
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

#endif /* DECODERHANDLER_H_ */
