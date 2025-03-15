// C++
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QMap>
#include <QRegularExpression>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QStringList>
#include <QTextStream>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"
#include "libmythtv/mythsystemevent.h"
#include "libmythtv/previewgenerator.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttontree.h"
#include "libmythui/mythuicheckbox.h"
#include "libmythui/mythuiclock.h"
#include "libmythui/mythuieditbar.h"
#include "libmythui/mythuigroup.h"
#include "libmythui/mythuiguidegrid.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuiscrollbar.h"
#include "libmythui/mythuishape.h"
#include "libmythui/mythuispinbox.h"
#include "libmythui/mythuitextedit.h"
#include "libmythui/mythuivideo.h"
#if CONFIG_QTWEBKIT
#include "libmythui/mythuiwebbrowser.h"
#endif

// MythFrontend
#include "networkcontrol.h"


#define LOC QString("NetworkControl: ")
#define LOC_ERR QString("NetworkControl Error: ")

static constexpr qint64 FE_SHORT_TO {  2000 }; //  2 seconds
static constexpr qint64 FE_LONG_TO  { 10000 }; // 10 seconds

static const QEvent::Type kNetworkControlDataReadyEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type NetworkControlCloseEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

static const QRegularExpression kChanID1RE { "^\\d+$" };
static const QRegularExpression kStartTimeRE
    { R"(^\d\d\d\d-\d\d-\d\dT\d\d:\d\d:\d\dZ?$)" };

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
    return test.toLower() == command.left(test.length()).toLower();
}

