#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qwidget.h>
#include <qdialog.h>

class QSqlDatabase;
class QListViewItem;
class MyListView;
class QLabel;
class ProgramInfo;
class MythContext;

class ViewScheduled : public QDialog
{
    Q_OBJECT
  public:
    ViewScheduled(MythContext *context, QSqlDatabase *ldb, QWidget *parent = 0, 
                  const char *name = 0);

    void Show();
  
  protected slots:
    void selected(QListViewItem *);
    void changed(QListViewItem *);

  private:
    void FillList(void);
    void handleConflicting(ProgramInfo *rec);
    void handleNotRecording(ProgramInfo *rec);

    QSqlDatabase *db;

    QLabel *desclabel;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;

    MyListView *listview;

    MythContext *m_context;

    float wmult, hmult;
};

#endif
