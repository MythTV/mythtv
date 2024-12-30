#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <queue>
#include <thread>
#include <unistd.h> // for usleep(), gethostname
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QHostInfo>
#include <QMutex>
#include <QTcpSocket>
#ifdef Q_OS_ANDROID
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#else
#include <QJniEnvironment>
#include <QJniObject>
#define QAndroidJniEnvironment QJniEnvironment
#define QAndroidJniObject QJniObject
#endif
#endif

#ifdef _WIN32
#include "libmythbase/compat.h"
#endif
#include "libmythbase/configuration.h"
#include "libmythbase/dbutil.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythplugin.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/portchecker.h"
#include "libmythbase/remotefile.h"
#include "libmythui/guistartup.h"
#include "libmythui/langsettings.h"
#include "libmythui/mediamonitor.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythimage.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"
#include "libmythupnp/mythxmlclient.h"
#include "libmythupnp/upnp.h"

#include "backendselect.h"
#include "dbsettings.h"
#include "mythcontext.h"

#define LOC      QString("MythContext: ")

MythContext *gContext = nullptr;

static const QString sLocation = "MythContext";

namespace
{
class GUISettingsCache
{
  public:
    GUISettingsCache() = default;
    GUISettingsCache(const QString& cache_filename, QString cache_path)
        : m_cachePath(std::move(cache_path))
    {
        m_cacheFilename = m_cachePath + '/' + cache_filename;
        if (m_cachePath.isEmpty() || cache_filename.isEmpty())
        {
            m_cacheFilename = m_cachePath = QString();
        }
    }

    bool save();
    void loadOverrides() const;
    static void clearOverrides();

  private:
    QString m_cacheFilename {"cache/contextcache.xml"};
    QString m_cachePath     {"cache"};

    static const std::array<QString, 13> kSettings;
};

} // anonymous namespace

class MythContextPrivate : public QObject
{
    friend class MythContextSlotHandler;

  public:
    explicit MythContextPrivate(MythContext *lparent);
   ~MythContextPrivate() override;

    bool Init        (bool gui,
                      bool promptForBackend,
                      bool disableAutoDiscovery,
                      bool ignoreDB);
    bool FindDatabase(bool prompt, bool noAutodetect);

    void TempMainWindow(bool languagePrompt = true);
    void EndTempWindow(void);

    bool LoadDatabaseSettings(void);
    bool SaveDatabaseParams(const DatabaseParams &params, bool force);

    bool    PromptForDatabaseParams(const QString &error);
    QString TestDBconnection(bool prompt=true);
    void    SilenceDBerrors(void);
    void    EnableDBerrors(void);
    void    ResetDatabase(void) const;

    BackendSelection::Decision
            ChooseBackend(const QString &error);
    int     UPnPautoconf(std::chrono::milliseconds milliSeconds = 2s);
    bool    DefaultUPnP(QString& Error);
    bool    UPnPconnect(const DeviceLocation *backend, const QString &PIN);
    void    ShowGuiStartup(void);
    bool    checkPort(QString &host, int port, std::chrono::seconds timeLimit) const;
    static void processEvents(void);

  protected:
    bool event(QEvent* /*e*/) override; // QObject

    void ShowConnectionFailurePopup(bool persistent);
    void HideConnectionFailurePopup(void);

    void ShowVersionMismatchPopup(uint remote_version);

  public slots:
    void OnCloseDialog();

  public:
    MythContext            *m_parent             {nullptr};

                            /// Should this context use GUI elements?
    bool                    m_gui                {false};

    QString                 m_masterhostname;  ///< master backend hostname

    DatabaseParams          m_dbParams;  ///< Current database host & WOL details
    QString                 m_dbHostCp;  ///< dbHostName backup

    bool                   m_disableeventpopup   {false};

    MythUIHelper           *m_ui                 {nullptr};
    MythContextSlotHandler *m_sh                 {nullptr};
    GUIStartup             *m_guiStartup         {nullptr};
    QEventLoop             *m_loop               {nullptr};
    bool                    m_needsBackend       {false};

    GUISettingsCache        m_GUISettingsCache;

  private:
    MythConfirmationDialog *m_mbeVersionPopup    {nullptr};
    int                     m_registration       {-1};
    QDateTime               m_lastCheck;
    QTcpSocket             *m_socket             {nullptr};
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

    bool cardidok = false;
    int wantcardid = tokens[0].toInt(&cardidok, 10);

    if (cardidok && wantcardid > 0)
    {
        strlist << QString("LOCK_TUNER %1").arg(wantcardid);
        s = s.replace(0, tokens[0].length() + 1, "");
    }
    else
    {
        strlist << "LOCK_TUNER";
    }

    gCoreContext->SendReceiveStringList(strlist);
    int cardid = strlist[0].toInt();

