/*
 * Copyright (c) 2017 Peter G Bennett <pbennett@mythtv.org>
 *
 * This file is part of MythTV.
 *
 * MythTV is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as
 * published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * MythTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MythTV. If not, see <http://www.gnu.org/licenses/>.
 */

#include "prevreclist.h"

// C/C++
#include <algorithm>
#include <deque>                        // for _Deque_iterator, operator-, etc
#include <iterator>                     // for reverse_iterator
using namespace std;

// QT
#include <QDateTime>
#include <QString>

//MythTV
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythmiscutil.h"
#include "xmlparsebase.h"
#include "recordinginfo.h"
#include "recordingrule.h"
#include "scheduledrecording.h"

// MythUI
#include "mythuitext.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mythuiutils.h"                // for UIUtilE, UIUtilW
#include "mythdialogbox.h"

#define LOC      QString("PrevRecordedList: ")

// flags for PrevRecSortFlags setting
static const int fTitleGroup = 1;
static const int fReverseSort = 2;
static const int fDefault = fReverseSort;

PrevRecordedList::PrevRecordedList(MythScreenStack *parent, uint recid,
    QString title) :
    ScheduleCommon(parent,"PrevRecordedList"),
    m_recid(recid),
    m_title(std::move(title))
{

    if (m_recid && !m_title.isEmpty())
    {
        m_where = QString(" AND ( recordid = %1 OR title = :MTITLE )")
            .arg(m_recid);
    }
    else if (!m_title.isEmpty())
    {
        m_where = QString("AND title = :MTITLE ");
    }
    else if (m_recid)
    {
        m_where = QString("AND recordid = %1 ").arg(m_recid);
    }
    else
    {
        // Get sort options if this is not a filtered list
        int flags = gCoreContext->GetNumSetting("PrevRecSortFlags",fDefault);
        m_titleGroup = ((flags & fTitleGroup) != 0);
        m_reverseSort = ((flags & fReverseSort) != 0);

    }
}

PrevRecordedList::~PrevRecordedList()
{
    if (m_where.isEmpty())
    {
        // Save sort setting if this is not a filtered list
        int flags = 0;
        if (m_titleGroup)
            flags |= fTitleGroup;
        if (m_reverseSort)
            flags |= fReverseSort;
        gCoreContext->SaveSetting("PrevRecSortFlags", flags);
    }
    m_titleData.clear();
    m_showData.clear();
    gCoreContext->removeListener(this);
}

bool PrevRecordedList::Create(void)
{
    if (!LoadWindowFromXML("schedule-ui.xml", "prevreclist", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleList, "titles", &err);
    UIUtilE::Assign(this, m_showList, "shows", &err);
    UIUtilW::Assign(this, m_help1Text, "help1text");
    UIUtilW::Assign(this, m_help2Text, "help2text");
    UIUtilW::Assign(this, m_curviewText, "curview");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'prevreclist'");
        return false;
    }

    m_titleList->SetLCDTitles(tr("Programs"), "title");
    m_showList->SetLCDTitles(tr("Episodes"), "startdate|parttitle");

    BuildFocusList();
    gCoreContext->addListener(this);
    m_loadShows = false;
    LoadInBackground();

    return true;
}

void PrevRecordedList::Init(void)
{
    gCoreContext->addListener(this);
    connect(m_showList, &MythUIButtonList::itemSelected,
            this, &PrevRecordedList::updateInfo);
    connect(m_showList, &MythUIButtonList::LosingFocus,
            this, &PrevRecordedList::showListLoseFocus);
    connect(m_showList, &MythUIButtonList::TakingFocus,
            this, &PrevRecordedList::showListTakeFocus);
    connect(m_showList, SIGNAL(itemClicked(MythUIButtonListItem*)),
                this,  SLOT(ShowItemMenu()));

    UpdateTitleList();
    updateInfo();
}

// When m_loadShows is false we are loading the left hand
// button list, when it is true the right hand.
void PrevRecordedList::Load(void)
{
    if (m_loadShows)
    {
        if (m_titleGroup)
            LoadShowsByTitle();
        else
            LoadShowsByDate();
    }
    else
    {
        if (m_titleGroup)
            LoadTitles();
        else
            LoadDates();
    }
    ScreenLoadCompletionEvent *slce =
        new ScreenLoadCompletionEvent(objectName());
    QCoreApplication::postEvent(this, slce);
}

