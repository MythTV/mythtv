// Qt
#include <QMetaClassInfo>

// MythTV
#include "mythlogging.h"
#include "http/mythhttpmetaservice.h"
#include "http/mythhttptypes.h"

/*! \class MythHTTPMetaService
 *
 * \note Instances of this class SHOULD be initialised statically to improve performance but
 * MUST NOT be initialised globally. This is because a number of Qt types (e.g. enums)
 * and our own custom types will not have been initialised at that point
 * (i.e. qRegisterMetatype<>() has not been called).
*/
MythHTTPMetaService::MythHTTPMetaService(const QString& Name, const QMetaObject& Meta,
                                         const HTTPRegisterTypes& RegisterCallback,
                                         const QString& MethodsToHide)
  : m_meta(Meta),
    m_name(Name)
{
    // Register any types
    if (RegisterCallback != nullptr)
        std::invoke(RegisterCallback);

    // Static list of signals and slots to avoid
    static QString s_defaultHide = { "destroyed,deleteLater,objectNameChanged,RegisterCustomTypes" };

    // Retrieve version
    auto index = Meta.indexOfClassInfo("Version");
    if (index > -1)
        m_version = Meta.classInfo(index).value();
    else
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Service '%1' is missing version information").arg(Name));

    // Build complete list of meta objects for class hierarchy
    QList<const QMetaObject*> metas;
    metas.append(&Meta);
    const QMetaObject* super = Meta.superClass();
    while (super)
    {
        metas.append(super);
        super = super->superClass();
    }

    QString hide = s_defaultHide + MethodsToHide;

    // Pull out public signals and slots, ignoring 'MethodsToHide'
    for (const auto * meta : metas)
    {
        for (int i = 0; i < meta->methodCount(); ++i)
        {
            QMetaMethod method = meta->method(i);

            // We want only public methods
            if (QMetaMethod::Public != method.access())
                continue;

            // Filtered unwanted
            QString name(method.methodSignature());
            name = name.section('(', 0, 0);
            if (hide.contains(name))
                continue;

            auto RemoveExisting = [](HTTPMethods& Methods, const HTTPMethodPtr& Method, const QString& Search)
            {
                for (const auto & [_name, _method] : Methods)
                {
                    if ((_name == Search) && (_method->m_method.methodSignature() == Method->m_method.methodSignature()) &&
                         (_method->m_method.returnType() == Method->m_method.returnType()))
                    {
                        Methods.erase(_name);
                        break;
                    }
                }
            };

            // Signals
            if (QMetaMethod::Signal == method.methodType())
            {
                auto newmethod = MythHTTPMetaMethod::Create(i, method, HTTPUnknown, {}, false);
                if (newmethod)
                {
                    RemoveExisting(m_signals, newmethod, name);
                    m_signals.emplace(name, newmethod);
                }
            }
            // Slots
            else if (QMetaMethod::Slot == method.methodType())
            {
                QString returnname;
                int types = ParseRequestTypes(Meta, name, returnname);
                if (types != HTTPUnknown)
                {
                    auto newmethod = MythHTTPMetaMethod::Create(i, method, types, returnname);
                    if (newmethod)
                    {
                        newmethod->m_protected = isProtected(Meta, name);
                        RemoveExisting(m_slots, newmethod, name);
                        m_slots.emplace(name, newmethod);
                    }
                }
            }
        }
    }

    int constpropertyindex = -1;
    for (const auto * meta : metas)
    {
        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i)
        {
            QMetaProperty property = meta->property(i);
            QString propertyname(property.name());

            if (propertyname != QString("objectName") && property.isReadable() &&
                ((property.hasNotifySignal() && property.notifySignalIndex() > -1) || property.isConstant()))
            {
                // constant properties are given a signal index < 0
                if (property.notifySignalIndex() > -1)
                    m_properties.emplace(property.notifySignalIndex(), property.propertyIndex());
                else
                    m_properties.emplace(constpropertyindex--, property.propertyIndex());
            }
        }
    }
    LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' introspection complete").arg(Name));
}

int MythHTTPMetaService::ParseRequestTypes(const QMetaObject& Meta, const QString& Method, QString& ReturnName)
{
    int custom = HTTPUnknown;
    int index = Meta.indexOfClassInfo(Method.toLatin1());
    if (index > -1)
    {
        QStringList infos = QString(Meta.classInfo(index).value()).split(';', Qt::SkipEmptyParts);
        foreach (const QString &info, infos)
        {
            if (info.startsWith(QStringLiteral("methods=")))
                custom |= MythHTTP::RequestsFromString(info.mid(8));
            if (info.startsWith(QStringLiteral("name=")))
                ReturnName = info.mid(5).trimmed();
        }
    }

    // determine allowed request types
    int allowed = HTTPOptions;
    if (custom != HTTPUnknown)
    {
        allowed |= custom;
    }
    else if (Method.startsWith(QStringLiteral("Get"), Qt::CaseInsensitive))
    {
        allowed |= HTTPGet | HTTPHead | HTTPPost;
    }
    else if (Method.startsWith(QStringLiteral("Set"), Qt::CaseInsensitive))
    {
        // Put or Post?
        allowed |= HTTPPost;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to get request types for method '%1'- ignoring").arg(Method));
        return HTTPUnknown;
    }
    return allowed;
}

bool MythHTTPMetaService::isProtected(const QMetaObject& Meta, const QString& Method)
{
    int index = Meta.indexOfClassInfo(Method.toLatin1());
    if (index > -1)
    {
        QStringList infos = QString(Meta.classInfo(index).value()).split(';', Qt::SkipEmptyParts);
        auto isAuth = [](const QString& info)
            { return info.startsWith(QStringLiteral("AuthRequired=")); };
        return std::any_of(infos.cbegin(), infos.cend(), isAuth);
    }
    return false;
}
