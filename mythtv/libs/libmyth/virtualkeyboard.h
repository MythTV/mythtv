#ifndef VIRTUALKEYBOARD_H_
#define VIRTUALKEYBOARD_H_

#include <qstring.h>
#include <qimage.h>

#include "mythdialogs.h"

/// Preferred position to place virtual keyboard popup.
enum PopupPosition 
{
    VK_POSABOVEEDIT = 1,
    VK_POSBELOWEDIT,
    VK_POSTOPDIALOG,
    VK_POSBOTTOMDIALOG,
    VK_POSCENTERDIALOG
};

class MPUBLIC VirtualKeyboard : public MythThemedDialog
{
    Q_OBJECT
  public:
    VirtualKeyboard(MythMainWindow *parent, 
                    QWidget *parentEdit,
                    const char *name = 0,
                    bool setsize = true);

  public slots:
    virtual void SwitchLayout(const QString &language);
    virtual void Show(void);
    virtual void hide();

    virtual void deleteLater(void);

  protected slots:
    virtual void keyPressEvent(QKeyEvent *e);

  protected:
    void Teardown(void);
    ~VirtualKeyboard(); // use deleteLater() instead for thread safety

  private:
    UIKeyboardType *m_keyboard;
    QWidget        *m_parentEdit;
    int             m_popupWidth;
    int             m_popupHeight;
};

#endif
