#include <unistd.h>

#include <QCoreApplication>
#include <QRegExp>
#include <QStringList>
#include <QTextStream>
#include <QDir>
#include <QKeyEvent>
#include <QEvent>
#include <QMap>

#include "mythcorecontext.h"
#include "mythmiscutil.h"
#include "mythversion.h"
#include "networkcontrol.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "previewgenerator.h"
#include "compat.h"
#include "mythsystemevent.h"
#include "mythdirs.h"
#include "mythlogging.h"

// libmythui
#include "mythmainwindow.h"
#include "mythuihelper.h"

#define LOC QString("NetworkControl: ")
#define LOC_ERR QString("NetworkControl Error: ")

#define FE_SHORT_TO 2000
#define FE_LONG_TO  10000

static QEvent::Type kNetworkControlDataReadyEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type NetworkControlCloseEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

/** Is @p test an abbreviation of @p command ?
 * The @p test substring must be at least @p minchars long.
 * @param command the full command name
 * @param test the string to test against the command name
 * @param minchars the minimum length of test in order to declare a match
 * @return true if @p test is the initial substring of @p command
 */
static bool is_abbrev(QString const& command,
                      QString const& test, int minchars = 1)
{
    if (test.length() < minchars)
        return command.toLower() == test.toLower();
    else
        return test.toLower() == command.left(test.length()).toLower();
}

NetworkControl::NetworkControl() :
    ServerPool(), prompt("# "),
    gotAnswer(false), answer(""),
    clientLock(QMutex::Recursive),
    commandThread(new MThread("NetworkControl", this)),
    stopCommandThread(false)
{
    // Eventually this map should be in the jumppoints table
    jumpMap["channelpriorities"]     = "Channel Recording Priorities";
    jumpMap["livetv"]                = "Live TV";
    jumpMap["livetvinguide"]         = "Live TV In Guide";
    jumpMap["mainmenu"]              = "Main Menu";
    jumpMap["managerecordings"]      = "Manage Recordings / Fix Conflicts";
    jumpMap["mythgallery"]           = "MythGallery";
    jumpMap["mythvideo"]             = "Video Default";
    jumpMap["mythweather"]           = "MythWeather";
    jumpMap["mythgame"]              = "MythGame";
    jumpMap["mythnews"]              = "MythNews";
    jumpMap["playdvd"]               = "Play Disc";
    jumpMap["playmusic"]             = "Play music";
    jumpMap["programfinder"]         = "Program Finder";
    jumpMap["programguide"]          = "Program Guide";
    jumpMap["ripcd"]                 = "Rip CD";
    jumpMap["musicplaylists"]        = "Select music playlists";
    jumpMap["playbackrecordings"]    = "TV Recording Playback";
    jumpMap["videobrowser"]          = "Video Browser";
    jumpMap["videogallery"]          = "Video Gallery";
    jumpMap["videolistings"]         = "Video Listings";
    jumpMap["videomanager"]          = "Video Manager";
    jumpMap["zoneminderconsole"]     = "ZoneMinder Console";
    jumpMap["zoneminderliveview"]    = "ZoneMinder Live View";
    jumpMap["zoneminderevents"]      = "ZoneMinder Events";

    jumpMap["channelrecpriority"]    = "Channel Recording Priorities";
    jumpMap["viewscheduled"]         = "Manage Recordings / Fix Conflicts";
    jumpMap["previousbox"]           = "Previously Recorded";
    jumpMap["progfinder"]            = "Program Finder";
    jumpMap["guidegrid"]             = "Program Guide";
    jumpMap["managerecrules"]        = "Manage Recording Rules";
    jumpMap["statusbox"]             = "Status Screen";
    jumpMap["playbackbox"]           = "TV Recording Playback";
    jumpMap["pbb"]                   = "TV Recording Playback";

    keyMap["up"]                     = Qt::Key_Up;
    keyMap["down"]                   = Qt::Key_Down;
    keyMap["left"]                   = Qt::Key_Left;
    keyMap["right"]                  = Qt::Key_Right;
    keyMap["home"]                   = Qt::Key_Home;
    keyMap["end"]                    = Qt::Key_End;
    keyMap["enter"]                  = Qt::Key_Enter;
    keyMap["return"]                 = Qt::Key_Return;
    keyMap["pageup"]                 = Qt::Key_PageUp;
    keyMap["pagedown"]               = Qt::Key_PageDown;
    keyMap["escape"]                 = Qt::Key_Escape;
    keyMap["tab"]                    = Qt::Key_Tab;
    keyMap["backtab"]                = Qt::Key_Backtab;
    keyMap["space"]                  = Qt::Key_Space;
    keyMap["backspace"]              = Qt::Key_Backspace;
    keyMap["insert"]                 = Qt::Key_Insert;
    keyMap["delete"]                 = Qt::Key_Delete;
    keyMap["plus"]                   = Qt::Key_Plus;
    keyMap["+"]                      = Qt::Key_Plus;
    keyMap["comma"]                  = Qt::Key_Comma;
    keyMap[","]                      = Qt::Key_Comma;
    keyMap["minus"]                  = Qt::Key_Minus;
    keyMap["-"]                      = Qt::Key_Minus;
    keyMap["underscore"]             = Qt::Key_Underscore;
    keyMap["_"]                      = Qt::Key_Underscore;
    keyMap["period"]                 = Qt::Key_Period;
    keyMap["."]                      = Qt::Key_Period;
    keyMap["numbersign"]             = Qt::Key_NumberSign;
    keyMap["poundsign"]              = Qt::Key_NumberSign;
    keyMap["hash"]                   = Qt::Key_NumberSign;
    keyMap["#"]                      = Qt::Key_NumberSign;
    keyMap["bracketleft"]            = Qt::Key_BracketLeft;
    keyMap["["]                      = Qt::Key_BracketLeft;
    keyMap["bracketright"]           = Qt::Key_BracketRight;
    keyMap["]"]                      = Qt::Key_BracketRight;
    keyMap["backslash"]              = Qt::Key_Backslash;
    keyMap["\\"]                     = Qt::Key_Backslash;
    keyMap["dollar"]                 = Qt::Key_Dollar;
    keyMap["$"]                      = Qt::Key_Dollar;
    keyMap["percent"]                = Qt::Key_Percent;
    keyMap["%"]                      = Qt::Key_Percent;
    keyMap["ampersand"]              = Qt::Key_Ampersand;
    keyMap["&"]                      = Qt::Key_Ampersand;
    keyMap["parenleft"]              = Qt::Key_ParenLeft;
    keyMap["("]                      = Qt::Key_ParenLeft;
    keyMap["parenright"]             = Qt::Key_ParenRight;
    keyMap[")"]                      = Qt::Key_ParenRight;
    keyMap["asterisk"]               = Qt::Key_Asterisk;
    keyMap["*"]                      = Qt::Key_Asterisk;
    keyMap["question"]               = Qt::Key_Question;
    keyMap["?"]                      = Qt::Key_Question;
    keyMap["slash"]                  = Qt::Key_Slash;
    keyMap["/"]                      = Qt::Key_Slash;
    keyMap["colon"]                  = Qt::Key_Colon;
    keyMap[":"]                      = Qt::Key_Colon;
    keyMap["semicolon"]              = Qt::Key_Semicolon;
    keyMap[";"]                      = Qt::Key_Semicolon;
    keyMap["less"]                   = Qt::Key_Less;
    keyMap["<"]                      = Qt::Key_Less;
    keyMap["equal"]                  = Qt::Key_Equal;
    keyMap["="]                      = Qt::Key_Equal;
    keyMap["greater"]                = Qt::Key_Greater;
    keyMap[">"]                      = Qt::Key_Greater;
    keyMap["bar"]                    = Qt::Key_Bar;
    keyMap["pipe"]                   = Qt::Key_Bar;
    keyMap["|"]                      = Qt::Key_Bar;
    keyMap["f1"]                     = Qt::Key_F1;
    keyMap["f2"]                     = Qt::Key_F2;
    keyMap["f3"]                     = Qt::Key_F3;
    keyMap["f4"]                     = Qt::Key_F4;
    keyMap["f5"]                     = Qt::Key_F5;
    keyMap["f6"]                     = Qt::Key_F6;
    keyMap["f7"]                     = Qt::Key_F7;
    keyMap["f8"]                     = Qt::Key_F8;
    keyMap["f9"]                     = Qt::Key_F9;
    keyMap["f10"]                    = Qt::Key_F10;
    keyMap["f11"]                    = Qt::Key_F11;
    keyMap["f12"]                    = Qt::Key_F12;
    keyMap["f13"]                    = Qt::Key_F13;
    keyMap["f14"]                    = Qt::Key_F14;
    keyMap["f15"]                    = Qt::Key_F15;
    keyMap["f16"]                    = Qt::Key_F16;
    keyMap["f17"]                    = Qt::Key_F17;
    keyMap["f18"]                    = Qt::Key_F18;
    keyMap["f19"]                    = Qt::Key_F19;
    keyMap["f20"]                    = Qt::Key_F20;
    keyMap["f21"]                    = Qt::Key_F21;
    keyMap["f22"]                    = Qt::Key_F22;
    keyMap["f23"]                    = Qt::Key_F23;
    keyMap["f24"]                    = Qt::Key_F24;

    keyTextMap[Qt::Key_Plus]            = "+";
    keyTextMap[Qt::Key_Comma]           = ",";
    keyTextMap[Qt::Key_Minus]           = "-";
    keyTextMap[Qt::Key_Underscore]      = "_";
    keyTextMap[Qt::Key_Period]          = ".";
    keyTextMap[Qt::Key_NumberSign]      = "#";
    keyTextMap[Qt::Key_BracketLeft]     = "[";
    keyTextMap[Qt::Key_BracketRight]    = "]";
    keyTextMap[Qt::Key_Backslash]       = "\\";
    keyTextMap[Qt::Key_Dollar]          = "$";
    keyTextMap[Qt::Key_Percent]         = "%";
    keyTextMap[Qt::Key_Ampersand]       = "&";
    keyTextMap[Qt::Key_ParenLeft]       = "(";
    keyTextMap[Qt::Key_ParenRight]      = ")";
    keyTextMap[Qt::Key_Asterisk]        = "*";
    keyTextMap[Qt::Key_Question]        = "?";
    keyTextMap[Qt::Key_Slash]           = "/";
    keyTextMap[Qt::Key_Colon]           = ":";
    keyTextMap[Qt::Key_Semicolon]       = ";";
    keyTextMap[Qt::Key_Less]            = "<";
    keyTextMap[Qt::Key_Equal]           = "=";
    keyTextMap[Qt::Key_Greater]         = ">";
    keyTextMap[Qt::Key_Bar]             = "|";

    commandThread->start();

    gCoreContext->addListener(this);

    connect(this, SIGNAL(newConnection(QTcpSocket*)),
            this, SLOT(newConnection(QTcpSocket*)));
}

