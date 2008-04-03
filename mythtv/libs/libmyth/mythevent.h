#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <QString>
#include <QStringList>
#include <QEvent>

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

#endif /* MYTHEVENT_H */
