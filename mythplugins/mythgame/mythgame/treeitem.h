#ifndef TREEITEM_H_
#define TREEITEM_H_

#include <qlistview.h>
#include "rominfo.h"

class QPixmap;
class MythContext;

class TreeItem : public QListViewItem
{
  public:
    TreeItem(MythContext *context, QListView *parent, QString &ltext,
             const QString &llevel, RomInfo *rinfo);
    TreeItem(MythContext *context, TreeItem *parent, QString &ltext,
             const QString &llevel, RomInfo *rinfo);

   ~TreeItem(void) { if (rominfo) delete rominfo; }

    RomInfo *getRomInfo(void) { return rominfo; }
    QString getLevel(void) { return level; }

  private:
    void pickPixmap();

    static void setupPixmaps(MythContext *context);
    static QPixmap *scalePixmap(const char **xpmdata, float wmult, float hmult);

    RomInfo *rominfo;
    QString level;

    static bool pixmapsSet;
    static QPixmap *system;
    static QPixmap *game;
    static QPixmap *genre;

    MythContext *m_context;
};

#endif
