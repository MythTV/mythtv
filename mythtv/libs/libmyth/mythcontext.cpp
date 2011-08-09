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
#include "oldsettings.h"
#include "util.h"
#include "remotefile.h"
#include "mythplugin.h"
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

    void LoadDatabaseSettings(void);

    bool    PromptForDatabaseParams(const QString &error);
    QString TestDBconnection(void);
    void    SilenceDBerrors(void);
    void    EnableDBerrors(void);
    void    ResetDatabase(void);

    int     ChooseBackend(const QString &error);
    int     UPnPautoconf(const int milliSeconds = 2000);
    void    StoreConnectionInfo(void);
    bool    DefaultUPnP(QString &error);
    bool    UPnPconnect(const DeviceLocation *device, const QString &PIN);

  protected:
    bool event(QEvent*);

    void ShowConnectionFailurePopup(bool persistent);
    void HideConnectionFailurePopup(void);
    void ConnectFailurePopupClosed(void);

    void ShowVersionMismatchPopup(uint remoteVersion);
    void VersionMismatchPopupClosed(void);

  public:
    MythContext *parent;

    bool      m_gui;               ///< Should this context use GUI elements?

    QString m_masterhostname;    ///< master backend hostname

    DatabaseParams  m_DBparams;  ///< Current database host & WOL details
    QString         m_DBhostCp;  ///< dbHostName backup

    Configuration    *m_pConfig;

    bool disableeventpopup;
    bool disablelibrarypopup;

    MythPluginManager *pluginmanager;

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
    MythPluginManager *pmanager = gContext->getPluginManager();
    if (pmanager)
        if (pmanager->config_plugin(cmd.trimmed()))
            ShowOkPopup(QObject::tr("Failed to configure plugin %1").arg(cmd));
}

static void plugin_cb(const QString &cmd)
{
    MythPluginManager *pmanager = gContext->getPluginManager();
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
      disablelibrarypopup(false),
      pluginmanager(NULL),
      m_ui(NULL),
      m_sh(new MythContextSlotHandler(this)),
      MBEconnectPopup(NULL),
      MBEversionPopup(NULL)
{
    InitializeMythDirs();
}

MythContextPrivate::~MythContextPrivate()
{
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

    gCoreContext->SetSetting("Theme", DEFAULT_UI_THEME);
#ifdef Q_WS_MACX
    // Qt 4.4 has window-focus problems
    gCoreContext->SetSetting("RunFrontendInWindow", "1");
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

    m_pConfig = UPnp::GetConfiguration();

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
bool MythContextPrivate::FindDatabase(const bool prompt, const bool noPrompt)
{
    // The two bool. args actually form a Yes/Maybe/No (A tristate bool :-)
    bool manualSelect = prompt && !noPrompt;

    // In addition to the UI chooser, we can also try to autoconfigure
    bool autoSelect = !manualSelect;

    QString failure;


    // 1. Load either mysql.txt, or use sensible "localhost" defaults:
    LoadDatabaseSettings();


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
            failure = "No UPnP backends found";

        if (count == 1)
        {
            failure = TestDBconnection();
            if (failure.isEmpty())
                goto DBfound;
        }

        if (count > 1 || count == -1)     // Multiple BEs, or needs PIN.
            manualSelect = !noPrompt;     // If allowed, prompt user
    }

    if (!m_gui)
        manualSelect = false;  // no interactive command-line chooser yet



    // Last, get the user to select a backend from a possible list:
    if (manualSelect)
    {
        switch (ChooseBackend(QString::null))
        {
            case -1:    // User asked to configure database manually
                if (PromptForDatabaseParams(""))
                    break;
                else
                    goto NoDBfound;   // User cancelled - changed their mind?

            case 0:   // User cancelled. Exit application
                goto NoDBfound;

            case 1:    // User selected a backend, so m_DBparams
                break; // should now contain the database details

            default:
                goto NoDBfound;
        }
        failure = TestDBconnection();
    }


    // Queries the user for the DB info, using the command
    // line or the GUI depending on the application.
    while (!failure.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ALERT, failure);
        if (( manualSelect && ChooseBackend(failure)) ||
            (!manualSelect && PromptForDatabaseParams(failure)))
        {
            failure = TestDBconnection();
            if (failure.length())
                LOG(VB_GENERAL, LOG_ALERT, failure);
        }
        else
            goto NoDBfound;
    }

DBfound:
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "FindDatabase() - Success!");
#endif
    StoreConnectionInfo();
    EnableDBerrors();
    ResetDatabase();
    return true;

