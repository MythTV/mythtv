#include <qcursor.h>
#include <qdialog.h>
#include <qdir.h>
#include <qvbox.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qdir.h>
#include <qregexp.h>
#include <qaccel.h>
#include <qfocusdata.h>
#include <qdict.h>
#include <qsqldatabase.h>
#include <qobjectlist.h> 

#ifdef QWS
#include <qwindowsystem_qws.h>
#endif
#ifdef Q_WS_MACX
#import <HIToolbox/Menus.h>   // For HideMenuBar()
#endif

#include <iostream>
using namespace std;

#include <pthread.h>
#ifdef USE_LIRC
#include "lirc.h"
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
#endif

#include "uitypes.h"
#include "uilistbtntype.h"
#include "xmlparse.h"
#include "mythdialogs.h"
#include "lcddevice.h"
#include "mythmediamonitor.h"
#include "screensaver.h"
#include "mythdbcon.h"

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
    int xbase, ybase;
    bool does_fill_screen;

    vector<QWidget *> widgetList;

#ifdef USE_JOYSTICK_MENU
    bool ignore_joystick_keys;
#endif

#ifdef USE_LIRC
    bool ignore_lirc_keys;
#endif

    bool exitingtomain;

    QDict<KeyContext> keyContexts;
    QMap<int, JumpData*> jumpMap;
    QMap<QString, JumpData> destinationMap;
    QMap<QString, MHData> mediaHandlerMap;
    QMap<QString, MPData> mediaPluginMap;

    void (*exitmenucallback)(void);

    void (*exitmenumediadevicecallback)(MythMediaDevice* mediadevice);
    MythMediaDevice * mediadeviceforcallback;

    int escapekey;
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

MythMainWindow::MythMainWindow(QWidget *parent, const char *name, bool modal, 
                               WFlags flags)
              : QDialog(parent, name, modal, flags)
{
    d = new MythMainWindowPrivate;

    Init();
    d->exitingtomain = false;
    d->exitmenucallback = false;
    d->exitmenumediadevicecallback = false;
    d->mediadeviceforcallback = NULL;
    d->escapekey = Key_Escape;

#ifdef USE_LIRC
    d->ignore_lirc_keys = false;
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
}

MythMainWindow::~MythMainWindow()
{
    delete d;
}

void MythMainWindow::closeEvent(QCloseEvent *e)
{
    (void)e;
    qApp->quit();
}

