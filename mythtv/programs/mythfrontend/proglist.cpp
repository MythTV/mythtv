
#include "proglist.h"

// C/C++
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

// Qt
#include <QCoreApplication>
#include <QRegExp>

// MythTV
#include "mythcontext.h"
#include "remoteutil.h"
#include "scheduledrecording.h"
#include "recordingrule.h"
#include "channelutil.h"
#include "recordinginfo.h"
#include "mythdb.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"

// MythFrontend
#include "customedit.h"
#include "scheduleeditor.h"

ProgLister::ProgLister(MythScreenStack *parent, ProgListType pltype,
               const QString &view, const QString &from)
          : ScheduleCommon(parent, "ProgLister")
{
    m_type = pltype;
    m_addTables = from;
    m_startTime = QDateTime::currentDateTime();
    m_searchTime = m_startTime;

    m_dayFormat = gContext->GetSetting("DateFormat");
    m_hourFormat = gContext->GetSetting("TimeFormat");
    m_fullDateFormat = m_dayFormat + ' ' + m_hourFormat;
    m_channelOrdering = gContext->GetSetting("ChannelOrdering", "channum");
    m_channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

    m_schedText = m_curviewText = m_positionText = m_messageText = NULL;
    m_positionText = NULL;
    m_progList = NULL;

    switch (pltype)
    {
        case plTitleSearch:   m_searchType = kTitleSearch;   break;
        case plKeywordSearch: m_searchType = kKeywordSearch; break;
        case plPeopleSearch:  m_searchType = kPeopleSearch;  break;
        case plPowerSearch:   m_searchType = kPowerSearch;   break;
        case plSQLSearch:     m_searchType = kPowerSearch;   break;
        case plStoredSearch:  m_searchType = kPowerSearch;   break;
        default:              m_searchType = kNoSearch;      break;
    }

    m_allowEvents = true;
    m_titleSort = false;
    m_reverseSort = false;
    m_useGenres = false;

    m_curView = -1;
    m_view = view;
    m_view.detach();
}

// previously recorded ctor
ProgLister::ProgLister(MythScreenStack *parent, int recid, const QString &title)
          : ScheduleCommon(parent, "PreviousList")
{
    m_type = plPreviouslyRecorded;
    m_recid = recid;
    m_title = title;

    m_addTables.clear();
    m_startTime = QDateTime::currentDateTime();
    m_searchTime = m_startTime;

    m_dayFormat = gContext->GetSetting("DateFormat");
    m_hourFormat = gContext->GetSetting("TimeFormat");
    m_timeFormat = gContext->GetSetting("ShortDateFormat") + ' ' + m_hourFormat;
    m_fullDateFormat = m_dayFormat + ' ' + m_hourFormat;
    m_channelOrdering = gContext->GetSetting("ChannelOrdering", "channum");
    m_channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

    m_schedText = m_curviewText = m_positionText = m_messageText = NULL;
    m_positionText = NULL;
    m_progList = NULL;
    m_searchType = kNoSearch;

    m_allowEvents = true;
    m_titleSort = false;
    m_reverseSort = true;
    m_useGenres = false;

    m_curView = -1;
    m_view = "reverse time";
}

ProgLister::~ProgLister()
{
    m_itemList.clear();
    gContext->removeListener(this);
}

bool ProgLister::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "programlist", this))
        return false;

    bool err = false;
    UIUtilW::Assign(this, m_curviewText, "curview", &err);
    UIUtilE::Assign(this, m_progList, "proglist", &err);
    UIUtilW::Assign(this, m_schedText, "sched", &err);
    UIUtilW::Assign(this, m_messageText, "msg", &err);
    UIUtilW::Assign(this, m_positionText, "position", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'programlist'");
        return false;
    }

    connect(m_progList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));

    connect(m_progList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(select()));

    m_progList->SetLCDTitles(tr("Program List"), "title|channel|shortstarttimedate");

    BuildFocusList();

    QString value;
    switch (m_type)
    {
        case plTitle: value = tr("Program Listings"); break;
        case plNewListings: value = tr("New Title Search"); break;
        case plTitleSearch: value = tr("Title Search"); break;
        case plKeywordSearch: value = tr("Keyword Search"); break;
        case plPeopleSearch: value = tr("People Search"); break;
        case plStoredSearch: value = tr("Stored Search"); break;
        case plPowerSearch: value = tr("Power Search"); break;
        case plSQLSearch: value = tr("Power Search"); break;
        case plRecordid: value = tr("Rule Search"); break;
        case plCategory: value = tr("Category Search"); break;
        case plChannel: value = tr("Channel Search"); break;
        case plMovies: value = tr("Movie Search"); break;
        case plTime: value = tr("Time Search"); break;
        case plPreviouslyRecorded: value = tr("Previously Recorded"); break;
        default: value = tr("Unknown Search"); break;
    }

    if (m_schedText)
        m_schedText->SetText(value);

    gContext->addListener(this);

    LoadInBackground();

    return true;
}

void ProgLister::Load(void)
{
    if (m_curView < 0)
        fillViewList(m_view);

    fillItemList(false, false);

    ScreenLoadCompletionEvent *slce =
        new ScreenLoadCompletionEvent(objectName());
    QCoreApplication::postEvent(this, slce);
}

bool ProgLister::keyPressEvent(QKeyEvent *e)
{
    if (!m_allowEvents)
        return true;

    m_allowEvents = false;

    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(e))
    {
        m_allowEvents = true;
        return true;
    }

    bool handled = false;
    bool needUpdate = false;

    QStringList actions;
    handled = gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        if (action == "PREVVIEW")
            prevView();
        else if (action == "NEXTVIEW")
            nextView();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "EDIT")
            edit();
        else if (action == "DELETE")
            deleteItem();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS" || action == "INFO")
            details();
        else if (action == "TOGGLERECORD")
            quickRecord();
        else if (action == "1")
        {
            if (m_titleSort == true)
            {
                m_titleSort = false;
                m_reverseSort = (m_type == plPreviouslyRecorded);
            }
            else
            {
                m_reverseSort = !m_reverseSort;
            }
            needUpdate = true;
        }
        else if (action == "2")
        {
            if (m_titleSort == false)
            {
                m_titleSort = true;
                m_reverseSort = false;
            }
            else
            {
                m_reverseSort = !m_reverseSort;
            }
            needUpdate = true;
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(e))
        handled = true;

    if (needUpdate)
        LoadInBackground();

    m_allowEvents = true;

    return handled;
}

