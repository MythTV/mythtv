/* Class PList
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/**
 * \class PList
 *  A parser for binary property lists, using QVariant for internal
 *  storage. Values can be queried using GetValue and the structure
 *  can be exported to Xml with ToXml().
 *
 *  Support for importing Xml formatted property lists will be added.
 */

// TODO
// parse uid (and use QPair to differentiate?)

#include <QDateTime>
#include <QTextStream>
#include <QBuffer>

#include "mythlogging.h"
#include "plist.h"

#define LOC QString("PList: ")

#define MAGIC   QByteArray("bplist")
#define VERSION QByteArray("00")
#define MAGIC_SIZE   6
#define VERSION_SIZE 2
#define TRAILER_SIZE 26
#define MIN_SIZE (MAGIC_SIZE + VERSION_SIZE + TRAILER_SIZE)
#define TRAILER_OFFSIZE_INDEX  0
#define TRAILER_PARMSIZE_INDEX 1
#define TRAILER_NUMOBJ_INDEX   2
#define TRAILER_ROOTOBJ_INDEX  10
#define TRAILER_OFFTAB_INDEX   18

enum
{
    BPLIST_NULL    = 0x00,
    BPLIST_FALSE   = 0x08,
    BPLIST_TRUE    = 0x09,
    BPLIST_FILL    = 0x0F,
    BPLIST_UINT    = 0x10,
    BPLIST_REAL    = 0x20,
    BPLIST_DATE    = 0x30,
    BPLIST_DATA    = 0x40,
    BPLIST_STRING  = 0x50,
    BPLIST_UNICODE = 0x60,
    BPLIST_UID     = 0x70,
    BPLIST_ARRAY   = 0xA0,
    BPLIST_SET     = 0xC0,
    BPLIST_DICT    = 0xD0,
};

static void convert_float(quint8 *p, quint8 s)
{
#if HAVE_BIGENDIAN && !defined (__VFP_FP__)
    return;
#else
    for (quint8 i = 0; i < (s / 2); i++)
    {
        quint8 t = p[i];
        quint8 j = ((s - 1) + 0) - i;
        p[i] = p[j];
        p[j] = t;
    }
#endif
}

static quint8* convert_int(quint8 *p, quint8 s)
{
#if HAVE_BIGENDIAN
    return p;
#else
    for (quint8 i = 0; i < (s / 2); i++)
    {
        quint8 t = p[i];
        quint8 j = ((s - 1) + 0) - i;
        p[i] = p[j];
        p[j] = t;
    }
#endif
    return p;
}

PList::PList(const QByteArray &data)
  : m_data(NULL), m_offsetTable(NULL), m_rootObj(0),
    m_numObjs(0), m_offsetSize(0), m_parmSize(0)
{
    ParseBinaryPList(data);
}

QVariant PList::GetValue(const QString &key)
{
    if (m_result.type() != QVariant::Map)
        return QVariant();

    QVariantMap map = m_result.toMap();
    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        if (key == it.key())
            return it.value();
    }
    return QVariant();
}

QString PList::ToString(void)
{
    QByteArray res;
    QBuffer buf(&res);
    buf.open(QBuffer::WriteOnly);
    if (!ToXML(&buf))
        return QString("");
    return QString(res.data());
}

bool PList::ToXML(QIODevice *device)
{
    QXmlStreamWriter xml(device);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    xml.writeStartElement("plist");
    xml.writeAttribute("version", "1.0");
    bool success = ToXML(m_result, xml);
    xml.writeEndElement();
    xml.writeEndDocument();
    if (!success)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Invalid result.");
    return success;
}

bool PList::ToXML(const QVariant &data, QXmlStreamWriter &xml)
{
    switch (data.type())
    {
        case QVariant::Map:
            DictToXML(data, xml);
            break;
        case QVariant::List:
            ArrayToXML(data, xml);
            break;
        case QVariant::Double:
            xml.writeTextElement("real",
                                 QString("%1").arg(data.toDouble(), 0, 'f', 6));
            break;
        case QVariant::ByteArray:
            xml.writeTextElement("data",
                                 data.toByteArray().toBase64().data());
            break;
        case QVariant::ULongLong:
            xml.writeTextElement("integer",
                                 QString("%1").arg(data.toULongLong()));
            break;
        case QVariant::String:
            xml.writeTextElement("string", data.toString());
            break;
        case QVariant::DateTime:
            xml.writeTextElement("date", data.toDateTime().toString(Qt::ISODate));
            break;
        case QVariant::Bool:
            {
                bool val = data.toBool();
                xml.writeEmptyElement(val ? "true" : "false");
            }
            break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Unknown type.");
            return false;
    }
    return true;
}

