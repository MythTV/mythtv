// MythTV
#include "mythlogging.h"
#include "unziputil.h"
#include "http/mythmimedatabase.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttpresponse.h"
#include "http/mythhttpencoding.h"

// Qt
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#define LOC QString("HTTPEnc: ")

/*! \brief Parse the incoming HTTP 'Accept' header and return an ordered list of preferences.
 *
 * We retain the original MIME types rather than converting to a MythMimeType
 * as this potentially converts an alias type to its parent (i.e. QMimeDatabase
 * will always return application/xml for xml types).
 *
 * \note This will not remove wildcard mime specifiers but they will likely be
 * ignored elsewhere.
 *
 *
*/
using MimePair = std::pair<float,QString>;
QStringList MythHTTPEncoding::GetMimeTypes(const QString &Accept)
{
    // Split out mime types
    auto types = Accept.split(",", Qt::SkipEmptyParts);

    std::vector<MimePair> weightings;
    for (const auto & type : std::as_const(types))
    {
        QString mime = type.trimmed();
        auto quality = 1.0F;
        // Find any quality value (defaults to 1)
        if (auto index = type.lastIndexOf(";"); index > -1)
        {
            mime = type.mid(0, index).trimmed().toLower();
            auto qual = type.mid(index + 1).trimmed();
            if (auto index2 = qual.lastIndexOf("="); index2 > -1)
            {
                bool ok = false;
                auto newquality = qual.mid(index2 + 1).toFloat(&ok);
                if (ok)
                    quality = newquality;
            }
        }
        weightings.emplace_back(quality, mime);
    }

    // Sort the list
    auto comp = [](const MimePair& First, const MimePair& Second) { return First.first > Second.first; };
    std::sort(weightings.begin(), weightings.end(), comp);

    // Build the final result. This will pass through invalid types - which should
    // be handled by the consumer (e.g. wildcard specifiers are not handled).
    QStringList result;
    for (const auto & weight : weightings)
        result.append(weight.second);

    // Default to xml
    if (result.empty())
        result.append("application/xml");
    return result;
}

/*! \brief Parse the incoming Content-Type header for POST/PUT content
*/
void MythHTTPEncoding::GetContentType(MythHTTPRequest* Request)
{
    if (!Request || !Request->m_content.get())
        return;

    auto contenttype = MythHTTP::GetHeader(Request->m_headers, "content-type");

    // type is e.g. text/html; charset=UTF-8 or multipart/form-data; boundary=something
    auto types = contenttype.split(";", Qt::SkipEmptyParts);
    if (types.isEmpty())
        return;

    // Note: This can produce an invalid mime type but there is no sensible fallback
    if (auto mime = MythMimeDatabase::MimeTypeForName(types[0].trimmed().toLower()); mime.IsValid())
    {
        Request->m_content->m_mimeType = mime;
        if (mime.Name() == "application/x-www-form-urlencoded")
            GetURLEncodedParameters(Request);
        else if (mime.Name() == "text/xml" || mime.Name() == "application/xml" ||
                 mime.Name() == "application/soap+xml")
            GetXMLEncodedParameters(Request);
        else if (mime.Name() == "application/json")
            GetJSONEncodedParameters(Request);
        else
            LOG(VB_HTTP, LOG_ERR, QString("Don't know how to get the parameters for MIME type: '%1'").arg(mime.Name()));
    }
    else
    {
        LOG(VB_HTTP, LOG_ERR, QString("Unknown MIME type: '%1'").arg(types[0]));
    }

}

void MythHTTPEncoding::GetURLEncodedParameters(MythHTTPRequest* Request)
{
    if (!Request || !Request->m_content.get())
        return;

    auto payload = QString::fromUtf8(Request->m_content->constData(), Request->m_content->size());

    // This looks odd, but it is here to cope with stupid UPnP clients that
    // forget to de-escape the URLs.  We can't map %26 here as well, as that
    // breaks anything that is trying to pass & as part of a name or value.
    payload.replace("&amp;", "&");
    if (!payload.isEmpty())
    {
        QStringList params = payload.split('&', Qt::SkipEmptyParts);
        for (const auto & param : std::as_const(params))
        {
            QString name  = param.section('=', 0, 0);
            QString value = param.section('=', 1);
            value.replace("+", " ");
            if (!name.isEmpty())
            {
                name  = QUrl::fromPercentEncoding(name.toUtf8());
                value = QUrl::fromPercentEncoding(value.toUtf8());
                Request->m_queries.insert(name.trimmed().toLower(), value);
            }
        }
    }
}