    if (cardid >= 0)
    {
        s = s.arg(qPrintable(strlist[1]),
                  qPrintable(strlist[2]),
                  qPrintable(strlist[3]));

        myth_system(s);

        strlist = QStringList(QString("FREE_TUNER %1").arg(cardid));
        gCoreContext->SendReceiveStringList(strlist);
    }
    else
    {
        QString label;

        if (cardidok)
        {
            if (cardid == -1)
            {
                label = QObject::tr("Could not find specified tuner (%1).")
                                    .arg(wantcardid);
            }
            else
            {
                label = QObject::tr("Specified tuner (%1) is already in use.")
                                    .arg(wantcardid);
            }
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
    if (!pmanager)
        return;

    if (GetNotificationCenter() && pmanager->config_plugin(cmd.trimmed()))
    {
        ShowNotificationError(cmd, sLocation,
                              QObject::tr("Failed to configure plugin"));
    }
}

static void plugin_cb(const QString &cmd)
{
    MythPluginManager *pmanager = gCoreContext->GetPluginManager();
    if (!pmanager)
        return;

    if (GetNotificationCenter() && pmanager->run_plugin(cmd.trimmed()))
    {
        ShowNotificationError(QObject::tr("Plugin failure"),
                              sLocation,
                              QObject::tr("%1 failed to run for some reason").arg(cmd));
    }
}

static void eject_cb(void)
{
    MediaMonitor::ejectOpticalDisc();
}

MythContextPrivate::MythContextPrivate(MythContext *lparent)
    : m_parent(lparent),
      m_sh(new MythContextSlotHandler(this)),
      m_loop(new QEventLoop(this))
{
    InitializeMythDirs();
}

MythContextPrivate::~MythContextPrivate()
{
    if (GetNotificationCenter() && m_registration > 0)
    {
        GetNotificationCenter()->UnRegister(this, m_registration, true);
    }

    delete m_loop;

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

#ifdef Q_OS_DARWIN
    // Qt 4.4 has window-focus problems
    gCoreContext->OverrideSettingForSession("RunFrontendInWindow", "1");
#endif
    GetMythUI()->Init();
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
    if (HasMythMainWindow())
    {
        if (m_guiStartup && !m_guiStartup->m_Exit)
        {
            MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
            if (mainStack)
            {
                mainStack->PopScreen(m_guiStartup, false);
                m_guiStartup = nullptr;
            }
        }
    }
    EnableDBerrors();
}

/**
 * Check if a port is open and sort out the link-local scope.
 *
 * \param host      Host or IP address. Will be updated with link-local
 *                  scope if needed.
 * \param port      Port number to check.
 * \param timeLimit Limit in seconds for testing.
 */

bool MythContextPrivate::checkPort(QString &host, int port, std::chrono::seconds timeLimit) const
{
    PortChecker checker;
    if (m_guiStartup)
        QObject::connect(m_guiStartup, &GUIStartup::cancelPortCheck, &checker, &PortChecker::cancelPortCheck);
    return checker.checkPort(host, port, timeLimit);
}


bool MythContextPrivate::Init(const bool gui,
                              const bool promptForBackend,
                              const bool disableAutoDiscovery,
                              const bool ignoreDB)
{
    gCoreContext->GetDB()->IgnoreDatabase(ignoreDB);
    m_gui = gui;
    if (gui)
    {
        m_GUISettingsCache.loadOverrides();
    }

    if (gCoreContext->IsFrontend())
        m_needsBackend = true;

    // Creates screen saver control if we will have a GUI
    if (gui)
        m_ui = GetMythUI();

    // ---- database connection stuff ----

    if (!ignoreDB && !FindDatabase(promptForBackend, disableAutoDiscovery))
    {
        EndTempWindow();
        return false;
    }

    // ---- keep all DB-using stuff below this line ----

    // Prompt for language if this is a first time install and
    // we didn't already do so.
    if (m_gui && !gCoreContext->GetDB()->HaveSchema())
    {
        TempMainWindow(false);
        LanguageSelection::prompt();
        MythTranslation::load("mythfrontend");
    }
    gCoreContext->InitLocale();
    gCoreContext->SaveLocaleDefaults();

    // Close GUI Startup Window.
    if (m_guiStartup)
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        if (mainStack)
            mainStack->PopScreen(m_guiStartup, false);
        m_guiStartup=nullptr;
    }
    EndTempWindow();

    if (gui)
    {
        MythUIMenuCallbacks cbs {};
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

    // 1. Either load XmlConfiguration::k_default_filename or use sensible "localhost" defaults:
    bool loaded = LoadDatabaseSettings();
    DatabaseParams dbParamsFromFile = m_dbParams;

    // In addition to the UI chooser, we can also try to autoSelect later,
    // but only if we're not doing manualSelect and there was no
    // valid XmlConfiguration::k_default_filename
    bool autoSelect = !manualSelect && !loaded && !noAutodetect;

    // 2. If the user isn't forcing up the chooser UI, look for a default
    //    backend in XmlConfiguration::k_default_filename, then test DB settings we've got so far:
    if (!manualSelect)
    {
        // XmlConfiguration::k_default_filename may contain a backend host UUID and PIN.
        // If so, try to AutoDiscover UPnP server, and use its DB settings:

        if (DefaultUPnP(failure))                // Probably a valid backend,
            autoSelect = manualSelect = false;   // so disable any further UPnP
        else
            if (!failure.isEmpty())
                LOG(VB_GENERAL, LOG_ALERT, failure);

        failure = TestDBconnection(loaded);
        if (failure.isEmpty())
            goto DBfound;
        if (m_guiStartup && m_guiStartup->m_Exit)
            return false;
        if (m_guiStartup && m_guiStartup->m_Search)
            autoSelect=true;
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
            if (m_guiStartup && m_guiStartup->m_Exit)
                return false;
        }

        // Multiple BEs, or needs PIN.
        manualSelect |= (count > 1 || count == -1);
        // Search requested
        if (m_guiStartup && m_guiStartup->m_Search)
            manualSelect=true;
    }

    manualSelect &= m_gui;  // no interactive command-line chooser yet

    // Queries the user for the DB info
    do
    {
        if (manualSelect)
        {
            // Get the user to select a backend from a possible list:
            switch (ChooseBackend(failure))
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
            // If this is a backend, No longer prompt for database.
            // Instaed allow the web server to start so that the
            // database can be set up there
            if (gCoreContext->IsBackend()
                || !PromptForDatabaseParams(failure))
                goto NoDBfound;
        }
        failure = TestDBconnection();
        if (!failure.isEmpty())
            LOG(VB_GENERAL, LOG_ALERT, failure);
        if (m_guiStartup && m_guiStartup->m_Exit)
            return false;
        if (m_guiStartup && m_guiStartup->m_Search)
            manualSelect=true;
        if (m_guiStartup && m_guiStartup->m_Setup)
            manualSelect=false;
    }
    while (!failure.isEmpty());

DBfound:
    LOG(VB_GENERAL, LOG_DEBUG, "FindDatabase() - Success!");
    // If we got the database from UPNP then the wakeup settings are lost.
    // Restore them.
    m_dbParams.m_wolEnabled = dbParamsFromFile.m_wolEnabled;
    m_dbParams.m_wolReconnect = dbParamsFromFile.m_wolReconnect;
    m_dbParams.m_wolRetry = dbParamsFromFile.m_wolRetry;
    m_dbParams.m_wolCommand = dbParamsFromFile.m_wolCommand;

    SaveDatabaseParams(m_dbParams,
                       !loaded || m_dbParams.m_forceSave ||
                       dbParamsFromFile != m_dbParams);
    EnableDBerrors();
    ResetDatabase();
    return true;

NoDBfound:
    LOG(VB_GENERAL, LOG_DEBUG, "FindDatabase() - failed");
    return false;
}

/** Load database and host settings from XmlConfiguration::k_default_filename, or set some defaults.
 *  \return true if XmlConfiguration::k_default_filename was parsed
 */
bool MythContextPrivate::LoadDatabaseSettings(void)
{
    auto config = XmlConfiguration(); // read-only

    m_dbParams.LoadDefaults();

    m_dbParams.m_localHostName  = config.GetValue("LocalHostName", "");
    m_dbParams.m_dbHostPing     = config.GetValue(kDefaultDB + "PingHost", true);
    m_dbParams.m_dbHostName     = config.GetValue(kDefaultDB + "Host", "");
    m_dbParams.m_dbUserName     = config.GetValue(kDefaultDB + "UserName", "");
    m_dbParams.m_dbPassword     = config.GetValue(kDefaultDB + "Password", "");
    m_dbParams.m_dbName         = config.GetValue(kDefaultDB + "DatabaseName", "");
    m_dbParams.m_dbPort         = config.GetValue(kDefaultDB + "Port", 0);

    m_dbParams.m_wolEnabled     = config.GetValue(kDefaultWOL + "Enabled", false);
    m_dbParams.m_wolReconnect   =
        config.GetDuration<std::chrono::seconds>(kDefaultWOL + "SQLReconnectWaitTime", 0s);
    m_dbParams.m_wolRetry       = config.GetValue(kDefaultWOL + "SQLConnectRetry", 5);
    m_dbParams.m_wolCommand     = config.GetValue(kDefaultWOL + "Command", "");

    bool ok = m_dbParams.IsValid(XmlConfiguration::kDefaultFilename);

    if (!ok)
        m_dbParams.LoadDefaults();

    gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);

