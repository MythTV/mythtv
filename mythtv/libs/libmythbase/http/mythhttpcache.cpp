// Qt
#include <QCryptographicHash>

// MythTV
#include "mythlogging.h"
#include "mythdate.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttpcache.h"

/*! \brief Process precondition checks
 *
 * This should be the first step in processing an HTTP request, typically only
 * when there is meaningful content to be returned (i.e. it is not used for error
 * responses, even though they may contain content).
 *
 * The status of the response may be updated and the ETag for the content may
 * be set.
 *
 * 'Last-Modified' and 'ETag' outgoing headers are supported, dependant on the cache
 * setting for the content. This defaults to HTTPIgnoreCache for errors etc,
 * HTTPLastModified for files and HTTPNoCache for in-memory data responses. It
 * is expected that the latter will be configured as required by the handler's
 * implementation.
 *
 * 'If-Modified-Since', 'If-None-Match' and 'If-Range' headers are supported in
 * incoming headers (i.e. last modified and ETag validators).
 *
 * There is currently no suport for 'If-Match' and 'If-Unmodified-Since' headers.
 *
 * We only ever use one validator and it is assumed the validator is used consistently.
 * i.e. we use ETag or Last-Modified across invocations and not both. This ensures
 * the client sends the correct validator in response.
 *
 * \note Last-Modified is used for files as they have valid last modified information, last
 * modified data gives reasonable validation (see below) and hashing the file contents
 * (or in memory data for compressed content) for the ETag is potentially expensive.
 *
 * \note Last-Modified is considered a weak validator as it only has second accuracy.
 * This should however be sufficient for our purposes when sending files. Likewise
 * our hashing for file responses when using ETag should also be considered weak
 * (as it does not guarantee byte-for-byte accuracy). Again however it should
 * be considered accurate enough for our needs - and more accurate than Last-Modified
 * as we use millisecond accuracy for our hash generation.
 *
 * \note It is recommended that Etags vary depending on the content encoding (e.g. gzipped).
 * We do not currently support this as our decision to compress depends on content
 * size, type, and the client range and encoding requests. It should not however
 * be an issue (!)
 */
void MythHTTPCache::PreConditionCheck(const HTTPResponse& Response)
{
    // Retrieve content
    auto * data = std::get_if<HTTPData>(&Response->m_response);
    auto * file = std::get_if<HTTPFile>(&Response->m_response);
    if (!(file || data))
        return;

    int cache = data ? (*data)->m_cacheType : (*file)->m_cacheType;

    // Explicitly ignore caching (error responses etc)
    if (cache == HTTPIgnoreCache)
        return;

    // Explicitly request no caching
    if ((cache & HTTPNoCache) == HTTPNoCache)
        return;

    // We use the last modified data for both the last modified header and potentially
    // hashing for the etag
    auto & lastmodified = data ? (*data)->m_lastModified : (*file)->m_lastModified;
    if (!lastmodified.isValid())
        lastmodified = QDateTime::currentDateTime();

    // If-Range precondition can contain either a last-modified or ETag validator
    QString ifrange = MythHTTP::GetHeader(Response->m_requestHeaders, "if-range");
    bool checkifrange = !ifrange.isEmpty();
    bool removeranges = false;

    if ((cache & HTTPETag) == HTTPETag)
    {
        QByteArray& etag = data ? (*data)->m_etag : (*file)->m_etag;
        if (file)
        {
            QByteArray hashdata = ((*file)->fileName() + lastmodified.toString("ddMMyyyyhhmmsszzz")).toLocal8Bit().constData();
            etag = QCryptographicHash::hash(hashdata, QCryptographicHash::Sha224).toHex();
        }
        else
        {
            etag = QCryptographicHash::hash((*data)->constData(), QCryptographicHash::Sha224).toHex();
        }

        // This assumes only one or other is present...
        if (checkifrange)
        {
            removeranges = !ifrange.contains(etag);
        }
        else
        {
            QString nonematch = MythHTTP::GetHeader(Response->m_requestHeaders, "if-none-match");
            if (!nonematch.isEmpty() && (nonematch.contains(etag)))
                Response->m_status = HTTPNotModified;
        }
    }
    else if (((cache & HTTPLastModified) == HTTPLastModified))
    {
        auto RemoveMilliseconds = [](QDateTime& DateTime)
        {
            auto residual = DateTime.time().msec();
            DateTime = DateTime.addMSecs(-residual);
        };

        auto ParseModified = [](const QString& Modified)
        {
            QDateTime time = QDateTime::fromString(Modified, Qt::RFC2822Date);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
            time.setTimeSpec(Qt::UTC);
#else
            time.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
            return time;
        };

        if (checkifrange)
        {
            auto time = ParseModified(ifrange);
            if (time.isValid())
            {
                RemoveMilliseconds(lastmodified);
                removeranges = lastmodified > time;
            }
        }
        else if (Response->m_requestType == HTTPGet || Response->m_requestType == HTTPHead)
        {
            QString modified = MythHTTP::GetHeader(Response->m_requestHeaders, "if-modified-since");
            if (!modified.isEmpty())
            {
                auto time = ParseModified(modified);
                if (time.isValid())
                {
                    RemoveMilliseconds(lastmodified);
                    if (lastmodified <= time)
                        Response->m_status = HTTPNotModified;
                }
            }
        }
    }

    // If the If-Range precondition fails, then we need to send back the complete
    // contents i.e. ignore any ranges. So search for the range headers and remove them.
    if (removeranges && Response->m_requestHeaders && Response->m_requestHeaders->contains("range"))
        Response->m_requestHeaders->replace("range", "");
}

/*! \brief Add precondition (cache) headers to the response.
 *
 * Must be after a call to PreConditionCheck.
*/
void MythHTTPCache::PreConditionHeaders(const HTTPResponse& Response)
{
    // Retrieve content
    auto * data = std::get_if<HTTPData>(&Response->m_response);
    auto * file = std::get_if<HTTPFile>(&Response->m_response);
    if (!(file || data))
        return;

    int cache = data ? (*data)->m_cacheType : (*file)->m_cacheType;

    // Explicitly ignore caching (error responses etc)
    if (cache == HTTPIgnoreCache)
        return;

    // Explicitly request no caching
    if ((cache & HTTPNoCache) == HTTPNoCache)
    {
        Response->AddHeader("Cache-Control", "no-store, max-age=0");
        return;
    }

    // Add the default cache control header
    QString duration {"0"};    // ??
    if ((cache & HTTPLongLife) == HTTPLongLife)
        duration = "604800";   // 7 days
    else if ((cache & HTTPMediumLife) == HTTPMediumLife)
        duration = "7200";     // 2 Hours
    Response->AddHeader("Cache-Control", "no-cache=\"Ext\",max-age=" + duration);

    if ((cache & HTTPETag) == HTTPETag)
    {
        Response->AddHeader("ETag", QString("\"") + (data ? (*data)->m_etag : (*file)->m_etag) + "\"");
    }
    else if ((cache & HTTPLastModified) == HTTPLastModified)
    {
        auto & lastmodified = data ? (*data)->m_lastModified : (*file)->m_lastModified;
        auto last = MythDate::toString(lastmodified, MythDate::kOverrideUTC | MythDate::kRFC822);
        Response->AddHeader("Last-Modified", last);
    }
}