NoDBfound:
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "FindDatabase() - failed");
#endif
    return false;
}

/**
 * Load database and host settings from mysql.txt, or set some defaults
 *
 * \returns true if mysql.txt was parsed
 */
void MythContextPrivate::LoadDatabaseSettings(void)
{
    gCoreContext->GetDB()->LoadDatabaseParamsFromDisk(m_DBparams, true);
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

    LOG(VB_GENERAL, LOG_INFO, QString("Using localhost value of %1")
            .arg(hostname));
    gCoreContext->SetLocalHostname(hostname);
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
    int     port   = m_DBparams.dbPort;


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
        err = QObject::tr("Cannot find (ping) database host %1 on the network");
        return err.arg(host);
    }


    // 2. Check that the supplied DBport is listening:

    if (host != "localhost" && port && !telnet(host, port))
    {
        SilenceDBerrors();
        err = QObject::tr("Cannot connect to port %1 on database host %2");
        return err.arg(port).arg(host);
    }


    // 3. Finally, try to login, et c:

    // Current DB connection may have been silenced (invalid):
    ResetDatabase();

    if (!MSqlQuery::testDBConnection())
    {
        SilenceDBerrors();
        return QObject::tr("Cannot login to database?");
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
    if (error.length())
        ShowOkPopup(error);

    LOG(VB_GENERAL, LOG_INFO, "Putting up the UPnP backend chooser");

    BackendSelection::prompt(&m_DBparams, m_pConfig);

    EndTempWindow();

    return 1;
}

/**
 * Try to store the current location of this backend in config.xml
 *
 * This is intended as a last resort for future connections
 * (e.g. Perl scripts, backend not running).
 *
 * Note that the Save() may fail (e.g. non writable ~/.mythtv)
 */
void MythContextPrivate::StoreConnectionInfo(void)
{
    if (!m_pConfig)
        return;

    m_pConfig->SetValue(kDefaultBE + "DBHostName", m_DBparams.dbHostName);
    m_pConfig->SetValue(kDefaultBE + "DBUserName", m_DBparams.dbUserName);
    m_pConfig->SetValue(kDefaultBE + "DBPassword", m_DBparams.dbPassword);
    m_pConfig->SetValue(kDefaultBE + "DBName",     m_DBparams.dbName);
    m_pConfig->SetValue(kDefaultBE + "DBPort",     m_DBparams.dbPort);
    m_pConfig->Save();
}

/**
 * If there is only a single UPnP backend, use it.
 *
 * This does <i>not</i> prompt for PIN entry. If the backend requires one,
 * it will fail, and the caller needs to put up a UI to ask for one.
 */
int MythContextPrivate::UPnPautoconf(const int milliSeconds)
{
    SSDPCacheEntries *backends = NULL;
    int               count;
    QString           loc = "UPnPautoconf() - ";
    QTime             timer;

    SSDP::Instance()->PerformSearch( gBackendURI );

    for (timer.start(); timer.elapsed() < milliSeconds; )
    {
        usleep(25000);
        backends = SSDP::Instance()->Find( gBackendURI );
        if (backends)
            break;
#if 0
        putchar('.');
#endif
    }
#if 0
    putchar('\n');
#endif

    if (!backends)
    {
        LOG(VB_GENERAL, LOG_INFO, loc + "No UPnP backends found");
        return 0;
    }

    count = backends->Count();
    switch (count)
    {
        case 0:
            LOG(VB_GENERAL, LOG_ALERT, loc +
                "No UPnP backends found, but SSDP::Find() not NULL!");
            break;
        case 1:
            LOG(VB_GENERAL, LOG_INFO, loc + "Found one UPnP backend");
            break;
        default:
            LOG(VB_GENERAL, LOG_INFO, loc +
                QString("More than one UPnP backend found (%1)") .arg(count));
    }

    if (count != 1)
    {
        backends->Release();
        return count;
    }

    // Get this backend's location:
    DeviceLocation *BE = backends->GetFirst();
    backends->Release();

    // We don't actually know the backend's access PIN, so this will
    // only work for ones that have PIN access disabled (i.e. 0000)
    int ret = (UPnPconnect(BE, QString::null)) ? 1 : -1;

    BE->Release();

    return ret;
}

/**
 * Get the default backend from config.xml, use UPnP to find it.
 *
 * Sets a string if there any connection problems
 */
