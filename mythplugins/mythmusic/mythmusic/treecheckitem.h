#ifndef TREECHECKITEM_H_
#define TREECHECKITEM_H_

#include <qlistview.h>
#include "metadata.h"

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

    Metadata *metadata;
    QString level;
};

#endif