void ProgLister::ShowMenu(void)
{
    QString label = tr("Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "menu");
        if (m_type != plPreviouslyRecorded)
            menuPopup->AddButton(tr("Choose Search Phrase..."));
        menuPopup->AddButton(tr("Sort"), NULL, true);
        if (m_type != plPreviouslyRecorded)
            menuPopup->AddButton(tr("Record"));
        menuPopup->AddButton(tr("Edit Schedule"));
        menuPopup->AddButton(tr("Program Details"));
        menuPopup->AddButton(tr("Upcoming"));
        menuPopup->AddButton(tr("Custom Edit"));

        ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];
        if (m_type != plPreviouslyRecorded)
        {
            if (pi && pi->recordid > 0)
                menuPopup->AddButton(tr("Delete Rule"));
        }
        else
            menuPopup->AddButton(tr("Delete Episode"));

        menuPopup->AddButton(tr("Cancel"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgLister::showSortMenu(void)
{
    QString label = tr("Sort Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "sortmenu");

        menuPopup->AddButton(tr("Reverse Sort Order"));
        menuPopup->AddButton(tr("Sort By Title"));
        menuPopup->AddButton(tr("Sort By Time"));
        menuPopup->AddButton(tr("Cancel"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgLister::prevView(void)
{
    if (m_type == plTime)
    {
        m_searchTime = m_searchTime.addSecs(-3600);
        m_curView = 0;
        m_viewList[m_curView] = m_searchTime.toString(m_fullDateFormat);
        m_viewTextList[m_curView] = m_viewList[m_curView];
        LoadInBackground();

        return;
    }

    if (m_viewList.count() < 2)
        return;

    m_curView--;
    if (m_curView < 0)
        m_curView = m_viewList.count() - 1;

    LoadInBackground();
}

void ProgLister::nextView(void)
{
    if (m_type == plTime)
    {
        m_searchTime = m_searchTime.addSecs(3600);
        m_curView = 0;
        m_viewList[m_curView] = m_searchTime.toString(m_fullDateFormat);
        m_viewTextList[m_curView] = m_viewList[m_curView];
        LoadInBackground();

        return;
    }
    if (m_viewList.count() < 2)
        return;

    m_curView++;
    if (m_curView >= (int)m_viewList.count())
        m_curView = 0;

    LoadInBackground();
}

void ProgLister::updateKeywordInDB(const QString &text, const QString &oldValue)
{
    int oldview = m_viewList.indexOf(oldValue);
    int newview = m_viewList.indexOf(text);

    if (newview < 0 || newview != oldview)
    {
        QString qphrase;
        if (oldview >= 0)
        {
            qphrase = m_viewList[oldview];

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM keyword "
                          "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
            query.bindValue(":PHRASE", qphrase);
            query.bindValue(":TYPE", m_searchType);
            if (!query.exec())
                MythDB::DBError("ProgLister::updateKeywordInDB -- delete",
                                query);

            m_viewList.removeAll(qphrase);
            m_viewTextList.removeAll(qphrase);
        }
        if (newview < 0)
        {
            qphrase = text;

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("REPLACE INTO keyword (phrase, searchtype)"
                          "VALUES(:PHRASE, :TYPE );");
            query.bindValue(":PHRASE", qphrase);
            query.bindValue(":TYPE", m_searchType);
            if (!query.exec())
                MythDB::DBError("ProgLister::updateKeywordInDB -- replace",
                                query);

            m_viewList.append(qphrase);
            m_viewTextList.append(qphrase);
        }
    }
}

void ProgLister::chooseView(void)
{
    if (m_type == plChannel || m_type == plCategory || m_type == plMovies ||
        m_type == plNewListings || m_type == plStoredSearch)
    {
        if (m_viewList.count() < 1)
            return;

        QString msg;
        switch (m_type)
        {
            case plMovies: msg = tr("Select Rating"); break;
            case plChannel: msg = tr("Select Channel"); break;
            case plCategory: msg = tr("Select Category"); break;
            case plNewListings: msg = tr("Select List"); break;
            case plStoredSearch: msg = QString("%1\n%2")
                    .arg(tr("Select a search stored from"))
                    .arg(tr("Custom Record")); break;
            default: msg = tr("Select"); break;
        }

        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
        MythUISearchDialog *searchDlg =
                new MythUISearchDialog(popupStack, msg, m_viewTextList, true, "");

        if (!searchDlg->Create())
        {
            delete searchDlg;
            return;
        }

        connect(searchDlg, SIGNAL(haveResult(QString)), SLOT(setViewFromList(QString)));

        popupStack->AddScreen(searchDlg);

        return;
    }
    else if (m_type == plTitleSearch || m_type == plKeywordSearch ||
             m_type == plPeopleSearch)
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
        QString currentItem;

        if (m_curView >= 0)
            currentItem = m_viewList[m_curView];

        PhrasePopup *popup = new PhrasePopup(popupStack, this, m_searchType,
                                             m_viewTextList, currentItem);

        if (!popup->Create())
        {
            delete popup;
            return;
        }

        connect(popup, SIGNAL(haveResult(QString)), SLOT(setViewFromList(QString)));

        popupStack->AddScreen(popup);

        return;
    }
    else if (m_type == plPowerSearch)
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
        QString currentItem;

        if (m_curView >= 0)
            currentItem = m_viewList[m_curView];

        PowerSearchPopup *popup = new PowerSearchPopup(popupStack, this, m_searchType,
                                                       m_viewTextList, currentItem);

        if (!popup->Create())
        {
            delete popup;
            return;
        }

        connect(popup, SIGNAL(haveResult(QString)), SLOT(setViewFromList(QString)));

        popupStack->AddScreen(popup);
    }
    else if (m_type == plTime)
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
        QString currentItem;

        TimePopup *popup = new TimePopup(popupStack, this);

        if (!popup->Create())
        {
            delete popup;
            return;
        }

        connect(popup, SIGNAL(haveResult(QDateTime)), SLOT(setViewFromTime(QDateTime)));

        popupStack->AddScreen(popup);
    }
}

void ProgLister::setViewFromTime(QDateTime searchTime)
{
    m_searchTime = searchTime;
    m_curView = 0;
    m_viewList[m_curView] = m_searchTime.toString(m_fullDateFormat);
    m_viewTextList[m_curView] = m_viewList[m_curView];

    LoadInBackground();
}

void ProgLister::setViewFromList(QString item)
{
    m_curView = m_viewTextList.indexOf(item);
    LoadInBackground();
}

bool ProgLister::powerStringToSQL(const QString &qphrase, QString &output,
                                  MSqlBindings &bindings)
{
    int ret = 0;
    output.clear();
    QString curfield;

    QStringList field = qphrase.split(':');

    if (field.count() != 6)
    {
        VERBOSE(VB_IMPORTANT, QString("Error. PowerSearch %1 has %2 fields")
                .arg(qphrase).arg(field.count()));
        return ret;
    }

    if (!field[0].isEmpty())
    {
        curfield = '%' + field[0] + '%';
        output += "program.title LIKE :POWERTITLE ";
        bindings[":POWERTITLE"] = curfield;
    }

    if (!field[1].isEmpty())
    {
        if (!output.isEmpty())
            output += "\nAND ";

        curfield = '%' + field[1] + '%';
        output += "program.subtitle LIKE :POWERSUB ";
        bindings[":POWERSUB"] = curfield;
    }

    if (!field[2].isEmpty())
    {
        if (!output.isEmpty())
            output += "\nAND ";

        curfield = '%' + field[2] + '%';
        output += "program.description LIKE :POWERDESC ";
        bindings[":POWERDESC"] = curfield;
    }

    if (!field[3].isEmpty())
    {
        if (!output.isEmpty())
            output += "\nAND ";

        output += "program.category_type = :POWERCATTYPE ";
        bindings[":POWERCATTYPE"] = field[3];
    }

    if (!field[4].isEmpty())
    {
        if (!output.isEmpty())
            output += "\nAND ";

        output += "programgenres.genre = :POWERGENRE ";
        bindings[":POWERGENRE"] = field[4];
        ret = 1;
    }

    if (!field[5].isEmpty())
    {
        if (!output.isEmpty())
            output += "\nAND ";

        output += "channel.callsign = :POWERCALLSIGN ";
        bindings[":POWERCALLSIGN"] = field[5];
    }
    return ret;
}

void ProgLister::quickRecord()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    RecordingInfo ri(*pi);
    ri.ToggleRecord();
    *pi = ri;
}

void ProgLister::select()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    if (m_type == plPreviouslyRecorded)
        oldRecordedActions();
    else
        EditRecording(pi);
}

void ProgLister::edit()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    EditScheduled(pi);
}

void ProgLister::customEdit()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    EditCustom(pi);
}

void ProgLister::deleteItem()
{
    if (m_type == plPreviouslyRecorded)
        deleteOldEpisode();
    else
        deleteRule();
}

void ProgLister::deleteRule()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi || pi->recordid <= 0)
        return;

    RecordingRule *record = new RecordingRule();
    if (!record->LoadByProgram(pi))
    {
        delete record;
        return;
    }

    QString message = tr("Delete '%1' %2 rule?").arg(record->m_title)
                                                .arg(pi->RecTypeText());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *okPopup = new MythConfirmationDialog(popupStack,
                                                                 message, true);

    okPopup->SetReturnEvent(this, "deleterule");
    okPopup->SetData(qVariantFromValue(record));

    if (okPopup->Create())
        popupStack->AddScreen(okPopup);
    else
        delete okPopup;
}

