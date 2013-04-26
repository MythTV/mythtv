#ifndef IMPORTMUSIC_H_
#define IMPORTMUSIC_H_

#include <iostream>
#include <vector>
using namespace std;

#include <QStringList>

#include <mythscreentype.h>
#include <mthread.h>

class MusicMetadata;
class ImportMusicDialog;

class MythUIText;
class MythUITextEdit;
class MythUIImage;
class MythUIButton;
class MythUIButtonList;
class MythUICheckBox;
class MythDialogBox;

typedef struct
{
    MusicMetadata *metadata;
    bool           isNewTune;
    bool           metadataHasChanged;
} TrackInfo;

class FileScannerThread: public MThread
{
    public:
        FileScannerThread(ImportMusicDialog *parent);
        virtual void run();

    private:
        ImportMusicDialog *m_parent;
};

class ImportMusicDialog : public MythScreenType
{

    Q_OBJECT

  public:
    ImportMusicDialog(MythScreenStack *parent);
    ~ImportMusicDialog();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *);

    bool somethingWasImported() { return m_somethingWasImported; }
    void doScan(void);

  public slots:
    void addAllNewPressed(void);
    void playPressed(void);
    void addPressed(void);
    void nextNewPressed(void);
    void locationPressed(void);
    void coverArtPressed(void);

    void nextPressed(void);
    void prevPressed(void);
    void showEditMetadataDialog(void);
    void startScan(void);
    void doExit(bool ok);

    // popup menu
    void showMenu(void);
    void saveDefaults(void);
    void setCompilation(void);
    void setCompilationArtist(void);
    void setArtist(void);
    void setAlbum(void);
    void setYear(void);
    void setTrack(void);
    void setGenre(void);
    void setRating(void);
    void setTitleWordCaps(void);
    void setTitleInitialCap(void);
    void metadataChanged(void);

  signals:
    void importFinished(void);

  private:
    void fillWidgets();
    void scanDirectory(QString &directory, vector<TrackInfo*> *tracks);
    void showImportCoverArtDialog();

    bool                 m_somethingWasImported;
    vector<TrackInfo*>  *m_tracks;
    QStringList          m_sourceFiles;
    int                  m_currentTrack;
    MusicMetadata       *m_playingMetaData;

    //  GUI stuff
    MythUITextEdit  *m_locationEdit;
    MythUIButton    *m_locationButton;
    MythUIButton    *m_scanButton;
    MythUIButton    *m_coverartButton;

    MythUIText      *m_filenameText;
    MythUIText      *m_compartistText;
    MythUIText      *m_artistText;
    MythUIText      *m_albumText;
    MythUIText      *m_titleText;
    MythUIText      *m_genreText;
    MythUIText      *m_yearText;
    MythUIText      *m_trackText;

    MythUIButton    *m_nextButton;
    MythUIButton    *m_prevButton;

    MythUIText      *m_currentText;
    MythUIText      *m_statusText;

    MythUIButton    *m_playButton;
    MythUIButton    *m_addButton;
    MythUIButton    *m_addallnewButton;
    MythUIButton    *m_nextnewButton;

    MythUICheckBox  *m_compilationCheck;

    MythDialogBox   *m_popupMenu;

    // default metadata values
    bool             m_defaultCompilation;
    QString          m_defaultCompArtist;
    QString          m_defaultArtist;
    QString          m_defaultAlbum;
    QString          m_defaultGenre;
    int              m_defaultYear;
    int              m_defaultRating;
    bool             m_haveDefaults;
};

///////////////////////////////////////////////////////////////////////////////

class ImportCoverArtDialog : public MythScreenType
{

    Q_OBJECT

  public:

    ImportCoverArtDialog(MythScreenStack *parent, const QString &sourceDir,
                         MusicMetadata *metadata);
    ~ImportCoverArtDialog();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  public slots:
    void copyPressed(void);
    void prevPressed(void);
    void nextPressed(void);
    void selectorChanged(void);

  private:
    void scanDirectory(void);
    void updateStatus(void);
    void updateTypeSelector(void);

    QStringList    m_filelist;
    QString        m_sourceDir;
    MusicMetadata *m_metadata;
    int            m_currentFile;
    QString        m_saveFilename;

    //
    //  GUI stuff
    //
    MythUIText          *m_filenameText;
    MythUIText          *m_currentText;
    MythUIText          *m_statusText;
    MythUIText          *m_destinationText;

    MythUIImage         *m_coverartImage;
    MythUIButtonList    *m_typeList;

    MythUIButton    *m_nextButton;
    MythUIButton    *m_prevButton;
    MythUIButton    *m_copyButton;
    MythUIButton    *m_exitButton;
};

#endif
