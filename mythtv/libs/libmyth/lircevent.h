#ifndef LIRCEVENT_H_
#define LIRCEVENT_H_

const int kLircKeycodeEventType = 23423;
const int kLircMuteEventType = 23424;

class LircKeycodeEvent : public QCustomEvent 
{
  public:
    LircKeycodeEvent(const QString &lirc_text, int key_code, bool key_down) :
            QCustomEvent(kLircKeycodeEventType), lirctext(lirc_text), 
            keycode(key_code), keydown(key_down) {}

    QString getLircText()
    {
        return lirctext;
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
    QString lirctext;
    int keycode;
    bool keydown;
};

class LircMuteEvent : public QCustomEvent
{
  public:
    LircMuteEvent(bool mute_events) : QCustomEvent(kLircMuteEventType),
            mute_lirc_events(mute_events) {}

    bool eventsMuted()
    {
        return mute_lirc_events;
    }

  private:
    bool mute_lirc_events;
};

class LircEventLock
{
  public:
    LircEventLock(bool lock_events = true);
    ~LircEventLock();
    void lock();
    void unlock();

  private:
    bool events_locked;
};

#endif

