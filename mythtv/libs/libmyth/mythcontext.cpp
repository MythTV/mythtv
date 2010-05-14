#include <QCoreApplication>
#include <QPixmap>
#include <QImage>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDesktopWidget>
#include <QPainter>
#include <QDebug>
#include <QMutex>

#include <cmath>

#include <queue>
#include <algorithm>
using namespace std;

#include "config.h"
#include "mythcontext.h"
#include "mythsocket.h"
#include "exitcodes.h"
#include "oldsettings.h"
#include "util.h"
#include "remotefile.h"
#include "dialogbox.h"
#include "mythdialogs.h"
#include "mythplugin.h"
#include "backendselect.h"
#include "dbsettings.h"
#include "langsettings.h"
#include "mythxdisplay.h"
#include "mythsocket.h"
#include "themeinfo.h"
#include "dbutil.h"
#include "DisplayRes.h"

#include "mythdb.h"
#include "mythdirs.h"
#include "mythversion.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythimage.h"
#include "mythxmlclient.h"
#include "upnp.h"

#ifdef USING_MINGW
#include <winsock2.h>
#include <unistd.h>
#include "compat.h"
#endif

#define LOC      QString("MythContext: ")
#define LOC_WARN QString("MythContext, Warning: ")
#define LOC_ERR  QString("MythContext, Error: ")

MythContext *gContext = NULL;
QMutex *avcodeclock = new QMutex(QMutex::Recursive);

// Some common UPnP search and XML value strings
const QString gBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";
const QString kDefaultBE  = "UPnP/MythFrontend/DefaultBackend/";
const QString kDefaultPIN = kDefaultBE + "SecurityPin";
const QString kDefaultUSN = kDefaultBE + "USN";

class MythContextPrivate : public QObject
{
    friend class MythContextSlotHandler;

  public:
    MythContextPrivate(MythContext *lparent);
   ~MythContextPrivate();

    bool Init        (const bool gui,    UPnp *UPnPclient,
                      const bool prompt, const bool noPrompt,
                      const bool ignoreDB);
    bool FindDatabase(const bool prompt, const bool noPrompt);

    void TempMainWindow(bool languagePrompt = true);
    void EndTempWindow(void);

    void LoadLogSettings(void);
    void LoadDatabaseSettings(void);

    bool LoadSettingsFile(void);
    bool WriteSettingsFile(const DatabaseParams &params,
                           bool overwrite = false);
    bool FindSettingsProbs(void);

    bool    PromptForDatabaseParams(const QString &error);
    QString TestDBconnection(void);
    void    SilenceDBerrors(void);
    void    EnableDBerrors(void);
    void    ResetDatabase(void);

    bool    InitUPnP(void);
    void    DeleteUPnP(void);
    int     ChooseBackend(const QString &error);
    int     UPnPautoconf(const int milliSeconds = 2000);
    void    StoreConnectionInfo(void);
    bool    DefaultUPnP(QString &error);
    bool    UPnPconnect(const DeviceLocation *device, const QString &PIN);

    bool WaitForWOL(int timeout_ms = INT_MAX);

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
    bool      m_backend;           ///< Is this host any sort of backend?

    QMutex  m_hostnamelock;      ///< Locking for thread-safe copying of:
    QString m_localhostname;     ///< hostname from mysql.txt or gethostname()
    QString m_masterhostname;    ///< master backend hostname

    DatabaseParams  m_DBparams;  ///< Current database host & WOL details
    QString         m_DBhostCp;  ///< dbHostName backup

    UPnp             *m_UPnP;    ///< For automatic backend discover
    XmlConfiguration *m_XML;
    HttpServer       *m_HTTP;

    QMutex         WOLInProgressLock;
    QWaitCondition WOLInProgressWaitCondition;
    bool           WOLInProgress;

    MythMainWindow *mainWindow;

    QMutex      sockLock;        ///< protects both serverSock and eventSock
    MythSocket *serverSock;      ///< socket for sending MythProto requests
    MythSocket *eventSock;       ///< socket events arrive on

    bool disablelibrarypopup;

    MythPluginManager *pluginmanager;

    int m_logenable, m_logmaxcount, m_logprintlevel;
    QMap<QString,int> lastLogCounts;
    QMap<QString,QString> lastLogStrings;

    QMutex m_priv_mutex;
    queue<MythPrivRequest> m_priv_requests;
    QWaitCondition m_priv_queued;

    MythDB *m_database;
    MythUIHelper *m_ui;
    MythContextSlotHandler *m_sh;

    QThread *m_UIThread;

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

    gContext->SendReceiveStringList(strlist);
    int cardid = strlist[0].toInt();

    if (cardid >= 0)
    {
        s = s.sprintf(qPrintable(s),
                      qPrintable(strlist[1]),
                      qPrintable(strlist[2]),
                      qPrintable(strlist[3]));

        myth_system(s);

        strlist = QStringList(QString("FREE_TUNER %1").arg(cardid));
        gContext->SendReceiveStringList(strlist);
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

        VERBOSE(VB_IMPORTANT, QString("exec_program_tv: ") + label);

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
    myth_eject();
}

MythContextPrivate::MythContextPrivate(MythContext *lparent)
    : parent(lparent),
      m_gui(false), m_backend(false),
      m_localhostname(QString::null),
      m_UPnP(NULL), m_XML(NULL), m_HTTP(NULL),
      WOLInProgress(false),
      mainWindow(NULL),
      sockLock(QMutex::NonRecursive),
      serverSock(NULL), eventSock(NULL),
      disablelibrarypopup(false),
      pluginmanager(NULL),
      m_logenable(-1), m_logmaxcount(-1), m_logprintlevel(-1),
      m_database(GetMythDB()), m_ui(NULL),
      m_sh(new MythContextSlotHandler(this)),
      m_UIThread(QThread::currentThread()),
      MBEconnectPopup(NULL),
      MBEversionPopup(NULL)
{
    InitializeMythDirs();
}

MythContextPrivate::~MythContextPrivate()
{
    DeleteUPnP();
    QMutexLocker locker(&sockLock);
    if (serverSock)
    {
        serverSock->DownRef();
        serverSock = NULL;
    }
    if (eventSock)
    {
        eventSock->DownRef();
        eventSock = NULL;
    }
    if (m_database)
        DestroyMythDB();
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
    if (mainWindow)
        return;

    SilenceDBerrors();

    m_database->SetSetting("Theme", DEFAULT_UI_THEME);
#ifdef Q_WS_MACX
    // Qt 4.4 has window-focus problems
    m_database->SetSetting("RunFrontendInWindow", "1");
#endif
    GetMythUI()->LoadQtConfig();

    MythMainWindow *mainWindow = MythMainWindow::getMainWindow(false);
    mainWindow->Init();
    parent->SetMainWindow(mainWindow);

    if (languagePrompt)
    {
        // ask user for language settings
        LanguageSettings::prompt();
        LanguageSettings::load("mythfrontend");
    }
}

void MythContextPrivate::EndTempWindow(void)
{
    parent->SetMainWindow(NULL);
    DestroyMythMainWindow();
    EnableDBerrors();
}

bool MythContextPrivate::Init(const bool gui, UPnp *UPnPclient,
                              const bool promptForBackend,
                              const bool noPrompt,
                              const bool ignoreDB)
{
    m_database->IgnoreDatabase(ignoreDB);
    m_gui = gui;
    if (UPnPclient)
    {
        m_UPnP = UPnPclient;
#ifndef _WIN32
        m_XML  = (XmlConfiguration *)UPnp::g_pConfig;
#endif
    }

    // Creates screen saver control if we will have a GUI
    if (gui)
        m_ui = GetMythUI();

    // ---- database connection stuff ----

    if (!ignoreDB && !FindDatabase(promptForBackend, noPrompt))
        return false;

    // ---- keep all DB-using stuff below this line ----

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
                VERBOSE(VB_IMPORTANT, failure);

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
        VERBOSE(VB_IMPORTANT, QString("%1").arg(failure));
        if (( manualSelect && ChooseBackend(failure)) ||
            (!manualSelect && PromptForDatabaseParams(failure)))
        {
            failure = TestDBconnection();
            if (failure.length())
                VERBOSE(VB_IMPORTANT, QString("%1").arg(failure));
        }
        else
            goto NoDBfound;
    }

DBfound:
    //VERBOSE(VB_GENERAL, "FindDatabase() - Success!");
    StoreConnectionInfo();
    EnableDBerrors();
    ResetDatabase();
    DeleteUPnP();
    return true;

NoDBfound:
    //VERBOSE(VB_GENERAL, "FindDatabase() - failed");
    DeleteUPnP();
    return false;
}