bool MythContextPrivate::DefaultUPnP(QString &error)
{
    Configuration *pConfig = new XmlConfiguration("config.xml");
    QString            loc = "DefaultUPnP() - ";
    QString  localHostName = pConfig->GetValue(kDefaultBE + "LocalHostName", "");
    QString            PIN = pConfig->GetValue(kDefaultPIN, "");
    QString            USN = pConfig->GetValue(kDefaultUSN, "");

    delete pConfig;

    if (USN.isEmpty())
    {
        LOG(VB_UPNP, LOG_INFO, loc + "No default UPnP backend");
        return false;
    }

    LOG(VB_UPNP, LOG_INFO, loc + "config.xml has default " +
             QString("PIN '%1' and host USN: %2") .arg(PIN).arg(USN));

    // ----------------------------------------------------------------------

    SSDP::Instance()->PerformSearch( gBackendURI );

    // ----------------------------------------------------------------------
    // We need to give the server time to respond...
    // ----------------------------------------------------------------------

    DeviceLocation *pDevLoc = NULL;
    QTime           timer;

    for (timer.start(); timer.elapsed() < 5000; )
    {
        usleep(25000);
        pDevLoc = SSDP::Instance()->Find( gBackendURI, USN );

        if (pDevLoc)
            break;

#if 0
        putchar('.');
#endif
    }
#if 0
    putchar('\n');
#endif

    // ----------------------------------------------------------------------

    if (!pDevLoc)
    {
        error = "Cannot find default UPnP backend";
        return false;
    }

    if (UPnPconnect(pDevLoc, PIN))
    {
        if (localHostName.length())
        {
            m_DBparams.localHostName = localHostName;
            m_DBparams.localEnabled  = true;
            gCoreContext->GetDB()->SetDatabaseParams(m_DBparams);
        }

        pDevLoc->Release();

        return true;
    }

    pDevLoc->Release();

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
    if (MBEconnectPopup)
    {
        MBEconnectPopup->Close();
        MBEconnectPopup = NULL;
    }
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

    delete gCoreContext;
    gCoreContext = NULL;

    delete d;
}

bool MythContext::TestPopupVersion(const QString &name,
                                   const QString &libversion,
                                   const QString &pluginversion)
{
    if (libversion == pluginversion)
        return true;

    QString err = QObject::tr(
        "Plugin %1 is not compatible with the installed MythTV "
        "libraries.");

    LOG(VB_GENERAL, LOG_EMERG, 
             QString("Plugin %1 (%2) binary version does not "
                     "match libraries (%3)")
                 .arg(name).arg(pluginversion).arg(libversion));

    if (GetMythMainWindow() && !d->disablelibrarypopup)
        ShowOkPopup(err.arg(name));

    return false;
}

void MythContext::SetDisableEventPopup(bool check)
{
    d->disableeventpopup = check;
}

void MythContext::SetDisableLibraryPopup(bool check)
{
    d->disablelibrarypopup = check;
}

void MythContext::SetPluginManager(MythPluginManager *pmanager)
{
    d->pluginmanager = pmanager;
}

MythPluginManager *MythContext::getPluginManager(void)
{
    return d->pluginmanager;
}

DatabaseParams MythContext::GetDatabaseParams(void)
{
    return d->m_DBparams;
}

bool MythContext::SaveDatabaseParams(const DatabaseParams &params)
{
    bool ret = true;
    DatabaseParams cur_params = GetDatabaseParams();

    // only rewrite file if it has changed
    if (params.dbHostName   != cur_params.dbHostName          ||
        params.dbHostPing   != cur_params.dbHostPing          ||
        params.dbPort       != cur_params.dbPort              ||
        params.dbUserName   != cur_params.dbUserName          ||
        params.dbPassword   != cur_params.dbPassword          ||
        params.dbName       != cur_params.dbName              ||
        params.dbType       != cur_params.dbType              ||
        params.localEnabled != cur_params.localEnabled        ||
        params.wolEnabled   != cur_params.wolEnabled          ||
        (params.localEnabled &&
         (params.localHostName != cur_params.localHostName))  ||
        (params.wolEnabled &&
         (params.wolReconnect  != cur_params.wolReconnect ||
          params.wolRetry      != cur_params.wolRetry     ||
          params.wolCommand    != cur_params.wolCommand)))
    {
        ret = MythDB::SaveDatabaseParamsToDisk(params, GetConfDir(), true);
        if (ret)
        {
            // Save the new settings:
            d->m_DBparams = params;
            gCoreContext->GetDB()->SetDatabaseParams(d->m_DBparams);

            // If database has changed, force its use:
            d->ResetDatabase();
        }
    }
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
