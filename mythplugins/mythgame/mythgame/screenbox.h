#ifndef SCREENBOX_H_
#define SCREENBOX_H_

#include <qwidget.h>
#include <qframe.h>

#include <mythtv/mythwidgets.h>

#include "selectframe.h"
#include "rominfo.h"

class QSqlDatabase;
class QListViewItem;
class TreeItem;
class QLabel;
class QListView;

class ScreenBox : public MythDialog
{
    Q_OBJECT
  public:
    ScreenBox(MythContext *context, QSqlDatabase *ldb, QString &paths,
              QWidget *parent = 0, const char *name = 0);

  protected slots:
    void setImages(QListViewItem *);
    void editSettings(QListViewItem *);
    void handleKey(QListViewItem *, int);

  private:
    void checkParent(QListViewItem *);

    void fillList(QListView *listview, QString &paths);
    void fillNextLevel(QString level, int num, QString querystr, 
                       QString matchstr, QStringList::Iterator line,
                       QStringList lines, TreeItem *parent);

    QPixmap getPixmap(QString &level);

    QSqlDatabase *db;

    SelectFrame *PicFrame;
    QString leafLevel;
    QLabel* mGameLabel;
};

#endif
