// Qt
#include <QCoreApplication>

// MythTV stuff
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythtv/tvremoteutil.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreenstack.h"

// MythTV Setup
#include "startprompt.h"

struct StartPrompterPrivate
{
    StartPrompterPrivate()
    {
        m_stk = GetMythMainWindow()->GetStack("popup stack");
    }

    MythScreenStack *m_stk;
};

StartPrompter::StartPrompter()
  : m_d(new StartPrompterPrivate)
{
}

StartPrompter::~StartPrompter()
{
    delete m_d;
}

void StartPrompter::handleStart()
{
    // Offer to stop the backend if sensible
    if (MythCoreContext::BackendIsRunning() && gCoreContext->IsMasterHost())
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
        backendIsRecording = RemoteGetRecordingStatus(nullptr, false);
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

    auto *dia = new MythDialogBox(warning, m_d->m_stk, "actionmenu");

    if (!dia->Create())
    {
        LOG(VB_GENERAL, LOG_ERR, "Can't create Prompt dialog?");
        delete dia;
        quit();
        return;
    }

    // This is a hack so that the button clicks target the correct slot:
    dia->SetReturnEvent(this, QString());

    m_d->m_stk->AddScreen(dia);

    QString commandString = gCoreContext->GetSetting("BackendStopCommand");
    if (!commandString.isEmpty())
    {
        // Only show option to stop backend if command is defined.
        dia->AddButton(tr("Stop Backend and Continue"), &StartPrompter::stopBackend);
    }
    dia->AddButton(tr("Continue"), &StartPrompter::leaveBackendRunning);
    dia->AddButton(tr("Exit"), &StartPrompter::quit);
}

void StartPrompter::quit()
{
    qApp->exit(GENERIC_EXIT_OK);
}