void ProgLister::deleteOldEpisode()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    QString message = tr("Delete this episode of '%1'?").arg(pi->title);

    ShowOkPopup(message, this, SLOT(doDeleteOldEpisode(bool)), true);
}

void ProgLister::doDeleteOldEpisode(bool ok)
{
    if (!ok)
        return;

    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM oldrecorded "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", pi->chanid);
    query.bindValue(":STARTTIME", pi->startts.toString(Qt::ISODate));
    if (!query.exec())
        MythDB::DBError("ProgLister::doDeleteOldEpisode", query);

    ScheduledRecording::signalChange(0);
    fillItemList(true);
}

void ProgLister::deleteOldTitle()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    QString message = tr("Delete all episodes of '%1'?").arg(pi->title);

    ShowOkPopup(message, this, SLOT(doDeleteOldTitle(bool)), true);
}

void ProgLister::doDeleteOldTitle(bool ok)
{
    if (!ok)
        return;

    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM oldrecorded WHERE title = :TITLE ;");
    query.bindValue(":TITLE", pi->title);
    if (!query.exec())
        MythDB::DBError("ProgLister::doDeleteOldTitle -- delete", query);

    ScheduledRecording::signalChange(0);
    fillItemList(true);
}

void ProgLister::oldRecordedActions()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi)
        return;

    QString message = pi->title;

    if (!pi->subtitle.isEmpty())
        message += QString(" - \"%1\"").arg(pi->subtitle);

    if (!pi->description.isEmpty())
        message += "\n\n" + pi->description;

    message += "\n\n\n" + tr("NOTE: removing items from this list will not "
               "delete any recordings.");

    QString title = tr("Previously Recorded");
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    MythDialogBox *menuPopup = new MythDialogBox(title, message, mainStack, "deletepopup", true);

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "deletemenu");
        if (pi->duplicate)
            menuPopup->AddButton(tr("Allow this episode to re-record"));
        else
            menuPopup->AddButton(tr("Never record this episode"));
        menuPopup->AddButton(tr("Remove this episode from the list"));
        menuPopup->AddButton(tr("Remove all episodes for this title"));
        menuPopup->AddButton(tr("Cancel"));

        mainStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void ProgLister::upcoming()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    if (!pi || m_type == plTitle)
        return;

    ShowUpcoming(pi);
}

void ProgLister::details()
{
    ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];

    ShowDetails(pi);
}

void ProgLister::fillViewList(const QString &view)
{
    m_viewList.clear();
    m_viewTextList.clear();

    if (m_type == plChannel) // list by channel
    {
        DBChanList channels = ChannelUtil::GetChannels(0, true,
                                                       "channum, chanid");
        ChannelUtil::SortChannels(channels, m_channelOrdering, true);

        for (uint i = 0; i < channels.size(); ++i)
        {
            QString chantext = m_channelFormat;
            chantext
                .replace("<num>",  channels[i].channum)
                .replace("<sign>", channels[i].callsign)
                .replace("<name>", channels[i].name);

            m_viewList << QString::number(channels[i].chanid);
            m_viewTextList << chantext;
        }

        if (!view.isEmpty())
            m_curView = m_viewList.indexOf(view);
    }
    else if (m_type == plCategory) // list by category
    {
        QString startstr = m_startTime.toString("yyyy-MM-ddThh:mm:50");
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT g1.genre, g2.genre "
                      "FROM program "
                      "JOIN programgenres g1 ON "
                      "  program.chanid = g1.chanid AND "
                      "  program.starttime = g1.starttime "
                      "LEFT JOIN programgenres g2 ON "
                      "  g1.chanid = g2.chanid AND "
                      "  g1.starttime = g2.starttime "
                      "WHERE program.endtime > :PGILSTART "
                      "GROUP BY g1.genre, g2.genre;");
        query.bindValue(":PGILSTART", startstr);

        if (query.exec() && query.size())
        {
            QString lastGenre1, genre1;

            while (query.next())
            {
                genre1 = query.value(0).toString();
                if (genre1.isEmpty())
                    continue;

                if (genre1 != lastGenre1)
                {
                    m_viewList << genre1;
                    m_viewTextList << genre1;
                    lastGenre1 = genre1;
                }

                QString genre2 = query.value(1).toString();
                if (genre2.isEmpty() || genre2 == genre1)
                    continue;

                m_viewList << genre1 + ":/:" + genre2;
                m_viewTextList << "    " + genre1 + " / " + genre2;
            }

            m_useGenres = true;
        }
        else
        {
            query.prepare("SELECT category "
                          "FROM program "
                          "WHERE program.endtime > :PGILSTART "
                          "GROUP BY category;");
            query.bindValue(":PGILSTART", startstr);

            if (query.exec())
            {
                while (query.next())
                {
                    QString category = query.value(0).toString();
                    if (category.isEmpty())
                        continue;
                    category = query.value(0).toString();
                    m_viewList << category;
                    m_viewTextList << category;
                }
            }

            m_useGenres = false;
        }

        if (!view.isEmpty())
            m_curView = m_viewList.indexOf(view);
    }
    else if (m_type == plTitleSearch || m_type == plKeywordSearch ||
             m_type == plPeopleSearch || m_type == plPowerSearch)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT phrase FROM keyword "
                      "WHERE searchtype = :SEARCHTYPE;");
        query.bindValue(":SEARCHTYPE", m_searchType);

        if (query.exec())
        {
            while (query.next())
            {
                /* The keyword.phrase column uses utf8_bin collation, so
                 * Qt uses QString::fromAscii() for toString(). Explicitly
                 * convert the value using QString::fromUtf8() to prevent
                 * corruption. */
                QString phrase = QString::fromUtf8(query.value(0)
                                                   .toByteArray().constData());
                if (phrase.isEmpty())
                    continue;
                m_viewList << phrase;
                m_viewTextList << phrase;
            }
        }
        if (!view.isEmpty())
        {
            m_curView = m_viewList.indexOf(view);

            if (m_curView < 0)
            {
                QString qphrase = view;

                MSqlQuery query(MSqlQuery::InitCon());
                query.prepare("REPLACE INTO keyword (phrase, searchtype)"
                              "VALUES(:VIEW, :SEARCHTYPE );");
                query.bindValue(":VIEW", qphrase);
                query.bindValue(":SEARCHTYPE", m_searchType);
                if (!query.exec())
                    MythDB::DBError("ProgLister::fillViewList -- "
                                    "replace keyword", query);

                m_viewList << qphrase;
                m_viewTextList << qphrase;

                m_curView = m_viewList.count() - 1;
            }
        }
        else
            m_curView = -1;
    }
    else if (m_type == plTitle)
    {
        if (!view.isEmpty())
        {
            m_viewList << view;
            m_viewTextList << view;
            m_curView = 0;
        }
        else
            m_curView = -1;
    }
    else if (m_type == plNewListings)
    {
        m_viewList << "all";
        m_viewTextList << tr("All");

        m_viewList << "premieres";
        m_viewTextList << tr("Premieres");

        m_viewList << "movies";
        m_viewTextList << tr("Movies");

        m_viewList << "series";
        m_viewTextList << tr("Series");

        m_viewList << "specials";
        m_viewTextList << tr("Specials");

        if (!view.isEmpty())
            m_curView = m_viewList.indexOf(view);
    }
    else if (m_type == plMovies)
    {
        m_viewList << ">= 0.0";
        m_viewTextList << tr("All");
        m_viewList << "= 0.0";
        m_viewTextList << tr("Unrated");
        m_viewList << ">= 1.0";
        m_viewTextList << "****";
        m_viewList << ">= 0.875 AND program.stars < 1.0";
        m_viewTextList << "***/";
        m_viewList << ">= 0.75 AND program.stars < 0.875";
        m_viewTextList << "***";
        m_viewList << ">= 0.625 AND program.stars < 0.75";
        m_viewTextList << "**/";
        m_viewList << ">= 0.5 AND program.stars < 0.625";
        m_viewTextList << "**";
        m_viewList << ">= 0.375 AND program.stars < 0.5";
        m_viewTextList << "*/";
        m_viewList << ">= 0.25 AND program.stars < 0.375";
        m_viewTextList << "*";
        m_viewList << ">= 0.125 AND program.stars < 0.25";
        m_viewTextList << "/";
        m_viewList << ">= 0.875";
        m_viewTextList << tr("At least ***/");
        m_viewList << ">= 0.75";
        m_viewTextList << tr("At least ***");
        m_viewList << ">= 0.625";
        m_viewTextList << tr("At least **/");
        m_viewList << ">= 0.5";
        m_viewTextList << tr("At least **");
        m_viewList << ">= 0.375";
        m_viewTextList << tr("At least */");
        m_viewList << ">= 0.25";
        m_viewTextList << tr("At least *");
        m_viewList << ">= 0.125";
        m_viewTextList << tr("At least /");

        if (!view.isEmpty())
            m_curView = m_viewList.indexOf(view);
    }
    else if (m_type == plTime)
    {
        m_curView = 0;
        m_viewList << m_searchTime.toString(m_fullDateFormat);
        m_viewTextList << m_viewList[m_curView];
    }
    else if (m_type == plSQLSearch)
    {
        m_curView = 0;
        m_viewList << view;
        m_viewTextList << tr("Power Recording Rule");
    }
    else if (m_type == plRecordid)
    {
        m_curView = 0;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM record "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", view);

        if (query.exec() && query.next())
        {
                QString title = query.value(0).toString();
                title = query.value(0).toString();
                m_viewList << view;
                m_viewTextList << title;
        }
    }
    else if (m_type == plStoredSearch) // stored searches
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT rulename FROM customexample "
                      "WHERE search > 0 ORDER BY rulename;");

        if (query.exec())
        {
            while (query.next())
            {
                QString rulename = query.value(0).toString();
                if (rulename.isEmpty() || rulename.trimmed().isEmpty())
                    continue;
                rulename = query.value(0).toString();
                m_viewList << rulename;
                m_viewTextList << rulename;
            }
        }
        if (!view.isEmpty())
            m_curView = m_viewList.indexOf(view);
    }
    else if (m_type == plPreviouslyRecorded) // previously recorded
    {
        m_viewList << "sort by time";
        m_viewTextList << tr("Time");

        m_viewList << "reverse time";
        m_viewTextList << tr("Reverse Time");

        m_viewList << "sort by title";
        m_viewTextList << tr("Title");

        m_viewList << "reverse title";
        m_viewTextList << tr("Reverse Title");

        if (!view.isEmpty())
            m_curView = m_viewList.indexOf(view);
    }

    if (m_curView >= (int)m_viewList.count())
        m_curView = m_viewList.count() - 1;
}

