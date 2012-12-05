
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

/**
*  \brief Show the Program Details screen
*/
void ScheduleCommon::ShowDetails(ProgramInfo *pginfo) const
{
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
void ScheduleCommon::ShowUpcoming(ProgramInfo *pginfo) const
{
    if (!pginfo)
        return;

    ShowUpcoming(pginfo->GetTitle(), pginfo->GetSeriesID());
}

/**
*  \brief Show the upcoming recordings for this recording rule
*/
void ScheduleCommon::ShowUpcomingScheduled(ProgramInfo *pginfo) const
{
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
*  \brief Creates a dialog for editing the recording status,
*         blocking until user leaves dialog.
*/
void ScheduleCommon::EditRecording(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    RecordingInfo ri(*pginfo);

    if (!ri.GetRecordingRuleID())
        EditScheduled(&ri);
    else if (ri.GetRecordingStatus() <= rsWillRecord ||
             ri.GetRecordingStatus() == rsOtherShowing)
        ShowRecordingDialog(ri);
    else
        ShowNotRecordingDialog(ri);
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
void ScheduleCommon::EditCustom(ProgramInfo *pginfo)
{
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
void ScheduleCommon::MakeOverride(RecordingInfo *recinfo, bool startActive)
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
    if (startActive)
        recrule->m_type = kOverrideRecord;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ScheduleEditor *schededit = new ScheduleEditor(mainStack, recrule);
    if (schededit->Create())
        mainStack->AddScreen(schededit);
    else
        delete schededit;
}

/**
*  \brief Creates a dialog displaying current recording status and options
*         available
*/
void ScheduleCommon::ShowRecordingDialog(const RecordingInfo& recinfo)
{
    QString message = recinfo.toString(ProgramInfo::kTitleSubtitle, " - ");

    message += "\n\n";
    message += toDescription(recinfo.GetRecordingStatus(),
                             recinfo.GetRecordingRuleType(),
                             recinfo.GetRecordingStartTime());

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(message, popupStack,
                                                 "recOptionPopup", true);

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "schedulerecording");

        QDateTime now = MythDate::current();

        if (recinfo.GetRecordingStartTime() < now &&
            recinfo.GetRecordingEndTime() > now)
        {
            if (recinfo.GetRecordingStatus() != rsRecording &&
                recinfo.GetRecordingStatus() != rsTuning &&
                recinfo.GetRecordingStatus() != rsOtherRecording && 
                recinfo.GetRecordingStatus() != rsOtherTuning)
                menuPopup->AddButton(tr("Reactivate"),
                                     qVariantFromValue(recinfo));
            else
                menuPopup->AddButton(tr("Stop recording"),
                                     qVariantFromValue(recinfo));
        }

        if (recinfo.GetRecordingEndTime() > now)
        {
            if (recinfo.GetRecordingRuleType() != kSingleRecord &&
                recinfo.GetRecordingRuleType() != kOverrideRecord)
            {
                if (recinfo.GetRecordingStartTime() > now)
                {
                    menuPopup->AddButton(tr("Don't record"),
                                         qVariantFromValue(recinfo));
                }

                const RecordingDupMethodType dupmethod =
                    recinfo.GetDuplicateCheckMethod();

                if (recinfo.GetRecordingStatus() != rsRecording &&
                    recinfo.GetRecordingStatus() != rsTuning &&
                    recinfo.GetRecordingStatus() != rsOtherRecording &&
                    recinfo.GetRecordingStatus() != rsOtherTuning &&
                    recinfo.GetRecordingRuleType() != kOneRecord &&
                    !((recinfo.GetFindID() == 0 ||
                       !IsFindApplicable(recinfo)) &&
                      recinfo.GetCategoryType() == "series" &&
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
                        menuPopup->AddButton(tr("Never record"),
                                             qVariantFromValue(recinfo));
                    }
            }

            if (recinfo.GetRecordingRuleType() != kOverrideRecord &&
                recinfo.GetRecordingRuleType() != kDontRecord)
            {
                if (recinfo.GetRecordingStatus() == rsRecording ||
                    recinfo.GetRecordingStatus() == rsTuning ||
                    recinfo.GetRecordingStatus() == rsOtherRecording ||
                    recinfo.GetRecordingStatus() == rsOtherTuning)
                {
                    menuPopup->AddButton(tr("Modify Recording Options"),
                                         qVariantFromValue(recinfo));
                }
                else
                {
                    menuPopup->AddButton(tr("Edit Options"),
                                         qVariantFromValue(recinfo));

                    if (recinfo.GetRecordingRuleType() != kSingleRecord &&
                        recinfo.GetRecordingRuleType() != kOneRecord)
                    {
                        menuPopup->AddButton(tr("Add Override"),
                                             qVariantFromValue(recinfo));
                    }
                }
            }

            if (recinfo.GetRecordingRuleType() == kOverrideRecord ||
                recinfo.GetRecordingRuleType() == kDontRecord)
            {
                if (recinfo.GetRecordingStatus() == rsRecording ||
                    recinfo.GetRecordingStatus() == rsTuning ||
                    recinfo.GetRecordingStatus() == rsOtherRecording ||
                    recinfo.GetRecordingStatus() == rsOtherTuning)
                {
                    menuPopup->AddButton(tr("Modify Recording Options"),
                                         qVariantFromValue(recinfo));
                }
                else
                {
                    menuPopup->AddButton(tr("Edit Override"),
                                         qVariantFromValue(recinfo));
                    menuPopup->AddButton(tr("Clear Override"),
                                         qVariantFromValue(recinfo));
                }
            }
        }

        popupStack->AddScreen(menuPopup);
    }
    else
        delete menuPopup;
}

/**
*  \brief Creates a dialog displaying current recording status and options
*         available
*/
void ScheduleCommon::ShowNotRecordingDialog(const RecordingInfo& recinfo)
{
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
                                                 "notRecOptionPopup", true);

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "schedulenotrecording");

        QDateTime now = MythDate::current();

        if ((recinfo.GetRecordingStartTime() < now) &&
            (recinfo.GetRecordingEndTime() > now) &&
            (recinfo.GetRecordingStatus() != rsDontRecord) &&
            (recinfo.GetRecordingStatus() != rsNotListed))
        {
            menuPopup->AddButton(tr("Reactivate"),
                                 qVariantFromValue(recinfo));
        }

        if (recinfo.GetRecordingEndTime() > now)
        {
            if ((recinfo.GetRecordingRuleType() != kSingleRecord &&
                recinfo.GetRecordingRuleType() != kOverrideRecord) &&
                (recinfo.GetRecordingStatus() == rsDontRecord ||
                recinfo.GetRecordingStatus() == rsPreviousRecording ||
                recinfo.GetRecordingStatus() == rsCurrentRecording ||
                recinfo.GetRecordingStatus() == rsEarlierShowing ||
                recinfo.GetRecordingStatus() == rsNeverRecord ||
                recinfo.GetRecordingStatus() == rsRepeat ||
                recinfo.GetRecordingStatus() == rsInactive ||
                recinfo.GetRecordingStatus() == rsLaterShowing))
            {
                menuPopup->AddButton(tr("Record anyway"),
                                    qVariantFromValue(recinfo));
                if (recinfo.GetRecordingStatus() == rsPreviousRecording ||
                    recinfo.GetRecordingStatus() == rsNeverRecord)
                {
                    menuPopup->AddButton(tr("Forget Previous"),
                                        qVariantFromValue(recinfo));
                }
            }

            if (recinfo.GetRecordingRuleType() != kOverrideRecord &&
                recinfo.GetRecordingRuleType() != kDontRecord)
            {
                if (recinfo.GetRecordingRuleType() != kSingleRecord &&
                    recinfo.GetRecordingStatus() != rsPreviousRecording &&
                    recinfo.GetRecordingStatus() != rsCurrentRecording &&
                    recinfo.GetRecordingStatus() != rsNeverRecord &&
                    recinfo.GetRecordingStatus() != rsNotListed)
                {
                    if (recinfo.GetRecordingStartTime() > now)
                    {
                        menuPopup->AddButton(tr("Don't record"),
                                            qVariantFromValue(recinfo));
                    }

                    const RecordingDupMethodType dupmethod =
                        recinfo.GetDuplicateCheckMethod();

                    if (recinfo.GetRecordingRuleType() != kOneRecord &&
                        !((recinfo.GetFindID() == 0 ||
                           !IsFindApplicable(recinfo)) &&
                          recinfo.GetCategoryType() == "series" &&
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
                            menuPopup->AddButton(tr("Never record"),
                                                 qVariantFromValue(recinfo));
                        }
                }

                menuPopup->AddButton(tr("Edit Options"),
                                     qVariantFromValue(recinfo));

                if (recinfo.GetRecordingRuleType() != kSingleRecord &&
                    recinfo.GetRecordingRuleType() != kOneRecord &&
                    recinfo.GetRecordingStatus() != rsNotListed)
                {
                    menuPopup->AddButton(tr("Add Override"),
                                        qVariantFromValue(recinfo));
                }
            }

            if (recinfo.GetRecordingRuleType() == kOverrideRecord ||
                recinfo.GetRecordingRuleType() == kDontRecord)
            {
                menuPopup->AddButton(tr("Edit Override"),
                                    qVariantFromValue(recinfo));
                menuPopup->AddButton(tr("Clear Override"),
                                    qVariantFromValue(recinfo));
            }
        }

        popupStack->AddScreen(menuPopup);
    }
    else
        delete menuPopup;
}

