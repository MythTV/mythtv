#include <chrono>

#include "mythuibutton.h"

// QT
#include <QTimer>
#include <QDomDocument>
#include <QCoreApplication>

// Myth headers
#include "libmythbase/mythlogging.h"

// MythUI headers
#include "mythgesture.h"
#include "mythmainwindow.h"
#include "mythuigroup.h"
#include "mythuistatetype.h"
#include "mythuitext.h"

MythUIButton::MythUIButton(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_clickTimer(new QTimer())
{
    m_clickTimer->setSingleShot(true);

    connect(m_clickTimer, &QTimer::timeout, this, &MythUIButton::UnPush);

    connect(this, &MythUIType::TakingFocus, this, &MythUIButton::Select);
    connect(this, &MythUIType::LosingFocus, this, &MythUIButton::Deselect);
    connect(this, &MythUIType::Enabling, this, &MythUIButton::Enable);
    connect(this, &MythUIType::Disabling, this, &MythUIButton::Disable);

    SetCanTakeFocus(true);
}

MythUIButton::~MythUIButton()
{
    if (m_clickTimer)
        m_clickTimer->deleteLater();
}

void MythUIButton::SetInitialStates()
{
    m_backgroundState = dynamic_cast<MythUIStateType *>(GetChild("buttonstate"));

    if (!m_backgroundState)
        LOG(VB_GENERAL, LOG_ERR, QString("Button %1 is missing required "
                                         "elements").arg(objectName()));

    SetState("active");

    if (m_text && m_message.isEmpty())
        m_message = m_text->GetDefaultText();
}

/*!
 * \copydoc MythUIType::Reset()
 */
void MythUIButton::Reset()
{
    MythUIType::Reset();
}

void MythUIButton::Select()
{
    if (!IsEnabled() || m_pushed)
        return;

    SetState("selected");
}

void MythUIButton::Deselect()
{
    if (m_pushed)
        return;

    if (IsEnabled())
        SetState("active");
    else
        SetState("disabled");
}

void MythUIButton::Enable()
{
    SetState("active");
}

void MythUIButton::Disable()
{
    SetState("disabled");
}

void MythUIButton::SetState(const QString& state)
{
    if (m_state == state)
        return;

    if (m_pushed && state != "pushed")
        UnPush();

    m_state = state;

    if (!m_backgroundState)
        return;

    m_backgroundState->DisplayState(m_state);

    auto *activeState = dynamic_cast<MythUIGroup *>
                               (m_backgroundState->GetCurrentState());

    if (activeState)
        m_text = dynamic_cast<MythUIText *>(activeState->GetChild("text"));

    if (m_text)
    {
        m_text->SetFontState(m_state);
        m_text->SetText(m_message);
    }
}

/**
 *  \copydoc MythUIType::keyPressEvent()
 */
bool MythUIButton::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (IsEnabled())
            {
                if (m_pushed)
                    UnPush();
                else
                    Push();
            }
        }
        else
        {
            handled = false;
        }
    }

    return handled;
}

/**
 *  \copydoc MythUIType::gestureEvent()
 */
bool MythUIButton::gestureEvent(MythGestureEvent *event)
{
    if (event->GetGesture() == MythGestureEvent::Click)
    {
        if (IsEnabled())
        {
            if (m_pushed)
                UnPush();
            else
                Push();

            return true;
        }
    }

    return false;
}

void MythUIButton::Push(bool lock)
{
    m_pushed = true;
    SetState("pushed");

    if (!lock && !m_lockable)
        m_clickTimer->start(500ms);

    emit Clicked();
}

void MythUIButton::UnPush()
{
    if (!m_pushed)
        return;

    m_clickTimer->stop();

    m_pushed = false;

    if (m_hasFocus)
        SetState("selected");
    else if (m_enabled)
        SetState("active");
    else
        SetState("disabled");

    if (m_lockable)
        emit Clicked();
}

void MythUIButton::SetLocked(bool locked)
{
    if (!m_lockable)
        return;

    if (locked)
    {
        m_pushed = true;
        SetState("pushed");
    }
    else
    {
        m_pushed = false;

        if (m_hasFocus)
            SetState("selected");
        else if (m_enabled)
            SetState("active");
        else
            SetState("disabled");
    }
}

void MythUIButton::SetText(const QString &msg)
{
    if (m_message == msg)
        return;

    m_message = msg;

    auto *activeState = dynamic_cast<MythUIGroup *>
                               (m_backgroundState->GetCurrentState());

    if (activeState)
        m_text = dynamic_cast<MythUIText *>(activeState->GetChild("text"));

    if (m_text)
        m_text->SetText(msg);
}

QString MythUIButton::GetText() const
{
    return m_message;
}

QString MythUIButton::GetDefaultText() const
{
    return m_text->GetDefaultText();
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIButton::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "value")
    {
        m_valueText = QCoreApplication::translate("ThemeUI",
                                      parseText(element).toUtf8());
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIButton::CreateCopy(MythUIType *parent)
{
    auto *button = new MythUIButton(parent, objectName());
    button->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIButton::CopyFrom(MythUIType *base)
{
    auto *button = dynamic_cast<MythUIButton *>(base);
    if (!button)
    {
        LOG(VB_GENERAL, LOG_ERR, "Dynamic cast of base failed");
        return;
    }

    m_message = button->m_message;
    m_valueText = button->m_valueText;
    m_lockable = button->m_lockable;

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIButton::Finalize()
{
    SetInitialStates();
    SetText(m_valueText);
}
