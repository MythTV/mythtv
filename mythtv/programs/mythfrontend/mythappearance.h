#ifndef MYTHAPPEARANCE_H
#define MYTHAPPEARANCE_H

#include <qstringlist.h>
#include <qstring.h>
#include <qptrlist.h>

#include "mythscreentype.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuiimage.h"
//#include "mythgesture.h"
#include "mythdialogbox.h"

class XMLParse;
class MythAppearance : public MythScreenType
{
    Q_OBJECT

  public:

    MythAppearance(MythScreenStack *parent, const char *name);
    ~MythAppearance();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QCustomEvent *event);

  protected: 
    void doMenu();
  private:
    int m_x_offset;
    int m_y_offset;
    bool m_whicharrow;
    bool m_coarsefine;
    bool m_changed;
    int m_fine;
    int m_coarse;
    int m_change;
    int m_topleftarrow_x;
    int m_topleftarrow_y;
    int m_bottomrightarrow_x;
    int m_bottomrightarrow_y;
    int m_arrowsize_x;
    int m_arrowsize_y;
    int m_screenwidth;
    int m_screenheight;
    int m_xsize;
    int m_ysize;
    int m_xoffset;
    int m_yoffset;
    int m_xoffset_old;
    int m_yoffset_old;

    void getSettings();
    void getScreenInfo();

    QRect m_menuRect;
    QRect m_arrowsRect;

    MythUIImage *m_topleftarrow;
    MythUIImage *m_bottomrightarrow;
    MythUIText *m_size;
    MythUIText *m_offsets;
    MythUIText *m_changeamount;
    MythUIButton *OKButton;
    MythUIButton *updateButton;
    MythDialogBox *m_menuPopup;

    void moveUp();
    void moveDown();
    void moveLeft();
    void moveRight();
    void swapArrows();
    void wireUpTheme();
    void updateScreen();
    void updateSettings();
    void anythingChanged();
    void setContext(int context);

    void slotSaveSettings();
    void slotChangeCoarseFine();
    void closeMenu();
    void slotResetSettings();
};

#endif
