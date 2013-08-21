#ifndef GAMEUI_H_
#define GAMEUI_H_

#include <QString>
#include <QObject>

// myth
#include <mythscreentype.h>
#include <metadata/metadatacommon.h>
#include <metadata/metadatadownload.h>
#include <metadata/metadataimagedownload.h>
#include <mythprogressdialog.h>

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
    GameUI(MythScreenStack *parentStack);
    ~GameUI();

    bool Create();
    void BuildTree();
    bool keyPressEvent(QKeyEvent *event);

  public slots:
    void nodeChanged(MythGenericTree* node);
    void itemClicked(MythUIButtonListItem* item);
    void showImages(void);
    void searchComplete(QString);
    void gameSearch(MythGenericTree *node = NULL,
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
    void showMenu(void);
    void searchStart(void);
    void toggleFavorite(void);
    void customEvent(QEvent *event);
    void createBusyDialog(QString title);

    QString getFillSql(MythGenericTree* node) const;
    QString getChildLevelString(MythGenericTree *node) const;
    QString getFilter(MythGenericTree *node) const;
    int     getLevelsOnThisBranch(MythGenericTree *node) const;
    bool    isLeaf(MythGenericTree *node) const;
    void    fillNode(MythGenericTree *node);
    void    resetOtherTrees(MythGenericTree *node);
    void    updateChangedNode(MythGenericTree *node, RomInfo *romInfo);
    void    handleDownloadedImages(MetadataLookup *lookup);

  private:
    bool m_showHashed;
    int m_gameShowFileName;

    MythGenericTree  *m_gameTree;
    MythGenericTree  *m_favouriteNode;

    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;

    MythUIButtonTree *m_gameUITree;
    MythUIText       *m_gameTitleText;
    MythUIText       *m_gameSystemText;
    MythUIText       *m_gameYearText;
    MythUIText       *m_gameGenreText;
    MythUIText       *m_gamePlotText;
    MythUIStateType  *m_gameFavouriteState;
    MythUIImage      *m_gameImage;
    MythUIImage      *m_fanartImage;
    MythUIImage      *m_boxImage;

    MetadataDownload      *m_query;
    MetadataImageDownload *m_imageDownload;

    GameScanner      *m_scanner;
};

#endif
