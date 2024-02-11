
#include <QApplication>
#include <QFileInfo>
#include <QRunnable>
#include <utility>

#include "libmythbase/exitcodes.h"
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"

#include "cardutil.h"
#include "mythsystemevent.h"

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
    explicit SystemEventThread(QString cmd, QString eventName = "")
      : m_command(std::move(cmd)), m_event(std::move(eventName)) {};

    /** \fn SystemEventThread::run()
     *  \brief Runs the System Event handler command
     *
     *  Overrides QRunnable::run()
     */
    void run(void) override // QRunnable
    {
        uint flags = kMSDontBlockInputDevs;
        uint result = myth_system(m_command, flags);

        LOG(VB_GENERAL,
            (result == GENERIC_EXIT_OK ? LOG_INFO : LOG_WARNING), LOC +
            QString("Finished '%1' result %2")
            .arg(m_command).arg(result));

        if (m_event.isEmpty())
            return;

        gCoreContext->SendMessage(
            QString("SYSTEM_EVENT_RESULT %1 SENDER %2 RESULT %3")
                    .arg(m_event, gCoreContext->GetHostName(),
                         QString::number(result)));
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
    setObjectName("MythSystemEventHandler");
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
            (*it == "COMMAND") ||
            (*it == "RECSTATUS") ||
            (*it == "HOSTNAME") ||
            (*it == "SECS") ||
            (*it == "SENDER") ||
            (*it == "PATH") ||
            (*it == "VIDEODEVICE") ||
            (*it == "VBIDEVICE"))
        {
            QString token = *it;

            if (++it == tokens.end())
                break;

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

            recstartts = MythDate::fromString(*it);

            if (!args.isEmpty())
                args += " ";
            args += *it;
        }

        ++it;
    }

    command.replace(QString("%ARGS%"), args);

    // 1st, try loading RecordingInfo / ProgramInfo
    RecordingInfo recinfo(chanid, recstartts);
    bool loaded = recinfo.GetChanID() != 0U;
    if (loaded)
        recinfo.SubstituteMatches(command);
    else
    {
        // 2rd Try searching for RecordingInfo
        RecordingInfo::LoadStatus status = RecordingInfo::kNoProgram;
        RecordingInfo recinfo2(chanid, recstartts, false, 0h, &status);
        if (status == RecordingInfo::kFoundProgram)
            recinfo2.SubstituteMatches(command);
        else
        {
            // 3th just use what we know
            command.replace(QString("%CHANID%"), QString::number(chanid));
            command.replace(QString("%STARTTIME%"),
                            MythDate::toString(recstartts,
                                               MythDate::kFilename));
            command.replace(QString("%STARTTIMEISO%"),
                            recstartts.toString(Qt::ISODate));
        }
    }

    command.replace(QString("%VERBOSELEVEL%"), QString("%1").arg(verboseMask));
    command.replace("%VERBOSEMODE%", QString("%1").arg(logPropagateArgs));

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
    QStringList parts = name.toLower().split('_', Qt::SkipEmptyParts);
    for (const auto & part : std::as_const(parts))
    {
        result += part.at(0).toUpper();
        result += part.mid(1);
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
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;
        QString msg = me->Message().simplified();

        if (msg == "CLEAR_SETTINGS_CACHE")
            msg = "SYSTEM_EVENT SETTINGS_CACHE_CLEARED";

        // Listen for any GLOBAL_SYSTEM_EVENT messages and resend to
        // the master backend as regular SYSTEM_EVENT messages.
        if (msg.startsWith("GLOBAL_SYSTEM_EVENT "))
        {
            gCoreContext->SendMessage(msg.mid(7) +
                QString(" SENDER %1").arg(gCoreContext->GetHostName()));
            return;
        }

        if ((!msg.startsWith("SYSTEM_EVENT ")) &&
            (!msg.startsWith("LOCAL_SYSTEM_EVENT ")))
            return;

        QStringList tokens = msg.split(' ', Qt::SkipEmptyParts);

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

            auto *eventThread = new SystemEventThread(cmd);
            MThreadPool::globalInstance()->startReserved(
                eventThread, "SystemEvent");
        }

        // Check for an EventCmd for this particular event
        cmd = gCoreContext->GetSetting(EventNameToSetting(tokens[1]));
        if (!cmd.isEmpty())
        {
            SubstituteMatches(tokens, cmd);

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Starting thread for command '%1'").arg(cmd));

            auto *eventThread = new SystemEventThread(cmd, tokens[1]);
            MThreadPool::globalInstance()->startReserved(
                eventThread, "SystemEvent");
        }
    }
}

/****************************************************************************/

/** \fn SendMythSystemRecEvent(const QString msg, const RecordingInfo *pginfo)
 *  \brief Sends a System Event for an in-progress recording
 *  \sa MythCoreContext::SendSystemEvent(const QString msg)
 *  \param msg    Const QString to send as a System Event
 *  \param pginfo Pointer to a RecordingInfo containing info on a recording
 */
