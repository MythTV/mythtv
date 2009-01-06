#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

// qt
#include <QApplication>
#include <QRegExp>

// myth
#include "proglist.h"
#include "scheduledrecording.h"
#include "customedit.h"
#include "mythcontext.h"
#include "remoteutil.h"
#include "libmythdb/mythdbcon.h"
#include "libmythdb/mythverbose.h"
#include "channelutil.h"
#include "mythuitext.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"

ProgLister::ProgLister(MythScreenStack *parent, ProgListType pltype,
               const QString &view, const QString &from)
          : MythScreenType(parent, "ProgLister")
{
    m_type = pltype;
    m_addTables = from;
    m_startTime = QDateTime::currentDateTime();
    m_searchTime = m_startTime;

    m_dayFormat = gContext->GetSetting("DateFormat");
    m_hourFormat = gContext->GetSetting("TimeFormat");
    m_timeFormat = gContext->GetSetting("ShortDateFormat") + " " + m_hourFormat;
    m_fullDateFormat = m_dayFormat + " " + m_hourFormat;
    m_channelOrdering = gContext->GetSetting("ChannelOrdering", "channum");
    m_channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");

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
    fillViewList(view);

    gContext->addCurrentLocation("ProgLister");
}

ProgLister::~ProgLister()
{
    m_itemList.clear();
    gContext->removeListener(this);
    gContext->removeCurrentLocation();
}

bool ProgLister::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "programlist", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_curviewText, "curview", &err);
    UIUtilE::Assign(this, m_progList, "proglist", &err);
    UIUtilE::Assign(this, m_titleText, "title", &err);
    UIUtilE::Assign(this, m_chanText, "channel", &err);
    UIUtilE::Assign(this, m_timedateText, "timedate", &err);
    UIUtilE::Assign(this, m_subdescText, "description", &err);
    UIUtilE::Assign(this, m_typeText, "type", &err);
    UIUtilE::Assign(this, m_progIDText, "programid", &err);
    UIUtilE::Assign(this, m_schedText, "sched", &err);
    UIUtilE::Assign(this, m_messageText, "msg", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'programlist'");
        return false;
    }

     connect(m_progList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));

     connect(m_progList, SIGNAL(itemClicked(MythUIButtonListItem*)),
             SLOT(select()));

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
        default: value = tr("Unknown Search"); break;
    }
    m_schedText->SetText(value);

    m_subdescText->SetVisible(false);

    fillItemList(false);

    gContext->addListener(this);

    return true;
}

