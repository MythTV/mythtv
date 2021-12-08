// Qt
#include <QStringList>

// MythTV
#include "http/mythwebsockettypes.h"

WSFrame MythWebSocketFrame::CreateFrame(const QByteArray &Header)
{
    if (Header.size() != 2)
        return nullptr;

    bool final      = (Header[0] & 0x80) != 0;
    auto code       = static_cast<WSOpCode>(Header[0] & 0x0F);
    bool masked     = (Header[1] & 0x80) != 0;
    uint64_t length = (Header[1] & 0x7F);
    bool invalid    = (Header[0] & 0x70) != 0;
    return std::shared_ptr<MythWebSocketFrame>(new MythWebSocketFrame(invalid, code, final, masked, length));
}

MythWebSocketFrame::MythWebSocketFrame(bool Invalid, WSOpCode Code, bool Final,
                                       bool Masked, uint64_t Length)
  : m_invalid(Invalid),
    m_opCode(Code),
    m_length(Length),
    m_final(Final),
    m_masked(Masked)
{
}

QString MythWS::OpCodeToString(WSOpCode Code)
{
    switch (Code)
    {
        case WSOpClose:        return "Close";
        case WSOpContinuation: return "Continuation";
        case WSOpPing:         return "Ping";
        case WSOpPong:         return "Pong";
        case WSOpBinaryFrame:  return "Binary";
        case WSOpTextFrame:    return "Text";
        default: break;
    }
    return "Reserved";
}

WSOpCode MythWS::FrameFormatForProtocol(MythSocketProtocol Protocol)
{
    switch (Protocol)
    {
        case ProtJSONRPC:
        case ProtXMLRPC:
        case ProtPLISTRPC: return WSOpTextFrame;
        case ProtCBORRPC:  return WSOpBinaryFrame;
        default: break;
    }
    return WSOpBinaryFrame;
}

bool MythWS::UseRawTextForProtocol(MythSocketProtocol Protocol)
{
    switch (Protocol)
    {
        case ProtFrame: // for testing atm
        case ProtJSONRPC: return true;
        default: break;
    }
    return false;
}