NetworkControl::NetworkControl() :
    m_commandThread(new MThread("NetworkControl", this))
{
    // Eventually this map should be in the jumppoints table
    m_jumpMap["channelpriorities"]     = "Channel Recording Priorities";
    m_jumpMap["livetv"]                = "Live TV";
    m_jumpMap["mainmenu"]              = "Main Menu";
    m_jumpMap["managerecordings"]      = "Manage Recordings / Fix Conflicts";
    m_jumpMap["mythgallery"]           = "MythGallery";
    m_jumpMap["mythvideo"]             = "Video Default";
    m_jumpMap["mythweather"]           = "MythWeather";
    m_jumpMap["mythgame"]              = "MythGame";
    m_jumpMap["mythnews"]              = "MythNews";
    m_jumpMap["playdvd"]               = "Play Disc";
    m_jumpMap["playmusic"]             = "Play music";
    m_jumpMap["playlistview"]          = "Play music";
    m_jumpMap["programfinder"]         = "Program Finder";
    m_jumpMap["programguide"]          = "Program Guide";
    m_jumpMap["ripcd"]                 = "Rip CD";
    m_jumpMap["musicplaylists"]        = "Select music playlists";
    m_jumpMap["playbackrecordings"]    = "TV Recording Playback";
    m_jumpMap["videobrowser"]          = "Video Browser";
    m_jumpMap["videogallery"]          = "Video Gallery";
    m_jumpMap["videolistings"]         = "Video Listings";
    m_jumpMap["videomanager"]          = "Video Manager";
    m_jumpMap["zoneminderconsole"]     = "ZoneMinder Console";
    m_jumpMap["zoneminderliveview"]    = "ZoneMinder Live View";
    m_jumpMap["zoneminderevents"]      = "ZoneMinder Events";

    m_jumpMap["channelrecpriority"]    = "Channel Recording Priorities";
    m_jumpMap["viewscheduled"]         = "Manage Recordings / Fix Conflicts";
    m_jumpMap["previousbox"]           = "Previously Recorded";
    m_jumpMap["progfinder"]            = "Program Finder";
    m_jumpMap["guidegrid"]             = "Program Guide";
    m_jumpMap["managerecrules"]        = "Manage Recording Rules";
    m_jumpMap["statusbox"]             = "Status Screen";
    m_jumpMap["playbackbox"]           = "TV Recording Playback";
    m_jumpMap["pbb"]                   = "TV Recording Playback";

    m_jumpMap["reloadtheme"]           = "Reload Theme";
    m_jumpMap["showborders"]           = "Toggle Show Widget Borders";
    m_jumpMap["shownames"]             = "Toggle Show Widget Names";

    m_keyMap["up"]                     = Qt::Key_Up;
    m_keyMap["down"]                   = Qt::Key_Down;
    m_keyMap["left"]                   = Qt::Key_Left;
    m_keyMap["right"]                  = Qt::Key_Right;
    m_keyMap["home"]                   = Qt::Key_Home;
    m_keyMap["end"]                    = Qt::Key_End;
    m_keyMap["enter"]                  = Qt::Key_Enter;
    m_keyMap["return"]                 = Qt::Key_Return;
    m_keyMap["pageup"]                 = Qt::Key_PageUp;
    m_keyMap["pagedown"]               = Qt::Key_PageDown;
    m_keyMap["escape"]                 = Qt::Key_Escape;
    m_keyMap["tab"]                    = Qt::Key_Tab;
    m_keyMap["backtab"]                = Qt::Key_Backtab;
    m_keyMap["space"]                  = Qt::Key_Space;
    m_keyMap["backspace"]              = Qt::Key_Backspace;
    m_keyMap["insert"]                 = Qt::Key_Insert;
    m_keyMap["delete"]                 = Qt::Key_Delete;
    m_keyMap["plus"]                   = Qt::Key_Plus;
    m_keyMap["+"]                      = Qt::Key_Plus;
    m_keyMap["comma"]                  = Qt::Key_Comma;
    m_keyMap[","]                      = Qt::Key_Comma;
    m_keyMap["minus"]                  = Qt::Key_Minus;
    m_keyMap["-"]                      = Qt::Key_Minus;
    m_keyMap["underscore"]             = Qt::Key_Underscore;
    m_keyMap["_"]                      = Qt::Key_Underscore;
    m_keyMap["period"]                 = Qt::Key_Period;
    m_keyMap["."]                      = Qt::Key_Period;
    m_keyMap["numbersign"]             = Qt::Key_NumberSign;
    m_keyMap["poundsign"]              = Qt::Key_NumberSign;
    m_keyMap["hash"]                   = Qt::Key_NumberSign;
    m_keyMap["#"]                      = Qt::Key_NumberSign;
    m_keyMap["bracketleft"]            = Qt::Key_BracketLeft;
    m_keyMap["["]                      = Qt::Key_BracketLeft;
    m_keyMap["bracketright"]           = Qt::Key_BracketRight;
    m_keyMap["]"]                      = Qt::Key_BracketRight;
    m_keyMap["backslash"]              = Qt::Key_Backslash;
    m_keyMap["\\"]                     = Qt::Key_Backslash;
    m_keyMap["dollar"]                 = Qt::Key_Dollar;
    m_keyMap["$"]                      = Qt::Key_Dollar;
    m_keyMap["percent"]                = Qt::Key_Percent;
    m_keyMap["%"]                      = Qt::Key_Percent;
    m_keyMap["ampersand"]              = Qt::Key_Ampersand;
    m_keyMap["&"]                      = Qt::Key_Ampersand;
    m_keyMap["parenleft"]              = Qt::Key_ParenLeft;
    m_keyMap["("]                      = Qt::Key_ParenLeft;
    m_keyMap["parenright"]             = Qt::Key_ParenRight;
    m_keyMap[")"]                      = Qt::Key_ParenRight;
    m_keyMap["asterisk"]               = Qt::Key_Asterisk;
    m_keyMap["*"]                      = Qt::Key_Asterisk;
    m_keyMap["question"]               = Qt::Key_Question;
    m_keyMap["?"]                      = Qt::Key_Question;
    m_keyMap["slash"]                  = Qt::Key_Slash;
    m_keyMap["/"]                      = Qt::Key_Slash;
    m_keyMap["colon"]                  = Qt::Key_Colon;
    m_keyMap[":"]                      = Qt::Key_Colon;
    m_keyMap["semicolon"]              = Qt::Key_Semicolon;
    m_keyMap[";"]                      = Qt::Key_Semicolon;
    m_keyMap["less"]                   = Qt::Key_Less;
    m_keyMap["<"]                      = Qt::Key_Less;
    m_keyMap["equal"]                  = Qt::Key_Equal;
    m_keyMap["="]                      = Qt::Key_Equal;
    m_keyMap["greater"]                = Qt::Key_Greater;
    m_keyMap[">"]                      = Qt::Key_Greater;
    m_keyMap["bar"]                    = Qt::Key_Bar;
    m_keyMap["pipe"]                   = Qt::Key_Bar;
    m_keyMap["|"]                      = Qt::Key_Bar;
    m_keyMap["f1"]                     = Qt::Key_F1;
    m_keyMap["f2"]                     = Qt::Key_F2;
    m_keyMap["f3"]                     = Qt::Key_F3;
    m_keyMap["f4"]                     = Qt::Key_F4;
    m_keyMap["f5"]                     = Qt::Key_F5;
    m_keyMap["f6"]                     = Qt::Key_F6;
    m_keyMap["f7"]                     = Qt::Key_F7;
    m_keyMap["f8"]                     = Qt::Key_F8;
    m_keyMap["f9"]                     = Qt::Key_F9;
    m_keyMap["f10"]                    = Qt::Key_F10;
    m_keyMap["f11"]                    = Qt::Key_F11;
    m_keyMap["f12"]                    = Qt::Key_F12;
    m_keyMap["f13"]                    = Qt::Key_F13;
    m_keyMap["f14"]                    = Qt::Key_F14;
    m_keyMap["f15"]                    = Qt::Key_F15;
    m_keyMap["f16"]                    = Qt::Key_F16;
    m_keyMap["f17"]                    = Qt::Key_F17;
    m_keyMap["f18"]                    = Qt::Key_F18;
    m_keyMap["f19"]                    = Qt::Key_F19;
    m_keyMap["f20"]                    = Qt::Key_F20;
    m_keyMap["f21"]                    = Qt::Key_F21;
    m_keyMap["f22"]                    = Qt::Key_F22;
    m_keyMap["f23"]                    = Qt::Key_F23;
    m_keyMap["f24"]                    = Qt::Key_F24;

    m_keyTextMap[Qt::Key_Space]           = " ";
    m_keyTextMap[Qt::Key_Plus]            = "+";
    m_keyTextMap[Qt::Key_Comma]           = ",";
    m_keyTextMap[Qt::Key_Minus]           = "-";
    m_keyTextMap[Qt::Key_Underscore]      = "_";
    m_keyTextMap[Qt::Key_Period]          = ".";
    m_keyTextMap[Qt::Key_NumberSign]      = "#";
    m_keyTextMap[Qt::Key_BracketLeft]     = "[";
    m_keyTextMap[Qt::Key_BracketRight]    = "]";
    m_keyTextMap[Qt::Key_Backslash]       = "\\";
    m_keyTextMap[Qt::Key_Dollar]          = "$";
    m_keyTextMap[Qt::Key_Percent]         = "%";
    m_keyTextMap[Qt::Key_Ampersand]       = "&";
    m_keyTextMap[Qt::Key_ParenLeft]       = "(";
    m_keyTextMap[Qt::Key_ParenRight]      = ")";
    m_keyTextMap[Qt::Key_Asterisk]        = "*";
    m_keyTextMap[Qt::Key_Question]        = "?";
    m_keyTextMap[Qt::Key_Slash]           = "/";
    m_keyTextMap[Qt::Key_Colon]           = ":";
    m_keyTextMap[Qt::Key_Semicolon]       = ";";
    m_keyTextMap[Qt::Key_Less]            = "<";
    m_keyTextMap[Qt::Key_Equal]           = "=";
    m_keyTextMap[Qt::Key_Greater]         = ">";
    m_keyTextMap[Qt::Key_Bar]             = "|";

    m_commandThread->start();

    gCoreContext->addListener(this);

    connect(this, &ServerPool::newConnection,
            this, &NetworkControl::newControlConnection);
}

