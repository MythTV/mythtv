// Qt
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/tvremoteutil.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreentype.h"

// MythFrontend
#include "customedit.h"
#include "guidegrid.h"
#include "prevreclist.h"
#include "progdetails.h"
#include "proglist.h"
#include "schedulecommon.h"
#include "scheduleeditor.h"

/**
*  \brief Show the Program Details screen
*/
void ScheduleCommon::ShowDetails(void) const
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *details_dialog  = new ProgDetails(mainStack, pginfo);

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
                                  const QString &seriesid)
{
    if (title.isEmpty())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plTitle, title, seriesid);
    if (pl->Create())
    {
        mainStack->AddScreen(pl);
    }
    else
    {
        delete pl;
    }
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
    {
        ShowUpcomingScheduled();
        return;
    }

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

    uint id = ri.GetRecordingRuleID();
    if (id == 0)
    {
        ShowUpcoming(pginfo->GetTitle(), pginfo->GetSeriesID());
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plRecordid, QString::number(id), "");

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
    auto *pl = new ProgLister(mainStack, plChannel,
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
    auto *schededit = new ScheduleEditor(mainStack, recinfo);
    if (schededit->Create())
        mainStack->AddScreen(schededit);
    else
    {
        delete schededit;
        ShowOkPopup(tr("Recording rule does not exist."));
    }
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
    auto *ce = new CustomEdit(mainStack, pginfo);
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

    auto *recrule = new RecordingRule();

    if (!recrule->LoadByProgram(recinfo))
        LOG(VB_GENERAL, LOG_ERR, "Failed to load by program info");

    if (!recrule->MakeOverride())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to make Override");
        delete recrule;
        return;
    }
    recrule->m_type = kOverrideRecord;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *schededit = new ScheduleEditor(mainStack, recrule);
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

    ShowPrevious(pginfo->GetRecordingRuleID(), pginfo->GetTitle());
}

/**
*  \brief Show the previous recordings for this recording rule
*/
void ScheduleCommon::ShowPrevious(uint ruleid, const QString &title) const
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new PrevRecordedList(mainStack, ruleid, title);
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

