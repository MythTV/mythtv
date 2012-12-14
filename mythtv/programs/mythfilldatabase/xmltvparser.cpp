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

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

XMLTVParser::XMLTVParser() : isJapan(false), current_year(0)
{
    current_year = MythDate::current().date().toString("yyyy").toUInt();
}

static uint ELFHash(const QByteArray &ba)
{
    const uchar *k = (const uchar *)ba.data();
    uint h = 0;
    uint g;

    if (k)
    {
        while (*k)
        {
            h = (h << 4) + *k++;
            if ((g = (h & 0xf0000000)) != 0)
                h ^= g >> 24;
            h &= ~g;
        }
    }

    return h;
}

static QString getFirstText(QDomElement element)
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

    chaninfo->xmltvid = xmltvid;
    chaninfo->tvformat = "Default";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "icon")
            {
                QString path = info.attribute("src", "");
                if (!path.isEmpty() && !path.contains("://"))
                {
                    QString base = baseUrl.toString(QUrl::StripTrailingSlash);
                    chaninfo->icon = base +
                        ((path.left(1) == "/") ? path : QString("/") + path);
                }
                else if (!path.isEmpty())
                {
                    QUrl url(path);
                    if (url.isValid())
                        chaninfo->icon = url.toString();
                }
            }
            else if (info.tagName() == "display-name")
            {
                if (chaninfo->name.isEmpty())
                {
                    chaninfo->name = info.text();
                }
                else if (isJapan && chaninfo->callsign.isEmpty())
                {
                    chaninfo->callsign = info.text();
                }
                else if (chaninfo->channum.isEmpty())
                {
                    chaninfo->channum = info.text();
                }
            }
        }
    }

    chaninfo->freqid = chaninfo->channum;
    return chaninfo;
}

static int TimezoneToInt (QString timezone)
{
    // we signal an error by setting it invalid (> 840min = 14hr)
    int result = 841;

    if (timezone.toUpper() == "UTC" || timezone.toUpper() == "GMT")
        return 0;

    if (timezone.length() == 5)
    {
        bool ok;

        result = timezone.mid(1,2).toInt(&ok, 10);

        if (!ok)
            result = 841;
        else
        {
            result *= 60;

            int min = timezone.right(2).toInt(&ok, 10);

            if (!ok)
                result = 841;
            else
            {
                result += min;
                if (timezone.left(1) == "-")
                    result *= -1;
            }
        }
    }
    return result;
}

// localTimezoneOffset: 841 == "None", -841 == "Auto", other == fixed offset
static void fromXMLTVDate(QString &timestr, QDateTime &dt, int localTimezoneOffset = 841)
{
    if (timestr.isEmpty())
    {
        LOG(VB_XMLTV, LOG_ERR, "Found empty Date/Time in XMLTV data, ignoring");
        return;
    }

    QStringList split = timestr.split(" ");
    QString ts = split[0];
    bool ok;
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

    if (ts.length() == 14)
    {
        year  = ts.left(4).toInt(&ok, 10);
        month = ts.mid(4,2).toInt(&ok, 10);
        day   = ts.mid(6,2).toInt(&ok, 10);
        hour  = ts.mid(8,2).toInt(&ok, 10);
        min   = ts.mid(10,2).toInt(&ok, 10);
        sec   = ts.mid(12,2).toInt(&ok, 10);
    }
    else if (ts.length() == 12)
    {
        year  = ts.left(4).toInt(&ok, 10);
        month = ts.mid(4,2).toInt(&ok, 10);
        day   = ts.mid(6,2).toInt(&ok, 10);
        hour  = ts.mid(8,2).toInt(&ok, 10);
        min   = ts.mid(10,2).toInt(&ok, 10);
        sec   = 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Ignoring unknown timestamp format: %1")
                .arg(ts));
        return;
    }

    dt = QDateTime(QDate(year, month, day),QTime(hour, min, sec),
                   Qt::LocalTime);

    if ((split.size() > 1) && (localTimezoneOffset <= 840))
    {
        QString tmp = split[1].trimmed();

        int ts_offset = TimezoneToInt(tmp);
        if (abs(ts_offset) > 840)
        {
            ts_offset = 0;
            localTimezoneOffset = 841;
        }
        dt = dt.addSecs(-ts_offset * 60);
    }

    if (localTimezoneOffset < -840)
    {
        dt.setTimeSpec(Qt::UTC);
    }
    else if (abs(localTimezoneOffset) <= 840)
    {
        dt = dt.addSecs(localTimezoneOffset * 60 );
    }

    dt = dt.toUTC();

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
                    pginfo->videoProps |= VID_HDTV;
            }
            else if (info.tagName() == "aspect")
            {
                if (getFirstText(info) == "16:9")
                    pginfo->videoProps |= VID_WIDESCREEN;
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
                    pginfo->audioProps |= AUD_MONO;
                }
                else if (getFirstText(info) == "stereo")
                {
                    pginfo->audioProps |= AUD_STEREO;
                }
                else if (getFirstText(info) == "dolby" ||
                        getFirstText(info) == "dolby digital")
                {
                    pginfo->audioProps |= AUD_DOLBY;
                }
                else if (getFirstText(info) == "surround")
                {
                    pginfo->audioProps |= AUD_SURROUND;
                }
            }
        }
    }
}

