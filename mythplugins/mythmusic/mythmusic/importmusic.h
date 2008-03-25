#ifndef IMPORTMUSIC_H_
#define IMPORTMUSIC_H_

#include <iostream>
using namespace std;

//Added by qt3to4:
#include <QKeyEvent>
#include <QThread>

#include <mythtv/mythdialogs.h>

class Metadata;
class ImportMusicDialog;

typedef struct
{
    Metadata *metadata;
    bool      isNewTune;
    bool      metadataHasChanged;
} TrackInfo;

class FileScannerThread: public QThread
{
    public:
        FileScannerThread(ImportMusicDialog *parent);
        virtual void run();

    private:
        ImportMusicDialog *m_parent;
};

class ImportMusicDialog : public MythThemedDialog
{

  Q_OBJECT

  public:

    ImportMusicDialog(MythMainWindow *parent, const char* name = 0);
    ~ImportMusicDialog();

    bool somethingWasImported() { return m_somethingWasImported; }
    void doScan(void);

  public slots:
    void editLostFocus();
    void addAllNewPressed(void);
    void playPressed(void);
    void addPressed(void);
    void nextNewPressed(void);
    void locationPressed(void);
    void coverArtPressed(void);

    void nextPressed(void);
    void prevPressed(void);
    void showEditMetadataDialog(void);
    void scanPressed(void);

    // popup menu
    void showMenu(void);
    void closeMenu(void);
    void saveDefaults(void);
    void setCompilation(void);
    void setCompilationArtist(void);
    void setArtist(void);
    void setAlbum(void);
    void setYear(void);
    void setGenre(void);
    void setRating(void);
    void setTitleWordCaps(void);
    void setTitleInitialCap(void);

  private:
    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();
    void startScan(void);
    void scanDirectory(QString &directory, vector<TrackInfo*> *tracks);
    void showImportCoverArtDialog();

    bool                 m_somethingWasImported;
    vector<TrackInfo*>  *m_tracks;
    QStringList          m_sourceFiles;
    int                  m_currentTrack;

    //
    //  GUI stuff
    //
    UIRemoteEditType    *m_location_edit;
    UIPushButtonType    *m_location_button;
    UITextButtonType    *m_scan_button;
    UITextButtonType    *m_coverart_button;

    UITextType          *m_filename_text;
    UITextType          *m_compartist_text;
    UITextType          *m_artist_text;
    UITextType          *m_album_text;
    UITextType          *m_title_text;
    UITextType          *m_genre_text;
    UITextType          *m_year_text;
    UITextType          *m_track_text;

    UIPushButtonType    *m_next_button;
    UIPushButtonType    *m_prev_button;

    UITextType          *m_current_text;
    UITextType          *m_status_text;

    UITextButtonType    *m_play_button;
    UITextButtonType    *m_add_button;
    UITextButtonType    *m_addallnew_button;
    UITextButtonType    *m_nextnew_button;

    UICheckBoxType      *m_compilation_check;

    MythPopupBox        *m_popupMenu;

    // default metadata values
    bool                 m_defaultCompilation;
    QString              m_defaultCompArtist;
    QString              m_defaultArtist;
    QString              m_defaultAlbum;
    QString              m_defaultGenre;
    int                  m_defaultYear;
    int                  m_defaultRating;
    bool                 m_haveDefaults;
};

///////////////////////////////////////////////////////////////////////////////

class ImportCoverArtDialog : public MythThemedDialog
{

  Q_OBJECT

  public:

      ImportCoverArtDialog(const QString &sourceDir, Metadata *metadata,
                           MythMainWindow *parent, const char* name = 0);
    ~ImportCoverArtDialog();


  public slots:
    void copyPressed(void);
    void prevPressed(void);
    void nextPressed(void);
    void selectorChanged(int item);

  private:
    void wireUpTheme();
    void keyPressEvent(QKeyEvent *e);
    void scanDirectory(void);
    void updateStatus(void);
    void updateTypeSelector(void);

    QStringList         m_filelist;
    QString             m_sourceDir;
    Metadata           *m_metadata;
    int                 m_currentFile;
    QString             m_saveFilename;

    //
    //  GUI stuff
    //
    UITextType          *m_filename_text;
    UITextType          *m_current_text;
    UITextType          *m_status_text;
    UITextType          *m_destination_text;

    UIImageType         *m_coverart_image;
    UISelectorType      *m_type_selector;

    UIPushButtonType    *m_next_button;
    UIPushButtonType    *m_prev_button;
    UITextButtonType    *m_copy_button;
    UITextButtonType    *m_exit_button;
};

#endif
