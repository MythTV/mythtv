#include <qdict.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qkeysequence.h>
#include <qpaintdevicemetrics.h>
#ifdef QWS
#include <qwindowsystem_qws.h>
#endif
#ifdef Q_WS_MACX
#import <HIToolbox/Menus.h>   // For GetMBarHeight()
#endif

#include <pthread.h>

#ifdef USE_LIRC
#include "lirc.h"
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
#endif

#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "mythpainter.h"
#include "mythpainter_ogl.h"
#include "mythpainter_qt.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythgesture.h"

/* from libmyth */
#include "screensaver.h"
#include "mythdialogs.h"
#include "mythmediamonitor.h"

#define GESTURE_TIMEOUT 1000

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

#ifdef USE_JOYSTICK_MENU
static void *SpawnJoystickMenu(void *param)
{
    MythMainWindow *main_window = (MythMainWindow *)param;
    QString config_file = MythContext::GetConfDir() + "/joystickmenurc";
    JoystickMenuClient *js = new JoystickMenuClient(main_window);
    if (!js->Init(config_file))
        js->Process();

    return NULL;
}
#endif

class KeyContext
{
  public:
    void AddMapping(int key, QString action)
    {
        actionMap[key].append(action);
    }

    bool GetMapping(int key, QStringList &actions)
    {
        if (actionMap.count(key) > 0)
        {
            actions += actionMap[key];
            return true;
        }
        return false;
    }

    QMap<int, QStringList> actionMap;
};

struct JumpData
{
    void (*callback)(void);
    QString destination;
    QString description;
};

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

class MythMainWindowPrivate
{
  public:
    int TranslateKeyNum(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;

    QRect screenRect;

    int xbase, ybase;
    bool does_fill_screen;

    bool ignore_lirc_keys;
    bool ignore_joystick_keys;

    bool exitingtomain;
    bool popwindows;

    QDict<KeyContext> keyContexts;
    QMap<int, JumpData*> jumpMap;
    QMap<QString, JumpData> destinationMap;
    QMap<QString, MHData> mediaHandlerMap;
    QMap<QString, MPData> mediaPluginMap;

    void (*exitmenucallback)(void);

    void (*exitmenumediadevicecallback)(MythMediaDevice* mediadevice);
    MythMediaDevice * mediadeviceforcallback;

    int escapekey;

    QTimer *drawTimer;
    QValueVector<MythScreenStack *> stackList;
    MythScreenStack *mainStack;

    MythPainter *painter;

    bool AllowInput;

    QRegion repaintRegion;

    MythGesture gesture;
    QTimer *gestureTimer;

    /* compatability only, FIXME remove */
    vector<QWidget *> widgetList;
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

void MythMainWindow::destroyMainWindow(void)
{
    if (mainWin)
        delete mainWin;
    mainWin = NULL;
}

MythMainWindow *GetMythMainWindow(void)
{
    return MythMainWindow::getMainWindow();
}

void DestroyMythMainWindow(void)
{
    MythMainWindow::destroyMainWindow();
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

    QString painter = gContext->GetSetting("ThemePainter", "qt");
    if (painter == "opengl")
    {
        VERBOSE(VB_GENERAL, "Using the OpenGL painter");
        d->painter = new MythOpenGLPainter();
    }
    else
    {
        VERBOSE(VB_GENERAL, "Using the Qt painter");
        d->painter = new MythQtPainter();
    }

    Init();

    d->ignore_lirc_keys = false;
    d->ignore_joystick_keys = false;
    d->exitingtomain = false;
    d->popwindows = true;
    d->exitmenucallback = false;
    d->exitmenumediadevicecallback = false;
    d->mediadeviceforcallback = NULL;
    d->escapekey = Key_Escape;
    d->mainStack = NULL;

    installEventFilter(this);

#ifdef USE_LIRC
    pthread_t lirc_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&lirc_tid, &attr, SpawnLirc, this);
#endif

#ifdef USE_JOYSTICK_MENU
    d->ignore_joystick_keys = false;
    pthread_t js_tid;

    pthread_attr_t attr2;
    pthread_attr_init(&attr2);
    pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_DETACHED);

