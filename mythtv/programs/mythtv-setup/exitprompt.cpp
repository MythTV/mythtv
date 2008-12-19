#include <QApplication>

// MythTV stuff
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "remoteutil.h"

#include "checksetup.h"
#include "exitprompt.h"

MythMainWindow  *win;
MythScreenStack *stk;

ExitPrompter::ExitPrompter()
{
    win = GetMythMainWindow();
    stk = win->GetStack("popup stack");
}

void ExitPrompter::fillPrompt()
{
    if (gContext->IsMasterHost())
    {
        QString prompt = tr("If this is the master backend server,"
                            " please run 'mythfilldatabase' to populate"
                            " the database with channel information.");

        MythDialogBox *dia = new MythDialogBox(prompt, stk, "fill prompt");
        if (!dia->Create())
        {
            VERBOSE(VB_IMPORTANT, "Can't create fill DB prompt?");
            delete dia;
            quit();
        }
        dia->AddButton(tr("OK"), SLOT(quit()));

        // This is a hack so that the button clicks target the correct slot:
        dia->SetReturnEvent(this, QString());

        stk->AddScreen(dia);
    }
}

void ExitPrompter::handleExit()
{
    QString *problems;
    QString  prompt;

    // Look for common problems
    if (CheckSetup(problems))
    {
        if (problems->count("\n") > 1)
            prompt = tr("Do you want to fix these problems?");
        else
            prompt = tr("Do you want to fix this problem?");

        MythDialogBox *dia = new MythDialogBox(problems->append("\n" + prompt),
                                     stk, "exit prompt");
        if (!dia->Create())
        {
            VERBOSE(VB_IMPORTANT, "Can't create Exit Prompt dialog?");
            delete dia;
            quit();
        }
        dia->AddButton(tr("Yes please"));
        dia->AddButton(tr("No, I know what I am doing"), SLOT(fillPrompt()));

        // This is a hack so that the button clicks target the correct slot:
        dia->SetReturnEvent(this, QString());

        stk->AddScreen(dia);
    }
    else
        fillPrompt();
}

void ExitPrompter::quit()
{
    if (gContext->BackendIsRunning())
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");

    delete gContext;

    qApp->exit(GENERIC_EXIT_OK);
}
