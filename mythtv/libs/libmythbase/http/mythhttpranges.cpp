// MythTV
#include "mythlogging.h"
#include "http/mythhttpranges.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttpresponse.h"

#define LOC QString("HTTPRange: ")

auto sumrange = [](uint64_t Cum, HTTPRange Range) { return ((Range.second + 1) - Range.first) + Cum; };

void MythHTTPRanges::HandleRangeRequest(MythHTTPResponse* Response, const QString& Request)
{
    if (!Response || Request.isEmpty())
        return;

    // Check content type and size first
    auto * data = std::get_if<HTTPData>(&(Response->m_response));
    auto * file = std::get_if<HTTPFile>(&(Response->m_response));
    int64_t size {0};
    if (data)
        size = (*data)->size();
    else if (file)
        size = (*file)->size();
    if (size < 1)
        return;

    // Parse
    HTTPRanges&    ranges = data ? (*data)->m_ranges      : (*file)->m_ranges;
    int64_t&  partialsize = data ? (*data)->m_partialSize : (*file)->m_partialSize;
    MythHTTPStatus status = MythHTTPRanges::ParseRanges(Request, size, ranges, partialsize);
    if ((status == HTTPRequestedRangeNotSatisfiable) || (status == HTTPPartialContent))
        Response->m_status = status;
}

void MythHTTPRanges::BuildMultipartHeaders(MythHTTPResponse* Response)
{
    if (!Response || (Response->m_status != HTTPPartialContent))
        return;

    auto * data = std::get_if<HTTPData>(&(Response->m_response));
    auto * file = std::get_if<HTTPFile>(&(Response->m_response));
    int64_t size {0};
    if (data)
        size = (*data)->size();
    else if (file)
        size = (*file)->size();
    if (size < 1)
        return;

    HTTPRanges& ranges = data ? (*data)->m_ranges : (*file)->m_ranges;
    if (ranges.size() < 2)
        return;

    auto & mime = data ? (*data)->m_mimeType : (*file)->m_mimeType;
    HTTPContents headers;
    for (auto & range : ranges)
    {
        auto header = QString("\r\n--%1\r\nContent-Type: %2\r\nContent-Range: %3\r\n\r\n")
            .arg(s_multipartBoundary, MythHTTP::GetContentType(mime),
                 MythHTTPRanges::GetRangeHeader(range, size));
        headers.emplace_back(MythHTTPData::Create(qPrintable(header)));

    }
    headers.emplace_back(MythHTTPData::Create(qPrintable(QString("\r\n--%1--")
                                                           .arg(s_multipartBoundary))));
    std::reverse(headers.begin(), headers.end());
    int64_t headersize = 0;
    for (auto & header : headers)
        headersize += header->size();
    if (data)
    {
        (*data)->m_multipartHeaders = headers;
        (*data)->m_multipartHeaderSize = headersize;
    }
    if (file)
    {
        (*file)->m_multipartHeaders = headers;
        (*file)->m_multipartHeaderSize = headersize;
    }
}

QString MythHTTPRanges::GetRangeHeader(HTTPRange& Range, int64_t Size)
{
    return QString("bytes %1-%2/%3").arg(Range.first).arg(Range.second).arg(Size);
}

QString MythHTTPRanges::GetRangeHeader(HTTPRanges& Ranges, int64_t Size)
{
    if (Ranges.empty())
        return "ErRoR";
    if (Ranges.size() == 1)
        return MythHTTPRanges::GetRangeHeader(Ranges[0], Size);
    return "multipart/byteranges; boundary=" + s_multipartBoundary;
}

HTTPMulti MythHTTPRanges::HandleRangeWrite(HTTPVariant Data, int64_t Available, int64_t &ToWrite, int64_t &Offset)
{
    HTTPMulti result { nullptr, nullptr };
    auto * data = std::get_if<HTTPData>(&Data);
    auto * file = std::get_if<HTTPFile>(&Data);
    if (!(data || file))
        return result;

    int64_t partialsize   = data ? (*data)->m_partialSize : (*file)->m_partialSize;
    auto written          = static_cast<uint64_t>(data ? (*data)->m_written : (*file)->m_written);
    HTTPRanges& ranges    = data ? (*data)->m_ranges : (*file)->m_ranges;
    HTTPContents& headers = data ? (*data)->m_multipartHeaders : (*file)->m_multipartHeaders;

    uint64_t    oldsum = 0;
    for (const auto& range : ranges)
    {
        uint64_t newsum = sumrange(oldsum, range);
        if (oldsum <= written && written < newsum)
        {
            // This is the start of a multipart range. Add the start headers.
            if ((oldsum == written) && !headers.empty())
            {
                result.first = headers.back();
                headers.pop_back();
            }

            // Restrict the write to the remainder of this range if necessary
            ToWrite = std::min(Available, static_cast<int64_t>(newsum - written));

            // We need to ensure we send from the correct offset in the data
            Offset = static_cast<int64_t>(range.first - oldsum);
            if (file)
                Offset += written;

            // This is the end of the multipart ranges. Add the closing header
            // (We add this first so we can pop the contents when sending the headers)
            if (((static_cast<int64_t>(written) + ToWrite) >= partialsize) && !headers.empty())
            {
                result.second = headers.back();
                headers.pop_back();
            }

            return result;
        }
        oldsum = newsum;
    }
    return result;
}

