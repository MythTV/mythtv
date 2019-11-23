#include "xmltvparser.h"

// Qt headers
#include <QFile>
#include <QStringList>
#include <QDateTime>
#include <QDomDocument>
#include <QUrl>

// C++ headers
#include <iostream>
#include <cstdlib>

// libmyth headers
#include "exitcodes.h"
#include "mythcorecontext.h"
#include "mythdate.h"

// libmythtv headers
#include "programinfo.h"
#include "programdata.h"
#include "dvbdescriptors.h"
#include "channelinfo.h"

// libmythmetadata headers
#include "metadatadownload.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

XMLTVParser::XMLTVParser()
{
    m_current_year = MythDate::current().date().toString("yyyy").toUInt();
}

void XMLTVParser::lateInit()
{
    m_movieGrabberPath = MetadataDownload::GetMovieGrabber();
    m_tvGrabberPath = MetadataDownload::GetTelevisionGrabber();
}

static uint ELFHash(const QByteArray &ba)
{
    const uchar *k = (const uchar *)ba.data();
    uint h = 0;

    if (k)
    {
        while (*k)
        {
            uint g = 0;
            h = (h << 4) + *k++;
            if ((g = (h & 0xf0000000)) != 0)
                h ^= g >> 24;
            h &= ~g;
        }
    }

    return h;
}

static QString getFirstText(const QDomElement& element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return QString();
}

ChannelInfo *XMLTVParser::parseChannel(QDomElement &element, QUrl &baseUrl)
{
    ChannelInfo *chaninfo = new ChannelInfo;

    QString xmltvid = element.attribute("id", "");

    chaninfo->m_xmltvid = xmltvid;
    chaninfo->m_tvformat = "Default";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "icon")
            {
                if (chaninfo->m_icon.isEmpty())
                {
                    QString path = info.attribute("src", "");
                    if (!path.isEmpty() && !path.contains("://"))
                    {
                        QString base = baseUrl.toString(QUrl::StripTrailingSlash);
                        chaninfo->m_icon = base +
                            ((path.startsWith("/")) ? path : QString("/") + path);
                    }
                    else if (!path.isEmpty())
                    {
                        QUrl url(path);
                        if (url.isValid())
                            chaninfo->m_icon = url.toString();
                    }
                }
            }
            else if (info.tagName() == "display-name")
            {
                if (chaninfo->m_name.isEmpty())
                {
                    chaninfo->m_name = info.text();
                }
                else if (chaninfo->m_callsign.isEmpty())
                {
                    chaninfo->m_callsign = info.text();
                }
                else if (chaninfo->m_channum.isEmpty())
                {
                    chaninfo->m_channum = info.text();
                }
            }
        }
    }

    chaninfo->m_freqid = chaninfo->m_channum;
    return chaninfo;
}

