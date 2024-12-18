/*  -*- Mode: c++ -*-
 *
 *   Class HTTPLiveStream
 *
 *   Copyright (C) Chris Pinkham 2011
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// POSIX headers
#include <unistd.h> // for usleep

// C headers
#include <cstdio>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QRunnable>
#include <QUrl>
#include <utility>

#include "libmythbase/exitcodes.h"
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/storagegroup.h"

#include "httplivestream.h"

#define LOC QString("HLS(%1): ").arg(m_sourceFile)
#define LOC_ERR QString("HLS(%1) Error: ").arg(m_sourceFile)
#define SLOC QString("HLS(): ")
#define SLOC_ERR QString("HLS() Error: ")

/** \class HTTPLiveStreamThread
 *  \brief QRunnable class for running mythtranscode for HTTP Live Streams
 *
 *  The HTTPLiveStreamThread class runs a mythtranscode command in
 *  non-blocking mode.
 */
class HTTPLiveStreamThread : public QRunnable
{
  public:
    /** \fn HTTPLiveStreamThread::HTTPLiveStreamThread(int)
     *  \brief Constructor for creating a SystemEventThread
     *  \param streamid The stream identifier.
     */
    explicit HTTPLiveStreamThread(int streamid)
      : m_streamID(streamid) {}

    /** \fn HTTPLiveStreamThread::run()
     *  \brief Runs mythtranscode for the given HTTP Live Stream ID
     *
     *  Overrides QRunnable::run()
     */
    void run(void) override // QRunnable
    {
        uint flags = kMSDontBlockInputDevs;

        QString command = GetAppBinDir() +
            QString("mythtranscode --hls --hlsstreamid %1")
                    .arg(m_streamID) + logPropagateArgs;

        uint result = myth_system(command, flags);

        if (result != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_WARNING, SLOC +
                QString("Command '%1' returned %2")
                    .arg(command).arg(result));
        }
    }

  private:
    int m_streamID;
};


HTTPLiveStream::HTTPLiveStream(QString srcFile, uint16_t width, uint16_t height,
                               uint32_t bitrate, uint32_t abitrate,
                               uint16_t maxSegments, uint16_t segmentSize,
                               uint32_t aobitrate, int32_t srate)
  : m_sourceFile(std::move(srcFile)),
    m_segmentSize(segmentSize),  m_maxSegments(maxSegments),
    m_height(height),            m_width(width),
    m_bitrate(bitrate),
    m_audioBitrate(abitrate),    m_audioOnlyBitrate(aobitrate),
    m_sampleRate(srate),
    m_created(MythDate::current()),
    m_lastModified(MythDate::current())
{
    if ((m_width == 0) && (m_height == 0))
        m_width = 640;

    if (m_bitrate == 0)
        m_bitrate = 800000;

    if (m_audioBitrate == 0)
        m_audioBitrate = 64000;

    if (m_segmentSize == 0)
        m_segmentSize = 4;

    if (m_audioOnlyBitrate == 0)
        m_audioOnlyBitrate = 64000;

    m_sourceHost = gCoreContext->GetHostName();

    QFileInfo finfo(m_sourceFile);
    m_outBase = finfo.fileName() +
        QString(".%1x%2_%3kV_%4kA").arg(m_width).arg(m_height)
                .arg(m_bitrate/1000).arg(m_audioBitrate/1000);

    SetOutputVars();

    m_fullURL     = m_httpPrefix + m_outBase + ".m3u8";
    m_relativeURL = m_httpPrefixRel + m_outBase + ".m3u8";

    StorageGroup sgroup("Streaming", gCoreContext->GetHostName());
    m_outDir = sgroup.GetFirstDir();
    QDir outDir(m_outDir);

    if (!outDir.exists() && !outDir.mkdir(m_outDir))
    {
        LOG(VB_RECORD, LOG_ERR, "Unable to create HTTP Live Stream output "
            "directory, Live Stream will not be created");
        return;
    }

    AddStream();
}

HTTPLiveStream::HTTPLiveStream(int streamid)
  : m_streamid(streamid)
{
    LoadFromDB();
}

