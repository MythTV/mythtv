#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qstringlist.h>

#include "rominfo.h"

#include <mythtv/mythdialogs.h>

class QSqlDatabase;
class QListViewItem;
class TreeItem;
class QLabel;
class QListView;

class DatabaseBox : public MythDialog
{
    Q_OBJECT
  public:
    DatabaseBox(QSqlDatabase *ldb, QString &paths,
                MythMainWindow *parent, const char *name = 0);

  protected slots:
    void handleKey(QListViewItem *, int);
    void selected(QListViewItem *);
    void editSettings(QListViewItem *);

  private:
    void doSelected(QListViewItem *);
    void checkParent(QListViewItem *);

    void fillList(QListView *listview, QString &paths);

    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    QValueList<RomInfo> *rlist;
};

#endif
