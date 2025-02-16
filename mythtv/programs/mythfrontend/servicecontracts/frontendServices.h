#ifndef FRONTENDSERVICES_H
#define FRONTENDSERVICES_H

#include "service.h"

#include "frontendActionList.h"
#include "frontendStatus.h"

class FrontendServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO( "version", "2.1" );
    Q_CLASSINFO( "SendMessage_Method",            "POST" )
    Q_CLASSINFO( "SendNotification_Method",       "POST" )
    Q_CLASSINFO( "SendAction_Method",             "POST" )
    Q_CLASSINFO( "PlayRecording_Method",          "POST" )
    Q_CLASSINFO( "PlayVideo_Method",              "POST" )
    Q_CLASSINFO( "SendKey_Method",                "POST" )


  public:
    explicit FrontendServices(QObject *parent = nullptr) : Service(parent)
    {
        DTC::FrontendStatus::InitializeCustomTypes();
        DTC::FrontendActionList::InitializeCustomTypes();
    }

  public slots:
    virtual DTC::FrontendStatus* GetStatus(void) = 0;
    virtual bool                 SendMessage(const QString &Message,
                                             uint Timeout) = 0;

    virtual bool                 SendNotification(bool  Error,
                                                  const QString &Type,
                                                  const QString &Message,
                                                  const QString &Origin,
                                                  const QString &Description,
                                                  const QString &Image,
                                                  const QString &Extra,
                                                  const QString &ProgressText,
                                                  float Progress,
                                                  int   Timeout,
                                                  bool  Fullscreen,
                                                  uint  Visibility,
                                                  uint  Priority ) = 0;
    virtual bool                 SendAction(const QString &Action,
                                            const QString &Value,
                                            uint Width, uint Height) = 0;
    virtual bool                 PlayRecording(int RecordedId, int ChanId,
                                               const QDateTime &StartTime) = 0;
    virtual bool                 PlayVideo(const QString &Id,
                                           bool  UseBookmark) = 0;
    virtual QStringList          GetContextList(void) = 0;
    virtual DTC::FrontendActionList* GetActionList(const QString &Context) = 0;
    virtual bool                 SendKey(const QString &Key) = 0;


};

#endif // FRONTENDSERVICES_H
