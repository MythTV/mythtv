#ifndef TREEITEM_H_
#define TREEITEM_H_

#include <qlistview.h>
#include "rominfo.h"

class QPixmap;

class TreeItem : public QListViewItem
{
  public:
    TreeItem(QListView *parent, QString &ltext, const QString &llevel, 
             RomInfo *rinfo);
    TreeItem(TreeItem *parent, QString &ltext, const QString &llevel, 
             RomInfo *rinfo);

   ~TreeItem(void) { if (rominfo) delete rominfo; }

    RomInfo *getRomInfo(void) { return rominfo; }
    QString getLevel(void) { return level; }

    void setOpen(bool o);

  private:
    void pickPixmap();
    QString getClause(QString field);

    static void setupPixmaps(void);
    static QPixmap *scalePixmap(const char **xpmdata, float wmult, float hmult);

    RomInfo *rominfo;
    QString level;

    static bool pixmapsSet;
    static QPixmap *system;
    static QPixmap *game;
    static QPixmap *genre;
};

#endif