class plCompare
{
    public:
        plCompare(void) {};
        virtual bool operator()(const ProgramInfo *a, const ProgramInfo *b)
            = 0;
};

class plTitleSort
    : public plCompare
{
    public:
        plTitleSort(void) : plCompare() {;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b)
        {
            if (a->sortTitle != b->sortTitle)
                    return (a->sortTitle < b->sortTitle);

            if (a->recstatus == b->recstatus)
                return a->startts < b->startts;

            if (a->recstatus == rsRecording) return true;
            if (b->recstatus == rsRecording) return false;

            if (a->recstatus == rsWillRecord) return true;
            if (b->recstatus == rsWillRecord) return false;

            return a->startts < b->startts;
        }
};

class plPrevTitleSort
    : public plCompare
{
    public:
        plPrevTitleSort(void) : plCompare() {;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b)
        {
            if (a->sortTitle != b->sortTitle)
                    return (a->sortTitle < b->sortTitle);

            if (a->programid != b->programid)
                return a->programid < b->programid;

            return a->startts < b->startts;
        }
};

class plTimeSort
    : public plCompare
{
    public:
        plTimeSort(void) : plCompare() {;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b)
        {
            if (a->startts == b->startts)
                return (a->chanid < b->chanid);

            return (a->startts < b->startts);
        }
};

