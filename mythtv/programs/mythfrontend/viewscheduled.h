#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qwidget.h>
#include <qdialog.h>

class QSqlDatabase;
class QListViewItem;
class MyListView;
class QLabel;
class TV;
class Scheduler;
class ProgramInfo;
class MythContext;

class ViewScheduled : public QDialog
{
    Q_OBJECT
  public:
    ViewScheduled(MythContext *context, TV *ltv, QSqlDatabase *ldb, 
                  QWidget *parent = 0, const char *name = 0);

    void Show();
  
  protected slots:
    void selected(QListViewItem *);
    void changed(QListViewItem *);

  private:
    void FillList(void);
    void handleConflicting(ProgramInfo *rec);
    void handleNotRecording(ProgramInfo *rec);

    QSqlDatabase *db;
    TV *tv;

    QString fileprefix;

    QLabel *desclabel;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;

    Scheduler *sched;

    MyListView *listview;

    MythContext *m_context;
};

#endif
