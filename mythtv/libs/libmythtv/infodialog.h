#ifndef INFODIALOG_H_
#define INFODIALOG_H_

#include <qwidget.h>
#include <qdialog.h>

class ProgramInfo;
class QLabel;

class InfoDialog : public QDialog
{
    Q_OBJECT
  public:
    InfoDialog(ProgramInfo *pginfo, QWidget *parent = 0, const char *name = 0);

  private:
    QLabel *getDateLabel(ProgramInfo *pginfo);
};

#endif
