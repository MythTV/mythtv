// Qt
#include <QCoreApplication>

// MythTV stuff
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/remoteutil.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreenstack.h"

// MythTV Setup
#include "checksetup.h"
#include "exitprompt.h"

struct ExitPrompterPrivate
{
    ExitPrompterPrivate()
    {
        m_stk = GetMythMainWindow()->GetStack("popup stack");
    }

    MythScreenStack *m_stk;
};

ExitPrompter::ExitPrompter()
  : m_d(new ExitPrompterPrivate)
{
}

ExitPrompter::~ExitPrompter()
{
    delete m_d;
}

void ExitPrompter::masterPromptExit()
{
    if (gCoreContext->IsMasterHost() && needsMFDBReminder())
    {
        QString label = tr("If you've added or altered channels,"
                           " please run 'mythfilldatabase' on the"
                           " master backend to populate the"
                           " database with guide information.");

        auto *dia = new MythConfirmationDialog(m_d->m_stk, label, false);
        if (!dia->Create())
        {
            LOG(VB_GENERAL, LOG_ERR, "Can't create fill DB prompt?");
            delete dia;
            quit();
        }
        else
        {
            dia->SetReturnEvent(this, "mythfillprompt");
            m_d->m_stk->AddScreen(dia);
        }
    }
    else
    {
        quit();
    }
}

void ExitPrompter::handleExit()
{
    QStringList allproblems;

    // Look for common problems
    if (CheckSetup(allproblems))
    {
        // Only report the first 4 problems
        QStringList problems;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int limit = std::min(4, allproblems.size());
#else
        int limit = std::min(4LL, allproblems.size());
#endif
        for (int i = 0; i < limit; ++i)
        {
            problems.push_back(allproblems[i]);
        }
        if (problems.size() < allproblems.size())
        {
            problems.push_back(tr("...and more..."));
        }
        else
        {
            problems.push_back(QString());
        }

        problems.push_back(tr("Do you want to go back and fix this(these) "
                              "problem(s)?", nullptr, problems.size()));

        auto *dia = new MythDialogBox(tr("Configuration Problems"), problems.join("\n"),
                                      m_d->m_stk, "exit prompt", true);
        if (!dia->Create())
        {
            LOG(VB_GENERAL, LOG_ERR, "Can't create Exit Prompt dialog?");
            delete dia;
            quit();
            return;
        }

        dia->SetReturnEvent(this, "problemprompt");

        dia->AddButton(tr("Yes please"));
        dia->AddButton(tr("No, I know what I am doing"),
                       &ExitPrompter::masterPromptExit);

        m_d->m_stk->AddScreen(dia);
    }
    else
    {
        masterPromptExit();
    }
}

void ExitPrompter::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

        QString resultid= dce->GetId();
        int buttonnum = dce->GetResult();

        if (resultid == "mythfillprompt")
        {
            quit();
        }
        else if (resultid == "problemprompt")
        {
            switch (buttonnum)
            {
                case 0 :
                    break;
                case 1 :
                    masterPromptExit();
                    break;
            }
        }
    }
}

void ExitPrompter::quit()
{
    // If the backend was stopped restart it here
    if (gCoreContext->GetSetting("AutoRestartBackend") == "1")
    {
        QString commandString = gCoreContext->GetSetting("BackendStartCommand");
        if (!commandString.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "backendrestart"+commandString);
            myth_system(commandString);
        }
    }
    else
    {
        // No need to run this if the backend has just restarted
        if (MythCoreContext::BackendIsRunning())
        {
            gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
        }
    }

    qApp->exit(GENERIC_EXIT_OK);
}