void PList::DictToXML(const QVariant &data, QXmlStreamWriter &xml)
{
    xml.writeStartElement("dict");

    QVariantMap map = data.toMap();
    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        xml.writeStartElement("key");
        xml.writeCharacters(it.key());
        xml.writeEndElement();
        ToXML(it.value(), xml);
    }

    xml.writeEndElement();
}

void PList::ArrayToXML(const QVariant &data, QXmlStreamWriter &xml)
{
    xml.writeStartElement("array");

    QList<QVariant> list = data.toList();
    foreach (QVariant item, list)
        ToXML(item, xml);

    xml.writeEndElement();
}

void PList::ParseBinaryPList(const QByteArray &data)
{
    // reset
    m_result = QVariant();

    // check minimum size
    quint32 size = data.size();
    if (size < MIN_SIZE)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Binary: size %1, startswith '%2'")
        .arg(size).arg(data.left(8).data()));

    // check plist type & version
    if ((data.left(6) != MAGIC) ||
        (data.mid(MAGIC_SIZE, VERSION_SIZE) != VERSION))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unrecognised start sequence. Corrupt?");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Parsing binary plist (%1 bytes)")
        .arg(size));

    m_data = (quint8*)data.data();
    quint8* trailer = m_data + size - TRAILER_SIZE;
    m_offsetSize   = *(trailer + TRAILER_OFFSIZE_INDEX);
    m_parmSize = *(trailer + TRAILER_PARMSIZE_INDEX);
    m_numObjs  = *((quint64*)convert_int(trailer + TRAILER_NUMOBJ_INDEX, 8));
    m_rootObj  = *((quint64*)convert_int(trailer + TRAILER_ROOTOBJ_INDEX, 8));
    quint64 offset_tindex = *((quint64*)convert_int(trailer + TRAILER_OFFTAB_INDEX, 8));
    m_offsetTable = m_data + offset_tindex;

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("numObjs: %1 parmSize: %2 offsetSize: %3 rootObj: %4"
                "offset_tindex: %5").arg(m_numObjs).arg(m_parmSize)
                .arg(m_offsetSize).arg(m_rootObj).arg(offset_tindex));

    // something wrong?
    if (!m_numObjs || !m_parmSize || !m_offsetSize)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error parsing binary plist. Corrupt?");
        return;
    }

    // parse
    m_result = ParseBinaryNode(m_rootObj);

    LOG(VB_GENERAL, LOG_INFO, LOC + "Parse complete.");
}

QVariant PList::ParseBinaryNode(quint64 num)
{
    quint8* data = GetBinaryObject(num);
    if (!data)
        return QVariant();

    quint16 type = (*data) & 0xf0;
    quint64 size = (*data) & 0x0f;

    switch (type)
    {
        case BPLIST_SET:
        case BPLIST_ARRAY:   return ParseBinaryArray(data);
        case BPLIST_DICT:    return ParseBinaryDict(data);
        case BPLIST_STRING:  return ParseBinaryString(data);
        case BPLIST_UINT:    return ParseBinaryUInt(&data);
        case BPLIST_REAL:    return ParseBinaryReal(data);
        case BPLIST_DATE:    return ParseBinaryDate(data);
        case BPLIST_DATA:    return ParseBinaryData(data);
        case BPLIST_UNICODE: return ParseBinaryUnicode(data);
        case BPLIST_NULL:
        {
            switch (size)
            {
                case BPLIST_TRUE:  return QVariant(true);
                case BPLIST_FALSE: return QVariant(false);
                case BPLIST_NULL:
                default:           return QVariant();
            }
        }
        case BPLIST_UID: // FIXME
        default: return QVariant();
    }

    return QVariant();
}

quint64 PList::GetBinaryUInt(quint8 *p, quint64 size)
{
    if (size == 1) return (quint64)(*(p));
    if (size == 2) return (quint64)(*((quint16*)convert_int(p, 2)));
    if (size == 4) return (quint64)(*((quint32*)convert_int(p, 4)));
    if (size == 8) return (quint64)(*((quint64*)convert_int(p, 8)));

    if (size == 3)
    {
#if HAVE_BIGENDIAN
        return (quint64)(((*p) << 16) + (*(p + 1) << 8) + (*(p + 2)));
#else
        return (quint64)((*p) + (*(p + 1) << 8) + ((*(p + 2)) << 16));
#endif
    }

    return 0;
}

quint8* PList::GetBinaryObject(quint64 num)
{
    if (num > m_numObjs)
        return NULL;

    quint8* p = m_offsetTable + (num * m_offsetSize);
    quint64 offset = GetBinaryUInt(p, m_offsetSize);
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("GetBinaryObject num %1, offsize %2 offset %3")
        .arg(num).arg(m_offsetSize).arg(offset));

    return m_data + offset;
}

