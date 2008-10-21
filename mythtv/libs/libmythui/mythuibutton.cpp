
#include "mythuibutton.h"

// C/C++ headers
#include <iostream>
using namespace std;

// Myth headers
#include "mythverbose.h"

// MythUI headers
#include "mythmainwindow.h"

MythUIButton::MythUIButton(MythUIType *parent, const QString &name)
            : MythUIType(parent, name)
{
    m_state = "active";

    connect(this, SIGNAL(TakingFocus()), this, SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), this, SLOT(Deselect()));
    connect(this, SIGNAL(Enabling()), this, SLOT(Enable()));
    connect(this, SIGNAL(Disabling()), this, SLOT(Disable()));

    SetCanTakeFocus(true);
}

MythUIButton::~MythUIButton()
{
}

void MythUIButton::SetInitialStates()
{
    m_Text = dynamic_cast<MythUIText*>(GetChild("text"));
    m_BackgroundState = dynamic_cast<MythUIStateType*>(GetChild("background"));

    if (!m_Text || !m_BackgroundState)
        VERBOSE(VB_IMPORTANT, QString("Button %1 is missing required "
                                      "elements").arg(objectName()));

    if (m_BackgroundState)
        m_BackgroundState->DisplayState(m_state);
    m_Text->SetFontState(m_state);
}

void MythUIButton::Reset()
{
    MythUIType::Reset();
}

void MythUIButton::Select()
{
    if (!IsEnabled())
        return;

    m_state = "selected";
    m_BackgroundState->DisplayState(m_state);
    m_Text->SetFontState(m_state);
}

void MythUIButton::Deselect()
{
    if (IsEnabled())
        m_state = "active";
    else
        m_state = "disabled";
    m_BackgroundState->DisplayState(m_state);
    m_Text->SetFontState(m_state);
}

void MythUIButton::Enable()
{
    m_state = "active";
    m_BackgroundState->DisplayState(m_state);
    m_Text->SetFontState(m_state);
}

void MythUIButton::Disable()
{
    m_state = "disabled";
    m_BackgroundState->DisplayState(m_state);
    m_Text->SetFontState(m_state);
}

bool MythUIButton::ParseElement(QDomElement &element)
{
    if (element.tagName() == "value")
    {
        QString msg = getFirstText(element);
        m_Text->SetText(msg);
    }
    else
        return MythUIType::ParseElement(element);

    return true;
}

bool MythUIButton::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            emit Clicked();
        else
            handled = false;
    }

    return handled;
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUIButton::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        if (IsEnabled())
            emit Clicked();
    }
}

void MythUIButton::SetText(const QString &msg)
{
    m_Text->SetText(msg);
}

void MythUIButton::CreateCopy(MythUIType *parent)
{
    MythUIButton *button = new MythUIButton(parent, objectName());
    button->CopyFrom(this);
}

void MythUIButton::CopyFrom(MythUIType *base)
{
    MythUIButton *button = dynamic_cast<MythUIButton *>(base);
    if (!button)
    {
        VERBOSE(VB_IMPORTANT,
                        "MythUIButton::CopyFrom: Dynamic cast of base failed");
        return;
    }

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

void MythUIButton::Finalize()
{
    SetInitialStates();
}
