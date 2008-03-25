#ifndef DIALOGBOX_H_
#define DIALOGBOX_H_

#include <QObject>
#include <QCheckBox>
#include <Q3ButtonGroup>
#include <Q3VBoxLayout>

#include "mythdialogs.h"
#include "compat.h" // to undef DialogBox

class MPUBLIC DialogBox : public MythDialog
{
    Q_OBJECT
  public:
    DialogBox(MythMainWindow *parent, const QString &text, 
              const char *checkboxtext = NULL, const char *name = NULL);

    void AddButton(const QString &title);

    bool getCheckBoxState(void) {  if (checkbox) return checkbox->isChecked();
                                   return false; }

  protected slots:
    void buttonPressed(int which);

  protected:
    ~DialogBox() {} // use deleteLater() for thread safety

  private:
    Q3VBoxLayout *box;
    Q3ButtonGroup *buttongroup;

    QCheckBox *checkbox;
};

#endif
