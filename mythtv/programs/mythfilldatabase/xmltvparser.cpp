// Qt headers
#include <qfile.h>
#include <qstringlist.h>
#include <qdatetime.h>

// C++ headers
#include <iostream>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "util.h"

// libmythtv headers
#include "programdata.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"
#include "xmltvparser.h"

unsigned int ELFHash(const char *s)
{
    /* ELF hash uses unsigned chars and unsigned arithmetic for portability */
    const unsigned char *name = (const unsigned char *)s;
    unsigned long h = 0, g;

    while (*name)
    { /* do some fancy bitwanking on the string */
        h = (h << 4) + (unsigned long)(*name++);
        if ((g = (h & 0xF0000000UL))!=0)
            h ^= (g >> 24);
        h &= ~g;

    }

    return (int)h;
}

QString getFirstText(QDomElement element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

ChanInfo *XMLTVParser::parseChannel(QDomElement &element, QUrl baseUrl)
{
    ChanInfo *chaninfo = new ChanInfo;

    QString xmltvid = element.attribute("id", "");
    QStringList split = QStringList::split(" ", xmltvid);

    chaninfo->callsign = "";
    chaninfo->chanstr = "";
    chaninfo->xmltvid = xmltvid;

    chaninfo->iconpath = "";
    chaninfo->name = "";
    chaninfo->finetune = "";
    chaninfo->tvformat = "Default";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "icon")
            {
                QUrl iconUrl(baseUrl, info.attribute("src", ""), true);
                chaninfo->iconpath = iconUrl.toString();
            }
            else if (info.tagName() == "display-name")
            {
                if (chaninfo->name.length() == 0)
                {
                    chaninfo->name = info.text();
                }
                else if (isJapan && chaninfo->callsign.length() == 0)
                {
                    chaninfo->callsign = info.text();
                }
                else if (chaninfo->chanstr.length() == 0)
                {
                    chaninfo->chanstr = info.text();
                }
            }
        }
    }

    chaninfo->freqid = chaninfo->chanstr;
    return chaninfo;
}

int TimezoneToInt (QString timezone)
{
    // we signal an error by setting it invalid (> 840min = 14hr)
    int result = 841;
    
    if (timezone.upper() == "UTC" || timezone.upper() == "GMT")
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
void fromXMLTVDate(QString &timestr, QDateTime &dt, int localTimezoneOffset = 841)
{
    if (timestr.isEmpty())
    {
        cerr << "Ignoring empty timestamp." << endl;
        return;
    }

    QStringList split = QStringList::split(" ", timestr);
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
        cerr << "Ignoring unknown timestamp format: " << ts << endl;
        return;
    }

    dt = QDateTime(QDate(year, month, day),QTime(hour, min, sec));

    if ((split.size() > 1) && (localTimezoneOffset <= 840))
    {
        QString tmp = split[1];
        tmp.stripWhiteSpace();

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
        dt = MythUTCToLocal(dt);
    }
    else if (abs(localTimezoneOffset) <= 840)
    {
        dt = dt.addSecs(localTimezoneOffset * 60 );
    }

    timestr = dt.toString("yyyyMMddhhmmss");
}

void parseCredits(QDomElement &element, ProgInfo *pginfo)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            ProgCredit credit;
            credit.role = info.tagName();
            credit.name = getFirstText(info);
            pginfo->credits.append(credit);
        }
    }
}

void parseVideo(QDomElement &element, ProgInfo *pginfo)
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
                        pginfo->hdtv = true;
            }
        }
    }
}

void parseAudio(QDomElement &element, ProgInfo *pginfo)
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
                    pginfo->stereo = false;
                }
                else if (getFirstText(info) == "stereo" ||
                        getFirstText(info) == "dolby" ||
                        getFirstText(info) == "dolby digital" ||
                        getFirstText(info) == "surround")
                {
                    pginfo->stereo = true;
                }
            }
        }
    }
}

