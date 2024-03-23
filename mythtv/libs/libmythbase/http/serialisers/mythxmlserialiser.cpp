// Qt
#include <QMetaProperty>
#include <QSequentialIterable>

// MythTV
#include "mythdate.h"
#include "http/mythhttpdata.h"
#include "http/serialisers/mythxmlserialiser.h"

MythXMLSerialiser::MythXMLSerialiser(const QString& Name, const QVariant& Value)
{
    m_writer.setDevice(&m_buffer);
    m_writer.writeStartDocument("1.0");
    QString name = Name;
    if (name.startsWith("V2"))
        name.remove(0,2);
    AddObject(name, Value);
    m_writer.writeEndDocument();
}

void MythXMLSerialiser::AddObject(const QString& Name, const QVariant& Value)
{
    m_writer.writeStartElement(Name);
    if (m_first)
    {
        m_writer.writeAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
        m_writer.writeAttribute("serializerVersion", XML_SERIALIZER_VERSION);
        m_first = false;
    }
    auto * object = Value.value<QObject*>();
    AddValue(object ? GetContentName(Name, object->metaObject()) : Name, Value);
    m_writer.writeEndElement();
}

void MythXMLSerialiser::AddValue(const QString& Name, const QVariant& Value)
{
    if (Value.isNull() || !Value.isValid())
    {
        m_writer.writeAttribute("xsi:nil", "true");
        return;
    }

    auto * object = Value.value<QObject*>();
    if (object)
    {
        QVariant isNull = object->property("isNull");
        if (isNull.value<bool>())
            return;
        AddQObject(object);
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
        case QMetaType::QStringList:  AddStringList(Value); break;
        case QMetaType::QVariantList: AddList(Name, Value); break;
        case QMetaType::QVariantMap:  AddMap(Name, Value.toMap()); break;
        case QMetaType::QDateTime:
            {
                QDateTime dt(Value.toDateTime());
                if (dt.isNull())
                    m_writer.writeAttribute("xsi:nil", "true");
                else
                    m_writer.writeCharacters(MythDate::toString(dt, MythDate::ISODate));
            }
            break;
        case QMetaType::Double:
            m_writer.writeCharacters(QString::number(Value.toDouble(),'f',6));
            break;
        case QMetaType::Float:
            m_writer.writeCharacters(QString::number(Value.toFloat(),'f',6));
            break;
        default:
            m_writer.writeCharacters(Value.toString());
    }
}

void MythXMLSerialiser::AddQObject(const QObject* Object)
{
    if (!Object)
        return;

    const auto * meta = Object->metaObject();
    if (int index = meta->indexOfClassInfo("Version"); index >= 0)
        m_writer.writeAttribute("version", meta->classInfo(index).value());

    int count = meta->propertyCount();
    for (int index = 0; index  < count; ++index  )
    {
        QMetaProperty metaproperty = meta->property(index);
        if (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            metaproperty.isUser(Object)
#else
            metaproperty.isUser()
#endif
            )
        {
            const char *rawname = metaproperty.name();
            QString name(rawname);
            if (name.compare("objectName") == 0)
                continue;
            QVariant value(Object->property(rawname));
            m_writer.writeStartElement(name);
            AddProperty(name, value, meta, &metaproperty);
            m_writer.writeEndElement();
        }
    }
}

void MythXMLSerialiser::AddProperty(const QString& Name, const QVariant& Value,
                                    const QMetaObject* MetaObject, const QMetaProperty *MetaProperty)
{
    // Enum ?
    if (MetaProperty && (MetaProperty->isEnumType() || MetaProperty->isFlagType()))
    {
        QMetaEnum metaEnum = MetaProperty->enumerator();
        QString value = MetaProperty->isFlagType() ? metaEnum.valueToKeys(Value.toInt()).constData() :
                                                     metaEnum.valueToKey(Value.toInt());
        // If couldn't convert to enum name, return raw value
        m_writer.writeCharacters(value.isEmpty() ? Value.toString() : value);
        return;
    }

    AddValue(GetContentName(Name, MetaObject), Value);
}

void MythXMLSerialiser::AddStringList(const QVariant& Values)
{
    auto values = Values.value<QSequentialIterable>();
    for (const auto & value : values)
    {
        m_writer.writeStartElement("String");
        m_writer.writeCharacters(value.toString());
        m_writer.writeEndElement();
    }
}

void MythXMLSerialiser::AddList(const QString& Name, const QVariant& Values)
{
    auto values = Values.value<QSequentialIterable>();
    for (const auto & value : values)
    {
        m_writer.writeStartElement(Name);
        AddValue(Name, value);
        m_writer.writeEndElement();
    }
}

void MythXMLSerialiser::AddMap(const QString& Name, const QVariantMap& Map)
{
    QString itemname = GetItemName(Name);
    for (auto it = Map.cbegin(); it != Map.cend(); ++it)
    {
        m_writer.writeStartElement(itemname);
        m_writer.writeStartElement("Key");
        m_writer.writeCharacters(it.key());
        m_writer.writeEndElement();
        m_writer.writeStartElement("Value");
        AddValue(itemname, it.value());
        m_writer.writeEndElement();
        m_writer.writeEndElement();
    }
}

QString MythXMLSerialiser::GetItemName(const QString& Name)
{
    QString name = Name.startsWith("Q") ? Name.mid(1) : Name;
    if (name.startsWith("V2"))
        name.remove(0,2);
    name.remove(QChar('*'));
    return name;
}

/// FIXME We shouldn't be doing this on the fly
QString MythXMLSerialiser::GetContentName(const QString& Name, const QMetaObject* MetaObject)
{
    // Try to read Name or TypeName from classinfo metadata.
    if (int index = MetaObject ? MetaObject->indexOfClassInfo(Name.toLatin1()) : -1; index >= 0)
    {
        QStringList infos = QString(MetaObject->classInfo(index).value()).split(';', Qt::SkipEmptyParts);
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
