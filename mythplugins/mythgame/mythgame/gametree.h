#ifndef GAMETREE_H_
#define GAMETREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>
#include <qptrvector.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>

#include "rominfo.h"

class GameTreeRoot;
class GameTreeItem;

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
    void edit(void);
    void toggleFavorite(void);

    GenericTree           *m_gameTree;
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
