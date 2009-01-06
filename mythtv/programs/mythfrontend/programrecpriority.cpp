
#include <iostream>
#include <vector>
using namespace std;

#include <QDateTime>
#include <QRegExp>

#include "programrecpriority.h"
#include "scheduledrecording.h"
#include "customedit.h"
#include "proglist.h"

#include "mythdb.h"
#include "mythverbose.h"
#include "remoteutil.h"

#include "mythuihelper.h"
#include "mythuibuttonlist.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythdialogbox.h"

// overloaded version of ProgramInfo with additional recording priority
// values so we can keep everything together and don't
// have to hit the db mulitiple times
ProgramRecPriorityInfo::ProgramRecPriorityInfo(void) :
    ProgramInfo(),
    recTypeRecPriority(0), recType(kNotRecording),
    matchCount(0),         recCount(0),
    avg_delay(0),          autoRecPriority(0)
{
}

ProgramRecPriorityInfo::ProgramRecPriorityInfo(
    const ProgramRecPriorityInfo &other) :
    ProgramInfo::ProgramInfo(other),
    recTypeRecPriority(other.recTypeRecPriority),
    recType(other.recType),
    matchCount(other.matchCount),
    recCount(other.recCount),
    avg_delay(other.avg_delay),
    autoRecPriority(other.autoRecPriority)
{
    // TODO CHECK: should last_record be initialized too? -- dtk 22-12-2008
}

ProgramRecPriorityInfo &ProgramRecPriorityInfo::operator=(
    const ProgramInfo &other)
{
    (*(ProgramInfo*)(this)) = other;

#if 0
    // TODO: check if these really should be initialized here..
    //       seems like they should, but I don't want to break
    //       anything... -- dtk 22-12-2008
    recTypeRecPriority = 0;
    recType            = kNotRecording;
    matchCount         = 0;
    recCount           = 0;
    last_record        = QDateTime();
    avg_delay          = 0;
    autoRecPriority    = 0;
#endif

    return *this;
}


typedef struct RecPriorityInfo
{
    ProgramRecPriorityInfo *prog;
    int cnt;
} RecPriorityInfo;

class titleSort
{
    public:
        titleSort(bool m_reverseSort = false) {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            if (a.prog->sortTitle != b.prog->sortTitle)
            {
                if (m_reverse)
                    return (a.prog->sortTitle < b.prog->sortTitle);
                else
                    return (a.prog->sortTitle > b.prog->sortTitle);
            }

            int finalA = a.prog->recpriority + a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return typeA < typeB;
                else
                    return typeA > typeB;
            }

            if (m_reverse)
                return a.prog->recordid < b.prog->recordid;
            else
                return a.prog->recordid > b.prog->recordid;
        }

    private:
        bool m_reverse;
};

class programRecPrioritySort
{
    public:
        programRecPrioritySort(bool m_reverseSort = false)
                               {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            int finalA = a.prog->recpriority + a.prog->autoRecPriority +
                         a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + b.prog->autoRecPriority +
                         b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return typeA < typeB;
                else
                    return typeA > typeB;
            }

            if (m_reverse)
                return a.prog->recordid < b.prog->recordid;
            else
                return a.prog->recordid > b.prog->recordid;
        }

    private:
        bool m_reverse;
};

class programRecTypeSort
{
    public:
        programRecTypeSort(bool m_reverseSort = false)
                               {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return (typeA < typeB);
                else
                    return (typeA > typeB);
            }

            int finalA = a.prog->recpriority + a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            if (m_reverse)
                return a.prog->recordid < b.prog->recordid;
            else
                return a.prog->recordid > b.prog->recordid;
        }

    private:
        bool m_reverse;
};

