#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include "mythwidgets.h"

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
   ~InfoDialog();           

  protected slots:
    void selected(QListViewItem *);
    void advancedEdit(QListViewItem *);
    void numberPress(QListViewItem *, int);

  protected:
    bool eventFilter(QObject *o, QEvent *e);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);

    int programtype;
    RecordingType recordstatus;
    ProgramInfo *myinfo;

    MythListView *lview;
};

#endif
