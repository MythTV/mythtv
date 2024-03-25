// Qt
#include <QCryptographicHash>
#include <QMimeDatabase>

// MythTV
#include "mythlogging.h"
#include "mythdate.h"
#include "http/mythhttpresponse.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttpranges.h"
#include "http/mythhttpencoding.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpcache.h"

#define LOC QString("HTTPResp: ")

/*! \class MythHTTPResponse
*/
MythHTTPResponse::MythHTTPResponse(const HTTPRequest2& Request)
  : m_serverName(Request->m_serverName),
    m_version(Request->m_version),
    m_connection(Request->m_connection),
    m_timeout(Request->m_timeout),
    m_status(Request->m_status),
    m_requestType(Request->m_type),
    m_allowed(Request->m_allowed),
    m_requestHeaders(Request->m_headers)
{
}

/*! \brief Complete all necessary headers, add final line break after headers, remove data etc
*/
void MythHTTPResponse::Finalise(const MythHTTPConfig& Config)
{
    // Remaining entity headers
    auto * data = std::get_if<HTTPData>(&m_response);
    auto * file = std::get_if<HTTPFile>(&m_response);
    if ((data || file) && m_requestHeaders)
    {
        // Language
        if (!Config.m_language.isEmpty())
            AddHeader("Content-Language", Config.m_language);

        // Content disposition
        QString filename = data ? (*data)->m_fileName : (*file)->m_fileName;
        QString download = MythHTTP::GetHeader(m_requestHeaders, "mythtv-download");
        if (!download.isEmpty())
        {
            int lastDot = filename.lastIndexOf('.');
            if (lastDot > 0)
            {
                QString extension = filename.right(filename.length() - lastDot);
                download = download + extension;
            }
            filename = download;
        }
        // Warn about programmer error
        if (filename.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Response has no name");

        // Default to 'inline' but we should support 'attachment' when it would
        // be appropriate i.e. not when streaming a file to a upnp player or browser
        // that can support it natively
        // TODO: Add support for utf8 encoding - RFC 5987
        AddHeader("Content-Disposition", QString("inline; filename=\"%2\"").arg(qPrintable(filename)));

        // TODO Should these be moved to UPnP handlers?
        // UPnP headers
        // DLNA 7.5.4.3.2.33 MT transfer mode indication
        QString mode = MythHTTP::GetHeader(m_requestHeaders, "transferMode.dlna.org");
        if (mode.isEmpty())
        {
            QString mime = data ? (*data)->m_mimeType.Name() : (*file)->m_mimeType.Name();
            if (mime.startsWith("video/") || mime.startsWith("audio/"))
                mode = "Streaming";
            else
                mode = "Interactive";
        }

        if (mode == "Streaming" || mode == "Background" || mode == "Interactive")
            AddHeader("transferMode.dlna.org", mode);

        // See DLNA  7.4.1.3.11.4.3 Tolerance to unavailable contentFeatures.dlna.org header
        //
        // It is better not to return this header, than to return it containing
        // invalid or incomplete information. We are unable to currently determine
        // this information at this stage, so do not return it. Only older devices
        // look for it. Newer devices use the information provided in the UPnP
        // response

        // HACK Temporary hack for Samsung TVs - Needs to be moved later as it's not entirely DLNA compliant
        if (!MythHTTP::GetHeader(m_requestHeaders, "getcontentFeatures.dlna.org", "").isEmpty())
        {
            AddHeader("contentFeatures.dlna.org",
                   "DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000");
        }

        // Security (mostly copied from previous HTTP server implementation)
        // TODO Are these all needed for all content?

        // Force IE into 'standards' mode
        AddHeader("X-UA-Compatible", "IE=Edge");
        // SECURITY: Set X-Content-Type-Options to 'nosniff'
        AddHeader("X-Content-Type-Options", "nosniff");
        // SECURITY: Set Content Security Policy
        //
        // *No external content allowed*
        //
        // This is an important safeguard. Third party content
        // should never be permitted. It compromises security,
        // privacy and violates the key principal that the
        // WebFrontend should work on an isolated network with no
        // internet access. Keep all content hosted locally!
        //
        // For now the following are disabled as we use xhr to
        // trigger playback on frontends if we switch to triggering
        // that through an internal request then these would be
        // better enabled
        //"default-src 'self'; "
        //"connect-src 'self' https://services.mythtv.org; "

        // FIXME unsafe-inline should be phased out, replaced by nonce-{csp_nonce} but it requires
        // all inline event handlers and style attributes to be removed ...
        QString policy = "script-src 'self' 'unsafe-inline' 'unsafe-eval' https://services.mythtv.org; "
                         "style-src 'self' 'unsafe-inline'; "
                         "frame-src 'self'; "
                         "object-src 'none'; "
                         "media-src 'self'; "
                         "font-src 'self'; "
                         // This img-src is needed for displaying icons in channel icon search
                         // These icons come from many different urls
                         "img-src http: https: data:; "
                         "form-action 'self'; "
                         "frame-ancestors 'self'; ";

        // For standards compliant browsers
        AddHeader("Content-Security-Policy", policy);
        // For Internet Explorer
        AddHeader("X-Content-Security-Policy", policy);
        AddHeader("X-XSS-Protection", "1; mode=block");
    }

    // Validate CORS requests
    if (m_requestHeaders)
    {
        QString origin = MythHTTP::GetHeader(m_requestHeaders, "origin").trimmed().toLower();
        if (!origin.isEmpty())
        {
            // Try allowed origins first
            bool allow = Config.m_allowedOrigins.contains(origin);
            if (!allow)
            {
                // Our list of hosts do not include the scheme (e.g. http) - so strip
                // this from the origin.
                if (auto index = origin.lastIndexOf("://"); index > -1)
                {
                    auto scheme = origin.mid(0, index);
                    if (scheme == "http" || scheme == "https")
                    {
                        auto host = origin.mid(index + 3);
                        allow = Config.m_hosts.contains(host);
                    }
                }
            }
            if (allow)
            {
                AddHeader("Access-Control-Allow-Origin" ,      origin);
                AddHeader("Access-Control-Allow-Credentials" , "true");
                AddHeader("Access-Control-Allow-Headers" ,     "Content-Type, Accept, Range");
                AddHeader("Access-Control-Request-Method",     MythHTTP::AllowedRequestsToString(m_allowed));
                LOG(VB_HTTP, LOG_INFO, LOC + QString("Allowing CORS for origin: '%1'").arg(origin));
            }
            else
            {
                LOG(VB_HTTP, LOG_INFO, LOC + QString("Disallowing CORS for origin: '%1'").arg(origin));
            }
        }
    }

    // Add line break after headers
    m_responseHeaders.emplace_back(MythHTTPData::Create("", "\r\n"));

    // remove actual content for HEAD requests, failed range requests and 304 Not Modified's
    if ((m_requestType == HTTPHead) || (m_status == HTTPNotModified) ||
        (m_status == HTTPRequestedRangeNotSatisfiable))
    {
        m_response = std::monostate();
    }
}

HTTPResponse MythHTTPResponse::HandleOptions(const HTTPRequest2& Request)
{
    if (!Request)
        return nullptr;

    // The default allowed methods in MythHTTPRequest are HEAD, GET, OPTIONS.
    // Override if necessary before calling this functions.
    // N.B. GET and HEAD must be supported for HTTP/1.1
    if ((Request->m_type & Request->m_allowed) != Request->m_type)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("'%1' is not allowed for '%2' (Allowed: %3)")
            .arg(MythHTTP::RequestToString(Request->m_type), Request->m_fileName,
                 MythHTTP::AllowedRequestsToString(Request->m_allowed)));
        Request->m_status = HTTPMethodNotAllowed;
        return MythHTTPResponse::ErrorResponse(Request);
    }

    // Options
    if (Request->m_type == HTTPOptions)
        return MythHTTPResponse::OptionsResponse(Request);

    return nullptr;
}

