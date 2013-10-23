
// qt
#include <QKeyEvent>
#include <QFile>
#include <QTextStream>

// myth
#include "mythcorecontext.h"
#include "mythdialogbox.h"
#include "recordingrule.h"
#include "mythdb.h"
#include "mythuihelper.h"
#include "mythmainwindow.h"

#include "progdetails.h"
#include "mythdate.h"

#define LASTPAGE 2

ProgDetails::ProgDetails(MythScreenStack *parent, const ProgramInfo *progInfo) :
    MythScreenType (parent, "progdetails"),
    m_progInfo(*progInfo), m_browser(NULL),
    m_currentPage(0)
{
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
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'progdetails'");
        return false;
    }

    BuildFocusList();

    SetFocusWidget(m_browser);

    float zoom = gCoreContext->GetSetting("ProgDetailsZoom", "1.0").toFloat();
    m_browser->SetZoom(zoom);

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
    QString advisory;
    while (query.next())
    {
        if (query.value(0).toString().toLower() == "advisory")
        {
            advisory += query.value(1).toString() + ", ";
            continue;
        }
        main_ratings[query.value(0).toString()] = query.value(1).toString();
    }

    advisory = advisory.left(advisory.length() - 2);

    if (main_ratings.empty())
        return advisory;

    if (!advisory.isEmpty())
        advisory = ": " + advisory;

    if (main_ratings.size() == 1)
    {
        return *main_ratings.begin() + advisory;
    }

    QString ratings;
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
    float zoom = m_browser->GetZoom();
    gCoreContext->SaveSetting("ProgDetailsZoom", QString().setNum(zoom));
}

bool ProgDetails::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

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
        else if (action == "MENU")
            showMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ProgDetails::updatePage(void)
{
    if (m_page[m_currentPage].isEmpty())
        loadPage();

    m_browser->SetHtml(m_page[m_currentPage]);
}

