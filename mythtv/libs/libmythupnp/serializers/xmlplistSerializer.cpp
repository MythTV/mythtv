/* Class XmlPListSerializer
*
* Copyright (C) Mark Kendall 2012
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include <QMetaClassInfo>
#include <QDateTime>

#include "xmlplistSerializer.h"

#define XMLPLIST_SERIALIZER_VERSION "1.0"

XmlPListSerializer::XmlPListSerializer(QIODevice *pDevice)
 : XmlSerializer( pDevice, QString("") )
{
}

XmlPListSerializer::~XmlPListSerializer()
{
}

void XmlPListSerializer::BeginSerialize(QString &sName)
{
    m_pXmlWriter->setAutoFormatting(true);
    m_pXmlWriter->setAutoFormattingIndent(4);
    m_pXmlWriter->writeStartDocument("1.0");
    m_pXmlWriter->writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    m_pXmlWriter->writeStartElement("plist");
    m_pXmlWriter->writeAttribute("version", "1.0");
    m_pXmlWriter->writeStartElement("dict"); // top level node
}

void XmlPListSerializer::EndSerialize(void)
{
    m_pXmlWriter->writeEndElement(); // "dict"
    m_pXmlWriter->writeEndElement(); // "plist"
    m_pXmlWriter->writeEndDocument();
}

QString XmlPListSerializer::GetContentType()
{
    return "text/x-apple-plist+xml";
}

void XmlPListSerializer::RenderValue(const QString &sName,
                                     const QVariant &vValue,
                                     bool  needKey)
{
    if ( vValue.canConvert<QObject*>())
    {
        const QObject *pObject = vValue.value<QObject*>();
        SerializePListObjectProperties(sName, pObject, needKey);
        return;
    }

    switch(vValue.type())
    {
        case QVariant::List:
        {
            RenderList(sName, vValue.toList());
            break;
        }

        case QVariant::StringList:
        {
            RenderStringList(sName, vValue.toStringList());
            break;
        }

        case QVariant::Map:
        {
            RenderMap(sName, vValue.toMap());
            break;
        }

        case QVariant::DateTime:
        {
            if (vValue.toDateTime().isValid())
            {
                if (needKey)
                    m_pXmlWriter->writeTextElement("key", sName);
                m_pXmlWriter->writeTextElement("date", vValue.toDateTime()
                                               .toUTC().toString("yyyy-MM-ddThh:mm:ssZ"));
            }
            break;
        }

        case QVariant::ByteArray:
        {
            if (!vValue.toByteArray().isNull())
            {
                if (needKey)
                    m_pXmlWriter->writeTextElement("key", sName);
                m_pXmlWriter->writeTextElement("data",
                                vValue.toByteArray().toBase64().data());
            }
            break;
        }

        case QVariant::Bool:
        {
            if (needKey)
                m_pXmlWriter->writeTextElement("key", sName);
            m_pXmlWriter->writeEmptyElement(vValue.toBool() ?
                                            "true" : "false");
            break;
        }

        case QVariant::UInt:
        case QVariant::ULongLong:
        {
            if (needKey)
                m_pXmlWriter->writeTextElement("key", sName);
            m_pXmlWriter->writeTextElement("integer",
                    QString::number(vValue.toULongLong()));
            break;
        }

        case QVariant::Int:
        case QVariant::LongLong:
        case QVariant::Double:
        {
            if (needKey)
                m_pXmlWriter->writeTextElement("key", sName);
            m_pXmlWriter->writeTextElement("real",
                    QString("%1").arg(vValue.toDouble(), 0, 'f', 6));
            break;
        }

        // anything else will be unrecognised, so wrap in a string
        case QVariant::String:
        default:
        {
            if (needKey)
                m_pXmlWriter->writeTextElement("key", sName);
            m_pXmlWriter->writeTextElement("string", vValue.toString());
            break;
        }
    }
}

void XmlPListSerializer::RenderList(const QString &sName,
                                    const QVariantList &list)
{
    bool array = true;
    if (!list.isEmpty())
    {
        QVariant::Type t = list[0].type();
        QListIterator<QVariant> it(list);
        while (it.hasNext())
        {
            if (it.next().type() != t)
            {
                array = false;
                break;
            }
        }
    }

    QString sItemName = GetItemName(sName);
    m_pXmlWriter->writeTextElement("key", sName);
    m_pXmlWriter->writeStartElement(array ? "array" : "dict");

    QListIterator<QVariant> it(list);
    while (it.hasNext())
        RenderValue(sItemName, it.next(), !array);

    m_pXmlWriter->writeEndElement();
}

void XmlPListSerializer::RenderStringList(const QString &sName,
                                          const QStringList &list)
{
    m_pXmlWriter->writeTextElement("key", sName);
    m_pXmlWriter->writeStartElement("array");

    QListIterator<QString> it(list);
    while (it.hasNext())
        m_pXmlWriter->writeTextElement("string", it.next());

    m_pXmlWriter->writeEndElement();
}

void XmlPListSerializer::RenderMap(const QString &sName,
                                   const QVariantMap &map)
{
    QString sItemName = GetItemName(sName);
    m_pXmlWriter->writeTextElement("key", sItemName);
    m_pXmlWriter->writeStartElement("dict");

    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        RenderValue(it.key(), it.value());
    }

    m_pXmlWriter->writeEndElement();
}

void XmlPListSerializer::BeginObject(const QString &sName,
                                     const QObject *pObject)
{
    const QMetaObject *pMeta = pObject->metaObject();
    int nIdx = pMeta->indexOfClassInfo("version");

    if (nIdx >=0)
    {
        m_pXmlWriter->writeTextElement("key", "version");
        m_pXmlWriter->writeTextElement("string", pMeta->classInfo(nIdx).value());
    }

    m_pXmlWriter->writeTextElement("key", "serializerversion");
    m_pXmlWriter->writeTextElement("string", XMLPLIST_SERIALIZER_VERSION);

    m_pXmlWriter->writeTextElement("key", sName);
    m_pXmlWriter->writeStartElement("dict");
}

void XmlPListSerializer::EndObject(const QString &sName,
                                   const QObject *pObject)
{
    m_pXmlWriter->writeEndElement(); // dict
}

void XmlPListSerializer::AddProperty(const QString &sName,
                                     const QVariant &vValue,
                                     const QMetaObject *pMetaParent,
                                     const QMetaProperty *pMetaProp)
{
    RenderValue(sName, vValue);
}

void XmlPListSerializer::SerializePListObjectProperties(const QString &sName,
                                                        const QObject *pObject,
                                                        bool          needKey )
{
    if (!pObject)
        return;

    if (needKey)
    {
        QString sItemName = GetItemName(sName);
        m_pXmlWriter->writeTextElement("key", sItemName);
    }
    m_pXmlWriter->writeStartElement("dict");

    const QMetaObject *pMetaObject = pObject->metaObject();

    int nCount = pMetaObject->propertyCount();

    for (int nIdx=0; nIdx < nCount; ++nIdx)
    {
        QMetaProperty metaProperty = pMetaObject->property(nIdx);

        if (metaProperty.isDesignable(pObject))
        {
            const char *pszPropName = metaProperty.name();
            QString     sPropName(pszPropName);

            if (sPropName.compare("objectName") == 0)
                continue;

            QVariant value(pObject->property(pszPropName));

            AddProperty(sPropName, value, pMetaObject, &metaProperty);
        }
    }

    m_pXmlWriter->writeEndElement();
}
