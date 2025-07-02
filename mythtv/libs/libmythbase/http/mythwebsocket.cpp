// Std
#include <algorithm>
#include <chrono>
using namespace std::chrono_literals;
#include <limits> // workaround QTBUG-90395

// Qt
#include <QThread>
#include <QtEndian>
#include <QTcpSocket>

// MythTV
#include "mythlogging.h"
#include "mythrandom.h"
#include "http/mythwebsocket.h"

#define LOC QString("WS: ")
static constexpr int64_t MAX_FRAME_SIZE   { 1048576 }; // 1MB
static constexpr int64_t MAX_MESSAGE_SIZE { 4194304 }; // 4MB

/*! \class MythWebSocket
 * \brief An implementation of the WebSocket protocol...
 *
 *
 * This currently passes all 303 Autobahn test suite conformance and performance
 * tests (1.* -> 10.*) with only 3 'non-strict' passes (6.4.2, 6.4.3 and 6.4.4).
 *
 * \note When not in testing mode, connections will be closed if a frame larger
 * than 1MB or a message larger than 4MB is received.
 *
*/
MythWebSocket* MythWebSocket::CreateWebsocket(bool Server, QTcpSocket *Socket,
                                              MythSocketProtocol Protocol, bool Testing)
{
    if (Socket && (MythSocketProtocol::ProtHTTP != Protocol))
        return new MythWebSocket(Server, Socket, Protocol, Testing);
    return nullptr;
}

MythWebSocket::MythWebSocket(bool Server, QTcpSocket *Socket, MythSocketProtocol Protocol, bool Testing)
  : m_socket(Socket),
    m_protocol(Protocol),
    m_formatForProtocol(MythWS::FrameFormatForProtocol(Protocol)),
    m_preferRawText(MythWS::UseRawTextForProtocol(Protocol)),
    m_testing(Testing),
    m_timer(Testing ? nullptr : new QTimer()),
    m_serverSide(Server)
{
    // Connect read/write signals
    connect(m_socket, &QTcpSocket::readyRead,    this, &MythWebSocket::Read);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &MythWebSocket::Write);

    LOG(VB_HTTP, LOG_INFO, LOC + QString("Websocket setup: Protocol:%1 (Raw text: %2) Testing:%3")
        .arg(MythHTTPWS::ProtocolToString(m_protocol))
        .arg(m_preferRawText).arg(m_testing));

    if (m_timer)
    {
        connect(m_timer, &QTimer::timeout, this, &MythWebSocket::SendPing);
        m_timer->start(20s);
    }
}

MythWebSocket::~MythWebSocket()
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    delete m_utf8CodecState;
#else
    delete m_toUtf16;
#endif
    delete m_timer;
}

/*! \brief Initiate the close handshake when we are exiting.
 *
 * \note This is intended to be used when the server or client connection is
 * exiting and we want to be a good netizen and send a proper close request. There
 * is however no expectation that the socket will remain open to complete the close
 * handshake; and thus the close will likely be 'unclean'.
*/
void MythWebSocket::Close()
{
    SendClose(WSCloseGoingAway, "Exiting");
}

void MythWebSocket::SendTextFrame(const QString& Text)
{
    SendFrame(WSOpTextFrame, { MythSharedData::CreatePayload(Text) });
}

void MythWebSocket::SendBinaryFrame(const QByteArray& Data)
{
    SendFrame(WSOpBinaryFrame, { MythSharedData::CreatePayload(Data) });
}

