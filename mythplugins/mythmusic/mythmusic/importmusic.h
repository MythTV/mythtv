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

struct TrackInfo
{
    MusicMetadata *metadata;
    bool           isNewTune;
    bool           metadataHasChanged;
};

class FileScannerThread: public MThread
{
    public:
        explicit FileScannerThread(ImportMusicDialog *parent);
        void run() override; // MThread

    private:
        ImportMusicDialog *m_parent {nullptr};
};

class FileCopyThread: public MThread
{
    public:
        FileCopyThread(const QString &src, const QString &dst)
            : MThread("FileCopy"), m_srcFile(src), m_dstFile(dst) {}
        void run() override; // MThread

        bool GetResult(void) { return m_result; }

    private:
        QString m_srcFile;
        QString m_dstFile;
        bool    m_result  {false};
};

class ImportMusicDialog : public MythScreenType
{

    Q_OBJECT

  public:
    explicit ImportMusicDialog(MythScreenStack *parent);
    ~ImportMusicDialog();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent *) override; // MythUIType

    bool somethingWasImported() { return m_somethingWasImported; }
    void doScan(void);
    void doFileCopy(const QString &src, const QString &dst);

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
    void ShowMenu(void) override; // MythScreenType
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
    void chooseBackend(void);
    void setSaveHost(const QString& host);

  signals:
    void importFinished(void);

  private:
    void fillWidgets();
    void scanDirectory(QString &directory, vector<TrackInfo*> *tracks);
    void showImportCoverArtDialog();
    static bool copyFile(const QString &src, const QString &dst);

    QString              m_musicStorageDir;
    bool                 m_somethingWasImported {false};
    vector<TrackInfo*>  *m_tracks               {nullptr};
    QStringList          m_sourceFiles;
    int                  m_currentTrack         {0};
    MusicMetadata       *m_playingMetaData      {nullptr};

    //  GUI stuff
    MythUITextEdit  *m_locationEdit             {nullptr};
    MythUIButton    *m_locationButton           {nullptr};
    MythUIButton    *m_scanButton               {nullptr};
    MythUIButton    *m_coverartButton           {nullptr};

    MythUIText      *m_filenameText             {nullptr};
    MythUIText      *m_compartistText           {nullptr};
    MythUIText      *m_artistText               {nullptr};
    MythUIText      *m_albumText                {nullptr};
    MythUIText      *m_titleText                {nullptr};
    MythUIText      *m_genreText                {nullptr};
    MythUIText      *m_yearText                 {nullptr};
    MythUIText      *m_trackText                {nullptr};

    MythUIButton    *m_nextButton               {nullptr};
    MythUIButton    *m_prevButton               {nullptr};

    MythUIText      *m_currentText              {nullptr};
    MythUIText      *m_statusText               {nullptr};

    MythUIButton    *m_playButton               {nullptr};
    MythUIButton    *m_addButton                {nullptr};
    MythUIButton    *m_addallnewButton          {nullptr};
    MythUIButton    *m_nextnewButton            {nullptr};

    MythUICheckBox  *m_compilationCheck         {nullptr};

    // default metadata values
    bool             m_defaultCompilation       {false};
    QString          m_defaultCompArtist;
    QString          m_defaultArtist;
    QString          m_defaultAlbum;
    QString          m_defaultGenre;
    int              m_defaultYear              {0};
    int              m_defaultRating            {0};
    bool             m_haveDefaults             {false};
};

///////////////////////////////////////////////////////////////////////////////

class ImportCoverArtDialog : public MythScreenType
{

    Q_OBJECT

  public:

    ImportCoverArtDialog(MythScreenStack *parent, const QString &sourceDir,
                         MusicMetadata *metadata, const QString &storageDir)
        : MythScreenType(parent, "import_coverart"), m_sourceDir(sourceDir),
          m_musicStorageDir(storageDir), m_metadata(metadata) {}
    ~ImportCoverArtDialog() = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

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
    QString        m_musicStorageDir;
    MusicMetadata *m_metadata              {nullptr};
    int            m_currentFile           {0};
    QString        m_saveFilename;

    //
    //  GUI stuff
    //
    MythUIText          *m_filenameText    {nullptr};
    MythUIText          *m_currentText     {nullptr};
    MythUIText          *m_statusText      {nullptr};
    MythUIText          *m_destinationText {nullptr};

    MythUIImage         *m_coverartImage   {nullptr};
    MythUIButtonList    *m_typeList        {nullptr};

    MythUIButton    *m_nextButton          {nullptr};
    MythUIButton    *m_prevButton          {nullptr};
    MythUIButton    *m_copyButton          {nullptr};
    MythUIButton    *m_exitButton          {nullptr};
};

#endif
