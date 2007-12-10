#ifndef DIALOGBOX_H_
#define DIALOGBOX_H_

#include <qcheckbox.h>

#include "mythdialogs.h"
#include "compat.h" // to undef DialogBox

class QVBoxLayout;
class QButtonGroup;
class QString;

class MPUBLIC DialogBox : public MythDialog
{
    Q_OBJECT
  public:
    DialogBox(MythMainWindow *parent, const QString &text, 
              const char *checkboxtext = 0, const char *name = 0);

    void AddButton(const QString &title);

    bool getCheckBoxState(void) {  if (checkbox) return checkbox->isChecked();
                                   return false; }

  protected slots:
    void buttonPressed(int which);

  protected:
    ~DialogBox() {} // use deleteLater() for thread safety

  private:
    QVBoxLayout *box;
    QButtonGroup *buttongroup;

    QCheckBox *checkbox;
};

#endif