void ProgLister::fillItemList(bool restorePosition, bool updateDisp)
{
    if (m_type == plPreviouslyRecorded && m_curviewText)
    {
        if (!m_titleSort)
        {
            if (!m_reverseSort)
                m_curView = 0;
            else
                m_curView = 1;
        }
        else
        {
            if (!m_reverseSort)
                m_curView = 2;
            else
                m_curView = 3;
        }
    }

    if (m_curView < 0)
         return;

    bool oneChanid = false;
    QString where;
    QString startstr = m_startTime.toString("yyyy-MM-ddThh:mm:50");
    QString qphrase = m_viewList[m_curView];

    MSqlBindings bindings;

    if (m_type != plPreviouslyRecorded)
        bindings[":PGILSTART"] = startstr;

    if (m_type == plTitle) // per title listings
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.title = :PGILPHRASE0 ";
        bindings[":PGILPHRASE0"] = qphrase;
    }
    else if (m_type == plNewListings) // what's new list
    {
        where = "LEFT JOIN oldprogram ON "
                "  oldprogram.oldtitle = program.title "
                "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND oldprogram.oldtitle IS NULL "
                "  AND program.manualid = 0 ";

        if (qphrase == "premieres")
        {
            where += "  AND ( ";
            where += "    ( program.originalairdate=DATE(program.starttime) ";
            where += "      AND (program.category = 'Special' ";
            where += "        OR program.programid LIKE 'EP%0001')) ";
            where += "    OR (program.category_type='movie' ";
            where += "      AND program.stars > 0.5 ";
            where += "      AND program.airdate >= YEAR(NOW()) - 2) ";
            where += "  ) ";
        }
        else if (qphrase == "movies")
        {
            where += "  AND program.category_type = 'movie' ";
        }
        else if (qphrase == "series")
        {
            where += "  AND program.category_type = 'series' ";
        }
        else if (qphrase == "specials")
        {
            where += "  AND program.category_type = 'tvshow' ";
        }
        else
        {
            where += "  AND (program.category_type <> 'movie' ";
            where += "  OR program.airdate >= YEAR(NOW()) - 3) ";
        }
    }
    else if (m_type == plTitleSearch) // keyword search
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.title LIKE :PGILLIKEPHRASE0 ";
        bindings[":PGILLIKEPHRASE0"] = QString("%") + qphrase + '%';
    }
    else if (m_type == plKeywordSearch) // keyword search
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND (program.title LIKE :PGILLIKEPHRASE1 "
                "    OR program.subtitle LIKE :PGILLIKEPHRASE2 "
                "    OR program.description LIKE :PGILLIKEPHRASE3 ) ";
        bindings[":PGILLIKEPHRASE1"] = QString("%") + qphrase + '%';
        bindings[":PGILLIKEPHRASE2"] = QString("%") + qphrase + '%';
        bindings[":PGILLIKEPHRASE3"] = QString("%") + qphrase + '%';
    }
    else if (m_type == plPeopleSearch) // people search
    {
        where = ", people, credits WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND people.name LIKE :PGILPHRASE1 "
                "  AND credits.person = people.person "
                "  AND program.chanid = credits.chanid "
                "  AND program.starttime = credits.starttime";
        bindings[":PGILPHRASE1"] = qphrase;
    }
    else if (m_type == plPowerSearch) // complex search
    {
        QString powerWhere;
        MSqlBindings powerBindings;

        bool genreflag = powerStringToSQL(qphrase, powerWhere, powerBindings);

        if (!powerWhere.isEmpty())
        {
            if (genreflag)
                where = QString("LEFT JOIN programgenres ON "
                        "program.chanid = programgenres.chanid AND "
                        "program.starttime = programgenres.starttime ");

            where += QString("WHERE channel.visible = 1 "
                    "  AND program.endtime > :PGILSTART "
                    "  AND ( ") + powerWhere + " ) ";
            MSqlAddMoreBindings(bindings, powerBindings);
        }
    }
    else if (m_type == plSQLSearch) // complex search
    {
        qphrase.remove(QRegExp("^\\s*AND\\s+", Qt::CaseInsensitive));
        where = QString("WHERE channel.visible = 1 "
                        "  AND program.endtime > :PGILSTART "
                        "  AND ( %1 ) ").arg(qphrase);
        if (!m_addTables.isEmpty())
            where = m_addTables + ' ' + where;
    }
    else if (m_type == plChannel) // list by channel
    {
        oneChanid = true;
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND channel.chanid = :PGILPHRASE2 ";
        bindings[":PGILPHRASE2"] = qphrase;
    }
    else if (m_type == plCategory) // list by category
    {
        if (!m_useGenres)
        {
            where = "WHERE channel.visible = 1 "
                    "  AND program.endtime > :PGILSTART "
                    "  AND program.category = :PGILPHRASE3 ";
            bindings[":PGILPHRASE3"] = qphrase;
        }
        else if (m_viewList[m_curView].indexOf(":/:") < 0)
        {
            where = "JOIN programgenres g ON "
                    "  program.chanid = g.chanid AND "
                    "  program.starttime = g.starttime AND "
                    "  genre = :PGILPHRASE4 "
                    "WHERE channel.visible = 1 "
                    "  AND program.endtime > :PGILSTART ";
            bindings[":PGILPHRASE4"] = qphrase;
        }
        else
        {
            where = "JOIN programgenres g1 ON "
                    "  program.chanid = g1.chanid AND "
                    "  program.starttime = g1.starttime AND "
                    "  g1.genre = :GENRE1 "
                    "JOIN programgenres g2 ON "
                    "  program.chanid = g2.chanid AND "
                    "  program.starttime = g2.starttime AND "
                    "  g2.genre = :GENRE2 "
                    "WHERE channel.visible = 1 "
                    "  AND program.endtime > :PGILSTART ";
            bindings[":GENRE1"] = m_viewList[m_curView].section(":/:", 0, 0);
            bindings[":GENRE2"] = m_viewList[m_curView].section(":/:", 1, 1);
        }
    }
    else if (m_type == plMovies) // list movies
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND program.category_type = 'movie' "
                "  AND program.stars " + qphrase + ' ';
    }
    else if (m_type == plTime) // list by time
    {
        bindings[":PGILSEARCHTIME1"] = m_searchTime.toString("yyyy-MM-dd hh:00:00");
        where = "WHERE channel.visible = 1 "
                "  AND program.starttime >= :PGILSEARCHTIME1 ";
        if (m_titleSort)
        {
            where += "  AND program.starttime < DATE_ADD(:PGILSEARCHTIME2, "
                     "INTERVAL '1' HOUR) ";
            bindings[":PGILSEARCHTIME2"] = bindings[":PGILSEARCHTIME1"];
        }
    }
    else if (m_type == plRecordid) // list by recordid
    {
        where = "JOIN recordmatch ON "
                " (program.starttime = recordmatch.starttime "
                "  AND program.chanid = recordmatch.chanid) "
                "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND recordmatch.recordid = :PGILPHRASE5 ";
        bindings[":PGILPHRASE5"] = qphrase;
    }
    else if (m_type == plStoredSearch) // stored search
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT fromclause, whereclause FROM customexample "
                      "WHERE rulename = :RULENAME;");
        query.bindValue(":RULENAME", qphrase);

        if (query.exec() && query.next())
        {
            QString fromc = query.value(0).toString();
            QString wherec = query.value(1).toString();

            where = QString("WHERE channel.visible = 1 "
                            "  AND program.endtime > :PGILSTART "
                            "  AND ( %1 ) ").arg(wherec);
            if (!fromc.isEmpty())
                where = fromc + ' ' + where;
        }
    }
    else if (m_type == plPreviouslyRecorded)
    {
        if (m_recid > 0 && !m_title.isEmpty())
        {
            where = QString("WHERE recordid = %1 OR title = :MTITLE ").arg(m_recid);
            bindings[":MTITLE"] = m_title;
        }
        else if (!m_title.isEmpty())
        {
            where = QString("WHERE title = :MTITLE ");
            bindings[":MTITLE"] = m_title;
        }
        else if (m_recid > 0)
        {
            where = QString("WHERE recordid = %1 ").arg(m_recid);
        }
    }

    if (m_type == plPreviouslyRecorded)
    {
        LoadFromOldRecorded(m_itemList, where, bindings);
    }
    else
    {
        LoadFromScheduler(m_schedList);
        LoadFromProgram(m_itemList, where, bindings, m_schedList, oneChanid);
    }

    ProgramInfo *s;
    vector<ProgramInfo *> sortedList;
    const QRegExp prefixes("^(The |A |An )");

    while (!m_itemList.empty())
    {
        s = m_itemList.take(0);
        if (m_type == plTitle)
            s->sortTitle = s->subtitle;
        else
            s->sortTitle = s->title;

        s->sortTitle.remove(prefixes);
        sortedList.push_back(s);
    }

    if (m_type == plNewListings || m_titleSort)
    {
        if (m_type == plPreviouslyRecorded)
            sort(sortedList.begin(), sortedList.end(), plPrevTitleSort());
        else
        {
            // Prune to one per title
            sort(sortedList.begin(), sortedList.end(), plTitleSort());

            QString curtitle;
            vector<ProgramInfo *>::iterator i = sortedList.begin();
            while (i != sortedList.end())
            {
                ProgramInfo *p = *i;
                if (p->sortTitle != curtitle)
                {
                    curtitle = p->sortTitle;
                    i++;
                }
                else
                {
                    delete p;
                    i = sortedList.erase(i);
                }
            }
        }
    }

    if (!m_titleSort)
        sort(sortedList.begin(), sortedList.end(), plTimeSort());

    if (m_reverseSort)
    {
        vector<ProgramInfo *>::reverse_iterator r = sortedList.rbegin();
        for (; r != sortedList.rend(); r++)
            m_itemList.push_back(*r);
    }
    else
    {
        vector<ProgramInfo *>::iterator i = sortedList.begin();
        for (; i != sortedList.end(); ++i)
            m_itemList.push_back(*i);
    }

    if (updateDisp)
        updateDisplay(restorePosition);
}

void ProgLister::updateDisplay(bool restorePosition)
{
    if (m_messageText)
        m_messageText->SetVisible(m_itemList.empty());

    InfoMap infoMap;
    ProgramInfo pginfo;
    pginfo.ToMap(infoMap);
    ResetMap(infoMap);

    MythUIButtonListItem *currentItem = m_progList->GetItemCurrent();
    ProgramInfo *selected = NULL;
    if (currentItem)
        selected = new ProgramInfo(*(qVariantValue<ProgramInfo*>
                                        (currentItem->GetData())));
    int selectedOffset =
        m_progList->GetCurrentPos() - m_progList->GetTopItemPos();

    if (m_curviewText && m_curView >= 0)
        m_curviewText->SetText(m_viewTextList[m_curView]);

    if (m_positionText)
        m_positionText->Reset();

    updateButtonList();

    // Restore position after a list update
    if (restorePosition && selected)
    {
        plCompare *comp;
        if (!m_titleSort)
            comp = new plTimeSort();
        else if (m_type == plPreviouslyRecorded)
            comp = new plPrevTitleSort();
        else
            comp = new plTitleSort();

        int i;
        for (i = m_itemList.size() - 2; i >= 0; i--)
        {
            bool dobreak;
            if (m_reverseSort)
                dobreak = comp->operator()(selected, m_itemList[i]);
            else
                dobreak = comp->operator()(m_itemList[i], selected);
            if (dobreak)
                break;
        }

        delete comp;

        m_progList->SetItemCurrent(i + 1, i + 1 - selectedOffset);
    }

    delete selected;
}

