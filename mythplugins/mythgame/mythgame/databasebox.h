#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>

#include "rominfo.h"

class QSqlDatabase;
class QListViewItem;
class TreeItem;
class QLabel;
class QListView;

class DatabaseBox : public QDialog
{
    Q_OBJECT
  public:
    DatabaseBox(QSqlDatabase *ldb, QString &paths,
                QValueList<RomInfo> *romlist,
                QWidget *parent = 0, const char *name = 0);

    void Show();

  protected slots:
    void selected(QListViewItem *);

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