static void fromXMLTVDate(QString &timestr, QDateTime &dt)
{
    // The XMLTV spec requires dates to either be in UTC/GMT or to specify a
    // valid timezone. We are sticking to the spec and require all grabbers
    // to comply.

    if (timestr.isEmpty())
    {
        LOG(VB_XMLTV, LOG_ERR, "Found empty Date/Time in XMLTV data, ignoring");
        return;
    }

    QStringList split = timestr.split(" ", QString::SkipEmptyParts);
    QString ts = split[0];
    QDate tmpDate;
    QTime tmpTime;
    QString tzoffset;

    // Process the TZ offset (if any)
    if (split.size() > 1)
    {
        tzoffset = split[1];
        // These shouldn't be required and they aren't ISO 8601 but the
        // xmltv spec mentions these and just these so handle them just in
        // case
        if (tzoffset == "GMT" || tzoffset == "UTC")
            tzoffset = "+0000";
        else if (tzoffset == "BST")
            tzoffset = "+0100";
    }
    else
    {
        // We will accept a datetime with a trailing Z as being explicit
        if (ts.endsWith('Z'))
        {
            tzoffset = "+0000";
            ts.truncate(ts.length()-1);
        }
        else
        {
            tzoffset = "+0000";
            static bool s_warnedOnceOnImplicitUtc = false;
            if (!s_warnedOnceOnImplicitUtc)
            {
                LOG(VB_XMLTV, LOG_WARNING, "No explicit time zone found, "
                    "guessing implicit UTC! Please consider enhancing "
                    "the guide source to provide explicit UTC or local "
                    "time instead.");
                s_warnedOnceOnImplicitUtc = true;
            }
        }
    }

    // Process the date part
    QString tsDate = ts.left(8);
    if (tsDate.length() == 8)
        tmpDate = QDate::fromString(tsDate, "yyyyMMdd");
    else if (tsDate.length() == 6)
        tmpDate = QDate::fromString(tsDate, "yyyyMM");
    else if (tsDate.length() == 4)
        tmpDate = QDate::fromString(tsDate, "yyyy");
    if (!tmpDate.isValid())
    {
        LOG(VB_XMLTV, LOG_ERR,
            QString("Invalid datetime (date) in XMLTV data, ignoring: %1")
                .arg(timestr));
        return;
    }

    // Process the time part (if any)
    if (ts.length() > 8)
    {
        QString tsTime = ts.mid(8);
        if (tsTime.length() == 6)
        {
            if (tsTime == "235960")
                tsTime = "235959";
            tmpTime = QTime::fromString(tsTime, "HHmmss");
        }
        else if (tsTime.length() == 4)
            tmpTime = QTime::fromString(tsTime, "HHmm");
        else if (tsTime.length() == 2)
            tmpTime = QTime::fromString(tsTime, "HH");
        if (!tmpTime.isValid())
        {
            // Time part exists, but is (somehow) invalid
            LOG(VB_XMLTV, LOG_ERR,
                QString("Invalid datetime (time) in XMLTV data, ignoring: %1")
                    .arg(timestr));
            return;
        }
    }

    QDateTime tmpDT = QDateTime(tmpDate, tmpTime, Qt::UTC);
    if (!tmpDT.isValid())
        {
            LOG(VB_XMLTV, LOG_ERR,
                QString("Invalid datetime (combination of date/time) "
                    "in XMLTV data, ignoring: %1").arg(timestr));
            return;
        }

    // While this seems like a hack, it's better than what was done before
    QString isoDateString = tmpDT.toString(Qt::ISODate);
    if (isoDateString.endsWith('Z'))    // Should always be Z, but ...
        isoDateString.truncate(isoDateString.length()-1);
    isoDateString += tzoffset;
    dt = QDateTime::fromString(isoDateString, Qt::ISODate).toUTC();

    if (!dt.isValid())
    {
        LOG(VB_XMLTV, LOG_ERR,
            QString("Invalid datetime (zone offset) in XMLTV data, "
                "ignoring: %1").arg(timestr));
        return;
    }

    timestr = MythDate::toString(dt, MythDate::kFilename);
}

static void parseCredits(QDomElement &element, ProgInfo *pginfo)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
            pginfo->AddPerson(info.tagName(), getFirstText(info));
    }
}

static void parseVideo(QDomElement &element, ProgInfo *pginfo)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "quality")
            {
                if (getFirstText(info) == "HDTV")
                    pginfo->m_videoProps |= VID_HDTV;
            }
            else if (info.tagName() == "aspect")
            {
                if (getFirstText(info) == "16:9")
                    pginfo->m_videoProps |= VID_WIDESCREEN;
            }
        }
    }
}

static void parseAudio(QDomElement &element, ProgInfo *pginfo)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "stereo")
            {
                if (getFirstText(info) == "mono")
                {
                    pginfo->m_audioProps |= AUD_MONO;
                }
                else if (getFirstText(info) == "stereo")
                {
                    pginfo->m_audioProps |= AUD_STEREO;
                }
                else if (getFirstText(info) == "dolby" ||
                        getFirstText(info) == "dolby digital")
                {
                    pginfo->m_audioProps |= AUD_DOLBY;
                }
                else if (getFirstText(info) == "surround")
                {
                    pginfo->m_audioProps |= AUD_SURROUND;
                }
            }
        }
    }
}

