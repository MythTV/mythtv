#ifndef MYTHEVENT_H_
#define MYTHEVENT_H_

#include <qstring.h>
#include <qstringlist.h>
#include <qevent.h>

#include "mythexp.h" // for MPUBLIC

/** \class MythEvent
    \brief This class is used as a container for messages.
    
    Any subclass of this that adds data to the event should override
    the clone method. As example, see OutputEvent in output.h. 
 */
class MPUBLIC MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(int t);
    MythEvent(const QString &lmessage);
    MythEvent(const QString &lmessage, const QStringList &lextradata);
    
    virtual ~MythEvent();

    const QString& Message() const { return message; }
    const QString& ExtraData(int idx = 0) const;
    const QStringList& ExtraDataList() const { return extradata; } 
    int ExtraDataCount() const { return extradata.size(); }

    virtual MythEvent* clone() { return new MythEvent(message, extradata); }

  private:
    QString message;
    QStringList extradata;
};

#endif /* MYTHEVENT_H */
