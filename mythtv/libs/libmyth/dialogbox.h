#ifndef DIALOGBOX_H_
#define DIALOGBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qcheckbox.h>

class QVBoxLayout;
class QButtonGroup;
class QString;

class DialogBox : public QDialog
{
    Q_OBJECT
  public:
    DialogBox(const QString &text, const char *checkboxtext = 0,
              QWidget *parent = 0, const char *name = 0);

    void AddButton(const QString &title);
    void Show();

    bool getCheckBoxState(void) {  if (checkbox) return checkbox->isChecked();
                                   return false; }
  protected slots:
    void buttonPressed(int which);

  private:
    QVBoxLayout *box;
    QButtonGroup *buttongroup;

    QCheckBox *checkbox;
};

#endif