NetworkControl::~NetworkControl(void)
{
    gCoreContext->removeListener(this);

    clientLock.lock();
    while (!clients.isEmpty())
    {
        NetworkControlClient *ncc = clients.takeFirst();
        delete ncc;
    }
    clientLock.unlock();

    nrLock.lock();
    networkControlReplies.push_back(new NetworkCommand(NULL,
        "mythfrontend shutting down, connection closing..."));
    nrLock.unlock();

    notifyDataAvailable();

    ncLock.lock();
    stopCommandThread = true;
    ncCond.wakeOne();
    ncLock.unlock();
    commandThread->wait();
    delete commandThread;
    commandThread = NULL;
}

void NetworkControl::run(void)
{
    QMutexLocker locker(&ncLock);
    while (!stopCommandThread)
    {
        while (networkControlCommands.empty() && !stopCommandThread)
            ncCond.wait(&ncLock);
        if (!stopCommandThread)
        {
            NetworkCommand *nc = networkControlCommands.front();
            networkControlCommands.pop_front();
            locker.unlock();
            processNetworkControlCommand(nc);
            locker.relock();
        }
    }
}

void NetworkControl::processNetworkControlCommand(NetworkCommand *nc)
{
    QMutexLocker locker(&clientLock);
    QString result;

    int clientID = clients.indexOf(nc->getClient());

    if (is_abbrev("jump", nc->getArg(0)))
        result = processJump(nc);
    else if (is_abbrev("key", nc->getArg(0)))
        result = processKey(nc);
    else if (is_abbrev("play", nc->getArg(0)))
        result = processPlay(nc, clientID);
    else if (is_abbrev("query", nc->getArg(0)))
        result = processQuery(nc);
    else if (is_abbrev("set", nc->getArg(0)))
        result = processSet(nc);
    else if (is_abbrev("screenshot", nc->getArg(0)))
        result = saveScreenshot(nc);
    else if (is_abbrev("help", nc->getArg(0)))
        result = processHelp(nc);
    else if (is_abbrev("message", nc->getArg(0)))
        result = processMessage(nc);
    else if (is_abbrev("notification", nc->getArg(0)))
        result = processNotification(nc);
    else if ((nc->getArg(0).toLower() == "exit") || (nc->getArg(0).toLower() == "quit"))
        QCoreApplication::postEvent(this,
                                new NetworkControlCloseEvent(nc->getClient()));
    else if (! nc->getArg(0).isEmpty())
        result = QString("INVALID command '%1', try 'help' for more info")
                         .arg(nc->getArg(0));

    nrLock.lock();
    networkControlReplies.push_back(new NetworkCommand(nc->getClient(),result));
    nrLock.unlock();

    notifyDataAvailable();
}

