#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QStringList>
#include <QEvent>
#include "mythtypes.h"
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
    MythEvent(int t, const QString lmessage) : QEvent((QEvent::Type)t),
            m_message(lmessage),    m_extradata("empty")
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int t, const QString lmessage, const QStringList &lextradata)
           : QEvent((QEvent::Type)t),
            m_message(lmessage),    m_extradata(lextradata)
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(const QString lmessage) : QEvent(MythEventMessage),
            m_message(lmessage),    m_extradata("empty")
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(const QString lmessage, const QStringList &lextradata)
           : QEvent((QEvent::Type)MythEventMessage),
           m_message(lmessage),    m_extradata(lextradata)
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(const QString lmessage, const QString lextradata)
           : QEvent((QEvent::Type)MythEventMessage),
           m_message(lmessage),    m_extradata(lextradata)
    {
    }


    virtual ~MythEvent() {}

    const QString& Message() const { return m_message; }
    const QString& ExtraData(int idx = 0) const { return m_extradata[idx]; }
    const QStringList& ExtraDataList() const { return m_extradata; }
    int ExtraDataCount() const { return m_extradata.size(); }

    virtual MythEvent *clone() const
    { return new MythEvent(m_message, m_extradata); }

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
    QString m_message;
    QStringList m_extradata;
};

class MBASE_PUBLIC ExternalKeycodeEvent : public QEvent
{
  public:
    ExternalKeycodeEvent(const int key) :
        QEvent(kEventType), m_keycode(key) {}

    int getKeycode() { return m_keycode; }

    static Type kEventType;

  private:
    int m_keycode;
};

class MBASE_PUBLIC UpdateBrowseInfoEvent : public QEvent
{
  public:
    UpdateBrowseInfoEvent(const InfoMap &infoMap) :
        QEvent(MythEvent::kUpdateBrowseInfoEventType), im(infoMap) {}
    InfoMap im;
};

// TODO combine with UpdateBrowseInfoEvent above
class MBASE_PUBLIC MythInfoMapEvent : public MythEvent
{
  public:
    MythInfoMapEvent(const QString &lmessage,
                     const InfoMap &linfoMap)
      : MythEvent(lmessage), m_infoMap(linfoMap) { }

    virtual MythInfoMapEvent *clone() const
        { return new MythInfoMapEvent(Message(), m_infoMap); }
    const InfoMap* GetInfoMap(void) { return &m_infoMap; }

  private:
    InfoMap m_infoMap;
};

#endif /* MYTHEVENT_H */
