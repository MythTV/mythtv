#include <qdict.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qsqldatabase.h>
#include <qkeysequence.h>
#include <qpaintdevicemetrics.h>
#ifdef QWS
#include <qwindowsystem_qws.h>
#endif

#ifdef USE_LIRC
#include <pthread.h>
#include "lirc.h"
#include "lircevent.h"
#endif

#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "mythpainter.h"
#include "mythpainter_ogl.h"
#include "mythpainter_qt.h"
#include "mythcontext.h"

//#include "screensaver.h"

#ifdef USE_LIRC
static void *SpawnLirc(void *param)
{
    MythMainWindow *main_window = (MythMainWindow *)param;
    QString config_file = MythContext::GetConfDir() + "/lircrc";
    QString program("mythtv");
    LircClient *cl = new LircClient(main_window);
    if (!cl->Init(config_file, program))
        cl->Process();

    return NULL;
}
#endif

class KeyContext
{
  public:
    void AddMapping(int key, QString action)
    {
        actionMap[key] = action;
    }

    bool GetMapping(int key, QString &action)
    {
        if (actionMap.count(key) > 0)
        {
            action = actionMap[key];
            return true;
        }
        return false;
    }

    QMap<int, QString> actionMap;
};

struct JumpData
{
    void (*callback)(void);
    QString destination;
    QString description;
};

/*
struct MHData
{
    void (*callback)(MythMediaDevice *mediadevice);
    int MediaType;
    QString destination;
    QString description;
};

struct MPData {
    QString description;
    MediaPlayCallback playFn;
};
*/

class MythMainWindowPrivate
{
  public:
    int TranslateKeyNum(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;
    int xbase, ybase;

    bool ignore_lirc_keys;

    bool exitingtomain;

    QDict<KeyContext> keyContexts;
    QMap<int, JumpData*> jumpMap;
    QMap<QString, JumpData> destinationMap;
/*
    QMap<QString, MHData> mediaHandlerMap;
    QMap<QString, MPData> mediaPluginMap;
*/
    void (*exitmenucallback)(void);

/*
    void (*exitmenumediadevicecallback)(MythMediaDevice* mediadevice);
    MythMediaDevice * mediadeviceforcallback;
*/

    int escapekey;

    QTimer *drawTimer;
    QValueVector<MythScreenStack *> stackList;
    MythScreenStack *mainStack;

    MythPainter *painter;

    bool AllowInput;

    QRegion repaintRegion;
};

// Make keynum in QKeyEvent be equivalent to what's in QKeySequence
int MythMainWindowPrivate::TranslateKeyNum(QKeyEvent* e)
{
    int keynum = e->key();

    if (keynum != Qt::Key_Escape &&
        (keynum <  Qt::Key_Shift || keynum > Qt::Key_ScrollLock))
    {
        Qt::ButtonState modifiers;
        // if modifiers have been pressed, rebuild keynum
        if ((modifiers = e->state()) != 0)
        {
            int modnum = (((modifiers & Qt::ShiftButton) &&
                           keynum > 0x7f) ? Qt::SHIFT : 0) |
                         ((modifiers & Qt::ControlButton) ? Qt::CTRL : 0) |
                         ((modifiers & Qt::MetaButton) ? Qt::META : 0) |
                         ((modifiers & Qt::AltButton) ? Qt::ALT : 0);
            modnum &= ~Qt::UNICODE_ACCEL;
            return (keynum |= modnum);
        }
    }

    return keynum;
}

static MythMainWindow *mainWin = NULL;

MythMainWindow *MythMainWindow::getMainWindow(void)
{
    if (!mainWin)
    {
        mainWin = new MythMainWindow();
    }

    return mainWin;
}

MythMainWindow *GetMythMainWindow(void)
{
    return MythMainWindow::getMainWindow();
}

MythPainter *GetMythPainter(void)
{
    return MythMainWindow::getMainWindow()->GetCurrentPainter();
}

MythMainWindow::MythMainWindow()
              : QGLWidget(NULL, "mainWindow")
{
    d = new MythMainWindowPrivate;

    d->AllowInput = false;

    d->painter = new MythOpenGLPainter();
    //d->painter = new MythQtPainter();

    Init();

    d->ignore_lirc_keys = false;
    d->exitingtomain = false;
    d->exitmenucallback = false;
//    d->exitmenumediadevicecallback = false;
//    d->mediadeviceforcallback = NULL;
    d->escapekey = Key_Escape;
    d->mainStack = NULL;

#ifdef USE_LIRC
    pthread_t lirc_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&lirc_tid, &attr, SpawnLirc, this);
#endif

