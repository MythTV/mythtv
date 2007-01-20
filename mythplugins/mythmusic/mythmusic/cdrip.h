#ifndef CDRIP_H_
#define CDRIP_H_

#include "metadata.h"

#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

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

enum StatusTypes
{
    ST_TRACK_TEXT = 0,
    ST_OVERALL_TEXT,
    ST_STATUS_TEXT,
    ST_TRACK_PROGRESS,
    ST_TRACK_PERCENT,
    ST_TRACK_START,
    ST_OVERALL_PROGRESS,
    ST_OVERALL_PERCENT,
    ST_OVERALL_START,
    ST_FINISHED
};

typedef struct
{
    int type;
    QString text;
    int value;
} StatusData;

class RipStatus;

class CDRipperThread: public QThread
{
    public:
        CDRipperThread(RipStatus *parent, vector<Metadata*> *tracks, int quality);
        ~CDRipperThread();

        void cancel(void);

    private:
        virtual void run(void);
        int ripTrack(QString &cddevice, Encoder *encoder, int tracknum);
        void sendEvent(int eventType, const QString &value);
        void sendEvent(int eventType, int value);

        bool isCancelled(void);

        RipStatus         *m_parent;
        bool               m_quit;
        QMutex             m_mutex;
        int                m_quality;
        vector<Metadata*> *m_tracks;

        int                m_totalTracks;
        long int           m_totalSectors;
        long int           m_totalSectorsDone;

        int                m_lastTrackPct;
        int                m_lastOverallPct;
};

class Ripper : public MythThemedDialog
{
    Q_OBJECT
  public:
    Ripper(MythMainWindow *parent, const char *name = 0);
   ~Ripper(void);

    bool somethingWasRipped();
    void scanCD(void);
    void ejectCD(void);
    static QString filenameFromMetadata(Metadata *track);

  protected slots:
    void startRipper(void);
    void startScanCD(void);
    void startEjectCD(void);
    void artistChanged(QString newartist);
    void albumChanged(QString newalbum);
    void genreChanged(QString newgenre);
    void yearChanged(QString newyear);
    void compilationChanged(bool state);
    void switchTitlesAndArtists();
    void reject();
    void searchArtist(void);
    void searchAlbum(void);
    void searchGenre(void);

  private:
    void wireupTheme(void);
    void keyPressEvent(QKeyEvent *e);
    bool isNewTune(QString &artist, QString &album, QString &title);
    void deleteTrack(QString& artist, QString& album, QString& title);
    void updateTrackList(void);
    void trackListDown(bool page);
    void trackListUp(bool page);
    bool showList(QString caption, QString &value);
    void showEditMetadataDialog();
    static QString fixFileToken(QString token);
    static QString fixFileToken_sl(QString token);

    CdDecoder         *m_decoder;
    UIRemoteEditType  *m_artistEdit;
    UISelectorType    *m_qualitySelector;
    UIRemoteEditType  *m_albumEdit;
    UIRemoteEditType  *m_genreEdit;
    UIRemoteEditType  *m_yearEdit;
    UICheckBoxType    *m_compilation;
    UITextButtonType  *m_switchTitleArtist;
    UIListType        *m_trackList;
    UITextButtonType  *m_scanButton;
    UITextButtonType  *m_ripButton;
    UIPushButtonType  *m_searchArtistButton;
    UIPushButtonType  *m_searchAlbumButton;
    UIPushButtonType  *m_searchGenreButton;

    int                m_currentTrack;
    int                m_totalTracks;
    vector<Metadata*> *m_tracks;

    QString            m_albumName, m_artistName, m_genreName, m_year;
    QStringList        m_searchList;
    bool               m_somethingwasripped;
};


class RipStatus : public MythThemedDialog
{
  Q_OBJECT
  public:
    RipStatus(vector<Metadata*> *tracks, int quality, MythMainWindow *parent, const char *name = 0);
    ~RipStatus(void);


  protected slots:
    void startRip();

  private:
    void wireupTheme(void);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

    vector<Metadata*> *m_tracks;
    int                m_quality;

    UITextType        *m_overallText;
    UITextType        *m_trackText;
    UITextType        *m_statusText; 
    UITextType        *m_overallPctText;
    UITextType        *m_trackPctText;
    UIStatusBarType   *m_overallProgress;
    UIStatusBarType   *m_trackProgress;

    CDRipperThread    *m_ripperThread;
};

#endif
