#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qstringlist.h>

#include "rominfo.h"

#include <mythtv/mythwidgets.h>

class QSqlDatabase;
class QListViewItem;
class TreeItem;
class QLabel;
class QListView;

class DatabaseBox : public MythDialog
{
    Q_OBJECT
  public:
    DatabaseBox(MythContext *context, QSqlDatabase *ldb, QString &paths,
                QWidget *parent = 0, const char *name = 0);

  protected slots:
    void handleKey(QListViewItem *, int);
    void selected(QListViewItem *);
    void editSettings(QListViewItem *);

  private:
    void doSelected(QListViewItem *);
    void checkParent(QListViewItem *);

    void fillList(QListView *listview, QString &paths);
    void fillNextLevel(QString level, int num, QString querystr, 
                       QString matchstr, QStringList::Iterator line,
                       QStringList lines, TreeItem *parent);

    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    QValueList<RomInfo> *rlist;
};

#endif