void ProgLister::Init(void)
{
    if (m_curView < 0)
        QApplication::postEvent(this, new MythEvent("CHOOSE_VIEW"));
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
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "PREVVIEW")
            prevView();
        else if (action == "NEXTVIEW")
            nextView();
        else if (action == "MENU")
            chooseView();
        else if (action == "INFO")
            edit();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "DELETE")
            remove();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS")
            details();
        else if (action == "TOGGLERECORD")
            quickRecord();
        else if (action == "1")
        {
            if (m_titleSort == true)
            {
                m_titleSort = false;
                m_reverseSort = false;
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
        fillItemList(false);

    m_allowEvents = true;

    return handled;
}

void ProgLister::prevView(void)
{
    if (m_type == plTime)
    {
        m_searchTime = m_searchTime.addSecs(-3600);
        m_curView = 0;
        m_viewList[m_curView] = m_searchTime.toString(m_fullDateFormat);
        m_viewTextList[m_curView] = m_viewList[m_curView];
        fillItemList(false);

        return;
    }

    if (m_viewList.count() < 2)
        return;

    m_curView--;
    if (m_curView < 0)
        m_curView = m_viewList.count() - 1;

    fillItemList(false);
}

void ProgLister::nextView(void)
{
    if (m_type == plTime)
    {
        m_searchTime = m_searchTime.addSecs(3600);
        m_curView = 0;
        m_viewList[m_curView] = m_searchTime.toString(m_fullDateFormat);
        m_viewTextList[m_curView] = m_viewList[m_curView];
        fillItemList(false);

        return;
    }
    if (m_viewList.count() < 2)
        return;

    m_curView++;
    if (m_curView >= (int)m_viewList.count())
        m_curView = 0;

    fillItemList(false);
}

void ProgLister::updateKeywordInDB(const QString &text, const QString &oldValue)
{
    int oldview = m_viewList.indexOf(oldValue);
    int newview = m_viewList.indexOf(text);

    QString qphrase = NULL;

    if (newview < 0 || newview != oldview)
    {
        if (oldview >= 0)
        {
            qphrase = m_viewList[oldview];

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("DELETE FROM keyword "
                          "WHERE phrase = :PHRASE AND searchtype = :TYPE;");
            query.bindValue(":PHRASE", qphrase);
            query.bindValue(":TYPE", m_searchType);
            query.exec();

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
            query.exec();

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
        QString currentItem = "";

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
        QString currentItem = "";

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
        QString currentItem = "";

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

    fillItemList(false);
}

void ProgLister::setViewFromList(QString item)
{
    m_curView = m_viewTextList.indexOf(item);

    fillItemList(false);
}

bool ProgLister::powerStringToSQL(const QString &qphrase, QString &output,
                                  MSqlBindings &bindings)
{
    int ret = 0;
    output = "";
    QString curfield;

    QStringList field = qphrase.split(":");

    if (field.count() != 6)
    {
        VERBOSE(VB_IMPORTANT, QString("Error. PowerSearch %1 has %2 fields")
                .arg(qphrase).arg(field.count()));
        return ret;
    }

    if (!field[0].isEmpty())
    {
        curfield = "%" + field[0] + "%";
        output += "program.title LIKE :POWERTITLE ";
        bindings[":POWERTITLE"] = curfield;
    }

    if (!field[1].isEmpty())
    {
        if (output > "")
            output += "\nAND ";

        curfield = "%" + field[1] + "%";
        output += "program.subtitle LIKE :POWERSUB ";
        bindings[":POWERSUB"] = curfield;
    }

    if (!field[2].isEmpty())
    {
        if (output > "")
            output += "\nAND ";

        curfield = "%" + field[2] + "%";
        output += "program.description LIKE :POWERDESC ";
        bindings[":POWERDESC"] = curfield;
    }

    if (!field[3].isEmpty())
    {
        if (output > "")
            output += "\nAND ";

        output += "program.category_type = :POWERCATTYPE ";
        bindings[":POWERCATTYPE"] = field[3];
    }

    if (!field[4].isEmpty())
    {
        if (output > "")
            output += "\nAND ";

        output += "programgenres.genre = :POWERGENRE ";
        bindings[":POWERGENRE"] = field[4];
        ret = 1;
    }

    if (!field[5].isEmpty())
    {
        if (output > "")
            output += "\nAND ";

        output += "channel.callsign = :POWERCALLSIGN ";
        bindings[":POWERCALLSIGN"] = field[5];
    }
    return ret;
}

void ProgLister::quickRecord()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (!pi)
        return;

    pi->ToggleRecord();
}

void ProgLister::select()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (!pi)
        return;

    pi->EditRecording();
}

void ProgLister::edit()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (!pi)
        return;

    pi->EditScheduled();
}

void ProgLister::customEdit()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (!pi)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                    "customedit", pi);
    ce->exec();
    delete ce;
}

void ProgLister::remove()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (!pi || pi->recordid <= 0)
        return;

    ScheduledRecording *record = new ScheduledRecording();
    int recid = pi->recordid;
    record->loadByID(recid);

    QString message =
        tr("Delete '%1' %2 rule?").arg(record->getRecordTitle())
                                  .arg(pi->RecTypeText());

    ShowOkPopup(message, this, SLOT(doRemove(bool)), true);

    record->deleteLater();
}

void ProgLister::doRemove(bool ok)
{
    if (ok)
    {
        ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

        if (!pi || pi->recordid <= 0)
            return;

        ScheduledRecording *record = new ScheduledRecording();
        int recid = pi->recordid;
        record->loadByID(recid);
        record->remove();
        ScheduledRecording::signalChange(recid);
        record->deleteLater();
    }
}

