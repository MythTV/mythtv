/*----------------------------------------------------------------------------
** jsmenuevent.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.h
**--------------------------------------------------------------------------*/
#ifndef JSMENUEVENT_H_
#define JSMENUEVENT_H_

const int kJoystickKeycodeEventType = 24425;
const int kJoystickMuteEventType = 24426;

class JoystickKeycodeEvent : public QCustomEvent 
{
  public:
    JoystickKeycodeEvent(const QString &jsmenuevent_text, int key_code, bool key_down) :
            QCustomEvent(kJoystickKeycodeEventType), jsmenueventtext(jsmenuevent_text), 
            keycode(key_code), keydown(key_down) {}

    QString getJoystickMenuText()
    {
        return jsmenueventtext;
    }

    int getKeycode()
    {
        return keycode;
    }

    bool isKeyDown()
    {
        return keydown;
    }

  private:
    QString jsmenueventtext;
    int keycode;
    bool keydown;
};

class JoystickMenuMuteEvent : public QCustomEvent
{
  public:
    JoystickMenuMuteEvent(bool mute_events) : QCustomEvent(kJoystickMuteEventType),
            mute_jsmenu_events(mute_events) {}

    bool eventsMuted()
    {
        return mute_jsmenu_events;
    }

  private:
    bool mute_jsmenu_events;
};

class JoystickMenuEventLock
{
  public:
    JoystickMenuEventLock(bool lock_events = true);
    ~JoystickMenuEventLock();
    void lock();
    void unlock();

  private:
    bool events_locked;
};

#endif

