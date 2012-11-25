#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMutex>

#include <cmath>
#include <iostream>

#include <queue>
#include <algorithm>
using namespace std;

#include "config.h"
#include "mythcontext.h"
#include "exitcodes.h"
#include "mythdate.h"
#include "remotefile.h"
#include "backendselect.h"
#include "dbsettings.h"
#include "langsettings.h"
#include "mythtranslation.h"
#include "mythxdisplay.h"
#include "mythevent.h"
#include "dbutil.h"
#include "DisplayRes.h"
#include "mythmediamonitor.h"

#include "mythdb.h"
#include "mythdirs.h"
#include "mythversion.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythimage.h"
#include "mythxmlclient.h"
#include "upnp.h"
#include "mythlogging.h"
#include "mythsystem.h"
#include "mythmiscutil.h"

#include "mythplugin.h"

#ifdef USING_MINGW
#include <unistd.h>
#include "compat.h"
#endif

#define LOC      QString("MythContext: ")

MythContext *gContext = NULL;

class MythContextPrivate : public QObject
{
    friend class MythContextSlotHandler;

  public:
    MythContextPrivate(MythContext *lparent);
   ~MythContextPrivate();

    bool Init        (const bool gui,
                      const bool prompt, const bool noPrompt,
                      const bool ignoreDB);
    bool FindDatabase(const bool prompt, const bool noPrompt);

    void TempMainWindow(bool languagePrompt = true);
    void EndTempWindow(void);

    bool LoadDatabaseSettings(void);
    bool SaveDatabaseParams(const DatabaseParams &params, bool force);

    bool    PromptForDatabaseParams(const QString &error);
    QString TestDBconnection(void);
    void    SilenceDBerrors(void);
    void    EnableDBerrors(void);
    void    ResetDatabase(void);

    int     ChooseBackend(const QString &error);
    int     UPnPautoconf(const int milliSeconds = 2000);
    bool    DefaultUPnP(QString &error);
    bool    UPnPconnect(const DeviceLocation *device, const QString &PIN);

  protected:
    bool event(QEvent*);

    void ShowConnectionFailurePopup(bool persistent);
    void HideConnectionFailurePopup(void);

    void ShowVersionMismatchPopup(uint remoteVersion);

  public:
    MythContext *parent;

    bool      m_gui;               ///< Should this context use GUI elements?

    QString m_masterhostname;    ///< master backend hostname

    DatabaseParams  m_DBparams;  ///< Current database host & WOL details
    QString         m_DBhostCp;  ///< dbHostName backup

    Configuration    *m_pConfig;

    bool disableeventpopup;

    MythUIHelper *m_ui;
    MythContextSlotHandler *m_sh;

  private:
    MythConfirmationDialog *MBEconnectPopup;
    MythConfirmationDialog *MBEversionPopup;
};

static void exec_program_cb(const QString &cmd)
{
    myth_system(cmd);
}

static void exec_program_tv_cb(const QString &cmd)
{
    QString s = cmd;
    QStringList tokens = cmd.simplified().split(" ");
    QStringList strlist;

    bool cardidok;
    int wantcardid = tokens[0].toInt(&cardidok, 10);

    if (cardidok && wantcardid > 0)
    {
        strlist << QString("LOCK_TUNER %1").arg(wantcardid);
        s = s.replace(0, tokens[0].length() + 1, "");
    }
    else
        strlist << "LOCK_TUNER";

    gCoreContext->SendReceiveStringList(strlist);
    int cardid = strlist[0].toInt();

    if (cardid >= 0)
    {
        s = s.sprintf(qPrintable(s),
                      qPrintable(strlist[1]),
                      qPrintable(strlist[2]),
                      qPrintable(strlist[3]));

        myth_system(s);

        strlist = QStringList(QString("FREE_TUNER %1").arg(cardid));
        gCoreContext->SendReceiveStringList(strlist);
        QString ret = strlist[0];
    }
    else
    {
        QString label;

        if (cardidok)
        {
            if (cardid == -1)
                label = QObject::tr("Could not find specified tuner (%1).")
                                    .arg(wantcardid);
            else
                label = QObject::tr("Specified tuner (%1) is already in use.")
                                    .arg(wantcardid);
        }
        else
        {
            label = QObject::tr("All tuners are currently in use. If you want "
                                "to watch TV, you can cancel one of the "
                                "in-progress recordings from the delete menu");
        }

        LOG(VB_GENERAL, LOG_ALERT, QString("exec_program_tv: ") + label);

        ShowOkPopup(label);
    }
}

