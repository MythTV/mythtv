#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include <qwidget.h>
#include <qdialog.h>

#include "programinfo.h"

class QLabel;
class QCheckBox;
class MythContext;

namespace libmyth
{

class InfoDialog : public QDialog
{
    Q_OBJECT
  public:
    InfoDialog(MythContext *context, ProgramInfo *pginfo, 
               QWidget *parent = 0, const char *name = 0);

  protected slots:
    void norecPressed(void);
    void reconePressed(void);
    void rectimeslotPressed(void);
    void recchannelPressed(void);
    void receveryPressed(void);

  protected:
    void hideEvent(QHideEvent *e);

  private:
    void okPressed(void);
    QLabel *getDateLabel(ProgramInfo *pginfo);
  
    QCheckBox *norec;
    QCheckBox *recone;
    QCheckBox *rectimeslot;
    QCheckBox *recchannel;
    QCheckBox *recevery;

    int programtype;
    RecordingType recordstatus;
    ProgramInfo *myinfo;

    MythContext *m_context;
};

}

#endif