static bool comp_sorttitle_lt(
    const ProgramInfo *a, const ProgramInfo *b)
{
    QString a_st = a->GetSortTitle();
    QString b_st = b->GetSortTitle();
    return naturalCompare(a_st,b_st) < 0;
}

static bool comp_sorttitle_lt_rev(
    const ProgramInfo *a, const ProgramInfo *b)
{
    QString a_st = a->GetSortTitle();
    QString b_st = b->GetSortTitle();
    return naturalCompare(b_st, a_st) < 0;
}

static bool comp_sortdate_lt(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return a->GetRecordingStartTime() < b->GetRecordingStartTime();
}

static bool comp_sortdate_lt_rev(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return b->GetRecordingStartTime() < a->GetRecordingStartTime();
}

// Load a list of titles without subtitle or other info.
// each title can represent multiple recordings.
bool PrevRecordedList::LoadTitles(void)
{
    QString querystr = "SELECT DISTINCT title FROM oldrecorded "
        "WHERE oldrecorded.future = 0 " + m_where;

    m_titleData.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);

    if (!m_title.isEmpty())
        query.bindValue(":MTITLE", m_title);

    if (!query.exec())
    {
        MythDB::DBError("PrevRecordedList::LoadTitles", query);
        return false;
    }

    while (query.next())
    {
        QString title(query.value(0).toString());
        ProgramInfo *program = new ProgramInfo();
        program->SetTitle(title);
        m_titleData.push_back(program);
    }
    if (m_reverseSort)
        std::stable_sort(m_titleData.begin(), m_titleData.end(),
            comp_sorttitle_lt_rev);
    else
        std::stable_sort(m_titleData.begin(), m_titleData.end(),
            comp_sorttitle_lt);
    return true;
}

bool PrevRecordedList::LoadDates(void)
{
    QString querystr = "SELECT DISTINCT "
            "YEAR(CONVERT_TZ(starttime,'UTC','SYSTEM')), "
            "MONTH(CONVERT_TZ(starttime,'UTC','SYSTEM')) "
        "FROM oldrecorded "
        "WHERE oldrecorded.future = 0 " + m_where;

    m_titleData.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);

    if (!m_title.isEmpty())
        query.bindValue(":MTITLE", m_title);

    if (!query.exec())
    {
        MythDB::DBError("PrevRecordedList::LoadDates", query);
        return false;
    }

    // Create "Last two weeks" entry
    // It is identified by bogus date of 0000/00

    ProgramInfo *program = new ProgramInfo();
    program->SetRecordingStartTime(QDateTime::currentDateTime());
    program->SetTitle(tr("Last two weeks"), "0000/00");
    m_titleData.push_back(program);

    while (query.next())
    {
        int year(query.value(0).toInt());
        int month(query.value(1).toInt());
        program = new ProgramInfo();
        QDate startdate(year,month,1);
        QDateTime starttime(startdate);
        program->SetRecordingStartTime(starttime);
        QString date = QString("%1/%2")
            .arg(year,4,10,QChar('0')).arg(month,2,10,QChar('0'));
        QLocale locale = gCoreContext->GetLocale()->ToQLocale();
        QString title = QString("%1 %2").
            arg(locale.monthName(month)).arg(year);
        program->SetTitle(title, date);
        m_titleData.push_back(program);
    }
    if (m_reverseSort)
        std::stable_sort(m_titleData.begin(), m_titleData.end(),
            comp_sortdate_lt_rev);
    else
        std::stable_sort(m_titleData.begin(), m_titleData.end(),
            comp_sortdate_lt);
    return true;
}

void PrevRecordedList::UpdateTitleList(void)
{
    UpdateList(m_titleList, &m_titleData, false);
}

void PrevRecordedList::UpdateShowList(void)
{
    UpdateList(m_showList, &m_showData, true);
}

void PrevRecordedList::UpdateList(MythUIButtonList *bnList,
    ProgramList *progData, bool isShows)
{
    bnList->Reset();
    for (size_t i = 0; i < progData->size(); ++i)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(bnList, "", QVariant::fromValue((*progData)[i]));
        InfoMap infoMap;
        (*progData)[i]->ToMap(infoMap,true);
        QString state;
        if (isShows)
        {
            QString partTitle;
            if (m_titleGroup)
                partTitle = infoMap["subtitle"];
            else
                partTitle = infoMap["titlesubtitle"];
            infoMap["parttitle"] = partTitle;
            state = RecStatus::toUIState((*progData)[i]->GetRecordingStatus());
            if ((state == "warning"))
                state = "disabled";
        }
        else
            infoMap["buttontext"] = infoMap["title"];

        item->SetTextFromMap(infoMap, state);
    }
}

