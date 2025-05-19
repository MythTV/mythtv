#include "xmltvparser.h"

// C++ headers
#include <cstdlib>
#include <iostream>

// Qt headers
#include <QDateTime>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>

// MythTV headers
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythmetadata/metadatadownload.h"
#include "libmythtv/channelinfo.h"
#include "libmythtv/mpeg/dvbdescriptors.h"
#include "libmythtv/programdata.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

XMLTVParser::XMLTVParser()
{
    m_currentYear = MythDate::current().date().toString("yyyy").toUInt();
}

static uint ELFHash(const QByteArray &ba)
{
    const auto *k = (const uchar *)ba.data();
    uint h = 0;

    if (k)
    {
        while (*k)
        {
            h = (h << 4) + *k++;
            uint g = (h & 0xf0000000);
            if (g != 0)
                h ^= g >> 24;
            h &= ~g;
        }
    }

    return h;
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

    QStringList split = timestr.split(" ", Qt::SkipEmptyParts);
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
        {
            tmpTime = QTime::fromString(tsTime, "HHmm");
        }
        else if (tsTime.length() == 2)
        {
            tmpTime = QTime::fromString(tsTime, "HH");
        }
        if (!tmpTime.isValid())
        {
            // Time part exists, but is (somehow) invalid
            LOG(VB_XMLTV, LOG_ERR,
                QString("Invalid datetime (time) in XMLTV data, ignoring: %1")
                    .arg(timestr));
            return;
        }
    }

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QDateTime tmpDT = QDateTime(tmpDate, tmpTime, Qt::UTC);
#else
    QDateTime tmpDT = QDateTime(tmpDate, tmpTime, QTimeZone(QTimeZone::UTC));
#endif
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

static bool readNextWithErrorCheck(QXmlStreamReader &xml)
{
    xml.readNext();
    if (xml.hasError())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Malformed XML file at line %1, %2").arg(xml.lineNumber()).arg(xml.errorString()));
        return false;
    }
    return true;
}

bool XMLTVParser::parseFile(
    const QString& filename, ChannelInfoList *chanlist,
    QMap<QString, QList<ProgInfo> > *proglist)
{
    m_movieGrabberPath = MetadataDownload::GetMovieGrabber();
    m_tvGrabberPath = MetadataDownload::GetTelevisionGrabber();
    QFile f;
    if (!dash_open(f, filename, QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error unable to open '%1' for reading.") .arg(filename));
        return false;
    }

    if (filename != "-")
    {
        QFileInfo info(f);
        if (info.size() == 0)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("File %1 exists but is empty. Did the grabber fail?").arg(filename));
            f.close();
            return false;
        }
    }

    QXmlStreamReader xml(&f);
    QUrl baseUrl;
