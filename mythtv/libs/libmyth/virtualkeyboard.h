#ifndef VIRTUALKEYBOARD_H_
#define VIRTUALKEYBOARD_H_

#include <qstring.h>
#include <qimage.h>
#include "xmlparse.h"

class MythDialog;

class VirtualKeyboard : public MythThemedDialog
{
    Q_OBJECT
  public:
    VirtualKeyboard(MythMainWindow *parent, 
                    QWidget *parentEdit,
                    const char *name = 0,
                    bool setsize = true);
    ~VirtualKeyboard();

  public slots:
    void switchLayout(QString language);
    virtual void show();
    virtual void hide();

  signals:

  protected:

  protected slots:
    virtual void keyPressEvent(QKeyEvent *e);

  private:
    UIKeyboardType *m_keyboard;
    QWidget        *m_parentEdit;
    int            m_popupWidth;
    int            m_popupHeight;
};

#endif