void PrevRecordedList::updateInfo(void)
{
    if (m_help1Text)
        m_help1Text->Reset();
    if (m_help2Text)
        m_help2Text->Reset();

    if (!m_showData.empty())
    {
        InfoMap infoMap;
        m_showData[m_showList->GetCurrentPos()]->ToMap(infoMap,true);
        SetTextFromMap(infoMap);
        m_infoMap = infoMap;
    }
    else
    {
        ResetMap(m_infoMap);

        if (m_titleGroup)
        {
            m_titleList->SetLCDTitles(tr("Programs"), "title");
            m_showList->SetLCDTitles(tr("Episodes"), "startdate|parttitle");
            if (m_help1Text)
                m_help1Text->SetText(tr("Select a program..."));
            if (m_help2Text)
                m_help2Text->SetText(tr(
                "Select the title of the program you wish to find. "
                "When finished return with the left arrow key. "
                "To search by date press 1."));
            if (m_curviewText)
            {
                if (m_reverseSort)
                    m_curviewText->SetText(tr("Reverse Title","Sort sequence"));
                else
                    m_curviewText->SetText(tr("Title","Sort sequence"));
            }
        }
        else
        {
            m_titleList->SetLCDTitles(tr("Dates"), "title");
            m_showList->SetLCDTitles(tr("Programs"), "startdate|parttitle");
            if (m_help1Text)
                m_help1Text->SetText(tr("Select a month ..."));
            if (m_help2Text)
                m_help2Text->SetText(tr(
                "Select a month to search. "
                "When finished return with the left arrow key. "
                "To search by title press 2."));
            if (m_curviewText)
            {
                if (m_reverseSort)
                    m_curviewText->SetText(tr("Reverse Time","Sort sequence"));
                else
                    m_curviewText->SetText(tr("Time","Sort sequence"));
            }
        }
    }
}

void PrevRecordedList::showListLoseFocus(void)
{
    m_showData.clear();
    m_showList->Reset();
    updateInfo();
}

void PrevRecordedList::showListTakeFocus(void)
{
    m_loadShows = true;
    LoadInBackground();
}

void PrevRecordedList::LoadShowsByTitle(void)
{
    MSqlBindings bindings;
    QString sql = " AND oldrecorded.title = :TITLE " + m_where;
    uint selected = m_titleList->GetCurrentPos();
    if (selected < m_titleData.size())
        bindings[":TITLE"] = m_titleData[selected]->GetTitle();
    else
        bindings[":TITLE"] = "";
    if (!m_title.isEmpty())
        bindings[":MTITLE"] = m_title;
    m_showData.clear();
    LoadFromOldRecorded(m_showData, sql, bindings);
}

void PrevRecordedList::LoadShowsByDate(void)
{
    MSqlBindings bindings;
    int selected = m_titleList->GetCurrentPos();
    QString sortTitle = m_titleData[selected]->GetSortTitle();
    QStringList dateParts = sortTitle.split('/');
    if (dateParts.size() != 2)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid sort Date: %1").arg(sortTitle));
        return;
    }
    QString sortorder;
    if (m_reverseSort)
        sortorder = "DESC";
    QString sql;
    if (dateParts[0] == "0000")
        sql = "AND TIMESTAMPDIFF(DAY, starttime, NOW()) < 14 ";
    else
    {
        sql =
        " AND YEAR(CONVERT_TZ(starttime,'UTC','SYSTEM')) = :YEAR "
        " AND MONTH(CONVERT_TZ(starttime,'UTC','SYSTEM')) = :MONTH ";
        bindings[":YEAR"] = dateParts[0];
        bindings[":MONTH"] = dateParts[1];
    }
    sql = sql + m_where + QString(" ORDER BY starttime %1 ").arg(sortorder);
    if (!m_title.isEmpty())
        bindings[":MTITLE"] = m_title;
    m_showData.clear();
    LoadFromOldRecorded(m_showData, sql, bindings);
}

