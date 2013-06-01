// Qt
#include <QCoreApplication>

// MythTV stuff
#include "exitcodes.h"
#include "mythcorecontext.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythsystemlegacy.h"
#include "tvremoteutil.h"

#include "startprompt.h"

struct StartPrompterPrivate
{
    StartPrompterPrivate()
    {
        stk = GetMythMainWindow()->GetStack("popup stack");
    }

    MythScreenStack *stk;
};

StartPrompter::StartPrompter()
{
    m_d = new StartPrompterPrivate;
}

StartPrompter::~StartPrompter()
{
    delete m_d;
}

void StartPrompter::handleStart()
{
    // Offer to stop the backend if sensible
    if (gCoreContext->BackendIsRunning() && gCoreContext->IsMasterHost())
    {
        backendRunningPrompt();
    }    
}

void StartPrompter::leaveBackendRunning()
{
    LOG(VB_GENERAL, LOG_INFO, "Continuing with backend running");
    gCoreContext->OverrideSettingForSession("AutoRestartBackend", "0");
}

void StartPrompter::stopBackend()
{
    LOG(VB_GENERAL, LOG_INFO, "Trying to stop backend");

    QString commandString = gCoreContext->GetSetting("BackendStopCommand");
    if (!commandString.isEmpty())
    {
        myth_system(commandString);
    }
    gCoreContext->OverrideSettingForSession("AutoRestartBackend", "1");
}

void StartPrompter::backendRunningPrompt(void)
{
    bool backendIsRecording = false;
    // Get recording status
    if (!gCoreContext->IsConnectedToMaster() &&
        gCoreContext->ConnectToMasterServer(false))
    {
        backendIsRecording = RemoteGetRecordingStatus(NULL, false);
    }
    
    QString warning = tr("WARNING: The backend is currently running.")+"\n\n"+
                      tr("Changing existing card inputs, deleting anything, "
                     "or scanning for channels may not work.")+"\n\n";
    if (backendIsRecording)
    {
        warning += tr("Recording Status: RECORDING.")+"\n"+
                   tr("If you stop the backend now these recordings will be stopped!");
    }
    else
    {
        warning += tr("Recording Status: None.");
    }

    MythDialogBox *dia = new MythDialogBox(warning, m_d->stk, "actionmenu");

    if (!dia->Create())
    {
        LOG(VB_GENERAL, LOG_ERR, "Can't create Prompt dialog?");
        delete dia;
        quit();
    }

    // This is a hack so that the button clicks target the correct slot:
    dia->SetReturnEvent(this, QString());

    m_d->stk->AddScreen(dia);

    QString commandString = gCoreContext->GetSetting("BackendStopCommand");
    if (!commandString.isEmpty())
    {
        // Only show option to stop backend if command is defined.
        dia->AddButton(tr("Stop Backend and Continue"), SLOT(stopBackend()));
    }
    dia->AddButton(tr("Continue"), SLOT(leaveBackendRunning()));
    dia->AddButton(tr("Exit"), SLOT(quit()));
}

void StartPrompter::quit()
{
    qApp->exit(GENERIC_EXIT_OK);
}
