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

            int keycode = 0;
#if (QT_VERSION > 0x030100)
            for (unsigned int i = 0; i < a.count(); i++)
            {
                keycode = a[i];
#else
                keycode = a;
#endif
                if (keycode)
                {
                    int mod = keycode & MODIFIER_MASK;
                    int k = keycode & ~MODIFIER_MASK; /* trim off the mod */
                    QString text(QChar(k >> 24));
                    QKeyEvent *key_down = new QKeyEvent(QEvent::KeyPress, k,
                                                        k >> 24, mod, text);
                    QKeyEvent *key_up = new QKeyEvent(QEvent::KeyRelease, k,
                                                      k >> 24, mod, text);

                    // Make the key events go to the widgets almost
                    // the same way Qt would.
                    //
                    // Note: You might be tempted to lock the app mutex, don't.
                    // If you are unfortunate enough to be in the modal
                    // processing loop your lock will block until that loop
                    // is done.
                    QObject *key_target = NULL;
                    if (!key_target) 
                        key_target = QWidget::keyboardGrabber();
                    if (!key_target) 
                    {
                        QWidget *focus_widget = qApp->focusWidget();
                        if (focus_widget && focus_widget->isEnabled()) 
                        {
                            key_target = focus_widget;

                            // Yes this is special code for handling the
                            // the escape key.
                            if (key_down->key() == Key_Escape &&
                                focus_widget->topLevelWidget()) 
                            {
                                key_target = focus_widget->topLevelWidget();
                            }
                        }
                    }
                    if (!key_target) 
                        key_target = mainWindow;
                    QApplication::postEvent(key_target, key_down);
                    QApplication::postEvent(key_target, key_up);
                }
#if (QT_VERSION > 0x030100)
            }
#endif
            if (!keycode)
            {
                cerr << "LircClient warning: attempt to convert '"
                     <<  code << "' to a key sequence failed. Fix your "
                                "key mappings.\n";
            }
        }

        free(ir);
        if (ret == -1)
            break;
    }
}