void ProgLister::upcoming()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (!pi || m_type == plTitle)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitle, pi->title, "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void ProgLister::details()
{
    ProgramInfo *pi = m_itemList.at(m_progList->GetCurrentPos());

    if (pi)
        pi->showDetails();
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

        for (uint i = 0; i < channels.size(); i++)
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
        query.exec();

        if (query.isActive() && query.size())
        {
            QString lastGenre1;

            while (query.next())
            {
                QString genre1 = query.value(0).toString();
                if (genre1 <= " ")
                    continue;

                if (genre1 != lastGenre1)
                {
                    m_viewList << genre1;
                    m_viewTextList << genre1;
                    lastGenre1 = genre1;
                }

                QString genre2 = query.value(1).toString();
                if (genre2 <= " " || genre2 == genre1)
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
            query.exec();

            if (query.isActive() && query.size())
            {
                while (query.next())
                {
                    QString category = query.value(0).toString();
                    if (category <= " " || category.isNull())
                        continue;
                    category = query.value(0).toString();
                    m_viewList << category;
                    m_viewTextList << category;
                }
            }

            m_useGenres = false;
        }

        if (view != "")
            m_curView = m_viewList.indexOf(view);
    }
    else if (m_type == plTitleSearch || m_type == plKeywordSearch ||
             m_type == plPeopleSearch || m_type == plPowerSearch)
    {
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare("SELECT phrase FROM keyword "
                      "WHERE searchtype = :SEARCHTYPE;");
        query.bindValue(":SEARCHTYPE", m_searchType);
        query.exec();

        if (query.isActive() && query.size())
        {
            while (query.next())
            {
                QString phrase = query.value(0).toString();
                if (phrase <= " ")
                    continue;
                phrase = query.value(0).toString();
                m_viewList << phrase;
                m_viewTextList << phrase;
            }
        }
        if (view != "")
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
                query.exec();

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
        if (view != "")
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
        m_curView = 0;
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
        m_curView = 0;
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
        query.exec();

        if (query.isActive() && query.size())
        {
            if (query.next())
            {
                QString title = query.value(0).toString();
                title = query.value(0).toString();
                m_viewList << view;
                m_viewTextList << title;
            }
        }
    }
    else if (m_type == plStoredSearch) // stored searches
    {
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare("SELECT rulename FROM customexample "
                      "WHERE search > 0 ORDER BY rulename;");
        query.exec();

        if (query.isActive() && query.size())
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
        if (view != "")
            m_curView = m_viewList.indexOf(view);
    }

    if (m_curView >= (int)m_viewList.count())
        m_curView = m_viewList.count() - 1;
}

class plTitleSort
{
    public:
        plTitleSort(void) {;}

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

class plTimeSort
{
    public:
        plTimeSort(void) {;}