bool PrevRecordedList::keyPressEvent(QKeyEvent *e)
{
    if (!m_allowEvents)
        return true;

    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(e))
    {
        m_allowEvents = true;
        return true;
    }

    m_allowEvents = false;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress(
        "TV Frontend", e, actions);

    bool needUpdate = false;
    for (uint i = 0; i < uint(actions.size()) && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        if (action == "CUSTOMEDIT")
             EditCustom();
        else if (action == "EDIT")
             EditScheduled();
        else if (action == "DELETE")
             ShowDeleteOldEpisodeMenu();
        else if (action == "DETAILS" || action == "INFO")
             ShowDetails();
        else if (action == "GUIDE")
             ShowGuide();
        else if (action == "UPCOMING")
            ShowUpcoming();
        else if (action == "1")
        {
            if (m_titleGroup)
            {
                m_titleGroup = false;
                m_reverseSort = true;
            }
            else
            {
                m_reverseSort = !m_reverseSort;
            }
            needUpdate = true;
        }
        else if (action == "2")
        {
            if (!m_titleGroup)
            {
                m_titleGroup = true;
                m_reverseSort = false;
            }
            else
            {
                m_reverseSort = !m_reverseSort;
            }
            needUpdate = true;
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(e))
        handled = true;

    if (needUpdate)
    {
        m_loadShows = false;
        LoadInBackground();
    }

    m_allowEvents = true;

    return handled;
}

void PrevRecordedList::ShowMenu(void)
{
    MythMenu *sortMenu = new MythMenu(tr("Sort Options"), this, "sortmenu");
    sortMenu->AddItem(tr("Reverse Sort Order"));
    sortMenu->AddItem(tr("Sort By Title"));
    sortMenu->AddItem(tr("Sort By Time"));

    MythMenu *menu = new MythMenu(tr("List Options"), this, "menu");

    menu->AddItem(tr("Sort"), nullptr, sortMenu);

    ProgramInfo *pi = GetCurrentProgram();
    if (pi)
    {
        menu->AddItem(tr("Edit Schedule"),   SLOT(EditScheduled()));
        menu->AddItem(tr("Custom Edit"),     SLOT(EditCustom()));
        menu->AddItem(tr("Program Details"), SLOT(ShowDetails()));
        menu->AddItem(tr("Upcoming"), SLOT(ShowUpcoming()));
        menu->AddItem(tr("Channel Search"), SLOT(ShowChannelSearch()));
    }
    menu->AddItem(tr("Program Guide"),   SLOT(ShowGuide()));
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "menuPopup");

    if (!menuPopup->Create())
    {
        delete menuPopup;
        return;
    }

    popupStack->AddScreen(menuPopup);
}

void PrevRecordedList::ShowItemMenu(void)
{
    MythMenu *menu = new MythMenu(tr("Recording Options"), this, "menu");

    ProgramInfo *pi = GetCurrentProgram();
    if (pi)
    {
        if (pi->IsDuplicate())
            menu->AddItem(tr("Allow this episode to re-record"),   SLOT(AllowRecord()));
        else
            menu->AddItem(tr("Never record this episode"), SLOT(PreventRecord()));
        menu->AddItem(tr("Remove this episode from the list"),
            SLOT(ShowDeleteOldEpisodeMenu()));
        menu->AddItem(tr("Remove all episodes for this title"),
            SLOT(ShowDeleteOldSeriesMenu()));
    }
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "menuPopup");

    if (!menuPopup->Create())
    {
        delete menuPopup;
        return;
    }

    popupStack->AddScreen(menuPopup);
}

void PrevRecordedList::customEvent(QEvent *event)
{
    bool needUpdate = false;

    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        int     buttonnum  = dce->GetResult();

        if (resultid == "sortmenu")
        {
            switch (buttonnum)
            {
                case 0:
                    m_reverseSort = !m_reverseSort;
                    needUpdate    = true;
                    break;
                case 1:
                    m_titleGroup   = true;
                    m_reverseSort = false;
                    needUpdate    = true;
                    break;
                case 2:
                    m_titleGroup   = false;
                    m_reverseSort = true;
                    needUpdate    = true;
                    break;
            }
        }
        else if (resultid == "deleterule")
        {
            RecordingRule *record =
                dce->GetData().value<RecordingRule *>();
            if (record && buttonnum > 0 && !record->Delete())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to delete recording rule");
            }
            delete record;
        }
        else
        {
            ScheduleCommon::customEvent(event);
        }
    }
    else if (event->type() == ScreenLoadCompletionEvent::kEventType)
    {
        ScreenLoadCompletionEvent *slce = (ScreenLoadCompletionEvent*)(event);
        QString id = slce->GetId();

        if (id == objectName())
        {
            // CloseBusyPopup(); // opened by LoadInBackground()
            if (m_loadShows)
            {
                UpdateShowList();
                CloseBusyPopup(); // opened by LoadInBackground()
            }
            else
            {
                UpdateTitleList();
                m_showData.clear();
                m_showList->Reset();
                updateInfo();
                CloseBusyPopup(); // opened by LoadInBackground()
                SetFocusWidget(m_titleList);
            }
        }
    }

    if (needUpdate)
    {
        m_loadShows = false;
        LoadInBackground();
    }
}