NetworkControl::~NetworkControl(void)
{
    gCoreContext->removeListener(this);

    m_clientLock.lock();
    while (!m_clients.isEmpty())
    {
        NetworkControlClient *ncc = m_clients.takeFirst();
        delete ncc;
    }
    m_clientLock.unlock();

    auto * cmd = new (std::nothrow) NetworkCommand(nullptr,
        "mythfrontend shutting down, connection closing...");
    if (cmd != nullptr)
    {
        m_nrLock.lock();
        m_networkControlReplies.push_back(cmd);
        m_nrLock.unlock();
        notifyDataAvailable();
    }

    m_ncLock.lock();
    m_stopCommandThread = true;
    m_ncCond.wakeOne();
    m_ncLock.unlock();
    m_commandThread->wait();
    delete m_commandThread;
    m_commandThread = nullptr;
}

void NetworkControl::run(void)
{
    QMutexLocker locker(&m_ncLock);
    while (!m_stopCommandThread)
    {
        // cppcheck-suppress knownConditionTrueFalse
        while (m_networkControlCommands.empty() && !m_stopCommandThread)
            m_ncCond.wait(&m_ncLock);
        // cppcheck-suppress knownConditionTrueFalse
        if (!m_stopCommandThread)
        {
            NetworkCommand *nc = m_networkControlCommands.front();
            m_networkControlCommands.pop_front();
            locker.unlock();
            processNetworkControlCommand(nc);
            locker.relock();
        }
    }
}

void NetworkControl::processNetworkControlCommand(NetworkCommand *nc)
{
    QMutexLocker locker(&m_clientLock);
    QString result;

    int clientID = m_clients.indexOf(nc->getClient());

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
    else if (is_abbrev("theme", nc->getArg(0)))
        result = processTheme(nc);
    else if ((nc->getArg(0).toLower() == "exit") || (nc->getArg(0).toLower() == "quit"))
    {
        QCoreApplication::postEvent(this,
                                new NetworkControlCloseEvent(nc->getClient()));
    }
    else if (! nc->getArg(0).isEmpty())
    {
        result = QString("INVALID command '%1', try 'help' for more info")
                         .arg(nc->getArg(0));
    }

    m_nrLock.lock();
    m_networkControlReplies.push_back(new NetworkCommand(nc->getClient(),result));
    m_nrLock.unlock();

    notifyDataAvailable();
}

void NetworkControl::deleteClient(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Client Socket disconnected");
    QMutexLocker locker(&m_clientLock);

    gCoreContext->SendSystemEvent("NET_CTRL_DISCONNECTED");

    for (auto * ncc : std::as_const(m_clients))
    {
        if (ncc->getSocket()->state() == QTcpSocket::UnconnectedState)
        {
            deleteClient(ncc);
            return;
        }
    }
}

void NetworkControl::deleteClient(NetworkControlClient *ncc)
{
    int index = m_clients.indexOf(ncc);
    if (index >= 0)
    {
        m_clients.removeAt(index);

        delete ncc;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("deleteClient(%1), unable to "
                "locate specified NetworkControlClient").arg((long long)ncc));
    }
}

void NetworkControl::newControlConnection(QTcpSocket *client)
{
    QString welcomeStr;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("New connection established."));

    gCoreContext->SendSystemEvent("NET_CTRL_CONNECTED");

    auto *ncc = new NetworkControlClient(client);

    QMutexLocker locker(&m_clientLock);
    m_clients.push_back(ncc);

    connect(ncc, &NetworkControlClient::commandReceived,
            this, &NetworkControl::receiveCommand);
    connect(client, &QAbstractSocket::disconnected,
            this, qOverload<>(&NetworkControl::deleteClient));

    welcomeStr = "MythFrontend Network Control\r\n";
    welcomeStr += "Type 'help' for usage information\r\n"
                  "---------------------------------";
    m_nrLock.lock();
    m_networkControlReplies.push_back(new NetworkCommand(ncc,welcomeStr));
    m_nrLock.unlock();

    notifyDataAvailable();
}

NetworkControlClient::NetworkControlClient(QTcpSocket *s)
  : m_socket(s),
    m_textStream(new QTextStream(s))
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_textStream->setCodec("UTF-8");
#else
    m_textStream->setEncoding(QStringConverter::Utf8);
#endif
    connect(m_socket, &QIODevice::readyRead, this, &NetworkControlClient::readClient);
}

NetworkControlClient::~NetworkControlClient()
{
    m_socket->close();
    m_socket->deleteLater();

    delete m_textStream;
}