void MythMainWindow::Init(void)
{
    gContext->GetScreenSettings(d->xbase, d->screenwidth, d->wmult,
                                d->ybase, d->screenheight, d->hmult);
    setGeometry(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    setFixedSize(QSize(d->screenwidth, d->screenheight));

    setFont(gContext->GetMediumFont());

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

    reparent(parentWidget(), flags, pos());

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

void MythMainWindow::ExitToMainMenu(void)
{
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


bool MythMainWindow::HandleMedia(QString& handler, const QString &mrl, const QString& plot,
                                 const QString& title, const QString& director, int lenMins, const QString& year)
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

void MythMainWindow::keyPressEvent(QKeyEvent *e)
{
    e->accept();
    QWidget *current = currentWidget();
    if (current && current->isEnabled())
        qApp->notify(current, e);
    else
        QDialog::keyPressEvent(e);
}

void MythMainWindow::customEvent(QCustomEvent *ce)
{
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
            if (!iscatched)
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

            mod = ((mod & Qt::CTRL) ? Qt::ControlButton : 0) |
                  ((mod & Qt::META) ? Qt::MetaButton : 0) |
                  ((mod & Qt::ALT) ? Qt::AltButton : 0) |
                  ((mod & Qt::SHIFT) ? Qt::ShiftButton : 0);

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
#ifdef USE_JOYSTICK_MENU
    else if (ce->type() == kJoystickKeycodeEventType && !d->ignore_joystick_keys) 
    {
        JoystickKeycodeEvent *jke = (JoystickKeycodeEvent *)ce;
        int keycode = jke->getKeycode();

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

            QKeyEvent key(jke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, ascii, mod, text);

            QObject *key_target = getTarget(key);

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

MythDialog::MythDialog(MythMainWindow *parent, const char *name, bool setsize)
          : QFrame(parent, name)
{
    rescode = 0;

    if (!parent)
    {
        cerr << "Trying to create a dialog without a parent.\n";
        return;
    }

    in_loop = false;

    gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                ybase, screenheight, hmult);

    defaultBigFont = gContext->GetBigFont();
    defaultMediumFont = gContext->GetMediumFont();
    defaultSmallFont = gContext->GetSmallFont();

    setFont(defaultMediumFont);

    if (setsize)
    {
        move(0, 0);
        setFixedSize(QSize(screenwidth, screenheight));
        gContext->ThemeWidget(this);
    }

    parent->attach(this);
    m_parent = parent;
}

MythDialog::~MythDialog()
{
    m_parent->detach(this);
}

void MythDialog::setNoErase(void)
{
    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
#ifdef QWS
    flags |= WResizeNoErase;
#endif
    setWFlags(flags);
}

bool MythDialog::onMediaEvent(MythMediaDevice*)
{
    return false;
}



void MythDialog::Show(void)
{
    show();
}

void MythDialog::done(int r)
{
    hide();
    setResult(r);
    close();
}

void MythDialog::accept()
{
    done(Accepted);
}

void MythDialog::reject()
{
    done(Rejected);
}

int MythDialog::exec()
{
    if (in_loop) 
    {
        qWarning("MythDialog::exec: Recursive call detected.");
        return -1;
    }

    setResult(0);

    Show();

    in_loop = TRUE;
    qApp->enter_loop();

    int res = result();

    return res;
}

void MythDialog::hide(void)
{
    if (isHidden())
        return;

    // Reimplemented to exit a modal when the dialog is hidden.
    QWidget::hide();
    if (in_loop)  
    {
        in_loop = FALSE;
        qApp->exit_loop();
    }
}

void MythDialog::keyPressEvent( QKeyEvent *e )
{
    if (e->state() != 0)
        return;

    bool handled = false;
    QStringList actions;

    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
                reject();
            else if (action == "UP" || action == "LEFT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus))
                {
                }
                else
                    focusNextPrevChild(false);
            }
            else if (action == "DOWN" || action == "RIGHT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus)) 
                {
                }
                else
                    focusNextPrevChild(true);
            }
            else if (action == "MENU")
                emit menuButtonPressed();
            else
                handled = false;
        }
    }
}

MythPopupBox::MythPopupBox(MythMainWindow *parent, const char *name)
            : MythDialog(parent, name, false)
{
    float wmult, hmult;

    if (gContext->GetNumSetting("UseArrowAccels", 1))
        arrowAccel = true;
    else
        arrowAccel = false;

    gContext->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setPalette(parent->palette());
    popupForegroundColor = foregroundColor ();
    setFont(parent->font());

    hpadding = gContext->GetNumSetting("PopupHeightPadding", 120);
    wpadding = gContext->GetNumSetting("PopupWidthPadding", 80);

    vbox = new QVBoxLayout(this, (int)(10 * hmult));
}

MythPopupBox::MythPopupBox(MythMainWindow *parent, bool graphicPopup,
                           QColor popupForeground, QColor popupBackground,
                           QColor popupHighlight, const char *name)
            : MythDialog(parent, name, false)
{
    float wmult, hmult;

    if (gContext->GetNumSetting("UseArrowAccels", 1))
        arrowAccel = true;
    else
        arrowAccel = false;

    gContext->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setPalette(parent->palette());
    setFont(parent->font());

    hpadding = gContext->GetNumSetting("PopupHeightPadding", 120);
    wpadding = gContext->GetNumSetting("PopupWidthPadding", 80);

    vbox = new QVBoxLayout(this, (int)(10 * hmult));

    if (!graphicPopup)
        setPaletteBackgroundColor(popupBackground);
    else
        gContext->ThemeWidget(this);
    setPaletteForegroundColor(popupHighlight);

    popupForegroundColor = popupForeground;
}

bool MythPopupBox::focusNextPrevChild(bool next)
{
    QFocusData *focusList = focusData();
    QObjectList *objList = queryList(NULL,NULL,false,true);

    QWidget *startingPoint = focusList->home();
    QWidget *candidate = NULL;

    QWidget *w = (next) ? focusList->prev() : focusList->next();

    int countdown = focusList->count();

    do
    {
        if (w && w != startingPoint && !w->focusProxy() && 
            w->isVisibleTo(this) && w->isEnabled() &&
            (objList->find((QObject *)w) != -1))
        {
            candidate = w;
        }

        w = (next) ? focusList->prev() : focusList->next();
    }   
    while (w && !(candidate && w == startingPoint) && (countdown-- > 0));

    if (!candidate)
        return false;

    candidate->setFocus();
    return true;
}

void MythPopupBox::addWidget(QWidget *widget, bool setAppearance)
{
    if (setAppearance == true)
    {
         widget->setPalette(palette());
         widget->setFont(font());
    }

    if (widget->isA("QLabel"))
    {
        widget->setBackgroundOrigin(ParentOrigin);
        widget->setPaletteForegroundColor(popupForegroundColor);
    }

    vbox->addWidget(widget);
}

QLabel *MythPopupBox::addLabel(QString caption, LabelSize size, bool wrap)
{
    QLabel *label = new QLabel(caption, this);
    switch (size)
    {
        case Large: label->setFont(defaultBigFont); break;
        case Medium: label->setFont(defaultMediumFont); break;
        case Small: label->setFont(defaultSmallFont); break;
    }
    
    label->setMaximumWidth((int)m_parent->width() / 2);
    if (wrap)
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    
    addWidget(label, false);
    return label;
}

QButton *MythPopupBox::addButton(QString caption, QObject *target, 
                                 const char *slot)
{
    if (!target)
    {
        target = this;
        slot = SLOT(defaultButtonPressedHandler());
    }

    MythPushButton *button = new MythPushButton(caption, this, arrowAccel);
    m_parent->connect(button, SIGNAL(pressed()), target, slot);
    addWidget(button, false);
    return button;
}

void MythPopupBox::addLayout(QLayout *layout, int stretch)
{
    vbox->addLayout(layout, stretch);
}

void MythPopupBox::ShowPopup(QObject *target, const char *slot)
{
    ShowPopupAtXY(-1, -1, target, slot);
}

void MythPopupBox::ShowPopupAtXY(int destx, int desty, 
                                 QObject *target, const char *slot)
{
    const QObjectList *objlist = children();
    QObjectListIt it(*objlist);
    QObject *objs;

    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            widget->adjustSize();
        }
    }

    polish();

    int x = 0, y = 0, maxw = 0, poph = 0;

    it = QObjectListIt(*objlist);
    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QString objname = objs->name();
            if (objname != "nopopsize")
            {
                // little extra padding for these guys
                if (objs->isA("MythListBox"))
                    poph += (int)(25 * hmult);

                QWidget *widget = (QWidget *)objs;
                poph += widget->height();
                if (widget->width() > maxw)
                    maxw = widget->width();
            }
        }
    }

    poph += (int)(hpadding * hmult);
    setMinimumHeight(poph);

    maxw += (int)(wpadding * wmult);

    int width = (int)(800 * wmult);
    int height = (int)(600 * hmult);

    if (parentWidget())
    {
        width = parentWidget()->width();
        height = parentWidget()->height();
    }

    if (destx == -1)
        x = (int)(width / 2) - (int)(maxw / 2);
    else
        x = destx;

    if (desty == -1)
        y = (int)(height / 2) - (int)(poph / 2);
    else
        y = desty;

    if (poph + y > height)
        y = height - poph - (int)(8 * hmult);

    setFixedSize(maxw, poph);
    setGeometry(x, y, maxw, poph);

    if (target && slot)
        connect(this, SIGNAL(popupDone()), target, slot);

    Show();
}

void MythPopupBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions);
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];

        if ((action == "ESCAPE") || (arrowAccel && action == "LEFT"))
        {
            emit popupDone();
            handled = true;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

int MythPopupBox::ExecPopup(QObject *target, const char *slot)
{
    if (!target)
        ShowPopup(this, SLOT(defaultExitHandler()));
    else
        ShowPopup(target, slot);

    return exec();
}

int MythPopupBox::ExecPopupAtXY(int destx, int desty,
                            QObject *target, const char *slot)
{
    if (!target)
        ShowPopupAtXY(destx, desty, this, SLOT(defaultExitHandler()));
    else
        ShowPopupAtXY(destx, desty, target, slot);

    return exec();
}

void MythPopupBox::defaultButtonPressedHandler(void)
{
    const QObjectList *objlist = children();
    QObjectListIt it(*objlist);
    QObject *objs;
    int i = 0;
    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            if (widget->isA("MythPushButton"))
            {
                if (widget->hasFocus())
                    break;
                i++;
            }
        }
    }
    done(i);
}

