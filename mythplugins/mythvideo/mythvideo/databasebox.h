#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>

#include "metadata.h"

class QSqlDatabase;
class QListViewItem;
class TreeCheckItem;
class QLabel;
class QListView;
class MythContext;

class DatabaseBox : public QDialog
{
    Q_OBJECT
  public:
    DatabaseBox(MythContext *context, QSqlDatabase *ldb, QString &paths,
                QValueList<Metadata> *playlist,
                QWidget *parent = 0, const char *name = 0);

    void Show();

  protected slots:
    void selected(QListViewItem *);

  private:
    void doSelected(QListViewItem *);

    void fillList(QListView *listview, QValueList<Metadata> *playlist );


    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    TreeCheckItem *cditem;

    QValueList<Metadata> *plist;

    MythContext *m_context;
};

#endif
