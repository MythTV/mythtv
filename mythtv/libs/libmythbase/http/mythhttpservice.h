#ifndef MYTHHTTPSERVICE_H
#define MYTHHTTPSERVICE_H

// Qt
#include <QObject>

// MythTV
#include "libmythbase/http/mythhttprequest.h"
#include "libmythbase/http/mythhttpresponse.h"

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
    template<class T> static HTTPServicePtr Create() { return std::shared_ptr<MythHTTPService>(new T); }

    explicit MythHTTPService(MythHTTPMetaService* MetaService);
   ~MythHTTPService() override = default;

    virtual HTTPResponse HTTPRequest(const HTTPRequest2& Request);
    QString& Name();

  protected:
    QString m_name;
    MythHTTPMetaService* m_staticMetaService { nullptr };
    HTTPRequest2 m_request{nullptr};
    bool HAS_PARAMv2(const QString& p)
        { return m_request->m_queries.contains(p.toLower()); }
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

//NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// @def SERVICE_PROPERTY(Type, Name)
///
/// Define a variable and its accessor functions. The variable will be
/// called 'm_Name', the getter function will be 'GetName' and the
/// setter will be 'SetName'.
///
/// @param Type The type of the variable to be defined. This also
///             control the type used in the getter and setter
///             functions.
///
/// @param Name The name of the variable to be defined. Also controls
///             the name of the getter and setter functions.
///
/// This macro uses templating so it can pass parameters by reference
/// for complex types, and pass by value for simple types. The first
/// template handles complex lvalue arguments, the second handles
/// complex rvalue arguments, and the third handles simple arguments.
#define SERVICE_PROPERTY2(Type, Name)               \
    Q_PROPERTY(Type Name READ Get##Name MEMBER m_##Name USER true) \
    public:                                              \
        Type Get##Name() const { return m_##Name; }      \
        template <class T>                                         \
        typename std::enable_if_t<std::is_same_v<T, QString> || \
                                  std::is_same_v<T, QStringList> || \
                                  std::is_same_v<T, QDateTime> || \
                                  std::is_same_v<T, QVariantMap> ||     \
                                  std::is_same_v<T, QVariantList>,void> \
        set##Name(const T& value) { m_##Name = value; }             \
        template <class T>                                          \
        typename std::enable_if_t<!std::is_same_v<T, QString> && \
                                  !std::is_same_v<T, QStringList> && \
                                  !std::is_same_v<T, QDateTime> && \
                                  !std::is_same_v<T, QVariantMap> &&    \
                                  !std::is_same_v<T, QVariantList>,void> \
        set##Name(T value) { m_##Name = value; }                    \
    private:                                             \
    Type m_##Name { }; // NOLINT(readability-redundant-member-init)

#define SERVICE_PROPERTY_RO_REF( type, name ) \
    private: type m_##name;              \
    public:                              \
    type &name()                           /* NOLINT(bugprone-macro-parentheses) */ \
    {                                    \
        return m_##name;                 \
    }

#define SERVICE_PROPERTY_PTR( type, name )   \
    private: type* m_##name {nullptr};    /* NOLINT(bugprone-macro-parentheses) */ \
    public:                             \
    type* name()                          /* NOLINT(bugprone-macro-parentheses) */ \
    {                                   \
        if (m_##name == nullptr)        \
            m_##name = new type( this );\
        return m_##name;                \
    }

#define SERVICE_PROPERTY_COND_PTR( type, name )   \
    private: type* m_##name {nullptr};    /* NOLINT(bugprone-macro-parentheses) */ \
             bool  m_b##name {true};    \
    public:                             \
    type* name()                          /* NOLINT(bugprone-macro-parentheses) */ \
    {                                   \
        if (m_##name == nullptr && m_b##name)     \
            m_##name = new type( this );\
        return m_##name;                \
    } \
    void enable##name(bool enabled)     \
    {                                   \
        m_b##name = enabled;            \
    }

//NOLINTEND(cppcoreguidelines-macro-usage)

template< class T >
void CopyListContents( QObject *pParent, QVariantList &dst, const QVariantList &src )
{
    for (const auto& vValue : std::as_const(src))
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
