// -*- Mode: c++ -*-
// Qt headers
#include <qstring.h>

// MythTV headers
#include <mythtv/mythcontext.h>

// MythControls headers
#include "keygrabber.h"

KeyGrabPopupBox::KeyGrabPopupBox(MythScreenStack *parent)
    : MythDialogBox ("", parent, "keygrabberdialog")
{
    m_waitingForKeyRelease = m_keyReleaseSeen = false;
    m_capturedKey = QString::null;
}

KeyGrabPopupBox::~KeyGrabPopupBox()
{
}

bool KeyGrabPopupBox::Create(void)
{
    MythDialogBox::Create();

    QString label = QString("%1\n\n%2").arg(tr("Press A Key")).arg(tr("Waiting for key press"));

    m_textarea->SetText(label);
    AddButton(tr("Ok"), &m_capturedKey);
    AddButton(tr("Cancel"));
    m_buttonList->SetActive(false);

    return true;
}

bool KeyGrabPopupBox::keyPressEvent(QKeyEvent *event)
{
    // If no capturing has occured yet, then start waiting for key release
    m_waitingForKeyRelease |= !m_keyReleaseSeen;

    bool handled = false;

    if (!m_waitingForKeyRelease)
    {
        if (GetFocusWidget()->keyPressEvent(event))
            handled = true;
    }
    else
    {
        int keycode = event->key();

        // Modifier keypress, ignore until we see the complete combo
        if ((keycode >= Qt::Key_Shift) && (keycode <= Qt::Key_AltGr))
            return true;

        m_waitingForKeyRelease = false;
        m_keyReleaseSeen       = true;

        QString key_name = QString(QKeySequence(event->key()));
        if (!key_name.isEmpty() && !key_name.isNull())
        {
            QString modifiers = "";

            /* key modifier strings as defined by the QT docs */
            if (event->modifiers() & Qt::ShiftModifier)
                modifiers += "Shift+";
            if (event->modifiers() & Qt::ControlModifier)
                modifiers += "Ctrl+";
            if (event->modifiers() & Qt::AltModifier)
                modifiers += "Alt+";
            if (event->modifiers() & Qt::MetaModifier)
                modifiers += "Meta+";

            key_name = modifiers + key_name;
        }

        if (key_name.isEmpty())
        {
            m_textarea->SetText(tr("Pressed key not recognized"));
        }
        else
        {
            m_capturedKey = key_name;
            m_textarea->SetText(tr("Add key '%1'?").arg(key_name));
        }

        m_buttonList->SetActive(true);

        handled = true;
    }

    return handled;
}