HTTPLiveStream::~HTTPLiveStream()
{
    if (m_writing)
    {
        WritePlaylist(false, true);
        if (m_audioOnlyBitrate)
            WritePlaylist(true, true);
    }
}

bool HTTPLiveStream::InitForWrite(void)
{
    if ((m_streamid == -1) ||
        (!WriteHTML()) ||
        (!WriteMetaPlaylist()) ||
        (!UpdateStatus(kHLSStatusStarting)) ||
        (!UpdateStatusMessage("Transcode Starting")))
        return false;

    m_writing = true;

    return true;
}

QString HTTPLiveStream::GetFilename(uint16_t segmentNumber, bool fileOnly,
                                    bool audioOnly, bool encoded) const
{
    QString filename;

    if (encoded)
        filename = audioOnly ? m_audioOutFileEncoded : m_outFileEncoded;
    else
        filename = audioOnly ? m_audioOutFile : m_outFile;

    filename += ".%1.ts";

    if (!fileOnly)
        filename = m_outDir + "/" + filename;

    if (segmentNumber)
        return filename.arg(segmentNumber, 6, 10, QChar('0'));

    return filename.arg(1, 6, 10, QChar('0'));
}

QString HTTPLiveStream::GetCurrentFilename(bool audioOnly, bool encoded) const
{
    return GetFilename(m_curSegment, false, audioOnly, encoded);
}

int HTTPLiveStream::AddStream(void)
{
    m_status = kHLSStatusQueued;

    QString tmpBase = QString("");
    QString tmpFullURL = QString("");
    QString tmpRelURL = QString("");

    if (m_width && m_height)
    {
        tmpBase = m_outBase;
        tmpFullURL = m_fullURL;
        tmpRelURL = m_relativeURL;
    }

    // Check that this stream has not already been created.
    // We want to avoid creating multiple identical streams and transcode
    // jobs
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT id FROM livestream "
        "WHERE "
        "(width = :WIDTH OR height = :HEIGHT) AND bitrate = :BITRATE AND "
        "audioonlybitrate = :AUDIOONLYBITRATE AND samplerate = :SAMPLERATE AND "
        "audiobitrate = :AUDIOBITRATE AND segmentsize = :SEGMENTSIZE AND "
        "sourcefile = :SOURCEFILE AND status <= :STATUS ");
    query.bindValue(":WIDTH", m_width);
    query.bindValue(":HEIGHT", m_height);
    query.bindValue(":BITRATE", m_bitrate);
    query.bindValue(":AUDIOBITRATE", m_audioBitrate);
    query.bindValue(":SEGMENTSIZE", m_segmentSize);
    query.bindValue(":STATUS", (int)kHLSStatusCompleted);
    query.bindValue(":SOURCEFILE", m_sourceFile);
    query.bindValue(":AUDIOONLYBITRATE", m_audioOnlyBitrate);
    query.bindValue(":SAMPLERATE", (m_sampleRate == -1) ? 0 : m_sampleRate); // samplerate column is unsigned, -1 becomes 0

    if (!query.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "LiveStream existing stream check failed.");
        return -1;
    }

    if (!query.next())
    {
        query.prepare(
            "INSERT INTO livestream "
            "    ( width, height, bitrate, audiobitrate, segmentsize, "
            "      maxsegments, startsegment, currentsegment, segmentcount, "
            "      percentcomplete, created, lastmodified, relativeurl, "
            "      fullurl, status, statusmessage, sourcefile, sourcehost, "
            "      sourcewidth, sourceheight, outdir, outbase, "
            "      audioonlybitrate, samplerate ) "
            "VALUES "
            "    ( :WIDTH, :HEIGHT, :BITRATE, :AUDIOBITRATE, :SEGMENTSIZE, "
            "      :MAXSEGMENTS, 0, 0, 0, "
            "      0, :CREATED, :LASTMODIFIED, :RELATIVEURL, "
            "      :FULLURL, :STATUS, :STATUSMESSAGE, :SOURCEFILE, :SOURCEHOST, "
            "      :SOURCEWIDTH, :SOURCEHEIGHT, :OUTDIR, :OUTBASE, "
            "      :AUDIOONLYBITRATE, :SAMPLERATE ) ");
        query.bindValue(":WIDTH", m_width);
        query.bindValue(":HEIGHT", m_height);
        query.bindValue(":BITRATE", m_bitrate);
        query.bindValue(":AUDIOBITRATE", m_audioBitrate);
        query.bindValue(":SEGMENTSIZE", m_segmentSize);
        query.bindValue(":MAXSEGMENTS", m_maxSegments);
        query.bindValue(":CREATED", m_created);
        query.bindValue(":LASTMODIFIED", m_lastModified);
        query.bindValue(":RELATIVEURL", tmpRelURL);
        query.bindValue(":FULLURL", tmpFullURL);
        query.bindValue(":STATUS", (int)m_status);
        query.bindValue(":STATUSMESSAGE",
            QString("Waiting for mythtranscode startup."));
        query.bindValue(":SOURCEFILE", m_sourceFile);
        query.bindValue(":SOURCEHOST", gCoreContext->GetHostName());
        query.bindValue(":SOURCEWIDTH", 0);
        query.bindValue(":SOURCEHEIGHT", 0);
        query.bindValue(":OUTDIR", m_outDir);
        query.bindValue(":OUTBASE", tmpBase);
        query.bindValue(":AUDIOONLYBITRATE", m_audioOnlyBitrate);
        query.bindValue(":SAMPLERATE", (m_sampleRate == -1) ? 0 : m_sampleRate); // samplerate column is unsigned, -1 becomes 0

        if (!query.exec())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveStream insert failed.");
            return -1;
        }

        if (!query.exec("SELECT LAST_INSERT_ID()") || !query.next())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to query LiveStream streamid.");
            return -1;
        }
    }

    m_streamid = query.value(0).toUInt();

    return m_streamid;
}