static void configplugin_cb(const QString &cmd)
{
    MythPluginManager *pmanager = gCoreContext->GetPluginManager();
    if (pmanager)
        if (pmanager->config_plugin(cmd.trimmed()))
            ShowOkPopup(QObject::tr("Failed to configure plugin %1").arg(cmd));
}

static void plugin_cb(const QString &cmd)
{
    MythPluginManager *pmanager = gCoreContext->GetPluginManager();
    if (pmanager)
        if (pmanager->run_plugin(cmd.trimmed()))
            ShowOkPopup(QObject::tr("The plugin %1 has failed "
                                    "to run for some reason...").arg(cmd));
}

static void eject_cb(void)
{
    MediaMonitor::ejectOpticalDisc();
}

MythContextPrivate::MythContextPrivate(MythContext *lparent)
    : parent(lparent),
      m_gui(false),
      m_pConfig(NULL), 
      disableeventpopup(false),
      m_ui(NULL),
      m_sh(new MythContextSlotHandler(this)),
      MBEconnectPopup(NULL),
      MBEversionPopup(NULL)
{
    InitializeMythDirs();
}

MythContextPrivate::~MythContextPrivate()
{
    if (m_pConfig)
        delete m_pConfig;
    if (m_ui)
        DestroyMythUI();
    if (m_sh)
        m_sh->deleteLater();
}

/**
 * \brief Setup a minimal themed main window, and prompt for user's language.
 *
 * Used for warnings before the database is opened, or bootstrapping pages.
 * After using the window, call EndTempWindow().
 *
 * \bug   Some of these settings (<i>e.g.</I> window size, theme)
 *        seem to be used after the temp window is destroyed.
 *
 */
void MythContextPrivate::TempMainWindow(bool languagePrompt)
{
    if (HasMythMainWindow())
        return;

    SilenceDBerrors();

    gCoreContext->OverrideSettingForSession("Theme", DEFAULT_UI_THEME);
#ifdef Q_WS_MACX
    // Qt 4.4 has window-focus problems
    gCoreContext->OverrideSettingForSession("RunFrontendInWindow", "1");
#endif
    GetMythUI()->LoadQtConfig();

    MythMainWindow *mainWindow = MythMainWindow::getMainWindow(false);
    mainWindow->Init();

    if (languagePrompt)
    {
        // ask user for language settings
        LanguageSelection::prompt();
        MythTranslation::load("mythfrontend");
    }
}

void MythContextPrivate::EndTempWindow(void)
{
    DestroyMythMainWindow();
    gCoreContext->ClearOverrideSettingForSession("Theme");
    EnableDBerrors();
}

bool MythContextPrivate::Init(const bool gui,
                              const bool promptForBackend,
                              const bool noPrompt,
                              const bool ignoreDB)
{
    gCoreContext->GetDB()->IgnoreDatabase(ignoreDB);
    m_gui = gui;

    // We don't have a database yet, so lets use the config.xml file.
    m_pConfig = new XmlConfiguration("config.xml");

    // Creates screen saver control if we will have a GUI
    if (gui)
        m_ui = GetMythUI();

    // ---- database connection stuff ----

    if (!ignoreDB && !FindDatabase(promptForBackend, noPrompt))
        return false;

    // ---- keep all DB-using stuff below this line ----

    // Prompt for language if this is a first time install and
    // we didn't already do so.
    if (m_gui && !gCoreContext->GetDB()->HaveSchema())
    {
        TempMainWindow(false);
        LanguageSelection::prompt();
        MythTranslation::load("mythfrontend");
        EndTempWindow();
    }
    gCoreContext->InitLocale();
    gCoreContext->SaveLocaleDefaults();

    if (gui)
    {
        MythUIMenuCallbacks cbs;
        cbs.exec_program = exec_program_cb;
        cbs.exec_program_tv = exec_program_tv_cb;
        cbs.configplugin = configplugin_cb;
        cbs.plugin = plugin_cb;
        cbs.eject = eject_cb;

        m_ui->Init(cbs);
    }

    return true;
}

/**
 * Get database connection settings and test connectivity.
 *
 * Can use UPnP AutoDiscovery to locate backends, and get their DB settings.
 * The user can force the AutoDiscovery chooser with the --prompt argument,
 * and disable it by using the --disable-autodiscovery argument.
 * There is also an autoconfigure function, which counts the backends,
 * and if there is exactly one, uses it as above.
 *
 * Despite its name, the disable argument currently only disables the chooser.
 * If set, autoconfigure will still be attempted in some situations.
 */
