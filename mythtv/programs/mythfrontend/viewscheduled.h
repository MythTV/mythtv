#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qwidget.h>

#include "libmyth/mythwidgets.h"

class QSqlDatabase;
class QListViewItem;
class QLabel;
class ProgramInfo;

class ViewScheduled : public MythDialog
{
    Q_OBJECT
  public:
    ViewScheduled(QSqlDatabase *ldb, QWidget *parent = 0, const char *name = 0);

  protected slots:
    void selected(QListViewItem *);
    void changed(QListViewItem *);

  private:
    void FillList(void);
    void handleConflicting(ProgramInfo *rec);
    void handleNotRecording(ProgramInfo *rec);
    void chooseConflictingProgram(ProgramInfo *rec);

    QSqlDatabase *db;

    QLabel *desclabel;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;

    MythListView *listview;
};

#endif
