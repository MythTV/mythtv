#include <iostream>
using namespace std;

// qt
#include <QKeyEvent>
#include <QFile>

// myth
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "uitypes.h"
#include "scheduledrecording.h"
#include "libmythdb/mythdb.h"
#include "mythuihelper.h"

#include "progdetails.h"


#define LASTPAGE 2

ProgDetails::ProgDetails(MythScreenStack *parent, const ProgramInfo *progInfo)
           : MythScreenType (parent, "progdetails")
{
    m_progInfo = *progInfo;
    m_browser = NULL;

    m_currentPage = 0;
    m_page[0] = m_page[1] = QString();
}

bool ProgDetails::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("schedule-ui.xml", "progdetails", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_browser, "browser", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'progdetails'");
        return false;
    }

    if (!BuildFocusList())
    {
#ifdef USING_QTWEBKIT
        VERBOSE(VB_IMPORTANT,
                "Failed to build a focuslist. Something is wrong");
#else
        ShowOkPopup(tr("Sorry, this screen requires Qt 4.4 or greater"));
        return false;
#endif
    }

    SetFocusWidget(m_browser);

    return true;
}

QString ProgDetails::getRatings(bool recorded, uint chanid, QDateTime startts)
{
    QString table = (recorded) ? "recordedrating" : "programrating";
    QString sel = QString(
        "SELECT system, rating FROM %1 "
        "WHERE chanid  = :CHANID "
        "AND starttime = :STARTTIME").arg(table);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sel);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startts);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ProgDetails::getRatings", query);
        return "";
    }

    QMap<QString,QString> main_ratings;
    QString advisory = "";
    while (query.next())
    {
        if (query.value(0).toString().toLower() == "advisory")
        {
            advisory += query.value(1).toString() + ", ";
            continue;
        }
        main_ratings[query.value(0).toString()] = query.value(1).toString();
    }

    if (!advisory.length() > 2)
        advisory.left(advisory.length() - 2);

    if (main_ratings.empty())
        return advisory;

    if (!advisory.isEmpty())
        advisory = ": " + advisory;

    if (main_ratings.size() == 1)
    {
        return *main_ratings.begin() + advisory;
    }

    QString ratings = "";
    QMap<QString,QString>::const_iterator it;
    for (it = main_ratings.begin(); it != main_ratings.end(); ++it)
    {
        ratings += it.key() + ": " + *it + ", ";
    }

    return ratings + "Advisory" + advisory;
}

void ProgDetails::Init()
{
    updatePage();
}

ProgDetails::~ProgDetails(void)
{
}

