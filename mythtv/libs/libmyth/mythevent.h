#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <qstring.h>
#include <qstringlist.h>
#include <qevent.h>
#include <qdeepcopy.h>

/** \class MythEvent
    \brief This class is used as a container for messages.
    
    Any subclass of this that adds data to the event should override
    the clone method. As example, see OutputEvent in output.h. 
 */
class MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(int t) : QCustomEvent(t)
    { }

    MythEvent(const QString lmessage) : QCustomEvent(MythEventMessage)
    {
        message = QDeepCopy<QString>(lmessage);
        extradata = "empty";
    }

    MythEvent(const QString lmessage, const QStringList &lextradata)
           : QCustomEvent(MythEventMessage)
    {
        message = QDeepCopy<QString>(lmessage);
        extradata = lextradata;
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

#endif /* MYTHEVENT_H */