class programCountSort
{
    public:
        programCountSort(bool m_reverseSort = false) {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            int countA = a.prog->matchCount;
            int countB = b.prog->matchCount;
            int recCountA = a.prog->recCount;
            int recCountB = b.prog->recCount;

            if (countA != countB)
            {
                if (m_reverse)
                    return countA > countB;
                else
                    return countA < countB;
            }
            if (recCountA != recCountB)
            {
                if (m_reverse)
                    return recCountA > recCountB;
                else
                    return recCountA < recCountB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programRecCountSort
{
    public:
        programRecCountSort(bool m_reverseSort=false)
        {
            m_reverse = m_reverseSort;
        }

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            int countA = a.prog->matchCount;
            int countB = b.prog->matchCount;
            int recCountA = a.prog->recCount;
            int recCountB = b.prog->recCount;

            if (recCountA != recCountB)
            {
                if (m_reverse)
                    return recCountA > recCountB;
                else
                    return recCountA < recCountB;
            }
            if (countA != countB)
            {
                if (m_reverse)
                    return countA > countB;
                else
                    return countA < countB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programLastRecordSort
{
    public:
        programLastRecordSort(bool m_reverseSort=false)
            {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            QDateTime lastRecA = a.prog->last_record;
            QDateTime lastRecB = b.prog->last_record;

            if (lastRecA != lastRecB)
            {
                if (m_reverse)
                    return lastRecA > lastRecB;
                else
                    return lastRecA < lastRecB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programAvgDelaySort
{
    public:
        programAvgDelaySort(bool m_reverseSort=false)
            {m_reverse = m_reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            int avgA = a.prog->avg_delay;
            int avgB = b.prog->avg_delay;

            if (avgA != avgB)
            {
                if (m_reverse)
                    return avgA < avgB;
                else
                    return avgA > avgB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

////////////////////////////////////////////////////////

ProgramRecPriority::ProgramRecPriority(MythScreenStack *parent)
                   : MythScreenType(parent, "ProgramRecPriority")
{
    m_sortType = (SortType)gContext->GetNumSetting("ProgramRecPrioritySorting",
                                                 (int)byTitle);
    m_reverseSort = gContext->GetNumSetting("ProgramRecPriorityReverse", 0);
}

ProgramRecPriority::~ProgramRecPriority()
{
}

bool ProgramRecPriority::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "programrecpriority", this))
        return false;

    m_programList = dynamic_cast<MythUIButtonList *> (GetChild("programs"));

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));
    m_schedInfoText = dynamic_cast<MythUIText *> (GetChild("scheduleinfo"));
    m_typeText = dynamic_cast<MythUIText *> (GetChild("recordingtype"));
    m_rectypePriorityText = dynamic_cast<MythUIText *>
                                                 (GetChild("rectypepriority"));
    m_recPriorityText = dynamic_cast<MythUIText *> (GetChild("recpriority"));
    m_recPriorityBText = dynamic_cast<MythUIText *> (GetChild("recpriorityb"));
    m_finalPriorityText = dynamic_cast<MythUIText *> (GetChild("finalpriority"));

    if (!m_programList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_programList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));
    connect(m_programList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(edit(MythUIButtonListItem*)));

    FillList();

    BuildFocusList();

    return true;
}

bool ProgramRecPriority::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "RANKINC")
            changeRecPriority(1);
        else if (action == "RANKDEC")
            changeRecPriority(-1);
        else if (action == "ESCAPE")
        {
            saveRecPriority();
            gContext->SaveSetting("ProgramRecPrioritySorting",
                                    (int)m_sortType);
            gContext->SaveSetting("ProgramRecPriorityReverse",
                                    (int)m_reverseSort);
            Close();
        }
        else if (action == "1")
        {
            if (m_sortType != byTitle)
            {
                m_sortType = byTitle;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "2")
        {
            if (m_sortType != byRecPriority)
            {
                m_sortType = byRecPriority;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "4")
        {
            if (m_sortType != byRecType)
            {
                m_sortType = byRecType;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "5")
        {
            if (m_sortType != byCount)
            {
                m_sortType = byCount;
                m_reverseSort = false;
            }
            else
            {
                m_reverseSort = !m_reverseSort;
            }
            SortList();
        }
        else if (action == "6")
        {
            if (m_sortType != byRecCount)
            {
                m_sortType = byRecCount;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "7")
        {
            if (m_sortType != byLastRecord)
            {
                m_sortType = byLastRecord;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "8")
        {
            if (m_sortType != byAvgDelay)
            {
                m_sortType = byAvgDelay;
                m_reverseSort = false;
            }
            else
                m_reverseSort = !m_reverseSort;
            SortList();
        }
        else if (action == "PREVVIEW" || action == "NEXTVIEW")
        {
            m_reverseSort = false;
            if (m_sortType == byTitle)
                m_sortType = byRecPriority;
            else if (m_sortType == byRecPriority)
                m_sortType = byRecType;
            else
                m_sortType = byTitle;
            SortList();
        }
        else if (action == "SELECT" || action == "MENU" ||
                    action == "INFO")
        {
            saveRecPriority();
            edit(m_programList->GetItemCurrent());
        }
        else if (action == "CUSTOMEDIT")
        {
            saveRecPriority();
            customEdit();
        }
        else if (action == "DELETE")
        {
            saveRecPriority();
            remove();
        }
        else if (action == "UPCOMING")
        {
            saveRecPriority();
            upcoming();
        }
        else if (action == "DETAILS")
            details();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ProgramRecPriority::edit(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (pgRecInfo)
    {
        int recid = 0;

        {
            ScheduledRecording *record = new ScheduledRecording();
            record->loadByID(pgRecInfo->recordid);
            if (record->getSearchType() == kNoSearch)
                record->loadByProgram(pgRecInfo);
            record->exec();
            recid = record->getRecordID();
            record->deleteLater();
        }

        // We need to refetch the recording priority values since the Advanced
        // Recording Options page could've been used to change them

        if (!recid)
            recid = pgRecInfo->getRecordID();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT recpriority, type, inactive "
                      "FROM record "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", recid);
        if (!query.exec())
        {
            MythDB::DBError("Get new recording priority query", query);
        }
        else if (query.next())
        {
            int recPriority = query.value(0).toInt();
            int rectype = query.value(1).toInt();
            int inactive = query.value(2).toInt();

            int rtRecPriors[11];
            rtRecPriors[0] = 0;
            rtRecPriors[kSingleRecord] =
                gContext->GetNumSetting("SingleRecordRecPriority", 1);
            rtRecPriors[kTimeslotRecord] =
                gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
            rtRecPriors[kChannelRecord] =
                gContext->GetNumSetting("ChannelRecordRecPriority", 0);
            rtRecPriors[kAllRecord] =
                gContext->GetNumSetting("AllRecordRecPriority", 0);
            rtRecPriors[kWeekslotRecord] =
                gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
            rtRecPriors[kFindOneRecord] =
                gContext->GetNumSetting("FindOneRecordRecPriority", -1);
            rtRecPriors[kOverrideRecord] =
                gContext->GetNumSetting("OverrideRecordRecPriority", 0);
            rtRecPriors[kDontRecord] =
                gContext->GetNumSetting("OverrideRecordRecPriority", 0);
            rtRecPriors[kFindDailyRecord] =
                gContext->GetNumSetting("FindOneRecordRecPriority", -1);
            rtRecPriors[kFindWeeklyRecord] =
                gContext->GetNumSetting("FindOneRecordRecPriority", -1);

            // set the recording priorities of that program
            pgRecInfo->recpriority = recPriority;
            pgRecInfo->recType = (RecordingType)rectype;
            pgRecInfo->recTypeRecPriority = rtRecPriors[pgRecInfo->recType];
            // also set the m_origRecPriorityData with new recording
            // priority so we don't save to db again when we exit
            m_origRecPriorityData[pgRecInfo->recordid] =
                                                    pgRecInfo->recpriority;
            // also set the active/inactive state
            pgRecInfo->recstatus = inactive ? rsInactive : rsUnknown;

            SortList();
        }
        else
        {
            RemoveItemFromList(item);
        }

        countMatches();
    }
}

void ProgramRecPriority::customEdit(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                        "customedit", pgRecInfo);
    ce->exec();
    delete ce;
}

void ProgramRecPriority::remove(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    ScheduledRecording *record = new ScheduledRecording();
    int recid = pgRecInfo->recordid;
    record->loadByID(recid);

    QString message =
        tr("Delete '%1' %2 rule?").arg(record->getRecordTitle())
                                  .arg(pgRecInfo->RecTypeText());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
        popupStack->AddScreen(dialog);

    connect(dialog, SIGNAL(haveResult(bool)),
            this, SLOT(doRemove(bool)));

    record->deleteLater();
}

void ProgramRecPriority::doRemove(bool doRemove)
{
    if (doRemove)
    {
        MythUIButtonListItem *item = m_programList->GetItemCurrent();
        if (!item)
           return;

        ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

        ScheduledRecording *record = new ScheduledRecording();
        int recid = pgRecInfo->recordid;
        record->loadByID(recid);
        record->remove();

        RemoveItemFromList(item);

        countMatches();

        ScheduledRecording::signalChange(recid);

        UpdateList();

        record->deleteLater();
    }
}

void ProgramRecPriority::deactivate(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    if (pgRecInfo)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT inactive "
                      "FROM record "
                      "WHERE recordid = :RECID");
        query.bindValue(":RECID", pgRecInfo->recordid);

        int inactive = 0;
        if (!query.exec())
        {
            MythDB::DBError("ProgramRecPriority::deactivate()", query);
        }
        else if (query.next())
        {
            inactive = query.value(0).toInt();
            if (inactive)
                inactive = 0;
            else
                inactive = 1;

            query.prepare("UPDATE record "
                          "SET inactive = :INACTIVE "
                          "WHERE recordid = :RECID");
            query.bindValue(":INACTIVE", inactive);
            query.bindValue(":RECID", pgRecInfo->recordid);

            if (!query.exec())
            {
                MythDB::DBError(
                    "Update recording schedule inactive query", query);
            }
            else
            {
                ScheduledRecording::signalChange(0);
                pgRecInfo->recstatus = inactive ? rsInactive : rsUnknown;
                item->DisplayState("disabled", "status");
            }
        }
    }
}

void ProgramRecPriority::upcoming(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());


    if (m_listMatch[pgRecInfo->recordid] > 0)
    {
        ScheduledRecording *record = new ScheduledRecording();
        record->loadByID(pgRecInfo->recordid);
        record->runRuleList();
        record->deleteLater();
    }
    else
    {
        ProgLister *pl = NULL;
        QString trimTitle = pgRecInfo->title;
        trimTitle.remove(QRegExp(" \\(.*\\)$"));
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        pl = new ProgLister(mainStack, plTitle, trimTitle, "");
        if (pl->Create())
            mainStack->AddScreen(pl);
        else
            delete pl;
    }
}

void ProgramRecPriority::details(void)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo = qVariantValue<ProgramRecPriorityInfo *>
                                                            (item->GetData());

    pgRecInfo->showDetails();
}

void ProgramRecPriority::changeRecPriority(int howMuch)
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo =
                        qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    int tempRecPriority;
    // inc/dec recording priority
    tempRecPriority = pgRecInfo->recpriority + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        pgRecInfo->recpriority = tempRecPriority;

        // order may change if sorting by recording priority, so resort
        if (m_sortType == byRecPriority)
            SortList();
        else
        {
            // No need to re-fill the entire list, just update this entry
            int progRecPriority = pgRecInfo->recpriority;
            int finalRecPriority = progRecPriority +
                                    pgRecInfo->autoRecPriority +
                                    pgRecInfo->recTypeRecPriority;

            item->SetText(QString::number(progRecPriority), "progpriority");
            item->SetText(QString::number(finalRecPriority), "finalpriority");
        }
    }
}

void ProgramRecPriority::saveRecPriority(void)
{
    QMap<QString, ProgramRecPriorityInfo>::Iterator it;

    for (it = m_programData.begin(); it != m_programData.end(); ++it)
    {
        ProgramRecPriorityInfo *progInfo = &(*it);
        int key = progInfo->recordid;

        // if this program's recording priority changed from when we entered
        // save new value out to db
        if (progInfo->recpriority != m_origRecPriorityData[key])
            progInfo->ApplyRecordRecPriorityChange(progInfo->recpriority);
    }
}

void ProgramRecPriority::FillList(void)
{
    int cnt = 999, rtRecPriors[11];
    vector<ProgramInfo *> recordinglist;

    int autopriority = gContext->GetNumSetting("AutoRecPriority", 0);

    m_programData.clear();
    m_sortedProgram.clear();

    RemoteGetAllScheduledRecordings(recordinglist);

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        m_programData[QString::number(cnt)] = *(*pgiter);

        // save recording priority value in map so we don't have to
        // save all program's recording priority values when we exit
        m_origRecPriorityData[(*pgiter)->recordid] = (*pgiter)->recpriority;

        delete (*pgiter);
        cnt--;
    }

    // get all the recording type recording priority values
    rtRecPriors[0] = 0;
    rtRecPriors[kSingleRecord] =
        gContext->GetNumSetting("SingleRecordRecPriority", 1);
    rtRecPriors[kTimeslotRecord] =
        gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
    rtRecPriors[kChannelRecord] =
        gContext->GetNumSetting("ChannelRecordRecPriority", 0);
    rtRecPriors[kAllRecord] =
        gContext->GetNumSetting("AllRecordRecPriority", 0);
    rtRecPriors[kWeekslotRecord] =
        gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
    rtRecPriors[kFindOneRecord] =
        gContext->GetNumSetting("FindOneRecordRecPriority", -1);
    rtRecPriors[kOverrideRecord] =
        gContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[kDontRecord] =
        gContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[kFindDailyRecord] =
        gContext->GetNumSetting("FindOneRecordRecPriority", -1);
    rtRecPriors[kFindWeeklyRecord] =
        gContext->GetNumSetting("FindOneRecordRecPriority", -1);

    // get recording types associated with each program from db
    // (hope this is ok to do here, it's so much lighter doing
    // it all at once than once per program)

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid, title, chanid, starttime, startdate, "
                   "type, inactive, last_record, avg_delay "
                   "FROM record;");