void SendMythSystemRecEvent(const QString &msg, const RecordingInfo *pginfo)
{
    if (pginfo)
    {
        uint cardid = pginfo->GetInputID();
        gCoreContext->SendSystemEvent(
            QString("%1 CARDID %2 CHANID %3 STARTTIME %4 RECSTATUS %5 "
                    "VIDEODEVICE %6 VBIDEVICE %7")
            .arg(msg).arg(cardid)
            .arg(pginfo->GetChanID())
            .arg(pginfo->GetRecordingStartTime(MythDate::ISODate))
            .arg(pginfo->GetRecordingStatus())
            .arg(CardUtil::GetVideoDevice(cardid),
                 CardUtil::GetVBIDevice(cardid)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "SendMythSystemRecEvent() called with empty RecordingInfo");
    }
}

/** \fn SendMythSystemPlayEvent(const QString msg, const ProgramInfo *pginfo)
 *  \brief Sends a System Event for a previously recorded program
 *  \sa MythCoreContext::SendSystemEvent(const QString msg)
 *  \param msg    Const QString to send as a System Event
 *  \param pginfo Pointer to a RecordingInfo containing info on a recording
 */
void SendMythSystemPlayEvent(const QString &msg, const ProgramInfo *pginfo)
{
    if (pginfo)
    {
        gCoreContext->SendSystemEvent(
            QString("%1 HOSTNAME %2 CHANID %3 STARTTIME %4")
                    .arg(msg, gCoreContext->GetHostName())
                    .arg(pginfo->GetChanID())
                    .arg(pginfo->GetRecordingStartTime(MythDate::ISODate)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SendMythSystemPlayEvent() called with "
                                       "empty ProgramInfo");
    }
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
    createSettingList(m_settings);

}
 void MythSystemEventEditor::createSettingList(QMap <QString, QString>  &settings)
{
    // Event names are programmatically converted to settings names in
    // EventNameToSetting().  For convenience of searching the code
    // base, the event names are listed in comments.
    settings["EventCmdRecPending"]           = // REC_PENDING
        tr("Recording pending");
    settings["EventCmdRecPreFail"]           = // REC_PREFAIL
        tr("Recording about to fail");
    settings["EventCmdRecFailing"]           = // REC_FAILING
        tr("Recording failing");
    settings["EventCmdRecStarted"]           = // REC_STARTED
        tr("Recording started");
    settings["EventCmdRecStartedWriting"]    = // REC_STARTED_WRITING
        tr("Recording started writing");
    settings["EventCmdRecFinished"]          = // REC_FINISHED
        tr("Recording finished");
    settings["EventCmdRecDeleted"]           = // REC_DELETED
        tr("Recording deleted");
    settings["EventCmdRecExpired"]           = // REC_EXPIRED
        tr("Recording expired");
    settings["EventCmdLivetvStarted"]        = // LIVETV_STARTED
        tr("LiveTV started");
    settings["EventCmdLivetvEnded"]          = // LIVETV_ENDED
        tr("LiveTV ended");
    settings["EventCmdPlayStarted"]          = // PLAY_STARTED
        tr("Playback started");
    settings["EventCmdPlayStopped"]          = // PLAY_STOPPED
        tr("Playback stopped");
    settings["EventCmdPlayPaused"]           = // PLAY_PAUSED
        tr("Playback paused");
    settings["EventCmdPlayUnpaused"]         = // PLAY_UNPAUSED
        tr("Playback unpaused");
    settings["EventCmdPlayChanged"]          = // PLAY_CHANGED
        tr("Playback program changed");
    settings["EventCmdTuningSignalTimeout"]  = // TUNING_SIGNAL_TIMEOUT
        tr("Tuning signal waiting");
    settings["EventCmdMasterStarted"]        = // MASTER_STARTED
        tr("Master backend started");
    settings["EventCmdMasterShutdown"]       = // MASTER_SHUTDOWN
        tr("Master backend shutdown");
    settings["EventCmdClientConnected"]      = // CLIENT_CONNECTED
        tr("Client connected to master backend");
    settings["EventCmdClientDisconnected"]   = // CLIENT_DISCONNECTED
        tr("Client disconnected from master backend");
    settings["EventCmdSlaveConnected"]       = // SLAVE_CONNECTED
        tr("Slave backend connected to master");
    settings["EventCmdSlaveDisconnected"]    = // SLAVE_DISCONNECTED
        tr("Slave backend disconnected from master");
    settings["EventCmdNetCtrlConnected"]     = // NET_CTRL_CONNECTED
        tr("Network Control client connected");
    settings["EventCmdNetCtrlDisconnected"]  = // NET_CTRL_DISCONNECTED
        tr("Network Control client disconnected");
    settings["EventCmdMythfilldatabaseRan"]  = // MYTHFILLDATABASE_RAN
        tr("mythfilldatabase ran");
    settings["EventCmdSchedulerRan"]         = // SCHEDULER_RAN
        tr("Scheduler ran");
    settings["EventCmdSettingsCacheCleared"] = // SETTINGS_CACHE_CLEARED
        tr("Settings cache cleared");
    settings["EventCmdScreenType"]           = // SCREEN_TYPE
        tr("Screen created or destroyed");
    settings["EventCmdKey01"]                = // KEY_%1
        tr("Keystroke event #1");
    settings["EventCmdKey02"]                = // KEY_%1
        tr("Keystroke event #2");
    settings["EventCmdKey03"]                = // KEY_%1
        tr("Keystroke event #3");
    settings["EventCmdKey04"]                = // KEY_%1
        tr("Keystroke event #4");
    settings["EventCmdKey05"]                = // KEY_%1
        tr("Keystroke event #5");
    settings["EventCmdKey06"]                = // KEY_%1
        tr("Keystroke event #6");
    settings["EventCmdKey07"]                = // KEY_%1
        tr("Keystroke event #7");
    settings["EventCmdKey08"]                = // KEY_%1
        tr("Keystroke event #8");
    settings["EventCmdKey09"]                = // KEY_%1
        tr("Keystroke event #9");
    settings["EventCmdKey10"]                = // KEY_%1
        tr("Keystroke event #10");
    settings["EventCmdCecCommandReceived"]   = // CEC_COMMAND_RECEIVED
        tr("CEC command received");
    settings["EventCmdAll"]                  = // EventCmdAll
        tr("Any event");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