/**
*  \brief Creates a dialog for editing the recording status,
*         blocking until user leaves dialog.
*/
void ScheduleCommon::EditRecording(bool may_watch_now)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return;

    RecordingInfo recinfo(*pginfo);

    QString timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = QString("%1 - %2  %3\n")
        .arg(recinfo.GetRecordingStartTime().toLocalTime().toString(timeFormat),
             recinfo.GetRecordingEndTime().toLocalTime().toString(timeFormat),
             recinfo.toString(ProgramInfo::kTitleSubtitle, " - "));

    message += "\n";
    message += RecStatus::toDescription(recinfo.GetRecordingStatus(),
                                        recinfo.GetRecordingRuleType(),
                                        recinfo.GetRecordingStartTime());

    QString messageConflict;
    if (recinfo.GetRecordingStatus() == RecStatus::Conflict ||
        recinfo.GetRecordingStatus() == RecStatus::LaterShowing)
    {
        std::vector<ProgramInfo *> *confList = RemoteGetConflictList(&recinfo);
        uint chanid = recinfo.GetChanID();
        uint sourceid = ChannelUtil::GetSourceIDForChannel(chanid);

        for (auto *p : *confList)
        {
            if (sourceid == p->GetSourceID())
            {
                messageConflict += QString("%1 - %2  %3\n")
                    .arg(p->GetRecordingStartTime().toLocalTime().toString(timeFormat),
                         p->GetRecordingEndTime().toLocalTime().toString(timeFormat),
                         p->toString(ProgramInfo::kTitleSubtitle, " - "));
            }
            delete p;
        }
        delete confList;
    }

    if (!messageConflict.isEmpty())
    {
        message += " ";
        message += tr("The following programs will be recorded instead:");
        message += "\n";
        message += messageConflict;
        message += "\n";
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(message, popupStack, "recOptionPopup", true);

    if (!menuPopup->Create())
    {
        delete menuPopup;
        return;
    }
    menuPopup->SetReturnEvent(this, "editrecording");

    QDateTime now = MythDate::current();

    if(may_watch_now)
        menuPopup->AddButton(tr("Watch This Channel"));

    if (recinfo.GetRecordingStatus() == RecStatus::Unknown)
    {
        if (recinfo.GetRecordingEndTime() > now)
            menuPopup->AddButtonV(tr("Record this showing"),
                                  QVariant::fromValue(recinfo));
        menuPopup->AddButtonV(tr("Record all showings"),
                              QVariant::fromValue(recinfo));
        if (!recinfo.IsGeneric())
        {
            if (recinfo.GetCategoryType() == ProgramInfo::kCategoryMovie)
            {
                menuPopup->AddButtonV(tr("Record one showing"),
                                      QVariant::fromValue(recinfo));
            }
            else
            {
                menuPopup->AddButtonV(tr("Record one showing (this episode)"),
                                      QVariant::fromValue(recinfo));
            }

        }
        menuPopup->AddButtonV(tr("Record all showings (this channel)"),
                              QVariant::fromValue(recinfo));
        menuPopup->AddButtonV(tr("Edit recording rule"),
                              QVariant::fromValue(recinfo));
    }
    else if (recinfo.GetRecordingStatus() == RecStatus::Recording ||
             recinfo.GetRecordingStatus() == RecStatus::Tuning    ||
             recinfo.GetRecordingStatus() == RecStatus::Failing   ||
             recinfo.GetRecordingStatus() == RecStatus::Pending)
    {
        if (recinfo.GetRecordingStatus() != RecStatus::Pending)
            menuPopup->AddButtonV(tr("Stop this recording"),
                                  QVariant::fromValue(recinfo));
        menuPopup->AddButtonV(tr("Modify recording options"),
                              QVariant::fromValue(recinfo));
    }
    else
    {
        if (recinfo.GetRecordingStartTime() < now &&
            recinfo.GetRecordingEndTime() > now &&
            recinfo.GetRecordingStatus() != RecStatus::DontRecord &&
            recinfo.GetRecordingStatus() != RecStatus::NotListed)
        {
            menuPopup->AddButtonV(tr("Restart this recording"),
                                  QVariant::fromValue(recinfo));
        }

        if (recinfo.GetRecordingEndTime() > now &&
            recinfo.GetRecordingRuleType() != kSingleRecord &&
            recinfo.GetRecordingRuleType() != kOverrideRecord &&
            (recinfo.GetRecordingStatus() == RecStatus::DontRecord ||
             recinfo.GetRecordingStatus() == RecStatus::PreviousRecording ||
             recinfo.GetRecordingStatus() == RecStatus::CurrentRecording ||
             recinfo.GetRecordingStatus() == RecStatus::EarlierShowing ||
             recinfo.GetRecordingStatus() == RecStatus::TooManyRecordings ||
             recinfo.GetRecordingStatus() == RecStatus::LaterShowing ||
             recinfo.GetRecordingStatus() == RecStatus::Repeat ||
             recinfo.GetRecordingStatus() == RecStatus::Inactive ||
             recinfo.GetRecordingStatus() == RecStatus::NeverRecord))
        {
            menuPopup->AddButtonV(tr("Record this showing"),
                                  QVariant::fromValue(recinfo));
            if (recinfo.GetRecordingStartTime() > now &&
                (recinfo.GetRecordingStatus() == RecStatus::PreviousRecording ||
                 recinfo.GetRecordingStatus() == RecStatus::NeverRecord))
            {
                menuPopup->AddButtonV(tr("Forget previous recording"),
                                      QVariant::fromValue(recinfo));
            }
        }

        if (recinfo.GetRecordingRuleType() != kSingleRecord &&
            recinfo.GetRecordingRuleType() != kDontRecord &&
            (recinfo.GetRecordingStatus() == RecStatus::WillRecord ||
             recinfo.GetRecordingStatus() == RecStatus::CurrentRecording ||
             recinfo.GetRecordingStatus() == RecStatus::EarlierShowing ||
             recinfo.GetRecordingStatus() == RecStatus::TooManyRecordings ||
             recinfo.GetRecordingStatus() == RecStatus::Conflict ||
             recinfo.GetRecordingStatus() == RecStatus::LaterShowing ||
             recinfo.GetRecordingStatus() == RecStatus::Offline))
        {
            if (recinfo.GetRecordingStatus() == RecStatus::WillRecord ||
                recinfo.GetRecordingStatus() == RecStatus::Conflict)
                menuPopup->AddButtonV(tr("Don't record this showing"),
                                      QVariant::fromValue(recinfo));

            const RecordingDupMethodType dupmethod =
                recinfo.GetDuplicateCheckMethod();
            static const QRegularExpression kGenericEpisodeRE { "0000$" };
            if (recinfo.GetRecordingRuleType() != kOverrideRecord &&
                ((recinfo.GetFindID() != 0 &&
                   IsFindApplicable(recinfo)) ||
                  recinfo.GetCategoryType() != ProgramInfo::kCategorySeries ||
                  !recinfo.GetProgramID().contains(kGenericEpisodeRE)) &&
                ((((dupmethod & kDupCheckNone) == 0) &&
                  !recinfo.GetProgramID().isEmpty() &&
                  (recinfo.GetFindID() != 0 ||
                   !IsFindApplicable(recinfo))) ||
                 (((dupmethod & kDupCheckSub) != 0) &&
                  !recinfo.GetSubtitle().isEmpty()) ||
                 (((dupmethod & kDupCheckDesc) != 0) &&
                  !recinfo.GetDescription().isEmpty()) ||
                 (((dupmethod & kDupCheckSubThenDesc) != 0) &&
                  (!recinfo.GetSubtitle().isEmpty() ||
                   !recinfo.GetDescription().isEmpty())) ))
            {
                menuPopup->AddButtonV(tr("Never record this episode"),
                                      QVariant::fromValue(recinfo));
            }
        }

        if (recinfo.GetRecordingRuleType() == kOverrideRecord ||
            recinfo.GetRecordingRuleType() == kDontRecord)
        {
            menuPopup->AddButtonV(tr("Edit override rule"),
                                  QVariant::fromValue(recinfo));
            menuPopup->AddButtonV(tr("Delete override rule"),
                                  QVariant::fromValue(recinfo));
        }
        else
        {
            if (recinfo.GetRecordingRuleType() != kSingleRecord &&
                recinfo.GetRecordingStatus() != RecStatus::NotListed)
                menuPopup->AddButtonV(tr("Add override rule"),
                                      QVariant::fromValue(recinfo));
            menuPopup->AddButtonV(tr("Edit recording rule"),
                                  QVariant::fromValue(recinfo));
            menuPopup->AddButtonV(tr("Delete recording rule"),
                                  QVariant::fromValue(recinfo));
        }
    }

    popupStack->AddScreen(menuPopup);
}