void MythWebSocket::Read()
{
    QString errormsg;
    while (m_socket->bytesAvailable())
    {
        errormsg.clear();
        // For small frames this should process everything without looping -
        // including zero length payloads
        if (ReadHeader == m_readState)
        {
            // Header must mean a new frame
            m_currentFrame = nullptr;

            // we need at least 2 bytes to start reading
            if (m_socket->bytesAvailable() < 2)
                return;

            m_currentFrame = MythWebSocketFrame::CreateFrame(m_socket->read(2));
            auto code = m_currentFrame->m_opCode;
            bool control = (code & 0x8) != 0;
            bool fragmented = (!m_dataFragments.empty()) || (m_string.get() != nullptr);
            bool final = m_currentFrame->m_final;

            // Invalid use of reserved bits
            if (m_currentFrame->m_invalid)
            {
                errormsg = QStringLiteral("Invalid use of reserved bits");
            }
            // Valid opcode
            else if (!MythWS::OpCodeIsValid(code))
            {
                errormsg = QStringLiteral("Invalid use of reserved opcode");
            }
            // Clients must be sending masked frames and vice versa
            else if (m_serverSide != m_currentFrame->m_masked)
            {
                errormsg = QStringLiteral("Masking error");
            }
            // Invalid control frame size
            else if (control && (m_currentFrame->m_length > 125))
            {
                errormsg = QStringLiteral("Control frame payload too large");
                // need to acknowledge when an OpClose is received
                if (WSOpClose == m_currentFrame->m_opCode)
                    m_closeReceived = true;
            }
            // Control frames cannot be fragmented
            else if (control && !final)
            {
                errormsg = QStringLiteral("Fragmented control frame");
            }
            // Continuation frame must have an opening frame
            else if (!fragmented && code == WSOpContinuation)
            {
                errormsg = QStringLiteral("Fragmentation error (no opening frame)");
            }
            // Only continuation frames or control frames are expected once in the middle of a message
            else if (fragmented && (code != WSOpContinuation) && !control)
            {
                errormsg = QStringLiteral("Fragmentation error (bad frame)");
            }
            // ensure OpCode matches that expected by SubProtocol
            else if ((!m_testing && m_protocol != ProtFrame) &&
                     (code == WSOpTextFrame || code == WSOpBinaryFrame) &&
                      code != m_formatForProtocol)
            {
                errormsg = QStringLiteral("Received incorrect frame type for subprotocol");
            }

            if (!errormsg.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + errormsg);
                SendClose(WSCloseProtocolError, errormsg);
                // Invalidate the frame so it is ignored, but we must carry
                // on to ensure we process the close reply
                m_currentFrame->m_invalid = true;
            }

            if (126 == m_currentFrame->m_length)
            {
                m_readState = Read16BitLength;
            }
            else if (127 == m_currentFrame->m_length)
            {
                m_readState = Read64BitLength;
            }
            else
            {
                m_readState = m_currentFrame->m_masked ? ReadMask : ReadPayload;
                m_currentFrame->m_payload = MythSharedData::CreatePayload(m_currentFrame->m_length);
            }

            // Set the OpCode for the entire message in the case of fragmented messages
            if (!final && (code == WSOpBinaryFrame || code == WSOpTextFrame))
                m_messageOpCode = code;
        }

        if (Read16BitLength == m_readState)
        {
            if (m_socket->bytesAvailable() < 2)
                return;
            auto length = m_socket->read(2);
            auto size = qFromBigEndian<quint16>(reinterpret_cast<const void *>(length.data()));
            m_currentFrame->m_payload = MythSharedData::CreatePayload(size);
            m_currentFrame->m_length = size;
            m_readState = m_currentFrame->m_masked ? ReadMask : ReadPayload;
        }

        if (Read64BitLength == m_readState)
        {
            if (m_socket->bytesAvailable() < 8)
                return;
            auto length = m_socket->read(8);
            auto size = qFromBigEndian<quint64>(reinterpret_cast<const void *>(length.data()));
            m_currentFrame->m_payload = MythSharedData::CreatePayload(size);
            m_currentFrame->m_length = size;
            m_readState = m_currentFrame->m_masked ? ReadMask : ReadPayload;

            // Large frame sizes are not needed with current implementations
            if (!m_testing && (size > MAX_FRAME_SIZE))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Frame size is too large");
                SendClose(WSCloseTooLarge);
                m_currentFrame->m_invalid = true;
            }
        }

        if (ReadMask == m_readState)
        {
            if (m_socket->bytesAvailable() < 4)
                return;
            m_currentFrame->m_mask = m_socket->read(4);
            m_readState = ReadPayload;
        }

        if (ReadPayload == m_readState)
        {
            if (!m_currentFrame.get())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "No frame");
                SendClose(WSCloseUnexpectedErr, QStringLiteral("Internal error"));
                return;
            }

            auto & payload = m_currentFrame->m_payload;
            if (!payload)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "No payload allocated");
                SendClose(WSCloseUnexpectedErr, QStringLiteral("Internal error"));
                return;
            }

            auto available = m_socket->bytesAvailable();
            auto bytesneeded = payload->size() - payload->m_readPosition;
            // Note: payload size can be zero
            if (bytesneeded > 0 && available > 0)
            {
                auto read = m_socket->read(payload->data() + m_currentFrame->m_payload->m_readPosition,
                                           bytesneeded > available ? available : bytesneeded);
                if (read < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "Read error");
                    SendClose(WSCloseUnexpectedErr, QStringLiteral("Read error"));
                    return;
                }
                payload->m_readPosition += read;
                bytesneeded -= read;
                LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Payload read: %1 (%2 of %3)")
                    .arg(read).arg(payload->m_readPosition).arg(payload->size()));
            }

            // Have we finished reading the current frame
            if (bytesneeded < 1)
            {
                bool valid = !m_currentFrame->m_invalid;

                // unmask payload - TODO Optimise this (e.g SIMD)
                if (valid && m_currentFrame->m_masked)
                    for (int i = 0; i < payload->size(); ++i)
                        payload->data()[i] ^= m_currentFrame->m_mask[i % 4];

                // Ensure we have the current *message* opcode
                auto messageopcode = m_currentFrame->m_opCode;
                if (WSOpContinuation == messageopcode)
                    messageopcode = m_messageOpCode;

                // Validate UTF-8 text on a frame by frame basis, accumulating
                // into our string pointer. This optimises UTF-8 handling for
                // internal purposes by converting only once but is less efficient
                // for echo testing; as we immediately convert the QString back to UTF-8.
                // Note: In an ideal world we would avoid reading to an intermediary
                // buffer before converting. QTextStream notionally supports reading
                // and converting in a single operation (and appears to support
                // error detection in the conversion) but it can only be used to
                // read text; its internal buffering will break direct reads
                // from the socket (for headers, binary frames etc).
                if (WSOpTextFrame == messageopcode)
                {
                    m_messageSize += payload->size();

                    // Note: we check invalidChars here and remainingChars
                    // when the full message has been received - as fragments
                    // may not be on code point boundaries; whatever that means :)
                    // Note: This still does not catch 3 'fail fast on UTF-8'
                    // Autobahn tests - though we still pass with 'non-strict'.
                    // I can't see a way to pass strictly without breaking
                    // other tests for valid UTF-8
                    if (m_preferRawText)
                    {
                        m_dataFragments.emplace_back(payload);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                        (void)m_utf8Codec->toUnicode(payload->data(), payload->size(), m_utf8CodecState);
#endif
                    }
                    else
                    {
                        if (!m_string)
                            m_string = MythSharedString::CreateString();
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                        (*m_string).append(m_utf8Codec->toUnicode(payload->data(), payload->size(), m_utf8CodecState));
#else
                        (*m_string).append(m_toUtf16->decode(*payload));
#endif
                    }

                    if (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                        m_utf8CodecState->invalidChars
#else
                        m_toUtf16->hasError()
#endif
                        )
                    {
                        LOG(VB_HTTP, LOG_ERR, LOC + "Invalid UTF-8");
                        SendClose(WSCloseBadData, "Invalid UTF-8");
                        m_currentFrame->m_invalid = true;
                        valid = false;
                    }
                }

                // Add this data payload to the message payload
                if (WSOpBinaryFrame == messageopcode)
                {
                    m_dataFragments.emplace_back(payload);
                    m_messageSize += payload->size();
                }

                // Stop clients from sending large messages.
                if (!m_testing && (m_messageSize > MAX_MESSAGE_SIZE))
                {
                    LOG(VB_HTTP, LOG_ERR, LOC + "Message size is too large");
                    SendClose(WSCloseTooLarge);
                    m_currentFrame->m_invalid = true;
                    valid = false;
                }

                LOG(VB_HTTP, LOG_DEBUG, LOC + QString("%1 frame: Final %2 Length: %3 Masked: %4 Valid: %5")
                    .arg(MythWS::OpCodeToString(m_currentFrame->m_opCode))
                    .arg(m_currentFrame->m_final).arg(m_currentFrame->m_length)
                    .arg(m_currentFrame->m_masked).arg(valid));

                // Message complete?
                if (m_currentFrame->m_final)
                {
                    // Control frames (no fragmentation - so only one payload)
                    if (valid && WSOpPing == m_currentFrame->m_opCode)
                    {
                        PingReceived(payload);
                    }
                    else if (valid && WSOpPong == m_currentFrame->m_opCode)
                    {
                        PongReceived(payload);
                    }
                    else if (valid && WSOpClose == m_currentFrame->m_opCode)
                    {
                        CloseReceived(payload);
                    }
                    else
                    {
                        // Final UTF-8 validation
                        if ((WSOpTextFrame == messageopcode))
                        {
                            if (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                                m_utf8CodecState->remainingChars
#else
                                m_toUtf16->hasError()
#endif
                                )
                            {
                                LOG(VB_HTTP, LOG_ERR, LOC + "Invalid UTF-8");
                                SendClose(WSCloseBadData, "Invalid UTF-8");
                                valid = false;
                            }
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                            delete m_utf8CodecState;
                            m_utf8CodecState = new QTextCodec::ConverterState;
#else
                            delete m_toUtf16;
                            m_toUtf16 = new QStringDecoder;
#endif
                        }

                        // Echo back to the Autobahn server
                        if (valid && m_testing)
                        {
                            if (WSOpTextFrame == messageopcode && m_preferRawText)
                                SendFrame(WSOpTextFrame, m_dataFragments);
                            else if (WSOpTextFrame == messageopcode)
                                SendFrame(WSOpTextFrame, { MythSharedData::CreatePayload((*m_string)) });
                            else if (WSOpBinaryFrame == messageopcode)
                                SendFrame(WSOpBinaryFrame, m_dataFragments);
                        }
                        else if (valid)
                        {
                            if (WSOpTextFrame == messageopcode&& m_preferRawText)
                                emit NewRawTextMessage(m_dataFragments);
                            else if (WSOpTextFrame == messageopcode)
                                emit NewTextMessage(m_string);
                            else if (WSOpBinaryFrame == messageopcode)
                                emit NewBinaryMessage(m_dataFragments);
                        }

                        // Cleanup
                        m_dataFragments.clear();
                        m_string = nullptr;
                    }
                }

                // Next read must be a new frame with a fresh header
                m_currentFrame = nullptr;
                m_readState = ReadHeader;
            }
        }
    }
}