HTTPResponse MythHTTPResponse::ErrorResponse(MythHTTPStatus Status, const QString &ServerName)
{
    auto response = std::make_shared<MythHTTPResponse>();
    response->m_serverName   = ServerName;
    response->m_status       = Status;
    if (Status != HTTPServiceUnavailable)
    {
        response->m_response = MythHTTPData::Create("error.html",
            s_defaultHTTPPage.arg(MythHTTP::StatusToString(Status)).toUtf8().constData());
    }
    response->AddDefaultHeaders();
    if (Status == HTTPMethodNotAllowed)
        response->AddHeader("Allow", MythHTTP::AllowedRequestsToString(response->m_allowed));
    response->AddContentHeaders();
    return response;
}

HTTPResponse MythHTTPResponse::RedirectionResponse(const HTTPRequest2& Request, const QString &Redirect)
{
    Request->m_status = HTTPMovedPermanently;
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->AddDefaultHeaders();
    response->AddHeader("Location", Redirect);
    response->AddContentHeaders();
    return response;
}

HTTPResponse MythHTTPResponse::ErrorResponse(const HTTPRequest2& Request, const QString& Message)
{
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->m_response = MythHTTPData::Create("error.html", s_defaultHTTPPage
        .arg(Message.isEmpty() ? MythHTTP::StatusToString(Request->m_status) : Message).toUtf8().constData());
    response->AddDefaultHeaders();
    response->AddContentHeaders();
    return response;
}