void ProgDetails::addItem(const QString &key, const QString &title, const QString &data)
{
    QString escapedKey = "%" + key + "%";

    if (data.isEmpty())
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
            s.replace("%" + key + "_LABEL%", title);
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
    QString category_type, showtype, year, syndicatedEpisodeNum, rating, colorcode,
            title_pronounce;
    float stars = 0.0;
    int partnumber = 0, parttotal = 0;
    int audioprop = 0, videoprop = 0, subtype = 0, generic = 0;
    bool recorded = false;

    RecordingRule* record = NULL;
    if (m_progInfo.GetRecordingRuleID())
    {
        record = new RecordingRule();
        record->LoadByProgram(&m_progInfo);
    }

    if (m_progInfo.GetFilesize())
        recorded = true;

    if (m_progInfo.GetScheduledEndTime() != m_progInfo.GetScheduledStartTime())
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

        query.bindValue(":CHANID",    m_progInfo.GetChanID());
        query.bindValue(":STARTTIME", m_progInfo.GetScheduledStartTime());

        if (query.exec() && query.next())
        {
            category_type = query.value(0).toString();
            year = query.value(1).toString();
            stars = query.value(2).toDouble();
            partnumber = query.value(3).toInt();
            parttotal = query.value(4).toInt();
            audioprop = query.value(5).toInt();
            videoprop = query.value(6).toInt();
            subtype = query.value(7).toInt();
            syndicatedEpisodeNum = query.value(8).toString();
            generic = query.value(9).toInt();
            showtype = query.value(10).toString();
            colorcode = query.value(11).toString();
            title_pronounce = query.value(12).toString();
        }
        else if (!query.isActive())
            MythDB::DBError("ProgDetails", query);

        rating = getRatings(
            recorded, m_progInfo.GetChanID(),
            m_progInfo.GetScheduledStartTime());
    }

    if (category_type.isEmpty() && !m_progInfo.GetProgramID().isEmpty())
    {
        QString prefix = m_progInfo.GetProgramID().left(2);

        if (prefix == "MV")
           category_type = "movie";
        else if (prefix == "EP")
           category_type = "series";
        else if (prefix == "SP")
           category_type = "sports";
        else if (prefix == "SH")
           category_type = "tvshow";
    }

    addItem("TITLE", tr("Title"),
            m_progInfo.toString(ProgramInfo::kTitleSubtitle, " - "));

    addItem("TITLE_PRONOUNCE", tr("Title Pronounce"), title_pronounce);

    QString s = m_progInfo.GetDescription();

    QString attr;

    if (partnumber > 0)
        attr += tr("Part %1 of %2, ").arg(partnumber).arg(parttotal);

    if (!rating.isEmpty() && rating != "NR")
        attr += rating + ", ";
    if (category_type == "movie")
    {
        if (!year.isEmpty())
            attr += year + ", ";

        if (stars > 0.0)
            attr += tr("%n star(s)", "", (int) (stars * 4.0)) + ", ";
    }
    if (!colorcode.isEmpty())
        attr += colorcode + ", ";

    if (audioprop & AUD_MONO)
        attr += tr("Mono") + ", ";
    if (audioprop & AUD_STEREO)
        attr += tr("Stereo") + ", ";
    if (audioprop & AUD_SURROUND)
        attr += tr("Surround Sound") + ", ";
    if (audioprop & AUD_DOLBY)
        attr += tr("Dolby Sound") + ", ";
    if (audioprop & AUD_HARDHEAR)
        attr += tr("Audio for Hearing Impaired") + ", ";
    if (audioprop & AUD_VISUALIMPAIR)
        attr += tr("Audio for Visually Impaired") + ", ";

    if (videoprop & VID_HDTV)
        attr += tr("HDTV") + ", ";
    if  (videoprop & VID_WIDESCREEN)
        attr += tr("Widescreen") + ", ";
    if  (videoprop & VID_AVC)
        attr += tr("AVC/H.264") + ", ";
    if  (videoprop & VID_720)
        attr += tr("720p Resolution") + ", ";
    if  (videoprop & VID_1080)
        attr += tr("1080i/p Resolution") + ", ";

    if (subtype & SUB_HARDHEAR)
        attr += tr("CC","Closed Captioned") + ", ";
    if (subtype & SUB_NORMAL)
        attr += tr("Subtitles Available") + ", ";
    if (subtype & SUB_ONSCREEN)
        attr += tr("Subtitled") + ", ";
    if (subtype & SUB_SIGNED)
        attr += tr("Deaf Signing") + ", ";

    if (generic && category_type == "series")
        attr += tr("Unidentified Episode") + ", ";
    else if (m_progInfo.IsRepeat())
        attr += tr("Repeat") + ", ";

    if (!attr.isEmpty())
    {
        attr.truncate(attr.lastIndexOf(','));
        s += " (" + attr + ")";
    }

    addItem("DESCRIPTION", tr("Description"), s);

    s.clear();
    if (!m_progInfo.GetCategory().isEmpty())
    {
        s = m_progInfo.GetCategory();

        query.prepare("SELECT genre FROM programgenres "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND relevance > 0 ORDER BY relevance;");

        query.bindValue(":CHANID",    m_progInfo.GetChanID());
        query.bindValue(":STARTTIME", m_progInfo.GetScheduledStartTime());

        if (query.exec())
        {
            while (query.next())
                s += ", " + query.value(0).toString();
        }
    }
    addItem("CATEGORY", tr("Category"), s);

    s.clear();
    if (!category_type.isEmpty())
    {
        s = category_type;
        if (!m_progInfo.GetSeriesID().isEmpty())
            s += "  (" + m_progInfo.GetSeriesID() + ")";
        if (!showtype.isEmpty())
            s += "  " + showtype;
    }
    addItem("CATEGORY_TYPE", tr("Type", "category_type"), s);

    QString episode;
    if (m_progInfo.GetEpisodeTotal() > 0)
        episode = tr("%1 of %2").arg(m_progInfo.GetEpisode()).arg(m_progInfo.GetEpisodeTotal());
    else
        episode = QString::number(m_progInfo.GetEpisode());

    addItem("EPISODE", tr("Episode"), episode);
    addItem("SEASON", tr("Season"), QString::number(m_progInfo.GetSeason()));

    addItem("SYNDICATEDEPISODENUMBER", tr("Syndicated Episode Number"), syndicatedEpisodeNum);

    s.clear();
    if (m_progInfo.GetOriginalAirDate().isValid() &&
        category_type != "movie")
    {
        s = MythDate::toString(m_progInfo.GetOriginalAirDate(),
                               MythDate::kDateFull | MythDate::kAddYear);
    }
    addItem("ORIGINAL_AIRDATE", tr("Original Airdate"), s);

    addItem("PROGRAMID", tr("Program ID"), m_progInfo.GetProgramID());

    QString actors, directors, producers, execProducers;
    QString writers, guestStars, hosts, adapters;
    QString presenters, commentators, guests;

    if (m_progInfo.GetScheduledEndTime() != m_progInfo.GetScheduledStartTime())
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
        query.bindValue(":CHANID",    m_progInfo.GetChanID());
        query.bindValue(":STARTTIME", m_progInfo.GetScheduledStartTime());

        if (query.exec() && query.size() > 0)
        {
            QStringList plist;
            QString rstr, role, pname;

            while(query.next())
            {
                role = query.value(0).toString();
                /* The people.name column uses utf8_bin collation.
                 * Qt-MySQL drivers use QVariant::ByteArray for string-type
                 * MySQL fields marked with the BINARY attribute (those using a
                 * *_bin collation) and QVariant::String for all others.
                 * Since QVariant::toString() uses QString::fromAscii()
                 * (through QVariant::convert()) when the QVariant's type is
                 * QVariant::ByteArray, we have to use QString::fromUtf8()
                 * explicitly to prevent corrupting characters.
                 * The following code should be changed to use the simpler
                 * toString() approach, as above, if we do a DB update to
                 * coalesce the people.name values that differ only in case and
                 * change the collation to utf8_general_ci, to match the
                 * majority of other columns, or we'll have the same problem in
                 * reverse.
                 */
                pname = QString::fromUtf8(query.value(1)
                                          .toByteArray().constData());

                if (rstr != role)
                {
                    if (rstr == "actor")
                        actors = plist.join(", ");
                    else if (rstr == "director")
                        directors = plist.join(", ");
                    else if (rstr == "producer")
                        producers = plist.join(", ");
                    else if (rstr == "executive_producer")
                        execProducers = plist.join(", ");
                    else if (rstr == "writer")
                        writers = plist.join(", ");
                    else if (rstr == "guest_star")
                        guestStars = plist.join(", ");
                    else if (rstr == "host")
                        hosts = plist.join(", ");
                    else if (rstr == "adapter")
                        adapters = plist.join(", ");
                    else if (rstr == "presenter")
                        presenters = plist.join(", ");
                    else if (rstr == "commentator")
                        commentators = plist.join(", ");
                    else if (rstr == "guest")
                        guests =  plist.join(", ");

                    rstr = role;
                    plist.clear();
                }

                plist.append(pname);
            }
            if (rstr == "actor")
                actors = plist.join(", ");
            else if (rstr == "director")
                directors = plist.join(", ");
            else if (rstr == "producer")
                producers = plist.join(", ");
            else if (rstr == "executive_producer")
                execProducers = plist.join(", ");
            else if (rstr == "writer")
                writers = plist.join(", ");
            else if (rstr == "guest_star")
                guestStars = plist.join(", ");
            else if (rstr == "host")
                hosts = plist.join(", ");
            else if (rstr == "adapter")
                adapters = plist.join(", ");
            else if (rstr == "presenter")
                presenters = plist.join(", ");
            else if (rstr == "commentator")
                commentators = plist.join(", ");
            else if (rstr == "guest")
                guests =  plist.join(", ");
        }
    }
    addItem("ACTORS", tr("Actors"), actors);
    addItem("DIRECTOR", tr("Director"), directors);
    addItem("PRODUCER", tr("Producer"), producers);
    addItem("EXECUTIVE_PRODUCER", tr("Executive Producer"), execProducers);
    addItem("WRITER", tr("Writer"), writers);
    addItem("GUEST_STAR", tr("Guest Star"), guestStars);
    addItem("HOST", tr("Host"), hosts);
    addItem("ADAPTER", tr("Adapter"), adapters);
    addItem("PRESENTER", tr("Presenter"), presenters);
    addItem("COMMENTATOR", tr("Commentator"), commentators);
    addItem("GUEST", tr("Guest"), guests);

    // Begin MythTV information not found in the listings info
