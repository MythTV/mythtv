#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>

#include "metadata.h"
#include <mythtv/mythwidgets.h>

class QSqlDatabase;
class QListViewItem;
class TreeCheckItem;
class QLabel;

class DatabaseBox : public MythDialog
{
    Q_OBJECT
  public:
    DatabaseBox(QSqlDatabase *ldb, QValueList<Metadata> *playlist,
                QWidget *parent = 0, const char *name = 0);

  public slots:
    void selected(QListViewItem *);

  private:
    void doSelected(QListViewItem *);
    void fillList(MythListView *listview, QValueList<Metadata> *playlist );

    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    TreeCheckItem *cditem;

    QValueList<Metadata> *plist;

    MythListView *listview;
};

#endif