HTTPResponse MythHTTPResponse::OptionsResponse(const HTTPRequest2& Request)
{
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->AddDefaultHeaders();
    response->AddHeader("Allow", MythHTTP::AllowedRequestsToString(response->m_allowed));
    response->AddContentHeaders();
    return response;
}

HTTPResponse MythHTTPResponse::DataResponse(const HTTPRequest2& Request, const HTTPData& Data)
{
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->m_response = Data;
    MythHTTPCache::PreConditionCheck(response);
    response->AddDefaultHeaders();
    response->AddContentHeaders();
    MythHTTPCache::PreConditionHeaders(response);
    return response;
}

HTTPResponse MythHTTPResponse::FileResponse(const HTTPRequest2& Request, const HTTPFile& File)
{
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->m_response = File;
    MythHTTPCache::PreConditionCheck(response);
    response->AddDefaultHeaders();
    response->AddContentHeaders();
    MythHTTPCache::PreConditionHeaders(response);
    return response;
}

HTTPResponse MythHTTPResponse::EmptyResponse(const HTTPRequest2& Request)
{
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->AddDefaultHeaders();
    response->AddContentHeaders();
    return response;
}

void MythHTTPResponse::AddDefaultHeaders()
{
    // Handle range requests early as they influence the status, compression etc
    QString range = MythHTTP::GetHeader(m_requestHeaders, "range", "");
    if (!range.isEmpty())
        MythHTTPRanges::HandleRangeRequest(this, range);

    QByteArray def = QString("%1 %2\r\n").arg(MythHTTP::VersionToString(m_version),
                                              MythHTTP::StatusToString(m_status)).toUtf8();
    m_responseHeaders.emplace_back(MythHTTPData::Create(def));
    AddHeader("Date", MythDate::toString(MythDate::current(), MythDate::kRFC822)); // RFC 822
    AddHeader("Server", m_serverName);
    // Range requests are supported
    AddHeader("Accept-Ranges", "bytes");
    AddHeader("Connection", m_connection == HTTPConnectionClose ? "Close" : "Keep-Alive");
    if (m_connection == HTTPConnectionKeepAlive)
        AddHeader("Keep-Alive", QString("timeout=%1").arg(m_timeout.count() / 1000));

    // Required error specific headers
    if (m_status == HTTPServiceUnavailable)
        AddHeader("Retry-After", HTTP_SOCKET_TIMEOUT_MS / 1000);
}

