#ifndef MYTHFRONTENDSERVICE_H
#define MYTHFRONTENDSERVICE_H

// MythTV
#include "libmythbase/http/mythhttpservice.h"

#define FRONTEND_SERVICE QString("/Frontend/")
#define FRONTEND_HANDLE  QString("Frontend")

class FrontendStatus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.1")
    Q_CLASSINFO("State",          "type=QString")
    Q_CLASSINFO("ChapterTimes",   "type=QString;name=Chapter")
    Q_CLASSINFO("SubtitleTracks", "type=QString;name=Track")
    Q_CLASSINFO("AudioTracks",    "type=QString;name=Track")
    SERVICE_PROPERTY2(QString,      Name)
    SERVICE_PROPERTY2(QString,      Version)
    SERVICE_PROPERTY2(QVariantMap,  State)
    SERVICE_PROPERTY2(QVariantList, ChapterTimes)
    SERVICE_PROPERTY2(QVariantMap,  SubtitleTracks)
    SERVICE_PROPERTY2(QVariantMap,  AudioTracks)

  public:
    Q_INVOKABLE FrontendStatus(QObject *parent = nullptr)
      : QObject  ( parent ) {};
    FrontendStatus(QString Name, QString Version, QVariantMap State);
};

Q_DECLARE_METATYPE(FrontendStatus*)

class FrontendActionList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("Version",    "1.0")
    Q_CLASSINFO("ActionList", "type=QString;name=Action")
    SERVICE_PROPERTY2(QVariantMap, ActionList)

  public:
    Q_INVOKABLE FrontendActionList(QObject *parent = nullptr)
      : QObject  ( parent ) {};
    FrontendActionList(QVariantMap List);
};

Q_DECLARE_METATYPE(FrontendActionList*)

class MythFrontendService : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",          "2.1")
    Q_CLASSINFO("SendAction",       "methods=POST")
    Q_CLASSINFO("SendKey",          "methods=POST")
    Q_CLASSINFO("PlayVideo",        "methods=POST")
    Q_CLASSINFO("PlayRecording",    "methods=POST")
    Q_CLASSINFO("SendMessage",      "methods=POST")
    Q_CLASSINFO("SendNotification", "methods=POST")
    Q_CLASSINFO("GetContextList",   "name=StringList") // Consistency with old code

  public slots:
    static bool SendAction      (const QString& Action, const QString& Value, uint Width, uint Height);
    static bool SendKey         (const QString& Key);
    static FrontendActionList* GetActionList(const QString& Context);
    static QStringList GetContextList  ();
    static FrontendStatus* GetStatus ();
    static bool PlayVideo       (const QString& Id, bool UseBookmark);
    static bool PlayRecording   (int RecordedId, int ChanId, const QDateTime& StartTime);
    static bool SendMessage     (const QString& Message, uint Timeout);
    static bool SendNotification(bool  Error,                const QString& Type,
                                 const QString& Message,     const QString& Origin,
                                 const QString& Description, const QString& Image,
                                 const QString& Extra,       const QString& ProgressText,
                                 float Progress,             std::chrono::seconds Timeout,
                                 bool  Fullscreen,           uint  Visibility,
                                 uint  Priority);

  public:
    MythFrontendService();
   ~MythFrontendService() override = default;
    static void RegisterCustomTypes();

  protected:
    static bool IsValidAction(const QString& Action);

  private:
    Q_DISABLE_COPY(MythFrontendService)
};

#endif
