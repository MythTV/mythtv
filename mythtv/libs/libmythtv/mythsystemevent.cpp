
#include <QApplication>
#include <QFileInfo>
#include <QRunnable>

#include "mythcorecontext.h"
#include "mthreadpool.h"
#include "mythsystem.h"
#include "mythsystemevent.h"
#include "programinfo.h"
#include "remoteutil.h"
#include "exitcodes.h"
#include "mythlogging.h"

#define LOC      QString("MythSystemEventHandler: ")

/** \class SystemEventThread
 *  \brief QRunnable class for running MythSystemEvent handler commands
 *
 *  The SystemEventThread class runs a system event handler command in
 *  non-blocking mode.  The commands are run in the MThreadPool::globalInstance,
 *  but we release and reserve the thread inside ::run() so that long-running
 *  commands to not block other short-running commands from executing if we
 *  hit MThreadPool::maxThreadCount().
 */
class SystemEventThread : public QRunnable
{
  public:
    /** \fn SystemEventThread::SystemEventThread(const QString cmd,
                                                 QString eventName)
     *  \brief Constructor for creating a SystemEventThread
     *  \param cmd       Command line to run for this System Event
     *  \param eventName Optional System Event name for this command
     */
    SystemEventThread(const QString cmd, QString eventName = "")
      : m_command(cmd), m_event(eventName) {};

    /** \fn SystemEventThread::run()
     *  \brief Runs the System Event handler command
     *
     *  Overrides QRunnable::run()
     */
    void run(void)
    {
        uint flags = kMSDontBlockInputDevs;

        m_event.detach();
        m_command.detach();

        uint result = myth_system(m_command, flags);

        if (result != GENERIC_EXIT_OK)
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Command '%1' returned %2")
                    .arg(m_command).arg(result));

        if (m_event.isEmpty()) 
            return;

        RemoteSendMessage(QString("SYSTEM_EVENT_RESULT %1 SENDER %2 RESULT %3")
                                  .arg(m_event).arg(gCoreContext->GetHostName())
                                  .arg(result));
    }

  private:
    // Private data storage
    QString m_command;
    QString m_event;
};


/** \fn MythSystemEventHandler::MythSystemEventHandler(void)
 *  \brief Null Constructor
 *
 *  Adds this object as a gContext event listener.
 */
MythSystemEventHandler::MythSystemEventHandler(void)
{
    gCoreContext->addListener(this);
}

/** \fn MythSystemEventHandler::~MythSystemEventHandler()
 *  \brief Destructor
 *
 *  Removes this object as a gContext event listener.
 */
MythSystemEventHandler::~MythSystemEventHandler()
{
    gCoreContext->removeListener(this);
}

/** \fn MythSystemEventHandler::SubstituteMatches(const QStringList &tokens,
                                                  QString &command)
 *  \brief Substitutes %MATCH% variables in given command line.
 *  \sa ProgramInfo::SubstituteMatches(QString &str)
 *
 *  Subsitutes values for %MATCH% type variables in given command string.
 *  Some of these matches come from the tokens list passed in and some
 *  may come from a ProgramInfo if a chanid and starttime are specified
 *  in the tokens list.
 *
 *  \param tokens  Const QStringList containing token list passed with event.
 *  \param command Command line containing %MATCH% variables to be substituted.
 */
