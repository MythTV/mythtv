#ifndef GAMEUI_H_
#define GAMEUI_H_

#include <QString>

// myth
#include <mythscreentype.h>

class MythUIButtonTree;
class MythGenericTree;
class MythUIText;
class MythUIStateType;
class RomInfo;
class QTimer;
class QKeyEvent;
class QEvent;

class GameUI : public MythScreenType
{
    Q_OBJECT

  public:
    GameUI(MythScreenStack *parentStack);
    ~GameUI();

    bool Create();
    bool keyPressEvent(QKeyEvent *event);

  public slots:
    void nodeChanged(MythGenericTree* node);
    void itemClicked(MythUIButtonListItem* item);
    void showImages(void);
    void searchComplete(QString);

  private:
    void updateRomInfo(RomInfo *rom);
    void clearRomInfo(void);
    void edit(void);
    void showInfo(void);
    void showMenu(void);
    void searchStart(void);
    void toggleFavorite(void);
    void customEvent(QEvent *event);

    QString getFillSql(MythGenericTree* node) const;
    QString getChildLevelString(MythGenericTree *node) const;
    QString getFilter(MythGenericTree *node) const;
    int     getLevelsOnThisBranch(MythGenericTree *node) const;
    bool    isLeaf(MythGenericTree *node) const;
    void    fillNode(MythGenericTree *node);
    void    resetOtherTrees(MythGenericTree *node);
    void    updateChangedNode(MythGenericTree *node, RomInfo *romInfo);

  private:
    bool m_showHashed;
    int m_gameShowFileName;
    QTimer *timer;

    MythGenericTree  *m_gameTree;
    MythGenericTree  *m_favouriteNode;

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
};

#endif