bool HTTPLiveStream::AddSegment(void)
{
    if (m_streamid == -1)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    ++m_curSegment;
    ++m_segmentCount;

    if (!m_startSegment)
        m_startSegment = m_curSegment;

    if ((m_maxSegments) &&
        (m_segmentCount > (uint16_t)(m_maxSegments + 1)))
    {
        QString thisFile = GetFilename(m_startSegment);

        if (!QFile::remove(thisFile))
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unable to delete %1.").arg(thisFile));

        ++m_startSegment;
        --m_segmentCount;
    }

    SaveSegmentInfo();
    WritePlaylist(false);

    if (m_audioOnlyBitrate)
        WritePlaylist(true);

    return true;
}

QString HTTPLiveStream::GetHTMLPageName(void) const
{
    if (m_streamid == -1)
        return {};

    QString outFile = m_outDir + "/" + m_outBase + ".html";
    return outFile;
}

bool HTTPLiveStream::WriteHTML(void)
{
    if (m_streamid == -1)
        return false;

    QString outFile = m_outDir + "/" + m_outBase + ".html";
    QFile file(outFile);

    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(VB_RECORD, LOG_ERR, QString("Error opening %1").arg(outFile));
        return false;
    }

    file.write(QString(
        "<html>\n"
        "  <head>\n"
        "    <title>%1</title>\n"
        "  </head>\n"
        "  <body style='background-color:#FFFFFF;'>\n"
        "    <center>\n"
        "      <video controls>\n"
        "        <source src='%2.m3u8' />\n"
        "      </video>\n"
        "    </center>\n"
        "  </body>\n"
        "</html>\n"
        ).arg(m_sourceFile, m_outBaseEncoded)
         .toLatin1());

    file.close();

    return true;
}

QString HTTPLiveStream::GetMetaPlaylistName(void) const
{
    if (m_streamid == -1)
        return {};

    QString outFile = m_outDir + "/" + m_outBase + ".m3u8";
    return outFile;
}

