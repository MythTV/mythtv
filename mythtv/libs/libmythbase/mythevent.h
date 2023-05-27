#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QStringList>
#include <QEvent>
#include <utility>
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
    explicit MythEvent(int type) : QEvent((QEvent::Type)type)
    { }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int type, QString lmessage)
        : QEvent((QEvent::Type)type),
        m_message(::std::move(lmessage)),
        m_extradata("empty")
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(int type, QString lmessage, QStringList lextradata)
        : QEvent((QEvent::Type)type),
        m_message(::std::move(lmessage)),
        m_extradata(std::move(lextradata))
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    explicit MythEvent(QString lmessage)
        : QEvent(kMythEventMessage),
        m_message(::std::move(lmessage)),
        m_extradata("empty")
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(QString lmessage, QStringList lextradata)
        : QEvent(kMythEventMessage),
        m_message(::std::move(lmessage)),
        m_extradata(std::move(lextradata))
    {
    }

    // lmessage is passed by value for thread safety reasons per DanielK
    MythEvent(QString lmessage, const QString& lextradata)
        : QEvent(kMythEventMessage),
        m_message(::std::move(lmessage)),
        m_extradata(lextradata)
    {
    }


    ~MythEvent() override;

    const QString& Message() const { return m_message; }
    const QString& ExtraData(int idx = 0) const { return m_extradata[idx]; }
    const QStringList& ExtraDataList() const { return m_extradata; }
    int ExtraDataCount() const { return m_extradata.size(); }
    void log(const QString& prefix);

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    virtual MythEvent *clone() const
    { return new MythEvent(m_message, m_extradata); }
#else
    MythEvent *clone() const override
    { return new MythEvent(type(), m_message, m_extradata); }
#endif

    static const Type kMythEventMessage;
    static const Type kMythUserMessage;
    static const Type kUpdateTvProgressEventType;
    static const Type kExitToMainMenuEventType;
    static const Type kMythPostShowEventType;
    static const Type kPushDisableDrawingEventType;
    static const Type kPopDisableDrawingEventType;
    static const Type kLockInputDevicesEventType;
    static const Type kUnlockInputDevicesEventType;
    static const Type kUpdateBrowseInfoEventType;
    static const Type kDisableUDPListenerEventType;
    static const Type kEnableUDPListenerEventType;

  // No implicit copying.
  protected:
    MythEvent(const MythEvent &other) = default;
    MythEvent &operator=(const MythEvent &other) = default;
  public:
    MythEvent(MythEvent &&) = delete;
    MythEvent &operator=(MythEvent &&) = delete;

  protected:
    QString m_message;
    QStringList m_extradata;
};

class MBASE_PUBLIC ExternalKeycodeEvent : public QEvent
{
  public:
    explicit ExternalKeycodeEvent(const int key) :
        QEvent(kEventType), m_keycode(key) {}

    int getKeycode() const { return m_keycode; }

    static const Type kEventType;

  private:
    int m_keycode;
};

class MBASE_PUBLIC UpdateBrowseInfoEvent : public QEvent
{
  public:
    explicit UpdateBrowseInfoEvent(InfoMap infoMap) :
        QEvent(MythEvent::kUpdateBrowseInfoEventType), m_im(std::move(infoMap)) {}
    InfoMap m_im;
};

// TODO combine with UpdateBrowseInfoEvent above
class MBASE_PUBLIC MythInfoMapEvent : public MythEvent
{
  public:
    MythInfoMapEvent(const QString &lmessage,
                     InfoMap linfoMap)
      : MythEvent(lmessage), m_infoMap(std::move(linfoMap)) { }

    MythInfoMapEvent *clone() const override // MythEvent
        { return new MythInfoMapEvent(Message(), m_infoMap); }
    const InfoMap* GetInfoMap(void) { return &m_infoMap; }

  private:
    InfoMap m_infoMap;
};
#endif /* MYTHEVENT_H */
