#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QStringList>
#include <QEvent>
#include <QHash>

#include "mythexp.h"

/** \class MythEvent
    \brief This class is used as a container for messages.

    Any subclass of this that adds data to the event should override
    the clone method. As example, see OutputEvent in output.h.
 */
class MPUBLIC MythEvent : public QEvent
{
  public:
    MythEvent(int t) : QEvent((QEvent::Type)t)
    { }

    MythEvent(const QString lmessage) : QEvent(MythEventMessage)
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

    virtual MythEvent *clone() const
    { return new MythEvent(message, extradata); }

    static Type MythEventMessage;
    static Type kExitToMainMenuEventType;
    static Type kMythPostShowEventType;
    static Type kEnableDrawingEventType;
    static Type kDisableDrawingEventType;
    static Type kPushDisableDrawingEventType;
    static Type kPopDisableDrawingEventType;
    static Type kLockInputDevicesEventType;
    static Type kUnlockInputDevicesEventType;
    static Type kUpdateBrowseInfoEventType;

  private:
    QString message;
    QStringList extradata;
};

class MPUBLIC ExternalKeycodeEvent : public QEvent
{
  public:
    ExternalKeycodeEvent(const int key) :
        QEvent(kEventType), keycode(key) {}

    int getKeycode() { return keycode; }

    static Type kEventType;

  private:
    int keycode;
};

class MPUBLIC UpdateBrowseInfoEvent : public QEvent
{
  public:
    UpdateBrowseInfoEvent(const QHash<QString,QString> &infoMap) :
        QEvent(MythEvent::kUpdateBrowseInfoEventType), im(infoMap) {}
    QHash<QString,QString> im;
};

#endif /* MYTHEVENT_H */
