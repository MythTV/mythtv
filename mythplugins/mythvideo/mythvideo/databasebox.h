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
class MythListView;
class MythContext;

class DatabaseBox : public QDialog
{
    Q_OBJECT
  public:
    DatabaseBox(MythContext *context, QSqlDatabase *ldb,
                QValueList<Metadata> *playlist,
                QWidget *parent = 0, const char *name = 0);

    void Show();

  public slots:
    void selected(QListViewItem *);

  private:
    void doSelected(QListViewItem *);
    void fillList(MythListView *listview, QValueList<Metadata> *playlist );

    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    TreeCheckItem *cditem;

    QValueList<Metadata> *plist;

    MythContext *m_context;
    MythListView *listview;
};

#endif
