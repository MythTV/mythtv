// Qt
#include <QMetaProperty>

// MythTV
#include "mythdate.h"
#include "http/mythhttpdata.h"
#include "http/serialisers/mythjsonserialiser.h"

MythJSONSerialiser::MythJSONSerialiser(const QString& Name, const QVariant& Value)
{
    m_first.push(true);
    m_writer.setDevice(&m_buffer);
    AddObject(Name, Value);
    m_writer.flush();
}

void MythJSONSerialiser::AddObject(const QString& Name, const QVariant& Value)
{
    if (!m_first.top())
        m_writer << ",";
    m_writer << "{\"" << Name << "\": ";
    AddValue(Value);
    m_writer << "}";
    m_first.top() = false;
}

void MythJSONSerialiser::AddValue(const QVariant& Value)
{
    if (Value.isNull() || !Value.isValid())
    {
        m_writer << "null";
        return;
    }

    if (Value.value<QObject*>())
    {
        AddQObject(Value.value<QObject*>());
        return;
    }

    switch (Value.type())
    {
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
        case QVariant::Double:     m_writer << Value.toString(); break;
        case QVariant::Bool:       m_writer << (Value.toBool() ? "true" : "false"); break;
        case QVariant::StringList: AddStringList(Value); break;
        case QVariant::List:       AddList(Value); break;
        case QVariant::Map:        AddMap(Value.toMap()); break;
        case QVariant::DateTime:   m_writer << "\"" << Encode(MythDate::toString(Value.toDateTime(), MythDate::ISODate)) << "\""; break;
        default:
            m_writer << "\"" << Encode(Value.toString()) << "\"";
    }
}

void MythJSONSerialiser::AddQObject(const QObject* Object)
{
    if (!Object)
        return;
    const auto * metaobject = Object->metaObject();
    int count = metaobject->propertyCount();
    m_first.push(true);
    QString first;
    m_writer << "{";
    for (int index = 0; index  < count; ++index  )
    {
        QMetaProperty metaProperty = metaobject->property(index);
        if (metaProperty.isUser(Object))
        {
            const char *rawname = metaProperty.name();
            QString name(rawname);
            if (name.compare("objectName") == 0)
                continue;
            QVariant value(Object->property(rawname));
            m_writer << first << "\"" << name << "\": ";
            AddValue(value);
            first = ", ";
        }
    }
    m_writer << "}";
    m_first.pop();
}

void MythJSONSerialiser::AddStringList(const QVariant &Values)
{
    QString first;
    m_writer << "[";
    QSequentialIterable values = Values.value<QSequentialIterable>();
    for (const auto & value : values)
    {
        m_writer << first << "\"" << Encode(value.toString()) << "\"";
        first = ",";
    }
    m_writer << "]";
}

void MythJSONSerialiser::AddList(const QVariant& Values)
{
    m_first.push(true);
    QString first;
    m_writer << "[";
    QSequentialIterable values = Values.value<QSequentialIterable>();
    for (const auto & value : values)
    {
        m_writer << first;
        AddValue(value);
        first = ",";
    }
    m_writer << "]";
    m_first.pop();
}

void MythJSONSerialiser::AddMap(const QVariantMap& Map)
{
    m_first.push(true);
    QString first;
    m_writer << "{";
    for (auto it = Map.cbegin(); it != Map.cend(); ++it)
    {
        m_writer << first << "\"" << it.key() << "\":";
        AddValue(it.value());
        first = ",";
    }
    m_writer << "}";
    m_first.pop();
}

QString MythJSONSerialiser::Encode(const QString& Value)
{
    if (Value.isEmpty())
        return Value;

    QString value = Value;
    value.replace( '\\', "\\\\" ); // This must be first
    value.replace( '"' , "\\\"" );
    value.replace( '\b', "\\b"  );
    value.replace( '\f', "\\f"  );
    value.replace( '\n', "\\n"  );
    value.replace( "\r", "\\r"  );
    value.replace( "\t", "\\t"  );
    value.replace(  "/", "\\/"  );

    // Officially we need to handle \u0000 - \u001F, but only a limited
    // number are actually used in the wild.
    value.replace(QChar('\u0011'), "\\u0011"); // XON  ^Q
    value.replace(QChar('\u0013'), "\\u0013"); // XOFF ^S
    return value;
}
