#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include <qwidget.h>
#include <qdialog.h>

#include "programinfo.h"

class QLabel;
class QCheckBox;
class MythContext;
class QListView;
class QListViewItem;

namespace libmyth
{

class InfoDialog : public QDialog
{
    Q_OBJECT
  public:
    InfoDialog(MythContext *context, ProgramInfo *pginfo, 
               QWidget *parent = 0, const char *name = 0);

  protected slots:
    void selected(QListViewItem *);

  protected:
    void hideEvent(QHideEvent *e);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);
  
    int programtype;
    RecordingType recordstatus;
    ProgramInfo *myinfo;

    MythContext *m_context;

    QListView *lview;
};

}

#endif
