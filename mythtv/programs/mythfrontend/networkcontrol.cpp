#include <unistd.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qtextstream.h>

#include "libmyth/mythcontext.h"
#include "libmyth/mythdialogs.h"
#include "networkcontrol.h"
#include "programinfo.h"
#include "remoteutil.h"

#define LOC QString("NetworkControl: ")
#define LOC_ERR QString("NetworkControl Error: ")

const int kNetworkControlDataReadyEvent = 35671;
const int kNetworkControlCloseEvent     = 35672;

NetworkControl::NetworkControl(int port)
          : QServerSocket(port, 1),
            prompt("# "),
            gotAnswer(false), answer(""),
            client(NULL), cs(NULL)
{
    VERBOSE(VB_IMPORTANT, LOC +
            QString("Listening for remote connections on port %1").arg(port));

    // Eventually this map should be in the jumppoints table
    jumpMap["channelpriorities"]     = "Channel Recording Priorities";
    jumpMap["livetv"]                = "Live TV";
    jumpMap["livetvinguide"]         = "Live TV In Guide";
    jumpMap["mainmenu"]              = "Main Menu";
    jumpMap["managerecordings"]      = "Manage Recordings / Fix Conflicts";
    jumpMap["manualrecording"]       = "Manual Record Scheduling";
    jumpMap["mythgallery"]           = "MythGallery";
    jumpMap["mythmovietime"]         = "MythMovieTime";
    jumpMap["mythvideo"]             = "MythVideo";
    jumpMap["mythweather"]           = "MythWeather";
    jumpMap["playdvd"]               = "Play DVD";
    jumpMap["playmusic"]             = "Play music";
    jumpMap["programfinder"]         = "Program Finder";
    jumpMap["programguide"]          = "Program Guide";
    jumpMap["recordingpriorities"]   = "Program Recording Priorities";
    jumpMap["ripcd"]                 = "Rip CD";
    jumpMap["musicplaylists"]        = "Select music playlists";
    jumpMap["deleterecordings"]      = "TV Recording Deletion";
    jumpMap["playbackrecordings"]    = "TV Recording Playback";
    jumpMap["videobrowser"]          = "Video Browser";
    jumpMap["videogallery"]          = "Video Gallery";
    jumpMap["videolistings"]         = "Video Listings";
    jumpMap["videomanager"]          = "Video Manager";

    // These jump point names match the (lowercased) locations from gContext
    jumpMap["channelrecpriority"]    = "Channel Recording Priorities";
    jumpMap["viewscheduled"]         = "Manage Recordings / Fix Conflicts";
    jumpMap["manualbox"]             = "Manual Record Scheduling";
    jumpMap["previousbox"]           = "Previously Recorded";
    jumpMap["progfinder"]            = "Program Finder";
    jumpMap["guidegrid"]             = "Program Guide";
    jumpMap["programrecpriority"]    = "Program Recording Priorities";
    jumpMap["statusbox"]             = "Status Screen";
    jumpMap["deletebox"]             = "TV Recording Deletion";
    jumpMap["playbackbox"]           = "TV Recording Playback";

    keyMap["up"]                     = Qt::Key_Up;
    keyMap["down"]                   = Qt::Key_Down;
    keyMap["left"]                   = Qt::Key_Left;
    keyMap["right"]                  = Qt::Key_Right;
    keyMap["enter"]                  = Qt::Key_Enter;
    keyMap["pageup"]                 = Qt::Key_Prior;
    keyMap["pagedown"]               = Qt::Key_Next;
    keyMap["escape"]                 = Qt::Key_Escape;
    keyMap["space"]                  = Qt::Key_Space;
    keyMap["backspace"]              = Qt::Key_Backspace;
    keyMap["bracketleft"]            = Qt::Key_BracketLeft;
    keyMap["bracketright"]           = Qt::Key_BracketRight;
    keyMap["backslash"]              = Qt::Key_Backslash;
    keyMap["slash"]                  = Qt::Key_Slash;
    keyMap["colon"]                  = Qt::Key_Colon;
    keyMap["semicolon"]              = Qt::Key_Semicolon;
    keyMap["less"]                   = Qt::Key_Less;
    keyMap["equal"]                  = Qt::Key_Equal;
    keyMap["greater"]                = Qt::Key_Greater;
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

    runCommandThread = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&command_thread, &attr, CommandThread, this);

    gContext->addListener(this);
}