void MythPopupBox::defaultExitHandler()
{
    done(-1);
}

void MythPopupBox::showOkPopup(MythMainWindow *parent, QString title,
                               QString message)
{
    MythPopupBox popup(parent, title);
    popup.addLabel(message, Medium, true);
    QButton *okButton = popup.addButton(tr("OK"));
    okButton->setFocus();
    popup.ExecPopup();
}

bool MythPopupBox::showOkCancelPopup(MythMainWindow *parent, QString title,
                                     QString message, bool focusOk)
{
    MythPopupBox popup(parent, title);
    popup.addLabel(message, Medium, true);
    QButton *okButton = popup.addButton(tr("OK"));
    QButton *cancelButton = popup.addButton(tr("Cancel"));
    if (focusOk)
        okButton->setFocus();
    else
        cancelButton->setFocus();

    return (popup.ExecPopup() == 0);
}

bool MythPopupBox::showGetTextPopup(MythMainWindow *parent, QString title,
                                    QString message, QString& text)
{
    MythPopupBox popup(parent, title);
    popup.addLabel(message, Medium, true);
    
    MythRemoteLineEdit* textEdit = new MythRemoteLineEdit(&popup, "chooseEdit");
    textEdit->setText(text);
    popup.addWidget(textEdit);
    
    popup.addButton(tr("OK"));
    popup.addButton(tr("Cancel"));
    
    textEdit->setFocus();
    
    if (popup.ExecPopup() == 0)
    {
        text = textEdit->text();
        return true;
    }
    
    return false;
}


int MythPopupBox::show2ButtonPopup(MythMainWindow *parent, QString title,
                                   QString message, QString button1msg,
                                   QString button2msg, int defvalue)
{
    MythPopupBox popup(parent, title);

    popup.addLabel(message, Medium, true);
    popup.addLabel("");

    QButton *but1 = popup.addButton(button1msg);
    QButton *but2 = popup.addButton(button2msg);

    if (defvalue == 1)
        but1->setFocus();
    else
        but2->setFocus();

    return popup.ExecPopup();
}

int MythPopupBox::showButtonPopup(MythMainWindow *parent, QString title,
                                  QString message, QStringList buttonmsgs,
                                  int defvalue)
{
    MythPopupBox popup(parent, title);

    popup.addLabel(message, Medium, true);
    popup.addLabel("");

    for (unsigned int i = 0; i < buttonmsgs.size(); i++ )
    {
        QButton *but = popup.addButton(buttonmsgs[i]);
        if (defvalue == (int)i)
            but->setFocus();
    }

    return popup.ExecPopup();
}

MythProgressDialog::MythProgressDialog(const QString &message, int totalSteps)
                  : MythDialog(gContext->GetMainWindow(), "progress", false)
{
    int screenwidth, screenheight;
    float wmult, hmult;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setFont(gContext->GetMediumFont());

    gContext->ThemeWidget(this);

    int yoff = screenheight / 3;
    int xoff = screenwidth / 10;
    setGeometry(xoff, yoff, screenwidth - xoff * 2, yoff);
    setFixedSize(QSize(screenwidth - xoff * 2, yoff));

    QVBoxLayout *lay = new QVBoxLayout(this, 0);

    QVBox *vbox = new QVBox(this);
    lay->addWidget(vbox);

    vbox->setLineWidth(3);
    vbox->setMidLineWidth(3);
    vbox->setFrameShape(QFrame::Panel);
    vbox->setFrameShadow(QFrame::Raised);
    vbox->setMargin((int)(15 * wmult));

    QLabel *msglabel = new QLabel(vbox);
    msglabel->setBackgroundOrigin(ParentOrigin);
    msglabel->setText(message);

    progress = new QProgressBar(totalSteps, vbox);
    progress->setBackgroundOrigin(ParentOrigin);
    progress->setProgress(0);

    setTotalSteps(totalSteps);

    if (class LCD * lcddev = LCD::Get())
    {
        textItems = new QPtrList<LCDTextItem>;
        textItems->setAutoDelete(true);

        textItems->clear();
        textItems->append(new LCDTextItem(1, ALIGN_CENTERED, message, "Generic",
                          false));
        lcddev->switchToGeneric(textItems);
    }
    else 
        textItems = NULL;

    show();

    qApp->processEvents();
}

void MythProgressDialog::Close(void)
{
    accept();

    if (textItems)
    {
        LCD * lcddev = LCD::Get();
        lcddev->switchToNothing();
        lcddev->switchToTime();
        delete textItems;
    }
}

void MythProgressDialog::setProgress(int curprogress)
{
    progress->setProgress(curprogress);
    if (curprogress % steps == 0)
    {
        qApp->processEvents();
        if (LCD * lcddev = LCD::Get())
        {
            float fProgress = (float)curprogress / m_totalSteps;
            lcddev->setGenericProgress(fProgress);
        }
    }
}

void MythProgressDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
                handled = true;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythProgressDialog::setTotalSteps(int totalSteps)
{
    m_totalSteps = totalSteps;
    steps = totalSteps / 1000;
    if (steps == 0)
        steps = 1;
}

MythBusyDialog::MythBusyDialog(const QString &title)
    : MythProgressDialog(title, 0), timer(NULL)
{
}

MythBusyDialog::~MythBusyDialog()
{
    if (timer)
    {
        timer->disconnect();
        timer->deleteLater();
        timer = NULL;
    }
}

void MythBusyDialog::start(int interval)
{
    if (!timer)
        timer = new QTimer (this);

    connect(timer, SIGNAL(timeout()),
            this,  SLOT  (timeout()));

    timer->start(interval);
}

void MythBusyDialog::Close(void)
{
    if (timer)
    {
        timer->disconnect();
        timer->deleteLater();
        timer = NULL;
    }

    MythProgressDialog::Close();
}

void MythBusyDialog::setProgress(void)
{
    progress->setProgress(progress->progress () + 10);
    qApp->processEvents();
    if (LCD *lcddev = LCD::Get())
        lcddev->setGenericBusy();
}

void MythBusyDialog::timeout(void)
{
    setProgress();
}

MythThemedDialog::MythThemedDialog(MythMainWindow *parent, QString window_name,
                                   QString theme_filename, const char* name,
                                   bool setsize)
                : MythDialog(parent, name, setsize)
{
    setNoErase();

    theme = NULL;

    if (!loadThemedWindow(window_name, theme_filename))
    {
        QString msg = 
            QString(tr("Could not locate '%1' in theme '%2'."
                       "\n\nReturning to the previous menu."))
            .arg(window_name).arg(theme_filename);
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("Missing UI Element"), msg);
        done(-1);
        return;
    }
}