ProgInfo *XMLTVParser::parseProgram(
    QDomElement &element, int localTimezoneOffset)
{
    QString uniqueid, season, episode;
    int dd_progid_done = 0;
    ProgInfo *pginfo = new ProgInfo();

    QString text = element.attribute("start", "");
    fromXMLTVDate(text, pginfo->starttime, localTimezoneOffset);
    pginfo->startts = text;

    text = element.attribute("stop", "");
    fromXMLTVDate(text, pginfo->endtime, localTimezoneOffset);
    pginfo->endts = text;

    text = element.attribute("channel", "");
    QStringList split = text.split(" ");

    pginfo->channel = split[0];

    text = element.attribute("clumpidx", "");
    if (!text.isEmpty())
    {
        split = text.split('/');
        pginfo->clumpidx = split[0];
        pginfo->clumpmax = split[1];
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "title")
            {
                if (isJapan)
                {
                    if (info.attribute("lang") == "ja_JP")
                    {
                        pginfo->title = getFirstText(info);
                    }
                    else if (info.attribute("lang") == "ja_JP@kana")
                    {
                        pginfo->title_pronounce = getFirstText(info);
                    }
                }
                else if (pginfo->title.isEmpty())
                {
                    pginfo->title = getFirstText(info);
                }
            }
            else if (info.tagName() == "sub-title" &&
                     pginfo->subtitle.isEmpty())
            {
                pginfo->subtitle = getFirstText(info);
            }
            else if (info.tagName() == "desc" && pginfo->description.isEmpty())
            {
                pginfo->description = getFirstText(info);
            }
            else if (info.tagName() == "category")
            {
                const QString cat = getFirstText(info).toLower();

                if (kCategoryNone == pginfo->categoryType &&
                    string_to_myth_category_type(cat) != kCategoryNone)
                {
                    pginfo->categoryType = string_to_myth_category_type(cat);
                }
                else if (pginfo->category.isEmpty())
                {
                    pginfo->category = cat;
                }

                if (cat == "film")
                {
                    // Hack for tv_grab_uk_rt
                    pginfo->categoryType = kCategoryMovie;
                }
            }
            else if (info.tagName() == "date" && !pginfo->airdate)
            {
                // Movie production year
                QString date = getFirstText(info);
                pginfo->airdate = date.left(4).toUInt();
            }
            else if (info.tagName() == "star-rating" && pginfo->stars.isEmpty())
            {
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item;
                QString stars, num, den;
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
                item = values.item(0).toElement();
                if (!item.isNull())
                {
                    stars = getFirstText(item);
                    num = stars.section('/', 0, 0);
                    den = stars.section('/', 1, 1);
                    if (0.0 < den.toFloat())
                        rating = num.toFloat()/den.toFloat();
                }

                pginfo->stars.setNum(rating);
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
                rating.system = info.attribute("system", "");
                rating.rating = getFirstText(item);
                pginfo->ratings.append(rating);
            }
            else if (info.tagName() == "previously-shown")
            {
                pginfo->previouslyshown = true;

                QString prevdate = info.attribute("start");
                if (!prevdate.isEmpty())
                {
                    QDateTime date;
                    fromXMLTVDate(prevdate, date,
                                localTimezoneOffset);
                    pginfo->originalairdate = date.date();
                }
            }
            else if (info.tagName() == "credits")
            {
                parseCredits(info, pginfo);
            }
            else if (info.tagName() == "subtitles")
            {
                if (info.attribute("type") == "teletext")
                    pginfo->subtitleType |= SUB_NORMAL;
                else if (info.attribute("type") == "onscreen")
                    pginfo->subtitleType |= SUB_ONSCREEN;
                else if (info.attribute("type") == "deaf-signed")
                    pginfo->subtitleType |= SUB_SIGNED;
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
                    pginfo->programId = episodenum;
                    dd_progid_done = 1;
                }
                else if (info.attribute("system") == "xmltv_ns")
                {
                    int tmp;
                    QString episodenum(getFirstText(info));
                    episode = episodenum.section('.',1,1);
                    episode = episode.section('/',0,0).trimmed();
                    season = episodenum.section('.',0,0).trimmed();
                    QString part(episodenum.section('.',2,2));
                    QString partnumber(part.section('/',0,0).trimmed());
                    QString parttotal(part.section('/',1,1).trimmed());

                    pginfo->categoryType = kCategorySeries;

                    if (!episode.isEmpty())
                    {
                        tmp = episode.toInt() + 1;
                        episode = QString::number(tmp);
                        pginfo->syndicatedepisodenumber = QString('E' + episode);
                    }

                    if (!season.isEmpty())
                    {
                        tmp = season.toInt() + 1;
                        season = QString::number(tmp);
                        pginfo->syndicatedepisodenumber.append(QString('S' + season));
                    }

                    uint partno = 0;
                    if (!partnumber.isEmpty())
                    {
                        bool ok;
                        partno = partnumber.toUInt(&ok) + 1;
                        partno = (ok) ? partno : 0;
                    }

                    if (!parttotal.isEmpty() && partno > 0)
                    {
                        bool ok;
                        uint partto = parttotal.toUInt(&ok);
                        if (ok && partnumber <= parttotal)
                        {
                            pginfo->parttotal  = partto;
                            pginfo->partnumber = partno;
                        }
                    }
                }
                else if (info.attribute("system") == "onscreen" &&
                        pginfo->subtitle.isEmpty())
                {
                    pginfo->categoryType = kCategorySeries;
                    pginfo->subtitle = getFirstText(info);
                }
            }
        }
    }

    if (pginfo->category.isEmpty() && pginfo->categoryType != kCategoryNone)
        pginfo->category = myth_category_type_to_string(pginfo->categoryType);

    if (!pginfo->airdate)
        pginfo->airdate = current_year;

    /* Let's build ourself a programid */
    QString programid;

    if (kCategoryMovie == pginfo->categoryType)
        programid = "MV";
    else if (kCategorySeries == pginfo->categoryType)
        programid = "EP";
    else if (kCategorySports == pginfo->categoryType)
        programid = "SP";
    else
        programid = "SH";

    if (!uniqueid.isEmpty()) // we already have a unique id ready for use
        programid.append(uniqueid);
    else
    {
        QString seriesid = QString::number(ELFHash(pginfo->title.toUtf8()));
        pginfo->seriesId = seriesid;
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
                if (kCategoryMovie != pginfo->categoryType)
                    programid.clear();
            }
            else
            {
                programid.append(episode);
                programid.append(QString::number(season_int, 36));
                if (pginfo->partnumber && pginfo->parttotal)
                {
                    programid += QString::number(pginfo->partnumber);
                    programid += QString::number(pginfo->parttotal);
                }
            }
        }
        else
        {
            /* No ep/season info? Well then remove the programid and rely on
               normal dupchecking methods instead. */
            if (kCategoryMovie != pginfo->categoryType)
                programid.clear();
        }
    }
    if (dd_progid_done == 0)
        pginfo->programId = programid;

    return pginfo;
}