void MythContextPrivate::LoadLogSettings(void)
{
    m_logenable = parent->GetNumSetting("LogEnabled", 0);
    m_logmaxcount = parent->GetNumSetting("LogMaxCount", 0);
    m_logprintlevel = parent->GetNumSetting("LogPrintLevel", LP_ERROR);
}

/**
 * Load database and host settings from mysql.txt, or set some defaults
 *
 * \returns true if mysql.txt was parsed
 */
void MythContextPrivate::LoadDatabaseSettings(void)
{
    if (!LoadSettingsFile())
    {
        VERBOSE(VB_IMPORTANT, "Unable to read configuration file mysql.txt");

        // Sensible connection defaults.
        m_DBparams.dbHostName    = "localhost";
        m_DBparams.dbHostPing    = true;
        m_DBparams.dbPort        = 0;
        m_DBparams.dbUserName    = "mythtv";
        m_DBparams.dbPassword    = "mythtv";
        m_DBparams.dbName        = "mythconverg";
        m_DBparams.dbType        = "QMYSQL3";
        m_DBparams.localEnabled  = false;
        m_DBparams.localHostName = "my-unique-identifier-goes-here";
        m_DBparams.wolEnabled    = false;
        m_DBparams.wolReconnect  = 0;
        m_DBparams.wolRetry      = 5;
        m_DBparams.wolCommand    = "echo 'WOLsqlServerCommand not set'";
        m_database->SetDatabaseParams(m_DBparams);
    }

    // Even if we have loaded the settings file, it may be incomplete,
    // so we check for missing values and warn user
    FindSettingsProbs();

    m_localhostname = m_DBparams.localHostName;
    if (m_localhostname.isEmpty() ||
        m_localhostname == "my-unique-identifier-goes-here")
    {
        char localhostname[1024];
        if (gethostname(localhostname, 1024))
        {
            VERBOSE(VB_IMPORTANT,
                    "MCP: Error, could not determine host name." + ENO);
            localhostname[0] = '\0';
        }
        m_localhostname = localhostname;
        VERBOSE(VB_IMPORTANT, "Empty LocalHostName.");
    }

    VERBOSE(VB_GENERAL, QString("Using localhost value of %1")
            .arg(m_localhostname));
    m_database->SetLocalHostname(m_localhostname);
}

/**
 * Load mysql.txt and parse its values into m_DBparams
 */
bool MythContextPrivate::LoadSettingsFile(void)
{
    Settings *oldsettings = m_database->GetOldSettings();

    if (!oldsettings->LoadSettingsFiles("mysql.txt", GetInstallPrefix(),
                                        GetConfDir()))
        return false;

    m_DBparams.dbHostName = oldsettings->GetSetting("DBHostName");
    m_DBparams.dbHostPing = oldsettings->GetSetting("DBHostPing") != "no";
    m_DBparams.dbPort     = oldsettings->GetNumSetting("DBPort");
    m_DBparams.dbUserName = oldsettings->GetSetting("DBUserName");
    m_DBparams.dbPassword = oldsettings->GetSetting("DBPassword");
    m_DBparams.dbName     = oldsettings->GetSetting("DBName");
    m_DBparams.dbType     = oldsettings->GetSetting("DBType");

    m_DBparams.localHostName = oldsettings->GetSetting("LocalHostName");
    m_DBparams.localEnabled  = m_DBparams.localHostName.length() > 0;

    m_DBparams.wolReconnect
        = oldsettings->GetNumSetting("WOLsqlReconnectWaitTime");
    m_DBparams.wolEnabled = m_DBparams.wolReconnect > 0;

    m_DBparams.wolRetry   = oldsettings->GetNumSetting("WOLsqlConnectRetry");
    m_DBparams.wolCommand = oldsettings->GetSetting("WOLsqlCommand");
    m_database->SetDatabaseParams(m_DBparams);

    return true;
}