bool ProgDetails::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
        {
            m_currentPage++;
            if (m_currentPage >= LASTPAGE)
                m_currentPage = 0;

            updatePage();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ProgDetails::updatePage(void)
{
    if (m_page[m_currentPage] == QString())
        loadPage();

    m_browser->SetHtml(m_page[m_currentPage]);
}

void ProgDetails::addItem(const QString &key, const QString &title, const QString &data)
{
    QString escapedKey = "%" + key + "%";

    if (data == "")
    {
        removeItem(key);
        return;
    }

    // find the key in the html and replace with the correct data
    for (int x = 0; x < m_html.size(); x++)
    {
        QString s = m_html[x];
        if (s.contains(escapedKey))
        {
            // replace the label first
            s.replace("%" + key + "_LABLE%", title);
            // now replace the data
            s.replace(escapedKey, data);
            m_html[x] = s;
        }
    }
}

void ProgDetails::removeItem(const QString &key)
{
    // remove all line in the html that have key in them (should only be one line)
    for (int x = 0; x < m_html.size(); x++)
    {
        QString s = m_html[x];
        if (s.contains("%" + key + "%"))
        {
            m_html.removeAll(s);
            return;
        }
    }
}

void ProgDetails::loadPage(void)
{
    loadHTML();

    MSqlQuery query(MSqlQuery::InitCon());
    QString fullDateFormat = gContext->GetSetting("DateFormat", "M/d/yyyy");
    if (!fullDateFormat.contains("yyyy"))
        fullDateFormat += " yyyy";
    QString category_type, showtype, year, epinum, rating, colorcode,
            title_pronounce;
    float stars = 0.0;
    int partnumber = 0, parttotal = 0;
    int audioprop = 0, videoprop = 0, subtype = 0, generic = 0;
    bool recorded = false;

    ScheduledRecording* record = NULL;
    if (m_progInfo.recordid)
    {
        record = new ScheduledRecording();
        record->loadByProgram(&m_progInfo);
    }

    if (m_progInfo.filesize > 0)
        recorded = true;

    if (m_progInfo.endts != m_progInfo.startts)
    {
        QString ptable = "program";
        if (recorded)
            ptable = "recordedprogram";

        query.prepare(QString("SELECT category_type, airdate, stars,"
                      " partnumber, parttotal, audioprop+0, videoprop+0,"
                      " subtitletypes+0, syndicatedepisodenumber, generic,"
                      " showtype, colorcode, title_pronounce"
                      " FROM %1 WHERE chanid = :CHANID AND"
                      " starttime = :STARTTIME ;").arg(ptable));

        query.bindValue(":CHANID", m_progInfo.chanid);
        query.bindValue(":STARTTIME", m_progInfo.startts);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            category_type = query.value(0).toString();
            year = query.value(1).toString();
            stars = query.value(2).toDouble();
            partnumber = query.value(3).toInt();
            parttotal = query.value(4).toInt();
            audioprop = query.value(5).toInt();
            videoprop = query.value(6).toInt();
            subtype = query.value(7).toInt();
            epinum = query.value(8).toString();
            generic = query.value(9).toInt();
            showtype = query.value(10).toString();
            colorcode = query.value(11).toString();
            title_pronounce = query.value(12).toString();
        }
        else if (!query.isActive())
            MythDB::DBError("ProgDetails", query);

        rating = getRatings(recorded, m_progInfo.chanid.toUInt(), m_progInfo.startts);
    }

    if (category_type == "" && m_progInfo.programid != "")
    {
        QString prefix = m_progInfo.programid.left(2);

        if (prefix == "MV")
           category_type = "movie";
        else if (prefix == "EP")
           category_type = "series";
        else if (prefix == "SP")
           category_type = "sports";
        else if (prefix == "SH")
           category_type = "tvshow";
    }

    QString s   = "";

    s = m_progInfo.title;
    if (m_progInfo.subtitle != "")
        s += " - \"" + m_progInfo.subtitle + "\"";
    addItem("TITLE", QObject::tr("Title"), s);

    addItem("TITLE_PRONOUNCE", QObject::tr("Title Pronounce"), title_pronounce);

    s = m_progInfo.description;

    QString attr = "";

    if (partnumber > 0)
        attr += QString(QObject::tr("Part %1 of %2, ")).arg(partnumber).arg(parttotal);

    if (rating != "" && rating != "NR")
        attr += rating + ", ";
    if (category_type == "movie")
    {
        if (year != "")
            attr += year + ", ";

        if (stars > 0.0)
        {
            QString str = QObject::tr("stars");
            if (stars > 0 && stars <= 0.25)
                str = QObject::tr("star");

            attr += QString("%1 %2, ").arg(4.0 * stars).arg(str);
        }
    }
    if (colorcode != "")
        attr += colorcode + ", ";

    if (audioprop & AUD_MONO)
        attr += QObject::tr("Mono") + ", ";
    if (audioprop & AUD_STEREO)
        attr += QObject::tr("Stereo") + ", ";
    if (audioprop & AUD_SURROUND)
        attr += QObject::tr("Surround Sound") + ", ";
    if (audioprop & AUD_DOLBY)
        attr += QObject::tr("Dolby Sound") + ", ";
    if (audioprop & AUD_HARDHEAR)
        attr += QObject::tr("Audio for Hearing Impaired") + ", ";
    if (audioprop & AUD_VISUALIMPAIR)
        attr += QObject::tr("Audio for Visually Impaired") + ", ";

    if (videoprop & VID_HDTV)
        attr += QObject::tr("HDTV") + ", ";
    if  (videoprop & VID_WIDESCREEN)
        attr += QObject::tr("Widescreen") + ", ";
    if  (videoprop & VID_AVC)
        attr += QObject::tr("AVC/H.264") + ", ";

    if (subtype & SUB_HARDHEAR)
        attr += QObject::tr("CC","Closed Captioned") + ", ";
    if (subtype & SUB_NORMAL)
        attr += QObject::tr("Subtitles Available") + ", ";
    if (subtype & SUB_ONSCREEN)
        attr += QObject::tr("Subtitled") + ", ";
    if (subtype & SUB_SIGNED)
        attr += QObject::tr("Deaf Signing") + ", ";

    if (generic && category_type == "series")
        attr += QObject::tr("Unidentified Episode") + ", ";
    else if (m_progInfo.repeat)
        attr += QObject::tr("Repeat") + ", ";

    if (attr != "")
    {
        attr.truncate(attr.lastIndexOf(','));
        s += " (" + attr + ")";
    }

    addItem("DESCRIPTION", QObject::tr("Description"), s);

    s = "";
    if (m_progInfo.category != "")
    {
        s = m_progInfo.category;

        query.prepare("SELECT genre FROM programgenres "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND relevance > 0 ORDER BY relevance;");

        query.bindValue(":CHANID", m_progInfo.chanid);
        query.bindValue(":STARTTIME", m_progInfo.startts);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
                s += ", " + query.value(0).toString();
        }
    }
    addItem("CATEGORY", QObject::tr("Category"), s);

    s = "";
    if (category_type  != "")
    {
        s = category_type;
        if (m_progInfo.seriesid != "")
            s += "  (" + m_progInfo.seriesid + ")";
        if (showtype != "")
            s += "  " + showtype;
    }
    addItem("CATEGORY_TYPE", QObject::tr("Type", "category_type"), s);

    addItem("EPISODE", QObject::tr("Episode Number"), epinum);

    s = "";
    if (m_progInfo.hasAirDate && category_type != "movie")
    {
          s = m_progInfo.originalAirDate.toString(fullDateFormat);
    }
    addItem("ORIGINAL_AIRDATE", QObject::tr("Original Airdate"), s);

    addItem("PROGRAMID", QObject::tr("Program ID"), m_progInfo.programid);

    QString role = "", pname = "";
    QString actors = "", director = "", producer = "", execProducer = "";
    QString writer = "", guestStar = "", host = "", adapter = "";
    QString presenter = "", commentator = "", guest = "";

    if (m_progInfo.endts != m_progInfo.startts)
    {
        if (recorded)
            query.prepare("SELECT role,people.name FROM recordedcredits"
                          " AS credits"
                          " LEFT JOIN people ON credits.person = people.person"
                          " WHERE credits.chanid = :CHANID"
                          " AND credits.starttime = :STARTTIME"
                          " ORDER BY role;");
        else
            query.prepare("SELECT role,people.name FROM credits"
                          " LEFT JOIN people ON credits.person = people.person"
                          " WHERE credits.chanid = :CHANID"
                          " AND credits.starttime = :STARTTIME"
                          " ORDER BY role;");
        query.bindValue(":CHANID", m_progInfo.chanid);
        query.bindValue(":STARTTIME", m_progInfo.startts);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            QString rstr = "", plist = "";

            while(query.next())
            {
                role = query.value(0).toString();
                pname = query.value(1).toString();

                if (rstr == role)
                    plist += ", " + pname;
                else
                {
                    if (rstr == "actor")
                        actors = plist;
                    else if (rstr == "director")
                        director = plist;
                    else if (rstr == "producer")
                        producer = plist;
                    else if (rstr == "executive_producer")
                        execProducer = plist;
                    else if (rstr == "writer")
                        writer = plist;
                    else if (rstr == "guest_star")
                        guestStar = plist;
                    else if (rstr == "host")
                        host = plist;
                    else if (rstr == "adapter")
                        adapter = plist;
                    else if (rstr == "presenter")
                        presenter = plist;
                    else if (rstr == "commentator")
                        commentator = plist;
                    else if (rstr == "guest")
                        guest =  plist;

                    rstr = role;
                    plist = pname;
                }
            }
            if (rstr == "actor")
                actors = plist;
            else if (rstr == "director")
                director = plist;
            else if (rstr == "producer")
                producer = plist;
            else if (rstr == "executive_producer")
                execProducer = plist;
            else if (rstr == "writer")
                writer = plist;
            else if (rstr == "guest_star")
                guestStar = plist;
            else if (rstr == "host")
                host = plist;
            else if (rstr == "adapter")
                adapter = plist;
            else if (rstr == "presenter")
                presenter = plist;
            else if (rstr == "commentator")
                commentator = plist;
            else if (rstr == "guest")
                guest =  plist;
        }
    }
    addItem("ACTORS", QObject::tr("Actors"), actors);
    addItem("DIRECTOR", QObject::tr("Director"), director);
    addItem("PRODUCER", QObject::tr("Producer"), producer);
    addItem("EXECUTIVE_PRODUCER", QObject::tr("Executive Producer"), execProducer);
    addItem("WRITER", QObject::tr("Writer"), writer);
    addItem("GUEST_STAR", QObject::tr("Guest Star"), guestStar);
    addItem("HOST", QObject::tr("Host"), host);
    addItem("ADAPTER", QObject::tr("Adapter"), adapter);
    addItem("PRESENTER", QObject::tr("Presenter"), presenter);
    addItem("COMMENTATOR", QObject::tr("Commentator"), commentator);
    addItem("GUEST", QObject::tr("Guest"), guest);

    // Begin MythTV information not found in the listings info