    if (!result.exec())
    {
        MythDB::DBError("Get program recording priorities query", result);
    }
    else if (result.next())
    {
        countMatches();
        do {
            int recordid = result.value(0).toInt();
            QString title = result.value(1).toString();
            QString chanid = result.value(2).toString();
            QString tempTime = result.value(3).toString();
            QString tempDate = result.value(4).toString();
            RecordingType recType = (RecordingType)result.value(5).toInt();
            int recTypeRecPriority = rtRecPriors[recType];
            int inactive = result.value(6).toInt();
            QDateTime lastrec = result.value(7).toDateTime();
            int avgd = result.value(8).toInt();

            // find matching program in m_programData and set
            // recTypeRecPriority and recType
            QMap<QString, ProgramRecPriorityInfo>::Iterator it;
            for (it = m_programData.begin(); it != m_programData.end(); ++it)
            {
                ProgramRecPriorityInfo *progInfo = &(*it);

                if (progInfo->recordid == recordid)
                {
                    progInfo->sortTitle = progInfo->title;
                    progInfo->sortTitle.remove(QRegExp(tr("^(The |A |An )")));

                    progInfo->recTypeRecPriority = recTypeRecPriority;
                    progInfo->recType = recType;
                    progInfo->matchCount = m_listMatch[progInfo->recordid];
                    progInfo->recCount = m_recMatch[progInfo->recordid];
                    progInfo->last_record = lastrec;
                    progInfo->avg_delay = avgd;

                    if (autopriority)
                        progInfo->autoRecPriority =
                            autopriority - (progInfo->avg_delay *
                            (autopriority * 2 + 1) / 200);
                    else
                        progInfo->autoRecPriority = 0;

                    if (inactive)
                        progInfo->recstatus = rsInactive;
                    else if (m_conMatch[progInfo->recordid] > 0)
                        progInfo->recstatus = rsConflict;
                    else if (m_nowMatch[progInfo->recordid] > 0)
                        progInfo->recstatus = rsRecording;
                    else if (m_recMatch[progInfo->recordid] > 0)
                        progInfo->recstatus = rsWillRecord;
                    else
                        progInfo->recstatus = rsUnknown;

                    break;
                }
            }
        } while (result.next());
    }