    pthread_create(&js_tid, &attr2, SpawnJoystickMenu, this);
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
    RegisterKey("Global", "EJECT", "Eject Removable Media", "");

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

    setAutoBufferSwap(false);

    d->gestureTimer = new QTimer(this);
    connect(d->gestureTimer, SIGNAL(timeout()), this, SLOT(mouseTimeout()));

    d->drawTimer = new QTimer(this);
    connect(d->drawTimer, SIGNAL(timeout()), this, SLOT(drawTimeout()));
    d->drawTimer->start(1000 / 70);

    d->AllowInput = true;

    d->repaintRegion = QRegion(QRect(0,0,0,0));
}

MythMainWindow::~MythMainWindow()
{
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
    /* FIXME: remove */
    if (currentWidget())
        return;

    bool redraw = false;

    if (!d->repaintRegion.isEmpty())
        redraw = true;

    QValueVector<MythScreenStack *>::Iterator it;
    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
        QValueVector<MythScreenType *> drawList;
        (*it)->GetDrawOrder(drawList);

        QValueVector<MythScreenType *>::Iterator screenit;
        for (screenit = drawList.begin(); screenit != drawList.end();
             ++screenit)
        {
            (*screenit)->Pulse();
        }

        // Should we care if non-top level screens need redrawing?
        MythScreenType *top = (*it)->GetTopScreen();
        if (top && top->NeedsRedraw())
        {
            QRegion topDirty = top->GetDirtyArea();
            d->repaintRegion = d->repaintRegion.unite(topDirty);
            redraw = true;
        }
    }

    if (!redraw)
    {
        return;
    }

    if (!d->painter->SupportsClipping())
        d->repaintRegion = d->repaintRegion.unite(d->screenRect);

    d->painter->Begin(this);

    QMemArray<QRect> rects = d->repaintRegion.rects();

    for (unsigned int i = 0; i < rects.size(); i++)
    {
        if (rects[i].width() == 0 || rects[i].height() == 0)
            continue;

        if (rects[i] != d->screenRect)
            d->painter->SetClipRect(rects[i]);

        for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
        {
            QValueVector<MythScreenType *> redrawList;
            (*it)->GetDrawOrder(redrawList);

            QValueVector<MythScreenType *>::Iterator screenit;
            for (screenit = redrawList.begin(); screenit != redrawList.end(); 
                 ++screenit)
            {
                (*screenit)->Draw(d->painter, 0, 0, 255, rects[i]);
            }
        }
    }

    d->painter->End();
    
    d->repaintRegion = QRegion(QRect(0, 0, 0, 0));
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

    d->screenRect = QRect(d->xbase, d->ybase, d->screenwidth, d->screenheight);

    setGeometry(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    setFixedSize(QSize(d->screenwidth, d->screenheight));

    bool hideCursor = gContext->GetNumSetting("HideMouseCursor", 1);
#ifdef QWS
#if QT_VERSION >= 0x030300
    QWSServer::setCursorVisible(!hideCursor);
#endif
#endif

    if (gContext->GetNumSetting("RunFrontendInWindow", 0))
        d->does_fill_screen = false;
    else
        d->does_fill_screen = true;

    // Set window border based on fullscreen attribute
    Qt::WFlags flags = 0;
    if (d->does_fill_screen)
        flags = Qt::WStyle_Customize  |
                Qt::WStyle_NoBorder;
    else
        flags = Qt::WStyle_Customize | Qt::WStyle_NormalBorder;

    // Workarounds for Qt/Mac bugs
#ifdef Q_WS_MACX
    if (d->does_fill_screen)
    {
  #if QT_VERSION >= 0x030303
        flags = Qt::WStyle_Customize | Qt::WStyle_Splash;
  #else
        // Qt/Mac 3.3.2 and earlier have problems with input focus
        // when a borderless window is created more than once,
        // so we have to force windows to have borders
        flags = Qt::WStyle_Customize | Qt::WStyle_DialogBorder;

        // Move this window up enough to hide its title bar, which in
        // all the OS X releases so far is the same height as the menubar
        d->ybase -= GetMBarHeight();
  #endif
    }
#endif

    flags |= WRepaintNoErase;
#ifdef QWS
    flags |= WResizeNoErase;
#endif

    reparent(parentWidget(), flags, pos());

    /* FIXME these two lines should go away */
    setFont(gContext->GetMediumFont());
    gContext->ThemeWidget(this);

    Show();

    // Set cursor call must come after Show() to work on some systems.
    setCursor((hideCursor) ? (Qt::BlankCursor) : (Qt::ArrowCursor));

    move(d->xbase, d->ybase);
}