void ScheduleCommon::EditRecording(MythUIButtonListItem* /*item*/)
{
    EditRecording();
}

void ScheduleCommon::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "editrecording")
        {
            if (!dce->GetData().canConvert<RecordingInfo>())
                return;

            auto recInfo = dce->GetData().value<RecordingInfo>();

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
            {
                recInfo.ApplyRecordStateChange(kAllRecord);
            }
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
            {
                recInfo.ReactivateRecording();
            }
            else if (resulttext == tr("Forget previous recording"))
            {
                recInfo.ForgetHistory();
            }
            else if (resulttext == tr("Don't record this showing"))
            {
                recInfo.ApplyRecordStateChange(kDontRecord);
            }
            else if (resulttext == tr("Never record this episode"))
            {
                recInfo.ApplyNeverRecord();
            }
            else if (resulttext == tr("Edit recording rule") ||
                     resulttext == tr("Edit override rule"))
            {
                EditScheduled(&recInfo);
            }
            else if (resulttext == tr("Delete recording rule") ||
                     resulttext == tr("Delete override rule"))
            {
                recInfo.ApplyRecordStateChange(kNotRecording);
            }
        }
    }
}

/**
*  \brief Returns true if a search should be employed to find a matching
*         program.
*/
bool ScheduleCommon::IsFindApplicable(const RecordingInfo& recInfo)
{
    return recInfo.GetRecordingRuleType() == kDailyRecord ||
           recInfo.GetRecordingRuleType() == kWeeklyRecord;
}