    SortList();
}

void ProgramRecPriority::countMatches()
{
    m_listMatch.clear();
    m_conMatch.clear();
    m_nowMatch.clear();
    m_recMatch.clear();
    ProgramList schedList;
    schedList.FromScheduler();
    QDateTime now = QDateTime::currentDateTime();

    ProgramList::const_iterator it = schedList.begin();
    for (; it != schedList.end(); ++it)
    {
        const ProgramInfo *s = *it;
        if (s->recendts > now && s->recstatus != rsNotListed)
        {
            m_listMatch[s->recordid]++;
            if (s->recstatus == rsConflict ||
                s->recstatus == rsOffLine)
                m_conMatch[s->recordid]++;
            else if (s->recstatus == rsWillRecord)
                m_recMatch[s->recordid]++;
            else if (s->recstatus == rsRecording)
            {
                m_nowMatch[s->recordid]++;
                m_recMatch[s->recordid]++;
            }
        }
    }
}

void ProgramRecPriority::SortList()
{
    MythUIButtonListItem *item = m_programList->GetItemCurrent();

    if (item)
        m_currentItem = qVariantValue<ProgramRecPriorityInfo*>(item->GetData());

    int i, j;
    vector<RecPriorityInfo> sortingList;
    QMap<QString, ProgramRecPriorityInfo>::Iterator pit;
    vector<RecPriorityInfo>::iterator sit;
    RecPriorityInfo *recPriorityInfo;

    // copy m_programData into sortingList and make a copy
    // of m_programData in pdCopy
    for (i = 0, pit = m_programData.begin(); pit != m_programData.end();
         ++pit, i++)
    {
        ProgramRecPriorityInfo *progInfo = &(*pit);
        RecPriorityInfo tmp = {progInfo, i};
        sortingList.push_back(tmp);
    }

    // sort sortingList
    switch(m_sortType)
    {
        case byTitle :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          titleSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(), titleSort());
                 break;
        case byRecPriority :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programRecPrioritySort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programRecPrioritySort());
                 break;
        case byRecType :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programRecTypeSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programRecTypeSort());
                 break;
        case byCount :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programCountSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programCountSort());
                 break;
        case byRecCount :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programRecCountSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programRecCountSort());
                 break;
        case byLastRecord :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programLastRecordSort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programLastRecordSort());
                 break;
        case byAvgDelay :
                 if (m_reverseSort)
                     sort(sortingList.begin(), sortingList.end(),
                          programAvgDelaySort(true));
                 else
                     sort(sortingList.begin(), sortingList.end(),
                          programAvgDelaySort());
                 break;
    }

    m_sortedProgram.clear();

    // rebuild m_channelData in sortingList order from m_sortedProgram
    for (i = 0, sit = sortingList.begin(); sit != sortingList.end(); i++, ++sit)
    {
        recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in m_sortedChannel
        for (j = 0, pit = m_programData.begin(); j !=recPriorityInfo->cnt; j++, ++pit);

        m_sortedProgram[QString::number(999-i)] = &(*pit);
    }

    UpdateList();
}

