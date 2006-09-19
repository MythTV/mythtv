#include <qapplication.h>
#include <qevent.h>
#include <qkeysequence.h>
#include <cstdio>
#include <cerrno>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include "mythcontext.h"

#include "lirc.h"
#include "lircevent.h"
#include "util.h"

#if (QT_VERSION < 0x030100)
#error Native LIRC support requires Qt 3.1 or greater.
#endif

LircClient::LircClient(QObject *main_window)
{
    mainWindow = main_window;
}

int LircClient::Init(const QString &config_file, const QString &program)
{
    int fd;

    /* Connect the unix socket */
    fd = lirc_init((char *)program.latin1(), 1);
    if (fd == -1)
    {
        VERBOSE(VB_IMPORTANT,
                QString("lirc_init failed for %1, see preceding messages")
                .arg(program));
        return -1;
    }

    /* parse the config file */
    if (lirc_readconfig((char *)config_file.latin1(), &lircConfig, NULL))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to read lirc config %1 for %2")
                .arg(config_file).arg(program));
        lirc_deinit();
        return -1;
    }

    external_app = gContext->GetSetting("LircKeyPressedApp") + " &";

    VERBOSE(VB_GENERAL,
            QString("lirc init success using configuration file: %1")
            .arg(config_file));

    return 0;
}

LircClient::~LircClient()
{
    lirc_deinit();
    lirc_freeconfig(lircConfig);
}

void LircClient::Process(void)
{
    char *code = 0;
    char *ir = 0;
    int ret;

    /* Process all events read */
    while (lirc_nextcode(&ir) == 0)
    {
        if (!ir)
            continue;
        while ((ret = lirc_code2char(lircConfig, ir, &code)) == 0 &&
               code != NULL)
        {
            QKeySequence a(code);

            int keycode = 0;

            // Send a dummy keycode if we couldn't convert the key sequence.
            // This is done so the main code can output a warning for bad
            // mappings.
            if (!a.count())
                QApplication::postEvent(mainWindow, new LircKeycodeEvent(code, 
                                        keycode, true));

            for (unsigned int i = 0; i < a.count(); i++)
            {
                keycode = a[i];

                QApplication::postEvent(mainWindow, new LircKeycodeEvent(code, 
                                        keycode, true));
                QApplication::postEvent(mainWindow, new LircKeycodeEvent(code, 
                                        keycode, false));

                SpawnApp();
            }
        }

        free(ir);
        if (ret == -1)
            break;
    }
}

void LircClient::SpawnApp(void)
{
    // Spawn app to thwap led (or what ever the user has picked if
    // anything) to give positive feedback that a key was received
    if (external_app == " &")
        return;

    int status = myth_system(external_app);

    if (status > 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("External key pressed command exited with status %1")
                .arg(status));
    }
}

