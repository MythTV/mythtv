#include <unistd.h>

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <qregexp.h>

#include "tv_play.h"
#include "programinfo.h"

#include "libmyth/exitcodes.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/mythdialogs.h"

#include <iostream>
using namespace std;

static void *run_priv_thread(void *data)
{
    (void)data;
    while (true) 
    {
        gContext->waitPrivRequest();
        
        for (MythPrivRequest req = gContext->popPrivRequest(); 
             true; req = gContext->popPrivRequest()) 
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
    QString geometry = QString::null;
    QString display  = QString::null;
#ifdef Q_WS_X11
    // Remember any -display or -geometry argument
    // which QApplication init will remove.
    for(int argpos = 1; argpos + 1 < argc; ++argpos)
    {
        if (!strcmp(argv[argpos],"-geometry"))
            geometry = argv[argpos+1];
        else if (!strcmp(argv[argpos],"-display"))
            display = argv[argpos+1];
    }
#endif

    QApplication a(argc, argv);

    print_verbose_messages |= VB_PLAYBACK | VB_LIBAV;// | VB_AUDIO;

    QMap<QString, QString> settingsOverride;
    int argpos = 1;
    QString filename = "";

    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if ((a.argc() - 1) > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return GENERIC_EXIT_INVALID_CMDLINE;

                ++argpos;
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing argument to -v/--verbose option");
                return COMMFLAG_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-O") ||
                 !strcmp(a.argv()[argpos],"--override-setting"))
        {
            if ((a.argc() - 1) > argpos)
            {
                QString tmpArg = a.argv()[argpos+1];
                if (tmpArg.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to "
                            "-O/--override-setting option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                } 
 
                QStringList pairs = QStringList::split(",", tmpArg);
                for (unsigned int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = QStringList::split("=", pairs[index]);
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    settingsOverride[tokens[0]] = tokens[1];
                }
            }
            else
            { 
                cerr << "Invalid or missing argument to -O/--override-setting "
                        "option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"-display") ||
                 !strcmp(a.argv()[argpos],"--display"))
        {
            if (a.argc()-1 > argpos)
            {
                display = a.argv()[argpos+1];
                if (display.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -display option\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                else
                    ++argpos;
            }
            else
            {
                cerr << "Missing argument to -display option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (a.argv()[argpos][0] != '-')
        {
            filename = a.argv()[argpos];
        }

        ++argpos;
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return TV_EXIT_NO_MYTHCONTEXT;
    }

    if (!display.isEmpty())
    {
        gContext->SetX11Display(display);
    }

    if (!geometry.isEmpty() && !gContext->ParseGeometryOverride(geometry))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Illegal -geometry argument '%1' (ignored)")
                .arg(geometry));
    }

    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(it.data()));
            gContext->OverrideSettingForSession(it.key(), it.data());
        }
    }

    // Create priveleged thread, then drop privs
    pthread_t priv_thread;

    int status = pthread_create(&priv_thread, NULL, run_priv_thread, NULL);
    if (status) 
    {
        perror("pthread_create");
        priv_thread = 0;
    }
    setuid(getuid());

    QString themename = gContext->GetSetting("Theme");
    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {   
        QString msg = QString("Fatal Error: Couldn't find theme '%1'.")
            .arg(themename);
        VERBOSE(VB_IMPORTANT, msg);
        return TV_EXIT_NO_THEME;
    }
    
    gContext->LoadQtConfig();

#if defined(Q_OS_MACX)
    // Mac OS X doesn't define the AudioOutputDevice setting
#else
    QString auddevice = gContext->GetSetting("AudioOutputDevice");
    if (auddevice == "" || auddevice == QString::null)
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Audio not configured, you need "
                "to run 'mythfrontend', not 'mythtv'.");
        return TV_EXIT_NO_AUDIO;
    }
#endif

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    TV::InitKeys();

    TV *tv = new TV();
    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Could not initializing TV class.");
        return TV_EXIT_NO_TV;
    }

    if (filename != "")
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->endts = QDateTime::currentDateTime().addSecs(-180);
        pginfo->pathname = filename;
        pginfo->isVideo = true;
    
        tv->Playback(pginfo);
    }
    else
        tv->LiveTV(true);

    qApp->unlock();
    while (tv->GetState() != kState_None)
    {
        usleep(1000);
        qApp->processEvents();
    }

    sleep(1);
    delete tv;

    if (priv_thread != 0) 
    {
        void *value;
        gContext->addPrivRequest(MythPrivRequest::MythExit, NULL);
        pthread_join(priv_thread, &value);
    }
    delete gContext;

    return TV_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
