#ifndef CDRIP_H_
#define CDRIP_H_

#include "metadata.h"

#include <QEvent>
#include <QVector>

#include <mythscreentype.h>

class MythUIText;
class MythUITextEdit;
class MythUIImage;
class MythUIButton;
class MythUIButtonList;
class MythUICheckBox;

class CdDecoder;
class Encoder;
class Ripper;

class CDScannerThread: public QThread
{
  public:
    CDScannerThread(Ripper *ripper);
    virtual void run();

  private:
    Ripper *m_parent;
};

class CDEjectorThread: public QThread
{
    public:
        CDEjectorThread(Ripper *ripper);
        virtual void run();

    private:
        Ripper            *m_parent;
};

typedef struct
{
    Metadata *metadata;
    bool      active;
    int       length;
} RipTrack;

class RipStatus;

class CDRipperThread: public QThread
{
    public:
        CDRipperThread(RipStatus *parent,  QString device,
                       QVector<RipTrack*> *tracks, int quality);
        ~CDRipperThread();

        void cancel(void);

    private:
        virtual void run(void);
        int ripTrack(QString &cddevice, Encoder *encoder, int tracknum);

        bool isCancelled(void);

        RipStatus         *m_parent;
        bool               m_quit;
        QString            m_CDdevice;
        int                m_quality;
        QVector<RipTrack*> *m_tracks;

        long int           m_totalSectors;
        long int           m_totalSectorsDone;

        int                m_lastTrackPct;
        int                m_lastOverallPct;
};

class Ripper : public MythScreenType
{
    Q_OBJECT
  public:
    Ripper(MythScreenStack *parent, QString device);
   ~Ripper(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    bool somethingWasRipped();
    void scanCD(void);
    void ejectCD(void);

    static QString filenameFromMetadata(Metadata *track, bool createDir = true);
    static bool isNewTune(const QString &artist,
                          const QString &album, const QString &title);

  protected slots:
    void startRipper(void);
    void startScanCD(void);
    void startEjectCD(void);
    void artistChanged(void);
    void albumChanged(void);
    void genreChanged(void);
    void yearChanged(void);
    void compilationChanged(bool state);
    void switchTitlesAndArtists();
    void reject();
    void searchArtist(void);
    void searchAlbum(void);
    void searchGenre(void);
    void RipComplete(bool result);
    void toggleTrackActive(MythUIButtonListItem *);
    void showEditMetadataDialog(MythUIButtonListItem *);

  signals:
    void ripFinished(void);

  private:
    void deleteTrack(QString& artist, QString& album, QString& title);
    void updateTrackList(void);
    void updateTrackLengths(void);

    bool showList(QString caption, QString &value);

    static QString fixFileToken(QString token);
    static QString fixFileToken_sl(QString token);

    CdDecoder           *m_decoder;

    MythUITextEdit      *m_artistEdit;
    MythUITextEdit      *m_albumEdit;
    MythUITextEdit      *m_genreEdit;
    MythUITextEdit      *m_yearEdit;

    MythUICheckBox      *m_compilationCheck;

    MythUIButtonList    *m_trackList;
    MythUIButtonList    *m_qualityList;

    MythUIButton        *m_switchTitleArtist;
    MythUIButton  *m_scanButton;
    MythUIButton  *m_ripButton;
    MythUIButton  *m_searchArtistButton;
    MythUIButton  *m_searchAlbumButton;
    MythUIButton  *m_searchGenreButton;

    QVector<RipTrack*> *m_tracks;

    QString            m_albumName, m_artistName, m_genreName, m_year;
    QStringList        m_searchList;
    bool               m_somethingwasripped;
    bool               m_mediaMonitorActive;

    QString            m_CDdevice;
};


class RipStatusEvent : public QEvent
{
  public:
    RipStatusEvent(Type t, int val) :
        QEvent(t), text(""), value(val) {}
    RipStatusEvent(Type t, const QString &val) :
        QEvent(t), text(val), value(-1) {}
    ~RipStatusEvent() {}

    QString text;
    int value;

    static Type kTrackTextEvent;
    static Type kOverallTextEvent;
    static Type kStatusTextEvent;
    static Type kTrackProgressEvent;
    static Type kTrackPercentEvent;
    static Type kTrackStartEvent;
    static Type kOverallProgressEvent;
    static Type kOverallPercentEvent;
    static Type kOverallStartEvent;
    static Type kFinishedEvent;
    static Type kEncoderErrorEvent;
};

class RipStatus : public MythScreenType
{
  Q_OBJECT
  public:
    RipStatus(MythScreenStack *parent, const QString &device,
              QVector<RipTrack*> *tracks, int quality);
    ~RipStatus(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  signals:
    void Result(bool);

  protected slots:
    void startRip(void);

  private:
    void customEvent(QEvent *event);

    QVector<RipTrack*> *m_tracks;
    int                m_quality;
    QString            m_CDdevice;

    MythUIText        *m_overallText;
    MythUIText        *m_trackText;
    MythUIText        *m_statusText;
    MythUIText        *m_overallPctText;
    MythUIText        *m_trackPctText;
    MythUIProgressBar   *m_overallProgress;
    MythUIProgressBar   *m_trackProgress;

    CDRipperThread    *m_ripperThread;
};

#endif
