#include <qapplication.h>
#include <qevent.h>
#include <qkeysequence.h>
#include <cstdio>
#include <cerrno>

#include <iostream>
using namespace std;

#include "lirc.h"
#include "lircevent.h"

#if (QT_VERSION < 0x030100)
#error Native LIRC support requires Qt 3.1 or greater.
#endif

LircClient::LircClient(QObject *main_window)
{
    mainWindow = main_window;
}

int LircClient::Init(QString &config_file, QString &program)
{
    int fd;

    /* Connect the unix socket */
    fd = lirc_init((char *)program.latin1(), 1);
    if (fd == -1)
    {
        cerr << "lirc_init failed for " << program
             << ", see preceding messages\n";
        return -1;
    }

    /* parse the config file */
    if (lirc_readconfig((char *)config_file.latin1(), &lircConfig, NULL))
    {
         cerr << "Failed to read lirc config " << config_file << " for " 
              << program << endl;
        lirc_deinit();
        return -1;
    }
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
            }
        }

        free(ir);
        if (ret == -1)
            break;
    }
}

