// Qt
#include <QCoreApplication>

// MythTV
#include "config.h"
#include "exitprompt.h"
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythsystemlegacy.h"
#include "mythlogging.h"
#include "exitcodes.h"

ExitPrompter::ExitPrompter()
  : m_power(MythPower::AcquireRelease(this, true)),
    m_confirm(gCoreContext->GetBoolSetting("ConfirmPowerEvent", true)),
    m_haltCommand(gCoreContext->GetSetting("HaltCommand", "")),
    m_rebootCommand(gCoreContext->GetSetting("RebootCommand", "")),
    m_suspendCommand(gCoreContext->GetSetting("SuspendCommand", ""))
{
    // Log to confirm we are not leaking prompters...
    LOG(VB_GENERAL, LOG_INFO, "Created ExitPrompter");
}

ExitPrompter::~ExitPrompter()
{
    if (m_power)
        MythPower::AcquireRelease(this, false);
    LOG(VB_GENERAL, LOG_INFO, "Deleted ExitPrompter");
}

void ExitPrompter::DoQuit()
{
    qApp->exit();
    deleteLater();
}

void ExitPrompter::ConfirmHalt()
{
    Confirm(MythPower::FeatureShutdown);
}

void ExitPrompter::DoHalt(bool Confirmed)
{
    if (Confirmed)
    {
        bool handled = false;
        // Use user specified command if it exists
        if (!m_haltCommand.isEmpty())
        {
            uint ret = myth_system(m_haltCommand);
            if (ret == GENERIC_EXIT_OK)
            {
                handled = true;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "User defined HaltCommand failed, falling back to "
                    "alternative methods.");
            }
        }

        // Otherwise use MythPower
        if (!handled && m_power && m_power->IsFeatureSupported(MythPower::FeatureShutdown))
            if (m_power->RequestFeature(MythPower::FeatureShutdown))
                handled = true;

        // halt of last resort
        if (!handled)
            myth_system("sudo /sbin/halt -p");
    }
    deleteLater();
}

void ExitPrompter::ConfirmReboot()
{
    Confirm(MythPower::FeatureRestart);
}

void ExitPrompter::DoReboot(bool Confirmed)
{
    if (Confirmed)
    {
        bool handled = false;
        if (!m_rebootCommand.isEmpty())
        {
            uint ret = myth_system(m_rebootCommand);
            if (ret == GENERIC_EXIT_OK)
            {
                handled = true;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "User defined RebootCommand failed, falling back to "
                    "alternative methods.");
            }
        }

        // Otherwise use MythPower
        if (!handled && m_power && m_power->IsFeatureSupported(MythPower::FeatureRestart))
            if (m_power->RequestFeature(MythPower::FeatureRestart))
                handled = true;

        // reboot of last resort
        if (!handled)
            myth_system("sudo /sbin/reboot");
    }
    deleteLater();
}

void ExitPrompter::ConfirmSuspend()
{
    Confirm(MythPower::FeatureSuspend);
}

void ExitPrompter::DoSuspend(bool Confirmed)
{
    if (Confirmed)
    {
        bool handled = false;
        // Use user specified command if it exists
        if (!m_suspendCommand.isEmpty())
        {
            uint ret = myth_system(m_suspendCommand);
            if (ret == GENERIC_EXIT_OK)
            {
                handled = true;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "User defined SuspendCommand failed, falling back to alternative methods.");
            }
        }

        if (!handled && m_power && m_power->IsFeatureSupported(MythPower::FeatureSuspend))
            m_power->RequestFeature(MythPower::FeatureSuspend);
    }
    deleteLater();
}

void ExitPrompter::DoStandby()
{
    GetMythMainWindow()->IdleTimeout();
    deleteLater();
}

void ExitPrompter::MainDialogClosed(QString /*unused*/, int Id)
{
    // If the dialog box was closed without a result - or the result was no, then
    // delete
    if (Id < 1)
        deleteLater();
}