void MythHTTPResponse::AddContentHeaders()
{
    // Check content type and size first
    auto * data = std::get_if<HTTPData>(&m_response);
    auto * file = std::get_if<HTTPFile>(&m_response);
    int64_t size {0};

    if (data)
        size = (*data)->size();
    else if (file)
        size = (*file)->size();

    // Always add a zero length content header to keep some clients happy
    if (size < 1)
    {
        AddHeader("Content-Length", 0);
        return;
    }

    // Check mime type if not already set
    auto & mime = data ? (*data)->m_mimeType : (*file)->m_mimeType;
    if (!mime.IsValid())
        mime = MythHTTPEncoding::GetMimeType(m_response);

    // Range request?
    HTTPRanges& ranges = data ? (*data)->m_ranges : (*file)->m_ranges;
    bool rangerequest  = !ranges.empty();
    bool multipart     = ranges.size() > 1;

    // We now have the mime type and we can generate the multipart headers for a
    // multipart request
    if (multipart)
        MythHTTPRanges::BuildMultipartHeaders(this);

    // Set the content type - with special handling for multipart ranges
    AddHeader("Content-Type", multipart ? MythHTTPRanges::GetRangeHeader(ranges, size) :
                                       MythHTTP::GetContentType(mime));

    if (m_status == HTTPRequestedRangeNotSatisfiable)
    {
        // Mandatory 416 (Range Not Satisfiable) response
        // Note - we will remove content before sending
        AddHeader("Content-Range", QString("bytes */%1").arg(size));
        AddHeader("Content-Length", 0);
        return;
    }

    // Compress/chunk the result depending on client preferences, content and transfer type
    auto encode = MythHTTPEncoding::Compress(this, size);

    // and finally set the length if we aren't chunking or the transfer-encoding
    // header if we are
    if (encode == HTTPChunked)
    {
        AddHeader("Transfer-Encoding", "chunked");
    }
    else
    {
        if (rangerequest)
        {
            // Inform the client of the (single) range being served
            if (!multipart)
                AddHeader("Content-Range", MythHTTPRanges::GetRangeHeader(ranges, size));
            // Content-Length is now the number of bytes served, not the total
            size = data ? (*data)->m_partialSize : (*file)->m_partialSize;
        }

        // Add the size of the multipart headers to the content length
        if (multipart)
            size += data ? (*data)->m_multipartHeaderSize : (*file)->m_multipartHeaderSize;
        AddHeader("Content-Length", size);
    }
}

