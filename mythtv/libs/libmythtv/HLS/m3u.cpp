#include <QStringList>
#include <QUrl>

#include "HLS/m3u.h"
#include "mythlogging.h"

namespace M3U
{
    QString DecodedURI(const QString& uri)
    {
        QByteArray ba   = uri.toLatin1();
        QUrl url        = QUrl::fromEncoded(ba);
        return url.toString();
    }

    QString RelativeURI(const QString& surl, const QString& spath)
    {
        QUrl url  = QUrl(surl);
        QUrl path = QUrl(spath);

        if (!path.isRelative())
            return spath;

        return url.resolved(path).toString();
    }

    QString ParseAttributes(const QString& line, const char *attr)
    {
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
            return QString();

        QStringList list = QStringList(line.mid(p + 1).split(','));
        QStringList::iterator it = list.begin();
        for (; it != list.end(); ++it)
        {
            QString arg = (*it).trimmed();
            if (arg.startsWith(attr))
            {
                int pos = arg.indexOf(QLatin1String("="));
                if (pos < 0)
                    continue;
                return arg.mid(pos+1);
            }
        }
        return QString();
    }

    /**
     * Return the decimal argument in a line of type: blah:<decimal>
     * presence of valud <decimal> is compulsory or it will return RET_ERROR
     */
    bool ParseDecimalValue(const QString& line, int &target)
    {
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
            return false;
        int i = p;
        while (++i < line.size() && line[i].isNumber());
        if (i == p + 1)
            return false;
        target = line.mid(p + 1, i - p - 1).toInt();
        return true;
    }

    /**
     * Return the decimal argument in a line of type: blah:<decimal>
     * presence of valud <decimal> is compulsory or it will return RET_ERROR
     */
    bool ParseDecimalValue(const QString& line, int64_t &target)
    {
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
            return false;
        int i = p;
        while (++i < line.size() && line[i].isNumber());
        if (i == p + 1)
            return false;
        target = line.mid(p + 1, i - p - 1).toInt();
        return true;
    }

    bool ParseVersion(const QString& line, const QString& loc, int& version)
    {
        /*
         * The EXT-X-VERSION tag indicates the compatibility version of the
         * Playlist file.  The Playlist file, its associated media, and its
         * server MUST comply with all provisions of the most-recent version of
         * this document describing the protocol version indicated by the tag
         * value.
         *
         * Its format is:
         *
         * #EXT-X-VERSION:<n>
         */

        if (line.isNull() || !ParseDecimalValue(line, version))
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                "#EXT-X-VERSION: no protocol version found, should be version 1.");
            version = 1;
            return false;
        }

        if (version <= 0 || version > 3)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("#EXT-X-VERSION is %1, but we only understand 0 through 3")
                .arg(version));
            return false;
        }

        return true;
    }

    bool ParseStreamInformation(const QString& line,
                                const QString& url,
                                const QString& loc,
                                int& id, uint64_t& bandwidth)
    {
        /*
         * #EXT-X-STREAM-INF:[attribute=value][,attribute=value]*
         *  <URI>
         */
        QString attr;

        attr = ParseAttributes(line, "PROGRAM-ID");
        if (attr.isNull())
        {
            LOG(VB_RECORD, LOG_INFO, loc +
                "#EXT-X-STREAM-INF: expected PROGRAM-ID=<value>, using -1");
            id = -1;
        }
        else
        {
            id = attr.toInt();
        }

        attr = ParseAttributes(line, "BANDWIDTH");
        if (attr.isNull())
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                "#EXT-X-STREAM-INF: expected BANDWIDTH=<value>");
            return false;
        }
        bandwidth = attr.toInt();

        if (bandwidth == 0)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                "#EXT-X-STREAM-INF: bandwidth cannot be 0");
            return false;
        }

        LOG(VB_RECORD, LOG_INFO, loc +
            QString("bandwidth adaptation detected (program-id=%1, bandwidth=%2")
            .arg(id).arg(bandwidth));

        return true;
    }

    bool ParseTargetDuration(const QString& line, const QString& loc,
                             int& duration)
    {
        /*
         * #EXT-X-TARGETDURATION:<s>
         *
         * where s is an integer indicating the target duration in seconds.
         */
        duration = -1;

        if (!ParseDecimalValue(line, duration))
        {
            LOG(VB_RECORD, LOG_ERR, loc + "expected #EXT-X-TARGETDURATION:<s>");
            return false;
        }

        return true;
    }

    bool ParseSegmentInformation(int version, const QString& line,
                                 uint& duration, QString& title,
                                 const QString& loc)
    {
        /*
         * #EXTINF:<duration>,<title>
         *
         * "duration" is an integer that specifies the duration of the media
         * file in seconds.  Durations SHOULD be rounded to the nearest integer.
         * The remainder of the line following the comma is the title of the
         * media file, which is an optional human-readable informative title of
         * the media segment
         */
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseSegmentInformation: Missing ':' in '%1'")
                .arg(line));
            return false;
        }

        QStringList list = QStringList(line.mid(p + 1).split(','));

        /* read duration */
        if (list.isEmpty())
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseSegmentInformation: Missing arguments in '%1'")
                .arg(line));
            return false;
        }

        QString val = list[0];
        bool ok;

        if (version < 3)
        {
            duration = val.toInt(&ok);
            if (!ok)
            {
                duration = -1;
                LOG(VB_RECORD, LOG_ERR, loc +
                    QString("ParseSegmentInformation: invalid duration in '%1'")
                    .arg(line));
            }
        }
        else
        {
            double d = val.toDouble(&ok);
            if (!ok)
            {
                duration = -1;
                LOG(VB_RECORD, LOG_ERR, loc +
                    QString("ParseSegmentInformation: invalid duration in '%1'")
                    .arg(line));
                return false;
            }
            if ((d) - ((int)d) >= 0.5)
                duration = ((int)d) + 1;
            else
                duration = ((int)d);
        }

        if (list.size() >= 2)
        {
            title = list[1];
        }

        /* Ignore the rest of the line */
        return true;
    }

    bool ParseMediaSequence(int64_t & sequence_num,
                            const QString& line, const QString& loc)
    {
        /*
         * #EXT-X-MEDIA-SEQUENCE:<number>
         *
         * A Playlist file MUST NOT contain more than one EXT-X-MEDIA-SEQUENCE
         * tag.  If the Playlist file does not contain an EXT-X-MEDIA-SEQUENCE
         * tag then the sequence number of the first URI in the playlist SHALL
         * be considered to be 0.
         */

        if (!ParseDecimalValue(line, sequence_num))
        {
            LOG(VB_RECORD, LOG_ERR, loc + "expected #EXT-X-MEDIA-SEQUENCE:<s>");
            return false;
        }

        return true;
    }

    bool ParseKey(int version, const QString& line, bool& aesmsg,
                  const QString& loc, QString &path, QString &iv)
    {
        /*
         * #EXT-X-KEY:METHOD=<method>[,URI="<URI>"][,IV=<IV>]
         *
         * The METHOD attribute specifies the encryption method.  Two encryption
         * methods are defined: NONE and AES-128.
         */

        path.clear();
        iv.clear();

        QString attr = ParseAttributes(line, "METHOD");
        if (attr.isNull())
        {
            LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: expected METHOD=<value>");
            return false;
        }

        if (attr.startsWith(QLatin1String("NONE")))
        {
            QString uri = ParseAttributes(line, "URI");
            if (!uri.isNull())
            {
                LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: URI not expected");
                return false;
            }
            /* IV is only supported in version 2 and above */
            if (version >= 2)
            {
                iv = ParseAttributes(line, "IV");
                if (!iv.isNull())
                {
                    LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: IV not expected");
                    return false;
                }
            }
        }
#ifdef USING_LIBCRYPTO
        else if (attr.startsWith(QLatin1String("AES-128")))
        {
            QString uri;
            if (aesmsg == false)
            {
                LOG(VB_RECORD, LOG_INFO, loc +
                    "playback of AES-128 encrypted HTTP Live media detected.");
                aesmsg = true;
            }
            uri = ParseAttributes(line, "URI");
            if (uri.isNull())
            {
                LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: URI not found for "
                    "encrypted HTTP Live media in AES-128");
                return false;
            }

            /* Url is between quotes, remove them */
            path = DecodedURI(uri.remove(QChar(QLatin1Char('"'))));
            iv = ParseAttributes(line, "IV");
        }
