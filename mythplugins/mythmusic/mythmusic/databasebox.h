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
    DatabaseBox(QSqlDatabase *ldb, QString &paths, 
                QValueList<Metadata> *playlist,
                QWidget *parent = 0, const char *name = 0);

  protected slots:
    void selected(QListViewItem *);

  private:
    void doSelected(QListViewItem *);
    void checkParent(QListViewItem *);

    void fillCD(void);

    void fillList(QListView *listview, QString &paths);
    void fillNextLevel(QString level, int num, QString querystr, 
                       QString matchstr, QStringList::Iterator line,
                       QStringList lines, TreeCheckItem *parent);

    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    TreeCheckItem *cditem;

    QValueList<Metadata> *plist;
};

#endif
