#ifndef FRONTEND_H
#define FRONTEND_H

#include "libmythbase/mythconfig.h"
#include "servicecontracts/frontendServices.h"
#include "servicecontracts/service.h"

class Frontend : public FrontendServices
{
    friend class MythFEXML;

    Q_OBJECT

  public:
    Q_INVOKABLE explicit Frontend(QObject *parent = nullptr) : FrontendServices(parent) { }

  public:
    DTC::FrontendStatus* GetStatus(void) override; // FrontendServices
    bool                 SendMessage(const QString &Message,
                                     uint TimeoutInt) override; // FrontendServices
    bool                 SendNotification(bool  Error,
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
                                          uint  Visibility, uint Priority) override; // FrontendServices

    bool                 SendAction(const QString &Action,
                                    const QString &Value,
                                    uint Width, uint Height) override; // FrontendServices
    bool                 PlayRecording(int RecordedId, int ChanId,
                                       const QDateTime &StartTime) override; // FrontendServices
    bool                 PlayVideo(const QString &Id, bool UseBookmark) override; // FrontendServices
    QStringList          GetContextList(void) override; // FrontendServices
    DTC::FrontendActionList* GetActionList(const QString &Context) override; // FrontendServices


    static bool          IsValidAction(const QString &action);
    static void          InitialiseActions(void);
    bool                 SendKey(const QString &Key) override; // FrontendServices

  protected:
    static QStringList gActionList;
    static QHash<QString,QStringList> gActionDescriptions;
};

#endif // FRONTEND_H
