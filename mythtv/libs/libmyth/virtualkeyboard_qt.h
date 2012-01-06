#ifndef VIRTUALKEYBOARDQT_H_
#define VIRTUALKEYBOARDQT_H_

#include "mythdialogs.h"
#include "mythexp.h"
#include "mythmainwindow.h"

class QKeyEvent;
class UIKeyboardType;

/// Preferred position to place virtual keyboard popup.
enum PopupPositionQt 
{
    VKQT_POSABOVEEDIT = 1,
    VKQT_POSBELOWEDIT,
    VKQT_POSTOPDIALOG,
    VKQT_POSBOTTOMDIALOG,
    VKQT_POSCENTERDIALOG
};

class MPUBLIC VirtualKeyboardQt : public MythThemedDialog
{
    Q_OBJECT
  public:
    VirtualKeyboardQt(MythMainWindow *parent, 
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
    ~VirtualKeyboardQt(); // use deleteLater() instead for thread safety

  private:
    UIKeyboardType *m_keyboard;
    QWidget        *m_parentEdit;
    int             m_popupWidth;
    int             m_popupHeight;
};

#endif