/*! \brief Parse a range request header
*
* \note If we fail to parse the header, we return HTTP 200 and the range request is
* effectively ignored. We return HTTP 416 Requested Range Not Satisfiable where it
* is parsed but is invalid. If parsed and valid, we return HTTP 206 (Partial Content).
*
* \todo Per the specs, sending complicated range requests could potentially be used
* as part of a (D)DOS attack and it is suggested that various complicated requests
* are rejected. Probably unnecessary here.
*
*/
MythHTTPStatus MythHTTPRanges::ParseRanges(const QString& Request, int64_t TotalSize,
                                           HTTPRanges& Ranges, int64_t& PartialSize)
{
    MythHTTPStatus result = HTTPOK;
    Ranges.clear();

    // Just don't...
    if (TotalSize < 2)
        return result;

    LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Parsing: '%1'").arg(Request));

    // Split unit and range(s)
    QStringList initial = Request.toLower().split("=");
    if (initial.size() != 2)
    {
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Failed to parse ranges: '%1'").arg(Request));
        return result;
    }

    // We only recognise bytes
    if (!initial.at(0).contains("bytes"))
    {
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Unkown range units: '%1'").arg(initial.at(0)));
        return result;
    }

    // Split out individual ranges
    QStringList rangelist = initial.at(1).split(",", Qt::SkipEmptyParts);

    // No ranges
    if (rangelist.isEmpty())
    {
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Failed to find ranges: '%1'").arg(initial.at(1)));
        return result;
    }

    // Iterate over items
    HTTPRanges ranges;
    for (auto & range : rangelist)
    {
        QStringList parts = range.split("-");
        if (parts.size() != 2)
        {
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Failed to parse range: '%1'").arg(range));
            return result;
        }

        bool validrange = false;
        bool startvalid = false;
        bool endvalid   = false;
        bool startstr   = !parts.at(0).isEmpty();
        bool endstr     = !parts.at(1).isEmpty();
        uint64_t start  = parts.at(0).toULongLong(&startvalid);
        uint64_t end    = parts.at(1).toULongLong(&endvalid);

        // Regular range
        if (startstr && endstr && startvalid && endvalid)
        {
            validrange = ((end < static_cast<uint64_t>(TotalSize)) && (start <= end));
        }
        // Start only
        else if (startstr && startvalid)
        {
            end = static_cast<uint64_t>(TotalSize - 1);
            validrange = start <= end;
        }
        // End only
        else if (endstr && endvalid)
        {
            uint64_t size = end;
            end = static_cast<uint64_t>(TotalSize) - 1;
            start = static_cast<uint64_t>(TotalSize) - size;
            validrange = start <= end;
        }

        if (!validrange)
        {
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Invalid HTTP range: '%1'").arg(range));
            return HTTPRequestedRangeNotSatisfiable;
        }

        ranges.emplace_back(start, end);
    }

    // Rationalise so that we have the simplest, most efficient list of ranges:
    // - sort
    // - merge overlaps (also allowing for minimum multipart header overhead)
    // - remove duplicates
    static const int s_overhead = 90; // rough worst case overhead per part for multipart requests
    if (ranges.size() > 1)
    {
        auto equals = [](HTTPRange First, HTTPRange Second)
            { return (First.first == Second.first) && (First.second == Second.second); };
        auto lessthan = [](HTTPRange First, HTTPRange Second)
            { return First.first < Second.first; };

        // we MUST sort first
        std::sort(ranges.begin(), ranges.end(), lessthan);

        if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_INFO))
        {
            QStringList debug;
            for (const auto & range : ranges)
                debug.append(QString("%1:%2").arg(range.first).arg(range.second));
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Sorted ranges: %1").arg(debug.join(" ")));
        }

        // merge, de-duplicate, repeat...
        bool finished = false;
        while (!finished)
        {
            finished = true;
            for (uint i = 0; i < (ranges.size() - 1); ++i)
            {
                if ((ranges[i].second + s_overhead) >= ranges[i + 1].first)
                {
                    finished = false;
                    ranges[i + 1].first = ranges[i].first;
                    // N.B we have sorted by start byte - not end
                    uint64_t end = std::max(ranges[i].second, ranges[i + 1].second);
                    ranges[i].second = ranges[i + 1].second = end;
                }
            }

            auto last = std::unique(ranges.begin(), ranges.end(), equals);
            ranges.erase(last, ranges.end());
        }
    }

    // Sum the expected number of bytes to be sent
    PartialSize = std::accumulate(ranges.cbegin(), ranges.cend(), static_cast<uint64_t>(0), sumrange);
    Ranges = ranges;

    if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_INFO))
    {
        QStringList debug;
        for (const auto & range : ranges)
            debug.append(QString("%1:%2").arg(range.first).arg(range.second));
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Final ranges : %1").arg(debug.join(" ")));
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Bytes to send: %1").arg(PartialSize));
    }

    return HTTPPartialContent;
}