void ProgramRecPriority::UpdateList()
{
    if (!m_currentItem && m_programList->GetItemCurrent())
        m_currentItem = qVariantValue<ProgramRecPriorityInfo*>
                                (m_programList->GetItemCurrent()->GetData());

    m_programList->Reset();

    QMap<QString, ProgramRecPriorityInfo*>::Iterator it;
    MythUIButtonListItem *item;
    for (it = m_sortedProgram.begin(); it != m_sortedProgram.end(); ++it)
    {
        ProgramRecPriorityInfo *progInfo = *it;

        item = new MythUIButtonListItem(m_programList, "",
                                                qVariantFromValue(progInfo));

        int progRecPriority = progInfo->recpriority;
        int finalRecPriority = progRecPriority +
                                progInfo->autoRecPriority +
                                progInfo->recTypeRecPriority;

        QString tempSubTitle = progInfo->title;
        if ((progInfo->rectype == kSingleRecord ||
                progInfo->rectype == kOverrideRecord ||
                progInfo->rectype == kDontRecord) &&
            (progInfo->subtitle).trimmed().length() > 0)
            tempSubTitle = tempSubTitle + " - \"" +
                            progInfo->subtitle + "\"";

        QString state;
        if (progInfo->recType == kDontRecord ||
            progInfo->recstatus == rsInactive)
            state = "disabled";
        else if (m_conMatch[progInfo->recordid] > 0)
            state = "error";
        else if (m_nowMatch[progInfo->recordid] > 0)
            state = "running";
        else if (m_recMatch[progInfo->recordid] > 0)
            state = "normal";

        item->SetText(tempSubTitle, "title", state);
        item->SetText(progInfo->RecTypeChar(), "rectype", state);
        item->SetText(QString::number(progRecPriority), "progpriority", state);
        item->SetText(QString::number(finalRecPriority), "finalpriority", state);
        item->DisplayState(state, "status");

        if (m_currentItem == progInfo)
            m_programList->SetItemCurrent(item);
    }

    m_currentItem = NULL;

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_programData.isEmpty());
}