bool MythContextPrivate::WriteSettingsFile(const DatabaseParams &params,
                                           bool overwrite)
{
    QString path = GetConfDir() + "/mysql.txt";
    QFile   * f  = new QFile(path);

    if (!overwrite && f->exists())
    {
        return false;
    }

    QString dirpath = GetConfDir();
    QDir createDir(dirpath);

    if (!createDir.exists())
    {
        if (!createDir.mkdir(dirpath))
        {
            VERBOSE(VB_IMPORTANT, QString("Could not create %1").arg(dirpath));
            return false;
        }
    }

    if (!f->open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("Could not open settings file %1 "
                                      "for writing").arg(path));
        return false;
    }

    VERBOSE(VB_IMPORTANT, QString("Writing settings file %1").arg(path));
    QTextStream s(f);
    s << "DBHostName=" << params.dbHostName << endl;

    s << "\n"
      << "# By default, Myth tries to ping the DB host to see if it exists.\n"
      << "# If your DB host or network doesn't accept pings, set this to no:\n"
      << "#\n";

    if (params.dbHostPing)
        s << "#DBHostPing=no" << endl << endl;
    else
        s << "DBHostPing=no" << endl << endl;

    if (params.dbPort)
        s << "DBPort=" << params.dbPort << endl;

    s << "DBUserName=" << params.dbUserName << endl
      << "DBPassword=" << params.dbPassword << endl
      << "DBName="     << params.dbName     << endl
      << "DBType="     << params.dbType     << endl
      << endl
      << "# Set the following if you want to use something other than this\n"
      << "# machine's real hostname for identifying settings in the database.\n"
      << "# This is useful if your hostname changes often, as otherwise you\n"
      << "# will need to reconfigure mythtv every time.\n"
      << "# NO TWO HOSTS MAY USE THE SAME VALUE\n"
      << "#\n";

    if (params.localEnabled)
        s << "LocalHostName=" << params.localHostName << endl;
    else
        s << "#LocalHostName=my-unique-identifier-goes-here\n";

    s << endl
      << "# If you want your frontend to be able to wake your MySQL server\n"
      << "# using WakeOnLan, have a look at the following settings:\n"
      << "#\n"
      << "#\n"
      << "# The time the frontend waits (in seconds) between reconnect tries.\n"
      << "# This should be the rough time your MySQL server needs for startup\n"
      << "#\n";

    if (params.wolEnabled)
        s << "WOLsqlReconnectWaitTime=" << params.wolReconnect << endl;
    else
        s << "#WOLsqlReconnectWaitTime=0\n";

    s << "#\n"
      << "#\n"
      << "# This is the number of retries to wake the MySQL server\n"
      << "# until the frontend gives up\n"
      << "#\n";

    if (params.wolEnabled)
        s << "WOLsqlConnectRetry=" << params.wolRetry << endl;
    else
        s << "#WOLsqlConnectRetry=5\n";

    s << "#\n"
      << "#\n"
      << "# This is the command executed to wake your MySQL server.\n"
      << "#\n";

    if (params.wolEnabled)
        s << "WOLsqlCommand=" << params.wolCommand << endl;
    else
        s << "#WOLsqlCommand=echo 'WOLsqlServerCommand not set'\n";

    f->close();
    return true;
}

bool MythContextPrivate::FindSettingsProbs(void)
{
    bool problems = false;

    if (m_DBparams.dbHostName.isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBHostName is not set in mysql.txt");
        VERBOSE(VB_IMPORTANT, "Assuming localhost");
        m_DBparams.dbHostName = "localhost";
    }
    if (m_DBparams.dbUserName.isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBUserName is not set in mysql.txt");
    }
    if (m_DBparams.dbPassword.isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBPassword is not set in mysql.txt");
    }
    if (m_DBparams.dbName.isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBName is not set in mysql.txt");
    }
    m_database->SetDatabaseParams(m_DBparams);
    return problems;
}

bool MythContextPrivate::PromptForDatabaseParams(const QString &error)
{
    bool accepted = false;
    if (m_gui)
    {
        TempMainWindow();

        // Tell the user what went wrong:
        if (error.length())
            MythPopupBox::showOkPopup(mainWindow, "DB connect failure", error);

        // ask user for database parameters
        DatabaseSettings settings(m_DBhostCp);
        accepted = (settings.exec() == QDialog::Accepted);
        if (!accepted)
            VERBOSE(VB_IMPORTANT, "User cancelled database configuration");

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
    if (host == "localhost" || host == "127.0.0.1" || host == m_localhostname)
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

            VERBOSE(VB_GENERAL, QString("Trying to wake up host %1, attempt %2")
                                .arg(host).arg(attempt));
            myth_system(m_DBparams.wolCommand.toLocal8Bit().constData());

            VERBOSE(VB_GENERAL,
                    QString("Waiting for %1 seconds").arg(wakeupTime));
            sleep(m_DBparams.wolReconnect);
        }

    if (doPing)
    {
        VERBOSE(VB_GENERAL,
                QString("Testing network connectivity to '%1'").arg(host));
    }

    if (doPing && !ping(host, 3))  // Fail after trying for 3 seconds
    {
        SilenceDBerrors();
        err = QObject::tr("Cannot find (ping) database host %1 on the network");
        return err.arg(host);
    }


    // 2. Check that the supplied DBport is listening:

    if (port && !telnet(host, port))
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
    m_database->IgnoreDatabase(true);

    // Save the configured hostname, so that we can
    // still display it in the DatabaseSettings screens
    if (m_DBparams.dbHostName.length())
        m_DBhostCp = m_DBparams.dbHostName;

    m_DBparams.dbHostName = "";
    m_database->SetDatabaseParams(m_DBparams);
}

void MythContextPrivate::EnableDBerrors(void)
{
    // Restore (possibly) blanked hostname
    if (m_DBparams.dbHostName.isNull() && m_DBhostCp.length())
    {
        m_DBparams.dbHostName = m_DBhostCp;
        m_database->SetDatabaseParams(m_DBparams);
    }

    m_database->IgnoreDatabase(false);
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
    m_database->GetDBManager()->CloseDatabases();
    m_database->SetDatabaseParams(m_DBparams);
    parent->ClearSettingsCache();
}


bool MythContextPrivate::InitUPnP(void)
{
    if (m_UPnP)
        return true;

    VERBOSE(VB_UPNP, "Setting UPnP client for backend autodiscovery...");

    if (!m_XML)
        m_XML = new XmlConfiguration("");   // No file - use defaults only

    m_UPnP = new UPnp();
    m_UPnP->SetConfiguration(m_XML);

    int port=6549;
    m_HTTP = new HttpServer();

    if (!m_HTTP->listen(QHostAddress::Any,port))
    {
        VERBOSE(VB_IMPORTANT, "MCP::InitUPnP() - HttpServer Create Error");
        DeleteUPnP();
        return false;
    }

    if (!m_UPnP->Initialize(port, m_HTTP))
    {
        VERBOSE(VB_IMPORTANT, "MCP::InitUPnP() - UPnp::Initialize() Error");
        DeleteUPnP();
        return false;
    }

    // Create a dummy device description
    UPnpDevice   &device = UPnp::g_UPnpDeviceDesc.m_rootDevice;
    device.m_sDeviceType = "urn:schemas-upnp-org:device:MythContextClient:1";

    m_UPnP->Start();

    return true;
}

void MythContextPrivate::DeleteUPnP(void)
{
    if (m_UPnP && !m_HTTP)  // Init was passed an existing UPnP
        return;             // so let the caller delete it cleanly

    if (m_UPnP)
    {
        // This takes a few seconds, so inform the user:
        VERBOSE(VB_GENERAL, "Deleting UPnP client...");

        delete m_UPnP;  // This also deletes m_XML
        m_UPnP = NULL;
        m_XML  = NULL;
    }

    if (m_HTTP)
    {
        delete m_HTTP;
        m_HTTP = NULL;
    }
}

/**
 * Search for backends via UPnP, put up a UI for the user to choose one
 */
