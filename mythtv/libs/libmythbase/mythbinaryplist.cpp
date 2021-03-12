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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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

// Qt
#include <QDateTime>
#include <QSequentialIterable>
#include <QTextStream>
#include <QBuffer>

// MythTV
#include "mythlogging.h"
#include "mythbinaryplist.h"

// Std
#include <cmath>

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

static void convert_float(uint8_t *p, uint8_t s)
{
#if defined(HAVE_BIGENDIAN) && !defined (__VFP_FP__)
    return;
#else
    for (uint8_t i = 0; i < (s / 2); i++)
    {
        uint8_t t = p[i];
        uint8_t j = ((s - 1) + 0) - i;
        p[i] = p[j];
        p[j] = t;
    }
#endif
}

static uint8_t* convert_int(uint8_t *p, uint8_t s)
{
#if defined(HAVE_BIGENDIAN)
    return p;
#else
    for (uint8_t i = 0; i < (s / 2); i++)
    {
        uint8_t t = p[i];
        uint8_t j = ((s - 1) + 0) - i;
        p[i] = p[j];
        p[j] = t;
    }
#endif
    return p;
}

MythBinaryPList::MythBinaryPList(const QByteArray& Data)
{
    ParseBinaryPList(Data);
}

QVariant MythBinaryPList::GetValue(const QString& Key)
{
    if (m_result.type() != QVariant::Map)
        return QVariant();

    QVariantMap map = m_result.toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        if (Key == it.key())
            return it.value();
    return QVariant();
}

QString MythBinaryPList::ToString()
{
    QByteArray res;
    QBuffer buf(&res);
    buf.open(QBuffer::WriteOnly);
    if (!ToXML(&buf))
        return QString("");
    return QString(res.data());
}

bool MythBinaryPList::ToXML(QIODevice* Device)
{
    QXmlStreamWriter xml(Device);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeDTD(R"(<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">)");
    xml.writeStartElement("plist");
    xml.writeAttribute("version", "1.0");
    bool success = ToXML(m_result, xml);
    xml.writeEndElement();
    xml.writeEndDocument();
    if (!success)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Invalid result.");
    return success;
}

bool MythBinaryPList::ToXML(const QVariant& Data, QXmlStreamWriter& Xml)
{
    switch (Data.type())
    {
        case QVariant::Map:
            DictToXML(Data, Xml);
            break;
        case QVariant::List:
            ArrayToXML(Data, Xml);
            break;
        case QVariant::Double:
            Xml.writeTextElement("real", QString("%1").arg(Data.toDouble(), 0, 'f', 6));
            break;
        case QVariant::ByteArray:
            Xml.writeTextElement("data", Data.toByteArray().toBase64().data());
            break;
        case QVariant::ULongLong:
            Xml.writeTextElement("integer", QString("%1").arg(Data.toULongLong()));
            break;
        case QVariant::String:
            Xml.writeTextElement("string", Data.toString());
            break;
        case QVariant::DateTime:
            Xml.writeTextElement("date", Data.toDateTime().toString(Qt::ISODate));
            break;
        case QVariant::Bool:
            {
                bool val = Data.toBool();
                Xml.writeEmptyElement(val ? "true" : "false");
            }
            break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Unknown type.");
            return false;
    }
    return true;
}

void MythBinaryPList::DictToXML(const QVariant& Data, QXmlStreamWriter& Xml)
{
    Xml.writeStartElement("dict");
    QVariantMap map = Data.toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
    {
        Xml.writeStartElement("key");
        Xml.writeCharacters(it.key());
        Xml.writeEndElement();
        ToXML(it.value(), Xml);
    }
    Xml.writeEndElement();
}

void MythBinaryPList::ArrayToXML(const QVariant& Data, QXmlStreamWriter& Xml)
{
    Xml.writeStartElement("array");
    auto list = Data.value<QSequentialIterable>();
    for (const auto & item : qAsConst(list))
        ToXML(item, Xml);
    Xml.writeEndElement();
}