void MythHTTPEncoding::GetXMLEncodedParameters(MythHTTPRequest* Request)
{
    LOG(VB_HTTP, LOG_DEBUG, "Inspecting XML payload");

    if (!Request || !Request->m_content.get())
        return;

    // soapaction is formatted like "\"http://mythtv.org/Dvr/GetRecordedList\""
    QString soapaction = Request->m_headers->value("soapaction");
    soapaction.remove('"');
    int lastSlashPos= soapaction.lastIndexOf('/');
    if (lastSlashPos < 0)
        return;
    if (Request->m_path == "/")
        Request->m_path.append(Request->m_fileName).append("/");
    Request->m_fileName = soapaction.right(soapaction.size()-lastSlashPos-1);
    LOG(VB_HTTP, LOG_DEBUG, QString("Found method call (%1)").arg(Request->m_fileName));

    auto payload = QDomDocument();
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString err_msg;
    int err_line {-1};
    int err_col {-1};
    if (!payload.setContent(static_cast<QByteArray>(Request->m_content->constData()),
                            true, &err_msg, &err_line, &err_col))
    {
        LOG(VB_HTTP, LOG_WARNING, "Unable to parse XML request body");
        LOG(VB_HTTP, LOG_WARNING, QString("- Error at line %1, column %2, msg: %3")
                                          .arg(err_line).arg(err_col).arg(err_msg));
        return;
    }
#else
    auto parseresult = payload.setContent(Request->m_content->constData(),
                                          QDomDocument::ParseOption::UseNamespaceProcessing);
    if (!parseresult)
    {
        LOG(VB_HTTP, LOG_WARNING, "Unable to parse XML request body");
        LOG(VB_HTTP, LOG_WARNING, QString("- Error at line %1, column %2, msg: %3")
            .arg(parseresult.errorLine).arg(parseresult.errorColumn)
            .arg(parseresult.errorMessage));
        return;
    }
#endif
    QString doc_name = payload.documentElement().localName();
    if (doc_name.compare("envelope", Qt::CaseInsensitive) == 0)
    {
        LOG(VB_HTTP, LOG_DEBUG, "Found SOAP XML message envelope");
        auto doc_body = payload.documentElement().namedItem("Body");
        if (doc_body.isNull() || !doc_body.hasChildNodes()) // None or empty body
        {
            LOG(VB_HTTP, LOG_DEBUG, "Missing or empty SOAP body");
            return;
        }
        auto body_contents = doc_body.firstChild();
        if (body_contents.hasChildNodes()) // params for the method
        {
            for (QDomNode node = body_contents.firstChild(); !node.isNull(); node = node.nextSibling())
            {
                QString name = node.localName();
                QString value = node.toElement().text();
                if (!name.isEmpty())
                {
                    // TODO: html decode entities if required
                    Request->m_queries.insert(name.trimmed().toLower(), value);
                    LOG(VB_HTTP, LOG_DEBUG, QString("Found URL param (%1=%2)").arg(name, value));
                }
            }
        }
    }
}

void MythHTTPEncoding::GetJSONEncodedParameters(MythHTTPRequest* Request)
{
    LOG(VB_HTTP, LOG_DEBUG, "Inspecting JSON payload");

    if (!Request || !Request->m_content.get())
        return;

    QByteArray jstr = static_cast<QByteArray>(Request->m_content->constData());
    QJsonParseError parseError {};
    QJsonDocument doc = QJsonDocument::fromJson(jstr, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        LOG(VB_HTTP, LOG_WARNING,
            QString("Unable to parse JSON request body - Error at position %1, msg: %2")
            .arg(parseError.offset).arg(parseError.errorString()));
        return;
    }

    QJsonObject json = doc.object();
    foreach(const QString& key, json.keys())
    {
        if (!key.isEmpty())
        {
            QString value;
            if (json.value(key).isObject())
            {
                QJsonDocument vd(json.value(key).toObject());
                value = vd.toJson(QJsonDocument::Compact);
            }
            else
            {
                value = json.value(key).toVariant().toString();

                if (value.isEmpty())
                {
                    LOG(VB_HTTP, LOG_WARNING,
                        QString("Failed to parse value for key '%1' from %2")
                        .arg(key, QString(jstr)));
                }
            }

            Request->m_queries.insert(key.trimmed().toLower(), value);
            LOG(VB_HTTP, LOG_DEBUG,
                QString("Found URL param (%1=%2)").arg(key, value));
        }
    }
}

