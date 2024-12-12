
// Qt
#include <QFile>
#include <QKeyEvent>
#include <QTextStream>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythtv/recordingrule.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"

//  MythFrontend
#include "progdetails.h"

bool ProgDetails::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("schedule-ui.xml", "programdetails", this);
    if (!foundtheme)
        return false;

    // Initialise details list
    if (!m_infoList.Create(true))
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load 'Info buttonlist'");
        return false;
    }

    BuildFocusList();

    return true;
}

QString ProgDetails::getRatings(bool recorded, uint chanid, const QDateTime& startts)
{
    QString table = (recorded) ? "recordedrating" : "programrating";
    QString sel = QString(
        "SELECT `system`, rating FROM %1 "
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
    for (it = main_ratings.cbegin(); it != main_ratings.cend(); ++it)
    {
        ratings += it.key() + ": " + *it + ", ";
    }

    return ratings + "Advisory" + advisory;
}

ProgDetails::~ProgDetails(void)
{
    m_infoList.Hide();
}

void ProgDetails::Init(void)
{
    updatePage();
}

void ProgDetails::updatePage(void)
{
    if (m_data.isEmpty())
        loadPage();

    m_infoList.Display(m_data);
}

void ProgDetails::addItem(const QString &title, const QString &value,
                          ProgInfoList::VisibleLevel level)
{
    if (value.isEmpty())
        return;
    ProgInfoList::DataItem item = std::make_tuple(title, value, level);
    m_data.append(item);
}

bool ProgDetails::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "INFO" || action == "SELECT")
        {
            m_infoList.Toggle();
            updatePage();
        }
        else if (action == "DOWN")
        {
            m_infoList.PageDown();
        }
        else if (action == "UP")
        {
            m_infoList.PageUp();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ProgDetails::PowerPriorities(const QString & ptable)
{
    int      recordid = m_progInfo.GetRecordingRuleID();

    // If not a scheduled recording, abort .... ?
    if (!recordid)
        return;

    using string_pair = QPair<QString, QString>;
    QVector<string_pair> tests;
    QString  recmatch;
    QString  pwrpri;
    QString  desc;
    QString  adjustmsg;
    int      total = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    int prefinputpri    = gCoreContext->GetNumSetting("PrefInputPriority", 2);
    int hdtvpriority    = gCoreContext->GetNumSetting("HDTVRecPriority", 0);
    int wspriority      = gCoreContext->GetNumSetting("WSRecPriority", 0);
    int slpriority      = gCoreContext->GetNumSetting("SignLangRecPriority", 0);
    int onscrpriority   = gCoreContext->GetNumSetting("OnScrSubRecPriority", 0);
    int ccpriority      = gCoreContext->GetNumSetting("CCRecPriority", 0);
    int hhpriority      = gCoreContext->GetNumSetting("HardHearRecPriority", 0);
    int adpriority      = gCoreContext->GetNumSetting("AudioDescRecPriority", 0);

    tests.append(qMakePair(QString("channel.recpriority"),
                           QString("channel.recpriority")));
    tests.append(qMakePair(QString("capturecard.recpriority"),
                           QString("capturecard.recpriority")));

    if (prefinputpri)
    {
        pwrpri = QString("(capturecard.cardid = record.prefinput) * %1")
              .arg(prefinputpri);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (hdtvpriority)
    {
        pwrpri = QString("(program.hdtv > 0 OR "
                      "FIND_IN_SET('HDTV', program.videoprop) > 0) * %1")
              .arg(hdtvpriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (wspriority)
    {
        pwrpri = QString
                 ("(FIND_IN_SET('WIDESCREEN', program.videoprop) > 0) * %1")
                 .arg(wspriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (slpriority)
    {
        pwrpri = QString
                 ("(FIND_IN_SET('SIGNED', program.subtitletypes) > 0) * %1")
                 .arg(slpriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (onscrpriority)
    {
        pwrpri = QString
              ("(FIND_IN_SET('ONSCREEN', program.subtitletypes) > 0) * %1")
              .arg(onscrpriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (ccpriority)
    {
        pwrpri = QString
              ("(FIND_IN_SET('NORMAL', program.subtitletypes) > 0 OR "
               "program.closecaptioned > 0 OR program.subtitled > 0) * %1")
              .arg(ccpriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (hhpriority)
    {
        pwrpri = QString
                 ("(FIND_IN_SET('HARDHEAR', program.subtitletypes) > 0 OR "
                  "FIND_IN_SET('HARDHEAR', program.audioprop) > 0) * %1")
                 .arg(hhpriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }
    if (adpriority)
    {
        pwrpri = QString
              ("(FIND_IN_SET('VISUALIMPAIR', program.audioprop) > 0) * %1")
              .arg(adpriority);
        tests.append(qMakePair(pwrpri, pwrpri));
    }

    query.prepare("SELECT recpriority, selectclause FROM powerpriority;");

    if (!query.exec())
    {
        MythDB::DBError("Power Priority", query);
        return;
    }

    while (query.next())
    {
        int adj = query.value(0).toInt();
        if (adj)
        {
            QString sclause = query.value(1).toString();
            sclause.remove(RecordingInfo::kReLeadingAnd);
            sclause.remove(';');
            pwrpri = QString("(%1) * %2").arg(sclause)
                                            .arg(query.value(0).toInt());
            pwrpri.replace("RECTABLE", "record");

            desc = pwrpri;
            pwrpri += QString(" AS powerpriority ");

            tests.append(qMakePair(desc, pwrpri));
        }
    }

    recmatch = QString("INNER JOIN record "
                       "      ON ( record.recordid = %1 ) ")
        .arg(recordid);

    for (const auto & [label, csqlStart] : std::as_const(tests))
    {
        QString sqlStart = csqlStart;
        query.prepare("SELECT " + sqlStart.replace("program.", "p.")
                      + QString
                      (" FROM %1 as p "
                       "INNER JOIN channel "
                       "      ON ( channel.chanid = p.chanid ) "
                       "INNER JOIN capturecard "
                       "      ON ( channel.sourceid = capturecard.sourceid AND "
                       "           ( capturecard.schedorder <> 0 OR "
                       "             capturecard.parentid = 0 ) ) ").arg(ptable)
                       + recmatch +
                       "WHERE  p.chanid = :CHANID AND"
                       " p.starttime = :STARTTIME ;");

        query.bindValue(":CHANID",    m_progInfo.GetChanID());
        query.bindValue(":STARTTIME", m_progInfo.GetScheduledStartTime());

        adjustmsg = QString("%1 : ").arg(label);
        if (query.exec() && query.next())
        {
            int adj = query.value(0).toInt();
            if (adj)
            {
                adjustmsg += tr(" MATCHED, adding %1").arg(adj);
                total += adj;
            }
            else
            {
                adjustmsg += tr(" not matched");
            }
        }
        else
        {
            adjustmsg += tr(" Query FAILED");
        }

        addItem(tr("Recording Priority Adjustment"), adjustmsg,
                ProgInfoList::kLevel2);
    }

    if (!tests.isEmpty())
        addItem(tr("Priority Adjustment Total"), QString::number(total),
                ProgInfoList::kLevel2);
}

void ProgDetails::loadPage(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString category_type;
    QString showtype;
    QString year;
    QString syndicatedEpisodeNum;
    QString rating;
    QString colorcode;
    QString title_pronounce;
    float stars = 0.0;
    int partnumber = 0;
    int parttotal = 0;
    int audioprop = 0;
    int videoprop = 0;
    int subtype = 0;
    int generic = 0;
    bool recorded = false;

    RecordingRule* record = nullptr;
    if (m_progInfo.GetRecordingRuleID())
    {
        record = new RecordingRule();
        record->LoadByProgram(&m_progInfo);
    }

    if (m_progInfo.GetFilesize())
        recorded = true;

    QString ptable = recorded ? "recordedprogram" : "program";

    if (m_progInfo.GetScheduledEndTime() != m_progInfo.GetScheduledStartTime())
    {
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
            stars = query.value(2).toFloat();
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
        {
            MythDB::DBError("ProgDetails", query);
        }

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

    addItem(tr("Title"),
            m_progInfo.toString(ProgramInfo::kTitleSubtitle, " - "),
            ProgInfoList::kLevel1);

    addItem(tr("Title Pronounce"), title_pronounce, ProgInfoList::kLevel2);

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

    /* see #7810, was hardcoded to 4 star system, when every theme
     * uses 10 stars / 5 stars with half stars
     */
        if (stars > 0.0F)
            attr += tr("%n star(s)", "", roundf(stars * 10.0F)) + ", ";
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
    if  (videoprop & VID_HEVC)
        attr += tr("HEVC/H.265") + ", ";
    if  (videoprop & VID_720)
        attr += tr("720p Resolution") + ", ";

    if (videoprop & VID_PROGRESSIVE)
    {
        if  (videoprop & VID_1080)
             attr += tr("1080p Resolution") + ", ";
        if  (videoprop & VID_4K)
            attr += tr("4K Resolution") + ", ";
    }
    else
    {
        if  (videoprop & VID_1080)
            attr += tr("1080i Resolution") + ", ";
        if  (videoprop & VID_4K)
            attr += tr("4Ki Resolution") + ", ";
    }

    if  (videoprop & VID_DAMAGED)
        attr += tr("Damaged") + ", ";

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

    addItem(tr("Description"), s, ProgInfoList::kLevel1);

    QString actors;
    QString directors;
    QString producers;
    QString execProducers;
    QString writers;
    QString guestStars;
    QString hosts;
    QString adapters;
    QString presenters;
    QString commentators;
    QString guests;

    using string_pair = QPair<QString, QString>;
    QVector<string_pair> actor_list;
    QVector<string_pair> guest_star_list;
    QVector<string_pair> guest_list;

    if (m_progInfo.GetScheduledEndTime() != m_progInfo.GetScheduledStartTime())
    {
        QString table;
        if (recorded)
            table = "recordedcredits";
        else
            table = "credits";

        query.prepare(QString("SELECT role, people.name, roles.name FROM %1"
                              " AS credits"
                              " LEFT JOIN people ON"
                              "  credits.person = people.person"
                              " LEFT JOIN roles ON"
                              "  credits.roleid = roles.roleid"
                              " WHERE credits.chanid = :CHANID"
                              " AND credits.starttime = :STARTTIME"
                              " ORDER BY role, priority;").arg(table));

        query.bindValue(":CHANID",    m_progInfo.GetChanID());
        query.bindValue(":STARTTIME", m_progInfo.GetScheduledStartTime());

        if (query.exec() && query.size() > 0)
        {
            QStringList plist;
            QString rstr;
            QString role;
            QString pname;
            QString character;

            while(query.next())
            {
                role = query.value(0).toString();
                /* The people.name, roles.name columns uses utf8_bin collation.
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
                character = QString::fromUtf8(query.value(2)
                                              .toByteArray().constData());

                if (!character.isEmpty())
                {
                    if (role == "actor")
                        actor_list.append(qMakePair(pname, character));
                    else if (role == "guest_star")
                        guest_star_list.append(qMakePair(pname, character));
                    else if (role == "guest")
                        guest_list.append(qMakePair(pname, character));
                }

                if (rstr != role)
                {
                    plist.removeDuplicates();
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
            plist.removeDuplicates();
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
    addItem(tr("Actors"), actors, ProgInfoList::kLevel1);
    addItem(tr("Guest Star"), guestStars, ProgInfoList::kLevel1);
    addItem(tr("Guest"), guests, ProgInfoList::kLevel1);

    if (!actor_list.isEmpty())
    {
        for (const auto & [actor, role] : std::as_const(actor_list))
            addItem(role, actor, ProgInfoList::kLevel2);
    }
    if (!guest_star_list.isEmpty())
    {
        for (const auto & [actor, role] : std::as_const(guest_star_list))
            addItem(role, actor, ProgInfoList::kLevel2);
    }
    if (!guest_list.isEmpty())
    {
        for (const auto & [actor, role] : std::as_const(guest_list))
            if (!role.isEmpty())
                addItem(role, actor, ProgInfoList::kLevel2);
    }

    addItem(tr("Host"), hosts, ProgInfoList::kLevel1);
    addItem(tr("Presenter"), presenters, ProgInfoList::kLevel1);
    addItem(tr("Commentator"), commentators, ProgInfoList::kLevel1);
    addItem(tr("Director"), directors, ProgInfoList::kLevel1);
    addItem(tr("Producer"), producers, ProgInfoList::kLevel2);
    addItem(tr("Executive Producer"), execProducers, ProgInfoList::kLevel2);
    addItem(tr("Writer"), writers, ProgInfoList::kLevel2);
    addItem(tr("Adapter"), adapters, ProgInfoList::kLevel2);

    addItem(tr("Category"), m_progInfo.GetCategory(), ProgInfoList::kLevel1);

    query.prepare("SELECT genre FROM programgenres "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                  "AND relevance <> '0' ORDER BY relevance;");
    query.bindValue(":CHANID",    m_progInfo.GetChanID());
    query.bindValue(":STARTTIME", m_progInfo.GetScheduledStartTime());

    if (query.exec())
    {
        s.clear();
        while (query.next())
        {
            if (!s.isEmpty())
                s += ", ";
            s += query.value(0).toString();
        }
        addItem(tr("Genre"), s, ProgInfoList::kLevel1);
    }

    s.clear();
    if (!category_type.isEmpty())
    {
        s = category_type;
        if (!m_progInfo.GetSeriesID().isEmpty())
            s += "  (" + m_progInfo.GetSeriesID() + ")";
        if (!showtype.isEmpty())
            s += "  " + showtype;
    }
    addItem(tr("Type", "category_type"), s, ProgInfoList::kLevel1);

    s.clear();
    if (m_progInfo.GetSeason() > 0)
        s = QString::number(m_progInfo.GetSeason());
    addItem(tr("Season"), s, ProgInfoList::kLevel1);

    s.clear();
    if (m_progInfo.GetEpisode() > 0)
    {
        if (m_progInfo.GetEpisodeTotal() > 0)
        {
            s = tr("%1 of %2").arg(m_progInfo.GetEpisode())
                              .arg(m_progInfo.GetEpisodeTotal());
        }
        else
        {
            s = QString::number(m_progInfo.GetEpisode());
        }
    }
    addItem(tr("Episode"), s, ProgInfoList::kLevel1);

    addItem(tr("Syndicated Episode Number"), syndicatedEpisodeNum,
            ProgInfoList::kLevel1);

    s.clear();
    if (m_progInfo.GetOriginalAirDate().isValid() &&
        category_type != "movie")
    {
        s = MythDate::toString(m_progInfo.GetOriginalAirDate(),
                               MythDate::kDateFull | MythDate::kAddYear);
    }
    addItem(tr("Original Airdate"), s, ProgInfoList::kLevel1);

    addItem(tr("Program ID"), m_progInfo.GetProgramID(), ProgInfoList::kLevel1);

    // Begin MythTV information not found in the listings info
    QDateTime statusDate;
    if (m_progInfo.GetRecordingStatus() == RecStatus::WillRecord ||
        m_progInfo.GetRecordingStatus() == RecStatus::Pending)
        statusDate = m_progInfo.GetScheduledStartTime();

    RecordingType rectype = kSingleRecord; // avoid kNotRecording
    RecStatus::Type recstatus = m_progInfo.GetRecordingStatus();

    if (recstatus == RecStatus::PreviousRecording ||
        recstatus == RecStatus::NeverRecord ||
        recstatus == RecStatus::Unknown)
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
            if (recstatus == RecStatus::Unknown)
                recstatus = RecStatus::Type(query.value(0).toInt());

            if (recstatus == RecStatus::PreviousRecording ||
                recstatus == RecStatus::NeverRecord ||
                recstatus == RecStatus::Recorded)
            {
                statusDate = MythDate::as_utc(query.value(1).toDateTime());
            }
        }
    }

    if (recstatus == RecStatus::Unknown)
    {
        if (recorded)
        {
            recstatus = RecStatus::Recorded;
            statusDate = m_progInfo.GetScheduledStartTime();
        }
        else
        {
            // re-enable "Not Recording" status text
            rectype = m_progInfo.GetRecordingRuleType();
        }
    }

    s = RecStatus::toString(recstatus, rectype);

    if (statusDate.isValid())
        s += " " + MythDate::toString(statusDate, MythDate::kDateFull |
                                      MythDate::kAddYear);

    addItem(tr("MythTV Status"), s, ProgInfoList::kLevel1);

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
        if (record && !record->m_title.isEmpty())
            recordingRule += QString(" \"%2\"").arg(record->m_title);

        query.prepare("SELECT last_record, next_record, avg_delay "
                      "FROM record WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", m_progInfo.GetRecordingRuleID());

        if (query.exec() && query.next())
        {
            if (query.value(0).toDateTime().isValid())
            {
                lastRecorded = MythDate::toString(
                    MythDate::as_utc(query.value(0).toDateTime()),
                    MythDate::kDateFull | MythDate::kAddYear);
            }
            if (query.value(1).toDateTime().isValid())
            {
                nextRecording = MythDate::toString(
                    MythDate::as_utc(query.value(1).toDateTime()),
                    MythDate::kDateFull | MythDate::kAddYear);
            }
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
        if (record && record->m_searchType != kManualSearch &&
            record->m_description != m_progInfo.GetDescription())
            searchPhrase = record->m_description;
    }
    addItem(tr("Recording Rule"), recordingRule, ProgInfoList::kLevel1);
    addItem(tr("Search Phrase"), searchPhrase, ProgInfoList::kLevel1);

    s.clear();
    if (m_progInfo.GetFindID())
    {
        QDateTime fdate(QDate(1970, 1, 1),QTime(12,0,0));
        fdate = fdate.addDays((int)m_progInfo.GetFindID() - 719528);
        s = QString("%1 (%2)").arg(m_progInfo.GetFindID())
            .arg(MythDate::toString(
                     fdate, MythDate::kDateFull | MythDate::kAddYear));
    }
    addItem(tr("Find ID"), s, ProgInfoList::kLevel2);

    addItem(tr("Last Recorded"), lastRecorded, ProgInfoList::kLevel2);
    addItem(tr("Next Recording"), nextRecording, ProgInfoList::kLevel2);
    addItem(tr("Average Time Shift"), averageTimeShift, ProgInfoList::kLevel2);
    addItem(tr("Watch List Score"), watchListScore, ProgInfoList::kLevel2);
    addItem(tr("Watch List Status"), watchListStatus, ProgInfoList::kLevel2);

    QString recordingHost;
    QString recordingInput;
    QString recordedID;
    QString recordedPathname;
    QString recordedFilename;
    QString recordedFileSize;
    QString recordingGroup;
    QString storageGroup;
    QString playbackGroup;
    QString recordingProfile;

    recordingHost = m_progInfo.GetHostname();
    recordingInput = m_progInfo.GetInputName();

    if (recorded)
    {
        recordedID = QString::number(m_progInfo.GetRecordingID());
        recordedPathname = m_progInfo.GetPathname();
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
            recordingProfile = ProgramInfo::i18n(query.value(0).toString());
        }
        recordingGroup = ProgramInfo::i18n(m_progInfo.GetRecordingGroup());
        storageGroup   = ProgramInfo::i18n(m_progInfo.GetStorageGroup());
        playbackGroup  = ProgramInfo::i18n(m_progInfo.GetPlaybackGroup());
    }
    else if (m_progInfo.GetRecordingRuleID())
    {
        recordingProfile = record ? record->m_recProfile : tr("Unknown");
    }
    addItem(tr("Recording Host"), recordingHost, ProgInfoList::kLevel2);
    addItem(tr("Recording Input"), recordingInput, ProgInfoList::kLevel2);
    addItem(tr("Recorded ID"), recordedID, ProgInfoList::kLevel2);
    addItem(tr("Recorded Pathname"), recordedPathname, ProgInfoList::kLevel2);
    addItem(tr("Recorded File Name"), recordedFilename, ProgInfoList::kLevel1);
    addItem(tr("Recorded File Size"), recordedFileSize, ProgInfoList::kLevel1);
    addItem(tr("Recording Profile"), recordingProfile, ProgInfoList::kLevel2);
    addItem(tr("Recording Group"), recordingGroup, ProgInfoList::kLevel1);
    addItem(tr("Storage Group"), storageGroup, ProgInfoList::kLevel2);
    addItem(tr("Playback Group"),  playbackGroup, ProgInfoList::kLevel2);

    PowerPriorities(ptable);

    delete record;
}
