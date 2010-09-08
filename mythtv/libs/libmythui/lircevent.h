// -*- Mode: c++ -*-

#ifndef LIRCEVENT_H_
#define LIRCEVENT_H_

#include <QEvent>
#include <QString>

class LircKeycodeEvent : public QEvent
{
  public:
     LircKeycodeEvent(Type keytype, int key, Qt::KeyboardModifiers mod,
                      const QString &text, const QString &lirc_text) :
        QEvent(kEventType),
        m_keytype(keytype), m_key(key), m_modifiers(mod),
        m_text(text), m_lirctext(lirc_text)
    {
        m_text.detach();
        m_lirctext.detach();
    }

    Type                  keytype(void)   const { return m_keytype;   }
    int                   key(void)       const { return m_key;       }
    Qt::KeyboardModifiers modifiers(void) const { return m_modifiers; }
    QString               text(void)      const { return m_text;      }
    QString               lirctext(void)  const { return m_lirctext;  }

    static Type kEventType;

    static const int kLIRCInvalidKeyCombo  = 0xFFFFFFFF;

  private:
    Type                  m_keytype;
    int                   m_key;
    Qt::KeyboardModifiers m_modifiers;
    QString               m_text;
    QString               m_lirctext;
};

#endif
