// -*- Mode: c++ -*-
/**
 * @file keygrabber.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Implementation of popups used by mythcontrols.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */

// Qt headers
#include <qstring.h>
#include <q3deepcopy.h>
//Added by qt3to4:
#include <QKeyEvent>

// MythTV headers
#include <mythtv/mythcontext.h>

// MythControls headers
#include "keygrabber.h"

KeyGrabPopupBox::KeyGrabPopupBox(MythMainWindow *window)
    : MythPopupBox(window, "keygrabber"),
      m_waitingForKeyRelease(false), m_keyReleaseSeen(false),
      m_capturedKey(QString::null),
      m_ok(NULL), m_cancel(NULL), m_label(NULL)
{
    addLabel(tr("Press A Key"), Large, false);
    m_label  = addLabel(tr("Waiting for key press"), Small, false);
    m_ok     = addButton(QObject::tr("OK"),     this, SLOT(accept()));
    m_cancel = addButton(QObject::tr("Cancel"), this, SLOT(reject()));

    grabKeyboard();
}

KeyGrabPopupBox::~KeyGrabPopupBox()
{
    Teardown();
}

void KeyGrabPopupBox::deleteLater(void)
{
    Teardown();
    MythPopupBox::deleteLater();
}

void KeyGrabPopupBox::Teardown(void)
{
    m_capturedKey = QString::null;
    m_ok          = NULL;
    m_cancel      = NULL;
    m_label       = NULL;
    releaseKeyboard();
}

/// \brief Get the string containing the captured key event plus
///        modifier keys.
QString KeyGrabPopupBox::GetCapturedKey(void) const
{
    return Q3DeepCopy<QString>(m_capturedKey);
}

void KeyGrabPopupBox::keyReleaseEvent(QKeyEvent *e)
{
    if (!m_ok || !m_cancel || !m_label)
        return;

    if (!m_waitingForKeyRelease)
        return;

    m_waitingForKeyRelease = false;
    m_keyReleaseSeen       = true;

    QString key_name = QString(QKeySequence(e->key()));
    if (!key_name.isEmpty() && !key_name.isNull())
    {
        QString modifiers = "";

        /* key modifier strings as defined by the QT docs */
        if (e->state() & Qt::ShiftButton)
            modifiers += "Shift+";
        if (e->state() & Qt::ControlButton)
            modifiers += "Ctrl+";
        if (e->state() & Qt::AltButton)
            modifiers += "Alt+";
        if (e->state() & Qt::MetaButton)
            modifiers += "Meta+";

        key_name = modifiers + key_name;
    }

    if (key_name.isEmpty())
    {
        m_label->setText(tr("Pressed key not recognized"));
        m_ok->setDisabled(true);
        m_cancel->setFocus();
    }
    else
    {
        m_capturedKey = key_name;
        m_label->setText(tr("Add key '%1'?").arg(key_name));
        m_ok->setFocus();
    }

    releaseKeyboard();
}

void KeyGrabPopupBox::keyPressEvent(QKeyEvent *e)
{
    // if no capturing has occured yet, then start waiting for key release
    m_waitingForKeyRelease |= !m_keyReleaseSeen;

    if (m_waitingForKeyRelease)
    {
        e->accept();
        return;
    }

    MythPopupBox::keyPressEvent(e);
}