ProgInfo *XMLTVParser::parseProgram(
    QDomElement &element, int localTimezoneOffset)
{
    QString uniqueid, seriesid, season, episode;
    ProgInfo *pginfo = new ProgInfo;
 
    pginfo->previouslyshown = pginfo->stereo = pginfo->subtitled =
    pginfo->hdtv = pginfo->closecaptioned = false;

    pginfo->subtitle = pginfo->title = pginfo->desc =
    pginfo->category = pginfo->content = pginfo->catType =
    pginfo->syndicatedepisodenumber =  pginfo->partnumber =
    pginfo->parttotal = pginfo->showtype = pginfo->colorcode =
    pginfo->stars = "";

    QString text = element.attribute("start", "");
    fromXMLTVDate(text, pginfo->start, localTimezoneOffset);
    pginfo->startts = text;

    text = element.attribute("stop", "");
    fromXMLTVDate(text, pginfo->end, localTimezoneOffset);
    pginfo->endts = text;

    text = element.attribute("channel", "");
    QStringList split = QStringList::split(" ", text);   
 
    pginfo->channel = split[0];

    text = element.attribute("clumpidx", "");
    if (!text.isEmpty()) 
    {
        split = QStringList::split("/", text);
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
                else if (pginfo->title == "")
                {
                    pginfo->title = getFirstText(info);
                }
            }
            else if (info.tagName() == "sub-title" && pginfo->subtitle == "")
            {
                pginfo->subtitle = getFirstText(info);
            }
            else if (info.tagName() == "content")
            {
                pginfo->content = getFirstText(info);
            }
            else if (info.tagName() == "desc" && pginfo->desc == "")
            {
                pginfo->desc = getFirstText(info);
            }
            else if (info.tagName() == "category")
            {
                QString cat = getFirstText(info);

                if (cat == "movie" || cat == "series" || 
                    cat == "sports" || cat == "tvshow")
                {
                    if (pginfo->catType.isEmpty())
                        pginfo->catType = cat;
                }
                else if (pginfo->category.isEmpty())
                {
                    pginfo->category = cat;
                }

//                 if ((cat == "Film" || cat == "film") && !isNorthAmerica)
//                 {
//                     // Hack for tv_grab_uk_rt
//                     pginfo->catType = "movie";
//                 }
            }
            else if (info.tagName() == "date" && pginfo->airdate == "")
            {
                // Movie production year
                QString date = getFirstText(info);
                pginfo->airdate = date.left(4);
            }
            else if (info.tagName() == "star-rating")
            {
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item;
                QString stars, num, den;
                float avg = 0.0;
                // not sure why the XML suggests multiple ratings,
                // but the following will average them anyway.
                for (unsigned int i = 0; i < values.length(); i++)
                {
                    item = values.item(i).toElement();
                    if (item.isNull())
                        continue;
                    stars = getFirstText(item);
                    num = stars.section('/', 0, 0);
                    den = stars.section('/', 1, 1);
                    if (0.0 >= den.toFloat())
                        continue;
                    avg *= i/(i+1);
                    avg += (num.toFloat()/den.toFloat()) / (i+1);
                }
                pginfo->stars.setNum(avg);
            }
            else if (info.tagName() == "rating")
            {
                // again, the structure of ratings seems poorly represented
                // in the XML.  no idea what we'd do with multiple values.
                QDomNodeList values = info.elementsByTagName("value");
                QDomElement item = values.item(0).toElement();
                if (item.isNull())
                    continue;
                ProgRating rating;
                rating.system = info.attribute("system", "");
                rating.rating = getFirstText(item);
                if ("" != rating.system)
                    pginfo->ratings.append(rating);
            }
            else if (info.tagName() == "previously-shown")
            {
                pginfo->previouslyshown = true;

                QString prevdate = getFirstText(info);
                pginfo->originalairdate = prevdate;
            } 
            else if (info.tagName() == "credits")
            {
                parseCredits(info, pginfo);
            }
            else if (info.tagName() == "subtitles" && info.attribute("type") == "teletext")
            {
                pginfo->closecaptioned = true;
            }
            else if (info.tagName() == "subtitles" && info.attribute("type") == "onscreen")
            {
                pginfo->subtitled = true;
            }
            else if (info.tagName() == "audio")
            {
                parseAudio(info, pginfo);
            }
            else if (info.tagName() == "video")
            {
                parseVideo(info, pginfo);
            }
            else if (info.tagName() == "episode-num" &&
                     info.attribute("system") == "xmltv_ns")
            {
                int tmp;
                QString episodenum(getFirstText(info));
                episode = episodenum.section('.',1,1);
                episode = episode.section('/',0,0).stripWhiteSpace();
                season = episodenum.section('.',0,0).stripWhiteSpace();
                QString part(episodenum.section('.',2,2));
                QString partnumber(part.section('/',0,0).stripWhiteSpace());
                QString parttotal(part.section('/',1,1).stripWhiteSpace());

                pginfo->catType = "series";

                if (!episode.isEmpty())
                {
                    tmp = episode.toInt() + 1;
                    episode = QString::number(tmp);
                    pginfo->syndicatedepisodenumber = QString("E" + episode);
                }

                if (!season.isEmpty())
                {
                    tmp = season.toInt() + 1;
                    season = QString::number(tmp);
                    pginfo->syndicatedepisodenumber.append(QString("S" + season));
                }

                if (!partnumber.isEmpty())
                {                
                    tmp = partnumber.toInt() + 1;
                    partnumber = QString::number(tmp);
                }
                
                if (partnumber != 0 && parttotal >= partnumber && !parttotal.isEmpty())
                {
                    pginfo->parttotal = parttotal;
                    pginfo->partnumber = partnumber;
                }
            }
            else if (info.tagName() == "episode-num" &&
                     info.attribute("system") == "onscreen" &&
                     pginfo->subtitle.isEmpty())
            {
                 pginfo->catType = "series";
                 pginfo->subtitle = getFirstText(info);
            }
        }
    }

    if (pginfo->category.isEmpty() && !pginfo->catType.isEmpty())
        pginfo->category = pginfo->catType;
    
    /* Hack for teveblad grabber to do something with the content tag*/
    if (pginfo->content != "")
    {
        if (pginfo->category == "film")
        {
            pginfo->subtitle = pginfo->desc;
            pginfo->desc = pginfo->content;
        }
        else if (pginfo->desc != "") 
        {
            pginfo->desc = pginfo->desc + " - " + pginfo->content;
        }
        else if (pginfo->desc == "")
        {
            pginfo->desc = pginfo->content;
        }
    }
    
    if (pginfo->airdate.isEmpty())
        pginfo->airdate = QDate::currentDate().toString("yyyy");

    /* Let's build ourself a programid */
    QString programid;
    
    if (pginfo->catType == "movie")
        programid = "MV";
    else if (pginfo->catType == "series")
        programid = "EP";
    else if (pginfo->catType == "sports")
        programid = "SP";
    else
        programid = "SH";
    
    if (!uniqueid.isEmpty()) // we already have a unique id ready for use
        programid.append(uniqueid);
    else
    {
        if (seriesid.isEmpty()) //need to hash ourself a seriesid from the title
        {
            seriesid = QString::number(ELFHash(pginfo->title));
        }
        pginfo->seriesid = seriesid;
        programid.append(seriesid);

        if (!episode.isEmpty() && !season.isEmpty())
        {
            programid.append(episode);
            programid.append(season);
            if (!pginfo->partnumber.isEmpty() && !pginfo->parttotal.isEmpty())
            {
                programid.append(pginfo->partnumber);
                programid.append(pginfo->parttotal);
            }
        }
        else
        {
            /* No ep/season info? Well then remove the programid and rely on
               normal dupchecking methods instead. */
            if (pginfo->catType != "movie")
                programid = "";
        }
    }
    
    pginfo->programid = programid;

    return pginfo;
}
                  