void MythBinaryPList::ParseBinaryPList(const QByteArray& Data)
{
    // reset
    m_result = QVariant();

    // check minimum size
    auto size = static_cast<quint32>(Data.size());
    if (size < MIN_SIZE)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Binary: size %1, startswith '%2'")
        .arg(size).arg(Data.left(8).data()));

    // check plist type & version
    if ((!Data.startsWith(MAGIC)) || (Data.mid(MAGIC_SIZE, VERSION_SIZE) != VERSION))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unrecognised start sequence. Corrupt?");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Parsing binary plist (%1 bytes)").arg(size));

    m_data = reinterpret_cast<uint8_t*>(const_cast<char*>(Data.data()));
    uint8_t* trailer = m_data + size - TRAILER_SIZE;
    m_offsetSize = *(trailer + TRAILER_OFFSIZE_INDEX);
    m_parmSize   = *(trailer + TRAILER_PARMSIZE_INDEX);
    m_numObjs    = *(reinterpret_cast<uint64_t*>(convert_int(trailer + TRAILER_NUMOBJ_INDEX, 8)));
    m_rootObj    = *(reinterpret_cast<uint64_t*>(convert_int(trailer + TRAILER_ROOTOBJ_INDEX, 8)));
    uint64_t offset_tindex = *(reinterpret_cast<uint64_t*>(convert_int(trailer + TRAILER_OFFTAB_INDEX, 8)));
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

QVariant MythBinaryPList::ParseBinaryNode(uint64_t Num)
{
    uint8_t* data = GetBinaryObject(Num);
    if (!data)
        return QVariant();

    quint16 type = (*data) & 0xf0;
    uint64_t size = (*data) & 0x0f;

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
        default: break;
    }

    return QVariant();
}

uint64_t MythBinaryPList::GetBinaryUInt(uint8_t *Data, uint64_t Size)
{
    if (Size == 1) return static_cast<uint64_t>(*Data);
    if (Size == 2) return static_cast<uint64_t>(*(reinterpret_cast<quint16*>(convert_int(Data, 2))));
    if (Size == 4) return static_cast<uint64_t>(*(reinterpret_cast<quint32*>(convert_int(Data, 4))));
    if (Size == 8) return                      (*(reinterpret_cast<uint64_t*>(convert_int(Data, 8))));
    if (Size == 3)
    {
#if defined(HAVE_BIGENDIAN)
        return static_cast<uint64_t>(((*Data) << 16) + (*(Data + 1) << 8) + (*(Data + 2)));
#else
        return static_cast<uint64_t>((*Data) + (*(Data + 1) << 8) + ((*(Data + 2)) << 16));
#endif
    }

    return 0;
}

uint8_t* MythBinaryPList::GetBinaryObject(uint64_t Num)
{
    if (Num > m_numObjs)
        return nullptr;

    uint8_t* p = m_offsetTable + (Num * m_offsetSize);
    uint64_t offset = GetBinaryUInt(p, m_offsetSize);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("GetBinaryObject num %1, offsize %2 offset %3")
        .arg(Num).arg(m_offsetSize).arg(offset));
    return m_data + offset;
}

