// Qt
#include <QDateTime>
#include <QFileInfo>
#include <QTimeZone>

// MythTV
#include "mythlogging.h"
#include "http/mythhttpmetamethod.h"

#define LOC QString("MetaMethod: ")

/*! \class MythHTTPMetaMethod
 *
 * \note Instances of this class SHOULD be initialised statically to improve performance but
 * MUST NOT be initialised globally. This is because a number of Qt types (e.g. enums)
 * and our own custom types will not have been initialised at that point
 * (i.e. qRegisterMetatype<>() has not been called).
*/
MythHTTPMetaMethod::MythHTTPMetaMethod(int Index, QMetaMethod& Method, int RequestTypes,
                                       const QString& ReturnName, bool Slot)
  : m_index(Index),
    m_requestTypes(RequestTypes),
    m_method(Method)
{
    // Static list of unsupported return types (cannot be serialised (QHash) or of no use (pointers))
    static const std::vector<int> s_invalidTypes =
    {
        QMetaType::UnknownType, QMetaType::VoidStar, QMetaType::QObjectStar,
        QMetaType::QVariantHash, QMetaType::QRect, QMetaType::QRectF,
        QMetaType::QSize, QMetaType::QSizeF, QMetaType::QLine,
        QMetaType::QLineF, QMetaType::QPoint, QMetaType::QPointF
    };

    // Static list of unsupported parameters (all invalid types plus extras)
    static const std::vector<int> s_invalidParams =
    {
        QMetaType::UnknownType, QMetaType::VoidStar, QMetaType::QObjectStar,
        QMetaType::QVariantHash, QMetaType::QRect, QMetaType::QRectF,
        QMetaType::QSize, QMetaType::QSizeF, QMetaType::QLine,
        QMetaType::QLineF, QMetaType::QPoint, QMetaType::QPointF,
        QMetaType::QVariantMap, QMetaType::QStringList, QMetaType::QVariantList
    };

    int returntype = Method.returnType();

    // Discard methods with an unsupported return type
    if (std::any_of(s_invalidTypes.cbegin(), s_invalidTypes.cend(), [&returntype](int Type) { return Type == returntype; }))
    {
        LOG(VB_HTTP, LOG_ERR, LOC + QString("Method '%1' has unsupported return type '%2'").arg(Method.name().constData(), Method.typeName()));
        return;
    }

    // Warn about complicated methods not supported by QMetaMethod - these will
    // fail if all arguments are required and used
    if (Method.parameterCount() > (Q_METAMETHOD_INVOKE_MAX_ARGS - 1))
    {
        LOG(VB_HTTP, LOG_WARNING, LOC + QString("Method '%1' takes more than %2 parameters; will probably fail")
            .arg(Method.name().constData()).arg(Q_METAMETHOD_INVOKE_MAX_ARGS - 1));
    }

    // Decide on the name of the returned type
    if (Slot && ValidReturnType(returntype))
    {
        // Explicitly set via Q_CLASSINFO (name=XXX)
        m_returnTypeName = ReturnName;
        if (m_returnTypeName.isEmpty())
        {
            // If this is a user type, we assume its name should be used, otherwise
            // prefer deduction from methodname
            if (returntype <= QMetaType::User)
                if (QString(Method.name()).startsWith(QStringLiteral("Get"), Qt::CaseInsensitive))
                    m_returnTypeName = Method.name().mid(3);
            // Default to the type name
            if (m_returnTypeName.isEmpty())
                m_returnTypeName = Method.typeName();
            // Duplicated from MythXMLSerialiser
            if (m_returnTypeName.startsWith("Q"))
                m_returnTypeName = m_returnTypeName.mid(1);
            m_returnTypeName.remove("DTC::");
            m_returnTypeName.remove(QChar('*'));
        }
    }

    // Add method name and return type
    m_types.emplace_back(returntype >= 0 ? returntype : 0);
    m_names.emplace_back(Method.name());

    // Add type/value for each method parameter
    auto names = Method.parameterNames();
    auto types = Method.parameterTypes();

    // Add type/value for each method parameter
    for (int i = 0; i < names.size(); ++i)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int type = QMetaType::type(types[i]);
#else
        int type = QMetaType::fromName(types[i]).id();
#endif

        // Discard methods that use unsupported parameter types.
        // Note: slots only - these are supportable for signals
        if (Slot && std::any_of(s_invalidParams.cbegin(), s_invalidParams.cend(), [&type](int Type) { return type == Type; }))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Method '%1' has unsupported parameter type '%2' (%3)")
                .arg(Method.name().constData(), types[i].constData()).arg(type));
            return;
        }

        m_names.emplace_back(names[i]);
        m_types.emplace_back(type);
    }

    m_valid = true;
}

HTTPMethodPtr MythHTTPMetaMethod::Create(int Index, QMetaMethod &Method, int RequestTypes,
                                         const QString &ReturnName, bool Slot)
{
    HTTPMethodPtr result =
        std::shared_ptr<MythHTTPMetaMethod>(new MythHTTPMetaMethod(Index, Method, RequestTypes, ReturnName, Slot));
    if (result->m_valid)
        return result;
    return nullptr;
}