void ExitPrompter::HandleExit()
{
    // HACK IsFrontendOnly() triggers a popup if there is no BE connection.
    // We really don't need that right now. This hack prevents it.
    gContext->SetDisableEventPopup(true);

    // first of all find out, if this is a frontend only host...
    bool frontendOnly = gCoreContext->IsFrontendOnly();

    // HACK Undo the hack, just in case we _don't_ quit:
    gContext->SetDisableEventPopup(false);

    // how do you want to quit today?
    bool allowExit     = false;
    bool allowReboot   = false;
    bool allowShutdown = false;
    bool allowStandby  = false;
    bool allowSuspend  = false;

    bool haveshutdown = !m_haltCommand.isEmpty();
    bool havereboot   = !m_rebootCommand.isEmpty();
    bool havesuspend  = !m_suspendCommand.isEmpty();

#ifdef Q_OS_ANDROID
    haveshutdown = false;
    havereboot   = false;
    havesuspend  = false;
#endif

    if (m_power)
    {
        havereboot   |= m_power->IsFeatureSupported(MythPower::FeatureRestart);
        haveshutdown |= m_power->IsFeatureSupported(MythPower::FeatureShutdown);
        havesuspend  |= m_power->IsFeatureSupported(MythPower::FeatureSuspend);
    }

    switch (gCoreContext->GetNumSetting("OverrideExitMenu", 0))
    {
        case 0:
            allowExit = true;
            if (frontendOnly)
                allowShutdown = haveshutdown;
            break;
        case 1:
            allowExit = true;
            break;
        case 2:
            allowExit = true;
            allowShutdown = haveshutdown;
            break;
        case 3:
            allowExit     = true;
            allowReboot   = havereboot;
            allowShutdown = haveshutdown;
            break;
        case 4:
            allowShutdown = haveshutdown;
            break;
        case 5:
            allowReboot   = havereboot;
            break;
        case 6:
            allowReboot   = havereboot;
            allowShutdown = haveshutdown;
            break;
        case 7:
            allowStandby  = true;
            break;
        case 8:
            allowSuspend  = havesuspend;
            break;
        case 9:
            allowExit = true;
            allowSuspend  = havesuspend;
            break;
        case 10:
            allowExit = true;
            allowReboot = havereboot;
            allowShutdown = haveshutdown;
            allowSuspend = havesuspend;
            break;
    }

    MythScreenStack *ss = GetMythMainWindow()->GetStack("popup stack");
    auto *dlg = new MythDialogBox(tr("Do you really want to exit MythTV?"), ss,
                                  "exit prompt");

    if (!dlg->Create())
    {
        LOG(VB_GENERAL, LOG_ERR, "Can't create Exit Prompt dialog?");
        delete dlg;
        DoQuit();
        return;
    }

    // ensure ExitPrompt is deleted if the dialog is not actioned
    connect(dlg, &MythDialogBox::Closed, this, &ExitPrompter::MainDialogClosed);

    bool confirm = m_confirm || !frontendOnly;

    dlg->AddButton(QCoreApplication::translate("(Common)", "No"));
    if (allowExit)
        dlg->AddButton(tr("Yes, Exit now"), SLOT(DoQuit()));
    if (allowReboot)
        dlg->AddButton(tr("Yes, Exit and Reboot"), confirm ? SLOT(ConfirmReboot()) : SLOT(DoReboot()));
    if (allowShutdown)
        dlg->AddButton(tr("Yes, Exit and Shutdown"), confirm ? SLOT(ConfirmHalt()) : SLOT(DoHalt()));
    if (allowStandby)
        dlg->AddButton(tr("Yes, Enter Standby Mode"), SLOT(DoStandby()));
    if (allowSuspend)
        dlg->AddButton(tr("Yes, Suspend"), confirm ? SLOT(ConfirmSuspend()) : SLOT(DoSuspend()));

    // This is a hack so that the button clicks target the correct slot:
    dlg->SetReturnEvent(this, QString());

    ss->AddScreen(dlg);
}

void ExitPrompter::Confirm(MythPower::Feature Action)
{
    MythScreenStack *ss = GetMythMainWindow()->GetStack("popup stack");

    QString msg;
    switch (Action)
    {
        case MythPower::FeatureShutdown: msg = tr("Are you sure you want to shutdown?"); break;
        case MythPower::FeatureRestart:  msg = tr("Are you sure you want to reboot?");   break;
        case MythPower::FeatureSuspend:  msg = tr("Are you sure you want to suspend?");  break;
        default: break;
    }

    gContext->SetDisableEventPopup(true);
    if (!gCoreContext->IsFrontendOnly())
        msg.prepend(tr("Mythbackend is running on this system. "));
    gContext->SetDisableEventPopup(false);

    auto *dlg = new MythConfirmationDialog(ss, msg);

    if (!dlg->Create())
    {
        delete dlg;
        DoQuit();
        return;
    }

    if (Action == MythPower::FeatureShutdown)
        connect(dlg, &MythConfirmationDialog::haveResult, this, &ExitPrompter::DoHalt);
    else if (Action == MythPower::FeatureRestart)
        connect(dlg, &MythConfirmationDialog::haveResult, this, &ExitPrompter::DoReboot);
    else if (Action == MythPower::FeatureSuspend)
        connect(dlg, &MythConfirmationDialog::haveResult, this, &ExitPrompter::DoSuspend);

    ss->AddScreen(dlg);
}