    QString hostname = m_dbParams.m_localHostName;
    if (hostname.isEmpty() ||
        hostname == "my-unique-identifier-goes-here")
    {
        LOG(VB_GENERAL, LOG_INFO, "Empty LocalHostName. This is typical.");
        hostname = QHostInfo::localHostName();

#ifndef Q_OS_ANDROID
        if (hostname.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ALERT,
                    "MCP: Error, could not determine host name." + ENO);
        }
#else //elif defined Q_OS_ANDROID
#define ANDROID_EXCEPTION_CHECK \
  if (env->ExceptionCheck()) \
  { \
    env->ExceptionClear(); \
    exception=true; \
  }

        if ((hostname == "localhost") || hostname.isEmpty())
        {
            hostname = "android";
            bool exception=false;
            QAndroidJniEnvironment env;
            QAndroidJniObject myID = QAndroidJniObject::fromString("android_id");
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            QAndroidJniObject activity = QtAndroid::androidActivity();
#else
            QJniObject activity = QNativeInterface::QAndroidApplication::context();
#endif
            ANDROID_EXCEPTION_CHECK;
            QAndroidJniObject appctx = activity.callObjectMethod
                ("getApplicationContext", "()Landroid/content/Context;");
            ANDROID_EXCEPTION_CHECK;
            QAndroidJniObject contentR = appctx.callObjectMethod
                ("getContentResolver", "()Landroid/content/ContentResolver;");
            ANDROID_EXCEPTION_CHECK;
            QAndroidJniObject androidId = QAndroidJniObject::callStaticObjectMethod
                ("android/provider/Settings$Secure", "getString",
                 "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
                 contentR.object<jobject>(),
                 myID.object<jstring>());
            ANDROID_EXCEPTION_CHECK;
            if (exception)
                LOG(VB_GENERAL, LOG_ALERT,
                    "Java exception looking for android id");
            else
                hostname = QString("android-%1").arg(androidId.toString());
        }
#endif

    }
    else
    {
        m_dbParams.m_localEnabled = true;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Using a profile name of: '%1' (Usually the "
                                      "same as this host's name.)")
                                      .arg(hostname));
    gCoreContext->SetLocalHostname(hostname);

    return ok;
}