//  QUrl sourceUrl;
    QString aggregatedTitle;
    QString aggregatedDesc;
    bool haveReadTV = false;
    while (!xml.atEnd() && !xml.hasError() && (! (xml.isEndElement() && xml.name() == QString("tv"))))
    {
#if 0
        if (xml.isDTD())
        {
            QStringRef text = xml.text();
            QStringRef name = xml.dtdName();
            QStringRef publicId = xml.dtdPublicId();
            QStringRef systemId = xml.dtdSystemId();
            QXmlStreamEntityDeclarations entities = xml.entityDeclarations();
            QXmlStreamNotationDeclarations notations = xml.notationDeclarations();

            QString msg = QString("DTD %1 name %2 PublicId %3 SystemId %4")
                          .arg(text).arg(name).arg(publicId).arg(systemId);

            if (!entities.isEmpty())
            {
                msg += " Entities";
                for (const auto entity : entities)
                    msg += QString(":name %1 PublicId %2 SystemId %3 ")
                           .arg(entity.name())
                           .arg(entity.publicId())
                           .arg(entity.systemId());
            }

            if (!notations.isEmpty())
            {
                msg += " Notations";
                for (const auto notation : notations)
                    msg += QString(": name %1 PublicId %2 SystemId %3 ")
                           .arg(notation.name())
                           .arg(notation.publicId())
                           .arg(notation.systemId());
            }

            LOG(VB_XMLTV, LOG_INFO, msg);
        }
#endif

        if (xml.readNextStartElement())
        {
            if (xml.name() == QString("tv"))
            {
//              sourceUrl = QUrl(xml.attributes().value("source-info-url").toString());
                baseUrl = QUrl(xml.attributes().value("source-data-url").toString());
                haveReadTV = true;
            }
            if (xml.name() == QString("channel"))
            {
                if (!haveReadTV)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Malformed XML file, no <tv> element found, at line %1, %2").arg(xml.lineNumber()).arg(xml.errorString()));
                    return false;
                }

                //get id attribute
                QString xmltvid;
                xmltvid = xml.attributes().value( "id").toString();
                auto *chaninfo = new ChannelInfo;
                chaninfo->m_xmltvId = xmltvid;
                chaninfo->m_tvFormat = "Default";

                //readNextStartElement says it reads for the next start element WITHIN the current element; but it doesnt; so we use readNext()
                while (!xml.isEndElement() || (xml.name() != QString("channel")))
                {
                    if (!readNextWithErrorCheck(xml))
                    {
                        delete chaninfo;
                        return false;
                    }
                    if (xml.name() == QString("icon"))
                    {
                        if (chaninfo->m_icon.isEmpty())
                        {
                            QString path = xml.attributes().value("src").toString();
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
                    else if (xml.name() == QString("display-name"))
                    {
                        //now get text
                        QString text;
                        text = xml.readElementText(QXmlStreamReader::SkipChildElements);
                        if (!text.isEmpty())
                        {
                            if (chaninfo->m_name.isEmpty())
                            {
                                chaninfo->m_name = text;
                            }
                            else if (chaninfo->m_callSign.isEmpty())
                            {
                                chaninfo->m_callSign = text;
                            }
                            else if (chaninfo->m_chanNum.isEmpty())
                            {
                                chaninfo->m_chanNum = text;
                            }
                        }
                    }
                }
                chaninfo->m_freqId = chaninfo->m_chanNum;
                //TODO optimize this, no use to do al this parsing if xmltvid is empty; but make sure you will read until the next channel!!
                if (!chaninfo->m_xmltvId.isEmpty())
                    chanlist->push_back(*chaninfo);
                delete chaninfo;
            }//channel
            else if (xml.name() == QString("programme"))
            {
                if (!haveReadTV)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Malformed XML file, no <tv> element found, at line %1, %2").arg(xml.lineNumber()).arg(xml.errorString()));
                    return false;
                }

                QString programid;
                QString season;
                QString episode;
                QString totalepisodes;
                auto *pginfo = new ProgInfo();

                QString text = xml.attributes().value("start").toString();
                fromXMLTVDate(text, pginfo->m_starttime);
                pginfo->m_startts = text;

                text = xml.attributes().value("stop").toString();
                //not a mandatory attribute according to XMLTV DTD https://github.com/XMLTV/xmltv/blob/master/xmltv.dtd
                fromXMLTVDate(text, pginfo->m_endtime);
                pginfo->m_endts = text;

                text = xml.attributes().value("channel").toString();
                QStringList split = text.split(" ");
                pginfo->m_channel = split[0];

                text = xml.attributes().value("clumpidx").toString();
                if (!text.isEmpty())
                {
                    split = text.split('/');
                    pginfo->m_clumpidx = split[0];
                    pginfo->m_clumpmax = split[1];
                }

                while (!xml.isEndElement() || (xml.name() != QString("programme")))
                {
                    if (!readNextWithErrorCheck(xml))
                    {
                        delete pginfo;
                        return false;
                    }
                    if (xml.name() == QString("title"))
                    {
                        QString text2=xml.readElementText(QXmlStreamReader::SkipChildElements);
                        if (xml.attributes().value("lang").toString() == "ja_JP")
                        { // NOLINT(bugprone-branch-clone)
                            pginfo->m_title = text2;
                        }
                        else if (xml.attributes().value("lang").toString() == "ja_JP@kana")
                        {
                            pginfo->m_title_pronounce = text2;
                        }
                        else if (pginfo->m_title.isEmpty())
                        {
                            pginfo->m_title = text2;
                        }
                    }
                    else if (xml.name() == QString("sub-title") &&  pginfo->m_subtitle.isEmpty())
                    {
                        pginfo->m_subtitle = xml.readElementText(QXmlStreamReader::SkipChildElements);
                    }
                    else if (xml.name() == QString("subtitles"))
                    {
                        if (xml.attributes().value("type").toString() == "teletext")
                            pginfo->m_subtitleType |= SUB_NORMAL;
                        else if (xml.attributes().value("type").toString() == "onscreen")
                            pginfo->m_subtitleType |= SUB_ONSCREEN;
                        else if (xml.attributes().value("type").toString() == "deaf-signed")
                            pginfo->m_subtitleType |= SUB_SIGNED;
                    }
                    else if (xml.name() == QString("desc") && pginfo->m_description.isEmpty())
                    {
                        pginfo->m_description = xml.readElementText(QXmlStreamReader::SkipChildElements);
                    }
                    else if (xml.name() == QString("category"))
                    {
                        const QString cat = xml.readElementText(QXmlStreamReader::SkipChildElements);

                        if (ProgramInfo::kCategoryNone == pginfo->m_categoryType && string_to_myth_category_type(cat) != ProgramInfo::kCategoryNone)
                        {
                            pginfo->m_categoryType = string_to_myth_category_type(cat);
                        }
                        else if (pginfo->m_category.isEmpty())
                        {
                            pginfo->m_category = cat;
                        }
                        if ((cat.compare(QObject::tr("movie"),Qt::CaseInsensitive) == 0) || (cat.compare(QObject::tr("film"),Qt::CaseInsensitive) == 0))
                        {
                            // Hack for tv_grab_uk_rt
                            pginfo->m_categoryType = ProgramInfo::kCategoryMovie;
                        }
                        pginfo->m_genres.append(cat);
                    }
                    else if (xml.name() == QString("date") && (pginfo->m_airdate == 0U))
                    {
                        // Movie production year
                        QString date = xml.readElementText(QXmlStreamReader::SkipChildElements);
                        pginfo->m_airdate = date.left(4).toUInt();
                    }
                    else if (xml.name() == QString("star-rating"))
                    {
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
                        stars = "0"; //no rating
                        while (!xml.isEndElement() || (xml.name() != QString("star-rating")))
                        {
                            if (!readNextWithErrorCheck(xml))
                                return false;
                            if (xml.isStartElement())
                            {
                                if (xml.name() == QString("value"))
                                {
                                    stars=xml.readElementText(QXmlStreamReader::SkipChildElements);
                                }
                            }
                        }
                        if (pginfo->m_stars == 0.0F)
                        {
                            float num = stars.section('/', 0, 0).toFloat() + 1;
                            float den = stars.section('/', 1, 1).toFloat() + 1;
                            if (0.0F < den)
                                rating = num/den;
                        }
                        pginfo->m_stars = rating;
                    }
                    else if (xml.name() == QString("rating"))
                    {
                        // again, the structure of ratings seems poorly represented
                        // in the XML.  no idea what we'd do with multiple values.
                        QString rat;
                        QString rating_system = xml.attributes().value("system").toString();
                        if (rating_system == nullptr)
                            rating_system = "";

                        while (!xml.isEndElement() || (xml.name() != QString("rating")))
                        {
                            if (!readNextWithErrorCheck(xml))
                                return false;
                            if (xml.isStartElement())
                            {
                                if (xml.name() == QString("value"))
                                {
                                    rat=xml.readElementText(QXmlStreamReader::SkipChildElements);
                                }
                            }
                        }

                        if (!rat.isEmpty())
                        {
                            EventRating rating;
                            rating.m_system = rating_system;
                            rating.m_rating = rat;
                            pginfo->m_ratings.append(rating);
                        }
                    }
                    else if (xml.name() == QString("previously-shown"))
                    {
                        pginfo->m_previouslyshown = true;
                        QString prevdate = xml.attributes().value( "start").toString();
                        if ((!prevdate.isEmpty()) && (pginfo->m_originalairdate.isNull()))
                        {
                            QDateTime date;
                            fromXMLTVDate(prevdate, date);
                            pginfo->m_originalairdate = date.date();
                        }
                    }
                    else if (xml.name() == QString("credits"))
                    {
                        int priority = 1;
                        while (!xml.isEndElement() || (xml.name() != QString("credits")))
                        {
                            if (!readNextWithErrorCheck(xml))
                                return false;
                            if (xml.isStartElement())
                            {
                                // Character role in optional role attribute
                                QString character = xml.attributes()
                                                  .value("role").toString();
                                QString tagname = xml.name().toString();
                                if (tagname == "actor")
                                {
                                    QString guest = xml.attributes()
                                                       .value("guest")
                                                       .toString();
                                    if (guest == "yes")
                                        tagname = "guest_star";
                                }
                                QString name = xml.readElementText(QXmlStreamReader::SkipChildElements);
                                QStringList characters = character.split("/", Qt::SkipEmptyParts);
                                if (characters.isEmpty())
                                {
                                    pginfo->AddPerson(tagname, name,
                                                      priority, character);
                                    ++priority;
                                }
                                else
                                {
                                    for (auto & c : characters)
                                    {
                                        pginfo->AddPerson(tagname, name,
                                                          priority,
                                                          c.simplified());
                                        ++priority;
                                    }
                                }
                            }
                        }
                    }
                    else if (xml.name() == QString("audio"))
                    {
                        while (!xml.isEndElement() || (xml.name() != QString("audio")))
                        {
                            if (!readNextWithErrorCheck(xml))
                                return false;
                            if (xml.isStartElement())
                            {
                                if (xml.name() == QString("stereo"))
                                {
                                    QString text2=xml.readElementText(QXmlStreamReader::SkipChildElements);
                                    if (text2 == "mono")
                                    {
                                        pginfo->m_audioProps |= AUD_MONO;
                                    }
                                    else if (text2 == "stereo")
                                    {
                                        pginfo->m_audioProps |= AUD_STEREO;
                                    }
                                    else if (text2 == "dolby" || text2 == "dolby digital")
                                    {
                                        pginfo->m_audioProps |= AUD_DOLBY;
                                    }
                                    else if (text2 == "surround")
                                    {
                                        pginfo->m_audioProps |= AUD_SURROUND;
                                    }
                                }
                            }
                        }
                    }
                    else if (xml.name() == QString("video"))
                    {
                        while (!xml.isEndElement() || (xml.name() != QString("video")))
                        {
                            if (!readNextWithErrorCheck(xml))
                                return false;
                            if (xml.isStartElement())
                            {
                                if (xml.name() == QString("quality"))
                                {
                                    if (xml.readElementText(QXmlStreamReader::SkipChildElements) == "HDTV")
                                        pginfo->m_videoProps |= VID_HDTV;
                                }
                                else if (xml.name() == QString("aspect"))
                                {
                                    if (xml.readElementText(QXmlStreamReader::SkipChildElements) == "16:9")
                                        pginfo->m_videoProps |= VID_WIDESCREEN;
                                }
                            }
                        }
                    }
                    else if (xml.name() == QString("episode-num"))
                    {
                        QString system = xml.attributes().value( "system").toString();
                        if (system == "dd_progid")
                        {
                            QString episodenum(xml.readElementText(QXmlStreamReader::SkipChildElements));
                            // if this field includes a dot, strip it out
                            int idx = episodenum.indexOf('.');
                            if (idx != -1)
                                episodenum.remove(idx, 1);
                            programid = episodenum;
                            // Only EPisodes and SHows are part of a series for SD
                            if (programid.startsWith(QString("EP")) ||
                                    programid.startsWith(QString("SH")))
                                pginfo->m_seriesId = QString("EP") + programid.mid(2,8);
                        }
                        else if (system == "xmltv_ns")
                        {
                            QString episodenum(xml.readElementText(QXmlStreamReader::SkipChildElements));
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
                        else if (system == "onscreen")
                        {
                            pginfo->m_categoryType = ProgramInfo::kCategorySeries;
                            if (pginfo->m_subtitle.isEmpty())
                            {
                                pginfo->m_subtitle = xml.readElementText(QXmlStreamReader::SkipChildElements);
                            }
                        }
                        else if ((system == "themoviedb.org") &&  (m_movieGrabberPath.endsWith(QString("/tmdb3.py"))))
                        {
                            // text is movie/<inetref>
                            QString inetrefRaw(xml.readElementText(QXmlStreamReader::SkipChildElements));
                            if (inetrefRaw.startsWith(QString("movie/")))
                            {
                                QString inetref(QString ("tmdb3.py_") + inetrefRaw.section('/',1,1).trimmed());
                                pginfo->m_inetref = inetref;
                            }
                        }
                        else if ((system == "thetvdb.com") && (m_tvGrabberPath.endsWith(QString("/ttvdb4.py"))))
                        {
                            // text is series/<inetref>
                            QString inetrefRaw(xml.readElementText(QXmlStreamReader::SkipChildElements));
                            if (inetrefRaw.startsWith(QString("series/")))
                            {
                                QString inetref(QString ("ttvdb4.py_") + inetrefRaw.section('/',1,1).trimmed());
                                pginfo->m_inetref = inetref;
                                // ProgInfo does not have a collectionref, so we don't set any
                            }
                        }
                        else if (system == "schedulesdirect.org")
                        {
                            QString details(xml.readElementText(QXmlStreamReader::SkipChildElements));
                            if (details.startsWith(QString("originalAirDate/")))
                            {
                                QString value(details.section('/', 1, 1).trimmed());
                                QDateTime datetime;
                                fromXMLTVDate(value, datetime);
                                pginfo->m_originalairdate = datetime.date();
                            }
                            else if (details.startsWith(QString("newEpisode/")))
                            {
                                QString value(details.section('/', 1, 1).trimmed());
                                if (value == QString("true"))
                                {
                                    pginfo->m_previouslyshown = false;
                                }
                                else if (value == QString("false"))
                                {
                                    pginfo->m_previouslyshown = true;
                                }
                            }
                        }
                    }//episode-num
                }

                if (pginfo->m_category.isEmpty() && pginfo->m_categoryType != ProgramInfo::kCategoryNone)
                    pginfo->m_category = myth_category_type_to_string(pginfo->m_categoryType);

                if (!pginfo->m_airdate && ProgramInfo::kCategorySeries != pginfo->m_categoryType)
                    pginfo->m_airdate = m_currentYear;

                if (programid.isEmpty())
                {
                    //Let's build ourself a programid
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
                if (!(pginfo->m_starttime.isValid()))
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Invalid programme (%1), " "invalid start time, " "skipping").arg(pginfo->m_title));
                }
                else if (pginfo->m_channel.isEmpty())
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Invalid programme (%1), " "missing channel, " "skipping").arg(pginfo->m_title));
                }
                else if (pginfo->m_startts == pginfo->m_endts)
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Invalid programme (%1), " "identical start and end " "times, skipping").arg(pginfo->m_title));
                }
                else
                {
                    // so we have a (relatively) clean program element now, which is good enough to process or to store
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
                        if (pginfo->m_clumpidx.toInt() == pginfo->m_clumpmax.toInt() - 1)
                        {
                            pginfo->m_title = aggregatedTitle;
                            pginfo->m_description = aggregatedDesc;
                            (*proglist)[pginfo->m_channel].push_back(*pginfo);
                        }
                    }
                }
                delete pginfo;
            }//if programme
        }//if readNextStartElement
    }//while loop
    if (! (xml.isEndElement() && xml.name() == QString("tv")))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Malformed XML file, missing </tv> element, at line %1, %2").arg(xml.lineNumber()).arg(xml.errorString()));
        return false;
    }
    //TODO add code for adding data on the run
    f.close();

    return true;
}