void MythSystemEventHandler::SubstituteMatches(const QStringList &tokens,
                                               QString &command)
{
    if (command.isEmpty())
        return;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("SubstituteMatches: BEFORE: %1")
                                            .arg(command));
    QString args;
    uint chanid = 0;
    QDateTime recstartts;
    QString sender;

    QStringList::const_iterator it = tokens.begin();
    ++it;
    command.replace(QString("%EVENTNAME%"), *it);

    ++it;
    while (it != tokens.end())
    {
        if (!args.isEmpty())
            args += " ";
        args += *it;

        // Check for some token names that we substitute one for one as
        // %MATCH% type variables.
        if ((*it == "CARDID") ||
            (*it == "HOSTNAME") ||
            (*it == "SECS") ||
            (*it == "SENDER") ||
            (*it == "PATH"))
        {
            QString token = *it;

            if (++it == tokens.end())
                break;

            if (token == "SENDER")
                sender = *it;

            // The following string is broken up on purpose to indicate
            // what we're replacing is the token surrounded by percent signs
            command.replace(QString("%" "%1" "%").arg(token), *it);

            if (!args.isEmpty())
                args += " ";
            args += *it;
        }

        // Remember any chanid and starttime so we can lookup info about
        // the recording from the database.
        if (*it == "CHANID")
        {
            if (++it == tokens.end())
                break;

            chanid = (*it).toUInt();

            if (!args.isEmpty())
                args += " ";
            args += *it;
        }

        if (*it == "STARTTIME")
        {
            if (++it == tokens.end())
                break;

            recstartts = QDateTime::fromString(*it, Qt::ISODate);

            if (!args.isEmpty())
                args += " ";
            args += *it;
        }

        ++it;
    }

    command.replace(QString("%ARGS%"), args);

    ProgramInfo pginfo(chanid, recstartts);
    bool pginfo_loaded = pginfo.GetChanID();
    if (!pginfo_loaded)
    {
        RecordingInfo::LoadStatus status;
        pginfo = RecordingInfo(chanid, recstartts, false, 0, &status);
        pginfo_loaded = RecordingInfo::kFoundProgram == status;
    }

    if (pginfo_loaded)
    {
        pginfo.SubstituteMatches(command);
    }
    else
    {
        command.replace(QString("%CHANID%"), QString::number(chanid));
        command.replace(QString("%STARTTIME%"),
                        recstartts.toString("yyyyMMddhhmmss"));
        command.replace(QString("%STARTTIMEISO%"),
                        recstartts.toString(Qt::ISODate));
    }

    command.replace(QString("%VERBOSELEVEL%"), QString("%1").arg(verboseMask));

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("SubstituteMatches: AFTER : %1")
                                            .arg(command));
}

/** \fn MythSystemEventHandler::EventNameToSetting(const QString &name)
 *  \brief Convert an MythSystemEvent name to a database setting name
 *
 *  Converts an underscored, all-capital-letters system event name of the form
 *  NET_CTRL_CONNECTED to the corresponding CamelCase database setting
 *  name EventCmdNetCtrlConnected.
 *
 *  \param name Const QString containing System Event name to convert
 */
QString MythSystemEventHandler::EventNameToSetting(const QString &name)
{
    QString result("EventCmd");
    QStringList parts = name.toLower().split('_', QString::SkipEmptyParts);

    QStringList::Iterator it = parts.begin();
    while (it != parts.end())
    {
        result += (*it).left(1).toUpper();
        result += (*it).mid(1);

        ++it;
    }

    return result;
}

/** \fn MythSystemEventHandler::customEvent(QEvent *e)
 *  \brief Custom Event handler for receiving and processing System Events
 *  \sa MythSystemEventHandler::SubstituteMatches(const QStringList &tokens,
                                                  QString &command)
 *      MythSystemEventHandler::EventNameToSetting(const QString &name)
 *
 *  This function listens for SYSTEM_EVENT messages and fires off any
 *  necessary event handler commands.  In addition to SYSTEM_EVENT messages,
 *  this code also forwards GLOBAL_SYSTEM_EVENT which may have been sent
 *  by the local system via code that does not have access to the master
 *  backend connection to send events on its own.  One example is the
 *  code that sends KEY_xx system events.
 *
 *  \param e Pointer to QEvent containing event to handle
 */
