#ifndef LIRCEVENT_H_
#define LIRCEVENT_H_

const int kLircKeycodeEventType = 23423;

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

#endif