void ProgLister::updateButtonList(void)
{
    m_progList->Reset();

    QString state;
    InfoMap infoMap;

    ProgramList::const_iterator it = m_itemList.begin();
    for (; it != m_itemList.end(); ++it)
    {
        ProgramInfo *pginfo = new ProgramInfo;
        pginfo->clone(*(*it)); // deep copy since we reload in the background

        if (pginfo->recstatus == rsRecorded          ||
            pginfo->recstatus == rsWillRecord)
            state = "normal";
        else if (pginfo->recstatus == rsRecording)
            state = "running";
        else if (pginfo->recstatus == rsConflict     ||
                 pginfo->recstatus == rsOffLine      ||
                 pginfo->recstatus == rsTunerBusy    ||
                 pginfo->recstatus == rsFailed       ||
                 pginfo->recstatus == rsAborted)
            state = "error";
        else if (m_type == plPreviouslyRecorded      ||
                 pginfo->recstatus == rsRepeat       ||
                 pginfo->recstatus == rsOtherShowing ||
                 pginfo->recstatus == rsNeverRecord  ||
                 pginfo->recstatus == rsDontRecord   ||
                 (pginfo->recstatus != rsDontRecord &&
                  pginfo->recstatus <= rsEarlierShowing))
            state = "disabled";
        else
            state = "warning";

        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_progList, "", qVariantFromValue(pginfo));

        pginfo->ToMap(infoMap);
        item->SetTextFromMap(infoMap, state);
        if (m_type == plTitle)
        {
            QString tempSubTitle = pginfo->subtitle;
            if (tempSubTitle.trimmed().isEmpty())
                tempSubTitle = pginfo->title;
            item->SetText(tempSubTitle, "titlesubtitle", state);
        }

        QString rating = QString::number((int)((pginfo->stars * 10.0) + 0.5));
        item->DisplayState(rating, "ratingstate");

        item->DisplayState(state, "status");
    }

    if (m_positionText)
    {
        m_positionText->SetText(tr("%1 of %2")
            .arg(m_progList->GetCurrentPos())
            .arg(m_progList->GetCount()));
    }
}

void ProgLister::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*> (item->GetData());
    if (pginfo)
    {
        InfoMap infoMap;
        pginfo->ToMap(infoMap);
        SetTextFromMap(infoMap);
        if (m_positionText)
        {
            m_positionText->SetText(tr("%1 of %2")
                   .arg(m_progList->GetCurrentPos())
                   .arg(m_progList->GetCount()));
        }
        MythUIStateType *ratingState = dynamic_cast<MythUIStateType*>
                                                    (GetChild("ratingstate"));
        if (ratingState)
        {
            QString rating = QString::number((int)((pginfo->stars * 10.0) + 0.5));
            ratingState->DisplayState(rating);
        }
    }
}

void ProgLister::customEvent(QEvent *event)
{
    bool needUpdate = false;

    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        int     buttonnum  = dce->GetResult();

        if (resultid == "menu")
        {
            if (resulttext == tr("Choose Search Phrase..."))
            {
                chooseView();
            }
            else if (resulttext == tr("Sort"))
            {
                showSortMenu();
            }
            else if (resulttext == tr("Record"))
            {
                quickRecord();
            }
            else if (resulttext == tr("Edit Schedule"))
            {
                edit();
            }
            else if (resulttext == tr("Program Details"))
            {
                details();
            }
            else if (resulttext == tr("Upcoming"))
            {
                upcoming();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                customEdit();
            }
            else if (resulttext == tr("Delete Rule"))
            {
                deleteRule();
            }
            else if (resulttext == tr("Delete Episode"))
            {
                deleteOldEpisode();
            }
        }
        else if (resultid == "sortmenu")
        {
            if (resulttext == tr("Reverse Sort Order"))
            {
                m_reverseSort = !m_reverseSort;
                needUpdate = true;
            }
            else if (resulttext == tr("Sort By Title"))
            {
                m_titleSort = true;
                m_reverseSort = false;
                needUpdate = true;
            }
            else if (resulttext == tr("Sort By Time"))
            {
                m_titleSort = false;
                m_reverseSort = (m_type == plPreviouslyRecorded);
                needUpdate = true;
            }
        }
        else if (resultid == "deletemenu")
        {
            if (resulttext == tr("Allow this episode to re-record"))
            {
                ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];
                if (pi)
                {
                    RecordingInfo ri(*pi);
                    ri.ForgetHistory();
                    *pi = ri;
                }
            }
            else if (resulttext == tr("Never record this episode"))
            {
                ProgramInfo *pi = m_itemList[m_progList->GetCurrentPos()];
                if (pi)
                {
                    RecordingInfo ri(*pi);
                    ri.SetDupHistory();
                    *pi = ri;
                }
            }
            else if (resulttext == tr("Remove this episode from the list"))
            {
                deleteOldEpisode();
            }
            else if (resulttext == tr("Remove all episodes for this title"))
            {
                deleteOldTitle();
            }
        }
        else if (resultid == "deleterule")
        {
            RecordingRule *record = qVariantValue<RecordingRule *>(dce->GetData());
            if (record)
            {
                if (buttonnum > 0)
                {
                    if (!record->Delete())
                        VERBOSE(VB_IMPORTANT, "Failed to delete recording rule");
                }
                delete record;
            }
        }
        else
            ScheduleCommon::customEvent(event);
    }
    else if (event->type() == ScreenLoadCompletionEvent::kEventType)
    {
        ScreenLoadCompletionEvent *slce = (ScreenLoadCompletionEvent*)(event);
        QString id = slce->GetId();

        if (id == objectName())
        {
            CloseBusyPopup(); // opened by LoadInBackground()
            updateDisplay(false);

            if (m_curView < 0 && m_type != plPreviouslyRecorded)
                chooseView();
            return;
        }
    }
    else if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message == "CHOOSE_VIEW")
        {
            chooseView();
            return;
        }
        else if (message == "SCHEDULE_CHANGE")
        {
            needUpdate = true;
        }
    }

    if (needUpdate)
        fillItemList(true);
}

///////////////////////////////////////////////////////////////////////////////

PhrasePopup::PhrasePopup(MythScreenStack *parentStack,
                         ProgLister *parent,
                         RecSearchType searchType,
                         const QStringList &list,
                         const QString &currentValue)
            : MythScreenType(parentStack, "phrasepopup"),
              m_parent(parent), m_searchType(searchType),  m_list(list),
              m_titleText(NULL), m_phraseList(NULL), m_phraseEdit(NULL),
              m_okButton(NULL), m_deleteButton(NULL), m_recordButton(NULL)
{
    m_currentValue = currentValue;
}

