
#include <QCoreApplication>
#include <QtDBus>
#include <QDBusInterface>

#include "exitprompt.h"
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythsystem.h"

void ExitPrompter::quit()
{
    qApp->exit();
}

void ExitPrompter::halt()
{
    QDBusInterface kde("org.kde.ksmserver",
                       "/KSMServer",
                       "org.kde.KSMServerInterface");
    QDBusInterface gnome("org.gnome.SessionManager",
                         "/org/gnome/SessionManager",
                         "org.gnome.SessionManager");
    QDBusInterface consolekit("org.freedesktop.ConsoleKit",
                              "/org/freedesktop/ConsoleKit/Manager",
                              "org.freedesktop.ConsoleKit.Manager",
                              QDBusConnection::systemBus());
    QDBusInterface hal("org.freedesktop.Hal",
                       "/org/freedesktop/Hal/devices/computer",
                       "org.freedesktop.Hal.Device.SystemPowerManagement",
                       QDBusConnection::systemBus());

    QDBusReply<void> void_reply = kde.call("logout", 0, 2, 2);
    QDBusReply<bool> bool_reply;
    QDBusReply<int>  int_reply;

    if (!void_reply.isValid())
    {
        bool_reply = gnome.call("CanShutdown");
        if (bool_reply.isValid() && bool_reply.value() == 1)
            void_reply = gnome.call("RequestShutdown");
    }
    if (!void_reply.isValid())
    {
        bool_reply = consolekit.call("CanStop");
        if (bool_reply.isValid() && bool_reply.value() == 1)
            void_reply = consolekit.call("Stop");
    }
    if (!void_reply.isValid())
        int_reply = hal.call("Shutdown");
    if (!void_reply.isValid() && !int_reply.isValid())
    {
        QString halt_cmd = gContext->GetSetting("HaltCommand",
                                            "sudo /sbin/halt -p");
        if (!halt_cmd.isEmpty())
            myth_system(halt_cmd);
        else
            VERBOSE(VB_IMPORTANT, "Cannot halt - null command!");
    }
}

void ExitPrompter::reboot()
{
    QDBusInterface kde("org.kde.ksmserver",
                       "/KSMServer",
                       "org.kde.KSMServerInterface");
    QDBusInterface gnome("org.gnome.SessionManager",
                         "/org/gnome/SessionManager",
                         "org.gnome.SessionManager");
    QDBusInterface consolekit("org.freedesktop.ConsoleKit",
                              "/org/freedesktop/ConsoleKit/Manager",
                              "org.freedesktop.ConsoleKit.Manager",
                              QDBusConnection::systemBus());
    QDBusInterface hal("org.freedesktop.Hal",
                       "/org/freedesktop/Hal/devices/computer",
                       "org.freedesktop.Hal.Device.SystemPowerManagement",
                       QDBusConnection::systemBus());

    QDBusReply<void> void_reply = kde.call("logout", 0, 1, 2);
    QDBusReply<bool> bool_reply;
    QDBusReply<int>  int_reply;

    if (!void_reply.isValid())
    {
        bool_reply = gnome.call("CanShutdown");
        if (bool_reply.isValid() && bool_reply.value() == 1)
            void_reply=gnome.call("RequestReboot");
    }
    if (!void_reply.isValid())
    {
        bool_reply = consolekit.call("CanRestart");
        if (bool_reply.isValid() && bool_reply.value() == 1)
            void_reply = consolekit.call("Restart");
    }
    if (!void_reply.isValid())
        int_reply = hal.call("Reboot");
    if (!void_reply.isValid() && !int_reply.isValid())
    {
        QString reboot_cmd = gContext->GetSetting("RebootCommand",
                                              "sudo /sbin/reboot");
        if (!reboot_cmd.isEmpty())
            myth_system(reboot_cmd);
        else
            VERBOSE(VB_IMPORTANT, "Cannot reboot - null command!");
    }
}

void ExitPrompter::handleExit()
{
    // IsFrontendOnly() triggers a popup if there is no BE connection.
    // We really don't need that right now. This hack prevents it.
    gContext->SetMainWindow(NULL);

    // first of all find out, if this is a frontend only host...
    bool frontendOnly = gContext->IsFrontendOnly();

    // Undo the hack, just in case we _don't_ quit:
    gContext->SetMainWindow(MythMainWindow::getMainWindow());


    // how do you want to quit today?
    bool allowExit     = false;
    bool allowReboot   = false;
    bool allowShutdown = false;

    switch (gContext->GetNumSetting("OverrideExitMenu", 0))
    {
        case 0:
            allowExit = true;
            if (frontendOnly)
                allowShutdown = true;
            break;
        case 1:
            allowExit = true;
            break;
        case 2:
            allowExit = true;
            allowShutdown = true;
            break;
        case 3:
            allowExit     = true;
            allowReboot   = true;
            allowShutdown = true;
            break;
        case 4:
            allowShutdown = true;
            break;
        case 5:
            allowReboot = true;
            break;
        case 6:
            allowReboot   = true;
            allowShutdown = true;
            break;
    }

    MythScreenStack *ss = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *dlg = new MythDialogBox(
        tr("Do you really want to exit MythTV?"), ss, "exit prompt");

    if (!dlg->Create())
    {
        VERBOSE(VB_IMPORTANT, "Can't create Exit Prompt dialog?");
        delete dlg;
        quit();
    }

    dlg->AddButton(tr("No"));
    if (allowExit)
        dlg->AddButton(QObject::tr("Yes, Exit now"),          SLOT(quit()));
    if (allowReboot)
        dlg->AddButton(QObject::tr("Yes, Exit and Reboot"),   SLOT(reboot()));
    if (allowShutdown)
        dlg->AddButton(QObject::tr("Yes, Exit and Shutdown"), SLOT(halt()));

    // This is a hack so that the button clicks target the correct slot:
    dlg->SetReturnEvent(this, QString());

    ss->AddScreen(dlg);
}
