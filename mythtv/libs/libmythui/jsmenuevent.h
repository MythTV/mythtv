// -*- Mode: c++ -*-
/*----------------------------------------------------------------------------
** jsmenuevent.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.h
**--------------------------------------------------------------------------*/
#ifndef JSMENUEVENT_H_
#define JSMENUEVENT_H_

#include <QEvent>
#include <QString>

class JoystickKeycodeEvent : public QEvent
{
  public:
    JoystickKeycodeEvent(
        const QString &jsmenuevent_text, int key_code, bool key_down) :
        QEvent(kEventType), m_jsmenueventtext(jsmenuevent_text),
        m_keycode(key_code), m_keydown(key_down)
    {
    }

    QString getJoystickMenuText() const { return m_jsmenueventtext; }
    int getKeycode() const { return m_keycode; }
    bool isKeyDown() const { return m_keydown; }

    static Type kEventType;

  private:
    QString m_jsmenueventtext;
    int m_keycode;
    bool m_keydown;
};

#endif