void PrevRecordedList::AllowRecord(void)
{
    ProgramInfo *pi = GetCurrentProgram();
    if (pi)
    {
        int pos = m_showList->GetCurrentPos();
        RecordingInfo ri(*pi);
        ri.ForgetHistory();
        showListTakeFocus();
        updateInfo();
        m_showList->SetItemCurrent(pos);
    }
}

void PrevRecordedList::PreventRecord(void)
{
    ProgramInfo *pi = GetCurrentProgram();
    if (pi)
    {
        int pos = m_showList->GetCurrentPos();
        RecordingInfo ri(*pi);
        ri.SetDupHistory();
        showListTakeFocus();
        updateInfo();
        m_showList->SetItemCurrent(pos);
    }
}


ProgramInfo *PrevRecordedList::GetCurrentProgram(void) const
{
    int pos = m_showList->GetCurrentPos();
    if (pos >= 0 && pos < (int) m_showData.size())
        return m_showData[pos];
    return nullptr;
}

void PrevRecordedList::ShowDeleteOldEpisodeMenu(void)
{
    ProgramInfo *pi = GetCurrentProgram();

    if (!pi)
        return;

    QString message = tr("Delete this episode of '%1' from the previously recorded history?").arg(pi->GetTitle());

    ShowOkPopup(message, this, SLOT(DeleteOldEpisode(bool)), true);
}

void PrevRecordedList::DeleteOldEpisode(bool ok)
{
    ProgramInfo *pi = GetCurrentProgram();
    if (!ok || !pi)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM oldrecorded "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");
    query.bindValue(":CHANID",    pi->GetChanID());
    query.bindValue(":STARTTIME", pi->GetScheduledStartTime());

    if (!query.exec())
        MythDB::DBError("ProgLister::DeleteOldEpisode", query);

    ScheduledRecording::RescheduleCheck(*pi, "DeleteOldEpisode");

    // Delete the current item from both m_showData and m_showList.
    ProgramList::iterator it =
        m_showData.begin() + m_showList->GetCurrentPos();
    m_showData.erase(it);
    MythUIButtonListItem *item = m_showList->GetItemCurrent();
    m_showList->RemoveItem(item);
}

void PrevRecordedList::ShowDeleteOldSeriesMenu(void)
{
    ProgramInfo *pi = GetCurrentProgram();

    if (!pi)
        return;

    QString message = tr("Delete all episodes of '%1' from the previously recorded history?").arg(pi->GetTitle());

    ShowOkPopup(message, this, SLOT(DeleteOldSeries(bool)), true);
}

void PrevRecordedList::DeleteOldSeries(bool ok)
{
    ProgramInfo *pi = GetCurrentProgram();
    if (!ok || !pi)
        return;
    QString title = pi->GetTitle();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM oldrecorded "
                  "WHERE title = :TITLE AND future = 0");
    query.bindValue(":TITLE", title);
    if (!query.exec())
        MythDB::DBError("ProgLister::DeleteOldSeries -- delete", query);

    // Set the programid to the special value of "**any**" which the
    // scheduler recognizes to mean the entire series was deleted.
    RecordingInfo tempri(*pi);
    tempri.SetProgramID("**any**");
    ScheduledRecording::RescheduleCheck(tempri, "DeleteOldSeries");

    // Delete the matching items from both m_showData and m_showList.
    int pos = 0;
    ProgramList::iterator it = m_showData.begin();
    while (pos < (int)m_showData.size())
    {
        if ((*it)->GetTitle() == title)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Deleting %1 at pos %2")
                .arg(title).arg(pos));
            it = m_showData.erase(it);
            MythUIButtonListItem *item = m_showList->GetItemAt(pos);
            m_showList->RemoveItem(item);
        }
        else
        {
            ++pos;
            ++it;
        }
    }
}