HTTPResponse MythHTTPResponse::UpgradeResponse(const HTTPRequest2& Request, MythSocketProtocol &Protocol, bool &Testing)
{
    // Assume the worst:) and create a default error response
    Request->m_status = HTTPBadRequest;
    auto response = std::make_shared<MythHTTPResponse>(Request);
    response->AddDefaultHeaders();
    response->AddContentHeaders();

    // This shouldn't happen
    if (!Request)
        return response;

    /* Excerpt from RFC 6455
    The requirements for this handshake are as follows.
       1.   The handshake MUST be a valid HTTP request as specified by
            [RFC2616].
       2.   The method of the request MUST be GET, and the HTTP version MUST
            be at least 1.1.
            For example, if the WebSocket URI is "ws://example.com/chat",
            the first line sent should be "GET /chat HTTP/1.1".
    */

    if ((Request->m_type != HTTPGet || Request->m_version != HTTPOneDotOne))
    {
        LOG(VB_HTTP, LOG_ERR, LOC + "Must be GET and HTTP/1.1");
        return response;
    }

    /*
       3.   The "Request-URI" part of the request MUST match the /resource
            name/ defined in Section 3 (a relative URI) or be an absolute
            http/https URI that, when parsed, has a /resource name/, /host/,
            and /port/ that match the corresponding ws/wss URI.
    */

    if (Request->m_path.isEmpty())
    {
        LOG(VB_HTTP, LOG_ERR, LOC + "Invalid Request-URI");
        return response;
    }

    /*
       4.   The request MUST contain a |Host| header field whose value
            contains /host/ plus optionally ":" followed by /port/ (when not
            using the default port).
    */

    // Already checked in MythHTTPRequest

    /*
       5.   The request MUST contain an |Upgrade| header field whose value
            MUST include the "websocket" keyword.
    */

    auto header = MythHTTP::GetHeader(Request->m_headers, "upgrade");
    if (header.isEmpty() || !header.contains("websocket", Qt::CaseInsensitive))
    {
        LOG(VB_HTTP, LOG_ERR, LOC + "Invalid/missing 'Upgrade' header");
        return response;
    }

    /*
       6.   The request MUST contain a |Connection| header field whose value
            MUST include the "Upgrade" token.
    */

    header = MythHTTP::GetHeader(Request->m_headers, "connection");
    if (header.isEmpty() || !header.contains("upgrade", Qt::CaseInsensitive))
    {
        LOG(VB_HTTP, LOG_ERR, LOC + "Invalid/missing 'Connection' header");
        return response;
    }

    /*
       7.   The request MUST include a header field with the name
            |Sec-WebSocket-Key|.  The value of this header field MUST be a
            nonce consisting of a randomly selected 16-byte value that has
            been base64-encoded (see Section 4 of [RFC4648]).  The nonce
            MUST be selected randomly for each connection.
            NOTE: As an example, if the randomly selected value was the
            sequence of bytes 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09
            0x0a 0x0b 0x0c 0x0d 0x0e 0x0f 0x10, the value of the header
            field would be "AQIDBAUGBwgJCgsMDQ4PEC=="
    */

    auto key = MythHTTP::GetHeader(Request->m_headers, "sec-websocket-key").trimmed();
    if (key.isEmpty())
    {
        LOG(VB_HTTP, LOG_ERR, LOC + "No Sec-WebSocket-Key header");
        return response;
    }
    auto nonce = QByteArray::fromBase64(key.toLatin1());
    if (nonce.length() != 16)
    {
        LOG(VB_HTTP, LOG_ERR, LOC + QString("Invalid Sec-WebSocket-Key header (length: %1)").arg(nonce.length()));
        return response;
    }

    /*
       8.   The request MUST include a header field with the name |Origin|
            [RFC6454] if the request is coming from a browser client.  If
            the connection is from a non-browser client, the request MAY
            include this header field if the semantics of that client match
            the use-case described here for browser clients.  The value of
            this header field is the ASCII serialization of origin of the
            context in which the code establishing the connection is
            running.  See [RFC6454] for the details of how this header field
            value is constructed.

            As an example, if code downloaded from www.example.com attempts
            to establish a connection to ww2.example.com, the value of the
            header field would be "http://www.example.com".
    */

    // No reasonable way of knowing if the client is a browser. May need more work.

    /*
       9.   The request MUST include a header field with the name
            |Sec-WebSocket-Version|.  The value of this header field MUST be
            13.
    */

    if (header = MythHTTP::GetHeader(Request->m_headers, "sec-websocket-version"); header.trimmed().toInt() != 13)
    {
        LOG(VB_HTTP, LOG_ERR, LOC + QString("Unsupported websocket version %1").arg(header));
        response->AddHeader(QStringLiteral("Sec-WebSocket-Version"), QStringLiteral("13"));
        return response;
    }

    /*
        10.  The request MAY include a header field with the name
        |Sec-WebSocket-Protocol|.  If present, this value indicates one
        or more comma-separated subprotocol the client wishes to speak,
        ordered by preference.  The elements that comprise this value
        MUST be non-empty strings with characters in the range U+0021 to
        U+007E not including separator characters as defined in
        [RFC2616] and MUST all be unique strings.  The ABNF for the
        value of this header field is 1#token, where the definitions of
        constructs and rules are as given in [RFC2616].
    */

    Protocol = MythHTTPWS::ProtocolFromString(MythHTTP::GetHeader(Request->m_headers, "sec-websocket-protocol"));

    // If we've got this far, everything is OK, we have set the protocol that will
    // be used and we need to respond positively.

    static const auto magic = QStringLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    QString hash = QCryptographicHash::hash((key + magic).toUtf8(), QCryptographicHash::Sha1).toBase64();

    // Replace response
    Request->m_status = HTTPSwitchingProtocols;
    response = std::make_shared<MythHTTPResponse>(Request);
    response->AddDefaultHeaders();
    response->AddContentHeaders();
    response->AddHeader(QStringLiteral("Connection"), QStringLiteral("Upgrade"));
    response->AddHeader(QStringLiteral("Upgrade"), QStringLiteral("websocket"));
    response->AddHeader(QStringLiteral("Sec-WebSocket-Accept"), hash);
    if (Protocol != ProtFrame)
        response->AddHeader(QStringLiteral("Sec-WebSocket-Protocol"), MythHTTPWS::ProtocolToString(Protocol));

    LOG(VB_HTTP, LOG_INFO, LOC + QString("Successful WebSocket upgrade (protocol: %1)")
        .arg(MythHTTPWS::ProtocolToString(Protocol)));

    // Check for Autobahn test suite
    if (header = MythHTTP::GetHeader(Request->m_headers, "user-agent"); header.contains("AutobahnTestSuite"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Autobahn test suite detected. Will echooooo...");
        Testing = true;
    }

    // Ensure we pass handling to the websocket code once the response is sent
    response->m_connection = HTTPConnectionUpgrade;
    return response;
}
