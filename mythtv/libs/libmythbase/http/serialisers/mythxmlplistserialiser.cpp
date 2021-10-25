// Qt
#include <QMetaProperty>

// MythTV
#include "mythdate.h"
#include "http/mythhttpdata.h"
#include "http/serialisers/mythxmlplistserialiser.h"

MythXMLPListSerialiser::MythXMLPListSerialiser(const QString& Name, const QVariant& Value)
{
    m_writer.setDevice(&m_buffer);
    m_writer.setAutoFormatting(true);
    m_writer.setAutoFormattingIndent(4);
    m_writer.writeStartDocument("1.0");
    m_writer.writeDTD(R"(<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">)");
    m_writer.writeStartElement("plist");
    m_writer.writeAttribute("version", "1.0");
    m_writer.writeStartElement("dict");
    m_writer.writeTextElement("key", "serializerversion");
    m_writer.writeTextElement("string", XML_PLIST_SERIALIZER_VERSION);
    QString name = Name;
    if (name.startsWith("V2"))
        name.remove(0,2);
    AddObject(name, Value);
    m_writer.writeEndElement();
    m_writer.writeEndElement();
    m_writer.writeEndDocument();
}

void MythXMLPListSerialiser::AddObject(const QString& Name, const QVariant& Value)
{
    auto * object = Value.value<QObject*>();
    AddValue(object ? GetContentName(Name, object->metaObject()) : Name, Value);
}

