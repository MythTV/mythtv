/* -*- myth -*- */
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

#include <iostream>

using namespace std;

#include "keygrabber.h"

#include <mythtv/mythcontext.h>


KeyGrabPopupBox::KeyGrabPopupBox(MythMainWindow *window)
    : MythPopupBox(window, "keygrabber")
{
    this->is_capturing = false;
    this->has_captured = false;
    addLabel("Press A Key", Large, false);
    key_label = addLabel("Waiting for key press", Small, false);

    ok_button = this->addButton(tr("OK"), this, SLOT(acceptBinding()));
    this->addButton(tr("Cancel"), this, SLOT(cancel()));

    this->grabKeyboard();
}



void KeyGrabPopupBox::keyReleaseEvent(QKeyEvent *e)
{
    if (!this->is_capturing) return;

    /* now we have an event, so we are not capturing */
    this->has_captured = true;
    this->is_capturing = false;

    /* get the base name of the qkeysequence */
    QString key_name = QString(QKeySequence(e->key()));

    /* if we really have a key, then process it */
    if (!key_name.isEmpty() && !key_name.isNull())
    {
        QString modifiers;

        /* key modifier strings as defined by the QT docs */
        if (e->state()&Qt::ShiftButton) modifiers+="Shift+";
        if (e->state()&Qt::ControlButton) modifiers+="Ctrl+";
        if (e->state()&Qt::AltButton) modifiers+="Alt+";
        if (e->state()&Qt::MetaButton) modifiers+="Meta+";
        key_name = modifiers + key_name;
    }

    this->captured_key_event = key_name;
    this->key_label->setText("Add key, \"" + key_name + "\"?");
    this->releaseKeyboard();
    this->ok_button->setFocus();
}



void KeyGrabPopupBox::keyPressEvent(QKeyEvent *e)
{

    /* if no capturing has occured yet, then start */
    if (!has_captured) this->is_capturing = true;

    /* accept events while we are capturing */
    if (this->is_capturing) e->accept();
    else MythPopupBox::keyPressEvent(e);
}

InvalidBindingPopup::InvalidBindingPopup(MythMainWindow *window)
    : MythPopupBox(window, "invalidbinding")
{
    QString warning = "This action is manditory and needs at least one key"
        " bound to it.  Instead, try rebinding with another key.";
    addLabel("Manditory Action", Large, false);
    addLabel(warning, Small, true);
}



InvalidBindingPopup::InvalidBindingPopup(MythMainWindow *window,
                                         const QString &action,
                                         const QString &context)
    : MythPopupBox(window, "invalidbinding")
{
    QString message = "This kebinding conflicts with ";
    message += action + " in the " + context;
    message += " context.";

    addLabel("Conflicting Binding", Large, false);
    addLabel(message, Small, true);
}



OptionsMenu::OptionsMenu(MythMainWindow *window)
    : MythPopupBox(window, "optionmenu")
{
    addLabel(tr("Options"), Large, false);
    addButton(tr("Save"), this, SLOT(save()));
    addButton(tr("Cancel"), this, SLOT(cancel()))->setFocus();
}
    

ActionMenu::ActionMenu(MythMainWindow *window)
    : MythPopupBox(window, "actionmenu")
{
    addLabel(tr("Modify Action"), Large, false);
    addButton(tr("Set Binding"), this, SLOT(set()));
    addButton(tr("Remove Binding"), this, SLOT(remove()));
    addButton(tr("Cancel"), this, SLOT(cancel()))->setFocus();
}

UnsavedMenu::UnsavedMenu(MythMainWindow *window)
    : MythPopupBox(window, "unsavedmenu")
{
    addLabel(tr("Unsaged Changes"), Large, false);
    addLabel(tr("Would you like to save now?"));
    addButton(tr("Save"), this, SLOT(save()))->setFocus();
    addButton(tr("Exit"), this, SLOT(cancel()));
}