        bool operator()(const ProgramInfo *a, const ProgramInfo *b) 
        {
            if (a->startts == b->startts)
                return (a->chanid < b->chanid);

            return (a->startts < b->startts);
        }
};

void ProgLister::fillItemList(bool restorePosition)
{
    if (m_curView < 0)
         return;

    QString selectedItem = m_progList->GetValue();

    if (m_curviewText && m_curView >= 0)
        m_curviewText->SetText(m_viewTextList[m_curView]);

    bool oneChanid = false;
    QString where = "";
    QString startstr = m_startTime.toString("yyyy-MM-ddThh:mm:50");
    QString qphrase = m_viewList[m_curView];

    MSqlBindings bindings;
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
        bindings[":PGILLIKEPHRASE0"] = QString("%") + qphrase + "%";
    }
    else if (m_type == plKeywordSearch) // keyword search
    {
        where = "WHERE channel.visible = 1 "
                "  AND program.endtime > :PGILSTART "
                "  AND (program.title LIKE :PGILLIKEPHRASE1 "
                "    OR program.subtitle LIKE :PGILLIKEPHRASE2 "
                "    OR program.description LIKE :PGILLIKEPHRASE3 ) ";
        bindings[":PGILLIKEPHRASE1"] = QString("%") + qphrase + "%";
        bindings[":PGILLIKEPHRASE2"] = QString("%") + qphrase + "%";
        bindings[":PGILLIKEPHRASE3"] = QString("%") + qphrase + "%";
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

        if (powerWhere != "")
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
        if (m_addTables > "")
            where = m_addTables + " " + where;
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
                "  AND program.stars "+qphrase+" ";
    }
    else if (m_type == plTime) // list by time
    {
        bindings[":PGILSEARCHTIME"] = m_searchTime.toString("yyyy-MM-dd hh:00:00");
        where = "WHERE channel.visible = 1 "
                "  AND program.starttime >= :PGILSEARCHTIME ";
        if (m_titleSort)
            where += "  AND program.starttime < DATE_ADD(:PGILSEARCHTIME, "
                     "INTERVAL '1' HOUR) ";
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
        QString fromc, wherec;
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare("SELECT fromclause, whereclause FROM customexample "
                      "WHERE rulename = :RULENAME;");
        query.bindValue(":RULENAME", qphrase);
        query.exec();

        if (query.isActive() && query.size())
        {
            query.next();
            fromc  = query.value(0).toString();
            wherec = query.value(1).toString();

            where = QString("WHERE channel.visible = 1 "
                            "  AND program.endtime > :PGILSTART "
                            "  AND ( %1 ) ").arg(wherec);
            if (fromc > "")
                where = fromc + " " + where;
        }
    }

    m_schedList.FromScheduler();
    m_itemList.FromProgram(where, bindings, m_schedList, oneChanid);

    ProgramInfo *s;
    vector<ProgramInfo *> sortedList;

    while (m_itemList.count())
    {
        s = m_itemList.take(0);
        if (m_type == plTitle)
            s->sortTitle = s->subtitle;
        else
            s->sortTitle = s->title;

        s->sortTitle.remove(QRegExp("^(The |A |An )"));
        sortedList.push_back(s);
    }

    if (m_type == plNewListings || m_titleSort)
    {
        // Prune to one per title
        sort(sortedList.begin(), sortedList.end(), plTitleSort());

        QString curtitle = "";
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
    if (!m_titleSort)
        sort(sortedList.begin(), sortedList.end(), plTimeSort());

    if (m_reverseSort)
    {
        vector<ProgramInfo *>::reverse_iterator r = sortedList.rbegin();
        for (; r != sortedList.rend(); r++)
            m_itemList.append(*r);
    }
    else
    {
        vector<ProgramInfo *>::iterator i = sortedList.begin();
        for (; i != sortedList.end(); i++)
            m_itemList.append(*i);
    }

    m_messageText->SetVisible((m_itemList.count() == 0));
    m_titleText->SetVisible((m_itemList.count() != 0));
    m_chanText->SetVisible((m_itemList.count() != 0));
    m_timedateText->SetVisible((m_itemList.count() != 0));
    m_subdescText->SetVisible((m_itemList.count() != 0));
    m_typeText->SetVisible((m_itemList.count() != 0));
    m_progIDText->SetVisible((m_itemList.count() != 0));

    updateButtonList();

    if (restorePosition)
        m_progList->MoveToNamedPosition(selectedItem);
}

void ProgLister::updateButtonList(void)
{
    m_progList->Reset();

    QString tmptitle;

    QStringList starMap;
    QString starstr = "";
    for (int i = 0; i <= 4; i++)
    {
        starMap << starstr;
        starMap << starstr + "/";
        starstr += "*";
    }

    for (uint i = 0; i < m_itemList.count(); i++)
    {
        ProgramInfo *pginfo = m_itemList.at(i);
        QString state;

        if (pginfo->recstatus == rsRecording)
            state = "running";
        else if (pginfo->recstatus == rsConflict ||
                    pginfo->recstatus == rsOffLine ||
                    pginfo->recstatus == rsAborted)
            state = "error";
        else if (pginfo->recstatus == rsWillRecord)
                state = "normal";
        else if (pginfo->recstatus == rsRepeat ||
                    pginfo->recstatus == rsOtherShowing ||
                    pginfo->recstatus == rsNeverRecord ||
                    pginfo->recstatus == rsDontRecord ||
                    (pginfo->recstatus != rsDontRecord &&
                    pginfo->recstatus <= rsEarlierShowing))
            state = "disabled";
        else
            state = "warning";

        MythUIButtonListItem *item =
                new MythUIButtonListItem(m_progList, "", qVariantFromValue(pginfo));
        item->SetText(pginfo->startts.toString(m_timeFormat), "time", state);
        item->SetText(pginfo->ChannelText(m_channelFormat), "channel", state);

        if (pginfo->stars > 0.0)
            tmptitle = QString("%1 (%2, %3 )")
                                .arg(pginfo->title).arg(pginfo->year)
                                .arg(starMap[(int) (pginfo->stars * 8)]);
        else if (pginfo->subtitle == "")
            tmptitle = pginfo->title;
        else
        {
            if (m_type == plTitle)
                tmptitle = pginfo->subtitle;
            else
                tmptitle = QString("%1 - \"%2\"")
                                    .arg(pginfo->title)
                                    .arg(pginfo->subtitle);
        }

        item->SetText(tmptitle, "title", state);
        item->SetText(pginfo->RecStatusChar(), "card", state);

        item->DisplayState(state, "status");
    }
}

void ProgLister::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo*> (item->GetData());
    if (pginfo)
    {
        QMap<QString, QString> infoMap;
        pginfo->ToMap(infoMap);
        setTextFromMap(this, infoMap);
    }
}

