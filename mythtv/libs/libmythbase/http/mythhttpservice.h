#ifndef MYTHHTTPSERVICE_H
#define MYTHHTTPSERVICE_H

// Qt
#include <QObject>

// MythTV
#include "http/mythhttprequest.h"
#include "http/mythhttpresponse.h"

class MythHTTPMetaService;

/*! \class MythHTTPService
 *
 * This is the base class for a service whose public signals and slots are exported
 * as a publicly accessible web interface.
 *
*/
class MBASE_PUBLIC MythHTTPService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("Subscribe",   "methods=GET")
    Q_CLASSINFO("Unsubscribe", "methods=GET")

  public slots:
    bool Subscribe();
    void Unsubscribe();
    QVariantMap GetServiceDescription();

  public:
    template<class T> static inline HTTPServicePtr Create() { return std::shared_ptr<MythHTTPService>(new T); }

    explicit MythHTTPService(MythHTTPMetaService* MetaService);
   ~MythHTTPService() override;

    virtual HTTPResponse HTTPRequest(HTTPRequest2 Request);
    QString& Name();

  protected:
    QString m_name;
    MythHTTPMetaService* m_staticMetaService { nullptr };
};

#define SERVICE_PROPERTY(Type, Name, name)               \
    Q_PROPERTY(Type Name READ Get##Name MEMBER m_##name USER true) \
    public:                                              \
        Type Get##Name() const { return m_##name; }      \
    private:                                             \
    Type m_##name { };
#endif
