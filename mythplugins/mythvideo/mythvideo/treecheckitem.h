#ifndef TREECHECKITEM_H_
#define TREECHECKITEM_H_

#include <qlistview.h>
#include "metadata.h"

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
    virtual void paintCell(QPainter* p, const QColorGroup& cg,int column, 
                           int width, int align);

    Metadata *metadata;
    QString level;

    MythContext *m_context;
};

#endif