void MythWebSocket::Write(int64_t Written)
{
    auto buffered = m_socket->bytesToWrite();
    if (Written)
    {
        LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Socket sent %1bytes (still to send %2)")
            .arg(Written).arg(buffered));

        if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_INFO) && m_writeQueue.empty() && !buffered)
        {
            if (m_writeTotal > 100)
            {
                auto seconds = static_cast<double>(m_writeTime.nsecsElapsed()) / 1000000000.0;
                auto rate = static_cast<uint64_t>(static_cast<double>(m_writeTotal) / seconds);
                LOG(VB_HTTP, LOG_INFO, LOC + QString("Wrote %1bytes in %2seconds (%3)")
                    .arg(m_writeTotal).arg(seconds, 8, 'f', 6, '0').arg(MythHTTPWS::BitrateToString(rate)));
            }
            m_writeTime.invalidate();
            m_writeTotal = 0;
        }
    }

    if (m_writeQueue.empty())
        return;

    if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_INFO) && !m_writeTime.isValid())
        m_writeTime.start();

    // If the client cannot consume data as fast as we are sending it, we just
    // buffer more and more data in the Qt socket code. So return and wait for
    // the next write.
    if (buffered > (HTTP_CHUNKSIZE << 1))
    {
        LOG(VB_HTTP, LOG_DEBUG, LOC + "Draining buffers");
        return;
    }

    int64_t available = HTTP_CHUNKSIZE;
    while ((available > 0) && !m_writeQueue.empty())
    {
        auto & payload = m_writeQueue.front();
        auto written = payload->m_writePosition;
        auto towrite = static_cast<int64_t>(payload->size()) - written;
        towrite = std::min(towrite, available);
        auto wrote = m_socket->write(payload->constData() + written, towrite);

        if (wrote < 0)
        {
            // If there is a write error, there is little option other than to
            // fail the socket
            LOG(VB_GENERAL, LOG_ERR, LOC + "Write error");
            m_socket->close();
            return;
        }

        payload->m_writePosition += wrote;
        available -= wrote;
        m_writeTotal += wrote;

        // Have we finished with this payload
        if (payload->m_writePosition >= payload->size())
            m_writeQueue.pop_front();
    }
}

 /* After both sending and receiving a Close message, an endpoint
   considers the WebSocket connection closed and MUST close the
   underlying TCP connection.  The server MUST close the underlying TCP
   connection immediately; the client SHOULD wait for the server to
   close the connection but MAY close the connection at any time after
   sending and receiving a Close message, e.g., if it has not received a
   TCP Close from the server in a reasonable time period. */
