#ifndef MYTHHTTPCOMMON_H
#define MYTHHTTPCOMMON_H

// Std
#include <cstddef>
#include <cstdint>
#include <memory>

// Qt
#include <QString>
#include <QMetaType>

// MythTV
#include "libmythbase/mythbaseexp.h"

/// A group of functions shared between HTTP and WebSocket code.

#define HTTP_CHUNKSIZE INT64_C(65536) // 64k

#define JSONRPC  QStringLiteral("jsonrpc")
#define XMLRPC   QStringLiteral("xmlrpc")
#define PLISTRPC QStringLiteral("plistrpc")
#define CBORRPC  QStringLiteral("cborrpc")

enum MythSocketProtocol : std::uint8_t
{
    ProtHTTP = 0, // Socket has not been upgraded
    ProtFrame,    // Default WebSocket text and binary frame transmission
    ProtJSONRPC,
    ProtXMLRPC,
    ProtPLISTRPC,
    ProtCBORRPC
};

class MythSharedData;
using DataPayload  = std::shared_ptr<MythSharedData>;
using DataPayloads = std::vector<DataPayload>;

class MythSharedData : public QByteArray
{
  public:
    static DataPayload CreatePayload(size_t Size);
    static DataPayload CreatePayload(const QString& Text);
    static DataPayload CreatePayload(const QByteArray& Other);

    int64_t m_readPosition  { 0 };
    int64_t m_writePosition { 0 };

  protected:
    explicit MythSharedData(uint64_t Size);
    explicit MythSharedData(const QString& Text);
    explicit MythSharedData(const QByteArray& Other);
};

class MythSharedString;
using StringPayload = std::shared_ptr<MythSharedString>;

class MythSharedString : public QString
{
  public:
    static StringPayload CreateString();

  protected:
    MythSharedString() = default;
};

Q_DECLARE_METATYPE(DataPayload)
Q_DECLARE_METATYPE(DataPayloads)
Q_DECLARE_METATYPE(StringPayload)

class MythHTTPWS
{
  public:
    static QString ProtocolToString(MythSocketProtocol Protocol);
    static MythSocketProtocol ProtocolFromString(const QString& Protocols);
    static QString BitrateToString(uint64_t Rate)
    {
        if (Rate < 1)
            return "-";
        if (Rate > (1073741824LL * 1024))
            return ">1TBps";
        if (Rate >= 1073741824)
            return QStringLiteral("%1GBps").arg(Rate / 1073741824.0);
        if (Rate >= 1048576)
            return QStringLiteral("%1MBps").arg(Rate / 1048576.0);
        if (Rate >= 1024)
            return QStringLiteral("%1kBps").arg(Rate / 1024.0);
        return QStringLiteral("%1Bps").arg(Rate);
    }
};

#endif
