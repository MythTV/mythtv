#ifndef CDRIP_H_
#define CDRIP_H_

// C++
#include <utility>

// qt
#include <QCoreApplication>
#include <QEvent>
#include <QVector>

// MythTV
#include <libmythbase/mthread.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythui/mythscreentype.h>

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
    explicit CDScannerThread(Ripper *ripper)
        : MThread("CDScanner"), m_parent(ripper) {}
    void run() override; // MThread

  private:
    Ripper *m_parent {nullptr};
};

class CDEjectorThread: public MThread
{
    public:
    explicit CDEjectorThread(Ripper *ripper)
        : MThread("CDEjector"), m_parent(ripper) {}
    void run() override; // MThread

    private:
        Ripper    *m_parent {nullptr};
};

struct RipTrack
{
    MusicMetadata             *metadata { nullptr };
    bool                       active   { false   };
    std::chrono::milliseconds  length   { 0       };
    bool                       isNew    { false   };
};

Q_DECLARE_METATYPE(RipTrack *)

class RipStatus;

class CDRipperThread: public MThread
{
    Q_DECLARE_TR_FUNCTIONS(CDRipperThread);

    public:
        CDRipperThread(RipStatus *parent,  QString device,
                       QVector<RipTrack*> *tracks, int quality);
        ~CDRipperThread() override;

        void cancel(void);

    private:
        void run(void) override; // MThread
        int ripTrack(QString &cddevice, Encoder *encoder, int tracknum);

        bool isCancelled(void) const;

        RipStatus         *m_parent           {nullptr};
        bool               m_quit             {false};
        QString            m_cdDevice;
        int                m_quality;
        QVector<RipTrack*> *m_tracks          {nullptr};

        long int           m_totalSectors     {0};
        long int           m_totalSectorsDone {0};

        int                m_lastTrackPct     {0};
        int                m_lastOverallPct   {0};

        QString            m_musicStorageDir;

};

class Ripper : public MythScreenType
{
    Q_OBJECT
  public:
    Ripper(MythScreenStack *parent, QString device);
   ~Ripper(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

    bool somethingWasRipped() const;
    void scanCD(void);
    void ejectCD(void);

    void ShowMenu(void) override; // MythScreenType

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
    void searchArtist(void) const;
    void searchAlbum(void) const;
    void searchGenre(void);
    void setArtist(const QString& artist);
    void setAlbum(const QString& album);
    void setGenre(const QString& genre);
    void RipComplete(bool result);
    void toggleTrackActive(MythUIButtonListItem *item);
    void showEditMetadataDialog(MythUIButtonListItem *item);
    void EjectFinished(void);
    void ScanFinished(void);
    void metadataChanged(void);
    void showEditMetadataDialog(void);
    void chooseBackend(void) const;
    void setSaveHost(const QString& host);

  signals:
    void ripFinished(void);

  private:
    bool deleteExistingTrack(RipTrack *track);
    void deleteAllExistingTracks(void);
    void updateTrackList(void);
    void updateTrackLengths(void);
    void toggleTrackActive(RipTrack *track);
    void ShowConflictMenu(RipTrack *track);

    QString             m_musicStorageDir;

    CdDecoder          *m_decoder            {nullptr};

    MythUITextEdit     *m_artistEdit         {nullptr};
    MythUITextEdit     *m_albumEdit          {nullptr};
    MythUITextEdit     *m_genreEdit          {nullptr};
    MythUITextEdit     *m_yearEdit           {nullptr};

    MythUICheckBox     *m_compilationCheck   {nullptr};

    MythUIButtonList   *m_trackList          {nullptr};
    MythUIButtonList   *m_qualityList        {nullptr};

    MythUIButton       *m_switchTitleArtist  {nullptr};
    MythUIButton       *m_scanButton         {nullptr};
    MythUIButton       *m_ripButton          {nullptr};
    MythUIButton       *m_searchArtistButton {nullptr};
    MythUIButton       *m_searchAlbumButton  {nullptr};
    MythUIButton       *m_searchGenreButton  {nullptr};

    QVector<RipTrack*> *m_tracks             {nullptr};

    QString            m_albumName;
    QString            m_artistName;
    QString            m_genreName;
    QString            m_year;
    QStringList        m_searchList;
    bool               m_somethingwasripped  {false};
    bool               m_mediaMonitorActive  {false};

    QString            m_cdDevice;

    CDEjectorThread   *m_ejectThread         {nullptr};
    CDScannerThread   *m_scanThread          {nullptr};
};


class RipStatusEvent : public QEvent
{
  public:
    RipStatusEvent(Type type, int val) :
        QEvent(type), m_text(""), m_value(val) {}
    RipStatusEvent(Type type, QString val) :
        QEvent(type), m_text(std::move(val)) {}
    ~RipStatusEvent() override = default;

    QString m_text;
    int     m_value {-1};

    static const Type kTrackTextEvent;
    static const Type kOverallTextEvent;
    static const Type kStatusTextEvent;
    static const Type kTrackProgressEvent;
    static const Type kTrackPercentEvent;
    static const Type kTrackStartEvent;
    static const Type kOverallProgressEvent;
    static const Type kOverallPercentEvent;
    static const Type kOverallStartEvent;
    static const Type kCopyStartEvent;
    static const Type kCopyEndEvent;
    static const Type kFinishedEvent;
    static const Type kEncoderErrorEvent;
};

class RipStatus : public MythScreenType
{
  Q_OBJECT
  public:
    RipStatus(MythScreenStack *parent, QString device,
              QVector<RipTrack*> *tracks, int quality)
        : MythScreenType(parent, "ripstatus"), m_tracks(tracks),
          m_quality(quality), m_cdDevice(std::move(device)) {}
    ~RipStatus(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  signals:
    void Result(bool);

  protected slots:
    void startRip(void);

  private:
    void customEvent(QEvent *event) override; // MythUIType

    QVector<RipTrack*> *m_tracks         {nullptr};
    int                m_quality;
    QString            m_cdDevice;

    MythUIText        *m_overallText     {nullptr};
    MythUIText        *m_trackText       {nullptr};
    MythUIText        *m_statusText      {nullptr};
    MythUIText        *m_overallPctText  {nullptr};
    MythUIText        *m_trackPctText    {nullptr};
    MythUIProgressBar *m_overallProgress {nullptr};
    MythUIProgressBar *m_trackProgress   {nullptr};

    CDRipperThread    *m_ripperThread    {nullptr};
};

#endif