bool MythContextPrivate::FindDatabase(bool prompt, bool noAutodetect)
{
    // We can only prompt if autodiscovery is enabled..
    bool manualSelect = prompt && !noAutodetect;

    QString failure;

    // 1. Either load config.xml or use sensible "localhost" defaults:
    bool loaded = LoadDatabaseSettings();
    DatabaseParams dbParamsFromFile = m_DBparams;

    // In addition to the UI chooser, we can also try to autoSelect later,
    // but only if we're not doing manualSelect and there was no
    // valid config.xml
    bool autoSelect = !manualSelect && !loaded && !noAutodetect;

    // 2. If the user isn't forcing up the chooser UI, look for a default
    //    backend in config.xml, then test DB settings we've got so far:
    if (!manualSelect)
    {
        // config.xml may contain a backend host UUID and PIN.
        // If so, try to AutoDiscover UPnP server, and use its DB settings:

        if (DefaultUPnP(failure))                // Probably a valid backend,
            autoSelect = manualSelect = false;   // so disable any further UPnP
        else
            if (failure.length())
                LOG(VB_GENERAL, LOG_ALERT, failure);

        failure = TestDBconnection();
        if (failure.isEmpty())
            goto DBfound;
    }


    // 3. Try to automatically find the single backend:
    if (autoSelect)
    {
        int count = UPnPautoconf();

        if (count == 0)
            failure = QObject::tr("No UPnP backends found", "Backend Setup");

        if (count == 1)
        {
            failure = TestDBconnection();
            if (failure.isEmpty())
                goto DBfound;
        }

        // Multiple BEs, or needs PIN.
        manualSelect |= (count > 1 || count == -1);
    }

    manualSelect &= m_gui;  // no interactive command-line chooser yet

    // Queries the user for the DB info
    do
    {
        if (manualSelect)
        {
            // Get the user to select a backend from a possible list:
            BackendSelection::Decision d = (BackendSelection::Decision)
                ChooseBackend(failure);
            switch (d)
            {
                case BackendSelection::kAcceptConfigure:
                    break;
                case BackendSelection::kManualConfigure:
                    manualSelect = false;
                    break;
                case BackendSelection::kCancelConfigure:
                    goto NoDBfound;
            }
        }

        if (!manualSelect)
        {
            if (!PromptForDatabaseParams(failure))
                goto NoDBfound;
        }

        failure = TestDBconnection();
        if (!failure.isEmpty())
            LOG(VB_GENERAL, LOG_ALERT, failure);
    }
    while (!failure.isEmpty());

DBfound:
    LOG(VB_GENERAL, LOG_DEBUG, "FindDatabase() - Success!");
    SaveDatabaseParams(m_DBparams,
                       !loaded || m_DBparams.forceSave ||
                       dbParamsFromFile != m_DBparams);
    EnableDBerrors();
    ResetDatabase();
    return true;

NoDBfound:
    LOG(VB_GENERAL, LOG_DEBUG, "FindDatabase() - failed");
    return false;
}

/** Load database and host settings from config.xml, or set some defaults.
 *  \return true if config.xml was parsed
 */
