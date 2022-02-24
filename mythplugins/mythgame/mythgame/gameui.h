#ifndef GAMEUI_H_
#define GAMEUI_H_

// Qt
#include <QString>
#include <QObject>

// MythTV
#include <libmythmetadata/metadatacommon.h>
#include <libmythmetadata/metadatadownload.h>
#include <libmythmetadata/metadataimagedownload.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreentype.h>

class MythUIButtonTree;
class MythGenericTree;
class MythUIText;
class MythUIStateType;
class RomInfo;
class QTimer;
class QKeyEvent;
class QEvent;
class GameScanner;

class GameUI : public MythScreenType
{
    Q_OBJECT

  public:
    explicit GameUI(MythScreenStack *parentStack);
    ~GameUI() override = default;

    bool Create() override; // MythScreenType
    void BuildTree();
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  public slots:
    void nodeChanged(MythGenericTree* node);
    void itemClicked(MythUIButtonListItem* item);
    void showImages(void);
    void searchComplete(const QString& string);
    void gameSearch(MythGenericTree *node = nullptr,
                     bool automode = false);
    void OnGameSearchListSelection(RefCountHandler<MetadataLookup> lookup);
    void OnGameSearchDone(MetadataLookup *lookup);
    void StartGameImageSet(MythGenericTree *node, QStringList coverart,
                           QStringList fanart, QStringList screenshot);
    void doScan(void);
    void reloadAllData(bool dbchanged);

  private:
    void updateRomInfo(RomInfo *rom);
    void clearRomInfo(void);
    void edit(void);
    void showInfo(void);
    void ShowMenu(void) override; // MythScreenType
    void searchStart(void);
    void toggleFavorite(void);
    void customEvent(QEvent *event) override; // MythUIType
    void createBusyDialog(const QString& title);

    QString getFillSql(MythGenericTree* node) const;
    static QString getChildLevelString(MythGenericTree *node);
    static QString getFilter(MythGenericTree *node) ;
    static int     getLevelsOnThisBranch(MythGenericTree *node);
    static bool    isLeaf(MythGenericTree *node);
    void    fillNode(MythGenericTree *node);
    void    resetOtherTrees(MythGenericTree *node);
    void    updateChangedNode(MythGenericTree *node, RomInfo *romInfo);
    void    handleDownloadedImages(MetadataLookup *lookup);

  private:
    bool m_showHashed                      {false};
    bool m_gameShowFileName                {false};

    MythGenericTree  *m_gameTree           {nullptr};
    MythGenericTree  *m_favouriteNode      {nullptr};

    MythUIBusyDialog *m_busyPopup          {nullptr};
    MythScreenStack  *m_popupStack         {nullptr};

    MythUIButtonTree *m_gameUITree         {nullptr};
    MythUIText       *m_gameTitleText      {nullptr};
    MythUIText       *m_gameSystemText     {nullptr};
    MythUIText       *m_gameYearText       {nullptr};
    MythUIText       *m_gameGenreText      {nullptr};
    MythUIText       *m_gamePlotText       {nullptr};
    MythUIStateType  *m_gameFavouriteState {nullptr};
    MythUIImage      *m_gameImage          {nullptr};
    MythUIImage      *m_fanartImage        {nullptr};
    MythUIImage      *m_boxImage           {nullptr};

    MetadataDownload      *m_query         {nullptr};
    MetadataImageDownload *m_imageDownload {nullptr};

    GameScanner      *m_scanner            {nullptr};
};

#endif