QVariantMap PList::ParseBinaryDict(quint8 *data)
{
    QVariantMap result;
    if (((*data) & 0xf0) != BPLIST_DICT)
        return result;

    quint64 count = GetBinaryCount(&data);

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Dict: Size %1").arg(count));

    if (!count)
        return result;

    quint64 off = m_parmSize * count;
    for (quint64 i = 0; i < count; i++, data += m_parmSize)
    {
        quint64 keyobj = GetBinaryUInt(data, m_parmSize);
        quint64 valobj = GetBinaryUInt(data + off, m_parmSize);
        QVariant key = ParseBinaryNode(keyobj);
        QVariant val = ParseBinaryNode(valobj);
        if (!key.canConvert<QString>())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid dictionary key type.");
            return result;
        }

        result.insert(key.toString(), val);
    }

    return result;
}

QList<QVariant> PList::ParseBinaryArray(quint8 *data)
{
    QList<QVariant> result;
    if (((*data) & 0xf0) != BPLIST_ARRAY)
        return result;

    quint64 count = GetBinaryCount(&data);

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Array: Size %1").arg(count));

    if (!count)
        return result;

    for (quint64 i = 0; i < count; i++, data += m_parmSize)
    {
        quint64 obj = GetBinaryUInt(data, m_parmSize);
        QVariant val = ParseBinaryNode(obj);
        result.push_back(val);
    }
    return result;
}

QVariant PList::ParseBinaryUInt(quint8 **data)
{
    quint64 result = 0;
    if (((**data) & 0xf0) != BPLIST_UINT)
        return QVariant(result);

    quint64 size = 1 << ((**data) & 0x0f);
    (*data)++;
    result = GetBinaryUInt(*data, size);
    (*data) += size;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UInt: %1").arg(result));
    return QVariant(result);
}

QVariant PList::ParseBinaryString(quint8 *data)
{
    QString result;
    if (((*data) & 0xf0) != BPLIST_STRING)
        return result;

    quint64 count = GetBinaryCount(&data);
    if (!count)
        return result;

    result = QString::fromLatin1((const char*)data, count);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("ASCII String: %1").arg(result));
    return QVariant(result);
}

QVariant PList::ParseBinaryReal(quint8 *data)
{
    double result = 0.0;
    if (((*data) & 0xf0) != BPLIST_REAL)
        return result;

    quint64 count = GetBinaryCount(&data);
    if (!count)
        return result;

    count = 1 << count;
    if (count == sizeof(float))
    {
        convert_float(data, count);
        result = (double)(*((float*)data));
    }
    else if (count == sizeof(double))
    {
        convert_float(data, count);
        result = *((double*)data);
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Real: %1").arg(result, 0, 'f', 6));
    return QVariant(result);
}

QVariant PList::ParseBinaryDate(quint8 *data)
{
    QDateTime result;
    if (((*data) & 0xf0) != BPLIST_DATE)
        return result;

    quint64 count = GetBinaryCount(&data);
    if (count != 3)
        return result;

    convert_float(data, 8);
    quint64 msec = *((double*)data) * 1000.0f;
    result = QDateTime::fromTime_t(msec / 1000);
    QTime time = result.time();
    time.addMSecs(msec % 1000);
    result.setTime(time);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Date: %1").arg(result.toString(Qt::ISODate)));
    return QVariant(result);
}

QVariant PList::ParseBinaryData(quint8 *data)
{
    QByteArray result;
    if (((*data) & 0xf0) != BPLIST_DATA)
        return result;

    quint64 count = GetBinaryCount(&data);
    if (!count)
        return result;

    result = QByteArray((const char*)data, count);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Data: Size %1 (count %2)")
        .arg(result.size()).arg(count));
    return QVariant(result);
}

QVariant PList::ParseBinaryUnicode(quint8 *data)
{
    QString result;
    if (((*data) & 0xf0) != BPLIST_UNICODE)
        return result;

    quint64 count = GetBinaryCount(&data);
    if (!count)
        return result;

    // source is big endian (and no BOM?)
    QByteArray tmp;
    for (quint64 i = 0; i < count; i++, data += 2)
    {
        quint16 twobyte = (quint16)(*((quint16*)convert_int(data, 2)));
        tmp.append((quint8)(twobyte & 0xff));
        tmp.append((quint8)((twobyte >> 8) & 0xff));
    }
    result = QString::fromUtf16((const quint16*)tmp.data(), count);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Unicode: %1").arg(result));
    return QVariant(result);
}

quint64 PList::GetBinaryCount(quint8 **data)
{
    quint64 count = (**data) & 0x0f;
    (*data)++;
    if (count == 0x0f)
    {
        QVariant newcount = ParseBinaryUInt(data);
        if (!newcount.canConvert<quint64>())
            return 0;
        count = newcount.toULongLong();
    }
    return count;
}