bool MythContextPrivate::LoadDatabaseSettings(void)
{
    // try new format first
    m_DBparams.LoadDefaults();

    m_DBparams.localHostName = m_pConfig->GetValue("LocalHostName", "");
    m_DBparams.dbHostPing = m_pConfig->GetValue(kDefaultDB + "PingHost", true);
    m_DBparams.dbHostName = m_pConfig->GetValue(kDefaultDB + "Host", "");
    m_DBparams.dbUserName = m_pConfig->GetValue(kDefaultDB + "UserName", "");
    m_DBparams.dbPassword = m_pConfig->GetValue(kDefaultDB + "Password", "");
    m_DBparams.dbName = m_pConfig->GetValue(kDefaultDB + "DatabaseName", "");
    m_DBparams.dbPort = m_pConfig->GetValue(kDefaultDB + "Port", 0);

    m_DBparams.wolEnabled =
        m_pConfig->GetValue(kDefaultWOL + "Enabled", false);
    m_DBparams.wolReconnect =
        m_pConfig->GetValue(kDefaultWOL + "SQLReconnectWaitTime", 0);
    m_DBparams.wolRetry =
        m_pConfig->GetValue(kDefaultWOL + "SQLConnectRetry", 5);
    m_DBparams.wolCommand =
        m_pConfig->GetValue(kDefaultWOL + "Command", "");

    bool ok = m_DBparams.IsValid("config.xml");
    if (!ok) // if new format fails, try legacy format
    {
        m_DBparams.LoadDefaults();
        m_DBparams.dbHostName = m_pConfig->GetValue(
            kDefaultMFE + "DBHostName", "");
        m_DBparams.dbUserName = m_pConfig->GetValue(
            kDefaultMFE + "DBUserName", "");
        m_DBparams.dbPassword = m_pConfig->GetValue(
            kDefaultMFE + "DBPassword", "");
        m_DBparams.dbName = m_pConfig->GetValue(
            kDefaultMFE + "DBName", "");
        m_DBparams.dbPort = m_pConfig->GetValue(
            kDefaultMFE + "DBPort", 0);
        m_DBparams.forceSave = true;
        ok = m_DBparams.IsValid("config.xml");
    }
    if (!ok)
        m_DBparams.LoadDefaults();

    gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);

    QString hostname = m_DBparams.localHostName;
    if (hostname.isEmpty() ||
        hostname == "my-unique-identifier-goes-here")
    {
        char localhostname[1024];
        if (gethostname(localhostname, 1024))
        {
            LOG(VB_GENERAL, LOG_ALERT,
                    "MCP: Error, could not determine host name." + ENO);
            localhostname[0] = '\0';
        }
        hostname = localhostname;
        LOG(VB_GENERAL, LOG_NOTICE, "Empty LocalHostName.");
    }
    else
    {
        m_DBparams.localEnabled = true;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Using localhost value of %1")
            .arg(hostname));
    gCoreContext->SetLocalHostname(hostname);

    return ok;
}

bool MythContextPrivate::SaveDatabaseParams(
    const DatabaseParams &params, bool force)
{
    bool ret = true;

    // only rewrite file if it has changed
    if (params != m_DBparams || force)
    {
        m_pConfig->SetValue(
            "LocalHostName", params.localHostName);

        m_pConfig->SetValue(
            kDefaultDB + "PingHost", params.dbHostPing);
        m_pConfig->SetValue(
            kDefaultDB + "Host", params.dbHostName);
        m_pConfig->SetValue(
            kDefaultDB + "UserName", params.dbUserName);
        m_pConfig->SetValue(
            kDefaultDB + "Password", params.dbPassword);
        m_pConfig->SetValue(
            kDefaultDB + "DatabaseName", params.dbName);
        m_pConfig->SetValue(
            kDefaultDB + "Port", params.dbPort);

        m_pConfig->SetValue(
            kDefaultWOL + "Enabled", params.wolEnabled);
        m_pConfig->SetValue(
            kDefaultWOL + "SQLReconnectWaitTime", params.wolReconnect);
        m_pConfig->SetValue(
            kDefaultWOL + "SQLConnectRetry", params.wolRetry);
        m_pConfig->SetValue(
            kDefaultWOL + "Command", params.wolCommand);

        // clear out any legacy nodes..
        m_pConfig->ClearValue(kDefaultMFE + "DBHostName");
        m_pConfig->ClearValue(kDefaultMFE + "DBUserName");
        m_pConfig->ClearValue(kDefaultMFE + "DBPassword");
        m_pConfig->ClearValue(kDefaultMFE + "DBName");
        m_pConfig->ClearValue(kDefaultMFE + "DBPort");
        m_pConfig->ClearValue(kDefaultMFE + "DBHostPing");

        // actually save the file
        m_pConfig->Save();

        // Save the new settings:
        m_DBparams = params;
        gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);

        // If database has changed, force its use:
        ResetDatabase();
    }
    return ret;
}