#endif
        else
        {
#ifndef _MSC_VER
            LOG(VB_RECORD, LOG_ERR, loc +
                "invalid encryption type, only NONE "
#ifdef USING_LIBCRYPTO
                "and AES-128 are supported"
#else
                "is supported."
#endif
                );
#else
// msvc doesn't like #ifdef in the middle of the LOG macro.
// Errors with '#':invalid character: possibly the result of a macro expansion
#endif
            return false;
        }
        return true;
    }

    bool ParseProgramDateTime(const QString& line, const QString& loc,
                              QDateTime &date)
    {
        /*
         * #EXT-X-PROGRAM-DATE-TIME:<YYYY-MM-DDThh:mm:ssZ>
         */
        LOG(VB_RECORD, LOG_DEBUG, loc +
            QString("tag not supported: #EXT-X-PROGRAM-DATE-TIME %1")
            .arg(line));
        return true;
    }

    bool ParseAllowCache(const QString& line, const QString& loc, bool &do_cache)
    {
        /*
         * The EXT-X-ALLOW-CACHE tag indicates whether the client MAY or MUST
         * NOT cache downloaded media files for later replay.  It MAY occur
         * anywhere in the Playlist file; it MUST NOT occur more than once.  The
         * EXT-X-ALLOW-CACHE tag applies to all segments in the playlist.  Its
         * format is:
         *
         * #EXT-X-ALLOW-CACHE:<YES|NO>
         */

        int pos = line.indexOf(QLatin1String(":"));
        if (pos < 0)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseAllowCache: missing ':' in '%1'")
                .arg(line));
            return false;
        }
        QString answer = line.mid(pos+1, 3);
        if (answer.size() < 2)
        {
            LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-ALLOW-CACHE, ignoring ...");
            return true;
        }
        do_cache = (!answer.startsWith(QLatin1String("NO")));

        return true;
    }

    bool ParseDiscontinuity(const QString& line, const QString& loc)
    {
        /* Not handled, never seen so far */
        LOG(VB_RECORD, LOG_DEBUG, loc + QString("#EXT-X-DISCONTINUITY %1")
            .arg(line));
        return true;
    }

    bool ParseEndList(const QString& loc, bool& is_vod)
    {
        /*
         * The EXT-X-ENDLIST tag indicates that no more media files will be
         * added to the Playlist file.  It MAY occur anywhere in the Playlist
         * file; it MUST NOT occur more than once.  Its format is:
         */
        is_vod = true;
        LOG(VB_RECORD, LOG_INFO, loc + "video on demand (vod) mode");
        return true;
    }

} // Namespace M3U
