#ifndef GAMETREE_H_
#define GAMETREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>
#include <qptrvector.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>

#include "rominfo.h"

class GameTreeItem
{
  public:
    GameTreeItem(const QString &llevel, RomInfo *rinfo)
    {
        level = llevel; rominfo = rinfo;  
        if (level == "gamename")
        {
            isleaf = true;
            filled = true;
        }
        else
        {
            isleaf = false;
            filled = false;
        }
    }
    GameTreeItem(const GameTreeItem &other)
    {
        level = other.level;
        rominfo = other.rominfo;
        isleaf = other.isleaf;
        filled = other.filled;
    }
    GameTreeItem()
    {
        level = "invalid"; rominfo = NULL; isleaf = false; filled = false;
    }

   ~GameTreeItem()
    {
        if (rominfo)
            delete rominfo;
    }

    QString level;
    RomInfo *rominfo;
    bool isleaf;
    bool filled;
};

class GameTree : public MythThemedDialog
{
    Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;

    GameTree(MythMainWindow *parent, QString window_name, 
             QString theme_filename, QString &paths, const char *name = 0);
   ~GameTree();

    void buildGameList();

  public slots:

    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);

  protected:
    void keyPressEvent(QKeyEvent *e);

  private:
    void wireUpTheme(void);
    void goRight(void);
    void FillListFrom(GameTreeItem *item);
    void edit(void);
    void toggleFavorite(void);

    QString getClause(QString field, GameTreeItem *item);

    UIManagedTreeListType *game_tree_list;
    GenericTree           *game_tree_root;
    GenericTree           *game_tree_data;

    QValueVector<GameTreeItem *> treeList;

    GameTreeItem *curitem;

    QString m_paths;
    QStringList m_pathlist;
    QString showfavs;

    UITextType  *game_title;
    UITextType  *game_system;
    UITextType  *game_year;
    UITextType  *game_genre;
    UITextType  *game_favorite;
    UIImageType *game_shot;
};

#endif