    d->keyContexts.setAutoDelete(true);

    RegisterKey("Global", "UP", "Up Arrow", "Up");
    RegisterKey("Global", "DOWN", "Down Arrow", "Down");
    RegisterKey("Global", "LEFT", "Left Arrow", "Left");
    RegisterKey("Global", "RIGHT", "Right Arrow", "Right");
    RegisterKey("Global", "SELECT", "Select", "Return,Enter,Space");
    RegisterKey("Global", "ESCAPE", "Escape", "Esc");
    RegisterKey("Global", "MENU", "Pop-up menu", "M");
    RegisterKey("Global", "INFO", "More information", "I");

    RegisterKey("Global", "PAGEUP", "Page Up", "PgUp");
    RegisterKey("Global", "PAGEDOWN", "Page Down", "PgDown");

    RegisterKey("Global", "PREVVIEW", "Previous View", "Home");
    RegisterKey("Global", "NEXTVIEW", "Next View", "End");

    RegisterKey("Global", "HELP", "Help", "F1");

    RegisterKey("Global", "0", "0", "0");
    RegisterKey("Global", "1", "1", "1");
    RegisterKey("Global", "2", "2", "2");
    RegisterKey("Global", "3", "3", "3");
    RegisterKey("Global", "4", "4", "4");
    RegisterKey("Global", "5", "5", "5");
    RegisterKey("Global", "6", "6", "6");
    RegisterKey("Global", "7", "7", "7");
    RegisterKey("Global", "8", "8", "8");
    RegisterKey("Global", "9", "9", "9");

    qApp->setOverrideCursor(QCursor(Qt::BlankCursor));

    setAutoBufferSwap(false);

    d->drawTimer = new QTimer(this);
    connect(d->drawTimer, SIGNAL(timeout()), this, SLOT(drawTimeout()));
    d->drawTimer->start(1000 / 70);

    d->AllowInput = true;

    d->repaintRegion = QRegion(QRect(0,0,0,0));
}

MythMainWindow::~MythMainWindow()
{
    qApp->restoreOverrideCursor();

    while (!d->stackList.isEmpty())
    {
        delete d->stackList.back();
        d->stackList.pop_back();
    }

    delete d;
}

MythPainter *MythMainWindow::GetCurrentPainter(void)
{
    return d->painter;
}

void MythMainWindow::AddScreenStack(MythScreenStack *stack, bool main)
{
    d->stackList.push_back(stack);
    if (main)
        d->mainStack = stack;
}

MythScreenStack *MythMainWindow::GetMainStack(void)
{
    return d->mainStack;
}

void MythMainWindow::drawTimeout(void)
{
    bool redraw = false;

    if(!d->repaintRegion.isEmpty())
    {
        redraw = true;
    }
    
    QValueVector<MythScreenStack *>::Iterator it;
    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
    
        //
        //  Shouldn't we be pushing down the redraw region here (ie. the
        //  thing passed to us by Qt/Window events that said "this region
        //  needs to be redrawn")?
        //

        QValueVector<MythScreenType *> drawList;
        (*it)->GetDrawOrder(drawList);

        QValueVector<MythScreenType *>::Iterator screenit;
        for (screenit = drawList.begin(); screenit != drawList.end();
             ++screenit)
        {
            (*screenit)->Pulse();
        }

         // Should we care if non-top level screens need redrawing?
         // Good Question
        MythScreenType *top = (*it)->GetTopScreen();
        if (top && top->NeedsRedraw())
            redraw = true;
    }

    if (!redraw)
    {
        return;
    }

    d->painter->Begin(this);

    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
        QValueVector<MythScreenType *> redrawList;
        (*it)->GetDrawOrder(redrawList);

        QValueVector<MythScreenType *>::Iterator screenit;
        for (screenit = redrawList.begin(); screenit != redrawList.end(); 
             ++screenit)
        {
            (*screenit)->Draw(d->painter, 0, 0);
        }
    }

    d->painter->End();
    
    d->repaintRegion = QRegion(QRect(0,0,0,0));
}

void MythMainWindow::closeEvent(QCloseEvent *e)
{
    (void)e;
    qApp->quit();
}

void MythMainWindow::paintEvent(QPaintEvent *pe)
{
    d->repaintRegion = d->repaintRegion.unite(pe->region());
}