bool PhrasePopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "phrasepopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleText, "title_text", &err);
    UIUtilE::Assign(this, m_phraseList, "phrase_list", &err);
    UIUtilE::Assign(this, m_phraseEdit, "phrase_edit", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);
    UIUtilE::Assign(this, m_deleteButton, "delete_button", &err);
    UIUtilE::Assign(this, m_recordButton, "record_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'phrasepopup'");
        return false;
    }

    if (m_searchType == kPowerSearch)
    {
        m_titleText->SetText(tr("Select Search"));
        new MythUIButtonListItem(m_phraseList, tr("<New Search>"), NULL, false);
        m_okButton->SetText(tr("Edit"));
    }
    else
    {
        m_titleText->SetText(tr("Phrase"));
        new MythUIButtonListItem(m_phraseList, tr("<New Phrase>"), NULL, false);
    }

    for (int x = 0; x < m_list.size(); x++)
    {
        new MythUIButtonListItem(m_phraseList, m_list.at(x), NULL, false);
    }

    connect(m_phraseList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(phraseClicked(MythUIButtonListItem*)));
    connect(m_phraseList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(phraseSelected(MythUIButtonListItem*)));


    m_phraseList->MoveToNamedPosition(m_currentValue);

    m_deleteButton->SetText(tr("Delete"));
    m_recordButton->SetText(tr("Record"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));
    connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deleteClicked()));
    connect(m_recordButton, SIGNAL(Clicked()), this, SLOT(recordClicked()));

    connect(m_phraseEdit, SIGNAL(valueChanged()), this, SLOT(editChanged()));

    BuildFocusList();

    SetFocusWidget(m_phraseList);

    return true;
}

void PhrasePopup::editChanged(void)
{
    m_okButton->SetEnabled(!m_phraseEdit->GetText().trimmed().isEmpty());
    m_deleteButton->SetEnabled((m_list.indexOf(m_phraseEdit->GetText().trimmed()) != -1));
    m_recordButton->SetEnabled(!m_phraseEdit->GetText().trimmed().isEmpty());
}

void PhrasePopup::phraseClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int pos = m_phraseList->GetCurrentPos();

    if (pos == 0)
       SetFocusWidget(m_phraseEdit);
    else
       okClicked();
}

void PhrasePopup::phraseSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_phraseList->GetCurrentPos() == 0)
        m_phraseEdit->SetText("");
    else
        m_phraseEdit->SetText(item->GetText());

    m_okButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_deleteButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_recordButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
}

void PhrasePopup::okClicked(void)
{
    if (m_phraseEdit->GetText().trimmed().isEmpty())
        return;

    // check to see if we need to save the phrase
    m_parent->updateKeywordInDB(m_phraseEdit->GetText(), m_phraseList->GetValue());

//    emit haveResult(m_phraseList->GetCurrentPos());
    emit haveResult(m_phraseEdit->GetText());

    Close();
}

void PhrasePopup::deleteClicked(void)
{
    int view = m_phraseList->GetCurrentPos() - 1;

    if (view < 0)
        return;

    QString text = m_list[view];
    QString qphrase = text;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM keyword "
                  "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
    query.bindValue(":PHRASE", qphrase);
    query.bindValue(":TYPE", m_searchType);
    if (!query.exec())
        MythDB::DBError("PhrasePopup::deleteClicked", query);

    m_phraseList->RemoveItem(m_phraseList->GetItemCurrent());

    m_parent->m_viewList.removeAll(text);
    m_parent->m_viewTextList.removeAll(text);

    if (view < m_parent->m_curView)
        m_parent->m_curView--;
    else if (view == m_parent->m_curView)
        m_parent->m_curView = -1;

    if (m_parent->m_viewList.count() < 1)
        SetFocusWidget(m_phraseEdit);
    else
        SetFocusWidget(m_phraseEdit);
}

void PhrasePopup::recordClicked(void)
{
    QString text = m_phraseEdit->GetText();
    bool genreflag = false;

    QString what = text;

    if (text.trimmed().isEmpty())
        return;

    if (m_searchType == kNoSearch)
    {
        VERBOSE(VB_IMPORTANT, "Unknown search in ProgLister");
        return;
    }

    if (m_searchType == kPowerSearch)
    {
        if (text == ":::::")
            return;

        MSqlBindings bindings;
        genreflag = m_parent->powerStringToSQL(text, what, bindings);

        if (what.isEmpty())
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    RecordingRule *record = new RecordingRule();

    if (genreflag)
    {
        QString fromgenre = QString("LEFT JOIN programgenres ON "
                "program.chanid = programgenres.chanid AND "
                "program.starttime = programgenres.starttime ");
        record->LoadBySearch(m_searchType, text, what, fromgenre);
    }
    else
    {
        record->LoadBySearch(m_searchType, text, what);
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        okClicked();
    }
    else
        delete schededit;
}

///////////////////////////////////////////////////////////////////////////////

TimePopup::TimePopup(MythScreenStack *parentStack, ProgLister *parent)
         : MythScreenType(parentStack, "timepopup"),
           m_parent(parent), m_dateList(NULL), m_timeList(NULL),
           m_okButton(NULL)
{
}

bool TimePopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "timepopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_dateList, "date_list", &err);
    UIUtilE::Assign(this, m_timeList, "time_list", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'timepopup'");
        return false;
    }

    // date
    for (int x = -1; x <= 14; x++)
    {
        new MythUIButtonListItem(m_dateList,
                                 m_parent->m_startTime.addDays(x).toString(m_parent->m_dayFormat),
                                 NULL, false);

        if (m_parent->m_startTime.addDays(x).toString("MMdd") ==
                m_parent->m_searchTime.toString("MMdd"))
            m_dateList->SetItemCurrent(m_dateList->GetCount() - 1);
    }

    // time
    QTime hr;
    for (int x = 0; x < 24; x++)
    {
        hr.setHMS(x, 0, 0);
        new MythUIButtonListItem(m_timeList,
                                 hr.toString(m_parent->m_hourFormat),
                                 NULL, false);

        if (hr.toString("hh") == m_parent->m_searchTime.toString("hh"))
            m_timeList->SetItemCurrent(x);
    }

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));

    BuildFocusList();

    SetFocusWidget(m_dateList);

    return true;
}

void TimePopup::okClicked(void)
{
    QDateTime startTime = m_parent->m_startTime;
    int dayOffset = m_dateList->GetCurrentPos() -1;

    startTime.setDate(startTime.addDays(dayOffset).date());

    QTime hr;
    hr.setHMS(m_timeList->GetCurrentPos(), 0, 0);
    startTime.setTime(hr);

    emit haveResult(startTime);

    Close();
}

//////////////////////////////////////////////////////////////////////////////

PowerSearchPopup::PowerSearchPopup(MythScreenStack *parentStack,
                                   ProgLister *parent,
                                   RecSearchType searchType,
                                   const QStringList &list,
                                   const QString &currentValue)
            : MythScreenType(parentStack, "phrasepopup"),
              m_parent(parent), m_searchType(searchType), m_list(list),
              m_currentValue(currentValue),
              m_titleText(NULL), m_phraseList(NULL), m_phraseEdit(NULL),
              m_editButton(NULL), m_deleteButton(NULL), m_recordButton(NULL)
{
}

bool PowerSearchPopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "powersearchpopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleText, "title_text", &err);
    UIUtilE::Assign(this, m_phraseList, "phrase_list", &err);
    UIUtilE::Assign(this, m_editButton, "edit_button", &err);
    UIUtilE::Assign(this, m_deleteButton, "delete_button", &err);
    UIUtilE::Assign(this, m_recordButton, "record_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'powersearchpopup'");
        return false;
    }

    m_titleText->SetText(tr("Select Search"));
    new MythUIButtonListItem(m_phraseList, tr("<New Search>"), NULL, false);

    for (int x = 0; x < m_list.size(); x++)
    {
        new MythUIButtonListItem(m_phraseList, m_list.at(x), NULL, false);
    }

    connect(m_phraseList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(phraseClicked(MythUIButtonListItem*)));
    connect(m_phraseList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(phraseSelected(MythUIButtonListItem*)));


    m_phraseList->MoveToNamedPosition(m_currentValue);

    m_editButton->SetText(tr("Edit"));
    m_deleteButton->SetText(tr("Delete"));
    m_recordButton->SetText(tr("Record"));

    connect(m_editButton, SIGNAL(Clicked()), this, SLOT(editClicked()));
    connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deleteClicked()));
    connect(m_recordButton, SIGNAL(Clicked()), this, SLOT(recordClicked()));

    BuildFocusList();

    SetFocusWidget(m_phraseList);

    return true;
}