void MythMainWindow::Show(void)
{
    show();
#ifdef Q_WS_MACX
    if (d->does_fill_screen)
        HideMenuBar();
    else
        ShowMenuBar();
#endif

    setActiveWindow();
    raise();
    qApp->wakeUpGuiThread();    // ensures that setActiveWindow() occurs
}

/* FIXME compatability only */
void MythMainWindow::attach(QWidget *child)
{
    if (currentWidget())
        currentWidget()->setEnabled(false);

    d->widgetList.push_back(child);
    child->raise();
    child->setFocus();
}

void MythMainWindow::detach(QWidget *child)
{
    if (d->widgetList.back() != child)
    {
        //cerr << "Not removing top-most widget\n";
        return;
    }

    d->widgetList.pop_back();
    QWidget *current = currentWidget();

    if (current)
    {
        current->setEnabled(true);
        current->setFocus();
    }

    if (d->exitingtomain)
        QApplication::postEvent(this, new ExitToMainMenuEvent());
}

QWidget *MythMainWindow::currentWidget(void)
{
    if (d->widgetList.size() > 0)
        return d->widgetList.back();
    return NULL;
}
/* FIXME: end compatability */

bool MythMainWindow::IsExitingToMain(void) const
{
    return d->exitingtomain;
}

void MythMainWindow::ExitToMainMenu(void)
{
    bool jumpdone = !(d->popwindows);

    d->exitingtomain = true;

    /* compatability code, remove, FIXME */
    QWidget *current = currentWidget();
    if (current && d->exitingtomain && d->popwindows)
    {
        if (current->name() != QString("mainmenu"))
        {
            if (current->name() == QString("video playback window"))
            {
                MythEvent *me = new MythEvent("EXIT_TO_MENU");
                QApplication::postEvent(current, me);
            }
            else if (MythDialog *dial = dynamic_cast<MythDialog*>(current))
            {
                (void)dial;
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey, 
                                               0, Qt::NoButton);
                QObject *key_target = getTarget(*key);
                QApplication::postEvent(key_target, key);
            }
        }
        else
            jumpdone = true;
    }

    MythScreenStack *toplevel = GetMainStack();
    if (toplevel && d->popwindows)
    {
        MythScreenType *screen = toplevel->GetTopScreen();
        if (screen && screen->name() != QString("mainmenu"))
        {
            if (screen->name() == QString("video playback window"))
            {
                MythEvent *me = new MythEvent("EXIT_TO_MENU");
                QApplication::postEvent(screen, me);
            }
            else
            {
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey,
                                               0, Qt::NoButton);
                QApplication::postEvent(this, key);
            }
        }
        else
            jumpdone = true;
    }

    if (jumpdone)
    {
        d->exitingtomain = false;
        d->popwindows = true;
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

bool MythMainWindow::TranslateKeyPress(const QString &context,
                                       QKeyEvent *e, QStringList &actions,
                                       bool allowJumps)
{
    actions.clear();
    int keynum = d->TranslateKeyNum(e);

    if (allowJumps && 
        d->jumpMap.count(keynum) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->exitmenucallback = d->jumpMap[keynum]->callback;
        QApplication::postEvent(this, new ExitToMainMenuEvent());
        return false;
    }

    bool retval = false;

    if (d->keyContexts[context])
    {
        if (d->keyContexts[context]->GetMapping(keynum, actions))
            retval = true;
    }

    if (context != "Global" && 
        d->keyContexts["Global"]->GetMapping(keynum, actions))
    {
        retval = true;
    }

    return retval;
}

void MythMainWindow::ClearKey(const QString &context, const QString &action)
{
    KeyContext * keycontext = d->keyContexts[context];
    if (keycontext == NULL) return;

    QMap<int,QStringList>::Iterator it;
    for (it = keycontext->actionMap.begin();
         it != keycontext->actionMap.end();
         it++)
    {

        /* find a pair with the action we are looking for */
        QStringList::iterator at = it.data().find(action);

        /* until the end of actions is reached check for keys */
        while (at != it.data().end())
        {
            /* the key should never contain duplicate actions */
            at = it.data().remove(at);
            /* but just in case, look again */
            at = it.data().find(at, action);
        }

        /* dont keep unbound keys in the map */
        if (it.data().isEmpty()) keycontext->actionMap.erase(it);
    }
}

void MythMainWindow::BindKey(const QString &context, const QString &action,
                             const QString &key)
{
    QKeySequence keyseq(key);

    if (!d->keyContexts[context])
        d->keyContexts.insert(context, new KeyContext());

    for (unsigned int i = 0; i < keyseq.count(); i++)
    {
        int keynum = keyseq[i];
        keynum &= ~Qt::UNICODE_ACCEL;

        QStringList dummyaction = "";
        if (d->keyContexts[context]->GetMapping(keynum, dummyaction))
        {
            VERBOSE(VB_GENERAL, QString("Key %1 is bound to multiple actions "
                                        "in context %2.")
                    .arg(key).arg(context));
        }

        d->keyContexts[context]->AddMapping(keynum, action);
        //VERBOSE(VB_GENERAL, QString("Binding: %1 to action: %2 (%3)")
        //                           .arg(key).arg(action)
        //                           .arg(context));

        if (action == "ESCAPE" && context == "Global" && i == 0)
            d->escapekey = keynum;
    }
}

void MythMainWindow::RegisterKey(const QString &context, const QString &action,
                                 const QString &description, const QString &key)
{
    QString keybind = key;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT keylist FROM keybindings WHERE "
                      "context = :CONTEXT AND action = :ACTION AND "
                      "hostname = :HOSTNAME ;");
        query.bindValue(":CONTEXT", context);
        query.bindValue(":ACTION", action);
        query.bindValue(":HOSTNAME", gContext->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            keybind = query.value(0).toString();
        }
        else
        {
            QString inskey = keybind;
            inskey.replace('\\', "\\\\");
            inskey.replace('\"', "\\\"");

            query.prepare("INSERT INTO keybindings (context, action, "
                          "description, keylist, hostname) VALUES "
                          "( :CONTEXT, :ACTION, :DESCRIPTION, :KEYLIST, "
                          ":HOSTNAME );");
            query.bindValue(":CONTEXT", context);
            query.bindValue(":ACTION", action);
            query.bindValue(":DESCRIPTION", description);
            query.bindValue(":KEYLIST", inskey);
            query.bindValue(":HOSTNAME", gContext->GetHostName());

            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError("Insert Keybinding", query);
            }
        }
    }

    BindKey(context, action, keybind);
}