void MythMainWindow::Init(void)
{

    gContext->GetScreenSettings(d->xbase, d->screenwidth, d->wmult,
                                d->ybase, d->screenheight, d->hmult);
    setGeometry(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    setFixedSize(QSize(d->screenwidth, d->screenheight));

/*
    setFont(gContext->GetMediumFont());
    setCursor(QCursor(Qt::BlankCursor));
*/
    setGeometry(100, 100, 800, 600);

    if (d->painter->GetName() != "OpenGL")
        setFixedSize(QSize(800, 600));

#ifdef QWS
#if QT_VERSION >= 0x030300
    QWSServer::setCursorVisible(false);
#endif
#endif

    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
#ifdef QWS
    flags |= WResizeNoErase;
#endif
    setWFlags(flags);

    Show();
    move(d->xbase, d->ybase);
}

void MythMainWindow::Show(void)
{
    //if (gContext->GetNumSetting("RunFrontendInWindow", 0))
        show();
    //else
    //    showFullScreen();
    setActiveWindow();
}

void MythMainWindow::ExitToMainMenu(void)
{
/*
    QWidget *current = currentWidget();

    if (current && d->exitingtomain)
    {
        if (current->name() != QString("mainmenu"))
        {
            if (current->name() == QString("video playback window"))
            {
                MythEvent *me = new MythEvent("EXIT_TO_MENU");
                QApplication::postEvent(current, me);
                d->exitingtomain = true;
            }
            else if (MythDialog *dial = dynamic_cast<MythDialog*>(current))
            {
                (void)dial;
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey, 
                                               0, Qt::NoButton);
                QObject *key_target = getTarget(*key);
                QApplication::postEvent(key_target, key);
                d->exitingtomain = true;
            }
        }
        else
        {
            d->exitingtomain = false;
            if (d->exitmenucallback)
            {
                void (*callback)(void) = d->exitmenucallback;
                d->exitmenucallback = NULL;

                callback();
            }
            else if (d->exitmenumediadevicecallback)
            {
                void (*callback)(MythMediaDevice*) = d->exitmenumediadevicecallback;
                MythMediaDevice * mediadevice = d->mediadeviceforcallback;
                d->mediadeviceforcallback = NULL;
                callback(mediadevice);
            }
        }
    }
*/
}

bool MythMainWindow::TranslateKeyPress(const QString &context,
                                       QKeyEvent *e, QStringList &actions)
{
    actions = QStringList();
    int keynum = d->TranslateKeyNum(e);

    if (d->jumpMap.count(keynum) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->exitmenucallback = d->jumpMap[keynum]->callback;
        QApplication::postEvent(this, new ExitToMainMenuEvent());
        return false;
    }

    bool retval = false;

    QString action;

    if (d->keyContexts[context])
    {
        action = "";
        if (d->keyContexts[context]->GetMapping(keynum, action))
        {
            actions += action;
            retval = true;
        }
    }

    action = "";
    if (d->keyContexts["Global"]->GetMapping(keynum, action))
    {
        actions += action;
        retval = true;
    }

    return retval;
}

void MythMainWindow::RegisterKey(const QString &context, const QString &action,
                                 const QString &description, const QString &key)
{
    QString keybind = key;

    QSqlDatabase *db = QSqlDatabase::database();

    QString thequery = QString("SELECT keylist FROM keybindings WHERE "
                               "context = \"%1\" AND action = \"%2\" AND "
                               "hostname = \"%2\";")
                              .arg(context).arg(action)
                              .arg(gContext->GetHostName());

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        keybind = query.value(0).toString();
    }
    else
    {
        QString inskey = keybind;
        inskey.replace('\\', "\\\\");
        inskey.replace('\"', "\\\"");

        thequery = QString("INSERT INTO keybindings (context, action, "
                           "description, keylist, hostname) VALUES "
                           "(\"%1\", \"%2\", \"%3\", \"%4\", \"%5\");")
                           .arg(context).arg(action).arg(description)
                           .arg(inskey).arg(gContext->GetHostName());

        query = db->exec(thequery);
        if (!query.isActive())
            MythContext::DBError("Insert Keybinding", query);
    }

    QKeySequence keyseq(keybind);

    if (!d->keyContexts[context])
        d->keyContexts.insert(context, new KeyContext());

    for (unsigned int i = 0; i < keyseq.count(); i++)
    {
        int keynum = keyseq[i];
        keynum &= ~Qt::UNICODE_ACCEL;

        QString dummyaction = "";
        if (d->keyContexts[context]->GetMapping(keynum, dummyaction))
        {
            VERBOSE(VB_GENERAL, QString("Key %1 is already bound in context "
                                        "%2.").arg(keybind).arg(context));
        }
        else
        {
            d->keyContexts[context]->AddMapping(keynum, action);
            //VERBOSE(VB_GENERAL, QString("Binding: %1 to action: %2 (%3)")
            //                           .arg(key).arg(action)
            //                           .arg(context));
        }

        if (action == "ESCAPE" && context == "Global" && i == 0)
            d->escapekey = keynum;
    }
}

