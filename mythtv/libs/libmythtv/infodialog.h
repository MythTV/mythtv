#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include <qwidget.h>

#include "programinfo.h"
#include "mythwidgets.h"

class QLabel;
class QCheckBox;
class QListView;
class QListViewItem;

class InfoDialog : public MythDialog
{
    Q_OBJECT
  public:
    InfoDialog(ProgramInfo *pginfo, QWidget *parent = 0, const char *name = 0);

  protected slots:
    void selected(QListViewItem *);

  protected:
    void hideEvent(QHideEvent *e);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);
  
    int programtype;
    ScheduledRecording::RecordingType recordstatus;
    ProgramInfo *myinfo;

    MythListView *lview;
};

#endif
