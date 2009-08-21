#include <QApplication>

// MythTV stuff
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "remoteutil.h"
#include "mythsystem.h"

#include "checksetup.h"
#include "exitprompt.h"

struct ExitPrompterPrivate
{
    ExitPrompterPrivate()
    {
        stk = GetMythMainWindow()->GetStack("popup stack");
    }

    MythScreenStack *stk;
};

ExitPrompter::ExitPrompter()
{
    m_d = new ExitPrompterPrivate;
}

ExitPrompter::~ExitPrompter()
{
    delete m_d;
}

void ExitPrompter::masterPromptExit()
{
    if (gContext->IsMasterHost())
    {
        QString prompt = tr("If this is the master backend server,"
                            " please run 'mythfilldatabase' to populate"
                            " the database with channel information.");

        MythDialogBox *dia = new MythDialogBox(prompt, m_d->stk, "fill prompt");
        if (!dia->Create())
        {
            VERBOSE(VB_IMPORTANT, "Can't create fill DB prompt?");
            delete dia;
            quit();
        }
        dia->AddButton(tr("OK"), SLOT(quit()));

        // This is a hack so that the button clicks target the correct slot:
        dia->SetReturnEvent(this, QString());

        m_d->stk->AddScreen(dia);
    }
    else
        quit();
}

void ExitPrompter::handleExit()
{
    QStringList problems;

    // Look for common problems
    if (CheckSetup(problems))
    {
        problems.push_back(QString());
        if (problems.size() > 2)
            problems.push_back(tr("Do you want to go back and fix these problems?"));
        else
            problems.push_back(tr("Do you want to go back and fix this problem?"));

        MythDialogBox *dia = new MythDialogBox(problems.join("\n"),
                m_d->stk, "exit prompt");
        if (!dia->Create())
        {
            VERBOSE(VB_IMPORTANT, "Can't create Exit Prompt dialog?");
            delete dia;
            quit();
        }
        dia->AddButton(tr("Yes please"));
        dia->AddButton(tr("No, I know what I am doing"),
                SLOT(masterPromptExit()));

        // This is a hack so that the button clicks target the correct slot:
        dia->SetReturnEvent(this, QString());

        m_d->stk->AddScreen(dia);
    }
    else
        masterPromptExit();
}

void ExitPrompter::quit()
{
    // If the backend was stopped restart it here
    if (gContext->GetSetting("AutoRestartBackend") == "1")
    {
        QString commandString = gContext->GetSetting("BackendStartCommand");
        if (!commandString.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "backendrestart"+commandString);
            myth_system(commandString);
        }
    }
    else
    {
        // No need to run this if the backend has just restarted
        if (gContext->BackendIsRunning())
        {
            RemoteSendMessage("CLEAR_SETTINGS_CACHE");
        }
    }

    qApp->exit(GENERIC_EXIT_OK);
}