void MythMainWindow::RegisterJump(const QString &destination, 
                                  const QString &description,
                                  const QString &key, void (*callback)(void))
{
    QString keybind = key;

    QSqlDatabase *db = QSqlDatabase::database();

    QString thequery = QString("SELECT keylist FROM jumppoints WHERE "
                               "destination = \"%1\" and hostname = \"%2\";")
                              .arg(destination).arg(gContext->GetHostName());

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
 
        keybind = query.value(0).toString();
    }
    else
    {
        QString inskey = keybind;
        inskey.replace('\\', "\\\\");
        inskey.replace('\"', "\\\"");

        thequery = QString("INSERT INTO jumppoints (destination, description, "
                           "keylist, hostname) VALUES (\"%1\", \"%2\", \"%3\", "
                           "\"%4\");").arg(destination).arg(description)
                                      .arg(inskey)
                                      .arg(gContext->GetHostName());

        query = db->exec(thequery);
        if (!query.isActive())
            MythContext::DBError("Insert Jump Point", query);
    }
   
    JumpData jd = { callback, destination, description };
    d->destinationMap[destination] = jd;
 
    QKeySequence keyseq(keybind);

    if (!keyseq.isEmpty())
    {
        int keynum = keyseq[0];
        keynum &= ~Qt::UNICODE_ACCEL;

        if (d->jumpMap.count(keynum) == 0)
        {
            //VERBOSE(VB_GENERAL, QString("Binding: %1 to JumpPoint: %2")
            //                           .arg(keybind).arg(destination));

            d->jumpMap[keynum] = &d->destinationMap[destination];
        }
        else
        {
            VERBOSE(VB_GENERAL, QString("Key %1 is already bound to a jump "
                                        "point.").arg(keybind));
        }
    }
    //else
    //    VERBOSE(VB_GENERAL, QString("JumpPoint: %2 exists, no keybinding")
    //                               .arg(destination));
}

void MythMainWindow::JumpTo(const QString& destination)
{
    if (d->destinationMap.count(destination) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->exitmenucallback = d->destinationMap[destination].callback;
        QApplication::postEvent(this, new ExitToMainMenuEvent());
        return;
    }
}

bool MythMainWindow::DestinationExists(const QString& destination) const
{
    return (d->destinationMap.count(destination) > 0) ? true : false;
}

/*
void MythMainWindow::RegisterMediaHandler(const QString &destination,
                                          const QString &description,
                                          const QString &key, 
                              void (*callback)(MythMediaDevice*mediadevice),
                                          int mediaType)
{
    (void)key;

    if (d->mediaHandlerMap.count(destination) == 0) 
    {
        MHData mhd = { callback, mediaType, destination, description };

        VERBOSE(VB_GENERAL, QString("Registering %1 as a media handler")
                                   .arg(destination));

        d->mediaHandlerMap[destination] = mhd;
    }
    else 
    {
       VERBOSE(VB_GENERAL, QString("%1 is already registered as a media "
                                   "handler.").arg(destination));
    }
}

void MythMainWindow::RegisterMediaPlugin(const QString &name, 
                                         const QString &desc, 
                                         MediaPlayCallback fn)
{
    if (d->mediaPluginMap.count(name) == 0) 
    {
        VERBOSE(VB_GENERAL, QString("Registering %1 as a media playback "
                                    "plugin.").arg(name));
        MPData mpd = {desc, fn};
        d->mediaPluginMap[name] = mpd;
    } 
    else
    {
        VERBOSE(VB_GENERAL, QString("%1 is already registered as a media "
                                    "playback plugin.").arg(name));
    }
}
*/
bool MythMainWindow::HandleMedia(QString& /* handler */, const QString& /*mrl*/)
{
/*
    if (handler.length() < 1)
        handler = "Internal";

    // Let's see if we have a plugin that matches the handler name...
    if (d->mediaPluginMap.count(handler)) 
    {
        d->mediaPluginMap[handler].playFn(mrl);
        return true;
    }
*/
    return false;
}

void MythMainWindow::AllowInput(bool allow)
{
    d->AllowInput = allow;
}

void MythMainWindow::keyPressEvent(QKeyEvent *e)
{
    if (!d->AllowInput)
        return;

    QValueVector<MythScreenStack *>::Iterator it;
    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
         MythScreenType *top = (*it)->GetTopScreen();
         if (top)
         {
             if (top->keyPressEvent(e))
                 break;
         }
    }
}

