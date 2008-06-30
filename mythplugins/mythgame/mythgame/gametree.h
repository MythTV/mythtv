#ifndef GAMETREE_H_
#define GAMETREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <q3valuevector.h>
#include <q3ptrvector.h>
//Added by qt3to4:
#include <QSqlQuery>
#include <QKeyEvent>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/uitypes.h>

#include "rominfo.h"

/**************************************************************************
    GameTreeRoot - helper class holding tree root details
 **************************************************************************/

class GameTreeRoot
{
  public:
    GameTreeRoot(const QString& levels, const QString& filter)
      : m_levels(QStringList::split(" ", levels))
      , m_filter(filter)
    {
    }

    ~GameTreeRoot()
    {
    }

    unsigned getDepth() const                   { return m_levels.size(); }
    const QString& getLevel(unsigned i) const   { return m_levels[i]; }
    const QString& getFilter() const            { return m_filter; }

  private:
    QStringList m_levels;
    QString m_filter;
};

/**************************************************************************
    GameTreeItem - helper class supplying data and methods to each node
 **************************************************************************/

class GameTreeItem : public QObject
{
    Q_OBJECT
  public:
    GameTreeItem(GameTreeRoot* root)
      : m_romInfo(0)
      , m_root(root)
      , m_depth(0)
      , m_isFilled(false)
    {
        info_popup = NULL;
        m_gameShowFileName = gContext->GetSetting("GameShowFileNames").toInt();
        m_showHashed = false;
    }

    GameTreeItem(GameTreeRoot* root, bool showHashed)
      : m_romInfo(0)
      , m_root(root)
      , m_depth(0)
      , m_isFilled(false)
    {
        info_popup = NULL;
        m_gameShowFileName = gContext->GetSetting("GameShowFileNames").toInt();
        m_showHashed = showHashed;
    }


    ~GameTreeItem()
    {
        if (m_romInfo)
            delete m_romInfo;
    }

    bool isFilled() const             { return m_isFilled; }
    bool isLeaf() const               { return m_depth == m_root->getDepth(); }

    const QString& getLevel() const   { return m_root->getLevel(m_depth - 1); }
    RomInfo* getRomInfo() const       { return m_romInfo; }
    QString getFillSql(QString layer) const;

    void setFilled(bool isFilled)     { m_isFilled = isFilled; }

    GameTreeItem* createChild(QSqlQuery *query) const;
    void showGameInfo(RomInfo *rom);

    RomInfo* m_romInfo;

  protected slots:
    void closeGameInfo(void);
    void edit(void);

  private:
    QAbstractButton *OKButton;
    MythPopupBox *info_popup;
    GameTreeRoot* m_root;
    unsigned m_depth;
    bool m_isFilled;
    int m_gameShowFileName;
    bool m_showHashed;
};

class GameTree : public MythThemedDialog
{
    Q_OBJECT

  public:
    typedef Q3ValueVector<int> IntVector;

    GameTree(MythMainWindow *parent, QString windowName,
             QString themeFilename, const char *name = 0);
   ~GameTree();

  public slots:
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);
    void showImageTimeout(void); 

  protected:
    void keyPressEvent(QKeyEvent *e);

  private:
    void updateRomInfo(RomInfo *rom);
    void clearRomInfo(void);
    void wireUpTheme(void);
    void fillNode(GenericTree *node);
    void showInfo(void);
    void toggleFavorite(void);

    GenericTree           *m_gameTree;
    GenericTree		  *m_favouriteNode;
    UIManagedTreeListType *m_gameTreeUI;

    Q3ValueVector<GameTreeRoot *> m_gameTreeRoots;
    Q3ValueVector<GameTreeItem *> m_gameTreeItems;

    UITextType  *m_gameTitle;
    UITextType  *m_gameSystem;
    UITextType  *m_gameYear;
    UITextType  *m_gameGenre;
    UITextType  *m_gameFavourite;
    UIImageType *m_gameImage;

    QTimer      *timer;
    int m_showHashed;
};

#endif
