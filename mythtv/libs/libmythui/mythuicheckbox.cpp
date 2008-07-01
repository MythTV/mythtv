
// MythUI headers
#include "mythuicheckbox.h"
#include "mythmainwindow.h"

MythUICheckBox::MythUICheckBox(MythUIType *parent, const QString &name,
                               bool doInit)
            : MythUIButton(parent, name, doInit)
{
    if (doInit)
        EnableCheck(true);
}

MythUICheckBox::~MythUICheckBox()
{
}

void MythUICheckBox::toggleCheckState()
{
    if (m_CheckState != MythUIStateType::Full)
        SetCheckState(MythUIStateType::Full);
    else
        SetCheckState(MythUIStateType::Off);
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUICheckBox::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        toggleCheckState();
    }
}

/** \brief Key event handler
 *
 *  \param event Keypress event
 */
bool MythUICheckBox::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            toggleCheckState();
        else
            handled = false;
    }

    return handled;
}

void MythUICheckBox::CreateCopy(MythUIType *parent)
{
    MythUICheckBox *checkbox = new MythUICheckBox(parent, objectName());
    checkbox->CopyFrom(this);
}

void MythUICheckBox::CopyFrom(MythUIType *base)
{
    MythUICheckBox *checkbox = dynamic_cast<MythUICheckBox *>(base);
    if (!checkbox)
        return;

    MythUIButton::CopyFrom(base);

    EnableCheck(true);
}
