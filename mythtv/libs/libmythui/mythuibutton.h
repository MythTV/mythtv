#ifndef MYTHUI_BUTTON_H_
#define MYTHUI_BUTTON_H_

#include <qstring.h>

#include "mythuitype.h"
#include "mythuistateimage.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythfontproperties.h"

// A button is a:
// - multi-state background image (active/selected/etc)
// - Text (with a set of properties)
// - optional checkbox (dual/tri state)
//
// Creator class _must_ create at least a 'Normal' state (button background and
// font properties).
class MythUIButton : public MythUIType
{
  public:
    enum StateType { None = 0, Normal, Disabled, Active, Selected, 
                     SelectedInactive };

    MythUIButton(MythUIType *parent, const char *name);
   ~MythUIButton();

    void SetBackgroundImage(StateType state, MythImage *image);
    void SetCheckImage(MythUIStateImage::StateType state, MythImage *image);
    void SetTextRect(const QRect &textRect);
    void SetFont(StateType state, MythFontProperties &prop);
    void SetButtonImage(MythImage *image);
    void SetRightArrowImage(MythImage *image);
    void SetPaddingMargin(int margin);

    void SetText(const QString &msg, int textFlags = -1);

    void SelectState(StateType newState);
    void SetCheckState(MythUIStateImage::StateType state);
    void EnableCheck(bool enable);
    void EnableRightArrow(bool enable);

    void SetupPlacement(void);

  protected:
    MythUIStateImage *m_BackgroundImage;
    MythUIText *m_Text;
    MythUIStateImage *m_CheckImage;
    MythUIImage *m_ButtonImage;
    MythUIImage *m_ArrowImage;

    QMap<int, MythFontProperties> m_FontProps;

    StateType m_State;

    QRect m_TextRect;
    int m_PaddingMargin;
};

#endif
