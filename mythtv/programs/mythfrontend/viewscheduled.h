#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qwidget.h>
#include <qdialog.h>

class QSqlDatabase;
class QListViewItem;
class QListView;
class QLabel;
class TV;
class Scheduler;

class ViewScheduled : public QDialog
{
    Q_OBJECT
  public:
    ViewScheduled(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                  QWidget *parent = 0, const char *name = 0);

    void Show();
  
  protected slots:
    void selected(QListViewItem *);
    void changed(QListViewItem *);

  private:
    void FillList(void);

    QSqlDatabase *db;
    TV *tv;

    QString fileprefix;

    QLabel *desclabel;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;

    Scheduler *sched;

    QListView *listview;
};

#endif