bool MythContextPrivate::PromptForDatabaseParams(const QString &error)
{
    bool accepted = false;
    if (m_gui)
    {
        TempMainWindow();

        // Tell the user what went wrong:
        if (error.length())
            ShowOkPopup(error);

        // ask user for database parameters
        DatabaseSettings settings(m_DBhostCp);
        accepted = (settings.exec() == QDialog::Accepted);
        if (!accepted)
            LOG(VB_GENERAL, LOG_ALERT,
                     "User cancelled database configuration");

        EndTempWindow();
    }
    else
    {
        DatabaseParams params = parent->GetDatabaseParams();
        QString        response;

        // give user chance to skip config
        cout << endl << error.toLocal8Bit().constData() << endl << endl;
        response = getResponse("Would you like to configure the database "
                               "connection now?",
                               "no");
        if (!response.startsWith('y', Qt::CaseInsensitive))
            return false;

        params.dbHostName = getResponse("Database host name:",
                                        params.dbHostName);
        response = getResponse("Should I test connectivity to this host "
                               "using the ping command?", "yes");
        params.dbHostPing = response.startsWith('y', Qt::CaseInsensitive);

        params.dbPort     = intResponse("Database non-default port:",
                                        params.dbPort);
        params.dbName     = getResponse("Database name:",
                                        params.dbName);
        params.dbUserName = getResponse("Database user name:",
                                        params.dbUserName);
        params.dbPassword = getResponse("Database password:",
                                        params.dbPassword);

        params.localHostName = getResponse("Unique identifier for this machine "
                                           "(if empty, the local host name "
                                           "will be used):",
                                           params.localHostName);
        params.localEnabled = !params.localHostName.isEmpty();

        response = getResponse("Would you like to use Wake-On-LAN to retry "
                               "database connections?",
                               (params.wolEnabled ? "yes" : "no"));
        params.wolEnabled = response.startsWith('y', Qt::CaseInsensitive);

        if (params.wolEnabled)
        {
            params.wolReconnect = intResponse("Seconds to wait for "
                                              "reconnection:",
                                              params.wolReconnect);
            params.wolRetry     = intResponse("Number of times to retry:",
                                              params.wolRetry);
            params.wolCommand   = getResponse("Command to use to wake server:",
                                              params.wolCommand);
        }

        accepted = parent->SaveDatabaseParams(params);
    }
    return accepted;
}

/**
 * Some quick sanity checks before opening a database connection
 *
 * \todo  Rationalise the WOL stuff. We should have one method to wake BEs
 */
QString MythContextPrivate::TestDBconnection(void)
{
    bool    doPing = m_DBparams.dbHostPing;
    QString err    = QString::null;
    QString host   = m_DBparams.dbHostName;


    // 1. Check the supplied host or IP address, to prevent the app
    //    appearing to hang if we cannot route to the machine:

    // No need to ping myself
    if ((host == "localhost") ||
        (host == "127.0.0.1") ||
        (host == gCoreContext->GetHostName()))
        doPing = false;

    // If WOL is setup, the backend might be sleeping:
    if (doPing && m_DBparams.wolEnabled)
        for (int attempt = 0; attempt < m_DBparams.wolRetry; ++attempt)
        {
            int wakeupTime = m_DBparams.wolReconnect;

            if (ping(host, wakeupTime))
            {
                doPing = false;
                break;
            }

            LOG(VB_GENERAL, LOG_INFO,
                     QString("Trying to wake up host %1, attempt %2")
                          .arg(host).arg(attempt));
            myth_system(m_DBparams.wolCommand);

            LOG(VB_GENERAL, LOG_INFO,
                     QString("Waiting for %1 seconds").arg(wakeupTime));
            sleep(m_DBparams.wolReconnect);
        }

    if (doPing)
    {
        LOG(VB_GENERAL, LOG_INFO,
                 QString("Testing network connectivity to '%1'").arg(host));
    }

    if (doPing && !ping(host, 3))  // Fail after trying for 3 seconds
    {
        SilenceDBerrors();
        err = QObject::tr(
            "Cannot find (ping) database host %1 on the network",
            "Backend Setup");
        return err.arg(host);
    }


    // 2. Try to login, et c:

    // Current DB connection may have been silenced (invalid):
    ResetDatabase();

    if (!MSqlQuery::testDBConnection())
    {
        SilenceDBerrors();
        return QObject::tr("Cannot login to database", "Backend Setup");
    }


    return QString::null;
}

/**
 * Cause MSqlDatabase::OpenDatabase() and MSqlQuery to fail silently.
 *
 * This is used when the DB host address is bad, is non-routable,
 * the passwords are bad, or the DB has some other problem.
 *
 * It prevents hundreds of long TCP/IP timeouts or DB connect errors.
 */
void MythContextPrivate::SilenceDBerrors(void)
{
    // This silences any DB errors from Get*Setting(),
    // (which is the vast majority of them)
    gCoreContext->GetDB()->SetSuppressDBMessages(true);

    // Save the configured hostname, so that we can
    // still display it in the DatabaseSettings screens
    if (m_DBparams.dbHostName.length())
        m_DBhostCp = m_DBparams.dbHostName;

    m_DBparams.dbHostName.clear();
    gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);
}

void MythContextPrivate::EnableDBerrors(void)
{
    // Restore (possibly) blanked hostname
    if (m_DBparams.dbHostName.isNull() && m_DBhostCp.length())
    {
        m_DBparams.dbHostName = m_DBhostCp;
        gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);
    }

    gCoreContext->GetDB()->SetSuppressDBMessages(false);
}


