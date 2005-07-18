#ifndef GAMETREE_H_
#define GAMETREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>
#include <qptrvector.h>

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
      : m_root(root)
      , m_romInfo(0)
      , m_depth(0)
      , m_isFilled(false)
    {
        info_popup = NULL;
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
    QString getFillSql() const;

    void setFilled(bool isFilled)     { m_isFilled = isFilled; }

    GameTreeItem* createChild(QSqlQuery *query) const;
    void showGameInfo(RomInfo *rom);

  protected slots:
    void closeGameInfo(void);

  private:
    QButton *OKButton;
    MythPopupBox *info_popup;
    GameTreeRoot* m_root;
    RomInfo* m_romInfo;
    unsigned m_depth;
    bool m_isFilled;
};

class GameTree : public MythThemedDialog
{
    Q_OBJECT

  public:
    typedef QValueVector<int> IntVector;

    GameTree(MythMainWindow *parent, QString windowName,
             QString themeFilename, const char *name = 0);
   ~GameTree();

  public slots:
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);

  protected:
    void keyPressEvent(QKeyEvent *e);

  private:
    void wireUpTheme(void);
    void fillNode(GenericTree *node);
    void showInfo(void);
    void toggleFavorite(void);

    GenericTree           *m_gameTree;
    GenericTree		  *m_favouriteNode;
    UIManagedTreeListType *m_gameTreeUI;

    QValueVector<GameTreeRoot *> m_gameTreeRoots;
    QValueVector<GameTreeItem *> m_gameTreeItems;

    UITextType  *m_gameTitle;
    UITextType  *m_gameSystem;
    UITextType  *m_gameYear;
    UITextType  *m_gameGenre;
    UITextType  *m_gameFavourite;
    UIImageType *m_gameImage;
};

#endif