bool MythContextPrivate::SaveDatabaseParams(
    const DatabaseParams &params, bool force)
{
    bool success = true;

    // only rewrite file if it has changed
    if (force || (params != m_dbParams))
    {
        /* Read in the current file on the filesystem, only setting/clearing as
        necessary.  This prevents losing changes to the file from between when it
        was read at startup of MythTV and when this function is called.
        */
        auto config = XmlConfiguration();

        config.SetValue("LocalHostName", params.m_localHostName);

        config.SetValue(kDefaultDB + "PingHost", params.m_dbHostPing);

        // If dbHostName is an IPV6 address with scope,
        // remove the scope. Unescaped % signs are an
        // xml violation
        QString dbHostName(params.m_dbHostName);
        QHostAddress addr;
        if (addr.setAddress(dbHostName))
        {
            addr.setScopeId(QString());
            dbHostName = addr.toString();
        }
        config.SetValue(kDefaultDB + "Host",     dbHostName);
        config.SetValue(kDefaultDB + "UserName", params.m_dbUserName);
        config.SetValue(kDefaultDB + "Password", params.m_dbPassword);
        config.SetValue(kDefaultDB + "DatabaseName", params.m_dbName);
        config.SetValue(kDefaultDB + "Port",     params.m_dbPort);

        config.SetValue(kDefaultWOL + "Enabled", params.m_wolEnabled);
        config.SetDuration(
            kDefaultWOL + "SQLReconnectWaitTime", params.m_wolReconnect);
        config.SetValue(kDefaultWOL + "SQLConnectRetry", params.m_wolRetry);
        config.SetValue(kDefaultWOL + "Command", params.m_wolCommand);

        // actually save the file
        success = config.Save();

        // Use the new settings:
        m_dbParams = params;
        gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);

        // If database has changed, force its use:
        ResetDatabase();
    }
    return success;
}

void MythContextSlotHandler::OnCloseDialog(void)
{
    if (d && d->m_loop
      && d->m_loop->isRunning())
        d->m_loop->exit();
}


// No longer prompt for database, instaed allow the
// web server to start so that the datbase can be
// set up there

bool MythContextPrivate::PromptForDatabaseParams(const QString &error)
{
    bool accepted = false;
    if (m_gui)
    {
        TempMainWindow();

        // Tell the user what went wrong:
        if (!error.isEmpty())
            ShowOkPopup(error);

        // ask user for database parameters

        EnableDBerrors();
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *dbsetting = new DatabaseSettings();
        auto *ssd = new StandardSettingDialog(mainStack, "databasesettings",
                                              dbsetting);
        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
            connect(dbsetting, &DatabaseSettings::isClosing,
                m_sh, &MythContextSlotHandler::OnCloseDialog);
            if (!m_loop->isRunning())
                m_loop->exec();
        }
        else
        {
            delete ssd;
        }
        SilenceDBerrors();
        EndTempWindow();
        accepted = true;
    }
    else
    {
        DatabaseParams params = m_dbParams;
        QString        response;
        std::this_thread::sleep_for(1s);
        // give user chance to skip config
        std::cout << std::endl << error.toLocal8Bit().constData() << std::endl << std::endl;
        response = getResponse("Would you like to configure the database "
                               "connection now?",
                               "no");
        if (!response.startsWith('y', Qt::CaseInsensitive))
            return false;

        params.m_dbHostName = getResponse("Database host name:",
                                          params.m_dbHostName);
        response = getResponse("Should I test connectivity to this host "
                               "using the ping command?", "yes");
        params.m_dbHostPing = response.startsWith('y', Qt::CaseInsensitive);

        params.m_dbPort     = intResponse("Database non-default port:",
                                          params.m_dbPort);
        params.m_dbName     = getResponse("Database name:",
                                          params.m_dbName);
        params.m_dbUserName = getResponse("Database user name:",
                                          params.m_dbUserName);
        params.m_dbPassword = getResponse("Database password:",
                                          params.m_dbPassword);

        params.m_localHostName = getResponse("Unique identifier for this machine "
                                             "(if empty, the local host name "
                                             "will be used):",
                                             params.m_localHostName);
        params.m_localEnabled = !params.m_localHostName.isEmpty();

        response = getResponse("Would you like to use Wake-On-LAN to retry "
                               "database connections?",
                               (params.m_wolEnabled ? "yes" : "no"));
        params.m_wolEnabled = response.startsWith('y', Qt::CaseInsensitive);

        if (params.m_wolEnabled)
        {
            params.m_wolReconnect =
                std::chrono::seconds(intResponse("Seconds to wait for "
                                                 "reconnection:",
                                                 params.m_wolReconnect.count()));
            params.m_wolRetry     = intResponse("Number of times to retry:",
                                                params.m_wolRetry);
            params.m_wolCommand   = getResponse("Command to use to wake server or server MAC address:",
                                                params.m_wolCommand);
        }

        accepted = m_parent->SaveDatabaseParams(params);
    }
    return accepted;
}

/**
 * Some quick sanity checks before opening a database connection
 *
 * \todo  Rationalise the WOL stuff. We should have one method to wake BEs
 */