//    msg += "<br>";
    QDateTime statusDate;
    if (m_progInfo.recstatus == rsWillRecord)
        statusDate = m_progInfo.startts;

    ProgramInfo *p = new ProgramInfo;
    p->rectype = kSingleRecord; // must be != kNotRecording
    p->recstatus = m_progInfo.recstatus;

    if (p->recstatus == rsPreviousRecording ||
        p->recstatus == rsNeverRecord || p->recstatus == rsUnknown)
    {
        query.prepare("SELECT recstatus, starttime "
                      "FROM oldrecorded WHERE duplicate > 0 AND "
                      "((programid <> '' AND programid = :PROGRAMID) OR "
                      " (title <> '' AND title = :TITLE AND "
                      "  subtitle <> '' AND subtitle = :SUBTITLE AND "
                      "  description <> '' AND description = :DECRIPTION));");

        query.bindValue(":PROGRAMID", m_progInfo.programid);
        query.bindValue(":TITLE", m_progInfo.title);
        query.bindValue(":SUBTITLE", m_progInfo.subtitle);
        query.bindValue(":DECRIPTION", m_progInfo.description);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("showDetails", query);

        if (query.isActive() && query.size() > 0)
        {
            query.next();
            if (p->recstatus == rsUnknown)
                p->recstatus = RecStatusType(query.value(0).toInt());
            if (p->recstatus == rsPreviousRecording ||
                p->recstatus == rsNeverRecord || p->recstatus == rsRecorded)
                statusDate = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        }
    }
    if (p->recstatus == rsUnknown)
    {
        if (recorded)
        {
            p->recstatus = rsRecorded;
            statusDate = m_progInfo.startts;
        }
        else
        {
            p->rectype = m_progInfo.rectype; // re-enable "Not Recording" status text.
        }
    }
    s = p->RecStatusText();
    if (statusDate.isValid())
        s += " " + statusDate.toString(fullDateFormat);
    addItem("MYTHTV_STATUS", QString("MythTV " + QObject::tr("Status")), s);
    delete p;

    QString recordingRule = "";
    QString lastRecorded = "";
    QString nextRecording = "";
    QString averageTimeShift = "";
    QString watchListScore = "";
    QString watchListStatus = "";
    QString searchPhrase = "";

    if (m_progInfo.recordid)
    {
        recordingRule = QString("%1, ").arg(m_progInfo.recordid);
        if (m_progInfo.rectype != kNotRecording)
            recordingRule += m_progInfo.RecTypeText();
        if (!(record->getRecordTitle().isEmpty()))
            recordingRule += QString(" \"%2\"").arg(record->getRecordTitle());

        query.prepare("SELECT last_record, next_record, avg_delay "
                      "FROM record WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", m_progInfo.recordid);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            if (query.value(0).toDateTime().isValid())
                lastRecorded = query.value(0).toDateTime().toString(fullDateFormat);
            if (query.value(1).toDateTime().isValid())
                nextRecording = query.value(1).toDateTime().toString(fullDateFormat);
            if (query.value(2).toInt() > 0)
                averageTimeShift = QString("%1 %2").arg(query.value(2).toInt())
                                        .arg(QObject::tr("hours"));
        }
        if (recorded)
        {
            if (m_progInfo.recpriority2 > 0)
                watchListScore = QString("%1").arg(m_progInfo.recpriority2);

            if (m_progInfo.recpriority2 < 0)
            {
                switch(m_progInfo.recpriority2)
                {
                case wlExpireOff:
                    watchListStatus = QObject::tr("Auto-expire off");
                    break;
                case wlWatched:
                    watchListStatus = QObject::tr("Marked as 'watched'");
                    break;
                case wlEarlier:
                    watchListStatus = QObject::tr("Not the earliest episode");
                    break;
                case wlDeleted:
                    watchListStatus = QObject::tr("Recently deleted episode");
                    break;
                }
            }
        }
        if (record->getSearchType() &&
            record->getSearchType() != kManualSearch &&
            record->getRecordDescription() != m_progInfo.description)
            searchPhrase = record->getRecordDescription()
                    .replace("<", "&lt;")
                    .replace(">", "&gt;").replace("\n", " ");
    }
    addItem("RECORDING_RULE", QObject::tr("Recording Rule"), recordingRule);
    addItem("LAST_RECORDED", QObject::tr("Last Recorded"), lastRecorded);
    addItem("NEXT_RECORDING", QObject::tr("Next Recording"), nextRecording);
    addItem("AVERAGE_TIME_SHIFT", QObject::tr("Average Time Shift"), averageTimeShift);
    addItem("WATCH_LIST_SCORE", QObject::tr("Watch List Score"), watchListScore);
    addItem("WATCH_LIST_STATUS", QObject::tr("Watch List Status"), watchListStatus);
    addItem("SEARCH_PHRASE", QObject::tr("Search Phrase"), searchPhrase);

    s = "";
    if (m_progInfo.findid > 0)
    {
        QDate fdate(1970, 1, 1);
        fdate = fdate.addDays(m_progInfo.findid - 719528);
        s = QString("%1 (%2)").arg(m_progInfo.findid).arg(fdate.toString(fullDateFormat));
    }
    addItem("FINDID", QObject::tr("Find ID"), s);

    QString recordingHost = "";
    QString recordedFilename = "";
    QString recordedFileSize = "";
    QString recordingGroup = "";
    QString storageGroup = "";
    QString playbackGroup = "";
    QString recordingProfile = "";

    if (recorded)
    {
        recordingHost = m_progInfo.hostname;
        recordedFilename = m_progInfo.GetRecordBasename();
        recordedFileSize.sprintf("%0.2f ", m_progInfo.filesize / 1024.0 / 1024.0 / 1024.0);
        recordedFileSize += QObject::tr("GB", "GigaBytes");

        query.prepare("SELECT profile FROM recorded"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME;");
        query.bindValue(":CHANID", m_progInfo.chanid);
        query.bindValue(":STARTTIME", m_progInfo.recstartts);

        if (query.exec() && query.next())
        {
            recordingProfile = m_progInfo.i18n(query.value(0).toString());
        }
        recordingGroup = m_progInfo.i18n(m_progInfo.recgroup);
        storageGroup =  m_progInfo.i18n(m_progInfo.storagegroup);
        playbackGroup =  m_progInfo.i18n(m_progInfo.playgroup);
    }
    else if (m_progInfo.recordid)
    {
        recordingProfile =  record->getProfileName();
    }
    addItem("RECORDING_HOST", QObject::tr("Recording Host"), recordingHost);
    addItem("RECORDED_FILE_NAME", QObject::tr("Recorded File Name"), recordedFilename);
    addItem("RECORDED_FILE_SIZE", QObject::tr("Recorded File Size"), recordedFileSize);
    addItem("RECORDING_PROFILE", QObject::tr("Recording Profile"), recordingProfile);
    addItem("RECORDING_GROUP", QObject::tr("Recording Group"), recordingGroup);
    addItem("STORAGE_GROUP", QObject::tr("Storage Group"), storageGroup);
    addItem("PLAYBACK_GROUP", QObject::tr("Playback Group"),  playbackGroup);

    m_page[m_currentPage] = m_html.join("\n");
}

bool ProgDetails::loadHTML(void)
{
    m_html.clear();

    QString filename = QString("htmls/progdetails_page%1.html").arg(m_currentPage + 1);
    if (!GetMythUI()->FindThemeFile(filename))
        return false;

    QFile file(QLatin1String(qPrintable(filename)));

    if (!file.exists())
        return false;

    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);

        while ( !stream.atEnd() )
        {
            m_html.append(stream.readLine());
        }
        file.close();
    }
    else
        return false;

    return true;
}
