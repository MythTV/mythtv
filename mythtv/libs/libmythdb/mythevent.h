#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QString>
#include <QStringList>
#include <QEvent>
#include "mythexp.h"

/** \class MythEvent
    \brief This class is used as a container for messages.

    Any subclass of this that adds data to the event should override
    the clone method. As example, see OutputEvent in output.h.
 */
class MPUBLIC MythEvent : public QEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(int t) : QEvent((QEvent::Type)t)
    { }

    MythEvent(const QString lmessage) : QEvent((QEvent::Type)MythEventMessage)
    {
        message = lmessage;
        extradata.append( "empty" );
    }

    MythEvent(const QString lmessage, const QStringList &lextradata)
           : QEvent((QEvent::Type)MythEventMessage)
    {
        message = lmessage;
        extradata = lextradata;
    }

    MythEvent(const QString lmessage, const QString lextradata)
           : QEvent((QEvent::Type)MythEventMessage)
    {
        message = lmessage;
        extradata.append( lextradata );
    }


    virtual ~MythEvent() {}

    const QString& Message() const { return message; }
    const QString& ExtraData(int idx = 0) const { return extradata[idx]; }
    const QStringList& ExtraDataList() const { return extradata; }
    int ExtraDataCount() const { return extradata.size(); }

    virtual MythEvent* clone() { return new MythEvent(message, extradata); }

  private:
    QString message;
    QStringList extradata;
};

const int kExternalKeycodeEventType = 33213;
const int kExitToMainMenuEventType = 33214;

class ExternalKeycodeEvent : public QEvent
{
  public:
    ExternalKeycodeEvent(const int key)
           : QEvent((QEvent::Type)kExternalKeycodeEventType), keycode(key) {}

    int getKeycode() { return keycode; }

  private:
    int keycode;
};

class ExitToMainMenuEvent : public QEvent
{
  public:
    ExitToMainMenuEvent(void) : QEvent((QEvent::Type)kExitToMainMenuEventType) {}
};

const int kMythPostShowEventType = QEvent::User + 2000;
class MPUBLIC MythPostShowEvent : public QEvent
{
  public:
    MythPostShowEvent() : QEvent((QEvent::Type)kMythPostShowEventType) {}
};

#endif /* MYTHEVENT_H */