QString MythContextPrivate::TestDBconnection(bool prompt)
{
    QString err;
    QString host;

    // Jan 20, 2017
    // Changed to use port check instead of ping

    int port = 0;

    // 1 = db awake, 2 = db listening, 3 = db connects,
    // 4 = backend awake, 5 = backend listening
    // 9 = all ok, 10 = quit

    enum  startupStates : std::uint8_t {
        st_start = 0,
        st_dbAwake = 1,
        st_dbStarted = 2,
        st_dbConnects = 3,
        st_beWOL = 4,
        st_beAwake = 5,
        st_success = 6
    } startupState = st_start;

    static const std::array<const QString, 7> kGuiStatuses
        {"start", "dbAwake", "dbStarted", "dbConnects", "beWOL", "beAwake", "success"};

    auto secondsStartupScreenDelay = gCoreContext->GetDurSetting<std::chrono::seconds>("StartupScreenDelay", 2s);
    auto msStartupScreenDelay = std::chrono::duration_cast<std::chrono::milliseconds>(secondsStartupScreenDelay);
    do
    {
        QElapsedTimer timer;
        timer.start();
        if (m_dbParams.m_dbHostName.isNull() && !m_dbHostCp.isEmpty())
            host = m_dbHostCp;
        else
            host = m_dbParams.m_dbHostName;
        port = m_dbParams.m_dbPort;
        if (port == 0)
            port = 3306;
        std::chrono::seconds wakeupTime = 3s;
        int attempts = 11;
        if (m_dbParams.m_wolEnabled)
        {
            wakeupTime = m_dbParams.m_wolReconnect;
            attempts = m_dbParams.m_wolRetry + 1;
            startupState = st_start;
        }
        else
        {
            startupState = st_dbAwake;
        }
        attempts = std::max(attempts, 6);
        if (!prompt)
            attempts=1;
        if (wakeupTime < 5s)
            wakeupTime = 5s;

        std::chrono::seconds progressTotal = wakeupTime * attempts;

        if (m_guiStartup && !m_guiStartup->m_Exit)
            m_guiStartup->setTotal(progressTotal);

        QString beWOLCmd = QString();
        QString backendIP = QString();
        int backendPort = 0;
        QString masterserver;

        for (int attempt = 0;
            attempt < attempts && startupState != st_success;
            ++attempt)
        {
            // The first time do everything with minimum timeout and
            // no GUI, for the normal case where all is OK
            // After that show the GUI (if this is a GUI program)

            LOG(VB_GENERAL, LOG_INFO,
                 QString("Start up testing connections. DB %1, BE %2, attempt %3, status %4, Delay: %5")
                      .arg(host, backendIP, QString::number(attempt),
                           kGuiStatuses[startupState],
                           QString::number(msStartupScreenDelay.count())) );

            std::chrono::seconds useTimeout = wakeupTime;
            if (attempt == 0)
                useTimeout=1s;

            if (m_gui && !m_guiStartup)
            {
                if (msStartupScreenDelay==0ms || timer.hasExpired(msStartupScreenDelay.count()))
                {
                    ShowGuiStartup();
                    if (m_guiStartup)
                        m_guiStartup->setTotal(progressTotal);
                }
            }
            if (m_guiStartup && !m_guiStartup->m_Exit)
            {
                if (attempt > 0)
                    m_guiStartup->setStatusState(kGuiStatuses[startupState]);
                m_guiStartup->setMessageState("empty");
                processEvents();
            }
            switch (startupState)
            {
            case st_start:
                if (m_dbParams.m_wolEnabled)
                {
                    if (attempt > 0)
                        MythWakeup(m_dbParams.m_wolCommand);
                    if (!checkPort(host, port, useTimeout))
                        break;
                }
                startupState = st_dbAwake;
                [[fallthrough]];
            case st_dbAwake:
                if (!checkPort(host, port, useTimeout))
                    break;
                startupState = st_dbStarted;
                [[fallthrough]];
            case st_dbStarted:
                // If the database is connecting with link-local
                // address, it may have changed
                if (m_dbParams.m_dbHostName != host)
                {
                    m_dbParams.m_dbHostName = host;
                    gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);
                }
                EnableDBerrors();
                ResetDatabase();
                if (!MSqlQuery::testDBConnection())
                {
                    for (std::chrono::seconds temp = 0s; temp < useTimeout * 2 ; temp++)
                    {
                        processEvents();
                        std::this_thread::sleep_for(500ms);
                    }
                    break;
                }
                startupState = st_dbConnects;
                [[fallthrough]];
            case st_dbConnects:
                if (m_needsBackend)
                {
                    beWOLCmd = gCoreContext->GetSetting("WOLbackendCommand", "");
                    if (!beWOLCmd.isEmpty())
                    {
                        wakeupTime += gCoreContext->GetDurSetting<std::chrono::seconds>
                            ("WOLbackendReconnectWaitTime", 0s);
                        attempts += gCoreContext->GetNumSetting
                            ("WOLbackendConnectRetry", 0);
                        useTimeout = wakeupTime;
                        if (m_gui && !m_guiStartup && attempt == 0)
                            useTimeout=1s;
                        progressTotal = wakeupTime * attempts;
                        if (m_guiStartup && !m_guiStartup->m_Exit)
                            m_guiStartup->setTotal(progressTotal);
                        startupState = st_beWOL;
                    }
                }
                else
                {
                    startupState = st_success;
                    break;
                }
                masterserver = gCoreContext->GetSetting
                    ("MasterServerName");
                backendIP = gCoreContext->GetSettingOnHost
                    ("BackendServerAddr", masterserver);
                backendPort = MythCoreContext::GetMasterServerPort();
                [[fallthrough]];
            case st_beWOL:
                if (!beWOLCmd.isEmpty())
                {
                    if (attempt > 0)
                        MythWakeup(beWOLCmd);
                    if (!checkPort(backendIP, backendPort, useTimeout))
                        break;
                }
                startupState = st_beAwake;
                [[fallthrough]];
            case st_beAwake:
                if (!checkPort(backendIP, backendPort, useTimeout))
                    break;
                startupState = st_success;
                [[fallthrough]];
            case st_success:
                // Quiet compiler warning.
                break;
            }
            if (m_guiStartup)
            {
                if (m_guiStartup->m_Exit
                  || m_guiStartup->m_Setup
                  || m_guiStartup->m_Search
                  || m_guiStartup->m_Retry)
                    break;
            }
            processEvents();
        }
        if (startupState == st_success)
            break;

        QString stateMsg = kGuiStatuses[startupState];
        stateMsg.append("Fail");
        LOG(VB_GENERAL, LOG_INFO,
             QString("Start up failure. host %1, status %2")
                  .arg(host, stateMsg));

        if (m_gui && !m_guiStartup)
        {
            ShowGuiStartup();
            if (m_guiStartup)
                m_guiStartup->setTotal(progressTotal);
        }

        if (m_guiStartup
          && !m_guiStartup->m_Exit
          && !m_guiStartup->m_Setup
          && !m_guiStartup->m_Search
          && !m_guiStartup->m_Retry)
        {
            m_guiStartup->updateProgress(true);
            m_guiStartup->setStatusState(stateMsg);
            m_guiStartup->setMessageState("makeselection");
            m_loop->exec();
        }
    }
    while (m_guiStartup && m_guiStartup->m_Retry);

    if (startupState < st_dbAwake)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Pinging to %1 failed, database will be unavailable").arg(host));
        SilenceDBerrors();
        err = QObject::tr(
            "Cannot find (ping) database host %1 on the network",
            "Backend Setup");
        return err.arg(host);
    }

    if (startupState < st_dbConnects)
    {
        SilenceDBerrors();
        return QObject::tr("Cannot login to database", "Backend Setup");
    }

    if (startupState < st_success)
    {
        return QObject::tr("Cannot connect to backend", "Backend Setup");
    }

    // Current DB connection may have been silenced (invalid):
    EnableDBerrors();
    ResetDatabase();

    return {};
}

