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

  public:
    template<class T> static inline HTTPServicePtr Create() { return std::shared_ptr<MythHTTPService>(new T); }

    explicit MythHTTPService(MythHTTPMetaService* MetaService);
   ~MythHTTPService() override;

    virtual HTTPResponse HTTPRequest(HTTPRequest2 Request);
    QString& Name();

  protected:
    QString m_name;
    MythHTTPMetaService* m_staticMetaService { nullptr };
    HTTPRequest2 m_request{nullptr};
};

class MBASE_PUBLIC V2HttpRedirectException
{
    public:

        QString m_hostName;

        explicit V2HttpRedirectException( QString sHostName = "")
               : m_hostName(std::move( sHostName ))
        {}

        ~V2HttpRedirectException() = default;
};


#define SERVICE_PROPERTY(Type, Name, name)               \
    Q_PROPERTY(Type Name READ Get##Name MEMBER m_##name USER true) \
    public:                                              \
        Type Get##Name() const { return m_##name; }      \
        void set##Name(Type value) { m_##name = value; } \
    private:                                             \
    Type m_##name { };

#define SERVICE_PROPERTY2(Type, Name)               \
    Q_PROPERTY(Type Name READ Get##Name MEMBER m_##Name USER true) \
    public:                                              \
        Type Get##Name() const { return m_##Name; }      \
        void set##Name(Type value) { m_##Name = value; } \
    private:                                             \
    Type m_##Name { };

#define SERVICE_PROPERTY_RO_REF( type, name ) \
    private: type m_##name;              \
    public:                              \
    type &name()                           /* NOLINT(bugprone-macro-parentheses) */ \
    {                                    \
        return m_##name;                 \
    }

#define SERVICE_PROPERTY_PTR( type, name )   \
    private: type* m_##name;              /* NOLINT(bugprone-macro-parentheses) */ \
    public:                             \
    type* name()                          /* NOLINT(bugprone-macro-parentheses) */ \
    {                                   \
        if (m_##name == nullptr)        \
            m_##name = new type( this );\
        return m_##name;                \
    }

template< class T >
void CopyListContents( QObject *pParent, QVariantList &dst, const QVariantList &src )
{
    for (const auto& vValue : qAsConst(src))
    {
        if ( vValue.canConvert< QObject* >())
        {
            const QObject *pObject = vValue.value< QObject* >();

            if (pObject != nullptr)
            {
                QObject *pNew = new T( pParent );

                ((T *)pNew)->Copy( (const T *)pObject );

                dst.append( QVariant::fromValue<QObject *>( pNew ));
            }
        }
    }
}

#endif