void ScheduleCommon::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "schedulenotrecording")
        {
            if (!qVariantCanConvert<RecordingInfo>(dce->GetData()))
                return;

            RecordingInfo recInfo = qVariantValue<RecordingInfo>
                (dce->GetData());

            if (resulttext == tr("Reactivate"))
                recInfo.ReactivateRecording();
            else if (resulttext == tr("Record anyway"))
            {
                recInfo.ApplyRecordStateChange(kOverrideRecord);
                if (recInfo.GetRecordingStartTime() < MythDate::current())
                    recInfo.ReactivateRecording();
            }
            else if (resulttext == tr("Forget Previous"))
                recInfo.ForgetHistory();
            else if (resulttext == tr("Don't record"))
                recInfo.ApplyRecordStateChange(kDontRecord);
            else if (resulttext == tr("Never record"))
            {
                recInfo.SetRecordingStatus(rsNeverRecord);
                recInfo.SetScheduledStartTime(MythDate::current());
                recInfo.SetScheduledEndTime(recInfo.GetRecordingStartTime());
                recInfo.AddHistory(true, true);
            }
            else if (resulttext == tr("Clear Override"))
                recInfo.ApplyRecordStateChange(kNotRecording);
            else if (resulttext == tr("Edit Override") ||
                     resulttext == tr("Edit Options"))
            {
                EditScheduled(&recInfo);
            }
            else if (resulttext == tr("Add Override"))
            {
                MakeOverride(&recInfo);
            }
        }
        else if (resultid == "schedulerecording")
        {
            if (!qVariantCanConvert<RecordingInfo>(dce->GetData()))
                return;

            RecordingInfo recInfo = qVariantValue<RecordingInfo>
                                                            (dce->GetData());

            if (resulttext == tr("Reactivate"))
                recInfo.ReactivateRecording();
            else if (resulttext == tr("Stop recording"))
            {
                ProgramInfo pginfo(
                    recInfo.GetChanID(), recInfo.GetRecordingStartTime());
                if (pginfo.GetChanID())
                    RemoteStopRecording(&pginfo);
            }
            else if (resulttext == tr("Don't record"))
                recInfo.ApplyRecordStateChange(kDontRecord);
            else if (resulttext == tr("Never record"))
            {
                recInfo.SetRecordingStatus(rsNeverRecord);
                recInfo.SetScheduledStartTime(MythDate::current());
                recInfo.SetScheduledEndTime(recInfo.GetRecordingStartTime());
                recInfo.AddHistory(true, true);
            }
            else if (resulttext == tr("Clear Override"))
                recInfo.ApplyRecordStateChange(kNotRecording);
            else if (resulttext == tr("Modify Recording Options"))
            {
                if (recInfo.GetRecordingRuleType() == kSingleRecord ||
                    recInfo.GetRecordingRuleType() == kOverrideRecord ||
                    recInfo.GetRecordingRuleType() == kOneRecord)
                    EditScheduled(&recInfo);
                else
                    MakeOverride(&recInfo, true);
            }
            else if (resulttext == tr("Edit Override") ||
                     resulttext == tr("Edit Options"))
            {
                EditScheduled(&recInfo);
            }
            else if (resulttext == tr("Add Override"))
            {
                MakeOverride(&recInfo);
            }
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