void ProgLister::setTextFromMap(MythUIType *parent,
                                   QMap<QString, QString> &infoMap)
{
    if (!parent)
        return;

    QList<MythUIType *> *children = parent->GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
        {
            QString newText = textType->GetDefaultText();
            QRegExp regexp("%(\\|(.))?([^\\|]+)(\\|(.))?%");
            regexp.setMinimal(true);
            if (newText.contains(regexp))
            {
                int pos = 0;
                QString tempString = newText;
                while ((pos = regexp.indexIn(newText, pos)) != -1)
                {
                    QString key = regexp.cap(3).toLower().trimmed();
                    QString replacement;
                    if (!infoMap.value(key).isEmpty())
                    {
                        replacement = QString("%1%2%3")
                                                .arg(regexp.cap(2))
                                                .arg(infoMap.value(key))
                                                .arg(regexp.cap(5));
                    }
                    tempString.replace(regexp.cap(0), replacement);
                    pos += regexp.matchedLength();
                }
                newText = tempString;
            }
            else
                newText = infoMap.value(textType->objectName());

            textType->SetText(newText);
        }
    }
}

void ProgLister::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) != MythEvent::MythEventMessage)
        return;

    MythEvent *me = (MythEvent *)e;
    QString message = me->Message();

    if (message == "CHOOSE_VIEW")
    {
        chooseView();
        if (m_curView < 0)
        {
            //Close();
            return;
        }
        return;
    }
    else if (message == "SCHEDULE_CHANGE")
    {
        fillItemList(true);
    }
}

///////////////////////////////////////////////////////////////////////////////

PhrasePopup::PhrasePopup(MythScreenStack *parentStack,
                         ProgLister *parent,
                         RecSearchType searchType,
                         const QStringList &list,
                         const QString &currentValue)
            : MythScreenType(parentStack, "phrasepopup")
{
    m_parent = parent;
    m_list = list;
    m_currentValue = currentValue;
    m_searchType = searchType;
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
        m_okButton->SetText(tr("Ok"));
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

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_phraseList);

    return true;
}

