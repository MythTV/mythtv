#ifndef SETRECTIME_H_
#define SETRECTIME_H_

#include <qwidget.h>
#include <qdialog.h>

class QSqlDatabase;
class QListViewItem;
class QListView;
class QLabel;
class TV;
class Scheduler;
class ProgramInfo;
class MythContext;
class QDateTimeEdit;

class SetRecTimeDialog : public QDialog
{
    Q_OBJECT
  public:
    SetRecTimeDialog(MythContext *context, ProgramInfo *rec,
                     QSqlDatabase *ldb, QWidget *parent = 0,
                     const char *name = 0);

  protected slots:
    void selected(QListViewItem *);

  protected:
    void hideEvent(QHideEvent *e);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);

    QSqlDatabase *db;

    QString fileprefix;

    QLabel *desclabel;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;

    Scheduler *sched;

    QListView *lview;

    MythContext *m_context;
    ProgramInfo *m_oldproginfo;
    ProgramInfo *m_proginfo;
    QDateTimeEdit *m_startte;
    QDateTimeEdit *m_endte;

};

#endif