NetworkControl::~NetworkControl(void)
{
    nrLock.lock();
    networkControlReplies.push_back(
        "mythfrontend shutting down, connection closing...");
    nrLock.unlock();

    notifyDataAvailable();

    runCommandThread = false;

    while (commandThreadRunning)
        usleep(1000);

    pthread_join(command_thread, NULL);
}

void *NetworkControl::CommandThread(void *param)
{
    NetworkControl *networkControl = (NetworkControl *)param;
    networkControl->RunCommandThread();

    return NULL;
}

void NetworkControl::RunCommandThread(void)
{
    QString command;
    int commands = 0;
    commandThreadRunning = true;

    while(runCommandThread)
    {
        ncLock.lock();
        commands = networkControlCommands.size();
        ncLock.unlock();

        while (commands)
        {
            ncLock.lock();
            command = networkControlCommands.front();
            networkControlCommands.pop_front();
            ncLock.unlock();

            processNetworkControlCommand(command);

            ncLock.lock();
            commands = networkControlCommands.size();
            ncLock.unlock();
        }

        usleep(50000);
    }

    commandThreadRunning = false;
}

void NetworkControl::processNetworkControlCommand(QString command)
{
    QMutexLocker locker(&clientLock);
    QString result = "";
    QStringList tokens = QStringList::split(" ", command);

    tokens[0] = tokens[0].lower();

    if (tokens[0] == "jump")
        result = processJump(tokens);
    else if (tokens[0] == "key")
        result = processKey(tokens);
    else if (tokens[0] == "play")
        result = processPlay(tokens);
    else if (tokens[0] == "query")
        result = processQuery(tokens);
    else if (tokens[0] == "help")
        result = processHelp(tokens);
    else if ((tokens[0] == "exit") || (tokens[0] == "quit"))
        QApplication::postEvent(this,
                                new QCustomEvent(kNetworkControlCloseEvent));
    else
        result = QString("INVALID command '%1', try 'help' for more info")
                         .arg(tokens[0]);

    if (result != "")
    {
        nrLock.lock();
        networkControlReplies.push_back(result);
        nrLock.unlock();

        notifyDataAvailable();
    }
}

void NetworkControl::newConnection(int socket)
{
    QString welcomeStr = "";
    bool closedOldConn = false;
    QSocket *s = new QSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    connect(s, SIGNAL(connectionClosed()), this, SLOT(discardClient()));
    s->setSocket(socket);

    VERBOSE(VB_IMPORTANT, LOC +
            QString("New connection established. (%1)").arg(socket));

    QTextStream *os = new QTextStream(s);
    os->setEncoding(QTextStream::UnicodeUTF8);

    QMutexLocker locker(&clientLock);
    if (client)
    {
        closedOldConn = true;
        client->close();
        delete client;
        delete cs;
    }

    client = s;
    cs = os;

    networkControlCommands.clear();
    networkControlReplies.clear();

    welcomeStr = "MythFrontend Network Control\r\n";
    if (closedOldConn)
    {
        welcomeStr +=
            "WARNING: mythfrontend was already under network control.\r\n";
        welcomeStr +=
            "         Previous session is being disconnected.\r\n";
    }

    welcomeStr += "Type 'help' for usage information\r\n"
                  "---------------------------------";
    nrLock.lock();
    networkControlReplies.push_back(welcomeStr);
    nrLock.unlock();

    notifyDataAvailable();
}

void NetworkControl::readClient(void)
{
    QSocket *socket = (QSocket *)sender();
    if (!socket)
        return;

    QString lineIn;
    QStringList tokens;
    while (socket->canReadLine())
    {
        lineIn = socket->readLine();
        lineIn.replace(QRegExp("[\r\n]"), "");

        tokens = QStringList::split(" ", lineIn);
        if (tokens[0].lower() != "key")
            lineIn = lineIn.lower();

        ncLock.lock();
        networkControlCommands.push_back(lineIn);
        ncLock.unlock();
    }
}

void NetworkControl::discardClient(void)
{
    QSocket *socket = (QSocket *)sender();
    if (!socket)
        return;

    QMutexLocker locker(&clientLock);
    if (client == socket)
    {
        delete cs;
        delete client;
        client = NULL;
        cs = NULL;
    }
    else
        delete socket;
}

QString NetworkControl::processJump(QStringList tokens)
{
    QString result = "OK";

    if ((tokens.size() < 2) || (!jumpMap.contains(tokens[1])))
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);

    gContext->GetMainWindow()->JumpTo(jumpMap[tokens[1]]);

    // Fixme, should do some better checking here, but that would
    // depend on all Locations matching their jumppoints
    QTime timer;
    timer.start();
    while ((timer.elapsed() < 2000) &&
           (gContext->getCurrentLocation().lower() != tokens[1]))
        usleep(10000);

    return result;
}