/*! \brief Populate the QMetaType object referenced by Parameter with Value.
 *
 * \note In the event that the type is not handled, we return the unaltered object
 * which may then contain an undefined value.
*/
void* MythHTTPMetaMethod::CreateParameter(void* Parameter, int Type, const QString& Value)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QByteArray typeName = QMetaType::typeName(Type);
#else
    QByteArray typeName = QMetaType(Type).name();
#endif

    // Enum types
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto typeflags = QMetaType::typeFlags(Type);
#else
    auto typeflags = QMetaType(Type).flags();
#endif
    if ((typeflags & QMetaType::IsEnumeration) == QMetaType::IsEnumeration)
    {
        // QMetaEnum::keyToValue will return -1 for an unrecognised enumerant, so
        // default to -1 for all error cases
        int value = -1;
        if (int index = typeName.lastIndexOf("::" ); index > -1)
        {
            QString enumname = typeName.mid(index + 2);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            const auto * metaobject = QMetaType::metaObjectForType(Type);
#else
            const auto * metaobject = QMetaType(Type).metaObject();
#endif
            if (metaobject)
            {
                int enumindex = metaobject->indexOfEnumerator(enumname.toUtf8());
                if (enumindex >= 0)
                {
                    QMetaEnum metaEnum = metaobject->enumerator(enumindex);
                    value = metaEnum.keyToValue(Value.toUtf8());
                }
            }
        }
        *(static_cast<int*>(Parameter)) = value;
        return Parameter;
    }

    // Handle parameters of type std::chrono::seconds
    if (typeName == "std::chrono::seconds")
    {
        *(static_cast<std::chrono::seconds*>(Parameter)) = std::chrono::seconds(Value.toInt());
        return Parameter;
    }

    switch (Type)
    {
        case QMetaType::QVariant  : *(static_cast<QVariant     *>(Parameter)) = QVariant(Value); break;
        case QMetaType::Bool      : *(static_cast<bool         *>(Parameter)) = ToBool(Value ); break;
        case QMetaType::Char      : *(static_cast<char         *>(Parameter)) = (Value.length() > 0) ? Value.at(0).toLatin1() : 0; break;
        case QMetaType::UChar     : *(static_cast<unsigned char*>(Parameter)) = (Value.length() > 0) ? static_cast<unsigned char>(Value.at(0).toLatin1()) : 0; break;
        case QMetaType::QChar     : *(static_cast<QChar        *>(Parameter)) = (Value.length() > 0) ? Value.at(0) : QChar(0); break;
        case QMetaType::Short     : *(static_cast<short        *>(Parameter)) = Value.toShort(); break;
        case QMetaType::UShort    : *(static_cast<ushort       *>(Parameter)) = Value.toUShort(); break;
        case QMetaType::Int       : *(static_cast<int          *>(Parameter)) = Value.toInt(); break;
        case QMetaType::UInt      : *(static_cast<uint         *>(Parameter)) = Value.toUInt(); break;
        case QMetaType::Long      : *(static_cast<long         *>(Parameter)) = Value.toLong(); break;
        case QMetaType::ULong     : *(static_cast<ulong        *>(Parameter)) = Value.toULong(); break;
        case QMetaType::LongLong  : *(static_cast<qlonglong    *>(Parameter)) = Value.toLongLong(); break;
        case QMetaType::ULongLong : *(static_cast<qulonglong   *>(Parameter)) = Value.toULongLong(); break;
        case QMetaType::Double    : *(static_cast<double       *>(Parameter)) = Value.toDouble(); break;
        case QMetaType::Float     : *(static_cast<float        *>(Parameter)) = Value.toFloat(); break;
        case QMetaType::QString   : *(static_cast<QString      *>(Parameter)) = Value; break;
        case QMetaType::QByteArray: *(static_cast<QByteArray   *>(Parameter)) = Value.toUtf8(); break;
        case QMetaType::QTime     : *(static_cast<QTime        *>(Parameter)) = QTime::fromString(Value, Qt::ISODate ); break;
        case QMetaType::QDate     : *(static_cast<QDate        *>(Parameter)) = QDate::fromString(Value, Qt::ISODate ); break;
        case QMetaType::QDateTime :
        {
            QDateTime dt = QDateTime::fromString(Value, Qt::ISODate);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
            dt.setTimeSpec(Qt::UTC);
#else
            dt.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
            *(static_cast<QDateTime*>(Parameter)) = dt;
            break;
        }
        default:
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Unknown QMetaType:%1 %2").arg(Type).arg(QString(typeName)));
            break;
    }
    return Parameter;
}

QVariant MythHTTPMetaMethod::CreateReturnValue(int Type, void* Value)
{
    if (!(ValidReturnType(Type)))
        return {};

    // This assumes any user type will be derived from QObject...
    // (Exception for QFileInfo)
    if (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        Type == QMetaType::type("QFileInfo")
#else
        Type == QMetaType::fromName("QFileInfo").id()
#endif
        )
        return QVariant::fromValue<QFileInfo>(*(static_cast<QFileInfo*>(Value)));

    if (Type > QMetaType::User)
    {
        QObject* object = *(static_cast<QObject**>(Value));
        return QVariant::fromValue<QObject*>(object);
    }

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return {Type, Value};
#else
    return QVariant(QMetaType(Type), Value);
#endif
}
