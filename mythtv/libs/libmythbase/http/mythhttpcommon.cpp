// Qt
#include <QStringList>

// MythTV
#include "http/mythhttpcommon.h"

DataPayload MythSharedData::CreatePayload(size_t Size)
{
    return std::shared_ptr<MythSharedData>(new MythSharedData(Size));
}

DataPayload MythSharedData::CreatePayload(const QString& Text)
{
    return std::shared_ptr<MythSharedData>(new MythSharedData(Text));
}

DataPayload MythSharedData::CreatePayload(const QByteArray &Other)
{
    return std::shared_ptr<MythSharedData>(new MythSharedData(Other));
}

MythSharedData::MythSharedData(uint64_t Size)
  : QByteArray(static_cast<int>(Size), Qt::Uninitialized)
{
}

MythSharedData::MythSharedData(const QString& Text)
  : QByteArray(Text.toUtf8())
{
}

MythSharedData::MythSharedData(const QByteArray& Other)
  : QByteArray(Other)
{
}

StringPayload MythSharedString::CreateString()
{
    return std::shared_ptr<MythSharedString>(new MythSharedString());
}

QString MythHTTPWS::ProtocolToString(MythSocketProtocol Protocol)
{
    switch (Protocol)
    {
        case ProtFrame:    return "Default";
        case ProtJSONRPC:  return JSONRPC;
        case ProtXMLRPC:   return XMLRPC;
        case ProtPLISTRPC: return PLISTRPC;
        case ProtCBORRPC:  return CBORRPC;
        default: break;
    }
    return "noprotocol";
}

MythSocketProtocol MythHTTPWS::ProtocolFromString(const QString &Protocols)
{
    auto ParseProtocol = [](const QString& Protocol)
    {
        if (Protocol.contains(JSONRPC))  return ProtJSONRPC;
        if (Protocol.contains(XMLRPC))   return ProtXMLRPC;
        if (Protocol.contains(PLISTRPC)) return ProtPLISTRPC;
        if (Protocol.contains(CBORRPC))  return ProtCBORRPC;
        return ProtFrame;
    };

    auto protocols = Protocols.trimmed().toLower().split(",", Qt::SkipEmptyParts);

    for (const auto & protocol : std::as_const(protocols))
        if (auto valid = ParseProtocol(protocol); valid != ProtFrame)
            return valid;

    return ProtFrame;
}