int MythContextPrivate::ChooseBackend(const QString &error)
{
    if (!InitUPnP())
        return -1;

    TempMainWindow();

    // Tell the user what went wrong:
    if (error.length())
        MythPopupBox::showOkPopup(mainWindow, "DB connect failure", error);

    VERBOSE(VB_GENERAL, "Putting up the UPnP backend chooser");

    BackendSelect *BEsel = new BackendSelect(mainWindow, &m_DBparams);
    switch (BEsel->exec())
    {
        case kDialogCodeRejected:
            VERBOSE(VB_IMPORTANT, "User canceled database configuration");
            return 0;

        case kDialogCodeButton0:
            VERBOSE(VB_IMPORTANT, "User requested Manual Config");
            return -1;
    }
    //BEsel->hide();
    //BEsel->deleteLater();

    QStringList buttons;
    QString     message;

    buttons += QObject::tr("Save database details");
    buttons += QObject::tr("Save backend details");
    buttons += QObject::tr("Don't Save");

    message = QObject::tr("Save that backend or database as the default?");

    DialogCode selected = MythPopupBox::ShowButtonPopup(
        mainWindow, "Save default", message, buttons, kDialogCodeButton2);
    switch (selected)
    {
        case kDialogCodeButton0:
            WriteSettingsFile(m_DBparams, true);
            // User prefers mysql.txt, so throw away default UPnP backend:
            m_XML->SetValue(kDefaultUSN, "");
            m_XML->Save();
            break;
        case kDialogCodeButton1:
            if (BEsel->m_PIN.length())
                m_XML->SetValue(kDefaultPIN, BEsel->m_PIN);
            m_XML->SetValue(kDefaultUSN, BEsel->m_USN);
            m_XML->Save();
            break;
    }

    delete BEsel;
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
    if (!m_XML)
        return;

    m_XML->SetValue(kDefaultBE + "DBHostName", m_DBparams.dbHostName);
    m_XML->SetValue(kDefaultBE + "DBUserName", m_DBparams.dbUserName);
    m_XML->SetValue(kDefaultBE + "DBPassword", m_DBparams.dbPassword);
    m_XML->SetValue(kDefaultBE + "DBName",     m_DBparams.dbName);
    m_XML->SetValue(kDefaultBE + "DBPort",     m_DBparams.dbPort);
    m_XML->Save();
}

/**
 * If there is only a single UPnP backend, use it.
 *
 * This does <i>not</i> prompt for PIN entry. If the backend requires one,
 * it will fail, and the caller needs to put up a UI to ask for one.
 */
int MythContextPrivate::UPnPautoconf(const int milliSeconds)
{
    if (!InitUPnP())
        return 0;

    SSDPCacheEntries *backends = NULL;
    int               count;
    QString           loc = "UPnPautoconf() - ";
    QTime             timer;

    m_UPnP->PerformSearch(gBackendURI);
    for (timer.start(); timer.elapsed() < milliSeconds; usleep(25000))
    {
        backends = m_UPnP->g_SSDPCache.Find(gBackendURI);
        if (backends)
        {
            backends->AddRef();
            break;
        }
        putchar('.');
    }
    putchar('\n');

    if (!backends)
    {
        VERBOSE(VB_GENERAL, loc + "No UPnP backends found");
        return 0;
    }


    // This could be tied to VB_UPNP?
    //m_UPnP->g_SSDPCache.Dump();


    count = backends->Count();
    switch (count)
    {
        case 0:
            VERBOSE(VB_IMPORTANT,
                    loc + "No UPnP backends found, but SSDP::Find() not NULL!");
            break;
        case 1:
            VERBOSE(VB_GENERAL, loc + "Found one UPnP backend");
            break;
        default:
            VERBOSE(VB_GENERAL,
                    (loc + "More than one UPnP backend found (%1)").arg(count));
    }

    if (count != 1)
    {
        backends->Release();
        return count;
    }


    // Get this backend's location:
    backends->Lock();
    DeviceLocation *BE = *(backends->GetEntryMap()->begin());
    backends->Unlock();
    backends->Release();

    // We don't actually know the backend's access PIN, so this will
    // only work for ones that have PIN access disabled (i.e. 0000)
    if (UPnPconnect(BE, QString::null))
        return 1;

    return -1;   // Try to force chooser & PIN
}

/**
 * Get the default backend from config.xml, use UPnP to find it.
 *
 * Sets a string if there any connection problems
 */
bool MythContextPrivate::DefaultUPnP(QString &error)
{
    XmlConfiguration *XML = new XmlConfiguration("config.xml");
    QString           loc = "MCP::DefaultUPnP() - ";
    QString localHostName = XML->GetValue(kDefaultBE + "LocalHostName", "");
    QString           PIN = XML->GetValue(kDefaultPIN, "");
    QString           USN = XML->GetValue(kDefaultUSN, "");

    delete XML;

    if (USN.isEmpty())
    {
        VERBOSE(VB_UPNP, loc + "No default UPnP backend");
        return false;
    }

    VERBOSE(VB_UPNP, loc + "config.xml has default " +
            QString("PIN '%1' and host USN: %2")
            .arg(PIN).arg(USN));

    if (!InitUPnP())
    {
        error = "UPnP is broken?";
        return false;
    }

    m_UPnP->PerformSearch(gBackendURI);
    DeviceLocation *pDevLoc = m_UPnP->g_SSDPCache.Find(gBackendURI, USN);
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
            m_database->SetDatabaseParams(m_DBparams);
        }

        return true;
    }

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
    MythXMLClient  XML(URL);

    VERBOSE(VB_UPNP, loc + QString("Trying host at %1").arg(URL));
    switch (XML.GetConnectionInfo(PIN, &m_DBparams, error))
    {
        case UPnPResult_Success:
            m_database->SetDatabaseParams(m_DBparams);
            VERBOSE(VB_UPNP,
                    loc + "Got database hostname: " + m_DBparams.dbHostName);
            return true;

        case UPnPResult_ActionNotAuthorized:
            // The stored PIN is probably not correct.
            // We could prompt for the PIN and try again, but that needs a UI.
            // Easier to fail for now, and put up the full UI selector later
            VERBOSE(VB_UPNP, loc + error + ". Wrong PIN?");
            break;

        default:
            VERBOSE(VB_UPNP, loc + error);
            break;
    }

    // This backend may have a local DB with the default user/pass/DBname.
    // For whatever reason, we have failed to get anything back via UPnP,
    // so we might as well try the database directly as a last resort.
    URL.remove("http://");
    URL.remove(QRegExp("[:/].*"));
    if (URL.isEmpty())
        return false;

    VERBOSE(VB_UPNP, "Trying default DB credentials at " + URL);
    m_DBparams.dbHostName = URL;

    return true;
}

/// If another thread has already started WOL process, wait on them...
///
/// Note: Caller must be holding WOLInProgressLock.
bool MythContextPrivate::WaitForWOL(int timeout_in_ms)
{
    int timeout_remaining = timeout_in_ms;
    while (WOLInProgress && (timeout_remaining > 0))
    {
        VERBOSE(VB_GENERAL, LOC + "Wake-On-LAN in progress, waiting...");

        int max_wait = min(1000, timeout_remaining);
        WOLInProgressWaitCondition.wait(
            &WOLInProgressLock, max_wait);
        timeout_remaining -= max_wait;
    }

    return !WOLInProgress;
}