void MythXMLPListSerialiser::AddValue(const QString& Name, const QVariant& Value, bool NeedKey)
{
    QObject* object = Value.value<QObject*>();
    if (object)
    {
        QVariant isNull = object->property("isNull");
        if (isNull.value<bool>())
            return;
        AddQObject(Name, object);
        return;
    }

    // The QT documentation states that Value.type() returns
    // a QVariant::Type which should be treated as a QMetaType::Type.
    // There is no QVariant::Float only a QMetaType::Float so we
    // need to cast it here so we can use QMetaType::Float without
    // warning messages.
    switch (static_cast<QMetaType::Type>(Value.type()))
    {
        case QVariant::StringList: AddStringList(Name, Value); break;
        case QVariant::List:       AddList(Name, Value.toList()); break;
        case QVariant::Map:        AddMap(Name, Value.toMap()); break;
        case QVariant::UInt:
        case QVariant::ULongLong:
        {
            if (NeedKey)
                m_writer.writeTextElement("key", Name);
            m_writer.writeTextElement("integer", QString::number(Value.toULongLong()));
            break;
        }

        case QVariant::Int:
        case QVariant::LongLong:
        case QVariant::Double:
        {
            if (NeedKey)
                m_writer.writeTextElement("key", Name);
            m_writer.writeTextElement("real", QString("%1").arg(Value.toDouble(), 0, 'f', 6));
            break;
        }
        case QMetaType::Float:
        {
            if (NeedKey)
                m_writer.writeTextElement("key", Name);
            m_writer.writeTextElement("real", QString("%1").arg(Value.toFloat(), 0, 'f', 6));
            break;
        }
        case QVariant::ByteArray:
        {
            if (!Value.toByteArray().isNull())
            {
                if (NeedKey)
                    m_writer.writeTextElement("key", Name);
                m_writer.writeTextElement("data", Value.toByteArray().toBase64().data());
            }
            break;
        }
        case QVariant::Bool:
        {
            if (NeedKey)
                m_writer.writeTextElement("key", Name);
            m_writer.writeEmptyElement(Value.toBool() ? "true" : "false");
            break;
        }
        case QVariant::DateTime:
            if (Value.toDateTime().isValid())
            {
                if (NeedKey)
                    m_writer.writeTextElement("key", Name);
                m_writer.writeTextElement("date", Value.toDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ"));
            }
            break;
        case QVariant::String:
        default:
            if (NeedKey)
                m_writer.writeTextElement("key", Name);
            m_writer.writeTextElement("string", Value.toString());
    }
}

void MythXMLPListSerialiser::AddQObject(const QString &Name, const QObject* Object)
{
    if (!Object)
        return;

    m_writer.writeTextElement("key", Name);
    m_writer.writeStartElement("dict");

    const auto * meta = Object->metaObject();
    if (int index = meta->indexOfClassInfo("Version"); index >= 0)
    {
        m_writer.writeTextElement("key", "version");
        m_writer.writeTextElement("string", meta->classInfo(index).value());
    }

    int count = meta->propertyCount();
    for (int index = 0; index  < count; ++index  )
    {
        QMetaProperty metaproperty = meta->property(index);
        if (metaproperty.isUser(Object))
        {
            const char *rawname = metaproperty.name();
            QString name(rawname);
            if (name.compare("objectName") == 0)
                continue;
            AddProperty(name, Object->property(rawname), meta, &metaproperty);
        }
    }

    m_writer.writeEndElement();
}

void MythXMLPListSerialiser::AddProperty(const QString& Name, const QVariant& Value,
                                    const QMetaObject* MetaObject, const QMetaProperty *MetaProperty)
{
    QString value;
    // Enum ?
    if (MetaProperty && (MetaProperty->isEnumType() || MetaProperty->isFlagType()))
    {
        QMetaEnum metaEnum = MetaProperty->enumerator();
        value = MetaProperty->isFlagType() ? metaEnum.valueToKeys(Value.toInt()).constData() :
                                             metaEnum.valueToKey(Value.toInt());
        return;
    }
    AddValue(GetContentName(Name, MetaObject), value.isEmpty() ? Value : value);
}

void MythXMLPListSerialiser::AddStringList(const QString& Name, const QVariant& Values)
{
    m_writer.writeTextElement("key", Name);
    m_writer.writeStartElement("array");
    QSequentialIterable values = Values.value<QSequentialIterable>();
    for (const auto & value : values)
        m_writer.writeTextElement("string", value.toString());
    m_writer.writeEndElement();
}

void MythXMLPListSerialiser::AddList(const QString& Name, const QVariantList &Values)
{
    bool array = true;
    if (!Values.isEmpty())
    {
        QVariant::Type type = Values.front().type();
        for (const auto & value : Values)
        {
            if (value.type() != type)
            {
                array = false;
                break;
            }
        }
    }

    QString name = GetItemName(Name);
    m_writer.writeTextElement("key", name);
    m_writer.writeStartElement(array ? "array" : "dict");

    QListIterator<QVariant> it(Values);
    while (it.hasNext())
        AddValue(name, it.next(), !array);
    m_writer.writeEndElement();
}

void MythXMLPListSerialiser::AddMap(const QString& Name, const QVariantMap& Map)
{
    QString itemname = GetItemName(Name);
    m_writer.writeTextElement("key", itemname);
    m_writer.writeStartElement("dict");
    for (auto it = Map.cbegin(); it != Map.cend(); ++it)
        AddValue(it.key(), it.value());
    m_writer.writeEndElement();
}

QString MythXMLPListSerialiser::GetItemName(const QString& Name)
{
    QString name = Name.startsWith("Q") ? Name.mid(1) : Name;
    name.remove("DTC::");
    if (name.startsWith("V2"))
        name.remove(0,2);
    name.remove(QChar('*'));
    return name;
}

/// FIXME We shouldn't be doing this on the fly
QString MythXMLPListSerialiser::GetContentName(const QString& Name, const QMetaObject* MetaObject)
{
    // Try to read Name or TypeName from classinfo metadata.
    if (int index = MetaObject ? MetaObject->indexOfClassInfo(Name.toLatin1()) : -1; index >= 0)
    {
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
        QStringList infos = QString(MetaObject->classInfo(index).value()).split(';', QString::SkipEmptyParts);
#else
        QStringList infos = QString(MetaObject->classInfo(index).value()).split(';', Qt::SkipEmptyParts);
#endif
        QString type; // fallback
        foreach (const QString &info, infos)
        {
            if (info.startsWith(QStringLiteral("name=")))
                if (auto name = info.mid(5).trimmed(); !name.isEmpty())
                    return GetItemName(name);
            if (info.startsWith(QStringLiteral("type=")))
                type = info.mid(5).trimmed();
        }
        if (!type.isEmpty())
            return GetItemName(type);
    }

    // Neither found, so lets use the type name (slightly modified).
    return GetItemName(Name);
}