void MythWebSocket::CheckClose()
{
    auto closing = m_closeReceived || m_closeSent;
    if (closing && !m_closing)
    {
        // Repurpose our ping timer as a timeout for the completion of the closing handshake
        if (m_timer)
        {
            m_timer->stop();
            m_timer->disconnect();
        }
        else
        {
            m_timer = new QTimer();
        }
        connect(m_timer, &QTimer::timeout, m_socket, &QTcpSocket::close);
        m_timer->start(5s);
    }
    m_closing = closing;

    if (m_serverSide && m_closeReceived && m_closeSent)
        m_socket->close();
}

void MythWebSocket::CloseReceived(const DataPayload& Payload)
{
    m_closeReceived = true;

    WSErrorCode close = WSCloseNormal;

    // Incoming payload must be empty or be at least 2 bytes in size
    if ((*Payload).size() == 1)
    {
        close = WSCloseProtocolError;
    }
    else if ((*Payload).size() > 1)
    {
        // Extra data *may* be available but it *should* be UTF-8 encoded
        if ((*Payload).size() > 2)
        {
            auto utf8 = (*Payload).mid(2);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            QTextCodec::ConverterState state;
            (void)m_utf8Codec->toUnicode(utf8.constData(), utf8.size(), &state);
            if (state.invalidChars || state.remainingChars)
                close = WSCloseProtocolError;
#else
            (void)m_toUtf16->decode(utf8);
            if(m_toUtf16->hasError())
                close = WSCloseProtocolError;
#endif
        }

        if (WSCloseNormal == close)
        {
            auto code = qFromBigEndian<quint16>(reinterpret_cast<void*>(Payload->data()));
            if ((code < 1000) || ((code > 1003) && (code < 1007)) || ((code > 1011) && (code < 3000)) ||
                (code > 4999))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid close code");
                close = WSCloseProtocolError;
            }
            else
            {
                close = static_cast<WSErrorCode>(code);
            }
        }
    }

    SendClose(close);
    CheckClose();
}

