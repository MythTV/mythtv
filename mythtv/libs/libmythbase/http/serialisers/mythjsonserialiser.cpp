// Qt
#include <QMetaProperty>
#include <QSequentialIterable>

// MythTV
#include "mythdate.h"
#include "http/mythhttpdata.h"
#include "http/serialisers/mythjsonserialiser.h"

MythJSONSerialiser::MythJSONSerialiser(const QString& Name, const QVariant& Value)
{
    m_first.push(true);
    m_writer.setDevice(&m_buffer);
    QString name = Name;
    if (name.startsWith("V2"))
        name.remove(0,2);
    AddObject(name, Value);
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

void MythJSONSerialiser::AddValue(const QVariant& Value, const QMetaProperty *MetaProperty)
{
    if (Value.isNull() || !Value.isValid())
    {
        m_writer << "null";
        return;
    }

    auto * object = Value.value<QObject*>();
    if (object)
    {
        QVariant isNull = object->property("isNull");
        if (isNull.value<bool>())
        {
            m_writer << "null";
            return;
        }
        AddQObject(object);
        return;
    }

    // Enum ?
    if (MetaProperty && (MetaProperty->isEnumType() || MetaProperty->isFlagType()))
    {
        QMetaEnum metaEnum = MetaProperty->enumerator();
        QString value = MetaProperty->isFlagType() ? metaEnum.valueToKeys(Value.toInt()).constData() :
                                                     metaEnum.valueToKey(Value.toInt());
        // If couldn't convert to enum name, return raw value
        value = (value.isEmpty() ? Value.toString() : value);
        m_writer << "\"" << Encode(value) << "\"";
        return;
    }

    switch (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        static_cast<QMetaType::Type>(Value.type())
#else
        static_cast<QMetaType::Type>(Value.typeId())
#endif
        )
    {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            m_writer << Value.toString();
            break;
        case QMetaType::Double:
            m_writer << QString::number(Value.toDouble(),'f',6);
            break;
        case QMetaType::Float:
            m_writer << QString::number(Value.toFloat(),'f',6);
            break;
        case QMetaType::Bool:         m_writer << (Value.toBool() ? "true" : "false"); break;
        case QMetaType::QStringList:  AddStringList(Value); break;
        case QMetaType::QVariantList: AddList(Value); break;
        case QMetaType::QVariantMap:  AddMap(Value.toMap()); break;
        case QMetaType::QDateTime:    m_writer << "\"" << Encode(MythDate::toString(Value.toDateTime(), MythDate::ISODate)) << "\""; break;
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
        if (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            metaProperty.isUser(Object)
#else
            metaProperty.isUser()
#endif
            )
        {
            const char *rawname = metaProperty.name();
            QString name(rawname);
            if (name.compare("objectName") == 0)
                continue;
            QVariant value(Object->property(rawname));
            m_writer << first << "\"" << name << "\": ";
            AddValue(value, &metaProperty);
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
    auto values = Values.value<QSequentialIterable>();
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
    auto values = Values.value<QSequentialIterable>();
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
    value.replace( '\b', "\\b"  ); // ^H (\u0008)
    value.replace( '\f', "\\f"  ); // ^L (\u000C)
    value.replace( '\n', "\\n"  ); // ^J (\u000A)
    value.replace( "\r", "\\r"  ); // ^M (\u000D)
    value.replace( "\t", "\\t"  ); // ^I (\u0009)
    value.replace(  "/", "\\/"  );

    // Escape remaining chars from \u0000 - \u001F
    // Details at https://en.wikipedia.org/wiki/C0_and_C1_control_codes
    value.replace(QChar('\u0000'), "\\u0000"); // ^@ NULL
    value.replace(QChar('\u0001'), "\\u0001"); // ^A
    value.replace(QChar('\u0002'), "\\u0002"); // ^B
    value.replace(QChar('\u0003'), "\\u0003"); // ^C
    value.replace(QChar('\u0004'), "\\u0004"); // ^D
    value.replace(QChar('\u0005'), "\\u0005"); // ^E
    value.replace(QChar('\u0006'), "\\u0006"); // ^F
    value.replace(QChar('\u0007'), "\\u0007"); // ^G (\a)
    value.replace(QChar('\u000B'), "\\u000B"); // ^K (\v)
    value.replace(QChar('\u000E'), "\\u000E"); // ^N
    value.replace(QChar('\u000F'), "\\u000F"); // ^O
    value.replace(QChar('\u0010'), "\\u0010"); // ^P
    value.replace(QChar('\u0011'), "\\u0011"); // ^Q XON
    value.replace(QChar('\u0012'), "\\u0012"); // ^R
    value.replace(QChar('\u0013'), "\\u0013"); // ^S XOFF
    value.replace(QChar('\u0014'), "\\u0014"); // ^T
    value.replace(QChar('\u0015'), "\\u0015"); // ^U
    value.replace(QChar('\u0016'), "\\u0016"); // ^V
    value.replace(QChar('\u0017'), "\\u0017"); // ^W
    value.replace(QChar('\u0018'), "\\u0018"); // ^X
    value.replace(QChar('\u0019'), "\\u0019"); // ^Y
    value.replace(QChar('\u001A'), "\\u001A"); // ^Z
    value.replace(QChar('\u001B'), "\\u001B");
    value.replace(QChar('\u001C'), "\\u001C");
    value.replace(QChar('\u001D'), "\\u001D");
    value.replace(QChar('\u001E'), "\\u001E");
    value.replace(QChar('\u001F'), "\\u001F");

    return value;
}