bool XMLTVParser::parseFile(
    QString filename, ChannelInfoList *chanlist,
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

    // now we calculate the localTimezoneOffset, so that we can fix
    // the programdata if needed
    QString config_offset = gCoreContext->GetSetting("TimeOffset", "None");
    // we disable this feature by setting it invalid (> 840min = 14hr)
    int localTimezoneOffset = 841;

    if (config_offset == "Auto")
    {
        // we mark auto with the -ve of the disable magic number
        localTimezoneOffset = -841;
    }
    else if (config_offset != "None")
    {
        localTimezoneOffset = TimezoneToInt(config_offset);
        if (abs(localTimezoneOffset) > 840)
        {
            LOG(VB_XMLTV, LOG_ERR, QString("Ignoring invalid TimeOffset %1")
                .arg(config_offset));
            localTimezoneOffset = 841;
        }
    }

    QDomElement docElem = doc.documentElement();

    QUrl baseUrl(docElem.attribute("source-data-url", ""));

    QUrl sourceUrl(docElem.attribute("source-info-url", ""));
    if (sourceUrl.toString() == "http://labs.zap2it.com/")
    {
        LOG(VB_GENERAL, LOG_ERR, "Don't use tv_grab_na_dd, use the"
                                 "internal datadirect grabber.");
        exit(GENERIC_EXIT_SETUP_ERROR);
    }

    QString aggregatedTitle;
    QString aggregatedDesc;
    QString groupingTitle;
    QString groupingDesc;

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "channel")
            {
                ChannelInfo *chinfo = parseChannel(e, baseUrl);
                if (!chinfo->xmltvid.isEmpty())
                    chanlist->push_back(*chinfo);
                delete chinfo;
            }
            else if (e.tagName() == "programme")
            {
                ProgInfo *pginfo = parseProgram(e, localTimezoneOffset);

                if (pginfo->startts == pginfo->endts)
                {
                    /* Not a real program : just a grouping marker */
                    if (!pginfo->title.isEmpty())
                        groupingTitle = pginfo->title + " : ";

                    if (!pginfo->description.isEmpty())
                        groupingDesc = pginfo->description + " : ";
                }
                else
                {
                    if (pginfo->clumpidx.isEmpty())
                    {
                        if (!groupingTitle.isEmpty())
                        {
                            pginfo->title.prepend(groupingTitle);
                            groupingTitle.clear();
                        }

                        if (!groupingDesc.isEmpty())
                        {
                            pginfo->description.prepend(groupingDesc);
                            groupingDesc.clear();
                        }

                        (*proglist)[pginfo->channel].push_back(*pginfo);
                    }
                    else
                    {
                        /* append all titles/descriptions from one clump */
                        if (pginfo->clumpidx.toInt() == 0)
                        {
                            aggregatedTitle.clear();
                            aggregatedDesc.clear();
                        }

                        if (!pginfo->title.isEmpty())
                        {
                            if (!aggregatedTitle.isEmpty())
                                aggregatedTitle.append(" | ");
                            aggregatedTitle.append(pginfo->title);
                        }

                        if (!pginfo->description.isEmpty())
                        {
                            if (!aggregatedDesc.isEmpty())
                                aggregatedDesc.append(" | ");
                            aggregatedDesc.append(pginfo->description);
                        }
                        if (pginfo->clumpidx.toInt() ==
                            pginfo->clumpmax.toInt() - 1)
                        {
                            pginfo->title = aggregatedTitle;
                            pginfo->description = aggregatedDesc;
                            (*proglist)[pginfo->channel].push_back(*pginfo);
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