void NetworkControlClient::readClient(void)
{
    auto *socket = (QTcpSocket *)sender();
    if (!socket)
        return;

    while (socket->canReadLine())
    {
        QString lineIn = socket->readLine();
#if 0
        static const QRegularExpression badChars
            { "[^-a-zA-Z0-9\\s\\.:_#/$%&()*+,;<=>?\\[\\]\\|]" };
        lineIn.remove(badChars);
#endif

        lineIn = lineIn.simplified();
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
    auto *ncc = qobject_cast<NetworkControlClient *>(sender());
    if (!ncc)
         return;

    m_ncLock.lock();
    m_networkControlCommands.push_back(new NetworkCommand(ncc,command));
    m_ncCond.wakeOne();
    m_ncLock.unlock();
}

QString NetworkControl::processJump(NetworkCommand *nc)
{
    QString result = "OK";

    if ((nc->getArgCount() < 2) || (!m_jumpMap.contains(nc->getArg(1))))
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    GetMythMainWindow()->JumpTo(m_jumpMap[nc->getArg(1)]);

    // Fixme, should do some better checking here, but that would
    // depend on all Locations matching their jumppoints
    QElapsedTimer timer;
    timer.start();
    while (!timer.hasExpired(FE_SHORT_TO) &&
           (GetMythUI()->GetCurrentLocation().toLower() != nc->getArg(1)))
        std::this_thread::sleep_for(10ms);

    return result;
}

QString NetworkControl::processKey(NetworkCommand *nc)
{
    QString result = "OK";
    QKeyEvent *event = nullptr;

    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    QObject *keyDest = nullptr;

    if (GetMythMainWindow())
        keyDest = GetMythMainWindow();
    else
        return {"ERROR: Application has no main window!\n"};

    int curToken = 1;
    while (curToken < nc->getArgCount())
    {
        int tokenLen = nc->getArg(curToken).length();

        if (nc->getArg(curToken) == "sleep")
        {
            std::this_thread::sleep_for(1s);
        }
        else if (m_keyMap.contains(nc->getArg(curToken)))
        {
            int keyCode = m_keyMap[nc->getArg(curToken)];
            QString keyText;

            if (m_keyTextMap.contains(keyCode))
                keyText = m_keyTextMap[keyCode];

            MythMainWindow::ResetScreensaver();

            event = new QKeyEvent(QEvent::KeyPress, keyCode, Qt::NoModifier,
                                  keyText);
            QCoreApplication::postEvent(keyDest, event);

            event = new QKeyEvent(QEvent::KeyRelease, keyCode, Qt::NoModifier,
                                  keyText);
            QCoreApplication::postEvent(keyDest, event);
        }
        else if (((tokenLen == 1) &&
                  (nc->getArg(curToken).at(0).isLetterOrNumber())) ||
                 ((tokenLen >= 1) &&
                  (nc->getArg(curToken).contains("+"))))
        {
            QKeySequence a(nc->getArg(curToken));
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
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
#else
            int keyCode = a[0].key();
            Qt::KeyboardModifiers modifiers = a[0].keyboardModifiers();
#endif
            if (tokenLen == 1)
            {
                if (nc->getArg(curToken) == nc->getArg(curToken).toUpper())
                    modifiers |= Qt::ShiftModifier;
            }

            MythMainWindow::ResetScreensaver();

            event = new QKeyEvent(QEvent::KeyPress, keyCode, modifiers,
                                  nc->getArg(curToken));
            QCoreApplication::postEvent(keyDest, event);

            event = new QKeyEvent(QEvent::KeyRelease, keyCode, modifiers,
                                  nc->getArg(curToken));
            QCoreApplication::postEvent(keyDest, event);
        }
        else
        {
            return QString("ERROR: Invalid syntax at '%1', see 'help %2' for "
                           "usage information")
                           .arg(nc->getArg(curToken), nc->getArg(0));
        }

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
            GetMythMainWindow()->JumpTo(m_jumpMap["mainmenu"]);

            QElapsedTimer timer;
            timer.start();
            while (!timer.hasExpired(FE_LONG_TO) &&
                   (GetMythUI()->GetCurrentLocation().toLower() != "mainmenu"))
                std::this_thread::sleep_for(10ms);
        }

        if (GetMythUI()->GetCurrentLocation().toLower() == "mainmenu")
        {
            QStringList args;
            args << nc->getFrom(2);
            auto *me = new MythEvent(ACTION_HANDLEMEDIA, args);
            qApp->postEvent(GetMythMainWindow(), me);
        }
        else
        {
            return {"Unable to change to main menu to start playback!"};
        }
    }
    else if ((nc->getArgCount() >= 4) &&
             (is_abbrev("program", nc->getArg(1))) &&
             (nc->getArg(2).contains(kChanID1RE)) &&
             (nc->getArg(3).contains(kStartTimeRE)))
    {
        if (GetMythUI()->GetCurrentLocation().toLower() == "playback")
        {
            QString msg = QString("NETWORK_CONTROL STOP");
            MythEvent me(msg);
            gCoreContext->dispatch(me);

            QElapsedTimer timer;
            timer.start();
            while (!timer.hasExpired(FE_LONG_TO) &&
                   (GetMythUI()->GetCurrentLocation().toLower() == "playback"))
                std::this_thread::sleep_for(10ms);
        }

        if (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox")
        {
            GetMythMainWindow()->JumpTo(m_jumpMap["playbackbox"]);

            QElapsedTimer timer;
            timer.start();
            while (!timer.hasExpired(10000) &&
                   (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox"))
                std::this_thread::sleep_for(10ms);

            timer.start();
            while (!timer.hasExpired(10000) && (!MythMainWindow::IsTopScreenInitialized()))
                std::this_thread::sleep_for(10ms);
        }

        if (GetMythUI()->GetCurrentLocation().toLower() == "playbackbox")
        {
            QString action = "PLAY";
            if (nc->getArgCount() == 5 && nc->getArg(4) == "resume")
                action = "RESUME";

            QString msg = QString("NETWORK_CONTROL %1 PROGRAM %2 %3 %4")
                                      .arg(action, nc->getArg(2),
                                           nc->getArg(3).toUpper(),
                                           QString::number(clientID));

            result.clear();
            m_gotAnswer = false;
            QElapsedTimer timer;
            timer.start();

            MythEvent me(msg);
            gCoreContext->dispatch(me);

            while (!timer.hasExpired(FE_LONG_TO) && !m_gotAnswer)
                std::this_thread::sleep_for(10ms);

            if (m_gotAnswer)
                result += m_answer;
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
                m_gotAnswer = false;

                MythEvent me(QString("MUSIC_COMMAND %1 GET_VOLUME").arg(hostname));
                gCoreContext->dispatch(me);

                QElapsedTimer timer;
                timer.start();
                while (!timer.hasExpired(FE_SHORT_TO) && !m_gotAnswer)
                {
                    qApp->processEvents();
                    std::this_thread::sleep_for(10ms);
                }

                if (m_gotAnswer)
                    return m_answer;

                return "unknown";
            }
            else if (is_abbrev("getmeta", nc->getArg(2)))
            {
                m_gotAnswer = false;

                MythEvent me(QString("MUSIC_COMMAND %1 GET_METADATA").arg(hostname));
                gCoreContext->dispatch(me);

                QElapsedTimer timer;
                timer.start();
                while (!timer.hasExpired(FE_SHORT_TO) && !m_gotAnswer)
                {
                    qApp->processEvents();
                    std::this_thread::sleep_for(10ms);
                }

                if (m_gotAnswer)
                    return m_answer;

                return "unknown";
            }
            else if (is_abbrev("getstatus", nc->getArg(2)))
            {
                m_gotAnswer = false;

                MythEvent me(QString("MUSIC_COMMAND %1 GET_STATUS").arg(hostname));
                gCoreContext->dispatch(me);

                QElapsedTimer timer;
                timer.start();
                while (!timer.hasExpired(FE_SHORT_TO) && !m_gotAnswer)
                {
                    qApp->processEvents();
                    std::this_thread::sleep_for(10ms);
                }

                if (m_gotAnswer)
                    return m_answer;

                return "unknown";
            }
            else
            {
                return {"ERROR: Invalid 'play music' command"};
            }
        }
        else if (nc->getArgCount() > 3)
        {
            if (is_abbrev("setvolume", nc->getArg(2)))
            {
                message = QString("MUSIC_COMMAND %1 SET_VOLUME %2")
                                .arg(hostname, nc->getArg(3));
            }
            else if (is_abbrev("track", nc->getArg(2)))
            {
                message = QString("MUSIC_COMMAND %1 PLAY_TRACK %2")
                                .arg(hostname, nc->getArg(3));
            }
            else if (is_abbrev("url", nc->getArg(2)))
            {
                message = QString("MUSIC_COMMAND %1 PLAY_URL %2")
                                .arg(hostname, nc->getArg(3));
            }
            else if (is_abbrev("file", nc->getArg(2)))
            {
                message = QString("MUSIC_COMMAND %1 PLAY_FILE '%2'")
                                .arg(hostname, nc->getFrom(3));
            }
            else
            {
                return {"ERROR: Invalid 'play music' command"};
            }
        }
        else
        {
            return {"ERROR: Invalid 'play music' command"};
        }
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
        if (nc->getArg(2).contains(kChanID1RE))
            message = QString("NETWORK_CONTROL CHANID %1").arg(nc->getArg(2));
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
    }
    else if (is_abbrev("channel", nc->getArg(1), 5))
    {
        static const QRegularExpression kChanID2RE { "^[-\\.\\d_#]+$" };

        if (nc->getArgCount() < 3)
            return "ERROR: See 'help play' for usage information";

        if (is_abbrev("up", nc->getArg(2)))
            message = "NETWORK_CONTROL CHANNEL UP";
        else if (is_abbrev("down", nc->getArg(2)))
            message = "NETWORK_CONTROL CHANNEL DOWN";
        else if (nc->getArg(2).contains(kChanID2RE))
            message = QString("NETWORK_CONTROL CHANNEL %1").arg(nc->getArg(2));
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
    }
    else if (is_abbrev("seek", nc->getArg(1), 2))
    {
        static const QRegularExpression kSeekTimeRE { R"(^\d\d:\d\d:\d\d$)" };

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
        else if (nc->getArg(2).contains(kSeekTimeRE))
        {
            int hours   = nc->getArg(2).mid(0, 2).toInt();
            int minutes = nc->getArg(2).mid(3, 2).toInt();
            int seconds = nc->getArg(2).mid(6, 2).toInt();
            message = QString("NETWORK_CONTROL SEEK POSITION %1")
                              .arg((hours * 3600) + (minutes * 60) + seconds);
        }
        else
        {
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
        }
    }
    else if (is_abbrev("speed", nc->getArg(1), 2))
    {
        static const QRegularExpression kSpeed1RE { R"(^\-*\d+x$)" };
        static const QRegularExpression kSpeed2RE { R"(^\-*\d+\/\d+x$)" };
        static const QRegularExpression kSpeed3RE { R"(^\-*\d*\.\d+x$)" };

        if (nc->getArgCount() < 3)
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));

        QString token2 = nc->getArg(2).toLower();
        if ((token2.contains(kSpeed1RE)) ||
            (token2.contains(kSpeed2RE)) ||
            (token2.contains(kSpeed3RE)))
            message = QString("NETWORK_CONTROL SPEED %1").arg(token2);
        else if (is_abbrev("normal", token2))
            message = QString("NETWORK_CONTROL SPEED normal");
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
    {
        message = QString("NETWORK_CONTROL STOP");
    }
    else if (is_abbrev("volume", nc->getArg(1), 2))
    {
        static const QRegularExpression kVolumeRE { "^\\d+%?$" };

        if ((nc->getArgCount() < 3) ||
            (!nc->getArg(2).toLower().contains(kVolumeRE)))
        {
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(nc->getArg(0));
        }

        message = QString("NETWORK_CONTROL VOLUME %1")
                          .arg(nc->getArg(2).toLower());
    }
    else if (is_abbrev("subtitles", nc->getArg(1), 2))
    {
        static const QRegularExpression kNumberRE { "^\\d+$" };
        if (nc->getArgCount() < 3)
            message = QString("NETWORK_CONTROL SUBTITLES 0");
        else if (!nc->getArg(2).toLower().contains(kNumberRE))
        {
            return QString("ERROR: See 'help %1' for usage information")
                .arg(nc->getArg(0));
        }
        else
        {
            message = QString("NETWORK_CONTROL SUBTITLES %1")
                .arg(nc->getArg(2));
        }
    }
    else
    {
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));
    }

    if (!message.isEmpty())
    {
        // Don't broadcast this event as the TV object will see it
        // twice: once directly from the Qt event system, and once
        // because its filtering all events before they are sent to
        // the main window.  Its much easier to get a pointer to the
        // MainWindow object than is is to a pointer to the TV object,
        // so send the event directly there.  The TV object will get
        // it anyway because because of the filter hook.  (The last
        // third of this function requires you to be in playback mode
        // so the TV object is guaranteed to exist.)
        auto *me = new MythEvent(message);
        qApp->postEvent(GetMythMainWindow(), me);
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
            m_gotAnswer = false;
            QString message = QString("NETWORK_CONTROL QUERY POSITION");
            MythEvent me(message);
            gCoreContext->dispatch(me);

            QElapsedTimer timer;
            timer.start();
            while (!timer.hasExpired(FE_SHORT_TO) && !m_gotAnswer)
                std::this_thread::sleep_for(10ms);

            if (m_gotAnswer)
                result += m_answer;
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
        return listSchedule();
    }
    else if (is_abbrev("version", nc->getArg(1)))
    {
        int dbSchema = gCoreContext->GetNumSetting("DBSchemaVer");

        return QString("VERSION: %1/%2 %3 %4 QT/%5 DBSchema/%6")
                       .arg(GetMythSourceVersion(),
                            GetMythSourcePath(),
                            MYTH_BINARY_VERSION,
                            MYTH_PROTO_VERSION,
                            QT_VERSION_STR,
                            QString::number(dbSchema));

    }
    else if(is_abbrev("time", nc->getArg(1)))
    {
        return MythDate::current_iso_string();
    }
    else if (is_abbrev("uptime", nc->getArg(1)))
    {
        QString str;
        std::chrono::seconds uptime = 0s;

        if (getUptime(uptime))
            str = QString::number(uptime.count());
        else
            str = QString("Could not determine uptime.");
        return str;
    }
    else if (is_abbrev("load", nc->getArg(1)))
    {
        QString str;
        loadArray loads = getLoadAvgs();
        if (loads[0] == -1)
            str = QString("getloadavg() failed");
        else
            str = QString("%1 %2 %3").arg(loads[0]).arg(loads[1]).arg(loads[2]);
        return str;
    }
    else if (is_abbrev("memstats", nc->getArg(1)))
    {
        QString str;
        int     totalMB = 0;
        int     freeMB = 0;
        int     totalVM = 0;
        int     freeVM = 0;

        if (getMemStats(totalMB, freeMB, totalVM, freeVM))
        {
            str = QString("%1 %2 %3 %4")
                          .arg(totalMB).arg(freeMB).arg(totalVM).arg(freeVM);
        }
        else
        {
            str = QString("Could not determine memory stats.");
        }
        return str;
    }
    else if (is_abbrev("volume", nc->getArg(1)))
    {
        QString str = "0%";

        QString location = GetMythUI()->GetCurrentLocation(false, false);

        if (location != "Playback")
            return str;

        m_gotAnswer = false;
        QString message = QString("NETWORK_CONTROL QUERY VOLUME");
        MythEvent me(message);
        gCoreContext->dispatch(me);

        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(FE_SHORT_TO) && !m_gotAnswer)
            std::this_thread::sleep_for(10ms);

        if (m_gotAnswer)
            str = m_answer;
        else
            str = "ERROR: Timed out waiting for reply from player";

        return str;
    }
    else if ((nc->getArgCount() == 4) &&
             is_abbrev("recording", nc->getArg(1)) &&
             (nc->getArg(2).contains(kChanID1RE)) &&
             (nc->getArg(3).contains(kStartTimeRE)))
    {
        return listRecordings(nc->getArg(2), nc->getArg(3).toUpper());
    }
    else if (is_abbrev("recordings", nc->getArg(1)))
    {
        return listRecordings();
    }
    else if (is_abbrev("channels", nc->getArg(1)))
    {
        if (nc->getArgCount() == 2)
            return listChannels(0, 0);  // give us all you can
        if (nc->getArgCount() == 4)
            return listChannels(nc->getArg(2).toLower().toUInt(),
                                nc->getArg(3).toLower().toUInt());
        return QString("ERROR: See 'help %1' for usage information "
                       "(parameters mismatch)").arg(nc->getArg(0));
    }
    else
    {
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));
    }

    return result;
}