void PhrasePopup::editChanged(void)
{
    m_okButton->SetEnabled((m_phraseEdit->GetText().trimmed().length() > 0));
    m_deleteButton->SetEnabled((m_list.indexOf(m_phraseEdit->GetText().trimmed()) != -1));
    m_recordButton->SetEnabled((m_phraseEdit->GetText().trimmed().length() > 0));
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
    if (m_phraseEdit->GetText().trimmed().length() == 0)
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
    query.exec();

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
    QString text = "";
    bool genreflag = false;

    text = m_phraseEdit->GetText();

    QString what = text;

    if (text.trimmed().length() == 0)
        return;

    if (m_searchType == kNoSearch)
    {
        VERBOSE(VB_IMPORTANT, "Unknown search in ProgLister");
        return;
    }

    if (m_searchType == kPowerSearch)
    {
        if (text == "" || text == ":::::")
            return;

        MSqlBindings bindings;
        genreflag = m_parent->powerStringToSQL(text, what, bindings);

        if (what == "")
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    ScheduledRecording *record = new ScheduledRecording();

    if (genreflag)
    {
        QString fromgenre = QString("LEFT JOIN programgenres ON "
                "program.chanid = programgenres.chanid AND "
                "program.starttime = programgenres.starttime ");
        record->loadBySearch(m_searchType, text, fromgenre, what);
    }
    else
    {
        record->loadBySearch(m_searchType, text, what);
    }

    record->exec();
    record->deleteLater();

    okClicked();
}

///////////////////////////////////////////////////////////////////////////////

TimePopup::TimePopup(MythScreenStack *parentStack, ProgLister *parent)
         : MythScreenType(parentStack, "timepopup")
{
    m_parent = parent;
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

    m_okButton->SetText(tr("Ok"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

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
            : MythScreenType(parentStack, "phrasepopup")
{
    m_parent = parent;
    m_list = list;
    m_currentValue = currentValue;
    m_searchType = searchType;
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

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

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
    query.exec();

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
    QString text = "";
    bool genreflag = false;

    text = m_phraseEdit->GetText();

    QString what = text;

    if (text.trimmed().length() == 0)
        return;

    if (m_searchType == kNoSearch)
    {
        VERBOSE(VB_IMPORTANT, "Unknown search in ProgLister");
        return;
    }

    if (m_searchType == kPowerSearch)
    {
        if (text == "" || text == ":::::")
            return;

        MSqlBindings bindings;
        genreflag = m_parent->powerStringToSQL(text, what, bindings);

        if (what == "")
            return;

        MSqlEscapeAsAQuery(what, bindings);
    }

    ScheduledRecording *record = new ScheduledRecording();

    if (genreflag)
    {
        QString fromgenre = QString("LEFT JOIN programgenres ON "
                "program.chanid = programgenres.chanid AND "
                "program.starttime = programgenres.starttime ");
        record->loadBySearch(m_searchType, text, fromgenre, what);
    }
    else
    {
        record->loadBySearch(m_searchType, text, what);
    }

    record->exec();
    record->deleteLater();

    emit haveResult(m_phraseList->GetValue());
    Close();
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
    QStringList field = m_currentValue.split(":");
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

    QStringList field = m_currentValue.split(":");

    m_titleEdit->SetText(field[0]);
    m_subtitleEdit->SetText(field[1]);
    m_descEdit->SetText(field[2]);

    initLists();

    m_okButton->SetText(tr("Ok"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okClicked()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_titleEdit);

    return true;
}

void EditPowerSearchPopup::okClicked(void)
{
    QString text = "";
    text =  m_titleEdit->GetText().replace(":","%").replace("*","%") + ":";
    text += m_subtitleEdit->GetText().replace(":","%").replace("*","%") + ":";
    text += m_descEdit->GetText().replace(":","%").replace("*","%") + ":";

    if (m_categoryList->GetCurrentPos() > 0)
        text += m_categories[m_categoryList->GetCurrentPos()];
    text += ":";
    if (m_genreList->GetCurrentPos() > 0)
        text += m_genres[m_genreList->GetCurrentPos()];
    text += ":";
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
    QStringList field = m_currentValue.split(":");

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
    query.exec();

    if (query.isActive() && query.size())
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

    for (uint i = 0; i < channels.size(); i++)
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
