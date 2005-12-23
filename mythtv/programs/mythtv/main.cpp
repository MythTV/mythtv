#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <unistd.h>
#include "tv.h"
#include "programinfo.h"
#include "livetvchain.h"

#include "libmyth/exitcodes.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/mythdialogs.h"

#include <iostream>
using namespace std;

MythContext *gContext;

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
    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return TV_EXIT_NO_MYTHCONTEXT;
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

    QString auddevice = gContext->GetSetting("AudioOutputDevice");
    if (auddevice == "" || auddevice == QString::null)
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Audio not configured, you need "
                "to run 'mythfrontend', not 'mythtv'.");
        return TV_EXIT_NO_AUDIO;
    }

    print_verbose_messages |= VB_PLAYBACK | VB_LIBAV;// | VB_AUDIO;

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    TV::InitKeys();

    TV *tv = new TV();
    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Could not initializing TV class.");
        return TV_EXIT_NO_TV;
    }

    if (a.argc() > 1)
    {
        QString filename = a.argv()[1];

        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->endts = QDateTime::currentDateTime().addSecs(-180);
        pginfo->pathname = filename;
    
        tv->Playback(pginfo);
    }
    else
    {
        LiveTVChain *tvchain = new LiveTVChain();
        tvchain->InitializeNewChain(gContext->GetHostName());
        tv->LiveTV(tvchain, true);
    }

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