void MythSystemEventHandler::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString msg = me->Message().simplified();

        if (msg == "CLEAR_SETTINGS_CACHE")
            msg = "SYSTEM_EVENT SETTINGS_CACHE_CLEARED";

        // Listen for any GLOBAL_SYSTEM_EVENT messages and resend to
        // the master backend as regular SYSTEM_EVENT messages.
        if (msg.startsWith("GLOBAL_SYSTEM_EVENT "))
        {
            RemoteSendMessage(msg.mid(7) + QString(" SENDER %1")
                              .arg(gCoreContext->GetHostName()));
            return;
        }

        if ((!msg.startsWith("SYSTEM_EVENT ")) &&
            (!msg.startsWith("LOCAL_SYSTEM_EVENT ")))
            return;

        QStringList tokens = msg.split(' ', QString::SkipEmptyParts);

        // Return if this event is for another host
        if ((tokens.size() >= 4) &&
            (tokens[2] == "HOST") &&
            (tokens[3] != gCoreContext->GetHostName()))
            return;

        QString cmd;

        // See if this system has a command that runs for all system events
        cmd = gCoreContext->GetSetting("EventCmdAll");
        if (!cmd.isEmpty())
        {
            SubstituteMatches(tokens, cmd);

            SystemEventThread *eventThread = new SystemEventThread(cmd);
            MThreadPool::globalInstance()->startReserved(
                eventThread, "SystemEvent");
        }

        // Check for an EventCmd for this particular event
        cmd = gCoreContext->GetSetting(EventNameToSetting(tokens[1]));
        if (!cmd.isEmpty())
        {
            SubstituteMatches(tokens, cmd);

            SystemEventThread *eventThread =
                new SystemEventThread(cmd, tokens[1]);
            MThreadPool::globalInstance()->startReserved(
                eventThread, "SystemEvent");
        }
    }
}

/****************************************************************************/

/** \fn SendMythSystemEvent(const QString msg)
 *  \brief Send the given message as a System Event
 *  \param msg Const QString to send as a System Event
 */
void SendMythSystemEvent(const QString msg)
{
    RemoteSendMessage(QString("SYSTEM_EVENT %1 SENDER %2")
                              .arg(msg).arg(gCoreContext->GetHostName()));
}

/** \fn SendMythSystemRecEvent(const QString msg, const RecordingInfo *pginfo)
 *  \brief Sends a System Event for an in-progress recording
 *  \sa SendMythSystemEvent(const QString msg)
 *  \param msg    Const QString to send as a System Event
 *  \param pginfo Pointer to a RecordingInfo containing info on a recording
 */
void SendMythSystemRecEvent(const QString msg, const RecordingInfo *pginfo)
{
    if (pginfo)
        SendMythSystemEvent(QString("%1 CARDID %2 CHANID %3 STARTTIME %4")
                            .arg(msg).arg(pginfo->GetCardID())
                            .arg(pginfo->GetChanID())
                            .arg(pginfo->GetRecordingStartTime(ISODate)));
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "SendMythSystemRecEvent() called with "
                                       "empty RecordingInfo");
}

/** \fn SendMythSystemPlayEvent(const QString msg, const ProgramInfo *pginfo)
 *  \brief Sends a System Event for a previously recorded program
 *  \sa SendMythSystemEvent(const QString msg)
 *  \param msg    Const QString to send as a System Event
 *  \param pginfo Pointer to a RecordingInfo containing info on a recording
 */
void SendMythSystemPlayEvent(const QString msg, const ProgramInfo *pginfo)
{
    if (pginfo)
        SendMythSystemEvent(
            QString("%1 HOSTNAME %2 CHANID %3 STARTTIME %4")
                    .arg(msg).arg(gCoreContext->GetHostName())
                    .arg(pginfo->GetChanID())
                    .arg(pginfo->GetRecordingStartTime(ISODate)));
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "SendMythSystemPlayEvent() called with "
                                       "empty ProgramInfo");
}