QString NetworkControl::processSet(NetworkCommand *nc)
{
    if (nc->getArgCount() == 1)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    if (nc->getArg(1) == "verbose")
    {
        if (nc->getArgCount() < 3)
            return {"ERROR: Missing filter name."};

        if (nc->getArgCount() > 3)
        {
            return QString("ERROR: Separate filters with commas with no "
                           "space: playback,audio\r\n See 'help %1' for usage "
                           "information").arg(nc->getArg(0));
        }

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

QString NetworkControl::getWidgetType(MythUIType* type)
{
    if (dynamic_cast<MythUIText *>(type))
        return "MythUIText";
    if (dynamic_cast<MythUITextEdit *>(type))
        return "MythUITextEdit";
    if (dynamic_cast<MythUIGroup *>(type))
        return "MythUIGroup";
    if (dynamic_cast<MythUIButton *>(type))
        return "MythUIButton";
    if (dynamic_cast<MythUICheckBox *>(type))
        return "MythUICheckBox";
    if (dynamic_cast<MythUIShape *>(type))
        return "MythUIShape";
    if (dynamic_cast<MythUIButtonList *>(type))
        return "MythUIButtonList";
    if (dynamic_cast<MythUIImage *>(type))
        return "MythUIImage";
    if (dynamic_cast<MythUISpinBox *>(type))
        return "MythUISpinBox";
#if CONFIG_QTWEBKIT
    if (dynamic_cast<MythUIWebBrowser *>(type))
        return "MythUIWebBrowser";
#endif
    if (dynamic_cast<MythUIClock *>(type))
        return "MythUIClock";
    if (dynamic_cast<MythUIStateType *>(type))
        return "MythUIStateType";
    if (dynamic_cast<MythUIProgressBar *>(type))
        return "MythUIProgressBar";
    if (dynamic_cast<MythUIButtonTree *>(type))
        return "MythUIButtonTree";
    if (dynamic_cast<MythUIScrollBar *>(type))
        return "MythUIScrollBar";
    if (dynamic_cast<MythUIVideo *>(type))
        return "MythUIVideo";
    if (dynamic_cast<MythUIGuideGrid *>(type))
        return "MythUIGuideGrid";
    if (dynamic_cast<MythUIEditBar *>(type))
        return "MythUIEditBar";

    return "Unknown";
}

QString NetworkControl::processTheme( NetworkCommand* nc)
{
    if (nc->getArgCount() == 1)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(nc->getArg(0));

    if (nc->getArg(1) == "getthemeinfo")
    {
        QString themeName = GetMythUI()->GetThemeName();
        QString themeDir = GetMythUI()->GetThemeDir();
        return QString("%1 - %2").arg(themeName, themeDir);
    }
    if (nc->getArg(1) == "reload")
    {
        GetMythMainWindow()->JumpTo(m_jumpMap["reloadtheme"]);

        return "OK";
    }
    if (nc->getArg(1) == "showborders")
    {
        GetMythMainWindow()->JumpTo(m_jumpMap["showborders"]);

        return "OK";
    }
    if (nc->getArg(1) == "shownames")
    {
        GetMythMainWindow()->JumpTo(m_jumpMap["shownames"]);

        return "OK";
    }
    if (nc->getArg(1) == "getwidgetnames")
    {
        QStringList path;

        if (nc->getArgCount() >= 3)
            path = nc->getArg(2).split('/');

        MythScreenStack *stack = GetMythMainWindow()->GetStack("popup stack");
        MythScreenType *topScreen = stack->GetTopScreen();

        if (!topScreen)
        {
            stack = GetMythMainWindow()->GetMainStack();
            topScreen = stack->GetTopScreen();
        }

        if (!topScreen)
            return {"ERROR: no top screen found!"};

        MythUIType *currType = topScreen;

        while (!path.isEmpty())
        {
            QString childName = path.takeFirst();
            currType = currType->GetChild(childName);
            if (!currType)
                return QString("ERROR: Failed to find child '%1'").arg(childName);
        }

        QList<MythUIType*> *children = currType->GetAllChildren();
        QString result;

        for (int i = 0; i < children->count(); i++)
        {
            MythUIType *type = children->at(i);
            QString widgetName = type->objectName();
            QString widgetType = getWidgetType(type);
            result += QString("%1 - %2\n\r").arg(widgetName, -20).arg(widgetType);
        }

        return result;
    }
    if (nc->getArg(1) == "getarea")
    {
        if (nc->getArgCount() < 3)
            return {"ERROR: Missing widget name."};

        QString widgetName = nc->getArg(2);
        QStringList path = widgetName.split('/');

        MythScreenStack *stack = GetMythMainWindow()->GetStack("popup stack");
        MythScreenType *topScreen = stack->GetTopScreen();

        if (!topScreen)
        {
            stack = GetMythMainWindow()->GetMainStack();
            topScreen = stack->GetTopScreen();
        }

        if (!topScreen)
            return {"ERROR: no top screen found!"};

        MythUIType *currType = topScreen;

        while (path.count() > 1)
        {
            QString childName = path.takeFirst();
            currType = currType->GetChild(childName);
            if (!currType)
                return QString("ERROR: Failed to find child '%1'").arg(childName);
        }

        MythUIType* type = currType->GetChild(path.first());
        if (!type)
            return QString("ERROR: widget '%1' not found!").arg(widgetName);

        int x = type->GetFullArea().x();
        int y = type->GetFullArea().y();
        int w = type->GetFullArea().width();
        int h = type->GetFullArea().height();
        return QString("The area of '%1' is x:%2, y:%3, w:%4, h:%5")
                       .arg(widgetName).arg(x).arg(y).arg(w).arg(h);
    }
    if (nc->getArg(1) == "setarea")
    {
        if (nc->getArgCount() < 3)
            return {"ERROR: Missing widget name."};

        if (nc->getArgCount() < 7)
            return {"ERROR: Missing X, Y, Width or Height."};

        QString widgetName = nc->getArg(2);
        QStringList path = widgetName.split('/');
        QString x = nc->getArg(3);
        QString y = nc->getArg(4);
        QString w = nc->getArg(5);
        QString h = nc->getArg(6);

        MythScreenStack *stack = GetMythMainWindow()->GetStack("popup stack");
        MythScreenType *topScreen = stack->GetTopScreen();

        if (!topScreen)
        {
            stack = GetMythMainWindow()->GetMainStack();
            topScreen = stack->GetTopScreen();
        }

        MythUIType *currType = topScreen;
        if (!topScreen)
            return {"ERROR: no top screen found!"};

        while (path.count() > 1)
        {
            QString childName = path.takeFirst();
            currType = currType->GetChild(childName);
            if (!currType)
                return QString("ERROR: Failed to find child '%1'").arg(childName);
        }

        MythUIType* type = currType->GetChild(path.first());
        if (!type)
            return QString("ERROR: widget '%1' not found!").arg(widgetName);

        type->SetArea(MythRect(x, y, w, h));

        return QString("Changed area of '%1' to x:%2, y:%3, w:%4, h:%5")
                       .arg(widgetName, x, y, w, h);
    }

    return QString("ERROR: See 'help %1' for usage information")
                   .arg(nc->getArg(0));
}

QString NetworkControl::processHelp(NetworkCommand *nc)
{
    QString command;
    QString helpText;

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

        for (it = m_jumpMap.begin(); it != m_jumpMap.end(); ++it)
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
        for (it = m_keyMap.begin(); it != m_keyMap.end(); ++it)
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
            "play music getstatus   - Get music player status playing/paused/stopped (MythMusic)\r\n"
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
    else if (is_abbrev("theme", command))
    {
        helpText +=
            "theme getthemeinfo               - Display the name and location of the current theme\r\n"
            "theme reload                     - Reload the theme\r\n"
            "theme showborders                - Toggle showing widget borders\r\n"
            "theme shownames ON/OFF           - Toggle showing widget names\r\n"
            "theme getwidgetnames PATH        - Display the name and type of all the child widgets from PATH\r\n"
            "theme getarea WIDGETNAME         - Get the area of widget WIDGET on the active screen\r\n"
            "theme setarea WIDGETNAME X Y W H - Change the area of widget WIDGET to X Y W H on the active screen\r\n";
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
        "theme              - Theme related commands\r\n"
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
    auto* me = new MythEvent(MythEvent::kMythUserMessage, message);
    qApp->postEvent(window, me);
    return {"OK"};
}

QString NetworkControl::processNotification(NetworkCommand *nc)
{
    if (nc->getArgCount() < 2)
        return QString("ERROR: See 'help %1' for usage information")
        .arg(nc->getArg(0));

    QString message = nc->getCommand().remove(0, 12).trimmed();
    MythNotification n(message, tr("Network Control"));
    GetNotificationCenter()->Queue(n);
    return {"OK"};
}

void NetworkControl::notifyDataAvailable(void)
{
    QCoreApplication::postEvent(
        this, new QEvent(kNetworkControlDataReadyEvent));
}

void NetworkControl::sendReplyToClient(NetworkControlClient *ncc,
                                       const QString &reply)
{
    if (!m_clients.contains(ncc))
    {
        // NetworkControl instance is unaware of control client
        // assume connection to client has been terminated and bail
        return;
    }

    static const QRegularExpression crlfRegEx("\r\n$");
    static const QRegularExpression crlfcrlfRegEx("\r\n.*\r\n");

    QTcpSocket  *client = ncc->getSocket();
    QTextStream *clientStream = ncc->getTextStream();

    if (client && clientStream && client->state() == QTcpSocket::ConnectedState)
    {
        *clientStream << reply;

        if ((!reply.contains(crlfRegEx)) ||
            ( reply.contains(crlfcrlfRegEx)))
            *clientStream << "\r\n" << m_prompt;

        clientStream->flush();
        client->flush();
    }
}

void NetworkControl::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;

        const QString& message = me->Message();

        if (message.startsWith("MUSIC_CONTROL"))
        {
            QStringList tokens = message.simplified().split(" ");
            if ((tokens.size() >= 4) &&
                (tokens[1] == "ANSWER") &&
                (tokens[2] == gCoreContext->GetHostName()))
            {
                m_answer = tokens[3];
                for (int i = 4; i < tokens.size(); i++)
                    m_answer += QString(" ") + tokens[i];
                m_gotAnswer = true;
            }

        }
        else if (message.startsWith("NETWORK_CONTROL"))
        {
            QStringList tokens = message.simplified().split(" ");
            if ((tokens.size() >= 3) &&
                (tokens[1] == "ANSWER"))
            {
                m_answer = tokens[2];
                for (int i = 3; i < tokens.size(); i++)
                    m_answer += QString(" ") + tokens[i];
                m_gotAnswer = true;
            }
            else if ((tokens.size() >= 4) &&
                     (tokens[1] == "RESPONSE"))
            {
//                int clientID = tokens[2].toInt();
                m_answer = tokens[3];
                for (int i = 4; i < tokens.size(); i++)
                    m_answer += QString(" ") + tokens[i];
                m_gotAnswer = true;
            }
        }
    }
    else if (e->type() == kNetworkControlDataReadyEvent)
    {
        QString reply;

        QMutexLocker locker(&m_clientLock);
        QMutexLocker nrLocker(&m_nrLock);

        while (!m_networkControlReplies.isEmpty())
        {
            NetworkCommand *nc = m_networkControlReplies.front();
            m_networkControlReplies.pop_front();

            reply = nc->getCommand();

            NetworkControlClient * ncc = nc->getClient();
            if (ncc)
            {
                sendReplyToClient(ncc, reply);
            }
            else //send to all clients
            {
                for (auto * ncc2 : std::as_const(m_clients))
                {
                    if (ncc2)
                        sendReplyToClient(ncc2, reply);
                }
            }
            delete nc;
        }
    }
    else if (e->type() == NetworkControlCloseEvent::kEventType)
    {
        auto *ncce = dynamic_cast<NetworkControlCloseEvent*>(e);
        if (ncce == nullptr)
            return;
        
        NetworkControlClient *ncc  = ncce->getClient();
        deleteClient(ncc);
    }
}

QString NetworkControl::listSchedule(const QString& chanID)
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
                .arg(QString::number(query.value(0).toInt()).rightJustified(5, ' '),
                     MythDate::as_utc(query.value(1).toDateTime()).toString(Qt::ISODate),
                     MythDate::as_utc(query.value(2).toDateTime()).toString(Qt::ISODate),
                     atitle);

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

QString NetworkControl::listRecordings(const QString& chanid, const QString& starttime)
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
        QString episode;
        QString title;
        QString subtitle;
        while (query.next())
        {
            title = query.value(2).toString();
            subtitle = query.value(3).toString();

            if (!subtitle.isEmpty())
            {
                episode = QString("%1 -\"%2\"").arg(title, subtitle);
            }
            else
            {
                episode = title;
            }

            result +=
                QString("%1 %2 %3")
                .arg(query.value(0).toString(),
                     MythDate::as_utc(query.value(1).toDateTime()).toString(Qt::ISODate),
                     episode);

            if (appendCRLF)
                result += "\r\n";
        }
    }
    else
    {
        result = "ERROR: Unable to retrieve recordings list.";
    }

    return result;
}