bool HTTPLiveStream::WriteMetaPlaylist(void)
{
    if (m_streamid == -1)
        return false;

    QString outFile = GetMetaPlaylistName();
    QFile file(outFile);

    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(VB_RECORD, LOG_ERR, QString("Error opening %1").arg(outFile));
        return false;
    }

    file.write(QString(
        "#EXTM3U\n"
        "#EXT-X-VERSION:4\n"
        "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"AV\",NAME=\"Main\",DEFAULT=YES,URI=\"%2.m3u8\"\n"
        "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%1\n"
        "%2.m3u8\n"
        ).arg((int)((m_bitrate + m_audioBitrate) * 1.1))
         .arg(m_outFileEncoded).toLatin1());

    if (m_audioOnlyBitrate)
    {
        file.write(QString(
            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"AO\",NAME=\"Main\",DEFAULT=NO,URI=\"%2.m3u8\"\n"
            "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=%1\n"
            "%2.m3u8\n"
            ).arg((int)((m_audioOnlyBitrate) * 1.1))
             .arg(m_audioOutFileEncoded).toLatin1());
    }

    file.close();

    return true;
}

QString HTTPLiveStream::GetPlaylistName(bool audioOnly) const
{
    if (m_streamid == -1)
        return {};

    if (audioOnly && m_audioOutFile.isEmpty())
        return {};

    QString base = audioOnly ? m_audioOutFile : m_outFile;
    QString outFile = m_outDir + "/" + base + ".m3u8";
    return outFile;
}

bool HTTPLiveStream::WritePlaylist(bool audioOnly, bool writeEndTag)
{
    if (m_streamid == -1)
        return false;

    QString outFile = GetPlaylistName(audioOnly);
    QString tmpFile = outFile + ".tmp";

    QFile file(tmpFile);

    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(VB_RECORD, LOG_ERR, QString("Error opening %1").arg(tmpFile));
        return false;
    }

    file.write(QString(
        "#EXTM3U\n"
        "#EXT-X-ALLOW-CACHE:YES\n"
        "#EXT-X-TARGETDURATION:%1\n"
        "#EXT-X-MEDIA-SEQUENCE:%2\n"
        ).arg(m_segmentSize).arg(m_startSegment).toLatin1());

    if (writeEndTag)
        file.write("#EXT-X-ENDLIST\n");

    // Don't write out the current segment until the end
    unsigned int tmpSegCount = m_segmentCount - 1;
    unsigned int i = 0;
    unsigned int segmentid = m_startSegment;

    if (writeEndTag)
        ++tmpSegCount;

    while (i < tmpSegCount)
    {
        file.write(QString(
            "#EXTINF:%1,\n"
            "%2\n"
            ).arg(m_segmentSize)
             .arg(GetFilename(segmentid + i, true, audioOnly, true)).toLatin1());

        ++i;
    }

    file.close();

    if(rename(tmpFile.toLatin1().constData(),
              outFile.toLatin1().constData()) == -1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Error renaming %1 to %2").arg(tmpFile, outFile) + ENO);
        return false;
    }

    return true;
}

bool HTTPLiveStream::SaveSegmentInfo(void)
{
    if (m_streamid == -1)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE livestream "
        "SET startsegment = :START, currentsegment = :CURRENT, "
        "    segmentcount = :COUNT "
        "WHERE id = :STREAMID; ");
    query.bindValue(":START", m_startSegment);
    query.bindValue(":CURRENT", m_curSegment);
    query.bindValue(":COUNT", m_segmentCount);
    query.bindValue(":STREAMID", m_streamid);

    if (query.exec())
        return true;

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Unable to update segment info for streamid %1")
                .arg(m_streamid));
    return false;
}