QString MythMainWindow::GetKey(const QString &context,
                               const QString &action) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return "?";

    query.prepare("SELECT keylist "
                  "FROM keybindings "
                  "WHERE context  = :CONTEXT AND "
                  "      action   = :ACTION  AND "
                  "      hostname = :HOSTNAME");
    query.bindValue(":CONTEXT", context);
    query.bindValue(":ACTION", action);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive() || !query.next())
        return "?";

    return query.value(0).toString();
}

void MythMainWindow::ClearJump(const QString &destination)
{
    /* make sure that the jump point exists (using [] would add it)*/
    if (d->destinationMap.find(destination) == d->destinationMap.end())
    {
       VERBOSE(VB_GENERAL, "Cannot clear ficticious jump point"+destination);
       return;
    }

    QMap<int, JumpData*>::Iterator it;
    for (it = d->jumpMap.begin(); it != d->jumpMap.end(); ++it)
    {

       JumpData *jd = it.data();
       if (jd->destination == destination)
           d->jumpMap.remove(it);
    }
}


void MythMainWindow::BindJump(const QString &destination, const QString &key)
{
    /* make sure the jump point exists */
    if (d->destinationMap.find(destination) == d->destinationMap.end())
    {
       VERBOSE(VB_GENERAL,"Cannot bind to ficticious jump point"+destination);
       return;
    }

    QKeySequence keyseq(key);

    for (unsigned int i = 0; i < keyseq.count(); i++)
    {
        int keynum = keyseq[i];
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
                                        "point.").arg(key));
        }
    }
    //else
    //    VERBOSE(VB_GENERAL, QString("JumpPoint: %2 exists, no keybinding")
    //                               .arg(destination));

}

