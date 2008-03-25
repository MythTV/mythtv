// -*- Mode: c++ -*-
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

#ifndef KEYGRABBER_H_
#define KEYGRABBER_H_

// MythTV headers
#include <mythtv/mythdialogs.h>
//Added by qt3to4:
#include <QKeyEvent>
#include <QLabel>


/** \class KeyGrabPopupBox
 *  \brief Captures a key.
 *
 *   NOTE: This popup box takes control over the keyboard
 *         until a key is released.
 */
class KeyGrabPopupBox : public MythPopupBox
{
    Q_OBJECT

  public:
    KeyGrabPopupBox(MythMainWindow *window);

    QString GetCapturedKey(void) const;

    virtual void deleteLater(void);

  protected:
    void Teardown(void);
    ~KeyGrabPopupBox(); // use deleteLater() instead for thread safety
    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);

  private:
    bool     m_waitingForKeyRelease;
    bool     m_keyReleaseSeen;
    QString  m_capturedKey;
    QAbstractButton *m_ok;
    QAbstractButton *m_cancel;
    QLabel  *m_label;
};

#endif // KEYGRABBER_H_