void NetworkControl::deleteClient(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Client Socket disconnected");
    QMutexLocker locker(&clientLock);

    gCoreContext->SendSystemEvent("NET_CTRL_DISCONNECTED");

    QList<NetworkControlClient *>::const_iterator it;
    for (it = clients.begin(); it != clients.end(); ++it)
    {
        NetworkControlClient *ncc = *it;
        if (ncc->getSocket()->state() == QTcpSocket::UnconnectedState)
        {
            deleteClient(ncc);
            return;
        }
    }
}

void NetworkControl::deleteClient(NetworkControlClient *ncc)
{
    int index = clients.indexOf(ncc);
    if (index >= 0)
    {
        clients.removeAt(index);

        delete ncc;
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("deleteClient(%1), unable to "
                "locate specified NetworkControlClient").arg((long long)ncc));
}

void NetworkControl::newConnection(QTcpSocket *client)
{
    QString welcomeStr;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("New connection established."));

    gCoreContext->SendSystemEvent("NET_CTRL_CONNECTED");

    NetworkControlClient *ncc = new NetworkControlClient(client);

    QMutexLocker locker(&clientLock);
    clients.push_back(ncc);

    connect(ncc, SIGNAL(commandReceived(QString&)), this,
            SLOT(receiveCommand(QString&)));
    connect(client, SIGNAL(disconnected()), this, SLOT(deleteClient()));

    welcomeStr = "MythFrontend Network Control\r\n";
    welcomeStr += "Type 'help' for usage information\r\n"
                  "---------------------------------";
    nrLock.lock();
    networkControlReplies.push_back(new NetworkCommand(ncc,welcomeStr));
    nrLock.unlock();

    notifyDataAvailable();
}

NetworkControlClient::NetworkControlClient(QTcpSocket *s)
{
    m_socket = s;
    m_textStream = new QTextStream(s);
    m_textStream->setCodec("UTF-8");
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readClient()));
}

NetworkControlClient::~NetworkControlClient()
{
    m_socket->close();
    m_socket->deleteLater();

    delete m_textStream;
}

void NetworkControlClient::readClient(void)
{
    QTcpSocket *socket = (QTcpSocket *)sender();
    if (!socket)
        return;

    QString lineIn;
    while (socket->canReadLine())
    {
        lineIn = socket->readLine();
#if 0
        lineIn.replace(QRegExp("[^-a-zA-Z0-9\\s\\.:_#/$%&()*+,;<=>?\\[\\]\\|]"),
                       "");
#endif

        // TODO: can this be replaced with lineIn.simplify()?
        lineIn.replace(QRegExp("[\r\n]"), "");
        lineIn.replace(QRegExp("^\\s"), "");

        if (lineIn.isEmpty())
            continue;

        LOG(VB_NETWORK, LOG_INFO, LOC +
            QString("emit commandReceived(%1)").arg(lineIn));
        emit commandReceived(lineIn);
    }
}

void NetworkControl::receiveCommand(QString &command)
{
    LOG(VB_NETWORK, LOG_INFO, LOC +
        QString("NetworkControl::receiveCommand(%1)").arg(command));
    NetworkControlClient *ncc = static_cast<NetworkControlClient *>(sender());
    if (!ncc)
         return;

    ncLock.lock();
    networkControlCommands.push_back(new NetworkCommand(ncc,command));
    ncCond.wakeOne();
    ncLock.unlock();
}

QString NetworkControl::processJump(NetworkCommand *nc)
{
    QString result = "OK";

    if ((nc->getArgCount() < 2) || (!jumpMap.contains(nc->getArg(1))))
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    GetMythMainWindow()->JumpTo(jumpMap[nc->getArg(1)]);

    // Fixme, should do some better checking here, but that would
    // depend on all Locations matching their jumppoints
    QTime timer;
    timer.start();
    while ((timer.elapsed() < FE_SHORT_TO) &&
           (GetMythUI()->GetCurrentLocation().toLower() != nc->getArg(1)))
        usleep(10000);

    return result;
}

QString NetworkControl::processKey(NetworkCommand *nc)
{
    QString result = "OK";
    QKeyEvent *event = NULL;

    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    QObject *keyDest = NULL;

    if (GetMythMainWindow())
        keyDest = GetMythMainWindow();
    else
        return QString("ERROR: Application has no main window!\n");

    if (GetMythMainWindow()->currentWidget())
        keyDest = GetMythMainWindow()->currentWidget()->focusWidget();

    int curToken = 1;
    int tokenLen = 0;
    while (curToken < nc->getArgCount())
    {
        tokenLen = nc->getArg(curToken).length();

        if (nc->getArg(curToken) == "sleep")
        {
            sleep(1);
        }
        else if (keyMap.contains(nc->getArg(curToken)))
        {
            int keyCode = keyMap[nc->getArg(curToken)];
            QString keyText;

            if (keyTextMap.contains(keyCode))
                keyText = keyTextMap[keyCode];

            GetMythUI()->ResetScreensaver();

            event = new QKeyEvent(QEvent::KeyPress, keyCode, Qt::NoModifier,
                                  keyText);
            QCoreApplication::postEvent(keyDest, event);

            event = new QKeyEvent(QEvent::KeyRelease, keyCode, Qt::NoModifier,
                                  keyText);
            QCoreApplication::postEvent(keyDest, event);
        }
        else if (((tokenLen == 1) &&
                  (nc->getArg(curToken)[0].isLetterOrNumber())) ||
                 ((tokenLen >= 1) &&
                  (nc->getArg(curToken).contains("+"))))
        {
            QKeySequence a(nc->getArg(curToken));
            int keyCode = a[0];
            Qt::KeyboardModifiers modifiers = Qt::NoModifier;

            if (tokenLen > 1)
            {
                QStringList tokenParts = nc->getArg(curToken).split('+');

                int partNum = 0;
                while (partNum < (tokenParts.size() - 1))
                {
                    if (tokenParts[partNum].toUpper() == "CTRL")
                        modifiers |= Qt::ControlModifier;
                    if (tokenParts[partNum].toUpper() == "SHIFT")
                        modifiers |= Qt::ShiftModifier;
                    if (tokenParts[partNum].toUpper() == "ALT")
                        modifiers |= Qt::AltModifier;
                    if (tokenParts[partNum].toUpper() == "META")
                        modifiers |= Qt::MetaModifier;

                    partNum++;
                }
            }
            else
            {
                if (nc->getArg(curToken) == nc->getArg(curToken).toUpper())
                    modifiers = Qt::ShiftModifier;
            }

            GetMythUI()->ResetScreensaver();

            event = new QKeyEvent(QEvent::KeyPress, keyCode, modifiers,
                                  nc->getArg(curToken));
            QCoreApplication::postEvent(keyDest, event);

            event = new QKeyEvent(QEvent::KeyRelease, keyCode, modifiers,
                                  nc->getArg(curToken));
            QCoreApplication::postEvent(keyDest, event);
        }
        else
            return QString("ERROR: Invalid syntax at '%1', see 'help %2' for "
                           "usage information")
                           .arg(nc->getArg(curToken)).arg(nc->getArg(0));

        curToken++;
    }

    return result;
}