void MythWebSocket::SendClose(WSErrorCode Code, const QString& Message)
{
    if (m_closeSent)
        return;

    auto header = MythSharedData::CreatePayload(2);
    (*header)[0] = static_cast<int8_t>((Code >> 8) & 0xff);
    (*header)[1] = static_cast<int8_t>(Code & 0xff);
    SendFrame(WSOpClose, { header, MythSharedData::CreatePayload(Message) });
    m_closeSent = true;
    CheckClose();
}

void MythWebSocket::SendFrame(WSOpCode Code, const DataPayloads& Payloads)
{
    if (m_closeSent || (m_closeReceived && (Code != WSOpClose)))
        return;

    // Payload size (if needed)
    auto addNext = [](int64_t acc, const auto & payload)
        { return payload.get() ? acc + payload->size() : acc; };
    int64_t length = std::accumulate(Payloads.cbegin(), Payloads.cend(), 0, addNext);

    // Payload for control frames must not exceed 125bytes
    if ((Code & 0x8) && (length > 125))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Control frame payload is too large");
        return;
    }

    // Assume no fragmentation for now
    auto header = MythSharedData::CreatePayload(2);
    header->data()[0] = static_cast<int8_t>(Code | 0x80);
    header->data()[1] = static_cast<int8_t>(0);

    // Add mask if necessary
    DataPayload mask = nullptr;
    if (!m_serverSide)
    {
        mask = MythSharedData::CreatePayload(4);
        for (int i = 0; i < 4; ++i)
        {
            mask->data()[i] = (int8_t)MythRandomInt(INT8_MIN, INT8_MAX);
        }
        header->data()[1] |= 0x80;
    }

    // generate correct size
    DataPayload size = nullptr;
    if (length < 126)
    {
        header->data()[1] |= static_cast<int8_t>(length);
    }
    else if (length <= 0xffff)
    {
        header->data()[1] |= 126;
        size = MythSharedData::CreatePayload(2);
        *reinterpret_cast<quint16*>(size->data()) = qToBigEndian(static_cast<quint16>(length));
    }
    else
    {
        header->data()[1] |= 127;
        size = MythSharedData::CreatePayload(8);
        *reinterpret_cast<quint64*>(size->data()) = qToBigEndian(static_cast<quint64>(length));
    }

    // Queue header
    m_writeQueue.emplace_back(header);
    // Queue size
    if (size.get())
        m_writeQueue.emplace_back(size);
    // Queue mask
    if (mask.get())
        m_writeQueue.emplace_back(mask);
    for (const auto & payload: Payloads)
        m_writeQueue.emplace_back(payload);

    LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Queued %1 frame: payload size %2 (queue length: %3)")
        .arg(MythWS::OpCodeToString(Code)).arg(length).arg(m_writeQueue.size()));

    // Kick off the write
    Write();
}

void MythWebSocket::PingReceived(DataPayload Payload)
{
    if (m_closeReceived || m_closeSent)
        return;
    SendFrame(WSOpPong, { std::move(Payload) });
}

void MythWebSocket::PongReceived(const DataPayload& Payload)
{
    // Validate against the last known ping payload and warn on error
    auto sizein = Payload.get() ? Payload.get()->size() : 0;
    auto sizeout = m_lastPingPayload.size();
    if ((sizein == sizeout) && sizeout > 0)
        if (*Payload.get() == m_lastPingPayload)
            return;
    LOG(VB_HTTP, LOG_DEBUG, LOC + "Unexpected pong payload (this may not be an error)");
    LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Last Payload is (%1), Pong Payload is (%2)")
        .arg(m_lastPingPayload.constData(), Payload.get()->constData()));
}

void MythWebSocket::SendPing()
{
    m_lastPingPayload = QString::number(MythRandom() & 0xFFFF, 16).toUtf8();
    if (auto payload = MythSharedData::CreatePayload(m_lastPingPayload); payload.get())
        SendFrame(WSOpPing, { payload });
}