bool MythContextPrivate::event(QEvent *e)
{
    if (e->type() == (QEvent::Type) MythEvent::MythEventMessage)
    {
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
            "server has gone away for some reason.. "
            "Is it running?") :
        QObject::tr(
            "Could not connect to the master backend server -- is "
            "it running?  Is the IP address set for it in the "
            "setup program correct?");

    if (mainWindow && m_ui && m_ui->IsScreenSetup())
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

    if (mainWindow && m_ui && m_ui->IsScreenSetup())
    {
        MBEversionPopup = ShowOkPopup(
            message, m_sh, SLOT(VersionMismatchPopupClosed()));
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + message);
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
        VERBOSE(VB_SOCKET, QString("WSAStartup returned %1").arg(res));
    }
#endif

    d = new MythContextPrivate(this);
}

bool MythContext::Init(const bool gui, UPnp *UPnPclient,
                       const bool promptForBackend,
                       const bool disableAutoDiscovery,
                       const bool ignoreDB)
{
    if (!d)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Init() Out-of-memory");
        return false;
    }

    if (app_binary_version != MYTH_BINARY_VERSION)
    {
        VERBOSE(VB_GENERAL, QString("Application binary version (%1) does not "
                                    "match libraries (%2)")
                                    .arg(app_binary_version)
                                    .arg(MYTH_BINARY_VERSION));

        QString warning = QObject::tr(
            "This application is not compatible "
            "with the installed MythTV libraries. "
            "Please recompile after a make distclean");
        if (gui)
        {
            d->TempMainWindow(false);
            MythPopupBox::showOkPopup(d->mainWindow,
                                      "Library version error", warning);
        }
        VERBOSE(VB_IMPORTANT, warning);

        return false;
    }

#ifdef _WIN32
    // HOME environment variable might not be defined
    // some libraries will fail without it
    char *home = getenv("HOME");
    if (!home)
    {
        home = getenv("LOCALAPPDATA");      // Vista
        if (!home)
            home = getenv("APPDATA");       // XP
        if (!home)
            home = ".";  // getenv("TEMP")?

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
            MythPopupBox::showOkPopup(d->mainWindow, "HOME error", warning);
        }
        VERBOSE(VB_IMPORTANT, warning);

        return false;
    }

    if (!d->Init(gui, UPnPclient, promptForBackend,
                 disableAutoDiscovery, ignoreDB))
    {
        return false;
    }

    ActivateSettingsCache(true);

    return true;
}

MythContext::~MythContext()
{
    if (QThreadPool::globalInstance()->activeThreadCount())
    {
        VERBOSE(VB_GENERAL,
                "~MythContext waiting for threads to exit.");
    }
    QThreadPool::globalInstance()->waitForDone();
    delete d;
}

// Assumes that either sockLock is held, or the app is still single
// threaded (i.e. during startup).
bool MythContext::ConnectToMasterServer(bool blockingClient)
{
    if (gContext->IsMasterBackend())
    {
        // Should never get here unless there is a bug in the code somewhere.
        // If this happens, it can cause endless event loops.
        VERBOSE(VB_IMPORTANT, "ERROR: Master backend tried to connect back "
                "to itself!");
        return false;
    }

    QString server = GetSetting("MasterServerIP", "localhost");
    int     port   = GetNumSetting("MasterServerPort", 6543);
    bool    proto_mismatch = false;

    if (!d->serverSock)
    {
        QString ann = QString("ANN %1 %2 %3")
            .arg(blockingClient ? "Playback" : "Monitor")
            .arg(d->m_localhostname).arg(false);
        d->serverSock = ConnectCommandSocket(
            server, port, ann, &proto_mismatch);
    }

    if (!d->serverSock)
        return false;

    if (!d->eventSock)
        d->eventSock = ConnectEventSocket(server, port);

    if (!d->eventSock)
    {
        d->serverSock->DownRef();
        d->serverSock = NULL;

        QCoreApplication::postEvent(
            d, new MythEvent("CONNECTION_FAILURE"));

        return false;
    }

    return true;
}

bool do_command_socket_setup(
    MythSocket *serverSock, const QString &announcement, uint timeout_in_ms,
    bool &proto_mismatch, MythContextPrivate *d)
{
    proto_mismatch = false;

#ifndef IGNORE_PROTO_VER_MISMATCH
    if (!MythContext::CheckProtoVersion(serverSock, timeout_in_ms, d))
    {
        proto_mismatch = true;
        return false;
    }
#endif

    QStringList strlist(announcement);

    if (!serverSock->writeStringList(strlist))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Connecting server socket to "
                "master backend, socket write failed");
        return false;
    }

    if (!serverSock->readStringList(strlist, true) || strlist.empty() ||
        (strlist[0] == "ERROR"))
    {
        if (!strlist.empty())
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Problem connecting "
                    "server socket to master backend");
        else
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Timeout connecting "
                    "server socket to master backend");
        return false;
    }

    return true;
}

MythSocket *MythContext::ConnectCommandSocket(
    const QString &hostname, int port, const QString &announce,
    bool *p_proto_mismatch, bool gui, int maxConnTry, int setup_timeout)
{
    MythSocket *serverSock = NULL;

    {
        QMutexLocker locker(&d->WOLInProgressLock);
        d->WaitForWOL();
    }

    QString WOLcmd = GetSetting("WOLbackendCommand", "");

    if (maxConnTry < 1)
        maxConnTry = max(GetNumSetting("BackendConnectRetry", 1), 1);

    int WOLsleepTime = 0, WOLmaxConnTry = 0;
    if (!WOLcmd.isEmpty())
    {
        WOLsleepTime  = GetNumSetting("WOLbackendReconnectWaitTime", 0);
        WOLmaxConnTry = max(GetNumSetting("WOLbackendConnectRetry", 1), 1);
        maxConnTry    = max(maxConnTry, WOLmaxConnTry);
    }

    bool we_attempted_wol = false;

    if (setup_timeout <= 0)
        setup_timeout = MythSocket::kShortTimeout;

    bool proto_mismatch = false;
    for (int cnt = 1; cnt <= maxConnTry; cnt++)
    {
        VERBOSE(VB_GENERAL, LOC +
                QString("Connecting to backend server: %1:%2 (try %3 of %4)")
                .arg(hostname).arg(port).arg(cnt).arg(maxConnTry));

        serverSock = new MythSocket();

        int sleepms = 0;
        if (serverSock->connect(hostname, port))
        {
            if (do_command_socket_setup(
                    serverSock, announce, setup_timeout, proto_mismatch, d))
            {
                break;
            }

            if (proto_mismatch)
            {
                if (p_proto_mismatch)
                    *p_proto_mismatch = true;

                serverSock->DownRef();
                serverSock = NULL;
                break;
            }

            setup_timeout *= 1.5f;
        }
        else if (!WOLcmd.isEmpty() && (cnt < maxConnTry))
        {
            if (!we_attempted_wol)
            {
                QMutexLocker locker(&d->WOLInProgressLock);
                if (d->WOLInProgress)
                {
                    d->WaitForWOL();
                    continue;
                }

                d->WOLInProgress = we_attempted_wol = true;
            }

            myth_system(WOLcmd);
            sleepms = WOLsleepTime * 1000;
        }

        serverSock->DownRef();
        serverSock = NULL;

        if (!serverSock && (cnt == 1))
        {
            QCoreApplication::postEvent(
                d, new MythEvent("CONNECTION_FAILURE"));
        }

        if (sleepms)
            usleep(sleepms * 1000);
    }

    if (we_attempted_wol)
    {
        QMutexLocker locker(&d->WOLInProgressLock);
        d->WOLInProgress = false;
        d->WOLInProgressWaitCondition.wakeAll();
    }

    if (!serverSock && !proto_mismatch)
    {
        VERBOSE(VB_IMPORTANT,
                "Connection to master server timed out.\n\t\t\t"
                "Either the server is down or the master server settings"
                "\n\t\t\t"
                "in mythtv-settings does not contain the proper IP address\n");
    }
    else
    {
        QCoreApplication::postEvent(
            d, new MythEvent("CONNECTION_RESTABLISHED"));
    }

    return serverSock;
}

