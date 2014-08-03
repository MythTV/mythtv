
#include "schedulecommon.h"

// QT
#include <QCoreApplication>

// libmyth
#include "mythcorecontext.h"
#include "programinfo.h"
#include "remoteutil.h"

// libmythtv
#include "recordinginfo.h"
#include "tvremoteutil.h"

// libmythui
#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"

// mythfrontend
#include "scheduleeditor.h"
#include "progdetails.h"
#include "proglist.h"
#include "customedit.h"
#include "guidegrid.h"

/**
*  \brief Show the Program Details screen
*/
void ScheduleCommon::ShowDetails(void) const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgDetails *details_dialog  = new ProgDetails(mainStack, pginfo);

    if (!details_dialog->Create())
    {
        delete details_dialog;
        return;
    }

    mainStack->AddScreen(details_dialog);
}

/**
*  \brief Show the upcoming recordings for this title
*/
void ScheduleCommon::ShowUpcoming(const QString &title,
                                  const QString &seriesid) const
{
    if (title.isEmpty())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitle, title, seriesid);
    if (pl->Create())
    {
        mainStack->AddScreen(pl);
    }
    else
        delete pl;
}

/**
*  \brief Show the upcoming recordings for this title
*/
void ScheduleCommon::ShowUpcoming(void) const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    if (pginfo->GetChanID() == 0 &&
        pginfo->GetRecordingRuleID() > 0)
        return ShowUpcomingScheduled();

    ShowUpcoming(pginfo->GetTitle(), pginfo->GetSeriesID());
}

/**
*  \brief Show the upcoming recordings for this recording rule
*/
void ScheduleCommon::ShowUpcomingScheduled(void) const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    RecordingInfo ri(*pginfo);
    uint id;

    if ((id = ri.GetRecordingRuleID()) <= 0)
        return ShowUpcoming(pginfo->GetTitle(), pginfo->GetSeriesID());

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plRecordid,
                                    QString::number(id), "");

    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

/**
*  \brief Show the channel search
*/
void ScheduleCommon::ShowChannelSearch() const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plChannel,
                                    QString::number(pginfo->GetChanID()), "",
                                    pginfo->GetScheduledStartTime());
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

/**
*  \brief Show the program guide
*/
void ScheduleCommon::ShowGuide(void) const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    QString startchannel = pginfo->GetChanNum();
    uint startchanid = pginfo->GetChanID();
    QDateTime starttime = pginfo->GetScheduledStartTime();
    GuideGrid::RunProgramGuide(startchanid, startchannel, starttime);
}

/**
*  \brief Create a kSingleRecord or bring up recording dialog.
*/
void ScheduleCommon::QuickRecord(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    if (pginfo->GetRecordingRuleID())
        EditRecording();
    else
    {
        RecordingInfo ri(*pginfo);
        ri.QuickRecord();
        *pginfo = ri;
    }
}

/**
*  \brief Creates a dialog for editing the recording schedule
*/
void ScheduleCommon::EditScheduled(void)
{
    EditScheduled(GetCurrentProgram());
}

/**
*  \brief Creates a dialog for editing the recording schedule
*/
void ScheduleCommon::EditScheduled(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    RecordingInfo ri(*pginfo);
    EditScheduled(&ri);
}

/**
*  \brief Creates a dialog for editing the recording schedule
*/
void ScheduleCommon::EditScheduled(RecordingInfo *recinfo)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, recinfo);
    if (schededit->Create())
        mainStack->AddScreen(schededit);
    else
        delete schededit;
}

/**
*  \brief Creates a dialog for creating a custom recording rule
*/
void ScheduleCommon::EditCustom(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    CustomEdit *ce = new CustomEdit(mainStack, pginfo);
    if (ce->Create())
        mainStack->AddScreen(ce);
    else
        delete ce;
}