void ProgramRecPriority::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo = qVariantValue<ProgramRecPriorityInfo *>
                                                            (item->GetData());

    if (!pgRecInfo)
        return;

    int progRecPriority, autorecpri, rectyperecpriority, finalRecPriority;
    RecordingType rectype;

    progRecPriority = pgRecInfo->recpriority;
    autorecpri = pgRecInfo->autoRecPriority;
    rectyperecpriority = pgRecInfo->recTypeRecPriority;
    finalRecPriority = progRecPriority + autorecpri + rectyperecpriority;

    rectype = pgRecInfo->recType;

    QString subtitle = "";
    if (pgRecInfo->subtitle != "(null)" &&
        (pgRecInfo->rectype == kSingleRecord ||
            pgRecInfo->rectype == kOverrideRecord ||
            pgRecInfo->rectype == kDontRecord))
    {
        subtitle = pgRecInfo->subtitle;
    }

    QString matchInfo;
    if (pgRecInfo->recstatus == rsInactive)
        matchInfo = QString("%1 %2").arg(m_listMatch[pgRecInfo->recordid])
                                    .arg(pgRecInfo->RecStatusText());
    else
        matchInfo = QString(tr("Recording %1 of %2"))
                                .arg(m_recMatch[pgRecInfo->recordid])
                                .arg(m_listMatch[pgRecInfo->recordid]);

    subtitle = QString("(%1) %2").arg(matchInfo).arg(subtitle);

    if (m_titleText)
        m_titleText->SetText(pgRecInfo->title);

    if (m_schedInfoText)
        m_schedInfoText->SetText(subtitle);

    if (m_typeText)
    {
        QString text;
        switch (rectype)
        {
            case kSingleRecord:
                text = tr("Recording just this showing");
                break;
            case kOverrideRecord:
                text = tr("Recording with override options");
                break;
            case kWeekslotRecord:
                text = tr("Recording every week");
                break;
            case kTimeslotRecord:
                text = tr("Recording in this timeslot");
                break;
            case kChannelRecord:
                text = tr("Recording on this channel");
                break;
            case kAllRecord:
                text = tr("Recording all showings");
                break;
            case kFindOneRecord:
                text = tr("Recording one showing");
                break;
            case kFindDailyRecord:
                text = tr("Recording a showing daily");
                break;
            case kFindWeeklyRecord:
                text = tr("Recording a showing weekly");
                break;
            case kDontRecord:
                text = tr("Not allowed to record this showing");
                break;
            case kNotRecording:
                text = tr("Not recording this showing");
                break;
            default:
                text = tr("Error!");
                break;
        }
        m_typeText->SetText(text);
    }

    if (m_rectypePriorityText)
        m_rectypePriorityText->SetText(QString::number(rectyperecpriority));

    if (m_recPriorityText)
    {
        QString msg = QString::number(pgRecInfo->recpriority);

        if(autorecpri != 0)
            msg += tr(" + %1 automatic priority (%2hr)")
                        .arg(autorecpri).arg((pgRecInfo->avg_delay));
        m_recPriorityText->SetText(msg);
    }

    if (m_recPriorityBText)
        m_recPriorityBText->SetText(QString::number(progRecPriority +
                                                    autorecpri));

    if (m_finalPriorityText)
        m_finalPriorityText->SetText(QString::number(finalRecPriority));

}

void ProgramRecPriority::RemoveItemFromList(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramRecPriorityInfo *pgRecInfo = qVariantValue<ProgramRecPriorityInfo *>
                                                            (item->GetData());

    if (!pgRecInfo)
        return;

    QMap<QString, ProgramRecPriorityInfo>::iterator it;
    for (it = m_programData.begin(); it != m_programData.end(); it++)
    {
        VERBOSE(VB_IMPORTANT, QString("Iterating %1").arg(it.key()));
        ProgramRecPriorityInfo *value = &(it.value());
        if (value == pgRecInfo)
        {
            VERBOSE(VB_IMPORTANT, QString("Deleting %1").arg(it.key()));
            m_programData.erase(it);
            break;
        }
    }

    SortList();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
