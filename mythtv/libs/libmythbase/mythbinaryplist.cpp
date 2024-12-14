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

// Std
#include <array>
#include <cmath>
#include <limits> // workaround QTBUG-90395

// Qt
#include <QtGlobal>
#include <QtEndian>
#include <QDateTime>
#include <QSequentialIterable>
#include <QTextStream>
#include <QTimeZone>
#include <QBuffer>

// MythTV
#include "mythlogging.h"
#include "mythbinaryplist.h"

#define LOC QString("PList: ")

static const QByteArray  MAGIC                  { "bplist" };
static const QByteArray  VERSION                { "00"     };
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
static constexpr int8_t  MAGIC_SIZE             { 6 };
static constexpr int8_t  VERSION_SIZE           { 2 };
static constexpr int8_t  TRAILER_SIZE           { 26 };
static constexpr int8_t  MIN_SIZE     { MAGIC_SIZE + VERSION_SIZE + TRAILER_SIZE};
#else
static constexpr ssize_t MAGIC_SIZE             { 6 };
static constexpr ssize_t VERSION_SIZE           { 2 };
static constexpr ssize_t TRAILER_SIZE           { 26 };
static constexpr ssize_t MIN_SIZE     { MAGIC_SIZE + VERSION_SIZE + TRAILER_SIZE};
#endif
static constexpr uint8_t TRAILER_OFFSIZE_INDEX  { 0 };
static constexpr uint8_t TRAILER_PARMSIZE_INDEX { 1 };
static constexpr uint8_t TRAILER_NUMOBJ_INDEX   { 2 };
static constexpr uint8_t TRAILER_ROOTOBJ_INDEX  { 10 };
static constexpr uint8_t TRAILER_OFFTAB_INDEX   { 18 };

// Apple's Core Data epoch starts 1/1/2001
static constexpr uint64_t CORE_DATA_EPOCH { 978307200 };

enum : std::uint8_t
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

template <typename T>
static T convert_float(const uint8_t *p)
{
// note: floating point endianness is not necessarily the same as integer endianness
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN) && !defined (__VFP_FP__)
    return *(reinterpret_cast<const T *>(p));
#else
    static std::array<uint8_t,sizeof(T)> temp;
    for (size_t i = 0; i < (sizeof(T) / 2); i++)
    {
        size_t j = sizeof(T) - 1 - i;
        temp[i] = p[j];
        temp[j] = p[i];
    }
    return *(reinterpret_cast<const T *>(temp.data()));
#endif
}

MythBinaryPList::MythBinaryPList(const QByteArray& Data)
{
    ParseBinaryPList(Data);
}

QVariant MythBinaryPList::GetValue(const QString& Key)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto type = static_cast<QMetaType::Type>(m_result.type());
#else
    auto type = m_result.typeId();
#endif

    if (type != QMetaType::QVariantMap)
        return {};

    QVariantMap map = m_result.toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        if (Key == it.key())
            return it.value();
    return {};
}

QString MythBinaryPList::ToString()
{
    QByteArray res;
    QBuffer buf(&res);
    buf.open(QBuffer::WriteOnly);
    if (!ToXML(&buf))
        return {""};
    return {res.data()};
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
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto type = static_cast<QMetaType::Type>(Data.type());
#else
    auto type = Data.typeId();
#endif
    switch (type)
    {
        case QMetaType::QVariantMap:
            DictToXML(Data, Xml);
            break;
        case QMetaType::QVariantList:
            ArrayToXML(Data, Xml);
            break;
        case QMetaType::Double:
            Xml.writeTextElement("real", QString("%1").arg(Data.toDouble(), 0, 'f', 6));
            break;
        case QMetaType::QByteArray:
            Xml.writeTextElement("data", Data.toByteArray().toBase64().data());
            break;
        case QMetaType::ULongLong:
            Xml.writeTextElement("integer", QString("%1").arg(Data.toULongLong()));
            break;
        case QMetaType::QString:
            Xml.writeTextElement("string", Data.toString());
            break;
        case QMetaType::QDateTime:
            Xml.writeTextElement("date", Data.toDateTime().toString(Qt::ISODate));
            break;
        case QMetaType::Bool:
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
    for (const auto & item : std::as_const(list))
        ToXML(item, Xml);
    Xml.writeEndElement();
}

void MythBinaryPList::ParseBinaryPList(const QByteArray& Data)
{
    // reset
    m_result = QVariant();

    // check minimum size
    auto size = Data.size();
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
    m_numObjs    = qFromBigEndian<quint64>(trailer + TRAILER_NUMOBJ_INDEX);
    m_rootObj    = qFromBigEndian<quint64>(trailer + TRAILER_ROOTOBJ_INDEX);
    auto offset_tindex = qFromBigEndian<quint64>(trailer + TRAILER_OFFTAB_INDEX);
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
        return {};

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
                case BPLIST_TRUE:  return {true};
                case BPLIST_FALSE: return {false};
                case BPLIST_NULL:
                default:           return {};
            }
        }
        case BPLIST_UID: // FIXME
        default: break;
    }

    return {};
}

uint64_t MythBinaryPList::GetBinaryUInt(uint8_t *Data, uint64_t Size)
{
    if (Size == 1) return static_cast<uint64_t>(*Data);
    if (Size == 2) return qFromBigEndian<quint16>(Data);
    if (Size == 4) return qFromBigEndian<quint32>(Data);
    if (Size == 8) return qFromBigEndian<quint64>(Data);
    if (Size == 3)
    {
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
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
        return {static_cast<quint64>(result)};

    uint64_t size = 1 << ((**Data) & 0x0f);
    (*Data)++;
    result = GetBinaryUInt(*Data, size);
    (*Data) += size;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UInt: %1").arg(result));
    return {static_cast<quint64>(result)};
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
    return {result};
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
        result = convert_float<float>(Data);
    }
    else if (count == sizeof(double))
    {
        result = convert_float<double>(Data);
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Real: %1").arg(result, 0, 'f', 6));
    return {result};
}

QVariant MythBinaryPList::ParseBinaryDate(uint8_t* Data)
{
    QDateTime result;
    if (((*Data) & 0xf0) != BPLIST_DATE)
        return result;

    uint64_t count = GetBinaryCount(&Data);
    if (count != 3)
        return result;

    auto sec = static_cast<uint64_t>(convert_float<double>(Data));
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    result = QDateTime::fromSecsSinceEpoch(CORE_DATA_EPOCH + sec, Qt::UTC);
#else
    result = QDateTime::fromSecsSinceEpoch(CORE_DATA_EPOCH + sec,
                                           QTimeZone(QTimeZone::UTC));
#endif

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Date: %1").arg(result.toString(Qt::ISODate)));
    return {result};
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
    return {result};
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
        auto twobyte = qFromBigEndian<quint16>(Data);
        tmp.append(static_cast<char>(twobyte & 0xff));
        tmp.append(static_cast<char>((twobyte >> 8) & 0xff));
    }
    result = QString::fromUtf16(reinterpret_cast<const char16_t*>(tmp.data()), static_cast<int>(count));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Unicode: %1").arg(result));
    return {result};
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