ProgInfo *XMLTVParser::parseProgram(QDomElement &element)
{
    QString programid, season, episode, totalepisodes;
    ProgInfo *pginfo = new ProgInfo();

    QString text = element.attribute("start", "");
    fromXMLTVDate(text, pginfo->m_starttime);
    pginfo->m_startts = text;

    text = element.attribute("stop", "");
    fromXMLTVDate(text, pginfo->m_endtime);
    pginfo->m_endts = text;

    text = element.attribute("channel", "");
    QStringList split = text.split(" ");

    pginfo->m_channel = split[0];

    text = element.attribute("clumpidx", "");
    if (!text.isEmpty())
    {
        split = text.split('/');
        pginfo->m_clumpidx = split[0];
        pginfo->m_clumpmax = split[1];
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "title")
            {
                if (info.attribute("lang") == "ja_JP")
                {   // NOLINT(bugprone-branch-clone)
                    pginfo->m_title = getFirstText(info);
                }
                else if (info.attribute("lang") == "ja_JP@kana")
                {
                    pginfo->m_title_pronounce = getFirstText(info);
                }
                else if (pginfo->m_title.isEmpty())
                {
                    pginfo->m_title = getFirstText(info);
                }
            }
            else if (info.tagName() == "sub-title" &&
                     pginfo->m_subtitle.isEmpty())
            {
                pginfo->m_subtitle = getFirstText(info);
            }
            else if (info.tagName() == "desc" && pginfo->m_description.isEmpty())
            {
                pginfo->m_description = getFirstText(info);
            }
            else if (info.tagName() == "category")
            {
                const QString cat = getFirstText(info);

                if (ProgramInfo::kCategoryNone == pginfo->m_categoryType &&
                    string_to_myth_category_type(cat) != ProgramInfo::kCategoryNone)
                {
                    pginfo->m_categoryType = string_to_myth_category_type(cat);
                }
                else if (pginfo->m_category.isEmpty())
                {
                    pginfo->m_category = cat;
                }

                if ((cat.compare(QObject::tr("movie"),Qt::CaseInsensitive) == 0) ||
                    (cat.compare(QObject::tr("film"),Qt::CaseInsensitive) == 0))
                {
                    // Hack for tv_grab_uk_rt
                    pginfo->m_categoryType = ProgramInfo::kCategoryMovie;
                }

                pginfo->m_genres.append(cat);
            }
            else if (info.tagName() == "date" && (pginfo->m_airdate == 0U))
            {
                // Movie production year
                QString date = getFirstText(info);
                pginfo->m_airdate = date.left(4).toUInt();
            }
            else if (info.tagName() == "star-rating" && pginfo->m_stars == 0.0F)
            {
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item;
                QString stars;
                float rating = 0.0;

                // Use the first rating to appear in the xml, this should be
                // the most important one.
                //
                // Averaging is not a good idea here, any subsequent ratings
                // are likely to represent that days recommended programmes
                // which on a bad night could given to an average programme.
                // In the case of uk_rt it's not unknown for a recommendation
                // to be given to programmes which are 'so bad, you have to
                // watch!'
                //
                // XMLTV uses zero based ratings and signals no rating by absence.
                // A rating from 1 to 5 is encoded as 0/4 to 4/4.
                // MythTV uses zero to signal no rating!
                // The same rating is encoded as 0.2 to 1.0 with steps of 0.2, it
                // is not encoded as 0.0 to 1.0 with steps of 0.25 because
                // 0 signals no rating!
                // See http://xmltv.cvs.sourceforge.net/viewvc/xmltv/xmltv/xmltv.dtd?revision=1.47&view=markup#l539
                item = values.item(0).toElement();
                if (!item.isNull())
                {
                    stars = getFirstText(item);
                    float num = stars.section('/', 0, 0).toFloat() + 1;
                    float den = stars.section('/', 1, 1).toFloat() + 1;
                    if (0.0F < den)
                        rating = num/den;
                }

                pginfo->m_stars = rating;
            }
            else if (info.tagName() == "rating")
            {
                // again, the structure of ratings seems poorly represented
                // in the XML.  no idea what we'd do with multiple values.
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item = values.item(0).toElement();
                if (item.isNull())
                    continue;
                EventRating rating;
                rating.m_system = info.attribute("system", "");
                rating.m_rating = getFirstText(item);
                pginfo->m_ratings.append(rating);
            }
            else if (info.tagName() == "previously-shown")
            {
                pginfo->m_previouslyshown = true;

                QString prevdate = info.attribute("start");
                if (!prevdate.isEmpty())
                {
                    QDateTime date;
                    fromXMLTVDate(prevdate, date);
                    pginfo->m_originalairdate = date.date();
                }
            }
            else if (info.tagName() == "credits")
            {
                parseCredits(info, pginfo);
            }
            else if (info.tagName() == "subtitles")
            {
                if (info.attribute("type") == "teletext")
                    pginfo->m_subtitleType |= SUB_NORMAL;
                else if (info.attribute("type") == "onscreen")
                    pginfo->m_subtitleType |= SUB_ONSCREEN;
                else if (info.attribute("type") == "deaf-signed")
                    pginfo->m_subtitleType |= SUB_SIGNED;
            }
            else if (info.tagName() == "audio")
            {
                parseAudio(info, pginfo);
            }
            else if (info.tagName() == "video")
            {
                parseVideo(info, pginfo);
            }
            else if (info.tagName() == "episode-num")
            {
                if (info.attribute("system") == "dd_progid")
                {
                    QString episodenum(getFirstText(info));
                    // if this field includes a dot, strip it out
                    int idx = episodenum.indexOf('.');
                    if (idx != -1)
                        episodenum.remove(idx, 1);
                    programid = episodenum;
                    /* Only EPisodes and SHows are part of a series for SD */
                    if (programid.startsWith(QString("EP")) ||
                        programid.startsWith(QString("SH")))
                        pginfo->m_seriesId = QString("EP") + programid.mid(2,8);
                }
                else if (info.attribute("system") == "xmltv_ns")
                {
                    QString episodenum(getFirstText(info));
                    episode = episodenum.section('.',1,1);
                    totalepisodes = episode.section('/',1,1).trimmed();
                    episode = episode.section('/',0,0).trimmed();
                    season = episodenum.section('.',0,0).trimmed();
                    season = season.section('/',0,0).trimmed();
                    QString part(episodenum.section('.',2,2));
                    QString partnumber(part.section('/',0,0).trimmed());
                    QString parttotal(part.section('/',1,1).trimmed());

                    pginfo->m_categoryType = ProgramInfo::kCategorySeries;

                    if (!season.isEmpty())
                    {
                        int tmp = season.toUInt() + 1;
                        pginfo->m_season = tmp;
                        season = QString::number(tmp);
                        pginfo->m_syndicatedepisodenumber = 'S' + season;
                    }

                    if (!episode.isEmpty())
                    {
                        int tmp = episode.toUInt() + 1;
                        pginfo->m_episode = tmp;
                        episode = QString::number(tmp);
                        pginfo->m_syndicatedepisodenumber.append('E' + episode);
                    }

                    if (!totalepisodes.isEmpty())
                    {
                        pginfo->m_totalepisodes = totalepisodes.toUInt();
                    }

                    uint partno = 0;
                    if (!partnumber.isEmpty())
                    {
                        bool ok = false;
                        partno = partnumber.toUInt(&ok) + 1;
                        partno = (ok) ? partno : 0;
                    }

                    if (!parttotal.isEmpty() && partno > 0)
                    {
                        bool ok = false;
                        uint partto = parttotal.toUInt(&ok);
                        if (ok && partnumber <= parttotal)
                        {
                            pginfo->m_parttotal  = partto;
                            pginfo->m_partnumber = partno;
                        }
                    }
                }
                else if (info.attribute("system") == "onscreen")
                {
                    pginfo->m_categoryType = ProgramInfo::kCategorySeries;
                    if (pginfo->m_subtitle.isEmpty())
                    {
                        pginfo->m_subtitle = getFirstText(info);
                    }
                }
                else if ((info.attribute("system") == "themoviedb.org") &&
                    (m_movieGrabberPath.endsWith(QString("/tmdb3.py"))))
                {
                    /* text is movie/<inetref> */
                    QString inetrefRaw(getFirstText(info));
                    if (inetrefRaw.startsWith(QString("movie/"))) {
                        QString inetref(QString ("tmdb3.py_") + inetrefRaw.section('/',1,1).trimmed());
                        pginfo->m_inetref = inetref;
                    }
                }
                else if ((info.attribute("system") == "thetvdb.com") &&
                    (m_tvGrabberPath.endsWith(QString("/ttvdb.py"))))
                {
                    /* text is series/<inetref> */
                    QString inetrefRaw(getFirstText(info));
                    if (inetrefRaw.startsWith(QString("series/"))) {
                        QString inetref(QString ("ttvdb.py_") + inetrefRaw.section('/',1,1).trimmed());
                        pginfo->m_inetref = inetref;
                        /* ProgInfo does not have a collectionref, so we don't set any */
                    }
                }
            }
        }
    }

    if (pginfo->m_category.isEmpty() &&
        pginfo->m_categoryType != ProgramInfo::kCategoryNone)
        pginfo->m_category = myth_category_type_to_string(pginfo->m_categoryType);

    if (!pginfo->m_airdate
        && ProgramInfo::kCategorySeries != pginfo->m_categoryType)
        pginfo->m_airdate = m_current_year;

    if (programid.isEmpty())
    {

        /* Let's build ourself a programid */

        if (ProgramInfo::kCategoryMovie == pginfo->m_categoryType)
            programid = "MV";
        else if (ProgramInfo::kCategorySeries == pginfo->m_categoryType)
            programid = "EP";
        else if (ProgramInfo::kCategorySports == pginfo->m_categoryType)
            programid = "SP";
        else
            programid = "SH";

        QString seriesid = QString::number(ELFHash(pginfo->m_title.toUtf8()));
        pginfo->m_seriesId = seriesid;
        programid.append(seriesid);

        if (!episode.isEmpty() && !season.isEmpty())
        {
            /* Append unpadded episode and season number to the seriesid (to
               maintain consistency with historical encoding), but limit the
               season number representation to a single base-36 character to
               ensure unique programid generation. */
            int season_int = season.toInt();
            if (season_int > 35)
            {
                // Cannot represent season as a single base-36 character, so
                // remove the programid and fall back to normal dup matching.
                if (ProgramInfo::kCategoryMovie != pginfo->m_categoryType)
                    programid.clear();
            }
            else
            {
                programid.append(episode);
                programid.append(QString::number(season_int, 36));
                if (pginfo->m_partnumber && pginfo->m_parttotal)
                {
                    programid += QString::number(pginfo->m_partnumber);
                    programid += QString::number(pginfo->m_parttotal);
                }
            }
        }
        else
        {
            /* No ep/season info? Well then remove the programid and rely on
               normal dupchecking methods instead. */
            if (ProgramInfo::kCategoryMovie != pginfo->m_categoryType)
                programid.clear();
        }
    }

    pginfo->m_programId = programid;

    return pginfo;
}