bool HTTPLiveStream::UpdateSizeInfo(uint16_t width, uint16_t height,
                                    uint16_t srcwidth, uint16_t srcheight)
{
    if (m_streamid == -1)
        return false;

    QFileInfo finfo(m_sourceFile);
    QString newOutBase = finfo.fileName() +
        QString(".%1x%2_%3kV_%4kA").arg(width).arg(height)
                .arg(m_bitrate/1000).arg(m_audioBitrate/1000);
    QString newFullURL = m_httpPrefix + newOutBase + ".m3u8";
    QString newRelativeURL = m_httpPrefixRel + newOutBase + ".m3u8";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE livestream "
        "SET width = :WIDTH, height = :HEIGHT, "
        "    sourcewidth = :SRCWIDTH, sourceheight = :SRCHEIGHT, "
        "    fullurl = :FULLURL, relativeurl = :RELATIVEURL, "
        "    outbase = :OUTBASE "
        "WHERE id = :STREAMID; ");
    query.bindValue(":WIDTH", width);
    query.bindValue(":HEIGHT", height);
    query.bindValue(":SRCWIDTH", srcwidth);
    query.bindValue(":SRCHEIGHT", srcheight);
    query.bindValue(":FULLURL", newFullURL);
    query.bindValue(":RELATIVEURL", newRelativeURL);
    query.bindValue(":OUTBASE", newOutBase);
    query.bindValue(":STREAMID", m_streamid);

    if (!query.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to update segment info for streamid %1")
                    .arg(m_streamid));
        return false;
    }

    m_width = width;
    m_height = height;
    m_sourceWidth = srcwidth;
    m_sourceHeight = srcheight;
    m_outBase = newOutBase;
    m_fullURL = newFullURL;
    m_relativeURL = newRelativeURL;

    SetOutputVars();

    return true;
}

bool HTTPLiveStream::UpdateStatus(HTTPLiveStreamStatus status)
{
    if (m_streamid == -1)
        return false;

    if ((m_status == kHLSStatusStopping) &&
        (status == kHLSStatusRunning))
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Attempted to switch streamid %1 from "
                    "Stopping to Running State").arg(m_streamid));
        return false;
    }

    QString mStatusStr = StatusToString(m_status);
    QString statusStr = StatusToString(status);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Switch streamid %1 from %2 to %3")
        .arg(QString::number(m_streamid), mStatusStr, statusStr));

    m_status = status;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE livestream "
        "SET status = :STATUS "
        "WHERE id = :STREAMID; ");
    query.bindValue(":STATUS", (int)status);
    query.bindValue(":STREAMID", m_streamid);

    if (query.exec())
        return true;

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Unable to update status for streamid %1").arg(m_streamid));
    return false;
}

bool HTTPLiveStream::UpdateStatusMessage(const QString& message)
{
    if (m_streamid == -1)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE livestream "
        "SET statusmessage = :MESSAGE "
        "WHERE id = :STREAMID; ");
    query.bindValue(":MESSAGE", message);
    query.bindValue(":STREAMID", m_streamid);

    if (query.exec())
    {
        m_statusMessage = message;
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Unable to update status message for streamid %1")
                .arg(m_streamid));
    return false;
}

bool HTTPLiveStream::UpdatePercentComplete(int percent)
{
    if (m_streamid == -1)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE livestream "
        "SET percentcomplete = :PERCENT "
        "WHERE id = :STREAMID; ");
    query.bindValue(":PERCENT", percent);
    query.bindValue(":STREAMID", m_streamid);

    if (query.exec())
    {
        m_percentComplete = percent;
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Unable to update percent complete for streamid %1")
                .arg(m_streamid));
    return false;
}

QString HTTPLiveStream::StatusToString(HTTPLiveStreamStatus status)
{
    switch (status) {
        case kHLSStatusUndefined : return {"Undefined"};
        case kHLSStatusQueued    : return {"Queued"};
        case kHLSStatusStarting  : return {"Starting"};
        case kHLSStatusRunning   : return {"Running"};
        case kHLSStatusCompleted : return {"Completed"};
        case kHLSStatusErrored   : return {"Errored"};
        case kHLSStatusStopping  : return {"Stopping"};
        case kHLSStatusStopped   : return {"Stopped"};
    };

    return {"Unknown status value"};
}

