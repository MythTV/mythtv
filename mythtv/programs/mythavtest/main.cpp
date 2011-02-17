#include <unistd.h>
#include <iostream>
using namespace std;

#include <QString>
#include <QRegExp>
#include <QDir>
#include <QApplication>

#include "tv_play.h"
#include "programinfo.h"
#include "mythcommandlineparser.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythdbcon.h"
#include "compat.h"
#include "dbcheck.h"

// libmythui
#include "mythuihelper.h"
#include "mythmainwindow.h"

static void *run_priv_thread(void *data)
{
    (void)data;
    while (true)
    {
        gCoreContext->waitPrivRequest();

        for (MythPrivRequest req = gCoreContext->popPrivRequest();
             true; req = gCoreContext->popPrivRequest())
        {
            bool done = false;

            switch (req.getType())
            {
            case MythPrivRequest::MythRealtime:
                {
                    pthread_t *target_thread = (pthread_t *)(req.getData());
                    // Raise the given thread to realtime priority
                    struct sched_param sp = {1};
                    if (target_thread)
                    {
                        int status = pthread_setschedparam(
                            *target_thread, SCHED_FIFO, &sp);
                        if (status)
                        {
                            // perror("pthread_setschedparam");
                            VERBOSE(VB_GENERAL, "Realtime priority would require SUID as root.");
                        }
                        else
                            VERBOSE(VB_GENERAL, "Using realtime priority.");
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, "Unexpected NULL thread ptr "
                                "for MythPrivRequest::MythRealtime");
                    }
                }
                break;
            case MythPrivRequest::MythExit:
                pthread_exit(NULL);
                break;
            case MythPrivRequest::PrivEnd:
                done = true; // queue is empty
                break;
            }
            if (done)
                break; // from processing the queue
        }
    }
    return NULL; // will never happen
}

int main(int argc, char *argv[])
{
    bool cmdline_err;
    MythCommandLineParser cmdline(
        kCLPOverrideSettings     |
        kCLPWindowed             |
        kCLPNoWindowed           |
#ifdef USING_X11
        kCLPDisplay              |
#endif // USING_X11
        kCLPGeometry             |
        kCLPVerbose              |
        kCLPExtra                |
        kCLPHelp);

    print_verbose_messages |= VB_PLAYBACK | VB_LIBAV;

    for (int argpos = 1; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
    }

    QApplication a(argc, argv);

    QFileInfo finfo(a.argv()[0]);
    QString binname = finfo.baseName();

    int argpos = 1;
    QString filename = "";

    while (argpos < a.argc())
    {
        if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
        else if (a.argv()[argpos][0] != '-')
        {
            filename = a.argv()[argpos];
        }
        else
        {
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    if (!cmdline.GetDisplay().isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.GetDisplay());
    }

    if (!cmdline.GetGeometry().isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(cmdline.GetGeometry());
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    gCoreContext->SetAppName(binname);

    QMap<QString, QString> settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    // Create priveledged thread, then drop privs
    pthread_t priv_thread;
    bool priv_thread_created = true;

    int status = pthread_create(&priv_thread, NULL, run_priv_thread, NULL);
    if (status)
    {
        VERBOSE(VB_IMPORTANT, QString("Warning: ") +
                "Failed to create priveledged thread." + ENO);
        priv_thread_created = false;
    }
    setuid(getuid());

    QString themename = gCoreContext->GetSetting("Theme");
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        QString msg = QString("Fatal Error: Couldn't find theme '%1'.")
            .arg(themename);
        VERBOSE(VB_IMPORTANT, msg);
        return GENERIC_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

#if defined(Q_OS_MACX)
    // Mac OS X doesn't define the AudioOutputDevice setting
#else
    QString auddevice = gCoreContext->GetSetting("AudioOutputDevice");
    if (auddevice.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Audio not configured, you need "
                "to run 'mythfrontend', not 'mythtv'.");
        return GENERIC_EXIT_SETUP_ERROR;
    }
#endif

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();

    TV::InitKeys();

    if (!UpgradeTVDatabaseSchema(false))
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Incorrect database schema.");
        delete gContext;
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    TV *tv = new TV();
    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Could not initialize TV class.");
        return GENERIC_EXIT_NOT_OK;
    }

    if (filename.isEmpty())
    {
        TV::StartTV(NULL, kStartTVNoFlags);
    }
    else
    {
        ProgramInfo pginfo(filename);
        TV::StartTV(&pginfo, kStartTVNoFlags);
    }

    if (priv_thread_created)
    {
        gCoreContext->addPrivRequest(MythPrivRequest::MythExit, NULL);
        pthread_join(priv_thread, NULL);
    }
    delete gContext;

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
