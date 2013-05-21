#ifndef SCREENWIZARD_H
#define SCREENWIZARD_H

#include <QStringList>
#include <QString>

#include "mythscreentype.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuiimage.h"
#include "mythdialogbox.h"
#include "mythuishape.h"
#include "mythuitype.h"

class XMLParse;
class ScreenWizard : public MythScreenType
{
    Q_OBJECT

  public:

    ScreenWizard(MythScreenStack *parent, const char *name);
    ~ScreenWizard();

    void SetInitialSettings(int _x, int _y, int _w, int _h);
    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

  protected:
    void doMenu();
    void doExit();
  private:
    bool m_whichcorner;
    bool m_coarsefine;
    bool m_changed;
    int m_fine;
    int m_coarse;
    int m_change;
    int m_topleft_x;
    int m_topleft_y;
    int m_bottomright_x;
    int m_bottomright_y;
    int m_screenwidth;
    int m_screenheight;
    int m_xsize;
    int m_ysize;
    int m_xoffset;
    int m_yoffset;

    QRect m_menuRect;
    QRect m_arrowsRect;

    MythUIShape *m_blackout;
    MythUIImage *m_preview;
    MythUIText *m_size;
    MythUIText *m_offsets;
    MythUIText *m_changeamount;
    MythDialogBox *m_menuPopup;

    bool moveTLUp(void);
    bool moveTLDown(void);
    bool moveTLLeft(void);
    bool moveTLRight(void);

    bool moveBRUp(void);
    bool moveBRDown(void);
    bool moveBRLeft(void);
    bool moveBRRight(void);

    void wireUpTheme();
    void updateScreen();
    bool anythingChanged();

    void slotSaveSettings();
    void slotChangeCoarseFine();
    void slotResetSettings();
};

#endif