MythSocket *MythContext::ConnectEventSocket(const QString &hostname, int port)
{
    MythSocket *eventSock = new MythSocket();

    while (eventSock->state() != MythSocket::Idle)
    {
        usleep(5000);
    }

    // Assume that since we _just_ connected the command socket,
    // this one won't need multiple retries to work...
    if (!eventSock->connect(hostname, port))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to connect event "
                "socket to master backend");
        eventSock->DownRef();
        eventSock = NULL;
        return NULL;
    }

    eventSock->Lock();

    QString str = QString("ANN Monitor %1 %2")
        .arg(d->m_localhostname).arg(true);
    QStringList strlist(str);
    eventSock->writeStringList(strlist);
    if (!eventSock->readStringList(strlist) || strlist.empty() ||
        (strlist[0] == "ERROR"))
    {
        if (!strlist.empty())
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Problem connecting "
                    "event socket to master backend");
        else
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Timeout connecting "
                    "event socket to master backend");

        eventSock->DownRef();
        eventSock->Unlock();
        eventSock = NULL;
        return NULL;
    }

    eventSock->Unlock();
    eventSock->setCallbacks(this);

    return eventSock;
}

bool MythContext::IsConnectedToMaster(void)
{
    QMutexLocker locker(&d->sockLock);
    return d->serverSock;
}

void MythContext::BlockShutdown(void)
{
    QStringList strlist;

    QMutexLocker locker(&d->sockLock);
    if (d->serverSock == NULL)
        return;

    strlist << "BLOCK_SHUTDOWN";
    d->serverSock->writeStringList(strlist);
    d->serverSock->readStringList(strlist);

    if (d->eventSock == NULL || d->eventSock->state() != MythSocket::Connected)
        return;

    strlist.clear();
    strlist << "BLOCK_SHUTDOWN";

    d->eventSock->Lock();

    d->eventSock->writeStringList(strlist);
    d->eventSock->readStringList(strlist);

    d->eventSock->Unlock();
}

void MythContext::AllowShutdown(void)
{
    QStringList strlist;

    QMutexLocker locker(&d->sockLock);
    if (d->serverSock == NULL)
        return;

    strlist << "ALLOW_SHUTDOWN";
    d->serverSock->writeStringList(strlist);
    d->serverSock->readStringList(strlist);

    if (d->eventSock == NULL || d->eventSock->state() != MythSocket::Connected)
        return;

    strlist.clear();
    strlist << "ALLOW_SHUTDOWN";

    d->eventSock->Lock();

    d->eventSock->writeStringList(strlist);
    d->eventSock->readStringList(strlist);

    d->eventSock->Unlock();
}

void MythContext::SetBackend(bool backend)
{
    d->m_backend = backend;
}

bool MythContext::IsBackend(void)
{
    return d->m_backend;
}

bool MythContext::IsMasterHost(void)
{
    QString myip = gContext->GetSetting("BackendServerIP");
    QString masterip = gContext->GetSetting("MasterServerIP");

    return (masterip == myip);
}

bool MythContext::IsMasterBackend(void)
{
    return (IsBackend() && IsMasterHost());
}

bool MythContext::BackendIsRunning(void)
{
#if CONFIG_DARWIN || (__FreeBSD__) || defined(__OpenBSD__)
    const char *command = "ps -axc | grep -i mythbackend | grep -v grep > /dev/null";
#elif defined USING_MINGW
    const char *command = "%systemroot%\\system32\\tasklist.exe "
       " | %systemroot%\\system32\\find.exe /i \"mythbackend.exe\" ";
#else
    const char *command = "ps -ae | grep mythbackend > /dev/null";
#endif
    bool res = myth_system(command,
                           MYTH_SYSTEM_DONT_BLOCK_LIRC |
                           MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);
    return !res;
}

bool MythContext::IsFrontendOnly(void)
{
    // find out if a backend runs on this host...
    bool backendOnLocalhost = false;

    QStringList strlist("QUERY_IS_ACTIVE_BACKEND");
    strlist << GetHostName();

    SendReceiveStringList(strlist);

    if (QString(strlist[0]) == "FALSE")
        backendOnLocalhost = false;
    else
        backendOnLocalhost = true;

    return !backendOnLocalhost;
}

QString MythContext::GetMasterHostPrefix(void)
{
    QString ret;

    QMutexLocker locker(&d->sockLock);
    if (!d->serverSock)
    {
        bool blockingClient = gContext->GetNumSetting("idleTimeoutSecs",0) > 0;
        ConnectToMasterServer(blockingClient);
    }

    if (d->serverSock)
        ret = QString("myth://%1:%2/")
                     .arg(d->serverSock->peerAddress().toString())
                     .arg(d->serverSock->peerPort());
    return ret;
}

QString MythContext::GetMasterHostName(void)
{
    QMutexLocker locker(&d->m_hostnamelock);

    if (d->m_masterhostname.isEmpty())
    {
        QStringList strlist("QUERY_HOSTNAME");

        SendReceiveStringList(strlist);

        d->m_masterhostname = strlist[0];
    }

    QString ret = d->m_masterhostname;
    ret.detach();

    return ret;
}

void MythContext::ClearSettingsCache(const QString &myKey)
{
    d->m_database->ClearSettingsCache(myKey);
}

void MythContext::ActivateSettingsCache(bool activate)
{
    d->m_database->ActivateSettingsCache(activate);
}

QString MythContext::GetHostName(void)
{
    QMutexLocker (&d->m_hostnamelock);
    QString tmp = d->m_localhostname;
    tmp.detach();
    return tmp;
}

QString MythContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

void MythContext::RefreshBackendConfig(void)
{
    d->LoadLogSettings();
}

void MythContext::GetResolutionSetting(const QString &type,
                                       int &width, int &height,
                                       double &forced_aspect,
                                       double &refresh_rate,
                                       int index)
{
    d->m_database->GetResolutionSetting(type, width, height, forced_aspect,
                                        refresh_rate, index);
}

void MythContext::GetResolutionSetting(const QString &t, int &w, int &h, int i)
{
    d->m_database->GetResolutionSetting(t, w, h, i);
}

MDBManager *MythContext::GetDBManager(void)
{
    return d->m_database->GetDBManager();
}