QString NetworkControl::processKey(QStringList tokens)
{
    QString result = "OK";
    QKeyEvent *event = NULL;

    if ((tokens.size() < 2) ||
        (!keyMap.contains(tokens[1]) && (tokens[1].length() > 1)))
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);

    QWidget *widget = gContext->GetMainWindow()->currentWidget()->focusWidget();
    if (keyMap.contains(tokens[1]))
    {
        event = new QKeyEvent(QEvent::KeyPress, keyMap[tokens[1]], 0, NoButton);
        QApplication::postEvent(widget, event);

        event = new QKeyEvent(QEvent::KeyRelease, keyMap[tokens[1]], 0, NoButton);
        QApplication::postEvent(widget, event);
    }
    else if (tokens[1].length() == 1 && tokens[1][0].isLetterOrNumber())
    {
        QKeySequence a(tokens[1]);
        int keyCode = a[0];
        int ch = (tokens[1].ascii())[0];
        int buttons = NoButton;

        if (tokens[1] == tokens[1].upper())
            buttons = ShiftButton;

        event = new QKeyEvent(QEvent::KeyPress, keyCode, ch, buttons,
                              tokens[1]);
        QApplication::postEvent(widget, event);

        event = new QKeyEvent(QEvent::KeyRelease, keyCode, ch, buttons,
                              tokens[1]);
        QApplication::postEvent(widget, event);
    }
    else
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);


    return result;
}

QString NetworkControl::processPlay(QStringList tokens)
{
    QString result = "OK";
    QString message = "";

    if (tokens.size() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);

    if ((tokens.size() >= 4) &&
        (tokens[1] == "program") &&
        (tokens[2].contains(QRegExp("^\\d+$"))) &&
        (tokens[3].contains(QRegExp(
                         "^\\d\\d\\d\\d-\\d\\d-\\d\\dt\\d\\d:\\d\\d:\\d\\d$"))))
    {
        if (gContext->getCurrentLocation() == "Playback")
        {
            QString message = QString("NETWORK_CONTROL STOP");
            MythEvent me(message);
            gContext->dispatch(me);

            QTime timer;
            timer.start();
            while ((timer.elapsed() < 10000) &&
                   (gContext->getCurrentLocation() == "Playback"))
                usleep(10000);
        }

        if (gContext->getCurrentLocation() != "PlaybackBox")
        {
            gContext->GetMainWindow()->JumpTo(jumpMap["playbackbox"]);

            QTime timer;
            timer.start();
            while ((timer.elapsed() < 10000) &&
                   (gContext->getCurrentLocation() != "PlaybackBox"))
                usleep(10000);
        }

        if (gContext->getCurrentLocation() == "PlaybackBox")
        {
            QString action = "PLAY";
            if (tokens.size() == 5 && tokens[4] == "resume")
                action = "RESUME";

            QString message = QString("NETWORK_CONTROL %1 PROGRAM %2 %3")
                                      .arg(action).arg(tokens[2])
                                      .arg(tokens[3].upper());
            MythEvent me(message);
            gContext->dispatch(me);

            result = "";
        }
        else
        {
            result = QString("ERROR: Unable to change to PlaybackBox from "
                             "%1, can not play requested file.")
                             .arg(gContext->getCurrentLocation());
        }
    }
    // Everything below here requires us to be in playback mode so check to
    // see if we are
    else if (gContext->getCurrentLocation().lower() != "playback")
    {
        return QString("ERROR: You are in %1 mode and this command is only "
                       "for playback mode")
                       .arg(gContext->getCurrentLocation());
    }
    else if (tokens[1] == "chanid")
    {
        if (tokens[2].contains(QRegExp("^\\d+$")))
            message = QString("NETWORK_CONTROL CHANID %1").arg(tokens[2]);
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(tokens[0]);
    }
    else if (tokens[1] == "channel")
    {
        if (tokens.size() < 3)
            return "ERROR: See 'help play' for usage information";

        if (tokens[2] == "up")
            message = "NETWORK_CONTROL CHANNEL UP";
        else if (tokens[2] == "down")
            message = "NETWORK_CONTROL CHANNEL DOWN";
        else if (tokens[2].contains(QRegExp("^[-\\.\\d_#]+$")))
            message = QString("NETWORK_CONTROL CHANNEL %1").arg(tokens[2]);
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(tokens[0]);
    }
    else if (tokens[1] == "seek")
    {
        if (tokens.size() < 3)
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(tokens[0]);

        if (tokens[2] == "beginning")
            message = "NETWORK_CONTROL SEEK BEGINNING";
        else if (tokens[2] == "forward")
            message = "NETWORK_CONTROL SEEK FORWARD";
        else if (tokens[2] == "rewind")
            message = "NETWORK_CONTROL SEEK BACKWARD";
        else if (tokens[2].contains(QRegExp("^\\d\\d:\\d\\d:\\d\\d$")))
        {
            int hours = tokens[2].left(0).toInt();
            int minutes = tokens[2].mid(3, 2).toInt();
            int seconds = tokens[2].mid(6, 2).toInt();
            message = QString("NETWORK_CONTROL SEEK POSITION %1")
                              .arg((hours * 3600) + (minutes * 60) + seconds);
        }
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(tokens[0]);
    }
    else if (tokens[1] == "speed")
    {
        if (tokens.size() < 3)
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(tokens[0]);

        if ((tokens[2].contains(QRegExp("^\\-*\\d+x$"))) ||
            (tokens[2].contains(QRegExp("^\\-*\\d+\\/\\d+x$"))) ||
            (tokens[2].contains(QRegExp("^\\d*\\.\\d+x$"))))
            message = QString("NETWORK_CONTROL SPEED %1").arg(tokens[2]);
        else if (tokens[2] == "normal")
            message = QString("NETWORK_CONTROL SPEED 1x");
        else if (tokens[2] == "pause")
            message = QString("NETWORK_CONTROL SPEED 0x");
        else
            return QString("ERROR: See 'help %1' for usage information")
                           .arg(tokens[0]);
    }
    else if (tokens[1] == "stop")
        message = QString("NETWORK_CONTROL STOP");
    else
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);

    if (message != "")
    {
        MythEvent me(message);
        gContext->dispatch(me);
    }

    return result;
}

