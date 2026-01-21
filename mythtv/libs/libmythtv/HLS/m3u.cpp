#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "HLS/m3u.h"

namespace M3U
{
    static const QRegularExpression kQuotes{"^\"|\"$"};

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
            return {};

        QStringList list = line.mid(p + 1).split(',');
        for (const auto & it : std::as_const(list))
        {
            QString arg = it.trimmed();
            if (arg.startsWith(attr))
            {
                int pos = arg.indexOf(QLatin1String("="));
                if (pos < 0)
                    continue;
                return arg.mid(pos+1);
            }
        }
        return {};
    }

    /**
     * Return the decimal argument in a line of type: blah:\<decimal\>
     * presence of value \<decimal\> is compulsory or it will return RET_ERROR
     */
    bool ParseDecimalValue(const QString& line, int &target)
    {
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
            return false;
        int i = p + 1;
        for ( ; i < line.size(); i++)
            if (!line[i].isNumber())
                break;
        if (i == p + 1)
            return false;
        target = line.mid(p + 1, i - p - 1).toInt();
        return true;
    }

    /**
     * Return the decimal argument in a line of type: blah:\<decimal\>
     * presence of value \<decimal\> is compulsory or it will return RET_ERROR
     */
    bool ParseDecimalValue(const QString& line, int64_t &target)
    {
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
            return false;
        int i = p + 1;
        for ( ; i < line.size(); i++)
            if (!line[i].isNumber())
                break;
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

        if (version < 1 || version > 7)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("#EXT-X-VERSION is %1, but we only understand 1 to 7")
                .arg(version));
            return false;
        }

        return true;
    }

    // EXT-X-STREAM-INF
    //
    bool ParseStreamInformation(const QString& line,
                                const QString& url,
                                const QString& loc,
                                int& id,
                                uint64_t& bandwidth,
                                QString& audio,
                                QString& video)
    {
        LOG(VB_RECORD, LOG_INFO, loc +
            QString("Parsing stream from %1").arg(url));

        /*
         * #EXT-X-STREAM-INF:[attribute=value][,attribute=value]*
         *  <URI>
         */
        QString attr;

        /* The PROGRAM-ID attribute of the EXT-X-STREAM-INF and the EXT-X-I-
         * FRAME-STREAM-INF tags was removed in protocol version 6.
         */
        attr = ParseAttributes(line, "PROGRAM-ID");
        if (attr.isNull())
        {
            LOG(VB_RECORD, LOG_INFO, loc +
                "#EXT-X-STREAM-INF: No PROGRAM-ID=<value>, using 1");
            id = 1;
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

        //      AUDIO
        //
        //   The value is a quoted-string.  It MUST match the value of the
        //   GROUP-ID attribute of an EXT-X-MEDIA tag elsewhere in the Master
        //   Playlist whose TYPE attribute is AUDIO.  It indicates the set of
        //   audio Renditions that SHOULD be used when playing the
        //   presentation.  See Section 4.3.4.2.1.
        //
        //   The AUDIO attribute is OPTIONAL.
        audio = ParseAttributes(line, "AUDIO");
        if (!audio.isEmpty())
        {
            audio.replace(M3U::kQuotes, "");
            LOG(VB_RECORD, LOG_INFO, loc +
                QString("#EXT-X-STREAM-INF: attribute AUDIO=%1").arg(audio));
        }

        //   The VIDEO attribute is OPTIONAL.
        video = ParseAttributes(line, "VIDEO");
        if (!video.isEmpty())
        {
            video.replace(M3U::kQuotes, "");
            LOG(VB_RECORD, LOG_INFO, loc +
                QString("#EXT-X-STREAM-INF: attribute VIDEO=%1").arg(video));
        }


        LOG(VB_RECORD, LOG_INFO, loc +
            QString("bandwidth adaptation detected (program-id=%1, bandwidth=%2)")
            .arg(id).arg(bandwidth));

        return true;
    }

    // EXT-X-MEDIA
    //
    bool ParseMedia(const QString& line,
                    const QString& loc,
                    QString& media_type,
                    QString& group_id,
                    QString& uri,
                    QString& name)
    {
        LOG(VB_RECORD, LOG_INFO, loc + QString("Parsing EXT-X-MEDIA line"));

        media_type = ParseAttributes(line, "TYPE");
        group_id = ParseAttributes(line, "GROUP-ID");
        uri = ParseAttributes(line, "URI");
        name = ParseAttributes(line, "NAME");

        // Remove string quotes
        group_id.replace(M3U::kQuotes, "");
        uri.replace(M3U::kQuotes, "");
        name.replace(M3U::kQuotes, "");

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
        if (!ParseDecimalValue(line, duration))
        {
            LOG(VB_RECORD, LOG_ERR, loc + "expected #EXT-X-TARGETDURATION:<s>");
            return false;
        }

        return true;
    }

    bool ParseSegmentInformation(int version, const QString& line,
                                 int& duration, QString& title,
                                 const QString& loc)
    {
        /*
         * #EXTINF:<duration>,<title>
         *
         * where duration is a decimal-floating-point or decimal-integer number
         * (as described in Section 4.2) that specifies the duration of the
         * Media Segment in seconds.  Durations SHOULD be decimal-floating-
         * point, with enough accuracy to avoid perceptible error when segment
         * durations are accumulated.  However, if the compatibility version
         * number is less than 3, durations MUST be integers.  Durations that
         * are reported as integers SHOULD be rounded to the nearest integer.
         * The remainder of the line following the comma is an optional human-
         * readable informative title of the Media Segment expressed as UTF-8
         * text.
         */
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseSegmentInformation: Missing ':' in '%1'")
                .arg(line));
            return false;
        }

        QStringList list = line.mid(p + 1).split(',');

        /* read duration */
        if (list.isEmpty())
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseSegmentInformation: Missing arguments in '%1'")
                .arg(line));
            return false;
        }

        // Duration in ms
        bool ok = false;
        const QString& val = list[0];
        if (version < 3)
        {
            int duration_seconds = val.toInt(&ok);
            if (ok)
            {
                duration = duration_seconds * 1000;
            }
            else
            {
                LOG(VB_RECORD, LOG_ERR, loc +
                    QString("ParseSegmentInformation: invalid duration in '%1'")
                    .arg(line));
                return false;
            }
        }
        else
        {
            double d = val.toDouble(&ok);
            if (!ok)
            {
                LOG(VB_RECORD, LOG_ERR, loc +
                    QString("ParseSegmentInformation: invalid duration in '%1'")
                    .arg(line));
                return false;
            }
            duration = static_cast<int>(d * 1000);
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

    bool ParseKey(int version, const QString& line,
                  [[maybe_unused]] bool& aesmsg,
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
            if (!uri.isEmpty())
            {
                LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: URI not expected");
                return false;
            }
            /* IV is only supported in version 2 and above */
            if (version >= 2)
            {
                QString parsed_iv = ParseAttributes(line, "IV");
                if (!parsed_iv.isEmpty())
                {
                    LOG(VB_RECORD, LOG_ERR, loc + "#EXT-X-KEY: IV not expected");
                    return false;
                }
            }
        }
#if CONFIG_LIBCRYPTO
        else if (attr.startsWith(QLatin1String("AES-128")))
        {
            QString uri;
            if (!aesmsg)
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

            LOG(VB_RECORD, LOG_DEBUG, QString("M3U::ParseKey #EXT-X-KEY: %1").arg(line));
            LOG(VB_RECORD, LOG_DEBUG, QString("M3U::ParseKey path:%1 IV:%2").arg(path, iv));
        }
        else if (attr.startsWith(QLatin1String("SAMPLE-AES")))
        {
            LOG(VB_RECORD, LOG_ERR, loc + "encryption SAMPLE-AES not supported.");
            return false;
        }
#endif
        else
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                "invalid encryption type, only NONE "
#if CONFIG_LIBCRYPTO
                "and AES-128 are supported"
#else
                "is supported."
#endif
                );
            return false;
        }
        return true;
    }

    bool ParseMap(const QString &line,
                  const QString &loc,
                  QString &uri)
    {
        /*
         * #EXT-X-MAP:<attribute-list>
         *
         * The EXT-X-MAP tag specifies how to obtain the Media Initialization
         * Section (Section 3) required to parse the applicable Media Segments.
         * It applies to every Media Segment that appears after it in the
         * Playlist until the next EXT-X-MAP tag or until the end of the
         * Playlist.
         *
         * The following attributes are defined:
         *
         *  URI
         * The value is a quoted-string containing a URI that identifies a
         * resource that contains the Media Initialization Section.  This
         * attribute is REQUIRED.
         */
        uri = ParseAttributes(line, "URI");
        if (uri.isEmpty())
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("Attribute URI not present in: #EXT-X-MAP %1")
                .arg(line));
            return false;
        }
        return true;
    }

    bool ParseProgramDateTime(const QString& line, const QString& loc,
                              QDateTime &dt)
    {
        /*
         * #EXT-X-PROGRAM-DATE-TIME:<YYYY-MM-DDThh:mm:ssZ>
         */
        int p = line.indexOf(QLatin1String(":"));
        if (p < 0)
        {
            LOG(VB_RECORD, LOG_ERR, loc +
                QString("ParseProgramDateTime: Missing ':' in '%1'")
                .arg(line));
            return false;
        }

        QString dt_string = line.mid(p+1);
        dt = MythDate::fromString(dt_string);
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

        /* The EXT-X-ALLOW-CACHE tag was removed in protocol version 7.
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

    bool ParseDiscontinuitySequence(const QString& line, const QString& loc, int &discontinuity_sequence)
    {
        /*
         * The EXT-X-DISCONTINUITY-SEQUENCE tag allows synchronization between
         * different Renditions of the same Variant Stream or different Variant
         * Streams that have EXT-X-DISCONTINUITY tags in their Media Playlists.
         *
         * Its format is:
         *
         * #EXT-X-DISCONTINUITY-SEQUENCE:<number>
         *
         * where number is a decimal-integer
         */
        if (!ParseDecimalValue(line, discontinuity_sequence))
        {
            LOG(VB_RECORD, LOG_ERR, loc + "expected #EXT-X-DISCONTINUITY-SEQUENCE:<s>");
            return false;
        }

        LOG(VB_RECORD, LOG_DEBUG, loc + QString("#EXT-X-DISCONTINUITY-SEQUENCE %1")
            .arg(line));
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
        LOG(VB_RECORD, LOG_INFO, loc + " video on demand (vod) mode");
        return true;
    }

    bool ParseIndependentSegments(const QString& line, const QString& loc)
    {
        /* #EXT-X-INDEPENDENT-SEGMENTS
         *
         * The EXT-X-INDEPENDENT-SEGMENTS tag indicates that all media samples
         * in a Media Segment can be decoded without information from other
         * segments.  It applies to every Media Segment in the Playlist.
         *
         * Its format is:
         *
         * #EXT-X-INDEPENDENT-SEGMENTS
         */

        // Not handled yet
        LOG(VB_RECORD, LOG_DEBUG, loc + QString("#EXT-X-INDEPENDENT-SEGMENTS %1")
            .arg(line));
        return true;
    }

} // namespace M3U