/**
 * Called when the user changes the DB connection settings
 *
 * The current DB connections may be invalid (<I>e.g.</I> wrong password),
 * or the user may have changed to a different database host. Either way,
 * any current connections need to be closed so that the new connection
 * can be attempted.
 *
 * Any cached settings also need to be cleared,
 * so that they can be re-read from the new database
 */
void MythContextPrivate::ResetDatabase(void)
{
    gCoreContext->GetDBManager()->CloseDatabases();
    gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);
    gCoreContext->ClearSettingsCache();
}

/**
 * Search for backends via UPnP, put up a UI for the user to choose one
 */
int MythContextPrivate::ChooseBackend(const QString &error)
{
    TempMainWindow();

    // Tell the user what went wrong:
    if (!error.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error: %1").arg(error));
        ShowOkPopup(error);
    }

    LOG(VB_GENERAL, LOG_INFO, "Putting up the UPnP backend chooser");

    BackendSelection::Decision ret =
        BackendSelection::Prompt(&m_DBparams, m_pConfig);

    EndTempWindow();

    return (int)ret;
}

/**
 * If there is only a single UPnP backend, use it.
 *
 * This does <i>not</i> prompt for PIN entry. If the backend requires one,
 * it will fail, and the caller needs to put up a UI to ask for one.
 */
int MythContextPrivate::UPnPautoconf(const int milliSeconds)
{
    LOG(VB_GENERAL, LOG_INFO, QString("UPNP Search %1 secs")
        .arg(milliSeconds / 1000));

    SSDP::Instance()->PerformSearch(gBackendURI, milliSeconds / 1000);

    // Search for a total of 'milliSeconds' ms, sending new search packet
    // about every 250 ms until less than one second remains.
    MythTimer totalTime; totalTime.start();
    MythTimer searchTime; searchTime.start();
    while (totalTime.elapsed() < milliSeconds)
    {
        usleep(25000);
        int ttl = milliSeconds - totalTime.elapsed();
        if ((searchTime.elapsed() > 249) && (ttl > 1000))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("UPNP Search %1 secs")
                .arg(ttl / 1000));
            SSDP::Instance()->PerformSearch(gBackendURI, ttl / 1000);
            searchTime.start();
        }
    }

    SSDPCacheEntries *backends = SSDP::Instance()->Find(gBackendURI);

    if (!backends)
    {
        LOG(VB_GENERAL, LOG_INFO, "No UPnP backends found");
        return 0;
    }

    int count = backends->Count();
    if (count)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Found %1 UPnP backends").arg(count));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "No UPnP backends found, but SSDP::Find() not NULL");
    }

    if (count != 1)
    {
        backends->DecrRef();
        return count;
    }

    // Get this backend's location:
    DeviceLocation *BE = backends->GetFirst();
    backends->DecrRef();
    backends = NULL;

    // We don't actually know the backend's access PIN, so this will
    // only work for ones that have PIN access disabled (i.e. 0000)
    int ret = (UPnPconnect(BE, QString::null)) ? 1 : -1;

    BE->DecrRef();

    return ret;
}

/**
 * Get the default backend from config.xml, use UPnP to find it.
 *
 * Sets a string if there any connection problems
 */
bool MythContextPrivate::DefaultUPnP(QString &error)
{
    QString            loc = "DefaultUPnP() - ";
    QString            PIN = m_pConfig->GetValue(kDefaultPIN, "");
    QString            USN = m_pConfig->GetValue(kDefaultUSN, "");

    if (USN.isEmpty())
    {
        LOG(VB_UPNP, LOG_INFO, loc + "No default UPnP backend");
        return false;
    }

    LOG(VB_UPNP, LOG_INFO, loc + "config.xml has default " +
             QString("PIN '%1' and host USN: %2") .arg(PIN).arg(USN));

    // ----------------------------------------------------------------------

    int timeout_ms = 2000;
    LOG(VB_GENERAL, LOG_INFO, QString("UPNP Search up to %1 secs")
        .arg(timeout_ms / 1000));
    SSDP::Instance()->PerformSearch(gBackendURI, timeout_ms / 1000);

    // ----------------------------------------------------------------------
    // We need to give the server time to respond...
    // ----------------------------------------------------------------------

    DeviceLocation *pDevLoc = NULL;
    MythTimer totalTime; totalTime.start();
    MythTimer searchTime; searchTime.start();
    while (totalTime.elapsed() < timeout_ms)
    {
        pDevLoc = SSDP::Instance()->Find( gBackendURI, USN );

        if (pDevLoc)
            break;

        usleep(25000);

        int ttl = timeout_ms - totalTime.elapsed();
        if ((searchTime.elapsed() > 249) && (ttl > 1000))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("UPNP Search up to %1 secs")
                .arg(ttl / 1000));
            SSDP::Instance()->PerformSearch(gBackendURI, ttl / 1000);
            searchTime.start();
        }
    }

    // ----------------------------------------------------------------------

    if (!pDevLoc)
    {
        error = "Cannot find default UPnP backend";
        return false;
    }

    if (UPnPconnect(pDevLoc, PIN))
    {
        pDevLoc->DecrRef();
        return true;
    }

    pDevLoc->DecrRef();

    error = "Cannot connect to default backend via UPnP. Wrong saved PIN?";
    return false;
}