bool HTTPLiveStream::LoadFromDB(void)
{
    if (m_streamid == -1)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT width, height, bitrate, audiobitrate, segmentsize, "
        "   maxsegments, startsegment, currentsegment, segmentcount, "
        "   percentcomplete, created, lastmodified, relativeurl, "
        "   fullurl, status, statusmessage, sourcefile, sourcehost, "
        "   sourcewidth, sourceheight, outdir, outbase, audioonlybitrate, "
        "   samplerate "
        "FROM livestream "
        "WHERE id = :STREAMID; ");
    query.bindValue(":STREAMID", m_streamid);

    if (!query.exec() || !query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to query DB info for stream %1")
                    .arg(m_streamid));
        return false;
    }

    m_width              = query.value(0).toUInt();
    m_height             = query.value(1).toUInt();
    m_bitrate            = query.value(2).toUInt();
    m_audioBitrate       = query.value(3).toUInt();
    m_segmentSize        = query.value(4).toUInt();
    m_maxSegments        = query.value(5).toUInt();
    m_startSegment       = query.value(6).toUInt();
    m_curSegment         = query.value(7).toUInt();
    m_segmentCount       = query.value(8).toUInt();
    m_percentComplete    = query.value(9).toUInt();
    m_created            = MythDate::as_utc(query.value(10).toDateTime());
    m_lastModified       = MythDate::as_utc(query.value(11).toDateTime());
    m_relativeURL        = query.value(12).toString();
    m_fullURL            = query.value(13).toString();
    m_status             = (HTTPLiveStreamStatus)(query.value(14).toInt());
    m_statusMessage      = query.value(15).toString();
    m_sourceFile         = query.value(16).toString();
    m_sourceHost         = query.value(17).toString();
    m_sourceWidth        = query.value(18).toUInt();
    m_sourceHeight       = query.value(19).toUInt();
    m_outDir             = query.value(20).toString();
    m_outBase            = query.value(21).toString();
    m_audioOnlyBitrate   = query.value(22).toUInt();
    m_sampleRate         = query.value(23).toUInt();

    SetOutputVars();

    return true;
}

void HTTPLiveStream::SetOutputVars(void)
{
    m_outBaseEncoded = QString(QUrl::toPercentEncoding(m_outBase, "", " "));

    m_outFile        = m_outBase + ".av";
    m_outFileEncoded = m_outBaseEncoded + ".av";

    if (m_audioOnlyBitrate)
    {
        m_audioOutFile = m_outBase +
            QString(".ao_%1kA").arg(m_audioOnlyBitrate/1000);
        m_audioOutFileEncoded = m_outBaseEncoded +
            QString(".ao_%1kA").arg(m_audioOnlyBitrate/1000);
    }

    m_httpPrefix = gCoreContext->GetSetting("HTTPLiveStreamPrefix", QString(
        "http://%1:%2/StorageGroup/Streaming/")
        .arg(gCoreContext->GetMasterServerIP())
        .arg(gCoreContext->GetMasterServerStatusPort()));

    if (!m_httpPrefix.endsWith("/"))
        m_httpPrefix.append("/");

    if (!gCoreContext->GetSetting("HTTPLiveStreamPrefixRel").isEmpty())
    {
        m_httpPrefixRel = gCoreContext->GetSetting("HTTPLiveStreamPrefixRel");
        if (!m_httpPrefix.endsWith("/"))
            m_httpPrefix.append("/");
    }
    else if (m_httpPrefix.contains("/StorageGroup/Streaming/"))
    {
        m_httpPrefixRel = "/StorageGroup/Streaming/";
    }
    else
    {
        m_httpPrefixRel = "";
    }
}

HTTPLiveStreamStatus HTTPLiveStream::GetDBStatus(void) const
{
    if (m_streamid == -1)
        return kHLSStatusUndefined;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT status FROM livestream "
        "WHERE id = :STREAMID; ");
    query.bindValue(":STREAMID", m_streamid);

    if (!query.exec() || !query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to check stop status for stream %1")
                    .arg(m_streamid));
        return kHLSStatusUndefined;
    }

    return (HTTPLiveStreamStatus)query.value(0).toInt();
}

bool HTTPLiveStream::CheckStop(void)
{
    if (m_streamid == -1)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT status FROM livestream "
        "WHERE id = :STREAMID; ");
    query.bindValue(":STREAMID", m_streamid);

    if (!query.exec() || !query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to check stop status for stream %1")
                    .arg(m_streamid));
        return false;
    }

    return query.value(0).toInt() == (int)kHLSStatusStopping;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