QString NetworkControl::processPlay(NetworkCommand *nc, int clientID)
{
    QString result = "OK";
    QString message;

    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    if ((nc->getArgCount() >= 3) &&
        (is_abbrev("file", nc->getArg(1))))
    {
        if (GetMythUI()->GetCurrentLocation().toLower() != "mainmenu")
        {
            GetMythMainWindow()->JumpTo(jumpMap["mainmenu"]);

            QTime timer;
            timer.start();
            while ((timer.elapsed() < FE_LONG_TO) &&
                   (GetMythUI()->GetCurrentLocation().toLower() != "mainmenu"))
                usleep(10000);
        }

        if (GetMythUI()->GetCurrentLocation().toLower() == "mainmenu")
        {
            QStringList args;
            args << nc->getFrom(2);
            MythEvent *me = new MythEvent(ACTION_HANDLEMEDIA, args);
            qApp->postEvent(GetMythMainWindow(), me);
        }
        else
            return QString("Unable to change to main menu to start playback!");
    }
    else if ((nc->getArgCount() >= 4) &&
             (is_abbrev("program", nc->getArg(1))) &&
             (nc->getArg(2).contains(QRegExp("^\\d+$"))) &&
             (nc->getArg(3).contains(QRegExp(
                         "^\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d$"))))
    {
        if (GetMythUI()->GetCurrentLocation().toLower() == "playback")
        {
            QString message = QString("NETWORK_CONTROL STOP");
            MythEvent me(message);
            gCoreContext->dispatch(me);

            QTime timer;
            timer.start();
            while ((timer.elapsed() < FE_LONG_TO) &&
                   (GetMythUI()->GetCurrentLocation().toLower() == "playback"))
                usleep(10000);
        }

        if (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox")
        {
            GetMythMainWindow()->JumpTo(jumpMap["playbackbox"]);

            QTime timer;
            timer.start();
            while ((timer.elapsed() < 10000) &&
                   (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox"))
                usleep(10000);

            timer.start();
            while ((timer.elapsed() < 10000) &&
                   (!GetMythUI()->IsTopScreenInitialized()))
                usleep(10000);
        }

        if (GetMythUI()->GetCurrentLocation().toLower() == "playbackbox")
        {
            QString action = "PLAY";
            if (nc->getArgCount() == 5 && nc->getArg(4) == "resume")
                action = "RESUME";

            QString message = QString("NETWORK_CONTROL %1 PROGRAM %2 %3 %4")
                                      .arg(action).arg(nc->getArg(2))
                                      .arg(nc->getArg(3).toUpper()).arg(clientID);

            result.clear();
            gotAnswer = false;
            QTime timer;
            timer.start();

            MythEvent me(message);
            gCoreContext->dispatch(me);

            while (timer.elapsed() < FE_LONG_TO && !gotAnswer)
                usleep(10000);

            if (gotAnswer)
                result += answer;
            else
                result = "ERROR: Timed out waiting for reply from player";

        }
        else
        {
            result = QString("ERROR: Unable to change to PlaybackBox from "
                             "%1, cannot play requested file.")
                             .arg(GetMythUI()->GetCurrentLocation());
        }
    }
    else if (is_abbrev("music", nc->getArg(1)))
    {
#if 0
        if (GetMythUI()->GetCurrentLocation().toLower() != "playmusic")
        {
            return QString("ERROR: You are in %1 mode and this command is "
                           "only for MythMusic")
                        .arg(GetMythUI()->GetCurrentLocation());
        }
#endif

        QString hostname = gCoreContext->GetHostName();

        if (nc->getArgCount() == 3)
        {
            if (is_abbrev("play", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 PLAY").arg(hostname);
            else if (is_abbrev("pause", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 PAUSE").arg(hostname);
            else if (is_abbrev("stop", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 STOP").arg(hostname);
            else if (is_abbrev("getvolume", nc->getArg(2)))
            {
                gotAnswer = false;

                MythEvent me(QString("MUSIC_COMMAND %1 GET_VOLUME").arg(hostname));
                gCoreContext->dispatch(me);

                QTime timer;
                timer.start();
                while (timer.elapsed() < FE_SHORT_TO && !gotAnswer)
                {
                    qApp->processEvents();
                    usleep(10000);
                }

                if (gotAnswer)
                    return answer;

                return "unknown";
            }
            else if (is_abbrev("getmeta", nc->getArg(2)))
            {
                gotAnswer = false;

                MythEvent me(QString("MUSIC_COMMAND %1 GET_METADATA").arg(hostname));
                gCoreContext->dispatch(me);

                QTime timer;
                timer.start();
                while (timer.elapsed() < FE_SHORT_TO && !gotAnswer)
                {
                    qApp->processEvents();
                    usleep(10000);
                }

                if (gotAnswer)
                    return answer;

                return "unknown";
            }
            else
                return QString("ERROR: Invalid 'play music' command");
        }
        else if (nc->getArgCount() > 3)
        {
            if (is_abbrev("setvolume", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 SET_VOLUME %2")
                                .arg(hostname)
                                .arg(nc->getArg(3));
            else if (is_abbrev("track", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 PLAY_TRACK %2")
                                .arg(hostname)
                                .arg(nc->getArg(3));
            else if (is_abbrev("url", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 PLAY_URL %2")
                                .arg(hostname)
                                .arg(nc->getArg(3));
            else if (is_abbrev("file", nc->getArg(2)))
                message = QString("MUSIC_COMMAND %1 PLAY_FILE '%2'")
                                .arg(hostname)
                                .arg(nc->getFrom(3));
            else
                return QString("ERROR: Invalid 'play music' command");
        }
        else
            return QString("ERROR: Invalid 'play music' command");
    }
    // Everything below here requires us to be in playback mode so check to
    // see if we are
    else if (GetMythUI()->GetCurrentLocation().toLower() != "playback")
    {
        return QString("ERROR: You are in %1 mode and this command is only "
                       "for playback mode")
                       .arg(GetMythUI()->GetCurrentLocation());
    }
    else if (is_abbrev("chanid", nc->getArg(1), 5))
    {
        if (nc->getArg(2).contains(QRegExp("^\\d+$")))
            message = QString("NETWORK_CONTROL CHANID %1").arg(nc->getArg(2));
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
    }
    else if (is_abbrev("channel", nc->getArg(1), 5))
    {
        if (nc->getArgCount() < 3)
            return "ERROR: See 'help play' for usage information";

        if (is_abbrev("up", nc->getArg(2)))
            message = "NETWORK_CONTROL CHANNEL UP";
        else if (is_abbrev("down", nc->getArg(2)))
            message = "NETWORK_CONTROL CHANNEL DOWN";
        else if (nc->getArg(2).contains(QRegExp("^[-\\.\\d_#]+$")))
            message = QString("NETWORK_CONTROL CHANNEL %1").arg(nc->getArg(2));
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
    }
    else if (is_abbrev("seek", nc->getArg(1), 2))
    {
        if (nc->getArgCount() < 3)
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));

        if (is_abbrev("beginning", nc->getArg(2)))
            message = "NETWORK_CONTROL SEEK BEGINNING";
        else if (is_abbrev("forward", nc->getArg(2)))
            message = "NETWORK_CONTROL SEEK FORWARD";
        else if (is_abbrev("rewind",   nc->getArg(2)) ||
                 is_abbrev("backward", nc->getArg(2)))
            message = "NETWORK_CONTROL SEEK BACKWARD";
        else if (nc->getArg(2).contains(QRegExp("^\\d\\d:\\d\\d:\\d\\d$")))
        {
            int hours   = nc->getArg(2).mid(0, 2).toInt();
            int minutes = nc->getArg(2).mid(3, 2).toInt();
            int seconds = nc->getArg(2).mid(6, 2).toInt();
            message = QString("NETWORK_CONTROL SEEK POSITION %1")
                              .arg((hours * 3600) + (minutes * 60) + seconds);
        }
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
    }
    else if (is_abbrev("speed", nc->getArg(1), 2))
    {
        if (nc->getArgCount() < 3)
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));

        QString token2 = nc->getArg(2).toLower();
        if ((token2.contains(QRegExp("^\\-*\\d+x$"))) ||
            (token2.contains(QRegExp("^\\-*\\d+\\/\\d+x$"))) ||
            (token2.contains(QRegExp("^\\-*\\d*\\.\\d+x$"))))
            message = QString("NETWORK_CONTROL SPEED %1").arg(token2);
        else if (is_abbrev("normal", token2))
            message = QString("NETWORK_CONTROL SPEED 1x");
        else if (is_abbrev("pause", token2))
            message = QString("NETWORK_CONTROL SPEED 0x");
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
    }
    else if (is_abbrev("save", nc->getArg(1), 2))
    {
        if (is_abbrev("screenshot", nc->getArg(2), 2))
            return saveScreenshot(nc);
    }
    else if (is_abbrev("stop", nc->getArg(1), 2))
        message = QString("NETWORK_CONTROL STOP");
    else if (is_abbrev("volume", nc->getArg(1), 2))
    {
        if ((nc->getArgCount() < 3) ||
            (!nc->getArg(2).toLower().contains(QRegExp("^\\d+%?$"))))
        {
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
        }

        message = QString("NETWORK_CONTROL VOLUME %1")
                          .arg(nc->getArg(2).toLower());
    }
    else if (is_abbrev("subtitles", nc->getArg(1), 2))
    {
	if (nc->getArgCount() < 3)
	    message = QString("NETWORK_CONTROL SUBTITLES 0");
	else if (!nc->getArg(2).toLower().contains(QRegExp("^\\d+$")))
	    return QString("ERROR: See 'help %1' for usage information")
		.arg(nc->getArg(0));
	else
	    message = QString("NETWORK_CONTROL SUBTITLES %1")
		.arg(nc->getArg(2));
    }
    else
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    if (!message.isEmpty())
    {
        MythEvent me(message);
        gCoreContext->dispatch(me);
    }

    return result;
}

QString NetworkControl::processQuery(NetworkCommand *nc)
{
    QString result = "OK";

    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    if (is_abbrev("location", nc->getArg(1)))
    {
        bool fullPath = false;
        bool mainStackOnly = true;

        if (nc->getArgCount() > 2)
            fullPath = (nc->getArg(2).toLower() == "true" || nc->getArg(2) == "1");
        if (nc->getArgCount() > 3)
            mainStackOnly = (nc->getArg(3).toLower() == "true" || nc->getArg(3) == "1");

        QString location = GetMythUI()->GetCurrentLocation(fullPath, mainStackOnly);
        result = location;

        // if we're playing something, then find out what
        if (location == "Playback")
        {
            result += " ";
            gotAnswer = false;
            QString message = QString("NETWORK_CONTROL QUERY POSITION");
            MythEvent me(message);
            gCoreContext->dispatch(me);

            QTime timer;
            timer.start();
            while (timer.elapsed() < FE_SHORT_TO  && !gotAnswer)
                usleep(10000);

            if (gotAnswer)
                result += answer;
            else
                result = "ERROR: Timed out waiting for reply from player";
        }
    }
    else if (is_abbrev("verbose", nc->getArg(1)))
    {
        return verboseString;
    }
    else if (is_abbrev("liveTV", nc->getArg(1)))
    {
        if(nc->getArgCount() == 3) // has a channel ID
            return listSchedule(nc->getArg(2));
        else
            return listSchedule();
    }
    else if (is_abbrev("version", nc->getArg(1)))
    {
        int dbSchema = gCoreContext->GetNumSetting("DBSchemaVer");

        return QString("VERSION: %1/%2 %3 %4 QT/%5 DBSchema/%6")
                       .arg(MYTH_SOURCE_VERSION)
                       .arg(MYTH_SOURCE_PATH)
                       .arg(MYTH_BINARY_VERSION)
                       .arg(MYTH_PROTO_VERSION)
                       .arg(QT_VERSION_STR)
                       .arg(dbSchema);

    }
    else if(is_abbrev("time", nc->getArg(1)))
        return MythDate::current_iso_string();
    else if (is_abbrev("uptime", nc->getArg(1)))
    {
        QString str;
        time_t  uptime;

        if (getUptime(uptime))
            str = QString::number(uptime);
        else
            str = QString("Could not determine uptime.");
        return str;
    }
    else if (is_abbrev("load", nc->getArg(1)))
    {
        QString str;
        double  loads[3];

        if (getloadavg(loads,3) == -1)
            str = QString("getloadavg() failed");
        else
            str = QString("%1 %2 %3").arg(loads[0]).arg(loads[1]).arg(loads[2]);
        return str;
    }
    else if (is_abbrev("memstats", nc->getArg(1)))
    {
        QString str;
        int     totalMB, freeMB, totalVM, freeVM;

        if (getMemStats(totalMB, freeMB, totalVM, freeVM))
            str = QString("%1 %2 %3 %4")
                          .arg(totalMB).arg(freeMB).arg(totalVM).arg(freeVM);
        else
            str = QString("Could not determine memory stats.");
        return str;
    }
    else if (is_abbrev("volume", nc->getArg(1)))
    {
        QString str = "0%";

        QString location = GetMythUI()->GetCurrentLocation(false, false);

        if (location != "Playback")
            return str;

        gotAnswer = false;
        QString message = QString("NETWORK_CONTROL QUERY VOLUME");
        MythEvent me(message);
        gCoreContext->dispatch(me);

        QTime timer;
        timer.start();
        while (timer.elapsed() < FE_SHORT_TO  && !gotAnswer)
            usleep(10000);

        if (gotAnswer)
            str = answer;
        else
            str = "ERROR: Timed out waiting for reply from player";

        return str;
    }
    else if ((nc->getArgCount() == 4) &&
             is_abbrev("recording", nc->getArg(1)) &&
             (nc->getArg(2).contains(QRegExp("^\\d+$"))) &&
             (nc->getArg(3).contains(QRegExp(
                         "^\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d$"))))
        return listRecordings(nc->getArg(2), nc->getArg(3).toUpper());
    else if (is_abbrev("recordings", nc->getArg(1)))
        return listRecordings();
    else if (is_abbrev("channels", nc->getArg(1)))
    {
        if (nc->getArgCount() == 2)
            return listChannels(0, 0);  // give us all you can
        else if (nc->getArgCount() == 4)
            return listChannels(nc->getArg(2).toLower().toUInt(),
                                nc->getArg(3).toLower().toUInt());
        else
            return QString("ERROR: See 'help %1' for usage information "
                           "(parameters mismatch)").arg(nc->getArg(0));
    }
    else
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    return result;
}

QString NetworkControl::processSet(NetworkCommand *nc)
{
    if (nc->getArgCount() == 1)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    if (nc->getArg(1) == "verbose")
    {
        if (nc->getArgCount() > 3)
            return QString("ERROR: Separate filters with commas with no "
                           "space: playback,audio\r\n See 'help %1' for usage "
                           "information").arg(nc->getArg(0));

        QString oldVerboseString = verboseString;
        QString result = "OK";

        int pva_result = verboseArgParse(nc->getArg(2));

        if (pva_result != 0 /*GENERIC_EXIT_OK */)
            result = "Failed";

        result += "\r\n";
        result += " Previous filter: " + oldVerboseString + "\r\n";
        result += "      New Filter: " + verboseString + "\r\n";

        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Verbose mask changed, new level is: %1")
                .arg(verboseString));

        return result;
    }

    return QString("ERROR: See 'help %1' for usage information")
                   .arg(nc->getArg(0));
}

QString NetworkControl::processHelp(NetworkCommand *nc)
{
    QString command, helpText;

    if (nc->getArgCount() >= 1)
    {
        if (is_abbrev("help", nc->getArg(0)))
        {
            if (nc->getArgCount() >= 2)
                command = nc->getArg(1);
            else
                command.clear();
        }
        else
        {
            command = nc->getArg(0);
        }
    }

    if (is_abbrev("jump", command))
    {
        QMap<QString, QString>::Iterator it;
        helpText +=
            "Usage: jump JUMPPOINT\r\n"
            "\r\n"
            "Where JUMPPOINT is one of the following:\r\n";

        for (it = jumpMap.begin(); it != jumpMap.end(); ++it)
        {
            helpText += it.key().leftJustified(20, ' ', true) + " - " +
                        *it + "\r\n";
        }
    }
    else if (is_abbrev("key", command))
    {
        helpText +=
            "key LETTER           - Send the letter key specified\r\n"
            "key NUMBER           - Send the number key specified\r\n"
            "key CODE             - Send one of the following key codes\r\n"
            "\r\n";

        QMap<QString, int>::Iterator it;
        bool first = true;
        for (it = keyMap.begin(); it != keyMap.end(); ++it)
        {
            if (first)
                first = false;
            else
                helpText += ", ";

            helpText += it.key();
        }
        helpText += "\r\n";
    }
    else if (is_abbrev("play", command))
    {
        helpText +=
            "play volume NUMBER%    - Change volume to given percentage value\r\n"
            "play channel up        - Change channel Up\r\n"
            "play channel down      - Change channel Down\r\n"
            "play channel NUMBER    - Change to a specific channel number\r\n"
            "play chanid NUMBER     - Change to a specific channel id (chanid)\r\n"
            "play file FILENAME     - Play FILENAME (FILENAME may be a file or a myth:// URL)\r\n"
            "play program CHANID yyyy-MM-ddThh:mm:ss\r\n"
            "                       - Play program with chanid & starttime\r\n"
            "play program CHANID yyyy-MM-ddThh:mm:ss resume\r\n"
            "                       - Resume program with chanid & starttime\r\n"
            "play save preview\r\n"
            "                       - Save preview image from current position\r\n"
            "play save preview FILENAME\r\n"
            "                       - Save preview image to FILENAME\r\n"
            "play save preview FILENAME WxH\r\n"
            "                       - Save preview image of size WxH\r\n"
            "play seek beginning    - Seek to the beginning of the recording\r\n"
            "play seek forward      - Skip forward in the video\r\n"
            "play seek backward     - Skip backwards in the video\r\n"
            "play seek HH:MM:SS     - Seek to a specific position\r\n"
            "play speed pause       - Pause playback\r\n"
            "play speed normal      - Playback at normal speed\r\n"
            "play speed 1x          - Playback at normal speed\r\n"
            "play speed SPEEDx      - Playback where SPEED must be a decimal\r\n"
            "play speed 1/8x        - Playback at 1/8x speed\r\n"
            "play speed 1/4x        - Playback at 1/4x speed\r\n"
            "play speed 1/3x        - Playback at 1/3x speed\r\n"
            "play speed 1/2x        - Playback at 1/2x speed\r\n"
            "play stop              - Stop playback\r\n"
            "play subtitles [#]     - Switch on indicated subtitle tracks\r\n"
            "play music play        - Resume playback (MythMusic)\r\n"
            "play music pause       - Pause playback (MythMusic)\r\n"
            "play music stop        - Stop Playback (MythMusic)\r\n"
            "play music setvolume N - Set volume to number (MythMusic)\r\n"
            "play music getvolume   - Get current volume (MythMusic)\r\n"
            "play music getmeta     - Get metadata for current track (MythMusic)\r\n"
            "play music file NAME   - Play specified file (MythMusic)\r\n"
            "play music track N     - Switch to specified track (MythMusic)\r\n"
            "play music url URL     - Play specified URL (MythMusic)\r\n";
    }
    else if (is_abbrev("query", command))
    {
        helpText +=
            "query location        - Query current screen or location\r\n"
            "query volume          - Query the current playback volume\r\n"
            "query recordings      - List currently available recordings\r\n"
            "query recording CHANID STARTTIME\r\n"
            "                      - List info about the specified program\r\n"
            "query liveTV          - List current TV schedule\r\n"
            "query liveTV CHANID   - Query current program for specified channel\r\n"
            "query load            - List 1/5/15 load averages\r\n"
            "query memstats        - List free and total, physical and swap memory\r\n"
            "query time            - Query current time on frontend\r\n"
            "query uptime          - Query machine uptime\r\n"
            "query verbose         - Get current VERBOSE mask\r\n"
            "query version         - Query Frontend version details\r\n"
            "query channels        - Query available channels\r\n"
            "query channels START LIMIT - Query available channels from START and limit results to LIMIT lines\r\n";
    }
    else if (is_abbrev("set", command))
    {
        helpText +=
            "set verbose debug-mask - "
            "Change the VERBOSE mask to 'debug-mask'\r\n"
            "                         (i.e. 'set verbose playback,audio')\r\n"
            "                         use 'set verbose default' to revert\r\n"
            "                         back to the default level of\r\n";
    }
    else if (is_abbrev("screenshot", command))
    {
        helpText +=
            "screenshot               - Takes a screenshot and saves it as screenshot.png\r\n"
            "screenshot WxH           - Saves the screenshot as a WxH size image\r\n";
    }
    else if (command == "exit")
    {
        helpText +=
            "exit                  - Terminates session\r\n\r\n";
    }
    else if ((is_abbrev("message", command)))
    {
        helpText +=
            "message               - Displays a simple text message popup\r\n";
    }
    else if ((is_abbrev("notification", command)))
    {
        helpText +=
            "notification          - Displays a simple text message notification\r\n";
    }

    if (!helpText.isEmpty())
        return helpText;

    if (!command.isEmpty())
            helpText += QString("Unknown command '%1'\r\n\r\n").arg(command);

    helpText +=
        "Valid Commands:\r\n"
        "---------------\r\n"
        "jump               - Jump to a specified location in Myth\r\n"
        "key                - Send a keypress to the program\r\n"
        "play               - Playback related commands\r\n"
        "query              - Queries\r\n"
        "set                - Changes\r\n"
        "screenshot         - Capture screenshot\r\n"
        "message            - Display a simple text message\r\n"
        "notification       - Display a simple text notification\r\n"
        "exit               - Exit Network Control\r\n"
        "\r\n"
        "Type 'help COMMANDNAME' for help on any specific command.\r\n";

    return helpText;
}

QString NetworkControl::processMessage(NetworkCommand *nc)
{
    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    QString message = nc->getCommand().remove(0, 7).trimmed();
    MythMainWindow *window = GetMythMainWindow();
    MythEvent* me = new MythEvent(MythEvent::MythUserMessage, message);
    qApp->postEvent(window, me);
    return QString("OK");
}

QString NetworkControl::processNotification(NetworkCommand *nc)
{
    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
        .arg(nc->getArg(0));

    QString message = nc->getCommand().remove(0, 12).trimmed();
    MythNotification n(message, tr("Network Control"));
    GetNotificationCenter()->Queue(n);
    return QString("OK");
}

void NetworkControl::notifyDataAvailable(void)
{
    QCoreApplication::postEvent(
        this, new QEvent(kNetworkControlDataReadyEvent));
}

void NetworkControl::sendReplyToClient(NetworkControlClient *ncc,
                                       QString &reply)
{
    if (!clients.contains(ncc))
        // NetworkControl instance is unaware of control client
        // assume connection to client has been terminated and bail
        return;

    QRegExp crlfRegEx("\r\n$");
    QRegExp crlfcrlfRegEx("\r\n.*\r\n");

    QTcpSocket  *client = ncc->getSocket();
    QTextStream *clientStream = ncc->getTextStream();

    if (client && clientStream && client->state() == QTcpSocket::ConnectedState)
    {
        *clientStream << reply;

        if ((!reply.contains(crlfRegEx)) ||
            ( reply.contains(crlfcrlfRegEx)))
            *clientStream << "\r\n" << prompt;

        clientStream->flush();
        client->flush();
    }
}

void NetworkControl::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.startsWith("MUSIC_CONTROL"))
        {
            QStringList tokens = message.simplified().split(" ");
            if ((tokens.size() >= 4) &&
                (tokens[1] == "ANSWER") &&
                (tokens[2] == gCoreContext->GetHostName()))
            {
                answer = tokens[3];
                for (int i = 4; i < tokens.size(); i++)
                    answer += QString(" ") + tokens[i];
                gotAnswer = true;
            } 

        }
        else if (message.startsWith("NETWORK_CONTROL"))
        {
            QStringList tokens = message.simplified().split(" ");
            if ((tokens.size() >= 3) &&
                (tokens[1] == "ANSWER"))
            {
                answer = tokens[2];
                for (int i = 3; i < tokens.size(); i++)
                    answer += QString(" ") + tokens[i];
                gotAnswer = true;
            }
            else if ((tokens.size() >= 4) &&
                     (tokens[1] == "RESPONSE"))
            {
//                int clientID = tokens[2].toInt();
                answer = tokens[3];
                for (int i = 4; i < tokens.size(); i++)
                    answer += QString(" ") + tokens[i];
                gotAnswer = true;
            }
        }
    }
    else if (e->type() == kNetworkControlDataReadyEvent)
    {
        NetworkCommand *nc;
        QString reply;

        QMutexLocker locker(&clientLock);
        QMutexLocker nrLocker(&nrLock);

        while (!networkControlReplies.isEmpty())
        {
            nc = networkControlReplies.front();
            networkControlReplies.pop_front();

            reply = nc->getCommand();

            NetworkControlClient * ncc = nc->getClient();
            if (ncc)
            {
                sendReplyToClient(ncc, reply);
            }
            else //send to all clients
            {
                QList<NetworkControlClient *>::const_iterator it;
                for (it = clients.begin(); it != clients.end(); ++it)
                {
                    NetworkControlClient *ncc = *it;
                    if (ncc)
                        sendReplyToClient(ncc, reply);
                }
            }
            delete nc;
        }
    }
    else if (e->type() == NetworkControlCloseEvent::kEventType)
    {
        NetworkControlCloseEvent *ncce = static_cast<NetworkControlCloseEvent*>(e);
        NetworkControlClient     *ncc  = ncce->getClient();

        deleteClient(ncc);
    }
}

QString NetworkControl::listSchedule(const QString& chanID) const
{
    QString result("");
    MSqlQuery query(MSqlQuery::InitCon());
    bool appendCRLF = true;
    QString queryStr("SELECT chanid, starttime, endtime, title, subtitle "
                         "FROM program "
                         "WHERE starttime < :START AND endtime > :END ");

    if (!chanID.isEmpty())
    {
        queryStr += " AND chanid = :CHANID";
        appendCRLF = false;
    }

    queryStr += " ORDER BY starttime, endtime, chanid";

    query.prepare(queryStr);
    query.bindValue(":START", MythDate::current());
    query.bindValue(":END", MythDate::current());
    if (!chanID.isEmpty())
    {
        query.bindValue(":CHANID", chanID);
    }

    if (query.exec())
    {
        while (query.next())
        {
            QString title = query.value(3).toString();
            QString subtitle = query.value(4).toString();

            if (!subtitle.isEmpty())
                title += QString(" -\"%1\"").arg(subtitle);
            QByteArray atitle = title.toLocal8Bit();

            result +=
                QString("%1 %2 %3 %4")
                .arg(QString::number(query.value(0).toInt())
                     .rightJustified(5, ' '))
                .arg(MythDate::as_utc(query.value(1).toDateTime()).toString(Qt::ISODate))
                .arg(MythDate::as_utc(query.value(2).toDateTime()).toString(Qt::ISODate))
                .arg(atitle.constData());

            if (appendCRLF)
                result += "\r\n";
        }
    }
    else
    {
       result = "ERROR: Unable to retrieve current schedule list.";
    }
    return result;
}

QString NetworkControl::listRecordings(QString chanid, QString starttime)
{
    QString result;
    MSqlQuery query(MSqlQuery::InitCon());
    QString queryStr;
    bool appendCRLF = true;

    queryStr = "SELECT chanid, starttime, title, subtitle "
               "FROM recorded WHERE deletepending = 0 ";

    if ((!chanid.isEmpty()) && (!starttime.isEmpty()))
    {
        queryStr += "AND chanid = " + chanid + " "
                    "AND starttime = '" + starttime + "' ";
        appendCRLF = false;
    }

    queryStr += "ORDER BY starttime, title;";

    query.prepare(queryStr);
    if (query.exec())
    {
        QString episode, title, subtitle;
        while (query.next())
        {
            title = query.value(2).toString();
            subtitle = query.value(3).toString();

            if (!subtitle.isEmpty())
                episode = QString("%1 -\"%2\"")
                                  .arg(title)
                                  .arg(subtitle);
            else
                episode = title;

            result +=
                QString("%1 %2 %3").arg(query.value(0).toInt())
                .arg(MythDate::as_utc(query.value(1).toDateTime()).toString(Qt::ISODate))
                .arg(episode);

            if (appendCRLF)
                result += "\r\n";
        }
    }
    else
        result = "ERROR: Unable to retrieve recordings list.";

    return result;
}

QString NetworkControl::listChannels(const uint start, const uint limit) const
{
    QString result;
    MSqlQuery query(MSqlQuery::InitCon());
    QString queryStr;
    uint cnt;
    uint maxcnt;
    uint sqlStart = start;

    // sql starts at zero, we want to start at 1
    if (sqlStart > 0)
        sqlStart--;

    queryStr = "select chanid, callsign, name from channel where visible=1";
    queryStr += " ORDER BY callsign";

    if (limit > 0)  // only if a limit is specified, we limit the results
    {
      QString limitStr = QString(" LIMIT %1,%2").arg(sqlStart).arg(limit);
      queryStr += limitStr;
    }

    query.prepare(queryStr);
    if (!query.exec())
    {
        result = "ERROR: Unable to retrieve channel list.";
        return result;
    }

    maxcnt = query.size();
    cnt = 0;
    if (maxcnt == 0)    // Feedback we have no usefull information
    {
        result += QString("0:0 0 \"Invalid\" \"Invalid\"");
        return result;
    }

    while (query.next())
    {
        // Feedback is as follow:
        // <current line count>:<max line count to expect> <channelid> <callsign name> <channel name>\r\n
        cnt++;
        result += QString("%1:%2 %3 \"%4\" \"%5\"\r\n")
                          .arg(cnt).arg(maxcnt).arg(query.value(0).toInt())
                          .arg(query.value(1).toString())
                          .arg(query.value(2).toString());
    }

    return result;
}

QString NetworkControl::saveScreenshot(NetworkCommand *nc)
{
    int width = 0;
    int height = 0;

    if (nc->getArgCount() == 2)
    {
        QStringList size = nc->getArg(1).split('x');
        if (size.size() == 2)
        {
            width  = size[0].toInt();
            height = size[1].toInt();
        }
    }

    MythMainWindow *window = GetMythMainWindow();
    QStringList args;
    if (width && height)
    {
        args << QString::number(width);
        args << QString::number(height);
    }
    MythEvent* me = new MythEvent(MythEvent::MythEventMessage,
                                  ACTION_SCREENSHOT, args);
    qApp->postEvent(window, me);
    return "OK";
}

QString NetworkCommand::getFrom(int arg)
{
    QString c = m_command;
    for(int i=0 ; i<arg ; i++) {
        QString arg = c.simplified().split(" ")[0];
        c = c.mid(arg.length()).trimmed();
    }
    return c;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

