#ifndef MENUBOX_H_
#define MENUBOX_H_

#include <qwidget.h>
#include <qdialog.h>

class QVBoxLayout;
class QButtonGroup;

class MenuBox : public QDialog
{
    Q_OBJECT
  public:
    MenuBox(const char *text, QWidget *parent = 0, const char *name = 0);

    void AddButton(const char *title);
    void Show();

  protected slots:
    void buttonPressed(int which);

  private:
    QVBoxLayout *box;
    QButtonGroup *buttongroup;
};

#endif
