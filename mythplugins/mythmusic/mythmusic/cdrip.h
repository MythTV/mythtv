#ifndef CDRIP_H_
#define CDRIP_H_

// qt
#include <QEvent>
#include <QVector>
#include <QCoreApplication>

// mythtv
#include <musicmetadata.h>
#include <mythscreentype.h>
#include <mthread.h>


class MythUIText;
class MythUITextEdit;
class MythUIImage;
class MythUIButton;
class MythUIButtonList;
class MythUICheckBox;

class CdDecoder;
class Encoder;
class Ripper;

class CDScannerThread: public MThread
{
  public:
    CDScannerThread(Ripper *ripper);
    virtual void run();

  private:
    Ripper *m_parent;
};

class CDEjectorThread: public MThread
{
    public:
        CDEjectorThread(Ripper *ripper);
        virtual void run();

    private:
        Ripper            *m_parent;
};

typedef struct
{
    MusicMetadata *metadata;
    bool           active;
    int            length;
    bool           isNew;
} RipTrack;

Q_DECLARE_METATYPE(RipTrack *)

class RipStatus;

class CDRipperThread: public MThread
{
    Q_DECLARE_TR_FUNCTIONS(CDRipperThread)

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

        QString            m_musicStorageDir;

};

class Ripper : public MythScreenType
{
    Q_OBJECT
  public:
    Ripper(MythScreenStack *parent, QString device);
   ~Ripper(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *);

    bool somethingWasRipped();
    void scanCD(void);
    void ejectCD(void);

    virtual void ShowMenu(void);

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
    void searchArtist(void);
    void searchAlbum(void);
    void searchGenre(void);
    void setArtist(QString artist);
    void setAlbum(QString album);
    void setGenre(QString genre);
    void RipComplete(bool result);
    void toggleTrackActive(MythUIButtonListItem *);
    void showEditMetadataDialog(MythUIButtonListItem *);
    void EjectFinished(void);
    void ScanFinished(void);
    void metadataChanged(void);
    void showEditMetadataDialog(void);
    void chooseBackend(void);
    void setSaveHost(QString host);

  signals:
    void ripFinished(void);

  private:
    bool deleteExistingTrack(RipTrack *track);
    void deleteAllExistingTracks(void);
    void updateTrackList(void);
    void updateTrackLengths(void);
    void toggleTrackActive(RipTrack *);
    void ShowConflictMenu(RipTrack *);

    QString    m_musicStorageDir;

    CdDecoder *m_decoder;

    MythUITextEdit   *m_artistEdit;
    MythUITextEdit   *m_albumEdit;
    MythUITextEdit   *m_genreEdit;
    MythUITextEdit   *m_yearEdit;

    MythUICheckBox   *m_compilationCheck;

    MythUIButtonList *m_trackList;
    MythUIButtonList *m_qualityList;

    MythUIButton  *m_switchTitleArtist;
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

    CDEjectorThread   *m_ejectThread;
    CDScannerThread   *m_scanThread;
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
    static Type kCopyStartEvent;
    static Type kCopyEndEvent;
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
    MythUIProgressBar *m_overallProgress;
    MythUIProgressBar *m_trackProgress;

    CDRipperThread    *m_ripperThread;
};

#endif
