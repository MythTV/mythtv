#ifndef TREECHECKITEM_H_
#define TREECHECKITEM_H_

#include <qlistview.h>
#include "metadata.h"

class QPixmap;

class TreeCheckItem : public QCheckListItem
{
  public:
    TreeCheckItem(QListView *parent, QString &ltext, 
                  const QString &llevel, Metadata *mdata);
    TreeCheckItem(TreeCheckItem *parent, QString &ltext, 
                  const QString &llevel, Metadata *mdata);

   ~TreeCheckItem(void) { if (metadata) delete metadata; }

    Metadata *getMetadata(void) { return metadata; }
    QString getLevel(void) { return level; }

  private:
    void pickPixmap();

    static void setupPixmaps();
    static QPixmap *scalePixmap(const char **xpmdata, float wmult, float hmult);

    Metadata *metadata;
    QString level;

    static bool pixmapsSet;
    static QPixmap *artist;
    static QPixmap *album;
    static QPixmap *track;
    static QPixmap *catalog;
    static QPixmap *cd;
};

#endif