QVariantMap MythBinaryPList::ParseBinaryDict(uint8_t *Data)
{
    QVariantMap result;
    if (((*Data) & 0xf0) != BPLIST_DICT)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Dict: Size %1").arg(count));
    if (!count)
        return result;

    uint64_t off = m_parmSize * count;
    for (uint64_t i = 0; i < count; i++, Data += m_parmSize)
    {
        uint64_t keyobj = GetBinaryUInt(Data, m_parmSize);
        uint64_t valobj = GetBinaryUInt(Data + off, m_parmSize);
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

QList<QVariant> MythBinaryPList::ParseBinaryArray(uint8_t* Data)
{
    QList<QVariant> result;
    if (((*Data) & 0xf0) != BPLIST_ARRAY)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Array: Size %1").arg(count));
    if (!count)
        return result;

    for (uint64_t i = 0; i < count; i++, Data += m_parmSize)
    {
        uint64_t obj = GetBinaryUInt(Data, m_parmSize);
        QVariant val = ParseBinaryNode(obj);
        result.push_back(val);
    }
    return result;
}

QVariant MythBinaryPList::ParseBinaryUInt(uint8_t** Data)
{
    uint64_t result = 0;
    if (((**Data) & 0xf0) != BPLIST_UINT)
        return QVariant(static_cast<quint64>(result));

    uint64_t size = 1 << ((**Data) & 0x0f);
    (*Data)++;
    result = GetBinaryUInt(*Data, size);
    (*Data) += size;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UInt: %1").arg(result));
    return QVariant(static_cast<quint64>(result));
}

QVariant MythBinaryPList::ParseBinaryString(uint8_t* Data)
{
    QString result;
    if (((*Data) & 0xf0) != BPLIST_STRING)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    if (!count)
        return result;

    result = QString::fromLatin1(reinterpret_cast<const char*>(Data), static_cast<int>(count));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("ASCII String: %1").arg(result));
    return QVariant(result);
}

QVariant MythBinaryPList::ParseBinaryReal(uint8_t* Data)
{
    double result = 0.0;
    if (((*Data) & 0xf0) != BPLIST_REAL)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    if (!count)
        return result;

    count = 1ULL << count;
    if (count == sizeof(float))
    {
        convert_float(Data, static_cast<uint8_t>(count));
        float temp = NAN;
        std::copy(Data, Data + sizeof(float), reinterpret_cast<uint8_t*>(&temp));
        result = static_cast<double>(temp);
    }
    else if (count == sizeof(double))
    {
        convert_float(Data, static_cast<uint8_t>(count));
        std::copy(Data, Data + sizeof(double), reinterpret_cast<uint8_t*>(&result));
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Real: %1").arg(result, 0, 'f', 6));
    return QVariant(result);
}

QVariant MythBinaryPList::ParseBinaryDate(uint8_t* Data)
{
    QDateTime result;
    if (((*Data) & 0xf0) != BPLIST_DATE)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    if (count != 3)
        return result;

    convert_float(Data, 8);
    auto temp = static_cast<double>(NAN);
    std::copy(Data, Data + sizeof(double), reinterpret_cast<uint8_t*>(&temp));
    auto msec = static_cast<uint64_t>(temp * 1000.0);
    result = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(msec), Qt::UTC);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Date: %1").arg(result.toString(Qt::ISODate)));
    return QVariant(result);
}

QVariant MythBinaryPList::ParseBinaryData(uint8_t* Data)
{
    QByteArray result;
    if (((*Data) & 0xf0) != BPLIST_DATA)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    if (!count)
        return result;

    result = QByteArray(reinterpret_cast<const char*>(Data), static_cast<int>(count));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Data: Size %1 (count %2)")
        .arg(result.size()).arg(count));
    return QVariant(result);
}

QVariant MythBinaryPList::ParseBinaryUnicode(uint8_t* Data)
{
    QString result;
    if (((*Data) & 0xf0) != BPLIST_UNICODE)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    if (!count)
        return result;

    // source is big endian (and no BOM?)
    QByteArray tmp;
    for (uint64_t i = 0; i < count; i++, Data += 2)
    {
        quint16 twobyte = *(reinterpret_cast<quint16*>(convert_int(Data, 2)));
        tmp.append(static_cast<char>(twobyte & 0xff));
        tmp.append(static_cast<char>((twobyte >> 8) & 0xff));
    }
    result = QString::fromUtf16(reinterpret_cast<const quint16*>(tmp.data()), static_cast<int>(count));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Unicode: %1").arg(result));
    return QVariant(result);
}

uint64_t MythBinaryPList::GetBinaryCount(uint8_t** Data)
{
    uint64_t count = (**Data) & 0x0f;
    (*Data)++;
    if (count == 0x0f)
    {
        QVariant newcount = ParseBinaryUInt(Data);
        if (!newcount.canConvert<uint64_t>())
            return 0;
        count = newcount.toULongLong();
    }
    return count;
}