/**
 * Query a backend via UPnP for its database connection parameters
 */
bool MythContextPrivate::UPnPconnect(const DeviceLocation *backend,
                                     const QString        &PIN)
{
    QString        error;
    QString        loc = "UPnPconnect() - ";
    QString        URL = backend->m_sLocation;
    MythXMLClient  client(URL);

    LOG(VB_UPNP, LOG_INFO, loc + QString("Trying host at %1").arg(URL));
    switch (client.GetConnectionInfo(PIN, &m_DBparams, error))
    {
        case UPnPResult_Success:
            gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);
            LOG(VB_UPNP, LOG_INFO, loc +
                "Got database hostname: " + m_DBparams.dbHostName);
            return true;

        case UPnPResult_ActionNotAuthorized:
            // The stored PIN is probably not correct.
            // We could prompt for the PIN and try again, but that needs a UI.
            // Easier to fail for now, and put up the full UI selector later
            LOG(VB_UPNP, LOG_ERR, loc + "Wrong PIN?");
            return false;

        default:
            LOG(VB_UPNP, LOG_ERR, loc + error);
            break;
    }

    // This backend may have a local DB with the default user/pass/DBname.
    // For whatever reason, we have failed to get anything back via UPnP,
    // so we might as well try the database directly as a last resort.
    URL.remove("http://");
    URL.remove(QRegExp("[:/].*"));
    if (URL.isEmpty())
        return false;

    LOG(VB_UPNP, LOG_INFO, "Trying default DB credentials at " + URL);
    m_DBparams.dbHostName = URL;

    return true;
}

bool MythContextPrivate::event(QEvent *e)
{
    if (e->type() == (QEvent::Type) MythEvent::MythEventMessage)
    {
        if (disableeventpopup)
            return true;

        MythEvent *me = (MythEvent*)e;
        if (me->Message() == "VERSION_MISMATCH" && (1 == me->ExtraDataCount()))
            ShowVersionMismatchPopup(me->ExtraData(0).toUInt());
        else if (me->Message() == "CONNECTION_FAILURE")
            ShowConnectionFailurePopup(false);
        else if (me->Message() == "PERSISTENT_CONNECTION_FAILURE")
            ShowConnectionFailurePopup(true);
        else if (me->Message() == "CONNECTION_RESTABLISHED")
            HideConnectionFailurePopup();
        return true;
    }

    return QObject::event(e);
}

void MythContextPrivate::ShowConnectionFailurePopup(bool persistent)
{
    if (MBEconnectPopup)
        return;

    QString message = (persistent) ?
        QObject::tr(
            "The connection to the master backend "
            "server has gone away for some reason. "
            "Is it running?") :
        QObject::tr(
            "Could not connect to the master backend server. Is "
            "it running?  Is the IP address set for it in "
            "mythtv-setup correct?");

    if (HasMythMainWindow() && m_ui && m_ui->IsScreenSetup())
    {
        MBEconnectPopup = ShowOkPopup(
            message, m_sh, SLOT(ConnectFailurePopupClosed()));
    }
}

void MythContextPrivate::HideConnectionFailurePopup(void)
{
    MythConfirmationDialog *dlg = this->MBEconnectPopup;
    this->MBEconnectPopup = NULL;
    if (dlg)
        dlg->Close();
}