QString NetworkControl::listChannels(const uint start, const uint limit)
{
    QString result;
    MSqlQuery query(MSqlQuery::InitCon());
    QString queryStr;
    uint sqlStart = start;

    // sql starts at zero, we want to start at 1
    if (sqlStart > 0)
        sqlStart--;

    queryStr = "select chanid, callsign, name from channel "
        "where deleted IS NULL and visible > 0 "
        "ORDER BY callsign";

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

    uint maxcnt = query.size();
    uint cnt = 0;
    if (maxcnt == 0)    // Feedback we have no usefull information
    {
        result += QString(R"(0:0 0 "Invalid" "Invalid")");
        return result;
    }

    while (query.next())
    {
        // Feedback is as follow:
        // <current line count>:<max line count to expect> <channelid> <callsign name> <channel name>\r\n
        cnt++;
        result += QString("%1:%2 %3 \"%4\" \"%5\"\r\n")
                          .arg(cnt).arg(maxcnt)
                          .arg(query.value(0).toString(),
                               query.value(1).toString(),
                               query.value(2).toString());
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
    auto *me = new MythEvent(MythEvent::kMythEventMessage,
                                  ACTION_SCREENSHOT, args);
    qApp->postEvent(window, me);
    return "OK";
}

QString NetworkCommand::getFrom(int arg)
{
    QString c = m_command;
    for(int i=0 ; i<arg ; i++) {
        QString argstr = c.simplified().split(" ")[0];
        c = c.mid(argstr.length()).trimmed();
    }
    return c;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
