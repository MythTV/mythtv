#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include <qwidget.h>
#include <qdialog.h>

class ProgramInfo;
class QLabel;
class QCheckBox;

class InfoDialog : public QDialog
{
    Q_OBJECT
  public:
    InfoDialog(ProgramInfo *pginfo, QWidget *parent = 0, const char *name = 0);

  protected slots:
    void norecPressed(void);
    void reconePressed(void);
    void rectimeslotPressed(void);
    void receveryPressed(void);
    void okPressed(void);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);
  
    QCheckBox *norec;
    QCheckBox *recone;
    QCheckBox *rectimeslot;
    QCheckBox *recevery;

    int programtype;
    int recordstatus;
    ProgramInfo *myinfo;
};

#endif