/** \fn SendMythSystemHostEvent(const QString msg, const QString &hostname,
                                const QString args)
 *  \brief Sends a System Event to a specific host
 *  \param msg      Const QString to send as a System Event
 *  \param hostname Const QString to send the event to
 *  \param args     Const QString containing other info to send in the event
 */
void SendMythSystemHostEvent(const QString msg, const QString &hostname,
                             const QString args)
{
    SendMythSystemEvent(QString("%1 HOST %2 %3")
                                .arg(msg).arg(hostname).arg(args));
}

/****************************************************************************/

/** \fn MythSystemEventEditor::MythSystemEventEditor(MythScreenStack *parent,
                                                     const char *name)
 *  \brief Constructor for the MythSystemEvent settings editor
 *
 *  Populates the settings name list with the System Event settings names
 *  and sets the title of the RawSettingsEditor screen to indicate that
 *  we are editting the System Event Commands.
 *
 *  \param parent Parent screen stack for this window
 *  \param name   Name of this window
 */
MythSystemEventEditor::MythSystemEventEditor(MythScreenStack *parent,
                                             const char *name)
  : RawSettingsEditor(parent, name)
{
    m_title = tr("System Event Command Editor");

    m_settings["EventCmdRecPending"]           = tr("Recording pending");
    m_settings["EventCmdRecStarted"]           = tr("Recording started");
    m_settings["EventCmdRecFinished"]          = tr("Recording finished");
    m_settings["EventCmdRecDeleted"]           = tr("Recording deleted");
    m_settings["EventCmdRecExpired"]           = tr("Recording expired");
    m_settings["EventCmdLivetvStarted"]        = tr("LiveTV started");
    m_settings["EventCmdPlayStarted"]          = tr("Playback started");
    m_settings["EventCmdPlayStopped"]          = tr("Playback stopped");
    m_settings["EventCmdPlayPaused"]           = tr("Playback paused");
    m_settings["EventCmdPlayUnpaused"]         = tr("Playback unpaused");
    m_settings["EventCmdPlayChanged"]          = tr("Playback program changed");
    m_settings["EventCmdMasterStarted"]        = tr("Master backend started");
    m_settings["EventCmdMasterShutdown"]       = tr("Master backend shutdown");
    m_settings["EventCmdClientConnected"]      = tr("Client connected to master backend");
    m_settings["EventCmdClientDisconnected"]   = tr("Client disconnected from master backend");
    m_settings["EventCmdSlaveConnected"]       = tr("Slave backend connected to master");
    m_settings["EventCmdSlaveDisconnected"]    = tr("Slave backend disconnected from master");
    m_settings["EventCmdNetCtrlConnected"]     = tr("Network Control client connected");
    m_settings["EventCmdNetCtrlDisconnected"]  = tr("Network Control client disconnected");
    m_settings["EventCmdMythfilldatabaseRan"]  = tr("mythfilldatabase ran");
    m_settings["EventCmdSchedulerRan"]         = tr("Scheduler ran");
    m_settings["EventCmdSettingsCacheCleared"] = tr("Settings cache cleared");
    m_settings["EventCmdKey01"]                = tr("Keystroke event #1");
    m_settings["EventCmdKey02"]                = tr("Keystroke event #2");
    m_settings["EventCmdKey03"]                = tr("Keystroke event #3");
    m_settings["EventCmdKey04"]                = tr("Keystroke event #4");
    m_settings["EventCmdKey05"]                = tr("Keystroke event #5");
    m_settings["EventCmdKey06"]                = tr("Keystroke event #6");
    m_settings["EventCmdKey07"]                = tr("Keystroke event #7");
    m_settings["EventCmdKey08"]                = tr("Keystroke event #8");
    m_settings["EventCmdKey09"]                = tr("Keystroke event #9");
    m_settings["EventCmdKey10"]                = tr("Keystroke event #10");
    m_settings["EventCmdAll"]                  = tr("Any event");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