void MythMainWindow::RegisterJump(const QString &destination, 
                                  const QString &description,
                                  const QString &key, void (*callback)(void))
{
    QString keybind = key;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT keylist FROM jumppoints WHERE "
                      "destination = :DEST and hostname = :HOST ;");
        query.bindValue(":DEST", destination);
        query.bindValue(":HOST", gContext->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            keybind = query.value(0).toString();
        }
        else
        {
            QString inskey = keybind;
            inskey.replace('\\', "\\\\");
            inskey.replace('\"', "\\\"");

            query.prepare("INSERT INTO jumppoints (destination, description, "
                          "keylist, hostname) VALUES ( :DEST, :DESC, :KEYLIST, "
                          ":HOST );");
            query.bindValue(":DEST", destination);
            query.bindValue(":DEDC", description);
            query.bindValue(":KEYLIST", inskey);
            query.bindValue(":HOST", gContext->GetHostName());

            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError("Insert Jump Point", query);
            }
        }
    }
 
    JumpData jd = { callback, destination, description };
    d->destinationMap[destination] = jd;

    BindJump(destination, keybind); 
}

void MythMainWindow::JumpTo(const QString& destination, bool pop)
{
    if (d->destinationMap.count(destination) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->popwindows = pop;
        d->exitmenucallback = d->destinationMap[destination].callback;
        QApplication::postEvent(this, new ExitToMainMenuEvent());
        return;
    }
}

bool MythMainWindow::DestinationExists(const QString& destination) const
{
    return (d->destinationMap.count(destination) > 0) ? true : false;
}