//    msg += "<br>";
    QDateTime statusDate;
    if (m_progInfo.GetRecordingStatus() == rsWillRecord)
        statusDate = m_progInfo.GetScheduledStartTime();

    RecordingType rectype = kSingleRecord; // avoid kNotRecording
    RecStatusType recstatus = m_progInfo.GetRecordingStatus();

    if (recstatus == rsPreviousRecording || recstatus == rsNeverRecord ||
        recstatus == rsUnknown)
    {
        query.prepare("SELECT recstatus, starttime "
                      "FROM oldrecorded WHERE duplicate > 0 AND "
                      "future = 0 AND "
                      "((programid <> '' AND programid = :PROGRAMID) OR "
                      " (title <> '' AND title = :TITLE AND "
                      "  subtitle <> '' AND subtitle = :SUBTITLE AND "
                      "  description <> '' AND description = :DECRIPTION));");

        query.bindValue(":PROGRAMID",  m_progInfo.GetProgramID());
        query.bindValue(":TITLE",      m_progInfo.GetTitle());
        query.bindValue(":SUBTITLE",   m_progInfo.GetSubtitle());
        query.bindValue(":DECRIPTION", m_progInfo.GetDescription());

        if (!query.exec())
        {
            MythDB::DBError("showDetails", query);
        }
        else if (query.next())
        {
            if (recstatus == rsUnknown)
                recstatus = RecStatusType(query.value(0).toInt());

            if (recstatus == rsPreviousRecording ||
                recstatus == rsNeverRecord ||
                recstatus == rsRecorded)
            {
                statusDate = MythDate::as_utc(query.value(1).toDateTime());
            }
        }
    }

    if (recstatus == rsUnknown)
    {
        if (recorded)
        {
            recstatus = rsRecorded;
            statusDate = m_progInfo.GetScheduledStartTime();
        }
        else
        {
            // re-enable "Not Recording" status text
            rectype = m_progInfo.GetRecordingRuleType();
        }
    }

    s = toString(recstatus, rectype);

    if (statusDate.isValid())
        s += " " + MythDate::toString(statusDate, MythDate::kDateFull | MythDate::kAddYear);

    addItem("MYTHTV_STATUS", tr("MythTV Status"), s);

    QString recordingRule;
    QString lastRecorded;
    QString nextRecording;
    QString averageTimeShift;
    QString watchListScore;
    QString watchListStatus;
    QString searchPhrase;

    if (m_progInfo.GetRecordingRuleID())
    {
        recordingRule = QString("%1, ").arg(m_progInfo.GetRecordingRuleID());
        if (m_progInfo.GetRecordingRuleType() != kNotRecording)
            recordingRule += toString(m_progInfo.GetRecordingRuleType());
        if (!(record->m_title.isEmpty()))
            recordingRule += QString(" \"%2\"").arg(record->m_title);

        query.prepare("SELECT last_record, next_record, avg_delay "
                      "FROM record WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", m_progInfo.GetRecordingRuleID());

        if (query.exec() && query.next())
        {
            if (query.value(0).toDateTime().isValid())
                lastRecorded = MythDate::toString(
                    MythDate::as_utc(query.value(0).toDateTime()),
                    MythDate::kDateFull | MythDate::kAddYear);
            if (query.value(1).toDateTime().isValid())
                nextRecording = MythDate::toString(
                    MythDate::as_utc(query.value(1).toDateTime()),
                    MythDate::kDateFull | MythDate::kAddYear);
            if (query.value(2).toInt() > 0)
                averageTimeShift = tr("%n hour(s)", "",
                                                query.value(2).toInt());
        }
        if (recorded)
        {
            if (m_progInfo.GetRecordingPriority2() > 0)
                watchListScore =
                    QString::number(m_progInfo.GetRecordingPriority2());

            if (m_progInfo.GetRecordingPriority2() < 0)
            {
                switch (m_progInfo.GetRecordingPriority2())
                {
                    case wlExpireOff:
                        watchListStatus = tr("Auto-expire off");
                        break;
                    case wlWatched:
                        watchListStatus = tr("Marked as 'watched'");
                        break;
                    case wlEarlier:
                        watchListStatus = tr("Not the earliest episode");
                        break;
                    case wlDeleted:
                        watchListStatus = tr("Recently deleted episode");
                        break;
                }
            }
        }
        if (record->m_searchType != kManualSearch &&
            record->m_description != m_progInfo.GetDescription())
        {
            searchPhrase = record->m_description
                .replace("<", "&lt;").replace(">", "&gt;").replace("\n", " ");
        }
    }
    addItem("RECORDING_RULE", tr("Recording Rule"), recordingRule);
    addItem("LAST_RECORDED", tr("Last Recorded"), lastRecorded);
    addItem("NEXT_RECORDING", tr("Next Recording"), nextRecording);
    addItem("AVERAGE_TIME_SHIFT", tr("Average Time Shift"), averageTimeShift);
    addItem("WATCH_LIST_SCORE", tr("Watch List Score"), watchListScore);
    addItem("WATCH_LIST_STATUS", tr("Watch List Status"), watchListStatus);
    addItem("SEARCH_PHRASE", tr("Search Phrase"), searchPhrase);

    s.clear();
    if (m_progInfo.GetFindID())
    {
        QDateTime fdate(QDate(1970, 1, 1),QTime(12,0,0));
        fdate = fdate.addDays((int)m_progInfo.GetFindID() - 719528);
        s = QString("%1 (%2)").arg(m_progInfo.GetFindID())
            .arg(MythDate::toString(
                     fdate, MythDate::kDateFull | MythDate::kAddYear));
    }
    addItem("FINDID", tr("Find ID"), s);

    QString recordingHost;
    QString recordedFilename;
    QString recordedFileSize;
    QString recordingGroup;
    QString storageGroup;
    QString playbackGroup;
    QString recordingProfile;

    if (recorded)
    {
        recordingHost = m_progInfo.GetHostname();
        recordedFilename = m_progInfo.GetBasename();
        recordedFileSize = QString("%1 ")
            .arg(m_progInfo.GetFilesize()/((double)(1<<30)),0,'f',2);
        recordedFileSize += tr("GB", "GigaBytes");

        query.prepare("SELECT profile FROM recorded"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME;");
        query.bindValue(":CHANID",    m_progInfo.GetChanID());
        query.bindValue(":STARTTIME", m_progInfo.GetRecordingStartTime());

        if (query.exec() && query.next())
        {
            recordingProfile = m_progInfo.i18n(query.value(0).toString());
        }
        recordingGroup = m_progInfo.i18n(m_progInfo.GetRecordingGroup());
        storageGroup   = m_progInfo.i18n(m_progInfo.GetStorageGroup());
        playbackGroup  = m_progInfo.i18n(m_progInfo.GetPlaybackGroup());
    }
    else if (m_progInfo.GetRecordingRuleID())
    {
        recordingProfile =  record->m_recProfile;
    }
    addItem("RECORDING_HOST", tr("Recording Host"), recordingHost);
    addItem("RECORDED_FILE_NAME", tr("Recorded File Name"), recordedFilename);
    addItem("RECORDED_FILE_SIZE", tr("Recorded File Size"), recordedFileSize);
    addItem("RECORDING_PROFILE", tr("Recording Profile"), recordingProfile);
    addItem("RECORDING_GROUP", tr("Recording Group"), recordingGroup);
    addItem("STORAGE_GROUP", tr("Storage Group"), storageGroup);
    addItem("PLAYBACK_GROUP", tr("Playback Group"),  playbackGroup);

    m_page[m_currentPage] = m_html.join("\n");

    delete record;
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

void ProgDetails::showMenu(void)
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");

        menuPopup->AddButton(tr("Zoom In"));
        menuPopup->AddButton(tr("Zoom Out"));
        menuPopup->AddButton(tr("Switch Page"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgDetails::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "menu")
        {
            if (resulttext == tr("Zoom Out"))
                m_browser->ZoomOut();
            else if (resulttext == tr("Zoom In"))
                m_browser->ZoomIn();
            else if (resulttext == tr("Switch Page"))
            {
                m_currentPage++;
                if (m_currentPage >= LASTPAGE)
                    m_currentPage = 0;

                updatePage();
            }
        }
    }
}
