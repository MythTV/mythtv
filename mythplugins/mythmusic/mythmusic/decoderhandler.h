#ifndef DECODERHANDLER_H_
#define DECODERHANDLER_H_

// c++
#include <iostream>

// qt
#include <QFile>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QUrl>

// MythTV
#include <libmythbase/mythobservable.h>
#include <libmythmetadata/musicmetadata.h>

// mythmusic
#include "pls.h"

class Decoder;

/** \brief Events sent by the \p DecoderHandler and it's helper classes.
 */
class DecoderHandlerEvent : public MythEvent
{
  public:
    explicit DecoderHandlerEvent(Type type)
        : MythEvent(type) {}

    DecoderHandlerEvent(Type type, QString *e)
        : MythEvent(type), m_msg(e) {}

    DecoderHandlerEvent(Type type, int available, int maxSize)
        : MythEvent(type), m_available(available), m_maxSize(maxSize) {}

    DecoderHandlerEvent(Type type, const MusicMetadata &m);
    ~DecoderHandlerEvent() override;

    QString *getMessage(void) const { return m_msg; }
    MusicMetadata *getMetadata(void) const { return m_meta; }
    void getBufferStatus(int *available, int *maxSize) const;

    MythEvent *clone(void) const override; // MythEvent

    static const Type kReady;
    static const Type kMeta;
    static const Type kBufferStatus;
    static const Type kOperationStart;
    static const Type kOperationStop;
    static const Type kError;

  // No implicit copying.
  protected:
    DecoderHandlerEvent(const DecoderHandlerEvent &other) = default;
    DecoderHandlerEvent &operator=(const DecoderHandlerEvent &other) = default;
  public:
    DecoderHandlerEvent(DecoderHandlerEvent &&) = delete;
    DecoderHandlerEvent &operator=(DecoderHandlerEvent &&) = delete;

  private:
    QString       *m_msg       {nullptr};
    MusicMetadata *m_meta      {nullptr};
    int            m_available {0};
    int            m_maxSize   {0};
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
    enum State : std::uint8_t
    {
        ACTIVE,
        LOADING,
        STOPPED
    };

    DecoderHandler(void) = default;
    ~DecoderHandler(void) override;

    Decoder *getDecoder(void) { return m_decoder; }

    void start(MusicMetadata *mdata);

    void stop(void);
    void customEvent(QEvent *e) override; // QObject
    bool done(void);
    bool next(void);
    void error(const QString &e);

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

    int               m_state        {STOPPED};
    int               m_playlistPos  {0};
    PlayListFile      m_playlist;
    Decoder          *m_decoder      {nullptr};
    MusicMetadata     m_meta;
    QUrl              m_url;
    bool              m_op           {false};
    uint              m_redirects    {0};
};

#endif /* DECODERHANDLER_H_ */