// Show the Gui Startup window.
// This is called if there is a delay in startup for any reason
// such as the database being unavailable
void MythContextPrivate::ShowGuiStartup(void)
{
    if (!m_gui)
        return;
    TempMainWindow(false);
    MythMainWindow *mainWindow = GetMythMainWindow();
    MythScreenStack *mainStack = mainWindow->GetMainStack();
    if (mainStack)
    {
        if (!m_guiStartup)
        {
            m_guiStartup = new GUIStartup(mainStack, m_loop);
            if (!m_guiStartup->Create())
            {
                delete m_guiStartup;
                m_guiStartup = nullptr;
            }
            if (m_guiStartup)
            {
                mainStack->AddScreen(m_guiStartup, false);
                processEvents();
            }
        }
    }
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
    if (!m_dbParams.m_dbHostName.isEmpty())
        m_dbHostCp = m_dbParams.m_dbHostName;

    m_dbParams.m_dbHostName.clear();
    gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);
}

void MythContextPrivate::EnableDBerrors(void)
{
    // Restore (possibly) blanked hostname
    if (m_dbParams.m_dbHostName.isNull() && !m_dbHostCp.isEmpty())
    {
        m_dbParams.m_dbHostName = m_dbHostCp;
        gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);
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
void MythContextPrivate::ResetDatabase(void) const
{
    gCoreContext->GetDBManager()->CloseDatabases();
    gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);
    gCoreContext->ClearSettingsCache();
}

/**
 * Search for backends via UPnP, put up a UI for the user to choose one
 */
BackendSelection::Decision MythContextPrivate::ChooseBackend(const QString &error)
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
        BackendSelection::Prompt(&m_dbParams, XmlConfiguration::kDefaultFilename);
    // TODO encapuslation: don't use a pointer

    EndTempWindow();

    return ret;
}

/**
 * If there is only a single UPnP backend, use it.
 *
 * This does <i>not</i> prompt for PIN entry. If the backend requires one,
 * it will fail, and the caller needs to put up a UI to ask for one.
 */
int MythContextPrivate::UPnPautoconf(const std::chrono::milliseconds milliSeconds)
{
    auto seconds = duration_cast<std::chrono::seconds>(milliSeconds);
    LOG(VB_GENERAL, LOG_INFO, QString("UPNP Search %1 secs")
        .arg(seconds.count()));

    SSDP::Instance()->PerformSearch(kBackendURI, seconds);

    // Search for a total of 'milliSeconds' ms, sending new search packet
    // about every 250 ms until less than one second remains.
    MythTimer totalTime; totalTime.start();
    MythTimer searchTime; searchTime.start();
    while (totalTime.elapsed() < milliSeconds)
    {
        usleep(25000);
        auto ttl = milliSeconds - totalTime.elapsed();
        if ((searchTime.elapsed() > 249ms) && (ttl > 1s))
        {
            auto ttlSeconds = duration_cast<std::chrono::seconds>(ttl);
            LOG(VB_GENERAL, LOG_INFO, QString("UPNP Search %1 secs")
                .arg(ttlSeconds.count()));
            SSDP::Instance()->PerformSearch(kBackendURI, ttlSeconds);
            searchTime.start();
        }
    }

    SSDPCacheEntries *backends = SSDP::Find(kBackendURI);

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
    backends = nullptr;

    // We don't actually know the backend's access PIN, so this will
    // only work for ones that have PIN access disabled (i.e. 0000)
    int ret = (UPnPconnect(BE, QString())) ? 1 : -1;

    BE->DecrRef();

    return ret;
}

/**
 * Get the default backend from XmlConfiguration::kDefaultFilename, use UPnP to find it.
 *
 * Sets a string if there any connection problems
 */
