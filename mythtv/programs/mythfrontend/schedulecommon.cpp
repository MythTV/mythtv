
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
void ScheduleCommon::ShowUpcoming(const QString &title) const
{
    if (title.isEmpty())
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitle, title, "");
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

    ShowUpcoming(pginfo->title);
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

    if (ri.recordid == 0)
        EditScheduled(&ri);
    else if (ri.recstatus <= rsWillRecord)
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
    if (!recinfo || recinfo->recordid <= 0)
        return;

    RecordingRule *recrule = new RecordingRule();
    
    if (!recrule->LoadByProgram(static_cast<ProgramInfo*>(recinfo)))
        VERBOSE(VB_IMPORTANT, QString("Failed to load by program info"));
    
    if (!recrule->MakeOverride())
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to make Override"));
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
    QString message = recinfo.title;
    
    if (!recinfo.subtitle.isEmpty())
        message += QString(" - \"%1\"").arg(recinfo.subtitle);
    
    message += "\n\n";
    message += recinfo.RecStatusDesc();
    
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(message, popupStack,
                                                 "recOptionPopup", true);
    
    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "schedulerecording");

        QDateTime now = QDateTime::currentDateTime();
        
        if (recinfo.recstartts < now && recinfo.recendts > now)
        {
            if (recinfo.recstatus != rsRecording)
                menuPopup->AddButton(tr("Reactivate"),
                                     qVariantFromValue(recinfo));
            else
                menuPopup->AddButton(tr("Stop recording"),
                                     qVariantFromValue(recinfo));
        }

        if (recinfo.recendts > now)
        {
            if (recinfo.rectype != kSingleRecord &&
                recinfo.rectype != kOverrideRecord)
            {
                if (recinfo.recstartts > now)
                {
                    menuPopup->AddButton(tr("Don't record"),
                                         qVariantFromValue(recinfo));
                }
                if (recinfo.recstatus != rsRecording &&
                    recinfo.rectype != kFindOneRecord &&
                    !((recinfo.findid == 0 || !IsFindApplicable(recinfo)) &&
                      recinfo.catType == "series" &&
                      recinfo.programid.contains(QRegExp("0000$"))) &&
                    ((!(recinfo.dupmethod & kDupCheckNone) &&
                      !recinfo.programid.isEmpty() &&
                      (recinfo.findid != 0 || !IsFindApplicable(recinfo))) ||
                     ((recinfo.dupmethod & kDupCheckSub) &&
                      !recinfo.subtitle.isEmpty()) ||
                     ((recinfo.dupmethod & kDupCheckDesc) &&
                      !recinfo.description.isEmpty()) ||
                     ((recinfo.dupmethod & kDupCheckSubThenDesc) &&
                      (!recinfo.subtitle.isEmpty() ||
                       !recinfo.description.isEmpty())) ))
                    {
                        menuPopup->AddButton(tr("Never record"),
                                             qVariantFromValue(recinfo));
                    }
            }
            
            if (recinfo.rectype != kOverrideRecord &&
                recinfo.rectype != kDontRecord)
            {
                if (recinfo.recstatus == rsRecording)
                {
                    menuPopup->AddButton(tr("Modify Recording Options"),
                                         qVariantFromValue(recinfo));
                }
                else
                {
                    menuPopup->AddButton(tr("Edit Options"),
                                         qVariantFromValue(recinfo));
                    
                    if (recinfo.rectype != kSingleRecord &&
                        recinfo.rectype != kFindOneRecord)
                    {
                        menuPopup->AddButton(tr("Add Override"),
                                             qVariantFromValue(recinfo));
                    }
                }
            }
            
            if (recinfo.rectype == kOverrideRecord ||
                recinfo.rectype == kDontRecord)
            {
                if (recinfo.recstatus == rsRecording)
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

    QString message = recinfo.title;

    if (!recinfo.subtitle.isEmpty())
        message += QString(" - \"%1\"").arg(recinfo.subtitle);

    message += "\n\n";
    message += recinfo.RecStatusDesc();

    if (recinfo.recstatus == rsConflict || recinfo.recstatus == rsLaterShowing)
    {
        vector<ProgramInfo *> *confList = RemoteGetConflictList(&recinfo);

        if (confList->size())
            message += tr(" The following programs will be recorded "
            "instead:\n");

        for (int maxi = 0; confList->begin() != confList->end() &&
            maxi < 4; maxi++)
        {
            ProgramInfo *p = *confList->begin();
            message += QString("%1 - %2  %3")
            .arg(p->recstartts.toString(timeFormat))
            .arg(p->recendts.toString(timeFormat)).arg(p->title);
            if (!p->subtitle.isEmpty())
                message += QString(" - \"%1\"").arg(p->subtitle);
            message += "\n";
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

        QDateTime now = QDateTime::currentDateTime();

        if ((recinfo.recstartts < now) && (recinfo.recendts > now) &&
            (recinfo.recstatus != rsDontRecord) &&
            (recinfo.recstatus != rsNotListed))
        {
            menuPopup->AddButton(tr("Reactivate"),
                                 qVariantFromValue(recinfo));
        }

        if (recinfo.recendts > now)
        {
            if ((recinfo.rectype != kSingleRecord &&
                recinfo.rectype != kOverrideRecord) &&
                (recinfo.recstatus == rsDontRecord ||
                recinfo.recstatus == rsPreviousRecording ||
                recinfo.recstatus == rsCurrentRecording ||
                recinfo.recstatus == rsEarlierShowing ||
                recinfo.recstatus == rsOtherShowing ||
                recinfo.recstatus == rsNeverRecord ||
                recinfo.recstatus == rsRepeat ||
                recinfo.recstatus == rsInactive ||
                recinfo.recstatus == rsLaterShowing))
            {
                menuPopup->AddButton(tr("Record anyway"),
                                    qVariantFromValue(recinfo));
                if (recinfo.recstatus == rsPreviousRecording ||
                    recinfo.recstatus == rsNeverRecord)
                {
                    menuPopup->AddButton(tr("Forget Previous"),
                                        qVariantFromValue(recinfo));
                }
            }

            if (recinfo.rectype != kOverrideRecord &&
                recinfo.rectype != kDontRecord)
            {
                if (recinfo.rectype != kSingleRecord &&
                    recinfo.recstatus != rsPreviousRecording &&
                    recinfo.recstatus != rsCurrentRecording &&
                    recinfo.recstatus != rsNeverRecord &&
                    recinfo.recstatus != rsNotListed)
                {
                    if (recinfo.recstartts > now)
                    {
                        menuPopup->AddButton(tr("Don't record"),
                                            qVariantFromValue(recinfo));
                    }
                    if (recinfo.rectype != kFindOneRecord &&
                        !((recinfo.findid == 0 || !IsFindApplicable(recinfo)) &&
                          recinfo.catType == "series" &&
                          recinfo.programid.contains(QRegExp("0000$"))) &&
                        ((!(recinfo.dupmethod & kDupCheckNone) &&
                          !recinfo.programid.isEmpty() &&
                          (recinfo.findid != 0 || !IsFindApplicable(recinfo))) ||
                         ((recinfo.dupmethod & kDupCheckSub) &&
                          !recinfo.subtitle.isEmpty()) ||
                         ((recinfo.dupmethod & kDupCheckDesc) &&
                          !recinfo.description.isEmpty()) ||
                         ((recinfo.dupmethod & kDupCheckSubThenDesc) &&
                          (!recinfo.subtitle.isEmpty() ||
                           !recinfo.description.isEmpty())) ))
                        {
                            menuPopup->AddButton(tr("Never record"),
                                                qVariantFromValue(recinfo));
                        }
                }

                menuPopup->AddButton(tr("Edit Options"),
                                     qVariantFromValue(recinfo));

                if (recinfo.rectype != kSingleRecord &&
                    recinfo.rectype != kFindOneRecord &&
                    recinfo.recstatus != rsNotListed)
                {
                    menuPopup->AddButton(tr("Add Override"),
                                        qVariantFromValue(recinfo));
                }
            }

            if (recinfo.rectype == kOverrideRecord ||
                recinfo.rectype == kDontRecord)
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
                if (recInfo.recstartts < QDateTime::currentDateTime())
                    recInfo.ReactivateRecording();
            }
            else if (resulttext == tr("Forget Previous"))
                recInfo.ForgetHistory();
            else if (resulttext == tr("Don't record"))
                recInfo.ApplyRecordStateChange(kDontRecord);
            else if (resulttext == tr("Never record"))
            {
                recInfo.recstatus = rsNeverRecord;
                recInfo.startts = QDateTime::currentDateTime();
                recInfo.endts = recInfo.recstartts;
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
                ProgramInfo *p = recInfo.GetProgramFromRecorded(
                                                        recInfo.chanid,
                                                        recInfo.recstartts);
                if (p)
                {
                    RemoteStopRecording(p);
                    delete p;
                }
            }
            else if (resulttext == tr("Don't record"))
                recInfo.ApplyRecordStateChange(kDontRecord);
            else if (resulttext == tr("Never record"))
            {
                recInfo.recstatus = rsNeverRecord;
                recInfo.startts = QDateTime::currentDateTime();
                recInfo.endts = recInfo.recstartts;
                recInfo.AddHistory(true, true);
            }
            else if (resulttext == tr("Clear Override"))
                recInfo.ApplyRecordStateChange(kNotRecording);
            else if (resulttext == tr("Modify Recording Options"))
            {
                if (recInfo.rectype == kSingleRecord ||
                    recInfo.rectype == kOverrideRecord ||
                    recInfo.rectype == kFindOneRecord)
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
    return recInfo.rectype == kFindDailyRecord ||
           recInfo.rectype == kFindWeeklyRecord;
}
