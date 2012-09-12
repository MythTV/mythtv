#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QStringList>
#include <QEvent>
#include <QHash>

#include "mythbaseexp.h"

/** \class MythEvent
    \brief This class is used as a container for messages.

    Any subclass of this that adds data to the event should override
    the clone method. As example, see OutputEvent in output.h.
 */
class MBASE_PUBLIC MythEvent : public QEvent
{
  public:
    MythEvent(int t) : QEvent((QEvent::Type)t)
    { }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int t, const QString lmessage) : QEvent((QEvent::Type)t)
    {
        message = lmessage;
        extradata.append( "empty" );
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int t, const QString lmessage, const QStringList &lextradata)
           : QEvent((QEvent::Type)t)
    {
        message = lmessage;
        extradata = lextradata;
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(const QString lmessage) : QEvent(MythEventMessage)
    {
        message = lmessage;
        extradata.append( "empty" );
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(const QString lmessage, const QStringList &lextradata)
           : QEvent((QEvent::Type)MythEventMessage)
    {
        message = lmessage;
        extradata = lextradata;
    }

    // lmessage is passed by value for thread safety reasons per DanielK
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
    static Type MythUserMessage;
    static Type kUpdateTvProgressEventType;
    static Type kExitToMainMenuEventType;
    static Type kMythPostShowEventType;
    static Type kEnableDrawingEventType;
    static Type kDisableDrawingEventType;
    static Type kPushDisableDrawingEventType;
    static Type kPopDisableDrawingEventType;
    static Type kLockInputDevicesEventType;
    static Type kUnlockInputDevicesEventType;
    static Type kUpdateBrowseInfoEventType;
    static Type kDisableUDPListenerEventType;
    static Type kEnableUDPListenerEventType;

  private:
    QString message;
    QStringList extradata;
};

class MBASE_PUBLIC ExternalKeycodeEvent : public QEvent
{
  public:
    ExternalKeycodeEvent(const int key) :
        QEvent(kEventType), keycode(key) {}

    int getKeycode() { return keycode; }

    static Type kEventType;

  private:
    int keycode;
};

class MBASE_PUBLIC UpdateBrowseInfoEvent : public QEvent
{
  public:
    UpdateBrowseInfoEvent(const QHash<QString,QString> &infoMap) :
        QEvent(MythEvent::kUpdateBrowseInfoEventType), im(infoMap) {}
    QHash<QString,QString> im;
};

// TODO combine with UpdateBrowseInfoEvent above
class MBASE_PUBLIC MythInfoMapEvent : public MythEvent
{
  public:
    MythInfoMapEvent(const QString &lmessage,
                     const QHash<QString,QString> &linfoMap)
      : MythEvent(lmessage), infoMap(linfoMap) { }

    virtual MythInfoMapEvent *clone() const
        { return new MythInfoMapEvent(Message(), infoMap); }
    const QHash<QString,QString>* InfoMap(void) { return &infoMap; }

  private:
    QHash<QString,QString> infoMap;
};

#endif /* MYTHEVENT_H */