bool MythContextPrivate::DefaultUPnP(QString& Error)
{
    static const QString loc = "DefaultUPnP() - ";

    // potentially saved in backendselect
    QString pin;
    QString usn;
    {
        auto config = XmlConfiguration(); // read-only
        pin = config.GetValue(kDefaultPIN, QString(""));
        usn = config.GetValue(kDefaultUSN, QString(""));
    }

    if (usn.isEmpty())
    {
        LOG(VB_UPNP, LOG_INFO, loc + "No default UPnP backend");
        return false;
    }

    LOG(VB_UPNP, LOG_INFO,
        loc + QString(XmlConfiguration::kDefaultFilename) +
        QString(" has default PIN '%1' and host USN: %2").arg(pin, usn));

    // ----------------------------------------------------------------------

    std::chrono::milliseconds timeout_ms {2s};
    auto timeout_s = duration_cast<std::chrono::seconds>(timeout_ms);
    LOG(VB_GENERAL, LOG_INFO, loc + QString("UPNP Search up to %1 secs")
        .arg(timeout_s.count()));
    SSDP::Instance()->PerformSearch(kBackendURI, timeout_s);

    // ----------------------------------------------------------------------
    // We need to give the server time to respond...
    // ----------------------------------------------------------------------

    DeviceLocation* devicelocation = nullptr;
    MythTimer totalTime;
    MythTimer searchTime;
    totalTime.start();
    searchTime.start();
    while (totalTime.elapsed() < timeout_ms)
    {
        devicelocation = SSDP::Find(kBackendURI, usn);
        if (devicelocation)
            break;

        usleep(25000);

        auto ttl = timeout_ms - totalTime.elapsed();
        if ((searchTime.elapsed() > 249ms) && (ttl > 1s))
        {
            auto ttlSeconds = duration_cast<std::chrono::seconds>(ttl);
            LOG(VB_GENERAL, LOG_INFO, loc + QString("UPNP Search up to %1 secs")
                .arg(ttlSeconds.count()));
            SSDP::Instance()->PerformSearch(kBackendURI, ttlSeconds);
            searchTime.start();
        }
    }

    // ----------------------------------------------------------------------

    if (!devicelocation)
    {
        Error = "Cannot find default UPnP backend";
        return false;
    }

    if (UPnPconnect(devicelocation, pin))
    {
        devicelocation->DecrRef();
        return true;
    }

    devicelocation->DecrRef();
    Error = "Cannot connect to default backend via UPnP. Wrong saved PIN?";
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
    switch (client.GetConnectionInfo(PIN, &m_dbParams, error))
    {
        case UPnPResult_Success:
            gCoreContext->GetDB()->SetDatabaseParams(m_dbParams);
            LOG(VB_UPNP, LOG_INFO, loc +
                "Got database hostname: " + m_dbParams.m_dbHostName);
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
    QUrl theURL(URL);
    URL = theURL.host();
    if (URL.isEmpty())
        return false;

    LOG(VB_UPNP, LOG_INFO, "Trying default DB credentials at " + URL);
    m_dbParams.m_dbHostName = URL;

    return true;
}

bool MythContextPrivate::event(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        if (m_disableeventpopup)
            return true;

        if (GetNotificationCenter() && m_registration < 0)
        {
            m_registration = GetNotificationCenter()->Register(this);
        }

        auto *me = dynamic_cast<MythEvent*>(e);
        if (me == nullptr)
            return true;

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
    QDateTime now = MythDate::current();

    if (!GetNotificationCenter() || !m_ui || !m_ui->IsScreenSetup())
        return;

    if (m_lastCheck.isValid() && now < m_lastCheck)
        return;

    // When WOL is disallowed, standy mode,
    // we should not show connection failures.
    if (!gCoreContext->IsWOLAllowed())
        return;

    m_lastCheck = now.addMSecs(5000); // don't refresh notification more than every 5s

    QString description = (persistent) ?
        QObject::tr(
            "The connection to the master backend "
            "server has gone away for some reason. "
            "Is it running?") :
        QObject::tr(
            "Could not connect to the master backend server. Is "
            "it running?  Is the IP address set for it in "
            "mythtv-setup correct?");

    QString message = QObject::tr("Could not connect to master backend");
    MythErrorNotification n(message, sLocation, description);
    n.SetId(m_registration);
    n.SetParent(this);
    GetNotificationCenter()->Queue(n);
}

void MythContextPrivate::HideConnectionFailurePopup(void)
{
    if (!GetNotificationCenter())
        return;

    if (!m_lastCheck.isValid())
        return;

    MythCheckNotification n(QObject::tr("Backend is online"), sLocation);
    n.SetId(m_registration);
    n.SetParent(this);
    n.SetDuration(5s);
    GetNotificationCenter()->Queue(n);
    m_lastCheck = QDateTime();
}

void MythContextPrivate::ShowVersionMismatchPopup(uint remote_version)
{
    if (m_mbeVersionPopup)
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
        m_mbeVersionPopup = ShowOkPopup(
            message, m_sh, &MythContextSlotHandler::VersionMismatchPopupClosed);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + message);
        qApp->exit(GENERIC_EXIT_SOCKET_ERROR);
    }
}

// Process Events while waiting for connection
// return true if progress is 100%
void MythContextPrivate::processEvents(void)
{
//    bool ret = false;
//    if (m_guiStartup)
//        ret = m_guiStartup->updateProgress();
    qApp->processEvents(QEventLoop::AllEvents, 250);
    qApp->processEvents(QEventLoop::AllEvents, 250);
//    return ret;
}

