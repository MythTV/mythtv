#ifndef MYTHWEBSOCKETTYPES_H
#define MYTHWEBSOCKETTYPES_H

// Std
#include <deque>

// Qt
#include <QString>

// MythTV
#include "libmythbase/http/mythhttpcommon.h"

enum WSOpCode : std::uint8_t
{
    WSOpContinuation = 0x0,
    WSOpTextFrame    = 0x1,
    WSOpBinaryFrame  = 0x2,
    WSOpReserved3    = 0x3,
    WSOpReserved4    = 0x4,
    WSOpReserved5    = 0x5,
    WSOpReserved6    = 0x6,
    WSOpReserved7    = 0x7,
    WSOpClose        = 0x8,
    WSOpPing         = 0x9,
    WSOpPong         = 0xA,
    WSOpReservedB    = 0xB,
    WSOpReservedC    = 0xC,
    WSOpReservedD    = 0xD,
    WSOpReservedE    = 0xE,
    WSOpReservedF    = 0xF
};

enum WSErrorCode : std::uint16_t
{
    WSCloseNormal        = 1000,
    WSCloseGoingAway     = 1001,
    WSCloseProtocolError = 1002,
    WSCloseUnsupported   = 1003,
    WSCloseNoStatus      = 1005,
    WSCloseAbnormal      = 1006,
    WSCloseBadData       = 1007,
    WSClosePolicy        = 1008,
    WSCloseTooLarge      = 1009,
    WSCloseNoExtensions  = 1010,
    WSCloseUnexpectedErr = 1011,
    WSCloseNoTLS         = 1012,
    WSCloseTLSHandshakeError = 1015
};

using WSQueue      = std::deque<DataPayload>;

class MythWebSocketFrame;
using WSFrame = std::shared_ptr<MythWebSocketFrame>;

class MythWebSocketFrame
{
  public:
    static WSFrame CreateFrame(const QByteArray& Header);

    bool       m_invalid { false };
    WSOpCode   m_opCode  { WSOpTextFrame };
    uint64_t   m_length  { 0 };
    bool       m_final   { false };
    bool       m_masked  { false };
    QByteArray m_mask;
    DataPayload  m_payload { nullptr };

  protected:
    MythWebSocketFrame(bool Invalid, WSOpCode Code, bool Final, bool Masked, uint64_t Length);
};


class MythWS
{
  public:
    static QString     OpCodeToString        (WSOpCode Code);
    static WSOpCode    FrameFormatForProtocol(MythSocketProtocol Protocol);
    static bool        UseRawTextForProtocol (MythSocketProtocol Protocol);
    static bool        OpCodeIsValid         (WSOpCode Code)
    {
        return Code == WSOpContinuation || Code == WSOpTextFrame ||
               Code == WSOpBinaryFrame  || Code == WSOpClose ||
               Code == WSOpPing         || Code == WSOpPong;
    }
};

#endif
