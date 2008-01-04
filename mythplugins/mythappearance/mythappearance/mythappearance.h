#ifndef MYTHAPPEARANCE_H
#define MYTHAPPEARANCE_H

#include <qstringlist.h>
#include <qstring.h>
#include <qptrlist.h>
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

class XMLParse;
class MythAppearance : public MythThemedDialog
{
Q_OBJECT
public:

    MythAppearance(MythMainWindow *parent, QString windowName,
        QString themeFilename, const char *name = 0);
    ~MythAppearance();

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

    UIImageType *m_topleftarrow;
    UIImageType *m_bottomrightarrow;
    UITextType *m_size;
    UITextType *m_offsets;
    UITextType *m_changeamount;

    void moveUp();
    void moveDown();
    void moveLeft();
    void moveRight();
    void swapArrows();
    void wireUpTheme();
    void updateScreen();
    void updateSettings();
    void keyPressEvent(QKeyEvent *e);
    void anythingChanged();
    MythPopupBox *menuPopup;
    QButton *OKButton;
    QButton *updateButton;

protected slots:
    void slotSaveSettings();
    void slotChangeCoarseFine();
    void closeMenu();
    void slotResetSettings();
};

#endif