bool XMLTVParser::parseFile(
    QString filename, QValueList<ChanInfo> *chanlist,
    QMap<QString, QValueList<ProgInfo> > *proglist)
{
    QDomDocument doc;
    QFile f;

    if (!dash_open(f, filename, IO_ReadOnly))
    {
        cerr << QString("Error unable to open '%1' for reading.")
                .arg(filename)
             << endl;
        return false;
    }

    QString errorMsg = "unknown";
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "Error in " << errorLine << ":" << errorColumn << ": "
             << errorMsg << endl;

        f.close();
        return true;
    }

    f.close();

    // now we calculate the localTimezoneOffset, so that we can fix
    // the programdata if needed
    QString config_offset = gContext->GetSetting("TimeOffset", "None");
    // we disable this feature by setting it invalid (> 840min = 14hr)
    int localTimezoneOffset = 841;

    if (config_offset == "Auto")
    {
        localTimezoneOffset = -841; // we mark auto with the -ve of the disable magic number
    }
    else if (config_offset != "None")
    {
        localTimezoneOffset = TimezoneToInt(config_offset);
        if (abs(localTimezoneOffset) > 840)
        {
            cerr << "Ignoring invalid TimeOffset " << config_offset << endl;
            localTimezoneOffset = 841;
        }
    }

    QDomElement docElem = doc.documentElement();

    QUrl baseUrl(docElem.attribute("source-data-url", ""));

    QUrl sourceUrl(docElem.attribute("source-info-url", ""));
    if (sourceUrl.toString() == "http://labs.zap2it.com/")
    {
        cerr << "Don't use tv_grab_na_dd, use the internal datadirect grabber."
             << endl;
        exit(FILLDB_BUGGY_EXIT_SRC_IS_DD);
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
                ChanInfo *chinfo = parseChannel(e, baseUrl);
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

                    if (!pginfo->desc.isEmpty())
                        groupingDesc = pginfo->desc + " : ";
                }
                else
                {
                    if (pginfo->clumpidx.isEmpty())
                    {
                        if (!groupingTitle.isEmpty())
                        {
                            pginfo->title.prepend(groupingTitle);
                            groupingTitle = "";
                        }

                        if (!groupingDesc.isEmpty())
                        {
                            pginfo->desc.prepend(groupingDesc);
                            groupingDesc = "";
                        }

                        (*proglist)[pginfo->channel].push_back(*pginfo);
                    }
                    else
                    {
                        /* append all titles/descriptions from one clump */
                        if (pginfo->clumpidx.toInt() == 0)
                        {
                            aggregatedTitle = "";
                            aggregatedDesc = "";
                        }

                        if (!pginfo->title.isEmpty())
                        {
                            if (!aggregatedTitle.isEmpty())
                                aggregatedTitle.append(" | ");
                            aggregatedTitle.append(pginfo->title);
                        }

                        if (!pginfo->desc.isEmpty())
                        {
                            if (!aggregatedDesc.isEmpty())
                                aggregatedDesc.append(" | ");
                            aggregatedDesc.append(pginfo->desc);
                        }    
                        if (pginfo->clumpidx.toInt() == 
                            pginfo->clumpmax.toInt() - 1)
                        {
                            pginfo->title = aggregatedTitle;
                            pginfo->desc = aggregatedDesc;
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

