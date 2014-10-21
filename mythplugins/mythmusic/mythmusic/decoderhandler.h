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
#include <musicmetadata.h>

// mythmusic

#include "pls.h"

class Decoder;

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

    DecoderHandlerEvent(Type t, const MusicMetadata &m);
    ~DecoderHandlerEvent();

    QString *getMessage(void) const { return m_msg; }
    MusicMetadata *getMetadata(void) const { return m_meta; }
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
    MusicMetadata *m_meta;
    int       m_available;
    int       m_maxSize;
};

/** \brief Class for starting stream decoding.

    This class handles starting the \p Decoder for the \p
    MusicPlayer via DecoderIOFactorys. 

    It operates on a playlist, either created with a single file, by
    loading a .pls or downloading it, and for each entry creates an
    appropriate Decoder.
 */
class DecoderHandler : public QObject, public MythObservable
{
  Q_OBJECT

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

    void start(MusicMetadata *mdata);

    void stop(void);
    void customEvent(QEvent *e);
    bool done(void);
    bool next(void);
    void error(const QString &msg);

    MusicMetadata& getMetadata() { return m_meta; }

    QUrl& getUrl() { return m_url; }
    void setUrl (const QUrl &url) { m_url = url; }

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

    int               m_state;
    int               m_playlist_pos;
    PlayListFile      m_playlist;
    Decoder          *m_decoder;
    MusicMetadata     m_meta;
    QUrl              m_url;
    bool              m_op;
    uint              m_redirects;

    static const uint MaxRedirects = 3;
};

#endif /* DECODERHANDLER_H_ */
