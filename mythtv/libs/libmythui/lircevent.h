#ifndef LIRCEVENT_H_
#define LIRCEVENT_H_

#include <QEvent>

const int kLircKeycodeEventType = 23423;
const int kLircMuteEventType = 23424;

class LircKeycodeEvent : public QEvent
{
  public:
    LircKeycodeEvent(const QString &lirc_text, int key_code, bool key_down) :
            QEvent((QEvent::Type)kLircKeycodeEventType), m_lirctext(lirc_text),
            m_keycode(key_code), m_keydown(key_down) {}

    QString getLircText() const { return m_lirctext; }
    int getKeycode() const { return m_keycode; }
    bool isKeyDown()const { return m_keydown; }

  private:
    QString m_lirctext;
    int m_keycode;
    bool m_keydown;
};

class LircMuteEvent : public QEvent
{
  public:
    LircMuteEvent(bool mute_events) : QEvent((QEvent::Type)kLircMuteEventType),
            m_muteLircEvents(mute_events) {}

    bool eventsMuted() const { return m_muteLircEvents; }

  private:
    bool m_muteLircEvents;
};

class LircEventLock
{
  public:
    LircEventLock(bool lock_events = true);
    ~LircEventLock();
    void lock();
    void unlock();

  private:
    bool m_eventsLocked;
};

#endif

