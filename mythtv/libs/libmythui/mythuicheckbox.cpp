
// Own header
#include "mythuicheckbox.h"

// Myth headers
#include "libmythbase/mythlogging.h"

// MythUI headers
#include "mythmainwindow.h"
#include "mythgesture.h"

MythUICheckBox::MythUICheckBox(MythUIType *parent, const QString &name)
    : MythUIType(parent, name)
{
    connect(this, &MythUIType::TakingFocus, this, &MythUICheckBox::Select);
    connect(this, &MythUIType::LosingFocus, this, &MythUICheckBox::Deselect);
    connect(this, &MythUIType::Enabling, this, &MythUICheckBox::Enable);
    connect(this, &MythUIType::Disabling, this, &MythUICheckBox::Disable);

    SetCanTakeFocus(true);
}

void MythUICheckBox::SetInitialStates()
{
    m_backgroundState = dynamic_cast<MythUIStateType *>(GetChild("background"));
    m_checkState = dynamic_cast<MythUIStateType *>(GetChild("checkstate"));

    if (!m_checkState || !m_backgroundState)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Checkbox %1 is missing required elements")
            .arg(objectName()));
    }

    if (m_checkState)
        m_checkState->DisplayState(m_currentCheckState);

    if (m_backgroundState)
        m_backgroundState->DisplayState(m_state);
}


void MythUICheckBox::toggleCheckState()
{
    bool onOff = false;

    if (m_currentCheckState != MythUIStateType::Full)
    {
        m_currentCheckState = MythUIStateType::Full;
        onOff = true;
    }
    else
    {
        m_currentCheckState = MythUIStateType::Off;
        onOff = false;
    }

    if (m_checkState)
        m_checkState->DisplayState(m_currentCheckState);

    emit DependChanged(!onOff);
    emit toggled(onOff);
    emit valueChanged();
}

void MythUICheckBox::SetCheckState(MythUIStateType::StateType state)
{
    m_currentCheckState = state;
    if (m_checkState)
        m_checkState->DisplayState(state);

    if (state == MythUIStateType::Off)
        emit DependChanged(true);
    else
        emit DependChanged(false);
    emit valueChanged();
}

void MythUICheckBox::SetCheckState(bool onOff)
{
    if (onOff)
    {
        m_currentCheckState = MythUIStateType::Full;
    }
    else
    {
        m_currentCheckState = MythUIStateType::Off;
    }

    if (m_checkState)
        m_checkState->DisplayState(m_currentCheckState);

    emit toggled(onOff);
    emit DependChanged(!onOff);
    emit valueChanged();
}

MythUIStateType::StateType MythUICheckBox::GetCheckState() const
{
    return m_currentCheckState;
}

bool MythUICheckBox::GetBooleanCheckState() const
{
    return m_currentCheckState == MythUIStateType::Full;
}

void MythUICheckBox::Select()
{
    if (!IsEnabled())
        return;

    m_state = "selected";
    if (m_backgroundState)
        m_backgroundState->DisplayState(m_state);
}

void MythUICheckBox::Deselect()
{
    if (IsEnabled())
        m_state = "active";
    else
        m_state = "disabled";

    if (m_backgroundState)
        m_backgroundState->DisplayState(m_state);
}

void MythUICheckBox::Enable()
{
    m_state = "active";
    if (m_backgroundState)
        m_backgroundState->DisplayState(m_state);
}

void MythUICheckBox::Disable()
{
    m_state = "disabled";
    if (m_backgroundState)
        m_backgroundState->DisplayState(m_state);
}

/** \brief Mouse click/movement handler, receives mouse gesture events from the
 *         QCoreApplication event loop. Should not be used directly.
 *
 *  \param event Mouse event
 */
bool MythUICheckBox::gestureEvent(MythGestureEvent *event)
{
    if (event->GetGesture() == MythGestureEvent::Click)
    {
        if (IsEnabled())
        {
            toggleCheckState();
            return true;
        }
    }

    return false;
}

/** \brief Key event handler
 *
 *  \param event Keypress event
 */
bool MythUICheckBox::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "SELECT")
            toggleCheckState();
        else
            handled = false;
    }

    return handled;
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUICheckBox::CreateCopy(MythUIType *parent)
{
    auto *checkbox = new MythUICheckBox(parent, objectName());
    checkbox->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUICheckBox::CopyFrom(MythUIType *base)
{
    auto *button = dynamic_cast<MythUICheckBox *>(base);

    if (!button)
    {
        LOG(VB_GENERAL, LOG_ERR, "Dynamic cast of base failed");
        return;
    }

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUICheckBox::Finalize()
{
    SetInitialStates();
}
