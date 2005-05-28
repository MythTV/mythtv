/**
 * @file keygrabber.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Header for popups used by mythcontrols.
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

#include <mythtv/mythdialogs.h>


/**
 * @class KeyGrabPopupBox
 * @brief Captures a key.
 *
 * This popup box takes control over the keyboard until a key is released.
 */
class KeyGrabPopupBox : public MythPopupBox
{

    Q_OBJECT

 public:

    /**
     * @brief Create a new key grabber.
     * @param window The main myth window.
     */
    KeyGrabPopupBox(MythMainWindow * window);

    /**
     * @brief Add the OK button.
     * @param button The button that will be hilighed once a key has
     * been captured.
     */
    inline void addOKButton(QButton *button) { ok_button = button; }

    /**
     * @brief Get the string containing the captured key event.
     */
    inline QString getCapturedKeyEvent() { return this->captured_key_event; }

 protected:

    /**
     * @brief The key press handler.
     * @param e The key event.
     */
    void keyPressEvent(QKeyEvent *e);

    /**
     * @brief The key release handler.
     * @param e The key event.
     */
    void keyReleaseEvent(QKeyEvent *e);

 private:

    bool is_capturing;
    bool has_captured;
    QString captured_key_event;
    QButton *ok_button;
    QLabel *key_label;
};