void PowerSearchPopup::phraseClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    int pos = m_phraseList->GetCurrentPos();

    if (pos == 0)
       editClicked();
    else
    {
        emit haveResult(m_phraseList->GetValue());
        Close();
    }
}

void PowerSearchPopup::phraseSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    m_editButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_deleteButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
    m_recordButton->SetEnabled((m_phraseList->GetCurrentPos() != 0));
}

void PowerSearchPopup::editClicked(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString currentItem = ":::::";

    if (m_phraseList->GetCurrentPos() != 0)
          currentItem = m_phraseList->GetValue();

    EditPowerSearchPopup *popup = new EditPowerSearchPopup(popupStack, m_parent, currentItem);

    if (!popup->Create())
    {
        delete popup;
        return;
    }

    popupStack->AddScreen(popup);

    Close();
}

void PowerSearchPopup::deleteClicked(void)
{
    int view = m_phraseList->GetCurrentPos() - 1;

    if (view < 0)
        return;

    QString text = m_list[view];
    QString qphrase = text;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM keyword "
                  "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
    query.bindValue(":PHRASE", qphrase);
    query.bindValue(":TYPE", m_searchType);
    if (!query.exec())
        MythDB::DBError("PowerSearchPopup::deleteClicked", query);

    m_phraseList->RemoveItem(m_phraseList->GetItemCurrent());

    m_parent->m_viewList.removeAll(text);
    m_parent->m_viewTextList.removeAll(text);

    if (view < m_parent->m_curView)
        m_parent->m_curView--;
    else if (view == m_parent->m_curView)
        m_parent->m_curView = -1;

    if (m_parent->m_viewList.count() < 1)
        SetFocusWidget(m_phraseEdit);
    else
        SetFocusWidget(m_phraseEdit);
}

void PowerSearchPopup::recordClicked(void)
{
    QString text = m_phraseEdit->GetText();
    bool genreflag = false;

    QString what = text;

    if (text.trimmed().isEmpty())
        return;

    if (m_searchType == kNoSearch)
    {
        VERBOSE(VB_IMPORTANT, "Unknown search in ProgLister");
        return;
    }

    if (m_searchType == kPowerSearch)
    {
        if (text == ":::::")
            return;

        MSqlBindings bindings;
        genreflag = m_parent->powerStringToSQL(text, what, bindings);

        if (what.isEmpty())
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    RecordingRule *record = new RecordingRule();

    if (genreflag)
    {
        QString fromgenre = QString("LEFT JOIN programgenres ON "
                "program.chanid = programgenres.chanid AND "
                "program.starttime = programgenres.starttime ");
        record->LoadBySearch(m_searchType, text, what, fromgenre);
    }
    else
    {
        record->LoadBySearch(m_searchType, text, what);
    }
    
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, record);
    if (schededit->Create())
    {
        mainStack->AddScreen(schededit);
        emit haveResult(m_phraseList->GetValue());
        Close();
    }
    else
        delete schededit;
}

///////////////////////////////////////////////////////////////////////////////

EditPowerSearchPopup::EditPowerSearchPopup(MythScreenStack *parentStack,
                                   ProgLister *parent,
                                   const QString &currentValue)
                : MythScreenType(parentStack, "phrasepopup")
{
    m_parent = parent;

    //sanity check currentvalue
    m_currentValue = currentValue;
    QStringList field = m_currentValue.split(':');
    if (field.count() != 6)
    {
        VERBOSE(VB_IMPORTANT, QString("Error. PowerSearch %1 has %2 fields")
                .arg(m_currentValue).arg(field.count()));
        m_currentValue = ":::::";
    }
}

bool EditPowerSearchPopup::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "editpowersearchpopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleEdit, "title_edit", &err);
    UIUtilE::Assign(this, m_subtitleEdit, "subtitle_edit", &err);
    UIUtilE::Assign(this, m_descEdit, "desc_edit", &err);
    UIUtilE::Assign(this, m_categoryList, "category_list", &err);
    UIUtilE::Assign(this, m_genreList, "genre_list", &err);
    UIUtilE::Assign(this, m_channelList, "channel_list", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'editpowersearchpopup'");
        return false;
    }

    QStringList field = m_currentValue.split(':');

    m_titleEdit->SetText(field[0]);
    m_subtitleEdit->SetText(field[1]);
    m_descEdit->SetText(field[2]);

    initLists();

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));

    BuildFocusList();

    SetFocusWidget(m_titleEdit);

    return true;
}

void EditPowerSearchPopup::okClicked(void)
{
    QString text;
    text =  m_titleEdit->GetText().replace(':','%').replace('*','%') + ':';
    text += m_subtitleEdit->GetText().replace(':','%').replace('*','%') + ':';
    text += m_descEdit->GetText().replace(':','%').replace('*','%') + ':';

    if (m_categoryList->GetCurrentPos() > 0)
        text += m_categories[m_categoryList->GetCurrentPos()];
    text += ':';
    if (m_genreList->GetCurrentPos() > 0)
        text += m_genres[m_genreList->GetCurrentPos()];
    text += ':';
    if (m_channelList->GetCurrentPos() > 0)
        text += m_channels[m_channelList->GetCurrentPos()];

    if (text == ":::::")
        return;

    m_parent->updateKeywordInDB(text, m_currentValue);
    m_parent->fillViewList(text);
    m_parent->setViewFromList(text);

    Close();
}

void EditPowerSearchPopup::initLists(void)
{
    QStringList field = m_currentValue.split(':');

    // category type
    m_categories.clear();
    new MythUIButtonListItem(m_categoryList, tr("(Any Program Type)"), NULL, false);
    m_categories << "";
    new MythUIButtonListItem(m_categoryList, tr("Movies"), NULL, false);
    m_categories << "movie";
    new MythUIButtonListItem(m_categoryList, tr("Series"), NULL, false);
    m_categories << "series";
    new MythUIButtonListItem(m_categoryList, tr("Show"), NULL, false);
    m_categories << "tvshow";
    new MythUIButtonListItem(m_categoryList, tr("Sports"), NULL, false);
    m_categories << "sports";
    m_categoryList->SetItemCurrent(m_categories.indexOf(field[3]));

    // genre
    m_genres.clear();
    new MythUIButtonListItem(m_genreList, tr("(Any Genre)"), NULL, false);
    m_genres << "";

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT genre FROM programgenres GROUP BY genre;");

    if (query.exec())
    {
        while (query.next())
        {
            QString category = query.value(0).toString();
            if (category.isEmpty() || category.trimmed().isEmpty())
                continue;
            category = query.value(0).toString();
            new MythUIButtonListItem(m_genreList, category, NULL, false);
            m_genres << category;
            if (category == field[4])
                m_genreList->SetItemCurrent(m_genreList->GetCount() - 1);
        }
    }

    // channel
    QString channelOrdering = gContext->GetSetting("ChannelOrdering", "channum");
    QString channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

    m_channels.clear();
    new MythUIButtonListItem(m_channelList, tr("(Any Channel)"), NULL, false);
    m_channels << "";

    DBChanList channels = ChannelUtil::GetChannels(0, true, "callsign");
    ChannelUtil::SortChannels(channels, channelOrdering, true);

    for (uint i = 0; i < channels.size(); ++i)
    {
        QString chantext = channelFormat;
        chantext
            .replace("<num>",  channels[i].channum)
            .replace("<sign>", channels[i].callsign)
            .replace("<name>", channels[i].name);

        m_parent->m_viewList << QString::number(channels[i].chanid);
        m_parent->m_viewTextList << chantext;

        new MythUIButtonListItem(m_channelList, chantext, NULL, false);

        m_channels << channels[i].callsign;
        if (channels[i].callsign == field[5])
            m_channelList->SetItemCurrent(m_channelList->GetCount() - 1);
    }
}
