#include <qapplication.h>
#include <qevent.h>
#include <qkeysequence.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <iostream>
using namespace std;

#include "lirc.h"

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
        cerr << "Failed to create lirc socket for " << program <<endl;
        return -1;
    }

    /* parse the config file */
    if (lirc_readconfig((char *)config_file.latin1(), &lircConfig, NULL))
    {
        cerr << "Failed to init " << config_file << " for " << program <<endl;
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
            for (unsigned int i = 0; i < a.count(); i++)
            {
                int mod = a[i] & MODIFIER_MASK;
                int k = a[i] & ~MODIFIER_MASK; /* trim off the mod */
                QString text(QChar(k >> 24));
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, k, k >> 24, 
                                               mod, text);
                QApplication::postEvent(mainWindow, key);
            }
        }
        free(ir);
        if (ret == -1)
            break;
    }
}