QString NetworkControl::processQuery(QStringList tokens)
{
    QString result = "OK";

    if (tokens.size() < 2)
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);

    if (tokens[1] == "location")
    {
        QString location = gContext->getCurrentLocation();
        result = location;

        // if we're playing something, then find out what
        if (location == "Playback")
        {
            result += " ";
            gotAnswer = false;
            QString message = QString("NETWORK_CONTROL QUERY POSITION");
            MythEvent me(message);
            gContext->dispatch(me);

            QTime timer;
            timer.start();
            while (timer.elapsed() < 2000  && !gotAnswer)
                usleep(10000);

            if (gotAnswer)
                result += answer;
            else
                result += "ERROR: Timed out waiting for reply from player";
        }
    }
    else if (tokens[1] == "recordings")
        return listRecordings();
    else
        return QString("ERROR: See 'help %1' for usage information")
                       .arg(tokens[0]);

    return result;
}

QString NetworkControl::processHelp(QStringList tokens)
{
    QString command = "";
    QString helpText = "";

    if (tokens.size() >= 1)
    {
        if (tokens[0] == "help")
        {
            if (tokens.size() >= 2)
                command = tokens[1];
            else
                command = "";
        }
        else
        {
            command = tokens[0];
        }
    }
        
    if (command == "jump")
    {
        QMap<QString, QString>::Iterator it;
        helpText +=
            "Usage: jump JUMPPOINT\r\n"
            "\r\n"
            "Where JUMPPOINT is one of the following:\r\n";

        for (it = jumpMap.begin(); it != jumpMap.end(); ++it)
        {
            helpText += it.key().leftJustify(20, ' ', true) + " - " +
                        it.data() + "\r\n";
        }
    }
    else if (command == "key")
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
    else if (command == "play")
    {
        helpText +=
            "play channel up       - Change channel Up\r\n"
            "play channel down     - Change channel Down\r\n"
            "play channel NUMBER   - Change to a specific channel number\r\n"
            "play chanid NUMBER    - Change to a specific channel id (chanid)\r\n"
            "play program CHANID yyyy-mm-ddThh:mm:ss\r\n"
            "                      - Play program with chanid & starttime\r\n"
            "play program CHANID yyyy-mm-ddThh:mm:ss resume\r\n"
            "                      - Resume program with chanid & starttime\r\n"
            "play seek beginning   - Seek to the beginning of the recording\r\n"
            "play seek forward     - Skip forward in the video\r\n"
            "play seek backward    - Skip backwards in the video\r\n"
            "play seek HH:MM:SS    - Seek to a specific position\r\n"
            "play speed pause      - Pause playback\r\n"
            "play speed normal     - Playback at normal speed\r\n"
            "play speed 1x         - Playback at normal speed\r\n"
            "play speed -1x        - Playback at normal speed in reverse\r\n"
            "play speed 1/16x      - Playback at 1/16x speed\r\n"
            "play speed 1/8x       - Playback at 1/8x speed\r\n"
            "play speed 1/4x       - Playback at 1/4x speed\r\n"
            "play speed 1/2x       - Playback at 1/2x speed\r\n"
            "play speed 2x         - Playback at 2x speed\r\n"
            "play speed 4x         - Playback at 4x speed\r\n"
            "play speed 8x         - Playback at 8x speed\r\n"
            "play speed 16x        - Playback at 16x speed\r\n"
            "play stop             - Stop playback\r\n";
    }
    else if (command == "query")
    {
        helpText +=
            "query location        - Query current screen or location\r\n"
            "query recordings      - List currently available recordings\r\n";
    }

    if (helpText != "")
        return helpText;

    if (command != "")
            helpText += QString("Unknown command '%1'\r\n\r\n").arg(command);

    helpText +=
        "Valid Commands:\r\n"
        "---------------\r\n"
        "jump               - Jump to a specified location in Myth\r\n"
        "key                - Send a keypress to the program\r\n"
        "play               - Playback related commands\r\n"
        "query              - Queries\r\n"
        "exit               - Exit NetworkControl\r\n"
        "\r\n"
        "Type 'help COMMANDNAME' for help on any specific command.\r\n";

    return helpText;
}

