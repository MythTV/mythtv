#ifndef TREECHECKITEM_H_
#define TREECHECKITEM_H_

#include <qlistview.h>
#include "metadata.h"

class QPixmap;
class MythContext;

class TreeCheckItem : public QListViewItem
{
  public:
    TreeCheckItem(MythContext *context, QListView *parent, QString &ltext, 
                  const QString &llevel, Metadata *mdata);
    TreeCheckItem(MythContext *context, TreeCheckItem *parent, QString &ltext, 
                  const QString &llevel, Metadata *mdata);

   ~TreeCheckItem(void) { if (metadata) delete metadata; }

    Metadata *getMetadata(void) { return metadata; }
    QString getLevel(void) { return level; }


  private:
    void pickPixmap();

      virtual void paintCell(QPainter* p, const QColorGroup& cg,int column, int width, int align);
    static void setupPixmaps(MythContext *context);
    static QPixmap *scalePixmap(const char **xpmdata, float wmult, float hmult);

    Metadata *metadata;
    QString level;

    static bool pixmapsSet;
    static QPixmap *artist;
    static QPixmap *album;
    static QPixmap *track;
    static QPixmap *catalog;
    static QPixmap *cd;

    MythContext *m_context;
};

#endif