void MythContextPrivate::ShowVersionMismatchPopup(uint remote_version)
{
    if (MBEversionPopup)
        return;

    QString message =
        QObject::tr(
            "The server uses network protocol version %1, "
            "but this client only understands version %2.  "
            "Make sure you are running compatible versions of "
            "the backend and frontend.")
        .arg(remote_version).arg(MYTH_PROTO_VERSION);

    if (HasMythMainWindow() && m_ui && m_ui->IsScreenSetup())
    {
        MBEversionPopup = ShowOkPopup(
            message, m_sh, SLOT(VersionMismatchPopupClosed()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + message);
        qApp->exit(GENERIC_EXIT_SOCKET_ERROR);
    }
}

void MythContextSlotHandler::ConnectFailurePopupClosed(void)
{
    d->MBEconnectPopup = NULL;
}

void MythContextSlotHandler::VersionMismatchPopupClosed(void)
{
    d->MBEversionPopup = NULL;
    qApp->exit(GENERIC_EXIT_SOCKET_ERROR);
}

MythContext::MythContext(const QString &binversion)
    : d(NULL), app_binary_version(binversion)
{
#ifdef USING_MINGW
    static bool WSAStarted = false;
    if (!WSAStarted) {
        WSADATA wsadata;
        int res = WSAStartup(MAKEWORD(2, 0), &wsadata);
        LOG(VB_SOCKET, LOG_INFO, 
                 QString("WSAStartup returned %1").arg(res));
    }
#endif

    d = new MythContextPrivate(this);

    gCoreContext = new MythCoreContext(app_binary_version, d);

    if (!gCoreContext || !gCoreContext->Init())
    {
        LOG(VB_GENERAL, LOG_EMERG, LOC + "Unable to allocate MythCoreContext");
        qApp->exit(GENERIC_EXIT_NO_MYTHCONTEXT);
    }
}

bool MythContext::Init(const bool gui,
                       const bool promptForBackend,
                       const bool disableAutoDiscovery,
                       const bool ignoreDB)
{
    if (!d)
    {
        LOG(VB_GENERAL, LOG_EMERG, LOC + "Init() Out-of-memory");
        return false;
    }

    if (app_binary_version != MYTH_BINARY_VERSION)
    {
        LOG(VB_GENERAL, LOG_EMERG, 
                 QString("Application binary version (%1) does not "
                         "match libraries (%2)")
                     .arg(app_binary_version) .arg(MYTH_BINARY_VERSION));

        QString warning = QObject::tr(
            "This application is not compatible "
            "with the installed MythTV libraries.");
        if (gui)
        {
            d->TempMainWindow(false);
            ShowOkPopup(warning);
        }
        LOG(VB_GENERAL, LOG_WARNING, warning);

        return false;
    }

#ifdef _WIN32
    // HOME environment variable might not be defined
    // some libraries will fail without it
    QString home = getenv("HOME");
    if (home.isEmpty())
    {
        home = getenv("LOCALAPPDATA");      // Vista
        if (home.isEmpty())
            home = getenv("APPDATA");       // XP
        if (home.isEmpty())
            home = QString(".");  // getenv("TEMP")?

        _putenv(QString("HOME=%1").arg(home).toLocal8Bit().constData());
    }
#endif

    // If HOME isn't defined, we won't be able to use default confdir of
    // $HOME/.mythtv nor can we rely on a MYTHCONFDIR that references $HOME
    QString homedir = QDir::homePath();
    QString confdir = getenv("MYTHCONFDIR");
    if ((homedir.isEmpty() || homedir == "/") &&
        (confdir.isEmpty() || confdir.contains("$HOME")))
    {
        QString warning = "Cannot locate your home directory."
                          " Please set the environment variable HOME";
        if (gui)
        {
            d->TempMainWindow(false);
            ShowOkPopup(warning);
        }
        LOG(VB_GENERAL, LOG_WARNING, warning);

        return false;
    }

    if (!d->Init(gui, promptForBackend, disableAutoDiscovery, ignoreDB))
    {
        return false;
    }

    gCoreContext->ActivateSettingsCache(true);

    return true;
}

MythContext::~MythContext()
{
    if (MThreadPool::globalInstance()->activeThreadCount())
        LOG(VB_GENERAL, LOG_INFO, "Waiting for threads to exit.");

    MThreadPool::globalInstance()->waitForDone();
    logStop();

    SSDP::Shutdown();
    TaskQueue::Shutdown();

    delete gCoreContext;
    gCoreContext = NULL;

    delete d;
}

void MythContext::SetDisableEventPopup(bool check)
{
    d->disableeventpopup = check;
}

DatabaseParams MythContext::GetDatabaseParams(void)
{
    return d->m_DBparams;
}

bool MythContext::SaveDatabaseParams(const DatabaseParams &params)
{
    return d->SaveDatabaseParams(params, false);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
