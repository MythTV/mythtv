#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include "mythwidgets.h"
#include "scheduledrecording.h"

class QLabel;
class QCheckBox;
class QListView;
class QListViewItem;
class ProgramInfo;
class QWidget;

class InfoDialog : public MythDialog
{
    Q_OBJECT
  public:
    InfoDialog(ProgramInfo *pginfo, MythMainWindow *parent, 
               const char *name = 0);

  protected slots:
    void selected(QListViewItem *);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);

    int programtype;
    ScheduledRecording::RecordingType recordstatus;
    ProgramInfo *myinfo;

    MythListView *lview;
};

#endif