MythThemedDialog::MythThemedDialog(MythMainWindow *parent, const char* name,
                                   bool setsize)
                : MythDialog(parent, name, setsize)
{
    setNoErase();
    theme = NULL;
}

bool MythThemedDialog::loadThemedWindow(QString window_name, QString theme_filename)
{

    if (theme)
        delete theme;

    context = -1;
    my_containers.clear();
    widget_with_current_focus = NULL;

    redrawRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, window_name, theme_filename))
    {
        return false;
    }

    loadWindow(xmldata);

    //
    //  Auto-connect signals we know about
    //

    //  Loop over containers
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;
    while ((looper = an_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            connect(type, SIGNAL(requestUpdate()), this, 
                    SLOT(updateForeground()));
            connect(type, SIGNAL(requestUpdate(const QRect &)), this, 
                    SLOT(updateForeground(const QRect &)));
            connect(type, SIGNAL(requestRegionUpdate(const QRect &)), this,
                    SLOT(updateForegroundRegion(const QRect &)));
        }
        ++an_it;
    }

    buildFocusList();

    updateBackground();
    initForeground();

    return true;
}

bool MythThemedDialog::buildFocusList()
{
    //
    //  Build a list of widgets that will take focus
    //

    focus_taking_widgets.clear();


    //  Loop over containers
    LayerSet *looper;
    QPtrListIterator<LayerSet> another_it(my_containers);
    while ((looper = another_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            if (type->canTakeFocus() && !type->isHidden())
            {
                if (context == -1 || type->GetContext() == -1 || 
                        context == type->GetContext())
                    focus_taking_widgets.append(type);
            }
        }
        ++another_it;
    }
    if (focus_taking_widgets.count() > 0)
    {
        return true;
    }
    return false;
}

MythThemedDialog::~MythThemedDialog()
{
    if (theme)
        delete theme;
}

void MythThemedDialog::loadWindow(QDomElement &element)
{
    //
    //  Parse all the child elements in the theme
    //

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("MythThemedDialog::loadWindow(): Do not "
                                "understand DOM Element: '%1'. Ignoring.")
                        .arg(e.tagName()));
            }
        }
    }
}

void MythThemedDialog::parseContainer(QDomElement &element)
{
    //
    //  Have the them object parse the containers
    //  but hold a pointer to each of them so
    //  that we can iterate over them later
    //

    QRect area;
    QString name;
    int a_context;
    theme->parseContainer(element, name, a_context, area);
    if (name.length() < 1)
    {
        VERBOSE(VB_IMPORTANT, "Failed to parse a container. Ignoring.");
        return;
    }

    LayerSet *container_reference = theme->GetSet(name);
    my_containers.append(container_reference);
}

void MythThemedDialog::parseFont(QDomElement &element)
{
    //
    //  this is just here so you can re-implement the virtual
    //  function and do something special if you like
    //

    theme->parseFont(element);
}

void MythThemedDialog::parsePopup(QDomElement &element)
{
    //
    //  theme doesn't know how to do this yet
    //
    element = element;
    cerr << "I don't know how to handle popops yet (I'm going to try and "
            "just ignore it)\n";
}

void MythThemedDialog::initForeground()
{
    my_foreground = my_background;
    updateForeground();
}

void MythThemedDialog::updateBackground()
{
    //
    //  Draw the background pixmap once
    //

    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    //
    //  Ask the container holding anything
    //  to do with the background to draw
    //  itself on a pixmap
    //

    LayerSet *container = theme->GetSet("background");

    //
    //  *IFF* there is a background, draw it
    //
    if (container)
    {
        container->Draw(&tmp, 0, context);
        tmp.end();
    }

    //
    //  Copy that pixmap to the permanent one
    //  and tell Qt about it
    //

    my_background = bground;
    setPaletteBackgroundPixmap(my_background);
}

void MythThemedDialog::updateForeground()
{
    QRect r = this->geometry();
    updateForeground(r);
}

void MythThemedDialog::updateForeground(const QRect &r)
{
    QRect rect_to_update = r;
    if (r.width() == 0 || r.height() == 0)
    {
        cerr << "MythThemedDialog.o: something is requesting a screen update of zero size. "
             << "A widget probably has not done a calculateScreeArea(). Will redraw "
             << "the whole screen (inefficient!)." 
             << endl;
             
        rect_to_update = this->geometry();
    }

    redrawRect = redrawRect.unite(r);

    update(redrawRect);
}

void MythThemedDialog::ReallyUpdateForeground(const QRect &r)
{
    QRect rect_to_update = r;
    if (r.width() == 0 || r.height() == 0)
    {
        cerr << "MythThemedDialog.o: something is requesting a screen update of zero size. "
             << "A widget probably has not done a calculateScreeArea(). Will redraw "
             << "the whole screen (inefficient!)."
             << endl;

        rect_to_update = this->geometry();
    }

    //
    //  We paint offscreen onto a pixmap
    //  and then BitBlt it over
    //

    QPainter whole_dialog_painter(&my_foreground);

    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while ((looper = an_it.current()) != 0)
    {
        QRect container_area = looper->GetAreaRect();

        //
        //  Only paint if the container's area is valid
        //  and it intersects with whatever Qt told us
        //  needed to be repainted
        //

        if (container_area.isValid() &&
            rect_to_update.intersects(container_area) &&
            looper->GetName().lower() != "background")
        {
            //
            //  Debugging
            //
            /*
            cout << "A container called \"" << looper->GetName() 
                 << "\" said its area is "
                 << container_area.left()
                 << ","
                 << container_area.top()
                 << " to "
                 << container_area.left() + container_area.width()
                 << ","
                 << container_area.top() + container_area.height()
                 << endl;
            */

            QPixmap container_picture(container_area.size());
            QPainter offscreen_painter(&container_picture);
            offscreen_painter.drawPixmap(0, 0, my_background, 
                                         container_area.left(), 
                                         container_area.top());

            //
            //  Loop over the draworder layers

            for (int i = 0; i <= looper->getLayers(); i++)
            {
                looper->Draw(&offscreen_painter, i, context);
            }

            //
            //  If it did in fact paint something (empty
            //  container?) then tell it we're done
            //
            if (offscreen_painter.isActive())
            {
                offscreen_painter.end();
                whole_dialog_painter.drawPixmap(container_area.topLeft(), 
                                                container_picture);
            }

        }
        ++an_it;
    }

    if (whole_dialog_painter.isActive())
    {
        whole_dialog_painter.end();
    }

    redrawRect = QRect(0, 0, 0, 0);
}

