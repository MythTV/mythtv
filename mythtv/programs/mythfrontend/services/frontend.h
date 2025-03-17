#ifndef FRONTEND_H
#define FRONTEND_H

#include "libmythbase/mythconfig.h"
#if CONFIG_QTSCRIPT
#include <QScriptEngine>
#endif
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

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
//
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
//
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

#if CONFIG_QTSCRIPT
class ScriptableFrontend : public QObject
{
    Q_OBJECT

  private:
    Frontend m_obj;
    QScriptEngine *m_pEngine;

  public:
    Q_INVOKABLE explicit ScriptableFrontend( QScriptEngine *pEngine, QObject *parent = nullptr )
      : QObject( parent ), m_pEngine(pEngine)
    {
    }
  public slots:
    QObject* GetStatus(void) { SCRIPT_CATCH_EXCEPTION( nullptr,
                                    return m_obj.GetStatus(); ) }
};

// NOLINTNEXTLINE(modernize-use-auto)
Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV(ScriptableFrontend, QObject*);
#endif

#endif // FRONTEND_H
