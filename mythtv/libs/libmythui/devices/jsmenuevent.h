// -*- Mode: c++ -*-
/*----------------------------------------------------------------------------
** jsmenuevent.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.h
**--------------------------------------------------------------------------*/
#ifndef JSMENUEVENT_H_
#define JSMENUEVENT_H_

#include <utility>

// Qt headers
#include <QEvent>
#include <QString>

class JoystickKeycodeEvent : public QEvent
{
  public:
    JoystickKeycodeEvent(
        QString jsmenuevent_text, int key_code,
        Qt::KeyboardModifiers key_modifiers, QEvent::Type key_action) :
        QEvent(kEventType), m_jsmenueventtext(std::move(jsmenuevent_text)),
        m_key(key_code), m_keyModifiers(key_modifiers), m_keyAction(key_action)
    {
    }

    QString getJoystickMenuText() const { return m_jsmenueventtext; }
    int key() const { return m_key; }
    Qt::KeyboardModifiers keyModifiers() const { return m_keyModifiers; }
    QEvent::Type keyAction() const { return m_keyAction; }

    static const Type kEventType;

  private:
    QString m_jsmenueventtext;
    int m_key;
    Qt::KeyboardModifiers m_keyModifiers;
    QEvent::Type m_keyAction;
};

#endif