/**
*  \brief Creates a dialog for editing an override recording schedule
*/
void ScheduleCommon::MakeOverride(RecordingInfo *recinfo)
{
    if (!recinfo || !recinfo->GetRecordingRuleID())
        return;

    RecordingRule *recrule = new RecordingRule();

    if (!recrule->LoadByProgram(static_cast<ProgramInfo*>(recinfo)))
        LOG(VB_GENERAL, LOG_ERR, "Failed to load by program info");

    if (!recrule->MakeOverride())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to make Override");
        delete recrule;
        return;
    }
    recrule->m_type = kOverrideRecord;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, recrule);
    if (schededit->Create())
        mainStack->AddScreen(schededit);
    else
        delete schededit;
}

/**
*  \brief Show the previous recordings for this recording rule
*/
void ScheduleCommon::ShowPrevious(void) const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, pginfo->GetRecordingRuleID(),
                                    pginfo->GetTitle());
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

/**
*  \brief Creates a dialog for editing the recording status,
*         blocking until user leaves dialog.
*/
void ScheduleCommon::EditRecording(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    RecordingInfo recinfo(*pginfo);

    QString timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = recinfo.toString(ProgramInfo::kTitleSubtitle, " - ");

    message += "\n\n";
    message += toDescription(recinfo.GetRecordingStatus(),
                             recinfo.GetRecordingRuleType(),
                             recinfo.GetRecordingStartTime());

    if (recinfo.GetRecordingStatus() == rsConflict ||
        recinfo.GetRecordingStatus() == rsLaterShowing)
    {
        vector<ProgramInfo *> *confList = RemoteGetConflictList(&recinfo);

        if (!confList->empty())
        {
            message += " ";
            message += tr("The following programs will be recorded instead:");
            message += "\n";
        }

        uint maxi = 0;
        for (; confList->begin() != confList->end() && maxi < 4; maxi++)
        {
            ProgramInfo *p = *confList->begin();
            message += QString("%1 - %2  %3\n")
                .arg(p->GetRecordingStartTime()
                     .toLocalTime().toString(timeFormat))
                .arg(p->GetRecordingEndTime()
                     .toLocalTime().toString(timeFormat))
                .arg(p->toString(ProgramInfo::kTitleSubtitle, " - "));
            delete p;
            confList->erase(confList->begin());
        }
        message += "\n";
        while (!confList->empty())
        {
            delete confList->back();
            confList->pop_back();
        }
        delete confList;
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(message, popupStack,
                                                 "recOptionPopup", true);
    if (!menuPopup->Create())
    {
        delete menuPopup;
        return;
    }
    menuPopup->SetReturnEvent(this, "editrecording");

    QDateTime now = MythDate::current();

    if (recinfo.GetRecordingStatus() == rsUnknown)
    {
        if (recinfo.GetRecordingEndTime() > now)
            menuPopup->AddButton(tr("Record this showing"),
                                 qVariantFromValue(recinfo));
        menuPopup->AddButton(tr("Record all showings"),
                             qVariantFromValue(recinfo));
        if (!recinfo.IsGeneric())
        {
            if (recinfo.GetCategoryType() == ProgramInfo::kCategoryMovie)
                menuPopup->AddButton(tr("Record one showing"),
                                     qVariantFromValue(recinfo));
            else
                menuPopup->AddButton(tr("Record one showing (this episode)"),
                                     qVariantFromValue(recinfo));

        }
        menuPopup->AddButton(tr("Record all showings (this channel)"),
                             qVariantFromValue(recinfo));
        menuPopup->AddButton(tr("Edit recording rule"),
                             qVariantFromValue(recinfo));
    }
    else if (recinfo.GetRecordingStatus() == rsRecording ||
             recinfo.GetRecordingStatus() == rsTuning    ||
             recinfo.GetRecordingStatus() == rsFailing)
    {
        menuPopup->AddButton(tr("Stop this recording"),
                             qVariantFromValue(recinfo));
        menuPopup->AddButton(tr("Modify recording options"),
                             qVariantFromValue(recinfo));
    }
    else
    {
        if (recinfo.GetRecordingStartTime() < now &&
            recinfo.GetRecordingEndTime() > now &&
            recinfo.GetRecordingStatus() != rsDontRecord &&
            recinfo.GetRecordingStatus() != rsNotListed)
        {
            menuPopup->AddButton(tr("Restart this recording"),
                                 qVariantFromValue(recinfo));
        }

        if (recinfo.GetRecordingEndTime() > now &&
            recinfo.GetRecordingRuleType() != kSingleRecord &&
            recinfo.GetRecordingRuleType() != kOverrideRecord &&
            (recinfo.GetRecordingStatus() == rsDontRecord ||
             recinfo.GetRecordingStatus() == rsPreviousRecording ||
             recinfo.GetRecordingStatus() == rsCurrentRecording ||
             recinfo.GetRecordingStatus() == rsEarlierShowing ||
             recinfo.GetRecordingStatus() == rsTooManyRecordings ||
             recinfo.GetRecordingStatus() == rsLaterShowing ||
             recinfo.GetRecordingStatus() == rsRepeat ||
             recinfo.GetRecordingStatus() == rsInactive ||
             recinfo.GetRecordingStatus() == rsNeverRecord))
        {
            menuPopup->AddButton(tr("Record this showing"),
                                 qVariantFromValue(recinfo));
            if (recinfo.GetRecordingStartTime() > now &&
                (recinfo.GetRecordingStatus() == rsPreviousRecording ||
                 recinfo.GetRecordingStatus() == rsNeverRecord))
            {
                menuPopup->AddButton(tr("Forget previous recording"),
                                     qVariantFromValue(recinfo));
            }
        }

        if (recinfo.GetRecordingRuleType() != kSingleRecord &&
            recinfo.GetRecordingRuleType() != kDontRecord &&
            (recinfo.GetRecordingStatus() == rsWillRecord ||
             recinfo.GetRecordingStatus() == rsCurrentRecording ||
             recinfo.GetRecordingStatus() == rsEarlierShowing ||
             recinfo.GetRecordingStatus() == rsTooManyRecordings ||
             recinfo.GetRecordingStatus() == rsConflict ||
             recinfo.GetRecordingStatus() == rsLaterShowing ||
             recinfo.GetRecordingStatus() == rsOffLine))
        {
            if (recinfo.GetRecordingStatus() == rsWillRecord ||
                recinfo.GetRecordingStatus() == rsConflict)
                menuPopup->AddButton(tr("Don't record this showing"),
                                     qVariantFromValue(recinfo));

            const RecordingDupMethodType dupmethod =
                recinfo.GetDuplicateCheckMethod();
            if (recinfo.GetRecordingRuleType() != kOverrideRecord &&
                !((recinfo.GetFindID() == 0 ||
                   !IsFindApplicable(recinfo)) &&
                  recinfo.GetCategoryType() == ProgramInfo::kCategorySeries &&
                  recinfo.GetProgramID().contains(QRegExp("0000$"))) &&
                ((!(dupmethod & kDupCheckNone) &&
                  !recinfo.GetProgramID().isEmpty() &&
                  (recinfo.GetFindID() != 0 ||
                   !IsFindApplicable(recinfo))) ||
                 ((dupmethod & kDupCheckSub) &&
                  !recinfo.GetSubtitle().isEmpty()) ||
                 ((dupmethod & kDupCheckDesc) &&
                  !recinfo.GetDescription().isEmpty()) ||
                 ((dupmethod & kDupCheckSubThenDesc) &&
                  (!recinfo.GetSubtitle().isEmpty() ||
                   !recinfo.GetDescription().isEmpty())) ))
            {
                menuPopup->AddButton(tr("Never record this episode"),
                                     qVariantFromValue(recinfo));
            }
        }

        if (recinfo.GetRecordingRuleType() == kOverrideRecord ||
            recinfo.GetRecordingRuleType() == kDontRecord)
        {
            menuPopup->AddButton(tr("Edit override rule"),
                                 qVariantFromValue(recinfo));
            menuPopup->AddButton(tr("Delete override rule"),
                                 qVariantFromValue(recinfo));
        }
        else
        {
            if (recinfo.GetRecordingRuleType() != kSingleRecord &&
                recinfo.GetRecordingStatus() != rsNotListed)
                menuPopup->AddButton(tr("Add override rule"),
                                     qVariantFromValue(recinfo));
            menuPopup->AddButton(tr("Edit recording rule"),
                                 qVariantFromValue(recinfo));
            menuPopup->AddButton(tr("Delete recording rule"),
                                 qVariantFromValue(recinfo));
        }
    }

    popupStack->AddScreen(menuPopup);
}

void ScheduleCommon::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "editrecording")
        {
            if (!dce->GetData().canConvert<RecordingInfo>())
                return;

            RecordingInfo recInfo = dce->GetData().value<RecordingInfo>();

            if (resulttext == tr("Record this showing"))
            {
                if (recInfo.GetRecordingRuleType() == kNotRecording)
                    recInfo.ApplyRecordStateChange(kSingleRecord);
                else
                {
                    recInfo.ApplyRecordStateChange(kOverrideRecord);
                    if (recInfo.GetRecordingStartTime() < MythDate::current())
                        recInfo.ReactivateRecording();
                }
            }
            else if (resulttext == tr("Record all showings"))
                recInfo.ApplyRecordStateChange(kAllRecord);
            else if (resulttext == tr("Record one showing (this episode)") ||
                     resulttext == tr("Record one showing"))
            {
                recInfo.ApplyRecordStateChange(kOneRecord, false);
                recInfo.GetRecordingRule()->m_filter |= 64; // This episode
                recInfo.GetRecordingRule()->Save();
            }
            else if (resulttext == tr("Record all showings (this channel)"))
            {
                recInfo.ApplyRecordStateChange(kAllRecord, false);
                recInfo.GetRecordingRule()->m_filter |= 1024; // This channel
                recInfo.GetRecordingRule()->Save();
            }
            else if (resulttext == tr("Stop this recording"))
            {
                RemoteStopRecording(&recInfo);
            }
            else if (resulttext == tr("Modify recording options") ||
                     resulttext == tr("Add override rule"))
            {
                if (recInfo.GetRecordingRuleType() == kSingleRecord ||
                    recInfo.GetRecordingRuleType() == kOverrideRecord ||
                    recInfo.GetRecordingRuleType() == kOneRecord)
                    EditScheduled(&recInfo);
                else
                    MakeOverride(&recInfo);
            }
            else if (resulttext == tr("Restart this recording"))
                recInfo.ReactivateRecording();
            else if (resulttext == tr("Forget previous recording"))
                recInfo.ForgetHistory();
            else if (resulttext == tr("Don't record this showing"))
                recInfo.ApplyRecordStateChange(kDontRecord);
            else if (resulttext == tr("Never record this episode"))
            {
                recInfo.ApplyNeverRecord();
            }
            else if (resulttext == tr("Edit recording rule") ||
                     resulttext == tr("Edit override rule"))
                EditScheduled(&recInfo);
            else if (resulttext == tr("Delete recording rule") ||
                     resulttext == tr("Delete override rule"))
                recInfo.ApplyRecordStateChange(kNotRecording);
        }
    }
}

/**
*  \brief Returns true if a search should be employed to find a matching
*         program.
*/
bool ScheduleCommon::IsFindApplicable(const RecordingInfo& recInfo) const
{
    return recInfo.GetRecordingRuleType() == kDailyRecord ||
           recInfo.GetRecordingRuleType() == kWeeklyRecord;
}

