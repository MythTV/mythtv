#ifndef FRONTEND_H
#define FRONTEND_H

#include <QScriptEngine>
#include "services/frontendServices.h"

class Frontend : public FrontendServices
{
    friend class MythFEXML;

    Q_OBJECT

  public:
    Q_INVOKABLE Frontend(QObject *parent = 0) : FrontendServices(parent) { }

  public:
    DTC::FrontendStatus* GetStatus(void);
    bool                 SendMessage(const QString &Message,
                                     uint Timeout);
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
                                          uint  Visibility, uint Priority);

    bool                 SendAction(const QString &Action,
                                    const QString &Value,
                                    uint Width, uint Height);
    virtual bool         PlayRecording(int ChanId,
                                       const QDateTime &StartTime);
    bool                 PlayVideo(const QString &Id, bool UseBookmark);
    QStringList          GetContextList(void);
    DTC::FrontendActionList* GetActionList(const QString &Context);


    static bool          IsValidAction(const QString &action);
    static void          InitialiseActions(void);

  protected:
    static QStringList gActionList;
    static QHash<QString,QStringList> gActionDescriptions;
};

class ScriptableFrontend : public QObject
{
    Q_OBJECT

  private:
    Frontend m_obj;

  public:
    Q_INVOKABLE ScriptableFrontend(QObject *parent = 0) : QObject(parent) { }

  public slots:
    QObject* GetStatus(void) { return m_obj.GetStatus(); }
};

Q_SCRIPT_DECLARE_QMETAOBJECT(ScriptableFrontend, QObject*);

#endif // FRONTEND_H