void MythMainWindow::customEvent(QCustomEvent* /* ce */)
{
#if 0
    if (ce->type() == kExitToMainMenuEventType && d->exitingtomain)
    {
        ExitToMainMenu();
    }
    else if (ce->type() == kExternalKeycodeEventType)
    {
        ExternalKeycodeEvent *eke = (ExternalKeycodeEvent *)ce;
        int keycode = eke->getKeycode();

        QKeyEvent key(QEvent::KeyPress, keycode, 0, Qt::NoButton);

        QObject *key_target = getTarget(key);

        QApplication::sendEvent(key_target, &key);
    }
    else if (ce->type() == kMediaEventType) 
    {
        MediaEvent *media_event = (MediaEvent*)ce;
        // Let's see which of our jump points are configured to handle this 
        // type of media...  If there's more than one we'll want to show some 
        // UI to allow the user to select which jump point to use. But for 
        // now we're going to just use the first one.
        QMap<QString, MHData>::Iterator itr = d->mediaHandlerMap.begin();
        MythMediaDevice *pDev = media_event->getDevice();

        if (pDev) 
        {
            while (itr != d->mediaHandlerMap.end())
            {
                if ((itr.data().MediaType & (int)pDev->getMediaType()))
                {
                    VERBOSE(VB_ALL, "Found a handler");
                    d->exitingtomain = true;
                    d->exitmenumediadevicecallback = itr.data().callback;
                    d->mediadeviceforcallback = pDev;
                    QApplication::postEvent(this, new ExitToMainMenuEvent());
                    break;
                }
                itr++;
            }
        }
    }
#ifdef USE_LIRC
    else if (ce->type() == kLircKeycodeEventType && !d->ignore_lirc_keys) 
    {
        LircKeycodeEvent *lke = (LircKeycodeEvent *)ce;
        int keycode = lke->getKeycode();

        if (keycode) 
        {
            gContext->ResetScreensaver();

            int mod = keycode & MODIFIER_MASK;
            int k = keycode & ~MODIFIER_MASK; /* trim off the mod */
            int ascii = 0;
            QString text;

            if (k & UNICODE_ACCEL)
            {
                QChar c(k & ~UNICODE_ACCEL);
                ascii = c.latin1();
                text = QString(c);
            }

            QKeyEvent key(lke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, ascii, mod, text);

            QObject *key_target = getTarget(key);

            QApplication::sendEvent(key_target, &key);
        }
        else
        {
            cerr << "LircClient warning: attempt to convert '"
                 << lke->getLircText() << "' to a key sequence failed. Fix"
                                           " your key mappings.\n";
        }
    }
    else if (ce->type() == kLircMuteEventType)
    {
        LircMuteEvent *lme = (LircMuteEvent *)ce;
        d->ignore_lirc_keys = lme->eventsMuted();
    }
#endif
    else if (ce->type() == ScreenSaverEvent::kScreenSaverEventType)
    {
        ScreenSaverEvent *sse = (ScreenSaverEvent *)ce;
        switch (sse->getSSEventType())
        {
            case ScreenSaverEvent::ssetDisable:
            {
                gContext->DoDisableScreensaver();
                break;
            }
            case ScreenSaverEvent::ssetRestore:
            {
                gContext->DoRestoreScreensaver();
                break;
            }
            case ScreenSaverEvent::ssetReset:
            {
                gContext->DoResetScreensaver();
                break;
            }
            default:
            {
                cerr << "Unknown ScreenSaverEvent type: " <<
                        sse->getSSEventType() << std::endl;
            }
        }
    }
#endif
}

QObject *MythMainWindow::getTarget(QKeyEvent &key)
{
    QObject *key_target = NULL;

    key_target = QWidget::keyboardGrabber();

    if (!key_target)
    {
        QWidget *focus_widget = qApp->focusWidget();
        if (focus_widget && focus_widget->isEnabled())
        {
            key_target = focus_widget;

            // Yes this is special code for handling the
            // the escape key.
            if (key.key() == d->escapekey && focus_widget->topLevelWidget())
                key_target = focus_widget->topLevelWidget();
        }
    }

    if (!key_target)
        key_target = this;

    return key_target;
}

QFont CreateFont(const QString &face, int pointSize, int weight, bool italic)
{
    // Store w/h mult in MainWindow, and query here
    //pointSize = pointSize * GetMythMainWindow()->GetHMult();

    // cache this in MainWindow as well.
    QPaintDeviceMetrics pdm(GetMythMainWindow());

    // Maybe just a helper function in MyhtMainWindow to calc this all?
    float desired = 100.0;
    pointSize = (int)(pointSize * desired / pdm.logicalDpiY());

    return QFont(face, pointSize, weight, italic);
}