/** /brief Returns true if database is being ignored.
 *
 *  This was created for some command line only programs which
 *  still need myth libraries, such as channel scanners, channel
 *  change programs, and the off-line commercial flagger.
 */
bool MythContext::IsDatabaseIgnored(void) const
{
    return d->m_database->IsDatabaseIgnored();
}

void MythContext::SaveSetting(const QString &key, int newValue)
{
    d->m_database->SaveSetting(key, newValue);
}

void MythContext::SaveSetting(const QString &key, const QString &newValue)
{
    d->m_database->SaveSetting(key, newValue);
}

bool MythContext::SaveSettingOnHost(const QString &key,
                                    const QString &newValue,
                                    const QString &host)
{
    return d->m_database->SaveSettingOnHost(key, newValue, host);
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval)
{
    return d->m_database->GetSetting(key, defaultval);
}

int MythContext::GetNumSetting(const QString &key, int defaultval)
{
    return d->m_database->GetNumSetting(key, defaultval);
}

double MythContext::GetFloatSetting(const QString &key, double defaultval)
{
    return d->m_database->GetFloatSetting(key, defaultval);
}

QString MythContext::GetSettingOnHost(const QString &key, const QString &host,
                                      const QString &defaultval)
{
    return d->m_database->GetSettingOnHost(key, host, defaultval);
}

int MythContext::GetNumSettingOnHost(const QString &key, const QString &host,
                                     int defaultval)
{
    return d->m_database->GetNumSettingOnHost(key, host, defaultval);
}

double MythContext::GetFloatSettingOnHost(
    const QString &key, const QString &host, double defaultval)
{
    return d->m_database->GetFloatSettingOnHost(key, host, defaultval);
}

void MythContext::SetSetting(const QString &key, const QString &newValue)
{
    d->m_database->SetSetting(key, newValue);
}

/**
 *  \brief Overrides the given setting for the execution time of the process.
 *
 * This allows defining settings for the session only, without touching the
 * settings in the data base.
 */
void MythContext::OverrideSettingForSession(const QString &key,
                                            const QString &value)
{
    d->m_database->OverrideSettingForSession(key, value);
}

bool MythContext::SendReceiveStringList(QStringList &strlist,
                                        bool quickTimeout, bool block)
{
/*
    if (!IsBackend() && QThread::currentThread() == d->m_UIThread)
    {
        QString msg = "SendReceiveStringList(";
        for (uint i=0; i<(uint)strlist.size() && i<2; i++)
            msg += (i?",":"") + strlist[i];
        msg += (strlist.size() > 2) ? "...)" : ")";
        msg += " called from UI thread";
        VERBOSE(VB_IMPORTANT, msg);
    }
 */

    QString query_type = "UNKNOWN";

    if (!strlist.isEmpty())
        query_type = strlist[0];
    
    QMutexLocker locker(&d->sockLock);

    if (!d->serverSock)
    {
        bool blockingClient = gContext->GetNumSetting("idleTimeoutSecs",0) > 0;
        ConnectToMasterServer(blockingClient);
    }

    bool ok = false;

    if (d->serverSock)
    {
        QStringList sendstrlist = strlist;
        d->serverSock->writeStringList(sendstrlist);
        ok = d->serverSock->readStringList(strlist, quickTimeout);

        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, QString("Connection to backend server lost"));
            d->serverSock->DownRef();
            d->serverSock = NULL;

            bool blockingClient = gContext->GetNumSetting("idleTimeoutSecs",0) > 0;
            ConnectToMasterServer(blockingClient);

            if (d->serverSock)
            {
                d->serverSock->writeStringList(sendstrlist);
                ok = d->serverSock->readStringList(strlist, quickTimeout);
            }
        }

        // this should not happen
        while (ok && strlist[0] == "BACKEND_MESSAGE")
        {
            // oops, not for us
            VERBOSE(VB_IMPORTANT, "SRSL you shouldn't see this!!");
            QString message = strlist[1];
            strlist.pop_front(); strlist.pop_front();

            MythEvent me(message, strlist);
            dispatch(me);

            ok = d->serverSock->readStringList(strlist, quickTimeout);
        }
        // .

        if (!ok)
        {
            if (d->serverSock)
            {
                d->serverSock->DownRef();
                d->serverSock = NULL;
            }

            VERBOSE(VB_IMPORTANT,
                    QString("Reconnection to backend server failed"));

            QCoreApplication::postEvent(
                d, new MythEvent("PERSISTENT_CONNECTION_FAILURE"));
        }
    }

    if (ok)
    {
        if (strlist.isEmpty())
            ok = false;
        else if (strlist[0] == "ERROR")
        {
            if (strlist.size() == 2)
                VERBOSE(VB_GENERAL, QString("Protocol query '%1' reponded "
                                            "with the error '%2'")
                                            .arg(query_type).arg(strlist[1]));
            else
                VERBOSE(VB_GENERAL, QString("Protocol query '%1' reponded "
                                        "with an error, but no error message.")
                                        .arg(query_type));
                
            ok = false;
        }

    }
    
    return ok;
}

void MythContext::readyRead(MythSocket *sock)
{
    while (sock->state() == MythSocket::Connected &&
           sock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!sock->readStringList(strlist))
            continue;

        QString prefix = strlist[0];
        QString message = strlist[1];

        if (prefix == "OK")
        {
        }
        else if (prefix != "BACKEND_MESSAGE")
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Received a: %1 message from the backend"
                    "\n\t\t\tBut I don't know what to do with it.")
                    .arg(prefix));
        }
        else if (message == "CLEAR_SETTINGS_CACHE")
        {
            // No need to dispatch this message to ourself, so handle it
            VERBOSE(VB_GENERAL, "Received a remote 'Clear Cache' request");
            ClearSettingsCache();
        }
        else
        {
            strlist.pop_front();
            strlist.pop_front();
            MythEvent me(message, strlist);
            dispatch(me);
        }
    }
}

bool MythContext::CheckProtoVersion(
    MythSocket *socket, uint timeout_ms, MythContextPrivate *d)
{
    if (!socket)
        return false;

    QStringList strlist(QString("MYTH_PROTO_VERSION %1")
                        .arg(MYTH_PROTO_VERSION));
    socket->writeStringList(strlist);

    if (!socket->readStringList(strlist, timeout_ms) || strlist.empty())
    {
        VERBOSE(VB_IMPORTANT, "Protocol version check failure.\n\t\t\t"
                "The response to MYTH_PROTO_VERSION was empty.\n\t\t\t"
                "This happens when the backend is too busy to respond,\n\t\t\t"
                "or has deadlocked in due to bugs or hardware failure.");

        return false;
    }
    else if (strlist[0] == "REJECT" && strlist.size() >= 2)
    {
        VERBOSE(VB_GENERAL, QString("Protocol version mismatch (frontend=%1,"
                                    "backend=%2)\n")
                                    .arg(MYTH_PROTO_VERSION).arg(strlist[1]));

        QStringList list(strlist[1]);
        QCoreApplication::postEvent(
            d, new MythEvent("VERSION_MISMATCH", list));

        return false;
    }
    else if (strlist[0] == "ACCEPT")
    {
        VERBOSE(VB_IMPORTANT, QString("Using protocol version %1")
                               .arg(MYTH_PROTO_VERSION));
        return true;
    }

    VERBOSE(VB_GENERAL, QString("Unexpected response to MYTH_PROTO_VERSION: %1")
                               .arg(strlist[0]));
    return false;
}

