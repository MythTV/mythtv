#ifndef SCREENWIZARD_H
#define SCREENWIZARD_H

// Qt
#include <QString>
#include <QStringList>

// MythTV
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuishape.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitype.h"

class XMLParse;
class ScreenWizard : public MythScreenType
{
    Q_OBJECT

  public:

    ScreenWizard(MythScreenStack *parent, const char *name);
    ~ScreenWizard() override = default;

    void SetInitialSettings(int _x, int _y, int _w, int _h);
    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

  protected:
    void doMenu();
    void doExit();
  private:
    bool           m_whichCorner   { true };
    bool           m_coarseFine    { false }; // fine adjustments by default
    bool           m_changed       { false };
    int            m_fine          { 1 };     // fine moves corners by one pixel
    int            m_coarse        { 10 };    // coarse moves corners by ten pixels
    int            m_change        { m_fine };
    int            m_topLeftX      { 0 };
    int            m_topLeftY      { 0 };
    int            m_bottomRightX  { 0 };
    int            m_bottomRightY  { 0 };
    int            m_screenWidth   { 0 };
    int            m_screenHeight  { 0 };
    int            m_xSize         { 0 };
    int            m_ySize         { 0 };
    int            m_xOffset       { 0 };
    int            m_yOffset       { 0 };

    QRect          m_menuRect;
    QRect          m_arrowsRect;

    MythUIShape   *m_blackout     { nullptr };
    MythUIImage   *m_preview      { nullptr };
    MythUIText    *m_size         { nullptr };
    MythUIText    *m_offsets      { nullptr };
    MythUIText    *m_changeAmount { nullptr };
    MythDialogBox *m_menuPopup    { nullptr };

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
    bool anythingChanged() const;

    void slotSaveSettings() const;
    void slotChangeCoarseFine();
    static void slotResetSettings();
};

#endif