bool XMLTVParser::parseFile(
    const QString& filename, ChannelInfoList *chanlist,
    QMap<QString, QList<ProgInfo> > *proglist)
{
    QDomDocument doc;
    QFile f;

    if (!dash_open(f, filename, QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error unable to open '%1' for reading.") .arg(filename));
        return false;
    }

    QString errorMsg = "unknown";
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error in %1:%2: %3")
            .arg(errorLine).arg(errorColumn).arg(errorMsg));

        f.close();
        return true;
    }

    f.close();

    QDomElement docElem = doc.documentElement();

    QUrl baseUrl(docElem.attribute("source-data-url", ""));
    //QUrl sourceUrl(docElem.attribute("source-info-url", ""));

    QString aggregatedTitle;
    QString aggregatedDesc;

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "channel")
            {
                ChannelInfo *chinfo = parseChannel(e, baseUrl);
                if (!chinfo->m_xmltvid.isEmpty())
                    chanlist->push_back(*chinfo);
                delete chinfo;
            }
            else if (e.tagName() == "programme")
            {
                ProgInfo *pginfo = parseProgram(e);

                if (!(pginfo->m_starttime.isValid()))
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Invalid programme (%1), "
                                                        "invalid start time, "
                                                        "skipping")
                                                        .arg(pginfo->m_title));
                }
                else if (pginfo->m_channel.isEmpty())
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Invalid programme (%1), "
                                                        "missing channel, "
                                                        "skipping")
                                                        .arg(pginfo->m_title));
                }
                else if (pginfo->m_startts == pginfo->m_endts)
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Invalid programme (%1), "
                                                        "identical start and end "
                                                        "times, skipping")
                                                        .arg(pginfo->m_title));
                }
                else
                {
                    if (pginfo->m_clumpidx.isEmpty())
                        (*proglist)[pginfo->m_channel].push_back(*pginfo);
                    else
                    {
                        /* append all titles/descriptions from one clump */
                        if (pginfo->m_clumpidx.toInt() == 0)
                        {
                            aggregatedTitle.clear();
                            aggregatedDesc.clear();
                        }

                        if (!pginfo->m_title.isEmpty())
                        {
                            if (!aggregatedTitle.isEmpty())
                                aggregatedTitle.append(" | ");
                            aggregatedTitle.append(pginfo->m_title);
                        }

                        if (!pginfo->m_description.isEmpty())
                        {
                            if (!aggregatedDesc.isEmpty())
                                aggregatedDesc.append(" | ");
                            aggregatedDesc.append(pginfo->m_description);
                        }
                        if (pginfo->m_clumpidx.toInt() ==
                            pginfo->m_clumpmax.toInt() - 1)
                        {
                            pginfo->m_title = aggregatedTitle;
                            pginfo->m_description = aggregatedDesc;
                            (*proglist)[pginfo->m_channel].push_back(*pginfo);
                        }
                    }
                }
                delete pginfo;
            }
        }
        n = n.nextSibling();
    }

    return true;
}