void NetworkControl::notifyDataAvailable(void)
{
    QApplication::postEvent(this,
                            new QCustomEvent(kNetworkControlDataReadyEvent));
}

void NetworkControl::customEvent(QCustomEvent *e)
{       
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {   
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.left(15) != "NETWORK_CONTROL")
            return;

        QStringList tokens = QStringList::split(" ", message);
        if ((tokens.size() >= 3) &&
            (tokens[1] == "ANSWER"))
        {
            answer = tokens[2];
            for (unsigned int i = 3; i < tokens.size(); i++)
                answer += QString(" ") + tokens[i];
            gotAnswer = true;
        }
        else if ((tokens.size() >= 3) &&
                 (tokens[1] == "RESPONSE"))
        {
            QString response = tokens[2];
            for (unsigned int i = 3; i < tokens.size(); i++)
                response += QString(" ") + tokens[i];
            nrLock.lock();
            networkControlReplies.push_back(response);
            nrLock.unlock();

            notifyDataAvailable();
        }
    }
    else if (e->type() == kNetworkControlDataReadyEvent)
    {
        QString reply;
        int replies;
        QRegExp crlfRegEx("\r\n$");
        QRegExp crlfcrlfRegEx("\r\n.*\r\n");

        nrLock.lock();
        replies = networkControlReplies.size();
        while (client && cs && replies > 0 &&
               client->state() == QSocket::Connected)
        {
            reply = networkControlReplies.front();
            networkControlReplies.pop_front();
            *cs << reply;
            if (!reply.contains(crlfRegEx) || reply.contains(crlfcrlfRegEx))
                *cs << "\r\n" << prompt;
            client->flush();

            replies = networkControlReplies.size();
        }
        nrLock.unlock();
    }
    else if (e->type() == kNetworkControlCloseEvent)
    {
        if (client && client->state() == QSocket::Connected)
        {
            clientLock.lock();
            client->close();
            delete client;
            delete cs;
            client = NULL;
            cs = NULL;
            clientLock.unlock();
        }
    }
}

QString NetworkControl::listRecordings(void)
{
    QString result = "";
    QString episode;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid, starttime, title, subtitle "
                  "FROM recorded WHERE deletepending = 0 "
                  "ORDER BY starttime, title;");
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            if (query.value(3).toString() > " ")
                episode = QString("%1 -\"%2\"")
                                  .arg(query.value(2).toString().local8Bit())
                                  .arg(query.value(3).toString().local8Bit());
            else
                episode = query.value(2).toString().local8Bit();

            result +=
                QString("%1 %2 %3\r\n").arg(query.value(0).toInt())
                        .arg(query.value(1).toDateTime().toString(Qt::ISODate))
                        .arg(episode);
        }
    }
    else
        result = "ERROR: Unable to retrieve recordings list.";

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