void MythThemedDialog::updateForegroundRegion(const QRect &r)
{
    QRect area = r;
    QPainter whole_dialog_painter(&my_foreground);

    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while ((looper = an_it.current()) != 0)
    {
        QRect container_area = looper->GetAreaRect();

        if (container_area.isValid() &&
            r.intersects(container_area) &&
            looper->GetName().lower() != "background")
        {
            QPixmap container_picture(r.size());
            QPainter offscreen_painter(&container_picture);
            offscreen_painter.drawPixmap(0, 0, my_background,
                                         r.left(), r.top());

            for (int i = 0; i <= looper->getLayers(); i++)
            {
                looper->DrawRegion(&offscreen_painter, area, i, context);
            }

            if (offscreen_painter.isActive())
            {
                offscreen_painter.end();
                whole_dialog_painter.drawPixmap(r.topLeft(),
                                                container_picture);
            }

        }
        ++an_it;
    }

    if (whole_dialog_painter.isActive())
        whole_dialog_painter.end();

    update(r);
}

void MythThemedDialog::paintEvent(QPaintEvent *e)
{
    if (redrawRect.width() > 0 && redrawRect.height() > 0)
        ReallyUpdateForeground(redrawRect);

    bitBlt(this, e->rect().left(), e->rect().top(),
           &my_foreground, e->rect().left(), e->rect().top(),
           e->rect().width(), e->rect().height());

    MythDialog::paintEvent(e);
}

bool MythThemedDialog::assignFirstFocus()
{
    if (widget_with_current_focus)
    {
        widget_with_current_focus->looseFocus();
    }

    QPtrListIterator<UIType> an_it(focus_taking_widgets);
    UIType *looper;

    while ((looper = an_it.current()) != 0)
    {
        if (looper->canTakeFocus())
        {
            widget_with_current_focus = looper;
            widget_with_current_focus->takeFocus();
            return true;
        }
        ++an_it;
    }

    return false;
}

bool MythThemedDialog::nextPrevWidgetFocus(bool up_or_down)
{
    if (up_or_down)
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        UIType *looper;

        while ((looper = an_it.current()) != 0)
        {
            if (reached_current && looper->canTakeFocus())
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;
            }

            if (looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            ++an_it;
        }

        if (assignFirstFocus())
        {
            return true;
        }
        return false;
    }
    else
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        an_it.toLast();
        UIType *looper;

        while ((looper = an_it.current()) != 0)
        {
            if (reached_current && looper->canTakeFocus())
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;
            }

            if (looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            --an_it;
        }

        if (reached_current)
        {
            an_it.toLast();
            while ((looper = an_it.current()) != 0)
            {
                if (looper->canTakeFocus())
                {
                    widget_with_current_focus->looseFocus();
                    widget_with_current_focus = looper;
                    widget_with_current_focus->takeFocus();
                    return true;
                }
                --an_it;
            }
        }
        return false;
    }
    return false;
}

void MythThemedDialog::activateCurrent()
{
    if (widget_with_current_focus)
    {
        widget_with_current_focus->activate();
    }
    else
    {
        cerr << "dialogbox.o: Something asked me activate the current widget, "
                "but there is no current widget\n";
    }
}