void MythContext::connected(MythSocket *sock)
{
    (void)sock;
}

void MythContext::connectionClosed(MythSocket *sock)
{
    (void)sock;

    VERBOSE(VB_IMPORTANT, QString("Event socket closed. "
            "No connection to the backend."));

    QMutexLocker locker(&d->sockLock);
    if (d->serverSock)
    {
        d->serverSock->DownRef();
        d->serverSock = NULL;
    }

    if (d->eventSock)
    {
        d->eventSock->DownRef();
        d->eventSock = NULL;
    }

    dispatch(MythEvent(QString("BACKEND_SOCKETS_CLOSED")));
}

void MythContext::SetMainWindow(MythMainWindow *mainwin)
{
    d->mainWindow = mainwin;
}

MythMainWindow *MythContext::GetMainWindow(void)
{
    return d->mainWindow;
}

/*
 * Convenience method, so that we don't have to do:
 *   if (gContext->GetMainWindow()->TranslateKeyPress(...))
 */
bool MythContext::TranslateKeyPress(const QString &context,
                                    QKeyEvent     *e,
                                    QStringList   &actions, bool allowJumps)
{
    if (!d->mainWindow)
    {
        VERBOSE(VB_IMPORTANT, "MC::TranslateKeyPress() called, but no window");
        return false;
    }

    return d->mainWindow->TranslateKeyPress(context, e, actions, allowJumps);
}

bool MythContext::TestPopupVersion(const QString &name,
                                   const QString &libversion,
                                   const QString &pluginversion)
{
    if (libversion == pluginversion)
        return true;

    QString err = QObject::tr(
        "Plugin %1 is not compatible with the installed MythTV "
        "libraries. Please recompile the plugin after a make "
        "distclean");

    VERBOSE(VB_GENERAL, QString("Plugin %1 (%2) binary version does not "
                                "match libraries (%3)")
                                .arg(name).arg(pluginversion).arg(libversion));

    if (GetMainWindow() && !d->disablelibrarypopup)
        ShowOkPopup(err.arg(name));

    return false;
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

void MythContext::LogEntry(const QString &module, int priority,
                           const QString &message, const QString &details)
{
    unsigned int logid;
    int howmany;

    if (d->m_database->IsDatabaseIgnored())
        return;

    if (d->m_logenable == -1) // Haven't grabbed the settings yet
        d->LoadLogSettings();
    if (d->m_logenable == 1)
    {
        QString fullMsg = message;
        if (!details.isEmpty())
            fullMsg += ": " + details;

        if (message.left(21) != "Last message repeated")
        {
            if (fullMsg == d->lastLogStrings[module])
            {
                d->lastLogCounts[module] += 1;
                return;
            }
            else
            {
                if (0 < d->lastLogCounts[module])
                {
                    LogEntry(module, priority,
                             QString("Last message repeated %1 times")
                                    .arg(d->lastLogCounts[module]),
                             d->lastLogStrings[module]);
                }

                d->lastLogCounts[module] = 0;
                d->lastLogStrings[module] = fullMsg;
            }
        }


        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO mythlog (module, priority, "
                      "logdate, host, message, details) "
                      "values (:MODULE, :PRIORITY, now(), :HOSTNAME, "
                      ":MESSAGE, :DETAILS );");

        query.bindValue(":MODULE", module);
        query.bindValue(":PRIORITY", priority);
        query.bindValue(":HOSTNAME", d->m_localhostname);
        query.bindValue(":MESSAGE", message);
        query.bindValue(":DETAILS", details);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("LogEntry", query);

        if (d->m_logmaxcount > 0)
        {
            query.prepare("SELECT logid FROM mythlog WHERE "
                          "module= :MODULE ORDER BY logdate ASC ;");
            query.bindValue(":MODULE", module);
            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("DelLogEntry#1", query);
            }
            else
            {
                howmany = query.size();
                if (howmany > d->m_logmaxcount)
                {
                    MSqlQuery delquery(MSqlQuery::InitCon());
                    while (howmany > d->m_logmaxcount)
                    {
                        query.next();
                        logid = query.value(0).toUInt();
                        delquery.prepare("DELETE FROM mythlog WHERE "
                                         "logid= :LOGID ;");
                        delquery.bindValue(":LOGID", logid);

                        if (!delquery.exec() || !delquery.isActive())
                        {
                            MythDB::DBError("DelLogEntry#2", delquery);
                        }
                        howmany--;
                    }
                }
            }
        }

        if (priority <= d->m_logprintlevel)
        {
            QByteArray tmp =
                QString("%1: %2").arg(module).arg(fullMsg).toUtf8();
            VERBOSE(VB_IMPORTANT, tmp.constData());
        }
    }
}

void MythContext::addPrivRequest(MythPrivRequest::Type t, void *data)
{
    QMutexLocker lockit(&d->m_priv_mutex);
    d->m_priv_requests.push(MythPrivRequest(t, data));
    d->m_priv_queued.wakeAll();
}

void MythContext::waitPrivRequest() const
{
    d->m_priv_mutex.lock();
    while (d->m_priv_requests.empty())
        d->m_priv_queued.wait(&d->m_priv_mutex);
    d->m_priv_mutex.unlock();
}

MythPrivRequest MythContext::popPrivRequest()
{
    QMutexLocker lockit(&d->m_priv_mutex);
    MythPrivRequest ret_val(MythPrivRequest::PrivEnd, NULL);
    if (!d->m_priv_requests.empty()) {
        ret_val = d->m_priv_requests.front();
        d->m_priv_requests.pop();
    }
    return ret_val;
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
        ret = d->WriteSettingsFile(params, true);
        if (ret)
        {
            // Save the new settings:
            d->m_DBparams = params;
            d->m_database->SetDatabaseParams(d->m_DBparams);

            // If database has changed, force its use:
            d->ResetDatabase();
        }
    }
    return ret;
}

void MythContext::dispatch(const MythEvent &event)
{
    VERBOSE(VB_NETWORK, QString("MythEvent: %1").arg(event.Message()));

    MythObservable::dispatch(event);
}

void MythContext::dispatchNow(const MythEvent &event)
{
    VERBOSE(VB_NETWORK, QString("MythEvent: %1").arg(event.Message()));

    MythObservable::dispatchNow(event);
}

void MythContext::sendPlaybackStart(void)
{
    MythEvent me(QString("PLAYBACK_START %1").arg(GetHostName()));
    dispatchNow(me);
}

void MythContext::sendPlaybackEnd(void)
{
    MythEvent me(QString("PLAYBACK_END %1").arg(GetHostName()));
    dispatchNow(me);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