namespace
{
// cache some settings in GUISettingsCache::m_cacheFilename
// only call this if the database is available.

const std::array<QString, 13> GUISettingsCache::kSettings
{ "Theme", "Language", "Country", "GuiHeight",
  "GuiOffsetX", "GuiOffsetY", "GuiWidth", "RunFrontendInWindow",
  "AlwaysOnTop", "HideMouseCursor", "ThemePainter", "libCECEnabled",
  "StartupScreenDelay" };


bool GUISettingsCache::save()
{
    QString cacheDirName = GetConfDir() + '/' + m_cachePath;
    QDir dir(cacheDirName);
    dir.mkpath(cacheDirName);
    XmlConfiguration config = XmlConfiguration(m_cacheFilename);
    bool dirty = false;
    for (const auto & setting : kSettings)
    {
        QString cacheValue = config.GetValue("Settings/" + setting, QString());
        gCoreContext->ClearOverrideSettingForSession(setting);
        QString value = gCoreContext->GetSetting(setting, QString());
        if (value != cacheValue)
        {
            config.SetValue("Settings/" + setting, value);
            dirty = true;
        }
    }
    clearOverrides();

    if (dirty)
    {
#ifndef Q_OS_ANDROID
        DestroyMythMainWindow();
#endif
        return config.Save();
    }
    return true;
}

void GUISettingsCache::loadOverrides() const
{
    auto config = XmlConfiguration(m_cacheFilename); // read only
    for (const auto & setting : kSettings)
    {
        if (!gCoreContext->GetSetting(setting, QString()).isEmpty())
            continue;
        QString value = config.GetValue("Settings/" + setting, QString());
        if (!value.isEmpty())
            gCoreContext->OverrideSettingForSession(setting, value);
    }
    // Prevent power off TV after temporary window
    gCoreContext->OverrideSettingForSession("PowerOffTVAllowed", nullptr);

    MythTranslation::load("mythfrontend");
}

void GUISettingsCache::clearOverrides()
{
    QString language = gCoreContext->GetSetting("Language", QString());
    for (const auto & setting : kSettings)
        gCoreContext->ClearOverrideSettingForSession(setting);
    // Restore power off TV setting
    gCoreContext->ClearOverrideSettingForSession("PowerOffTVAllowed");

    if (language != gCoreContext->GetSetting("Language", QString()))
        MythTranslation::load("mythfrontend");
}

} // anonymous namespace

void MythContextSlotHandler::VersionMismatchPopupClosed(void)
{
    d->m_mbeVersionPopup = nullptr;
    qApp->exit(GENERIC_EXIT_SOCKET_ERROR);
}

MythContext::MythContext(QString binversion, bool needsBackend)
    : d(new MythContextPrivate(this)),
      m_appBinaryVersion(std::move(binversion))
{
#ifdef _WIN32
    static bool WSAStarted = false;
    if (!WSAStarted)
    {
        WSADATA wsadata;
        int res = WSAStartup(MAKEWORD(2, 0), &wsadata);
        LOG(VB_SOCKET, LOG_INFO,
                 QString("WSAStartup returned %1").arg(res));
    }
#endif

    d->m_needsBackend = needsBackend;

    gCoreContext = new MythCoreContext(m_appBinaryVersion, d);

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

    qRegisterMetaType<std::chrono::seconds>("std::chrono::seconds");
    qRegisterMetaType<std::chrono::milliseconds>("std::chrono::milliseconds");
    qRegisterMetaType<std::chrono::microseconds>("std::chrono::microseconds");

    SetDisableEventPopup(true);

    if (gui && QCoreApplication::applicationName() == MYTH_APPNAME_MYTHTV_SETUP)
    {
        d->TempMainWindow(false);
        QString warning = QObject::tr("mythtv-setup is deprecated.\n"
                "To set up MythTV, start mythbackend and use:\n"
                "http://localhost:6544/setupwizard");
        WaitFor(ShowOkPopup(warning));
    }

    if (m_appBinaryVersion != MYTH_BINARY_VERSION)
    {
        LOG(VB_GENERAL, LOG_EMERG,
                 QString("Application binary version (%1) does not "
                         "match libraries (%2)")
                     .arg(m_appBinaryVersion, MYTH_BINARY_VERSION));

        QString warning = QObject::tr(
            "This application is not compatible "
            "with the installed MythTV libraries.");
        if (gui)
        {
            d->TempMainWindow(false);
            WaitFor(ShowOkPopup(warning));
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
    QString confdir = qEnvironmentVariable("MYTHCONFDIR");
    if ((homedir.isEmpty() || homedir == "/") &&
        (confdir.isEmpty() || confdir.contains("$HOME")))
    {
        QString warning = "Cannot locate your home directory."
                          " Please set the environment variable HOME";
        if (gui)
        {
            d->TempMainWindow(false);
            WaitFor(ShowOkPopup(warning));
        }
        LOG(VB_GENERAL, LOG_WARNING, warning);

        return false;
    }

    if (!d->Init(gui, promptForBackend, disableAutoDiscovery, ignoreDB))
    {
        return false;
    }

    SetDisableEventPopup(false);

    if (d->m_gui)
    {
        saveSettingsCache();
    }

    gCoreContext->ActivateSettingsCache(true);
    gCoreContext->InitPower();

    return true;
}

MythContext::~MythContext()
{
    gCoreContext->InitPower(false /*destroy*/);
    if (MThreadPool::globalInstance()->activeThreadCount())
        LOG(VB_GENERAL, LOG_INFO, "Waiting for threads to exit.");

    MThreadPool::globalInstance()->waitForDone();
    SSDP::Shutdown();
    TaskQueue::Shutdown();

    LOG(VB_GENERAL, LOG_INFO, "Exiting");

    logStop();

    delete gCoreContext;
    gCoreContext = nullptr;

    delete d;
}

void MythContext::SetDisableEventPopup(bool check)
{
    d->m_disableeventpopup = check;
}

bool MythContext::SaveDatabaseParams(const DatabaseParams &params, bool force)
{
    return d->SaveDatabaseParams(params, force);
}

bool MythContext::saveSettingsCache(void)
{
    /* this check is technically redundant since this is only called from
    MythContext::Init() and mythfrontend::main(); however, it is for safety
    and clarity until MythGUIContext is refactored out.
    */
    if (d->m_gui)
    {
        return d->m_GUISettingsCache.save();
    }
    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