UIType* MythThemedDialog::getUIObject(const QString &name)
{
    //
    //  Try and find the UIType target referenced
    //  by "name".
    //
    //  At some point, it would be nice to speed
    //  this up with a map across all instantiated
    //  UIType objects "owned" by this dialog
    //

    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while ((looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
            return hunter;
        ++an_it;
    }

    return NULL;
}

UIType* MythThemedDialog::getCurrentFocusWidget()
{
    if (widget_with_current_focus)
    {
        return widget_with_current_focus;
    }
    return NULL;
}

UIManagedTreeListType* MythThemedDialog::getUIManagedTreeListType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIManagedTreeListType *hunted;
            if ( (hunted = dynamic_cast<UIManagedTreeListType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UITextType* MythThemedDialog::getUITextType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UITextType *hunted;
            if ( (hunted = dynamic_cast<UITextType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIRichTextType* MythThemedDialog::getUIRichTextType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIRichTextType *hunted;
            if ( (hunted = dynamic_cast<UIRichTextType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIRemoteEditType* MythThemedDialog::getUIRemoteEditType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIRemoteEditType *hunted;
            if ( (hunted = dynamic_cast<UIRemoteEditType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIMultiTextType* MythThemedDialog::getUIMultiTextType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIMultiTextType *hunted;
            if ( (hunted = dynamic_cast<UIMultiTextType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIPushButtonType* MythThemedDialog::getUIPushButtonType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIPushButtonType *hunted;
            if ( (hunted = dynamic_cast<UIPushButtonType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UITextButtonType* MythThemedDialog::getUITextButtonType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UITextButtonType *hunted;
            if ( (hunted = dynamic_cast<UITextButtonType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIRepeatedImageType* MythThemedDialog::getUIRepeatedImageType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIRepeatedImageType *hunted;
            if ( (hunted = dynamic_cast<UIRepeatedImageType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UICheckBoxType* MythThemedDialog::getUICheckBoxType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UICheckBoxType *hunted;
            if ( (hunted = dynamic_cast<UICheckBoxType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UISelectorType* MythThemedDialog::getUISelectorType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UISelectorType *hunted;
            if ( (hunted = dynamic_cast<UISelectorType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIBlackHoleType* MythThemedDialog::getUIBlackHoleType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIBlackHoleType *hunted;
            if ( (hunted = dynamic_cast<UIBlackHoleType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIImageType* MythThemedDialog::getUIImageType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIImageType *hunted;
            if ( (hunted = dynamic_cast<UIImageType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIStatusBarType* MythThemedDialog::getUIStatusBarType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIStatusBarType *hunted;
            if ( (hunted = dynamic_cast<UIStatusBarType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIListBtnType* MythThemedDialog::getUIListBtnType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIListBtnType *hunted;
            if ((hunted = dynamic_cast<UIListBtnType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIListTreeType* MythThemedDialog::getUIListTreeType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIListTreeType *hunted;
            if ((hunted = dynamic_cast<UIListTreeType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIKeyboardType *MythThemedDialog::getUIKeyboardType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
        {
            UIKeyboardType *hunted;
            if ( (hunted = dynamic_cast<UIKeyboardType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

LayerSet* MythThemedDialog::getContainer(const QString& name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        if (looper->GetName() == name)
            return looper;
        
        ++an_it;
    }
    
    return NULL;
}

fontProp* MythThemedDialog::getFont(const QString &name)
{
    fontProp* font = NULL;
    if (theme)
        font = theme->GetFont(name, true);

    return font;
}

/*
---------------------------------------------------------------------
*/

MythPasswordDialog::MythPasswordDialog(QString message,
                                       bool *success,
                                       QString target,
                                       MythMainWindow *parent, 
                                       const char *name, 
                                       bool)
                   :MythDialog(parent, name, false)
{
    int textWidth =  fontMetrics().width(message);
    int totalWidth = textWidth + 175;

    success_flag = success;
    target_text = target;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    this->setGeometry((screenwidth - 250 ) / 2,
                      (screenheight - 50 ) / 2,
                      totalWidth,50);
    QFrame *outside_border = new QFrame(this);
    outside_border->setGeometry(0,0,totalWidth,50);
    outside_border->setFrameStyle(QFrame::Panel | QFrame::Raised );
    outside_border->setLineWidth(4);
    QLabel *message_label = new QLabel(message, this);
    message_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    message_label->setGeometry(15,10,textWidth,30);
    message_label->setBackgroundOrigin(ParentOrigin);
    password_editor = new MythLineEdit(this);
    password_editor->setEchoMode(QLineEdit::Password);
    password_editor->setGeometry(textWidth + 20,10,135,30);
    password_editor->setBackgroundOrigin(ParentOrigin);
    connect(password_editor, SIGNAL(textChanged(const QString &)),
            this, SLOT(checkPassword(const QString &)));

    this->setActiveWindow();
    password_editor->setFocus();
}

void MythPasswordDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                MythDialog::keyPressEvent(e);
            }
        }
    }
}


void MythPasswordDialog::checkPassword(const QString &the_text)
{
    if (the_text == target_text)
    {
        *success_flag = true;
        done(0);
    }
    else
    {
        //  Oh to beep 
    }
}

MythPasswordDialog::~MythPasswordDialog()
{
}

/*
---------------------------------------------------------------------
*/

MythSearchDialog::MythSearchDialog(MythMainWindow *parent, const char *name) 
                 :MythPopupBox(parent, name)
{
    // create the widgets
    caption = addLabel(QString(""));

    editor = new MythRemoteLineEdit(this);
    connect(editor, SIGNAL(textChanged()), this, SLOT(searchTextChanged()));
    addWidget(editor);
    editor->setFocus();
    editor->setPopupPosition(VK_POSBOTTOMDIALOG); 

    listbox = new MythListBox(this);
    listbox->setScrollBar(false);
    listbox->setBottomScrollBar(false);
    connect(listbox, SIGNAL(accepted(int)), this, SLOT(itemSelected(int)));
    addWidget(listbox);
    
    ok_button = addButton(tr("OK"), this, SLOT(okPressed()));
    cancel_button = addButton(tr("Cancel"), this, SLOT(cancelPressed()));
}

void MythSearchDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                done(-1);        
            }
            if (action == "LEFT")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            if (action == "RIGHT")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            if (action == "SELECT")
            {
                handled = true;
                done(0);
            }
        }
    }
    if (!handled)
        MythPopupBox::keyPressEvent(e);
}

void MythSearchDialog::itemSelected(int index)
{
    (void)index;
    done(0);
}

void MythSearchDialog::setCaption(QString text)
{
    caption->setText(text);
}

void MythSearchDialog::setSearchText(QString text)
{
    editor->setText(text);
    editor->setCursorPosition(0, editor->text().length());
}

void MythSearchDialog::searchTextChanged(void)
{
    listbox->setCurrentItem(editor->text(), false,  true);
    listbox->setTopItem(listbox->currentItem());
}

QString MythSearchDialog::getResult(void)
{
    return listbox->currentText();
}

void MythSearchDialog::setItems(QStringList items)
{
   listbox->insertStringList(items);
   searchTextChanged();
}

void MythSearchDialog::okPressed(void)
{
    done(0);  
}

void MythSearchDialog::cancelPressed(void)
{
    done(-1);
}

MythSearchDialog::~MythSearchDialog()
{
    if (listbox)
    {
        delete listbox;
        listbox = NULL;
    }    
    
    if (editor)
    {
        delete editor;
        editor = NULL;
    }    
}

/*
---------------------------------------------------------------------
 */

/** \fn MythImageFileDialog::MythImageFileDialog(QString*,QString,MythMainWindow*,QString,QString,const char*,bool)
 *  \brief Locates an image file dialog in the current theme.
 *
 *   MythImageFileDialog's expect there to be certain
 *   elements in the theme, or they will fail.
 *
 *   For example, we use the size of the background
 *   pixmap to define the geometry of the dialog. If
 *   the pixmap ain't there, we need to fail.
 */
MythImageFileDialog::MythImageFileDialog(QString *result,
                                         QString top_directory,
                                         MythMainWindow *parent, 
                                         QString window_name,
                                         QString theme_filename,
                                         const char *name, 
                                         bool setsize)
                   :MythThemedDialog(parent, 
                                     window_name, 
                                     theme_filename,
                                     name,
                                     setsize)
{
    selected_file = result;
    initial_node = NULL;
    
    UIImageType *file_browser_background = getUIImageType("file_browser_background");
    if (file_browser_background)
    {
        QPixmap background = file_browser_background->GetImage();
        
        this->setFixedSize(QSize(background.width(), background.height()));
        this->move((screenwidth - background.width()) / 2,
                   (screenheight - background.height()) / 2);
    }
    else
    {
        QString msg = 
            QString(tr("The theme you are using is missing the "
                       "'file_browser_background' "
                       "element. \n\nReturning to the previous menu."));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("Missing UI Element"), msg);
        done(-1);
        return;
    }

    //
    //  Make a nice border
    //

    this->setFrameStyle(QFrame::Panel | QFrame::Raised );
    this->setLineWidth(4);


    //
    //  Find the managed tree list that handles browsing.
    //  Fail if we can't find it.
    //

    file_browser = getUIManagedTreeListType("file_browser");
    if (file_browser)
    {
        file_browser->calculateScreenArea();
        file_browser->showWholeTree(true);
        connect(file_browser, SIGNAL(nodeSelected(int, IntVector*)),
                this, SLOT(handleTreeListSelection(int, IntVector*)));
        connect(file_browser, SIGNAL(nodeEntered(int, IntVector*)),
                this, SLOT(handleTreeListEntered(int, IntVector*)));
    }
    else
    { 
        QString msg = 
            QString(tr("The theme you are using is missing the "
                       "'file_browser' element. "
                       "\n\nReturning to the previous menu."));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("Missing UI Element"), msg);
        done(-1);
        return;
    }    
    
    //
    //  Find the UIImageType for previewing
    //  No image_box, no preview.
    //
    
    image_box = getUIImageType("image_box");
    if (image_box)
    {
        image_box->calculateScreenArea();
    }
    initialDir = top_directory;
        
    image_files.clear();
    buildTree(top_directory);

    file_browser->assignTreeData(root_parent);
    if (initial_node)
    {
        file_browser->setCurrentNode(initial_node);
    }
    file_browser->enter();
    file_browser->refresh();
    
}

void MythImageFileDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                file_browser->moveUp();
            else if (action == "DOWN")
                file_browser->moveDown();
            else if (action == "LEFT")
                file_browser->popUp();
            else if (action == "RIGHT")
                file_browser->pushDown();
            else if (action == "PAGEUP")
                file_browser->pageUp();
            else if (action == "PAGEDOWN")
                file_browser->pageDown();
            else if (action == "SELECT")
                file_browser->select();
            else
                handled = false;
        }
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void MythImageFileDialog::buildTree(QString starting_where)
{
    buildFileList(starting_where);
    
    root_parent = new GenericTree("Image Files root", -1, false);
    file_root = root_parent->addNode("Image Files", -1, false);

    //
    //  Go through the files and build a tree
    //    
    for(uint i = 0; i < image_files.count(); ++i)
    {
        bool make_active = false;
        QString file_string = *(image_files.at(i));
        if (file_string == *selected_file)
        {
            make_active = true;
        }
        QString prefix = initialDir;

        if (prefix.length() < 1)
        {
            cerr << "mythdialogs.o: Seems unlikely that this is going to work" << endl;
        }
        file_string.remove(0, prefix.length());
        QStringList list(QStringList::split("/", file_string));
        GenericTree *where_to_add;
        where_to_add = file_root;
        int a_counter = 0;
        QStringList::Iterator an_it = list.begin();
        for( ; an_it != list.end(); ++an_it)
        {
            if (a_counter + 1 >= (int) list.count())
            {
                QString title = (*an_it);
                GenericTree *added_node = where_to_add->addNode(title.section(".",0,0), i, true);
                if (make_active)
                {
                    initial_node = added_node;
                }
            }
            else
            {
                QString dirname = *an_it + "/";
                GenericTree *sub_node;
                sub_node = where_to_add->getChildByName(dirname);
                if (!sub_node)
                {
                    sub_node = where_to_add->addNode(dirname, -1, false);
                }
                where_to_add = sub_node;
            }
            ++a_counter;
        }
    }
    if (file_root->childCount() < 1)
    {
        //
        //  Nothing survived the requirements
        //
        file_root->addNode("No files found", -1, false);
    }
}

void MythImageFileDialog::buildFileList(QString directory)
{
    QStringList imageExtensions = QImage::inputFormatList();

    // Expand the list Qt gives us, working off what we know was built into
    // Qt based on the list it gave us

    if (imageExtensions.contains("jpeg") || imageExtensions.contains("JPEG"))
        imageExtensions += "jpg";

    QDir d(directory);
       
    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();

    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;
    QRegExp r;
    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." ||
            fi->fileName() == ".." )
        {
            continue;
        }
            
        if (fi->isDir())
        {
            buildFileList(fi->absFilePath());
        }
        else
        {
            r.setPattern("^" + fi->extension() + "$");
            r.setCaseSensitive(false);
            QStringList result = imageExtensions.grep(r);
            if (!result.isEmpty())
            {
                image_files.append(fi->absFilePath());
            }
            else
            {
                r.setPattern("^" + fi->extension());
                r.setCaseSensitive(false);
                QStringList other_result = imageExtensions.grep(r);
                if (!result.isEmpty())
                {
                    image_files.append(fi->absFilePath());
                }
            }
        }
    }                                                                                                                                                                                                                                                               
}

void MythImageFileDialog::handleTreeListEntered(int type, IntVector*)
{
    if (image_box)
    {
        if (type > -1)
        {
            image_box->SetImage(image_files[type]);
        }
        else
        {
            image_box->SetImage("");
        }
        image_box->LoadImage();
    }
}

void MythImageFileDialog::handleTreeListSelection(int type, IntVector*)
{
    if (type > -1)
    {
        *selected_file = image_files[type];
        done(0);
    }   
}

MythImageFileDialog::~MythImageFileDialog()
{
    if (root_parent)
    {
        root_parent->deleteAllChildren();
        delete root_parent;
        root_parent = NULL;
    }
}

// -----------------------------------------------------------------------------

MythScrollDialog::MythScrollDialog(MythMainWindow *parent,
                                   MythScrollDialog::ScrollMode mode,
                                   const char *name)
    : QScrollView(parent, name)
{
    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, 
                "MythScrollDialog: Programmer error, trying to create "
                "a dialog without a parent.");
        done(-1);
        return;
    }

    m_parent     = parent;
    m_scrollMode = mode;
        
    m_resCode    = 0;
    m_inLoop     = false;
    
    gContext->GetScreenSettings(m_xbase, m_screenWidth, m_wmult,
                                m_ybase, m_screenHeight, m_hmult);

    m_defaultBigFont = gContext->GetBigFont();
    m_defaultMediumFont = gContext->GetMediumFont();
    m_defaultSmallFont = gContext->GetSmallFont();

    setFont(m_defaultMediumFont);
    setCursor(QCursor(Qt::ArrowCursor));
    
    setFrameShape(QFrame::NoFrame);
    setHScrollBarMode(QScrollView::AlwaysOff);
    setVScrollBarMode(QScrollView::AlwaysOff);
    setFixedSize(QSize(m_screenWidth, m_screenHeight));

    gContext->ThemeWidget(viewport());
    if (viewport()->paletteBackgroundPixmap())
        m_bgPixmap = new QPixmap(*(viewport()->paletteBackgroundPixmap()));
    else {
        m_bgPixmap = new QPixmap(m_screenWidth, m_screenHeight);
        m_bgPixmap->fill(viewport()->colorGroup().base());
    }
    viewport()->setBackgroundMode(Qt::NoBackground);

    m_upArrowPix = gContext->LoadScalePixmap("scrollarrow-up.png");
    m_dnArrowPix = gContext->LoadScalePixmap("scrollarrow-dn.png");
    m_ltArrowPix = gContext->LoadScalePixmap("scrollarrow-left.png");
    m_rtArrowPix = gContext->LoadScalePixmap("scrollarrow-right.png");

    int wmargin = (int)(20*m_wmult);
    int hmargin = (int)(20*m_hmult);
    
    if (m_upArrowPix)
        m_upArrowRect = QRect(m_screenWidth - m_upArrowPix->width() - wmargin,
                              hmargin,
                              m_upArrowPix->width(), m_upArrowPix->height());
    if (m_dnArrowPix)
        m_dnArrowRect = QRect(m_screenWidth - m_dnArrowPix->width() - wmargin,
                              m_screenHeight - m_dnArrowPix->height() - hmargin,
                              m_dnArrowPix->width(), m_dnArrowPix->height());
    if (m_rtArrowPix)
        m_rtArrowRect = QRect(m_screenWidth - m_rtArrowPix->width() - wmargin,
                              m_screenHeight - m_rtArrowPix->height() - hmargin,
                              m_rtArrowPix->width(), m_rtArrowPix->height());
    if (m_ltArrowPix)
        m_ltArrowRect = QRect(wmargin,
                              m_screenHeight - m_ltArrowPix->height() - hmargin,
                              m_ltArrowPix->width(), m_ltArrowPix->height());
    
    m_showUpArrow  = true;
    m_showDnArrow  = true;
    m_showRtArrow  = false;
    m_showLtArrow  = false;
    
    m_parent->attach(this);
}

MythScrollDialog::~MythScrollDialog()
{
    m_parent->detach(this);
    delete m_bgPixmap;

    if (m_upArrowPix)
        delete m_upArrowPix;
    if (m_dnArrowPix)
        delete m_dnArrowPix;
    if (m_ltArrowPix)
        delete m_ltArrowPix;
    if (m_rtArrowPix)
        delete m_rtArrowPix;
}

void MythScrollDialog::setArea(int w, int h)
{
    resizeContents(w, h);
}

void MythScrollDialog::setAreaMultiplied(int areaWTimes, int areaHTimes)
{
    if (areaWTimes < 1 || areaHTimes < 1) {
        VERBOSE(VB_IMPORTANT,
                QString("MythScrollDialog::setAreaMultiplied(%1,%2): "
                        "Warning, Invalid areaWTimes or areaHTimes. "
                        "Setting to 1.")
                .arg(areaWTimes).arg(areaHTimes));
        areaWTimes = areaHTimes = 1;
    }

    resizeContents(m_screenWidth*areaWTimes,
                   m_screenHeight*areaHTimes);
}

int MythScrollDialog::result() const
{
    return m_resCode;    
}

void MythScrollDialog::show()
{
    QScrollView::show();    
}

void MythScrollDialog::hide()
{
    if (isHidden())
        return;

    // Reimplemented to exit a modal when the dialog is hidden.
    QWidget::hide();
    if (m_inLoop)  
    {
        m_inLoop = false;
        qApp->exit_loop();
    }
}

int MythScrollDialog::exec()
{
    if (m_inLoop) 
    {
        std::cerr << "MythScrollDialog::exec: Recursive call detected."
                  << std::endl;
        return -1;
    }

    setResult(0);

    show();

    m_inLoop = true;
    qApp->enter_loop();

    int res = result();

    return res;
}

void MythScrollDialog::done(int r)
{
    hide();
    setResult(r);
    close();
}

void MythScrollDialog::accept()
{
    done(Accepted);
}

void MythScrollDialog::reject()
{
    done(Rejected);
}

void MythScrollDialog::setResult(int r)
{
    m_resCode = r;    
}

void MythScrollDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->state() != 0)
        return;

    bool handled = false;
    QStringList actions;

    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
                reject();
            else if (action == "UP" || action == "LEFT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus))
                {
                }
                else
                    focusNextPrevChild(false);
            }
            else if (action == "DOWN" || action == "RIGHT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus)) 
                {
                }
                else
                    focusNextPrevChild(true);
            }
            else
                handled = false;
        }
    }
}

void MythScrollDialog::viewportPaintEvent(QPaintEvent *pe)
{
    if (!pe)
        return;

    QRect   er(pe->rect());
    QRegion reg(er);
    
    paintEvent(reg, er.x()+contentsX(), er.y()+contentsY(),
               er.width(), er.height());

    if (m_scrollMode == HScroll) {
        if (m_ltArrowPix && m_showLtArrow) {
            QPixmap pix(m_ltArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_ltArrowRect.x(), m_ltArrowRect.y());
            bitBlt(&pix, 0, 0, m_ltArrowPix);
            bitBlt(viewport(), m_ltArrowRect.x(), m_ltArrowRect.y(), &pix);
            reg -= m_ltArrowRect;
        }
        if (m_rtArrowPix && m_showRtArrow) {
            QPixmap pix(m_rtArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_rtArrowRect.x(), m_rtArrowRect.y());
            bitBlt(&pix, 0, 0, m_rtArrowPix);
            bitBlt(viewport(), m_rtArrowRect.x(), m_rtArrowRect.y(), &pix);
            reg -= m_rtArrowRect;
        }
    }
    else {
        if (m_upArrowPix && m_showUpArrow) {
            QPixmap pix(m_upArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_upArrowRect.x(), m_upArrowRect.y());
            bitBlt(&pix, 0, 0, m_upArrowPix);
            bitBlt(viewport(), m_upArrowRect.x(), m_upArrowRect.y(), &pix);
            reg -= m_upArrowRect;
        }
        if (m_dnArrowPix && m_showDnArrow) {
            QPixmap pix(m_dnArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_dnArrowRect.x(), m_dnArrowRect.y());
            bitBlt(&pix, 0, 0, m_dnArrowPix);
            bitBlt(viewport(), m_dnArrowRect.x(), m_dnArrowRect.y(), &pix);
            reg -= m_dnArrowRect;
        }
    }

    QPainter p(viewport());
    p.setClipRegion(reg);
    p.drawPixmap(0, 0, *m_bgPixmap, 0, 0, viewport()->width(),
                 viewport()->height());
    p.end();    
}

void MythScrollDialog::paintEvent(QRegion&, int , int , int , int )
{
}

void MythScrollDialog::setContentsPos(int x, int y)
{
    viewport()->setUpdatesEnabled(false);
    QScrollView::setContentsPos(x,y);
    viewport()->setUpdatesEnabled(true);
    updateContents();
}
    
      