/*! \brief Return a QMimeType that represents Content.
*/
MythMimeType MythHTTPEncoding::GetMimeType(HTTPVariant Content)
{
    auto * data = std::get_if<HTTPData>(&Content);
    auto * file = std::get_if<HTTPFile>(&Content);
    if (!(data || file))
        return {};

    QString filename;
    if (data)
        filename = (*data)->m_fileName;
    else if (file)
        filename = (*file)->m_fileName;

    // Look for unambiguous mime type
    auto types = MythMimeDatabase::MimeTypesForFileName(filename);
    if (types.size() == 1)
        return types.front();

    // Look for an override. QMimeDatabase gets it wrong sometimes when the result
    // is ambiguous and it resorts to probing. Add to this list as necessary
    static const std::map<QString,QString> s_mimeOverrides =
    {
        { "ts", "video/mp2t"}
    };

    auto suffix = MythMimeDatabase::SuffixForFileName(filename);
    for (const auto & type : s_mimeOverrides)
        if (suffix.compare(type.first, Qt::CaseInsensitive) == 0)
            return MythMimeDatabase::MimeTypeForName(type.second);

    // Try interrogating content as well
    if (data)
        if (auto mime = MythMimeDatabase::MimeTypeForFileNameAndData(filename, **data); mime.IsValid())
            return mime;
    if (file)
        if (auto mime = MythMimeDatabase::MimeTypeForFileNameAndData(filename, (*file).get()); mime.IsValid())
            return mime;

    // Default to text/plain (possibly use application/octet-stream as well?)
    return MythMimeDatabase::MimeTypeForName("text/plain");
}

/*! \brief Compress the response content under certain circumstances or mark
 * the content as 'chunkable'.
 *
 * This only supports gzip compression. deflate is simple enough to add using
 * qCompress but Qt adds a header and a footer to the result, which must be
 * removed. deflate does save a handful of bytes but we don't really need to support both.
*/
MythHTTPEncode MythHTTPEncoding::Compress(MythHTTPResponse* Response, int64_t& Size)
{
    auto result = HTTPNoEncode;
    if (!Response || !Response->m_requestHeaders)
        return result;

    // We need something to compress/chunk
    auto * data = std::get_if<HTTPData>(&Response->m_response);
    auto * file = std::get_if<HTTPFile>(&Response->m_response);
    if (!(data || file))
        return result;

    // Don't touch range requests. They do not work with compression and there
    // is no point in chunking gzipped content as the client still has to wait
    // for the entire payload before unzipping
    // Note: It is permissible to chunk a range request - but ignore for the
    // timebeing to keep the code simple.
    if ((data && !(*data)->m_ranges.empty()) || (file && !(*file)->m_ranges.empty()))
        return result;

    // Has the client actually requested compression
    bool wantgzip = MythHTTP::GetHeader(Response->m_requestHeaders, "accept-encoding").toLower().contains("gzip");

    // Chunking is HTTP/1.1 only - and must be supported
    bool chunkable = Response->m_version == HTTPOneDotOne;

    // Has the client requested no chunking by specifying identity?
    bool allowchunk = ! MythHTTP::GetHeader(Response->m_requestHeaders, "accept-encoding").toLower().contains("identity");

    // and restrict to 'chunky' files
    bool chunky = Size > 102400; // 100KB

    // Don't compress anything that is too large. Under normal circumstances this
    // should not be a problem as we only compress text based data - but avoid
    // potentially memory hungry compression operations.
    // On the flip side, don't compress trivial amounts of data
    bool gzipsize = Size > 512 && !chunky; //  0.5KB <-> 100KB

    // Only consider compressing text based content. No point in compressing audio,
    // video and images.
    bool compressable = (data ? (*data)->m_mimeType : (*file)->m_mimeType).Inherits("text/plain");

    // Decision time
    bool gzip  = wantgzip && gzipsize && compressable;
    bool chunk = chunkable && chunky && allowchunk;

    if (!gzip)
    {
        // Chunking happens as we write to the socket, so flag it as required
        if (chunk)
        {
            result = HTTPChunked;
            if (data) (*data)->m_encoding = result;
            if (file) (*file)->m_encoding = result;
        }
        return result;
    }

    // As far as I can tell, Qt's implicit sharing of data should ensure we aren't
    // copying data unnecessarily here - but I can't be sure. We could definitely
    // improve compressing files by avoiding the copy into a temporary buffer.
    HTTPData buffer = MythHTTPData::Create(data ? gzipCompress(**data) : gzipCompress((*file)->readAll()));

    // Add the required header
    Response->AddHeader("Content-Encoding", "gzip");

    LOG(VB_HTTP, LOG_INFO, LOC + QString("'%1' compressed from %2 to %3 bytes")
        .arg(data ? (*data)->m_fileName : (*file)->fileName())
        .arg(Size).arg(buffer->size()));

    // Copy the filename and last modified, set the new buffer and set the content size
    buffer->m_lastModified = data ? (*data)->m_lastModified : (*file)->m_lastModified;
    buffer->m_etag         = data ? (*data)->m_etag         : (*file)->m_etag;
    buffer->m_fileName     = data ? (*data)->m_fileName     : (*file)->m_fileName;
    buffer->m_cacheType    = data ? (*data)->m_cacheType    : (*file)->m_cacheType;
    buffer->m_encoding = HTTPGzip;
    Response->m_response = buffer;
    Size = buffer->size();
    return HTTPGzip;
}