void MythMainWindow::RegisterMediaHandler(const QString &destination,
                                          const QString &description,
                                          const QString &/*key*/,
                                          void (*callback)(MythMediaDevice*),
                                          int            mediaType,
                                          const QString &extensions)
{
    if (d->mediaHandlerMap.count(destination) == 0) 
    {
        MHData mhd = { callback, mediaType, destination, description };

        VERBOSE(VB_GENERAL, QString("Registering %1 as a media handler %2")
                .arg(destination).arg(QString("ext(%1)").arg(extensions)));

        d->mediaHandlerMap[destination] = mhd;

        MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
        if (!extensions.isEmpty())
            mon->MonitorRegisterExtensions(mediaType, extensions);
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

bool MythMainWindow::HandleMedia(QString &handler, const QString &mrl, 
                                 const QString &plot, const QString &title, 
                                 const QString &director, int lenMins, 
                                 const QString &year)
{
    if (handler.length() < 1)
        handler = "Internal";

    // Let's see if we have a plugin that matches the handler name...
    if (d->mediaPluginMap.count(handler)) 
    {
        d->mediaPluginMap[handler].playFn(mrl, plot, title, director, lenMins, year);
        return true;
    }

    return false;
}

void MythMainWindow::AllowInput(bool allow)
{
    d->AllowInput = allow;
}

void MythMainWindow::mouseTimeout(void)
{
    MythGestureEvent *e;

    /* complete the stroke if its our first timeout */
    if (d->gesture.recording())
    {
        d->gesture.stop();
    }

    /* get the last gesture */
    e = d->gesture.gesture();

    if (e->gesture() < MythGestureEvent::Click)
        QApplication::postEvent(this, e);
}

bool MythMainWindow::eventFilter(QObject *, QEvent *e)
{
    MythGestureEvent *ge;

    /* dont let anything through if input is disallowed. */
    if (!d->AllowInput)
        return true;

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            if (currentWidget())
            {
                QKeyEvent *ke = dynamic_cast<QKeyEvent*>(e);
                ke->accept();
                QWidget *current = currentWidget();
                if (current && current->isEnabled())
                    qApp->notify(current, ke);
                //else
                //    QDialog::keyPressEvent(ke);

                break;
            }

            QValueVector<MythScreenStack *>::Iterator it;
            for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
            {
                MythScreenType *top = (*it)->GetTopScreen();
                if (top)
                {
                    if (top->keyPressEvent(dynamic_cast<QKeyEvent*>(e)))
                        return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonPress:
        {
            if (!d->gesture.recording())
            {
                d->gesture.start();
                d->gesture.record(dynamic_cast<QMouseEvent*>(e)->pos());

                /* start a single shot timer */
                d->gestureTimer->start(GESTURE_TIMEOUT);

                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            if (d->gestureTimer->isActive())
                d->gestureTimer->stop();

            if (currentWidget())
                break;

            if (d->gesture.recording())
            {
                d->gesture.stop();
                ge = d->gesture.gesture();

                /* handle clicks separately */
                if (ge->gesture() == MythGestureEvent::Click)
                {            
                    MythUIType *clicked;
                    QValueVector<MythScreenStack *>::iterator it;
                    QPoint p = dynamic_cast<QMouseEvent*>(e)->pos();

                    delete ge;

                    for (it = d->stackList.begin(); it != d->stackList.end(); 
                         it++)
                    {
                        MythScreenType *screen = (*it)->GetTopScreen();
                        if (screen && (clicked = screen->GetChildAt(p)) != NULL)
                        {
                            clicked->gestureEvent(clicked, ge);
                            break;
                        }
                    }
                }
                else 
                    QApplication::postEvent(this, ge);

                return true;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            if (d->gesture.recording())
            {
                /* reset the timer */
                d->gestureTimer->stop();
                d->gestureTimer->start(GESTURE_TIMEOUT);

                d->gesture.record(dynamic_cast<QMouseEvent*>(e)->pos());
                return true;
            }
            break;
        }
        default:
            break;
    }

    return false;
}

void MythMainWindow::customEvent(QCustomEvent *ce)
{
    if (ce->type() == MythGestureEventType)
    {
        MythGestureEvent *ge = dynamic_cast<MythGestureEvent*>(ce);
        if (ge != NULL)
        {
            MythScreenStack *toplevel = GetMainStack();
            if (toplevel && !currentWidget())
            {
                MythScreenType *screen = toplevel->GetTopScreen();
                if (screen)
                    screen->gestureEvent(NULL, ge);
            }
            cout << "Gesture: " << QString(*ge) << endl;
        }
    }
    else if (ce->type() == kExitToMainMenuEventType && d->exitingtomain)
    {
        ExitToMainMenu();
    }
    else if (ce->type() == kExternalKeycodeEventType)
    {
        ExternalKeycodeEvent *eke = (ExternalKeycodeEvent *)ce;
        int keycode = eke->getKeycode();

        QKeyEvent key(QEvent::KeyPress, keycode, 0, Qt::NoButton);

        QObject *key_target = getTarget(key);
        if (!key_target)
            QApplication::sendEvent(this, &key);
        else
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
            /* FIXME, this needs rewritten */
            QWidget * activewidget = qApp->focusWidget();
            MythDialog * activedialog = NULL;
            bool iscatched = false;
            while (activewidget && !activedialog)
            {
                activedialog = dynamic_cast<MythDialog*>(activewidget);
                if (!activedialog)
                    activewidget = activewidget->parentWidget();
            }
            if (activedialog)
                iscatched = activedialog->onMediaEvent(pDev);

            MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
            if (iscatched || !mon->ValidateAndLock(pDev))
                mon = NULL;

            while (mon && (itr != d->mediaHandlerMap.end()))
            {
                if ((itr.data().MediaType & (int)pDev->getMediaType()))
                {
                    VERBOSE(VB_IMPORTANT, "Found a handler");
                    d->exitingtomain = true;
                    d->exitmenumediadevicecallback = itr.data().callback;
                    d->mediadeviceforcallback = pDev;
                    QApplication::postEvent(this, new ExitToMainMenuEvent());
                    break;
                }
                itr++;
            }
            if (mon)
                mon->Unlock(pDev);
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
            if (gContext->GetScreenIsAsleep())
                return;

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

            mod = ((mod & Qt::CTRL) ? Qt::ControlButton : 0) |
                  ((mod & Qt::META) ? Qt::MetaButton : 0) |
                  ((mod & Qt::ALT) ? Qt::AltButton : 0) |
                  ((mod & Qt::SHIFT) ? Qt::ShiftButton : 0);

            QKeyEvent key(lke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, ascii, mod, text);

            QObject *key_target = getTarget(key);
            if (!key_target)
                QApplication::sendEvent(this, &key);
            else
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
#ifdef USE_JOYSTICK_MENU
    else if (ce->type() == kJoystickKeycodeEventType && !d->ignore_joystick_keys)
    {
        JoystickKeycodeEvent *jke = (JoystickKeycodeEvent *)ce;
        int keycode = jke->getKeycode();

        if (keycode)
        {
            gContext->ResetScreensaver();
            if (gContext->GetScreenIsAsleep())
                return;

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

            QKeyEvent key(jke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, ascii, mod, text);

            QObject *key_target = getTarget(key);
            if (!key_target)
                QApplication::sendEvent(this, &key);
            else
                QApplication::sendEvent(key_target, &key);
        }
        else
        {
            cerr << "JoystickMenuClient warning: attempt to convert '"
                 << jke->getJoystickMenuText() << "' to a key sequence failed. Fix"
                                           " your key mappings.\n";
        }
    }
    else if (ce->type() == kJoystickMuteEventType)
    {
        JoystickMenuMuteEvent *jme = (JoystickMenuMuteEvent *)ce;
        d->ignore_joystick_keys = jme->eventsMuted();
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
}

QObject *MythMainWindow::getTarget(QKeyEvent &key)
{
    QObject *key_target = NULL;

    if (!currentWidget())
        return key_target;

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

int MythMainWindow::NormalizeFontSize(int pointSize)
{
    QPaintDeviceMetrics pdm(this);

    float desired = 100.0;
    pointSize = (int)(pointSize * desired / pdm.logicalDpiY());
    pointSize = (int)(pointSize * d->hmult);

    return pointSize;
}

QFont MythMainWindow::CreateFont(const QString &face, int pointSize, 
                                 int weight, bool italic)
{
    return QFont(face, NormalizeFontSize(pointSize), weight, italic);
}

QRect MythMainWindow::NormRect(const QRect &rect)
{
    QRect ret;
    ret.setWidth((int)(rect.width() * d->wmult));
    ret.setHeight((int)(rect.height() * d->hmult));
    ret.moveTopLeft(QPoint((int)(rect.x() * d->wmult),
                           (int)(rect.y() * d->hmult)));
    ret = ret.normalize();

    return ret;
}

QPoint MythMainWindow::NormPoint(const QPoint &point)
{
    QPoint ret;
    ret.setX((int)(point.x() * d->wmult));
    ret.setY((int)(point.y() * d->hmult));

    return ret;
}

QSize MythMainWindow::NormSize(const QSize &size)
{
    QSize ret;
    ret.setWidth((int)(size.width() * d->wmult));
    ret.setHeight((int)(size.height() * d->hmult));

    return ret;
}

int MythMainWindow::NormX(const int x)
{
    return (int)(x * d->wmult);
}

int MythMainWindow::NormY(const int y)
{
    return (int)(y * d->hmult);
}

QRect MythMainWindow::GetUIScreenRect(void)
{
    return QRect(0, 0, d->screenwidth, d->screenheight);
}

