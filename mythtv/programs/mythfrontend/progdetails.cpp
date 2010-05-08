
// qt
#include <QKeyEvent>
#include <QFile>

// myth
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "uitypes.h"
#include "recordingrule.h"
#include "mythdb.h"
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

    BuildFocusList();

    SetFocusWidget(m_browser);

    float zoom = gContext->GetSetting("ProgDetailsZoom", "1.0").toFloat();
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
    gContext->SaveSetting("ProgDetailsZoom", QString().setNum(zoom));
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
    QString fullDateFormat = gContext->GetSetting("DateFormat", "M/d/yyyy");
    if (!fullDateFormat.contains("yyyy"))
        fullDateFormat += " yyyy";
    QString category_type, showtype, year, epinum, rating, colorcode,
            title_pronounce;
    float stars = 0.0;
    int partnumber = 0, parttotal = 0;
    int audioprop = 0, videoprop = 0, subtype = 0, generic = 0;
    bool recorded = false;

    RecordingRule* record = NULL;
    if (m_progInfo.recordid)
    {
        record = new RecordingRule();
        record->LoadByProgram(&m_progInfo);
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

    if (category_type.isEmpty() && !m_progInfo.programid.isEmpty())
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

    QString s = m_progInfo.title;
    if (!m_progInfo.subtitle.isEmpty())
        s += " - \"" + m_progInfo.subtitle + "\"";
    addItem("TITLE", tr("Title"), s);

    addItem("TITLE_PRONOUNCE", tr("Title Pronounce"), title_pronounce);

    s = m_progInfo.description;

    QString attr;

    if (partnumber > 0)
        attr += QString(tr("Part %1 of %2, ")).arg(partnumber).arg(parttotal);

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
    else if (m_progInfo.repeat)
        attr += tr("Repeat") + ", ";

    if (!attr.isEmpty())
    {
        attr.truncate(attr.lastIndexOf(','));
        s += " (" + attr + ")";
    }

    addItem("DESCRIPTION", tr("Description"), s);

    s.clear();
    if (!m_progInfo.category.isEmpty())
    {
        s = m_progInfo.category;

        query.prepare("SELECT genre FROM programgenres "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND relevance > 0 ORDER BY relevance;");

        query.bindValue(":CHANID", m_progInfo.chanid);
        query.bindValue(":STARTTIME", m_progInfo.startts);

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
        if (!m_progInfo.seriesid.isEmpty())
            s += "  (" + m_progInfo.seriesid + ")";
        if (!showtype.isEmpty())
            s += "  " + showtype;
    }
    addItem("CATEGORY_TYPE", tr("Type", "category_type"), s);

    addItem("EPISODE", tr("Episode Number"), epinum);

    s.clear();
    if (m_progInfo.hasAirDate && category_type != "movie")
    {
        s = m_progInfo.originalAirDate.toString(fullDateFormat);
    }
    addItem("ORIGINAL_AIRDATE", tr("Original Airdate"), s);

    addItem("PROGRAMID", tr("Program ID"), m_progInfo.programid);

    QString actors, director, producer, execProducer;
    QString writer, guestStar, host, adapter;
    QString presenter, commentator, guest;

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

        if (query.exec() && query.size() > 0)
        {
            QString rstr, plist;
            QString role, pname;

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
    addItem("ACTORS", tr("Actors"), actors);
    addItem("DIRECTOR", tr("Director"), director);
    addItem("PRODUCER", tr("Producer"), producer);
    addItem("EXECUTIVE_PRODUCER", tr("Executive Producer"), execProducer);
    addItem("WRITER", tr("Writer"), writer);
    addItem("GUEST_STAR", tr("Guest Star"), guestStar);
    addItem("HOST", tr("Host"), host);
    addItem("ADAPTER", tr("Adapter"), adapter);
    addItem("PRESENTER", tr("Presenter"), presenter);
    addItem("COMMENTATOR", tr("Commentator"), commentator);
    addItem("GUEST", tr("Guest"), guest);

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

        if (query.next())
        {
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
    addItem("MYTHTV_STATUS", QString("MythTV " + tr("Status")), s);
    delete p;

    QString recordingRule;
    QString lastRecorded;
    QString nextRecording;
    QString averageTimeShift;
    QString watchListScore;
    QString watchListStatus;
    QString searchPhrase;

    if (m_progInfo.recordid)
    {
        recordingRule = QString("%1, ").arg(m_progInfo.recordid);
        if (m_progInfo.rectype != kNotRecording)
            recordingRule += m_progInfo.RecTypeText();
        if (!(record->m_title.isEmpty()))
            recordingRule += QString(" \"%2\"").arg(record->m_title);

        query.prepare("SELECT last_record, next_record, avg_delay "
                      "FROM record WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", m_progInfo.recordid);

        if (query.exec() && query.next())
        {
            if (query.value(0).toDateTime().isValid())
                lastRecorded = query.value(0).toDateTime().toString(fullDateFormat);
            if (query.value(1).toDateTime().isValid())
                nextRecording = query.value(1).toDateTime().toString(fullDateFormat);
            if (query.value(2).toInt() > 0)
                averageTimeShift = tr("%n hour(s)", "",
                                                query.value(2).toInt());
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
            record->m_description != m_progInfo.description)
            searchPhrase = record->m_description.replace("<", "&lt;")
                                                .replace(">", "&gt;")
                                                .replace("\n", " ");
    }
    addItem("RECORDING_RULE", tr("Recording Rule"), recordingRule);
    addItem("LAST_RECORDED", tr("Last Recorded"), lastRecorded);
    addItem("NEXT_RECORDING", tr("Next Recording"), nextRecording);
    addItem("AVERAGE_TIME_SHIFT", tr("Average Time Shift"), averageTimeShift);
    addItem("WATCH_LIST_SCORE", tr("Watch List Score"), watchListScore);
    addItem("WATCH_LIST_STATUS", tr("Watch List Status"), watchListStatus);
    addItem("SEARCH_PHRASE", tr("Search Phrase"), searchPhrase);

    s.clear();
    if (m_progInfo.findid > 0)
    {
        QDate fdate(1970, 1, 1);
        fdate = fdate.addDays(m_progInfo.findid - 719528);
        s = QString("%1 (%2)").arg(m_progInfo.findid).arg(fdate.toString(fullDateFormat));
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
        recordingHost = m_progInfo.hostname;
        recordedFilename = m_progInfo.GetRecordBasename();
        recordedFileSize.sprintf("%0.2f ", m_progInfo.filesize / 1024.0 / 1024.0 / 1024.0);
        recordedFileSize += tr("GB", "GigaBytes");

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
        menuPopup->AddButton(tr("Cancel"));

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
