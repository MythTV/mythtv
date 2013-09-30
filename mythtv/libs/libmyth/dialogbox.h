#ifndef DIALOGBOX_H_
#define DIALOGBOX_H_

#include <QObject>
#include <QCheckBox>

#include "mythdialogs.h"
#include "mythexp.h"
#include "compat.h" // to undef DialogBox
class QVBoxLayout;
class QButtonGroup;

class MPUBLIC DialogBox : public MythDialog
{
    Q_OBJECT
  public:
    DialogBox(MythMainWindow *parent, const QString &text, 
              const char *checkboxtext = NULL, const char *name = NULL) MDEPRECATED;

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
