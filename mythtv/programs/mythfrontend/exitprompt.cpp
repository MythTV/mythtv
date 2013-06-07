#include "config.h"

#include <QCoreApplication>
#if CONFIG_QTDBUS
#include <QtDBus>
#include <QDBusInterface>
#endif

#include "exitprompt.h"
#include "mythcontext.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythsystemlegacy.h"
#include "mythlogging.h"
#include "exitcodes.h"

void ExitPrompter::quit()
{
    qApp->exit();
}

static bool DBusHalt(void)
{
#if CONFIG_QTDBUS
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

    return void_reply.isValid() || int_reply.isValid();
#endif
    return false;
}

void ExitPrompter::halt()
{
    QString halt_cmd = gCoreContext->GetSetting("HaltCommand","");
    int ret = -1;

    if (!halt_cmd.isEmpty()) /* Use user specified command if it exists */
    {
        ret = myth_system(halt_cmd);
        if (ret != GENERIC_EXIT_OK)
            LOG(VB_GENERAL, LOG_ERR,
                "User defined HaltCommand failed, falling back to "
                "alternative methods.");
    }

    /* If supported, use DBus to shutdown */
    if (ret != GENERIC_EXIT_OK && !DBusHalt())
        myth_system("sudo /sbin/halt -p");
}

static bool DBusReboot(void)
{
#if CONFIG_QTDBUS
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

    return void_reply.isValid() || int_reply.isValid();
#endif
    return false;
}

void ExitPrompter::reboot()
{
    QString reboot_cmd = gCoreContext->GetSetting("RebootCommand","");
    int ret = -1;

    if (!reboot_cmd.isEmpty()) /* Use user specified command if it exists */
    {
        ret = myth_system(reboot_cmd);
        if (ret != GENERIC_EXIT_OK)
            LOG(VB_GENERAL, LOG_ERR,
                "User defined RebootCommand failed, falling back to "
                "alternative methods.");
    }

    /* If supported, use DBus to reboot */
    if (ret != GENERIC_EXIT_OK && !DBusReboot())
        myth_system("sudo /sbin/reboot");
}

void ExitPrompter::handleExit()
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

    switch (gCoreContext->GetNumSetting("OverrideExitMenu", 0))
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
        LOG(VB_GENERAL, LOG_ERR, "Can't create Exit Prompt dialog?");
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
