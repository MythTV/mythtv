#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qpainter.h>
#include <unistd.h>
#include <qsqldatabase.h>
#include <qurl.h>
#include <qnetwork.h>
#include <qwaitcondition.h>

#include <qsocketdevice.h>
#include <qhostaddress.h>

#include <cerrno>
#include <cmath>
#include <queue>

#include "config.h"
#include "mythcontext.h"
#include "oldsettings.h"
#include "themedmenu.h"
#include "util.h"
#include "remotefile.h"
#include "dialogbox.h"
#include "mythdialogs.h"
#include "mythplugin.h"
#include "screensaver.h"
#include "DisplayRes.h"
#include "dbsettings.h"
#include "langsettings.h"
#include "mythdbcon.h"

// These defines provide portability for different
// plugin file names.
#ifdef CONFIG_DARWIN
const QString kPluginLibPrefix = "lib";
const QString kPluginLibSuffix = ".dylib";
#else
const QString kPluginLibPrefix = "lib";
const QString kPluginLibSuffix = ".so";
#endif

MythContext *gContext = NULL;
QMutex MythContext::verbose_mutex(true);

int print_verbose_messages = VB_IMPORTANT | VB_GENERAL;

class MythContextPrivate
{
  public:
    MythContextPrivate(MythContext *lparent);
   ~MythContextPrivate();

    bool Init(bool gui);
    bool IsWideMode() const {return (m_baseWidth == 1280);}
    void SetWideMode() {m_baseWidth = 1280; m_baseHeight = 720;}
    bool IsSquareMode() const {return (m_baseWidth == 800);}
    void SetSquareMode() {m_baseWidth = 800; m_baseHeight = 600;}

    void GetScreenBounds(void);
    void StoreGUIsettings(void);

    void LoadLogSettings(void);
    bool LoadDatabaseSettings(bool reload);
    
    bool FixSettingsFile(void);
    bool WriteSettingsFile(const DatabaseParams &params,
                           bool overwrite = false);
    bool FindSettingsProbs(void);

    QString getResponse(const QString &query, const QString &def);
    int intResponse(const QString &query, int def);
    bool PromptForDatabaseParams(void);

    MythContext *parent;

    Settings *m_settings;
    Settings *m_qtThemeSettings;

    QString m_installprefix;

    bool m_gui;
    bool m_backend;
    bool m_themeloaded;
    QString m_menuthemepathname;
    QString m_themepathname;
    QPixmap *m_backgroundimage;
    QPalette m_palette;

    // Drawable area of the full screen. May cover several screens,
    // or exclude windowing system fixtures (like Mac menu bar)
    int m_xbase, m_ybase;
    int m_height, m_width;

    // Dimensions of the theme
    int m_baseWidth, m_baseHeight;
    
    QString m_localhostname;

    QMutex serverSockLock;

    QPtrList<QObject> listeners;

    MDBManager m_dbmanager;
    
    QMap<QString, QImage> imageCache;

    QString language;

    MythMainWindow *mainWindow;

    float m_wmult, m_hmult;

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to drawable area above.
    int m_screenxbase, m_screenybase;
    int m_screenwidth, m_screenheight;

    // Command-line GUI size, which overrides both the above sets of sizes
    int m_geometry_x, m_geometry_y;
    int m_geometry_w, m_geometry_h;

    QString themecachedir;

    int bigfontsize, mediumfontsize, smallfontsize;

    QSocketDevice *serverSock;
    QSocket *eventSock;

    bool disablelibrarypopup;

    MythPluginManager *pluginmanager;

    ScreenSaverControl *screensaver;

    int m_logenable, m_logmaxcount, m_logprintlevel;
    QMap<QString,int> lastLogCounts;
    QMap<QString,QString> lastLogStrings;

    bool screensaverEnabled;

    DisplayRes *display_res;

    QMutex *m_priv_mutex;
    queue<MythPrivRequest> m_priv_requests;
    QWaitCondition m_priv_queued;
};



MythContextPrivate::MythContextPrivate(MythContext *lparent)
{
    pluginmanager = NULL;

    parent = lparent;

    char *tmp_installprefix = getenv("MYTHTVDIR");
    if (tmp_installprefix)
        m_installprefix = tmp_installprefix;
    else
        m_installprefix = PREFIX;

    if (QDir(m_installprefix).isRelative())
    {
        // If the PREFIX is relative, evaluate it relative to our
        // executable directory. This can be fragile on Unix, so
        // use relative PREFIX values with care.
#if QT_VERSION >= 0x030200
        QDir prefixDir = qApp->applicationDirPath();
#else
        QString appPath = QFileInfo(qApp->argv()[0]).absFilePath();
        QDir prefixDir(appPath.left(appPath.findRev("/")));
#endif
        prefixDir.cd(m_installprefix);
        m_installprefix = prefixDir.canonicalPath();

        VERBOSE(VB_ALL, QString("Using runtime prefix = %1")
                                .arg(m_installprefix));
    }

    m_backend = false;
    m_settings = new Settings;
    m_qtThemeSettings = new Settings;

    language = "";
    m_themeloaded = false;
    m_backgroundimage = NULL;

    screensaver = ScreenSaverControl::get();
    m_baseHeight = 600;
    m_baseWidth = 800;

    m_xbase = m_ybase = 0;
    m_height = m_width = 0;
    m_screenxbase = m_screenybase = 0;
    m_screenheight = m_screenwidth = 0;
    m_geometry_x = m_geometry_y = 0;
    m_geometry_w = m_geometry_h = 0;
}

// Get screen size from Qt. If the windowing system environment
// has multiple screens (e.g. Xinerama or Mac OS X),
// QApplication::desktop()->width() will span all of them,
// so we usually need to get the geometry of a specific screen.

void MythContextPrivate::GetScreenBounds()
{
    if (m_geometry_w)
    {
        // Geometry on the command-line overrides everything
        m_xbase  = m_geometry_x;
        m_ybase  = m_geometry_y;
        m_width  = m_geometry_w;
        m_height = m_geometry_h;
        return;
    }

    QDesktopWidget * desktop = QApplication::desktop();

cout << "Total desktop width=" << desktop->width() <<
        ", height="     << desktop->height() <<
        ", numscreens=" << desktop->numScreens() << endl;

    int screen = parent->GetNumSetting("XineramaScreen", 0);

    if (screen == -1)       // Special case - span all screens
    {
        VERBOSE(VB_GENERAL, QString("Using all screens (currently %1)")
                            .arg(desktop->numScreens()));
        m_xbase  = 0;
        m_ybase  = 0;
        m_width  = desktop->width();
        m_height = desktop->height();

        VERBOSE(VB_GENERAL, QString("Total width = %1, height = %2")
                            .arg(m_width).arg(m_height));
    }
    else                    // User specified a single screen
    {
        if (screen < 0 || screen >= desktop->numScreens())
        {
            VERBOSE(VB_ALL, QString("Xinerama screen %1 was specified,"
                                    " but only %2 available?")
                            .arg(screen).arg(desktop->numScreens()));
            screen = 0;
        }


        QRect bounds;

        bool inWindow = parent->GetNumSetting("RunFrontendInWindow", 0);

        if (inWindow)
            VERBOSE(VB_ALL, QString("Running in a window"));

        if (inWindow)
            // This doesn't include the area occupied by the
            // Windows taskbar, or the Mac OS X menu bar and Dock
            bounds = desktop->availableGeometry(screen);
        else
            bounds = desktop->screenGeometry(screen);

        m_xbase  = bounds.x();
        m_ybase  = bounds.y();
        m_width  = bounds.width();
        m_height = bounds.height();

        VERBOSE(VB_GENERAL, QString("Using screen %1, %2x%3 at %4,%5")
                            .arg(screen).arg(m_width).arg(m_height)
                            .arg(m_xbase).arg(m_ybase));
    }
}

bool MythContextPrivate::Init(bool gui)
{
    m_gui = gui;
    if (!LoadDatabaseSettings(false))
        return false;

    serverSock = NULL;
    eventSock = new QSocket(0);

    m_logenable = -1;
    m_logmaxcount = -1;
    m_logprintlevel = -1;

    mainWindow = NULL;

    disablelibrarypopup = false;

    display_res = NULL;

    m_priv_mutex = new QMutex(true);

    if (!MSqlQuery::testDBConnection())
    {
        if (m_gui && PromptForDatabaseParams())
        {
            if(!MSqlQuery::testDBConnection())
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    // ---- keep all DB-using stuff below this line ----

    if (gui)
    {
        GetScreenBounds();
        StoreGUIsettings();
    }

    return true;
}

MythContextPrivate::~MythContextPrivate()
{
    imageCache.clear();

    if (m_settings)
        delete m_settings;
    if (m_qtThemeSettings)
        delete m_qtThemeSettings;
    if (serverSock)
        delete serverSock;
    if (eventSock)
        delete eventSock;
    if (m_priv_mutex)
        delete m_priv_mutex;
}

// Apply any user overrides to the screen geometry

void MythContextPrivate::StoreGUIsettings()
{
    if (m_geometry_w)
    {
        // Geometry on the command-line overrides everything
        m_screenxbase  = m_geometry_x;
        m_screenybase  = m_geometry_y;
        m_screenwidth  = m_geometry_w;
        m_screenheight = m_geometry_h;
    }
    else
    {
        m_screenxbase  = parent->GetNumSetting("GuiOffsetX");
        m_screenybase  = parent->GetNumSetting("GuiOffsetY");
        m_screenwidth = m_screenheight = 0;
        parent->GetResolutionSetting("Gui", m_screenwidth, m_screenheight);
    }

    // If any of these was _not_ set by the user,
    // (i.e. they are 0) use the whole-screen defaults

    if (!m_screenxbase)
        m_screenxbase = m_xbase;
    if (!m_screenybase)
        m_screenybase = m_ybase;
    if (!m_screenwidth)
        m_screenwidth = m_width;
    if (!m_screenheight)
        m_screenheight = m_height;

    if (m_screenheight < 160 || m_screenwidth < 160)
    {
        VERBOSE(VB_ALL, "Somehow, your screen size settings are bad.");
        VERBOSE(VB_ALL, QString("GuiResolution: %1")
                        .arg(parent->GetSetting("GuiResolution")));
        VERBOSE(VB_ALL, QString("  old GuiWidth: %1")
                        .arg(parent->GetNumSetting("GuiWidth")));
        VERBOSE(VB_ALL, QString("  old GuiHeight: %1")
                        .arg(parent->GetNumSetting("GuiHeight")));
        VERBOSE(VB_ALL, QString("m_width: %1").arg(m_width));
        VERBOSE(VB_ALL, QString("m_height: %1").arg(m_height));
        VERBOSE(VB_ALL, "Falling back to 640x480");

        m_screenwidth  = 640;
        m_screenheight = 480;
    }

    m_wmult = m_screenwidth  / (float)m_baseWidth;
    m_hmult = m_screenheight / (float)m_baseHeight;
    
    //VERBOSE(VB_ALL, QString("GUI multipliers are: width %1, height %2").arg(m_wmult).arg(m_hmult));
}


void MythContextPrivate::LoadLogSettings(void)
{
    m_logenable = parent->GetNumSetting("LogEnabled", 0);
    m_logmaxcount = parent->GetNumSetting("LogMaxCount", 0);
    m_logprintlevel = parent->GetNumSetting("LogPrintLevel", LP_ERROR);
}

bool MythContextPrivate::LoadDatabaseSettings(bool reload)
{
    if (reload)
    {
        if (m_settings)
            delete m_settings;
        m_settings = new Settings;
    }
    
    if (!parent->LoadSettingsFiles("mysql.txt"))
    {
        VERBOSE(VB_IMPORTANT, "Unable to read configuration file mysql.txt");
        if (!FixSettingsFile())
            return false;
        else
            parent->LoadSettingsFiles("mysql.txt");
    }

    // Even if we have loaded the settings file, it may be incomplete,
    // so we check for missing values and warn user
    FindSettingsProbs();

    m_localhostname = m_settings->GetSetting("LocalHostName", NULL);
    if (m_localhostname == NULL)
    {
        char localhostname[1024];
        if (gethostname(localhostname, 1024))
        {
            VERBOSE(VB_IMPORTANT,
                    QString("MCP: Error, could not determine "
                            "host name. '%1'").arg(strerror(errno)));
            return false;
        }
        m_localhostname = localhostname;
    }
    return true;
}

bool MythContextPrivate::FixSettingsFile(void)
{
    VERBOSE(VB_IMPORTANT, "Trying to create a basic mysql.txt file");
    DatabaseParams defaultParams = parent->GetDatabaseParams();
    
    return WriteSettingsFile(defaultParams);
}

bool MythContextPrivate::WriteSettingsFile(const DatabaseParams &params,
                                           bool overwrite)
{
    QString path = MythContext::GetConfDir() + "/mysql.txt";
    QFile   * f  = new QFile(path);
    
    if (!overwrite && f->exists())
    {
        return false;
    }

    QString dirpath = MythContext::GetConfDir();
    QDir createDir(dirpath);

    if (!createDir.exists())
    {
        if (!createDir.mkdir(dirpath, true))
        {
            VERBOSE(VB_IMPORTANT, QString("Could not create %1").arg(dirpath));
            return false;
        }
    }

    if (!f->open(IO_WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("Could not open settings file %1 "
                                      "for writing").arg(path));
        return false;
    }
    
    VERBOSE(VB_IMPORTANT, QString("Writing settings file %1").arg(path));
    QTextStream s(f);
    s << "DBHostName=" << params.dbHostName << endl
      << "DBUserName=" << params.dbUserName << endl
      << "DBPassword=" << params.dbPassword << endl
      << "DBName="     << params.dbName     << endl
      << "DBType="     << params.dbType     << endl
      << endl
      << "# Set the following if you want to use something other than the\n"
      << "# machine's real hostname for identifying settings in the database.\n"
      << "# This is useful if your hostname changes often, as otherwise\n"
      << "# you'll need to reconfigure mythtv (or futz with the DB) every time.\n"
      << "# TWO HOSTS MUST NOT USE THE SAME VALUE\n"
      << "#\n";
    
    if (params.localEnabled)
        s << "LocalHostName=" << params.localHostName << endl;
    else
        s << "#LocalHostName=my-unique-identifier-goes-here\n";
    
    s << endl
      << "# If you want your frontend to be able to wake your MySQL server\n"
      << "# using WakeOnLan, have a look at the following settings:\n"
      << "#\n"
      << "# Set the time the frontend waits (in seconds) between reconnect tries.\n"
      << "# This should be the rough time your MySQL server needs for startup\n";

    if (params.wolEnabled)
        s << "WOLsqlReconnectWaitTime=" << params.wolReconnect << endl;
    else
        s << "#WOLsqlReconnectWaitTime=0\n";
    
    s << "#\n"
      << "#\n"
      << "# This is the amount of retries to wake the MySQL server until the frontend\n"
      << "# gives up\n";
     
    if (params.wolEnabled)
        s << "WOLsqlConnectRetry=" << params.wolRetry << endl;
    else
        s << "#WOLsqlConnectRetry=5\n";
    
    s << "#\n"
      << "#\n"
      << "# This is the command executed to wake your MySQL server.\n";
    
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
    
    if (m_settings->GetSetting("DBHostName").isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBHostName is not set in mysql.txt");
    }
    if (m_settings->GetSetting("DBUserName").isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBUserName is not set in mysql.txt");
    }
    if (m_settings->GetSetting("DBPassword").isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBPassword is not set in mysql.txt");
    }
    if (m_settings->GetSetting("DBName").isEmpty())
    {
        problems = true;
        VERBOSE(VB_IMPORTANT, "DBName is not set in mysql.txt");
    }
    return problems;
}

QString MythContextPrivate::getResponse(const QString &query,
                                        const QString &def)
{
    cout << query;

    if (def != "")
        cout << " [" << def << "]  ";
    else
        cout << "  ";
    
    char response[80];
    cin.clear();
    cin.getline(response, 80);
    if (!cin.good())
    {
        cout << endl;
        VERBOSE(VB_IMPORTANT, "Read from stdin failed");
        return NULL;
    }

    QString qresponse = response;

    if (qresponse == "")
        qresponse = def;

    return qresponse;
}

int MythContextPrivate::intResponse(const QString &query, int def)
{
    QString str_resp = getResponse(query, QString("%1").arg(def));
    if (!str_resp)
        return false;
    bool ok;
    int resp = str_resp.toInt(&ok);
    return (ok ? resp : def);
}

bool MythContextPrivate::PromptForDatabaseParams(void)
{
    bool accepted = false;
    if (m_gui)
    {
        // construct minimal MythMainWindow
        m_settings->SetSetting("Theme", "blue");
#ifdef Q_WS_MACX
        // Myth looks horrible in default Mac style for Qt
        m_settings->SetSetting("Style", "Windows");
#endif
        parent->LoadQtConfig();
   
        MythMainWindow *mainWindow = new MythMainWindow();
        parent->SetMainWindow(mainWindow);

        // ask user for language settings
        LanguageSettings::prompt();
        LanguageSettings::load("mythfrontend");
        
        // ask user for database parameters
        DatabaseSettings settings;
        accepted = (settings.exec() == QDialog::Accepted);
        if (!accepted)
            VERBOSE(VB_IMPORTANT, "User canceled database configuration");

        // tear down temporary main window
        delete mainWindow;
    }
    else
    {
        DatabaseParams params = parent->GetDatabaseParams();
        QString response;
        
        // give user chance to skip config
        response = getResponse("Would you like to configure the database "
                               "connection now?",
                               "yes");
        if (!response || response.left(1).lower() != "y")
            return false;
        
        params.dbHostName = getResponse("Database host name:",
                                        params.dbHostName);
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
                               (params.wolEnabled ? "no" : "yes"));
        if (response.left(1).lower() == "y")
        {
            params.wolEnabled   = true;
            params.wolReconnect = intResponse("Seconds to wait for "
                                              "reconnection:",
                                              params.wolReconnect);
            params.wolRetry     = intResponse("Number of times to retry:",
                                              params.wolRetry);
            params.wolCommand   = getResponse("Command to use to wake server:",
                                              params.wolCommand);
        }
        else
            params.wolEnabled   = false;
        
        accepted = parent->SaveDatabaseParams(params);
    }
    return accepted;
}

MythContext::MythContext(const QString &binversion)
    : QObject(), d(NULL), app_binary_version(binversion)
{
    qInitNetworkProtocols();
}

bool MythContext::Init(bool gui)
{
    if (app_binary_version != MYTH_BINARY_VERSION)
    {
        VERBOSE(VB_IMPORTANT,
                QString("This app was compiled against libmyth version: %1"
                "\n\t\t\tbut the library is version: %2"
                "\n\t\t\tYou probably want to recompile everything, and do a"
                "\n\t\t\t'make distclean' first.")
                .arg(app_binary_version).arg(MYTH_BINARY_VERSION));
        return false;
    }

    d = new MythContextPrivate(this);

    if (!d->Init(gui))
        return false;

    connect(d->eventSock, SIGNAL(connected()), 
            this, SLOT(EventSocketConnected()));
    connect(d->eventSock, SIGNAL(readyRead()), 
            this, SLOT(EventSocketRead()));
    connect(d->eventSock, SIGNAL(connectionClosed()), 
            this, SLOT(EventSocketClosed()));

    return true;
}

MythContext::~MythContext()
{
    if (d)
        delete d;
}

bool MythContext::ConnectToMasterServer(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);
    return ConnectServer(server, port);
}

bool MythContext::ConnectServer(const QString &hostname, int port)
{
    if (d->serverSock)
        return true;

    int cnt = 0;

    int sleepTime = GetNumSetting("WOLbackendReconnectWaitTime", 0);
    int maxConnTry = GetNumSetting("WOLbackendConnectRetry", 1);

    do
    {
        VERBOSE(VB_GENERAL, QString("Connecting to backend server: "
                                    "%1:%2 (try %3 of %4)")
                                    .arg(hostname).arg(port).arg(cnt+1)
                                    .arg(maxConnTry));

        d->serverSock = new QSocketDevice(QSocketDevice::Stream);

        if (!connectSocket(d->serverSock, hostname, port))
        {
            delete d->serverSock;
            d->serverSock = NULL;
        
            // only inform the user of a failure if WOL is disabled 
            if (sleepTime <= 0)
            {
                cerr << "Connection timed out.\n";
                cerr << "You probably should modify the Master Server "
                     << "settings\nin the setup program and set the proper "
                     << "IP address.\n";
                if (d->m_height && d->m_width)
                {
                    MythPopupBox::showOkPopup(d->mainWindow, 
                                              "connection failure",
                                              tr("Could not connect to the "
                                                 "master backend server -- is "
                                                 "it running?  Is the IP "
                                                 "address set for it in the "
                                                 "setup program correct?"));
                }

                return false;
            }
            else
            {
                VERBOSE(VB_GENERAL, "Trying to wake up the MasterBackend "
                                    "now.");
                QString wol_cmd = GetSetting("WOLbackendCommand",
                                             "echo \'would run the "
                                             "WakeServerCommand now, if "
                                             "set!\'");
                system(wol_cmd.ascii());

                VERBOSE(VB_GENERAL, QString("Waiting for %1 seconds until I "
                                            "try to reconnect again.")
                                            .arg(sleepTime));
                sleep(sleepTime);
                ++cnt;
            }
        }
        else
            break;
    }
    while (cnt <= maxConnTry);

    if (!CheckProtoVersion(d->serverSock))
    {
        delete d->serverSock;
        d->serverSock = NULL;
        return false;
    }

    // called with the lock
    QString str = QString("ANN Playback %1 %2")
                         .arg(d->m_localhostname).arg(false);
    QStringList strlist = str;
    WriteStringList(d->serverSock, strlist);
    ReadStringList(d->serverSock, strlist, true);

    if (d->eventSock->state() == QSocket::Idle)    
        d->eventSock->connectToHost(hostname, port);
    
    return true;
}

bool MythContext::IsConnectedToMaster(void)
{
    return d->serverSock;
}

void MythContext::SetBackend(bool backend)
{
    d->m_backend = backend;
}

bool MythContext::IsBackend(void)
{
    return d->m_backend;
}

QString MythContext::GetMasterHostPrefix(void)
{
    QString ret = "";

    if (!d->serverSock)
    {
        d->serverSockLock.lock();
        ConnectToMasterServer();
        d->serverSockLock.unlock();
    }
    
    if (d->serverSock)
        ret = QString("myth://%1:%2/")
                     .arg(d->serverSock->peerAddress().toString())
                     .arg(d->serverSock->peerPort());
    return ret;
}

QString MythContext::GetHostName(void)
{
    return d->m_localhostname; 
}

QString MythContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

QString MythContext::GetInstallPrefix(void) 
{ 
    return d->m_installprefix; 
}

QString MythContext::GetConfDir(void)
{
    char *tmp_confdir = getenv("MYTHCONFDIR");
    QString dir;
    if (tmp_confdir)
    {
        dir = QString(tmp_confdir);
        //VERBOSE(VB_ALL, QString("Read conf dir = %1").arg(dir));
        dir.replace("$HOME", QDir::homeDirPath());
    }
    else
        dir = QDir::homeDirPath() + "/.mythtv";

    //VERBOSE(VB_ALL, QString("Using conf dir = %1").arg(dir));
    return dir;
}

QString MythContext::GetShareDir(void) 
{ 
    return d->m_installprefix + "/share/mythtv/"; 
}

QString MythContext::GetLibraryDir(void) 
{ 
    return d->m_installprefix + "/lib/mythtv/"; 
}

QString MythContext::GetThemesParentDir(void) 
{ 
    return GetShareDir() + "themes/"; 
}

QString MythContext::GetPluginsDir(void) 
{ 
    return GetLibraryDir() + "plugins/"; 
}

QString MythContext::GetPluginsNameFilter(void) 
{ 
    return kPluginLibPrefix + "*" + kPluginLibSuffix; 
}

QString MythContext::FindPlugin(const QString &plugname) 
{ 
    return GetPluginsDir() + kPluginLibPrefix + plugname + kPluginLibSuffix; 
}

QString MythContext::GetTranslationsDir(void) 
{ 
    return GetShareDir() + "i18n/"; 
}

QString MythContext::GetTranslationsNameFilter(void) 
{ 
    return "mythfrontend_*.qm"; 
}

QString MythContext::FindTranslation(const QString &translation) 
{ 
    return GetTranslationsDir() + "mythfrontend_" + translation.lower() + ".qm"; 
}

QString MythContext::GetFontsDir(void) 
{ 
    return GetShareDir();
}

QString MythContext::GetFontsNameFilter(void) 
{ 
    return "*ttf"; 
}

QString MythContext::FindFont(const QString &fontname) 
{ 
    return GetFontsDir() + fontname + ".ttf"; 
}

QString MythContext::GetFiltersDir(void) 
{ 
    return GetLibraryDir() + "filters/"; 
}

bool MythContext::LoadSettingsFiles(const QString &filename)
{
    return d->m_settings->LoadSettingsFiles(filename, d->m_installprefix);
}

void MythContext::LoadQtConfig(void)
{
    d->language = "";
    d->themecachedir = "";

    DisplayRes *dispRes = DisplayRes::GetDisplayRes(); // create singleton
    if (dispRes && GetNumSetting("UseVideoModes", 0))
    {
        d->display_res = dispRes;
        // Make sure DisplayRes has current context info
        d->display_res->Initialize();

        // Switch to desired GUI resolution
        d->display_res->SwitchToGUI();

        // Note the possibly changed screen settings
        d->GetScreenBounds();
    }
    else if (d->m_width <= 0 || d->m_height <= 0)
    {
        // For some reason, the screen resolution has not been stored yet.
        // This could be some strange error, or we could be bootstrapping
        // before connecting to a database. Either way, store screen size
        d->GetScreenBounds();
    }
    
    

    if (d->m_qtThemeSettings)
        delete d->m_qtThemeSettings;

    d->m_qtThemeSettings = new Settings;

    QString style = GetSetting("Style", "");
    if (style != "")
        qApp->setStyle(style);

    QString themename = GetSetting("Theme");    
    QString themedir = FindThemeDir(themename);
    
    if (themename.contains("-wide", false))
    {
        VERBOSE( VB_ALL, QString("Switching to wide mode (%1)").arg(themename));
        d->SetWideMode();
    }
    else
    {
        VERBOSE( VB_ALL, QString("Switching to square mode (%1)").arg(themename));
        d->SetSquareMode();
    }
    
    // Recalculate GUI dimensions
    d->StoreGUIsettings();
        
    d->m_themepathname = themedir + "/";
    
    themedir += "/qtlook.txt";
    d->m_qtThemeSettings->ReadSettings(themedir);
    d->m_themeloaded = false;

    if (d->m_backgroundimage)
        delete d->m_backgroundimage;
    d->m_backgroundimage = NULL;

    themename = GetSetting("MenuTheme");
    d->m_menuthemepathname = FindThemeDir(themename) +"/";

    d->bigfontsize    = GetNumSetting("QtFontBig",    25);
    d->mediumfontsize = GetNumSetting("QtFontMedium", 16);
    d->smallfontsize  = GetNumSetting("QtFontSmall",  12);
}

void MythContext::RefreshBackendConfig(void)
{
    d->LoadLogSettings();
}

void MythContext::UpdateImageCache(void)
{
    d->imageCache.clear();

    ClearOldImageCache();
    CacheThemeImages();
}

void MythContext::ClearOldImageCache(void)
{
    QString cachedirname = MythContext::GetConfDir() + "/themecache/";

    d->themecachedir = cachedirname + GetSetting("Theme") + "." + 
                       QString::number(d->m_screenwidth) + "." + 
                       QString::number(d->m_screenheight);

    QDir dir(cachedirname);

    if (!dir.exists())
        dir.mkdir(cachedirname);

    QString themecachedir = d->themecachedir;

    d->themecachedir += "/";

    dir.setPath(themecachedir);
    if (!dir.exists())
        dir.mkdir(themecachedir);

    dir.setPath(cachedirname);

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir() && !fi->isSymLink())
        {
            if (fi->absFilePath() == themecachedir)
                continue;
            RemoveCacheDir(fi->absFilePath());
        }
    }
}

void MythContext::RemoveCacheDir(const QString &dirname)
{
    QString cachedirname = MythContext::GetConfDir() + "/themecache/";

    if (!dirname.startsWith(cachedirname))
        return;

    cout << "removing stale cache dir: " << dirname << endl;

    QDir dir(dirname);

    if (!dir.exists())
        return;

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isFile() && !fi->isSymLink())
        {
            QFile file(fi->absFilePath());
            file.remove();
        }
        else if (fi->isDir() && !fi->isSymLink())
        {
            RemoveCacheDir(fi->absFilePath());
        }
    }

    dir.rmdir(dirname);
}
    
void MythContext::CacheThemeImages(void)
{
    QString baseDir = d->m_installprefix + "/share/mythtv/themes/default/";

    if (d->m_screenwidth == d->m_baseWidth && d->m_screenheight == d->m_baseHeight)
        return;

    CacheThemeImagesDirectory(d->m_themepathname);
    CacheThemeImagesDirectory(baseDir);
}

void MythContext::CacheThemeImagesDirectory(const QString &dirname,
                                            const QString &subdirname)
{
    QDir dir(dirname);
    
    if (!dir.exists())
        return;

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    MythProgressDialog *caching = NULL;
    if (subdirname.length() == 0)
        caching = new MythProgressDialog(QObject::tr("Pre-scaling theme images"),
                                         list->count());

    int progress = 0;

    QString destdir = d->themecachedir;
    if (subdirname.length() > 0)
        destdir += subdirname + "/";

    while ((fi = it.current()) != 0)
    {
        if (caching)
            caching->setProgress(progress);
        progress++;

        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir() && subdirname.length() == 0)
        {
            QString newdirname = fi->fileName();
            QDir newsubdir(d->themecachedir + newdirname);
            if (!newsubdir.exists())
                newsubdir.mkdir(d->themecachedir + newdirname);

            CacheThemeImagesDirectory(dirname + "/" + newdirname,
                                      newdirname);
            continue;
        }
        else if (fi->isDir())
            continue;

        if (fi->extension().lower() != "png" && 
            fi->extension().lower() != "jpg" &&
            fi->extension().lower() != "gif" &&
            fi->extension().lower() != "jpeg")
            continue;

        QString filename = fi->fileName();
        QFileInfo cacheinfo(destdir + filename);

        if (!cacheinfo.exists() ||
            (cacheinfo.lastModified() < fi->lastModified()))
        {
            VERBOSE(VB_FILE, QString("generating cache image for: %1")
                    .arg(fi->absFilePath()));

            QImage *tmpimage = LoadScaleImage(fi->absFilePath(), false);

            if (tmpimage && tmpimage->width() > 0 && tmpimage->height() > 0)
            {
                if (!tmpimage->save(destdir + filename, "PNG"))
                {
                    cerr << "Couldn't save cache cache image: "
                         << d->themecachedir + filename << endl;
                }
             
                delete tmpimage;
            }
        }
    }

    if (caching)
    {
        caching->Close();
        delete caching;
    }
}

void MythContext::GetScreenBounds(int &xbase, int &ybase,
                                  int &width, int &height)
{
    xbase  = d->m_xbase;
    ybase  = d->m_ybase;
    
    width  = d->m_width;
    height = d->m_height;
}

void MythContext::GetScreenSettings(float &wmult, float &hmult)
{
    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = d->m_screenheight;
    width = d->m_screenwidth;

    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythContext::GetScreenSettings(int &xbase, int &width, float &wmult, 
                                    int &ybase, int &height, float &hmult)
{
    xbase  = d->m_screenxbase;
    ybase  = d->m_screenybase;
    
    height = d->m_screenheight;
    width = d->m_screenwidth;

    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythContext::GetResolutionSetting(const QString &type,
                                       int &width, int &height,
                                       double &forced_aspect,
                                       short &refresh_rate, 
                                       int index)
{
    bool ok = false, ok0 = false, ok1 = false;
    QString sRes =    QString("%1Resolution").arg(type);
    QString sRR =     QString("%1RefreshRate").arg(type);
    QString sAspect = QString("%1ForceAspect").arg(type);
    QString sWidth =  QString("%1Width").arg(type);
    QString sHeight = QString("%1Height").arg(type);
    if (index >= 0)
    {
        sRes =    QString("%1Resolution%2").arg(type).arg(index);
        sRR =     QString("%1RefreshRate%2").arg(type).arg(index);
        sAspect = QString("%1ForceAspect%2").arg(type).arg(index);
        sWidth =  QString("%1Width%2").arg(type).arg(index);
        sHeight = QString("%1Height%2").arg(type).arg(index);
    }

    QString res = GetSetting(sRes);

    if ("" != res)
    {
        QStringList slist = QStringList::split("x", res);
        int w = width, h = height;
        if (2 == slist.size())
        {
            w = slist[0].toInt(&ok0);
            h = slist[1].toInt(&ok1);
        }
        bool ok = ok0 && ok1;
        if (ok)
        {
            width = w;
            height = h;
            refresh_rate = GetNumSetting(sRR);
            forced_aspect = GetFloatSetting(sAspect);
        }
    }
    else

    if (!ok)
    {
        int tmpWidth = GetNumSetting(sWidth, width);
        if (tmpWidth)
            width = tmpWidth;

        int tmpHeight = GetNumSetting(sHeight, height);
        if (tmpHeight)
            height = tmpHeight;

        refresh_rate = 0;
        forced_aspect = 0.0;
        //SetSetting(sRes, QString("%1x%2").arg(width).arg(height));
    }
}

void MythContext::GetResolutionSetting(const QString &t, int &w, int &h, int i)
{
    double forced_aspect = 0;
    short refresh_rate = 0;
    GetResolutionSetting(t, w, h, forced_aspect, refresh_rate, i);
}

// Parse an X11 style command line geometry string, like
//  -geometry 800x600
// or
//  -geometry 800x600+112+22
// and override the fullscreen and user default screen dimensions

bool MythContext::ParseGeometryOverride(const QString geometry)
{
    QRegExp     sre("^(\\d+)x(\\d+)$");
    QRegExp     lre("^(\\d+)x(\\d+)([+-]\\d+)([+-]\\d+)$");
    QStringList geo;
    bool        longForm = false;

    if (sre.exactMatch(geometry))
    {
        sre.search(geometry);
        geo = sre.capturedTexts();
    }
    else if (lre.exactMatch(geometry))
    {
        lre.search(geometry);
        geo = lre.capturedTexts();
        longForm = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Geometry does not match either form -");
        VERBOSE(VB_IMPORTANT, "WIDTHxHEIGHT or WIDTHxHEIGHT+XOFF+YOFF");
        return false;
    }

    bool parsed;

    d->m_geometry_w = geo[1].toInt(&parsed);
    if (!parsed)
    {
        VERBOSE(VB_IMPORTANT, "Could not parse width of geometry override");
        return false;
    }

    d->m_geometry_h = geo[2].toInt(&parsed);
    if (!parsed)
    {
        VERBOSE(VB_IMPORTANT, "Could not parse height of geometry override");
        return false;
    }

    d->m_geometry_x = geo[3].toInt(&parsed);
    if (longForm && !parsed)
    {
        VERBOSE(VB_IMPORTANT,
                "Could not parse horizontal offset of geometry override");
        return false;
    }

    d->m_geometry_y = geo[4].toInt(&parsed);
    if (longForm && !parsed)
    {
        VERBOSE(VB_IMPORTANT,
                "Could not parse vertical offset of geometry override");
        return false;
    }

    VERBOSE(VB_IMPORTANT, QString("Overriding GUI, width=%1,"
                                  " height=%2 at %3,%4")
                          .arg(d->m_geometry_w).arg(d->m_geometry_h)
                          .arg(d->m_geometry_x).arg(d->m_geometry_y));
    return true;
}

QString MythContext::FindThemeDir(const QString &themename)
{
    QString testdir = MythContext::GetConfDir() + "/themes/" + themename;

    QDir dir(testdir);
    if (dir.exists())
        return testdir;

    testdir = d->m_installprefix + "/share/mythtv/themes/" + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = "../menutest/" + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;


    // Don't complain about the "default" theme being missing
    if (themename == QObject::tr("Default"))
    {
        testdir = d->m_installprefix + "/share/mythtv/";
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;
    }
    
    cerr << "Could not find theme: " << themename << endl;
    return "";
}
QString MythContext::GetMenuThemeDir(void)
{
    return d->m_menuthemepathname;
}

QString MythContext::GetThemeDir(void)
{
    return d->m_themepathname;
}

MDBManager *MythContext::GetDBManager(void)
{
    return &d->m_dbmanager;
}

void MythContext::DBError(const QString &where, const QSqlQuery& query) 
{
    //if (query.lastError().type())
    {
        cerr << "DB Error (" << where << "):" << endl;
    }

#if QT_VERSION >= 0x030200
    cerr << "Query was:" << endl
         << query.executedQuery() << endl
         << DBErrorMessage(query.lastError()) << endl;
#else
    cerr << "Your version of Qt is too old to provide proper debugging\n";
    cerr << "Query was:" << endl
         << query.lastQuery() << endl
         << DBErrorMessage(query.lastError()) << endl;
#endif
}

QString MythContext::DBErrorMessage(const QSqlError& err)
{
    if (!err.type())
        return "No error type from QSqlError?  Strange...";

    return QString("Driver error was [%1/%2]:\n"
                   "%3\n"
                   "Database error was:\n"
                   "%4\n")
        .arg(err.type())
        .arg(err.number())
        .arg(err.driverText())
        .arg(err.databaseText());
}

Settings *MythContext::settings(void)
{
    return d->m_settings;
}

Settings *MythContext::qtconfig(void)
{
    return d->m_qtThemeSettings;
}

void MythContext::SaveSetting(const QString &key, int newValue)
{
    QString strValue = QString("%1").arg(newValue);

    SaveSetting(key, strValue);
}

void MythContext::SaveSetting(const QString &key, const QString &newValue)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("DELETE FROM settings WHERE value = :KEY "
                      "AND hostname = :HOSTNAME ;");
        query.bindValue(":KEY", key);
        query.bindValue(":HOSTNAME", d->m_localhostname);
        
        if (!query.exec() || !query.isActive())
                MythContext::DBError("Clear setting", query);
    
        query.prepare("INSERT settings ( value, data, hostname ) "
                      "VALUES ( :VALUE, :DATA, :HOSTNAME );");
        query.bindValue(":VALUE", key);
        query.bindValue(":DATA", newValue);
        query.bindValue(":HOSTNAME", d->m_localhostname);
        
        if (!query.exec() || !query.isActive())
                MythContext::DBError("Save new setting", query);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, 
             QString("Database not open while trying to save setting: %1")
                                .arg(key));
    }

}

QString MythContext::GetSetting(const QString &key, const QString &defaultval) 
{
    bool found = false;
    QString value;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        QString querystr = QString("SELECT data FROM settings WHERE value "
                                   " = '%1' AND hostname = '%2';")
                                  .arg(key).arg(d->m_localhostname);
        query.exec(querystr);

        if (query.isActive() && query.size() > 0)
        {
            query.next();
            value = QString::fromUtf8(query.value(0).toString());
            found = true;
        }
        else
        {
            querystr = QString("SELECT data FROM settings WHERE value = '%1' AND "
                               "hostname IS NULL;").arg(key);
            query.exec(querystr);

            if (query.isActive() && query.size() > 0)
            {
                query.next();
                value = QString::fromUtf8(query.value(0).toString());
                found = true;
            }
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, 
             QString("Database not open while trying to load setting: %1")
                                .arg(key));
    }

    if (found)
        return value;
    return d->m_settings->GetSetting(key, defaultval); 
}

int MythContext::GetNumSetting(const QString &key, int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSetting(key, val);

    return retval.toInt();
}

double MythContext::GetFloatSetting(const QString &key, double defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSetting(key, val);

    return retval.toDouble();
}

QString MythContext::GetSettingOnHost(const QString &key, const QString &host,
                                      const QString &defaultval)
{
    bool found = false;
    QString value = defaultval;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT data FROM settings WHERE value = :VALUE "
                      "AND hostname = :HOSTNAME ;");
        query.bindValue(":VALUE", key);
        query.bindValue(":HOSTNAME", host);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            value = query.value(0).toString();
            found = true;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, 
             QString("Database not open while trying to load setting: %1")
                                .arg(key));
    }

    return value;
}

int MythContext::GetNumSettingOnHost(const QString &key, const QString &host,
                                     int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSettingOnHost(key, host, val);

    return retval.toInt();
}

void MythContext::SetPalette(QWidget *widget)
{
    QPalette pal = widget->palette();

    QColorGroup active = pal.active();
    QColorGroup disabled = pal.disabled();
    QColorGroup inactive = pal.inactive();

    const QString names[] = { "Foreground", "Button", "Light", "Midlight",  
                              "Dark", "Mid", "Text", "BrightText", "ButtonText",
                              "Base", "Background", "Shadow", "Highlight",
                              "HighlightedText" };

    QString type = "Active";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Active, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    type = "Disabled";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Disabled, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    type = "Inactive";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Inactive, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    widget->setPalette(pal);
}

void MythContext::ThemeWidget(QWidget *widget)
{
    if (d->m_themeloaded)
    {
        widget->setPalette(d->m_palette);
        if (d->m_backgroundimage && d->m_backgroundimage->width() > 0)
        {
            widget->setPaletteBackgroundPixmap(*(d->m_backgroundimage));
        }
        return;
    }

    SetPalette(widget);
    d->m_palette = widget->palette();

    QPixmap *bgpixmap = NULL;

    if (d->m_qtThemeSettings->GetSetting("BackgroundPixmap") != "")
    {
        QString pmapname = d->m_themepathname +
                           d->m_qtThemeSettings->GetSetting("BackgroundPixmap");
 
        bgpixmap = LoadScalePixmap(pmapname);
        if (bgpixmap)
        {
            widget->setBackgroundOrigin(QWidget::AncestorOrigin);
            widget->setPaletteBackgroundPixmap(*bgpixmap);
            d->m_backgroundimage = new QPixmap(*bgpixmap);
        }
    }
    else if (d->m_qtThemeSettings->GetSetting("TiledBackgroundPixmap") != "")
    {
        QString pmapname = d->m_themepathname +
                      d->m_qtThemeSettings->GetSetting("TiledBackgroundPixmap");

        int width, height;
        float wmult, hmult;

        GetScreenSettings(width, wmult, height, hmult);

        bgpixmap = LoadScalePixmap(pmapname);
        if (bgpixmap)
        {
            QPixmap background(width, height);
            QPainter tmp(&background);

            tmp.drawTiledPixmap(0, 0, width, height, *bgpixmap);
            tmp.end();

            d->m_backgroundimage = new QPixmap(background);
            widget->setBackgroundOrigin(QWidget::AncestorOrigin);
            widget->setPaletteBackgroundPixmap(background);
        }
    }

    d->m_themeloaded = true;

    if (bgpixmap)
        delete bgpixmap;
}

QImage *MythContext::LoadScaleImage(QString filename, bool fromcache)
{
    if (filename.left(5) == "myth:")
        return NULL;
    QString baseDir = d->m_installprefix + "/share/mythtv/themes/default/";

    if (d->themecachedir != "" && fromcache)
    {
        QString cachefilepath;
        bool bFound = false;

        // Is absolute path in theme directory.
        if (!bFound)
        {
            if (!strcmp(filename.left(d->m_themepathname.length()),d->m_themepathname))
            {
                QString tmpfilename = filename;
                tmpfilename.remove(0, d->m_themepathname.length());
                cachefilepath = d->themecachedir + tmpfilename;
                QFile cachecheck(cachefilepath);
                if (cachecheck.exists())
                    bFound = true;
            }
        }

        // Is relative path in theme directory.
        if (!bFound)
        {
            cachefilepath = d->themecachedir + filename;
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        // Is in top level cache directory.
        if (!bFound)
        {
            QFileInfo fi(filename);
            cachefilepath = d->themecachedir + fi.fileName();
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        if (bFound)
        {
            QImage *ret = new QImage(cachefilepath);
            if (ret)
                return ret;
        }
    }

    QFile checkFile(filename);
    if (!checkFile.exists())
    {
        QString fileTemp;
        fileTemp = d->m_themepathname + filename;
        checkFile.setName(fileTemp);
        if (!checkFile.exists())
        {
            QFileInfo fi(filename);
            filename = d->m_themepathname + fi.fileName();
            checkFile.setName(filename);
            if (!checkFile.exists())
                filename = baseDir + fi.fileName();
        }
        else
        {
            filename = fileTemp;
        }
    }

    QImage *ret = NULL;

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != d->m_baseWidth || height != d->m_baseHeight)
    {
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult));
        ret = new QImage(tmp2);
    }
    else
    {
        ret = new QImage(filename);
        if (ret->width() == 0)
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
    }

    return ret;
}

QPixmap *MythContext::LoadScalePixmap(QString filename, bool fromcache) 
{
    if (filename.left(5) == "myth:")
        return NULL;
    QString baseDir = d->m_installprefix + "/share/mythtv/themes/default/";

    if (d->themecachedir != "" && fromcache)
    {
        QString cachefilepath;
        bool bFound = false;

        // Is absolute path in theme directory.
        if (!bFound)
        {
            if (!strcmp(filename.left(d->m_themepathname.length()),d->m_themepathname))
            {
                QString tmpfilename = filename;
                tmpfilename.remove(0, d->m_themepathname.length());
                cachefilepath = d->themecachedir + tmpfilename;
                QFile cachecheck(cachefilepath);
                if (cachecheck.exists())
                    bFound = true;
            }
        }

        // Is relative path in theme directory.
        if (!bFound)
        {
            cachefilepath = d->themecachedir + filename;
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        // Is in top level cache directory.
        if (!bFound)
        {
            QFileInfo fi(filename);
            cachefilepath = d->themecachedir + fi.fileName();
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        if (bFound)
        {
            QPixmap *ret = new QPixmap(cachefilepath);
            if (ret)
                return ret;
        }
    }

    QFile checkFile(filename);
    if (!checkFile.exists())
    {
        QString fileTemp;
        fileTemp = d->m_themepathname + filename;
        checkFile.setName(fileTemp);
        if (!checkFile.exists())
        {
            QFileInfo fi(filename);
            filename = d->m_themepathname + fi.fileName();
            checkFile.setName(filename);
            if (!checkFile.exists())
                filename = baseDir + fi.fileName();
        }
        else
        {
            filename = fileTemp;
        }

    }
        
    QPixmap *ret = new QPixmap();

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != d->m_baseWidth || height != d->m_baseHeight)
    {
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult));
        ret->convertFromImage(tmp2);
    }       
    else
    {
        if (!ret->load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
    }

    return ret;
}

QImage *MythContext::CacheRemotePixmap(const QString &url)
{
    QUrl qurl = url;
    if (qurl.host() == "")
        return NULL;

    if (d->imageCache.contains(url))
    {
        return &(d->imageCache[url]);
    }

    RemoteFile *rf = new RemoteFile(url);

    QByteArray data;
    bool ret = rf->SaveAs(data);

    delete rf;

    if (ret)
    {
        QImage image(data);
        if (image.width() > 0)
        {
            d->imageCache[url] = image;
            return &(d->imageCache[url]);
        }
    }
    
    return NULL;
}

void MythContext::SetSetting(const QString &key, const QString &newValue)
{
    d->m_settings->SetSetting(key, newValue);
}

bool MythContext::SendReceiveStringList(QStringList &strlist, bool quickTimeout)
{
    d->serverSockLock.lock();
    
    if (!d->serverSock)
        ConnectToMasterServer();

    bool ok = false;
    
    if (d->serverSock)
    {
        WriteStringList(d->serverSock, strlist);
        ok = ReadStringList(d->serverSock, strlist, quickTimeout);

        // this should be obsolete...
        while (ok && strlist[0] == "BACKEND_MESSAGE")
        {
            // oops, not for us
            cerr << "SRSL you shouldn't see this!!";
            QString message = strlist[1];
            QString extra = strlist[2];

            MythEvent me(message, extra);
            dispatch(me);

            ok = ReadStringList(d->serverSock, strlist, quickTimeout);
        }
        // .

        if (!ok)
        {
            qApp->lock();
            VERBOSE(VB_ALL, QString("Connection to backend server lost"));
            MythPopupBox::showOkPopup(d->mainWindow, "connection failure",
                             tr("The connection to the master backend "
                                "server has gone away for some reason.. "
                                "Is it running?"));

            delete d->serverSock;
            d->serverSock = NULL;
            qApp->unlock();
        }
    }    

    d->serverSockLock.unlock();
    
    return ok;
}

void MythContext::EventSocketRead(void)
{
    while (d->eventSock->state() == QSocket::Connected &&
           d->eventSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!ReadStringList(d->eventSock, strlist))
            continue;

        QString prefix = strlist[0];
        QString message = strlist[1];
        
        if (prefix == "OK")
        {
        }
        else if (prefix != "BACKEND_MESSAGE")
        {
            cerr << "Received a: " << prefix << " message from the backend\n";
            cerr << "But I don't know what to do with it.\n";
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

bool MythContext::CheckProtoVersion(QSocketDevice* socket)
{
    QStringList strlist = QString("MYTH_PROTO_VERSION %1")
                                 .arg(MYTH_PROTO_VERSION);
    WriteStringList(socket, strlist);
    ReadStringList(socket, strlist, true);
        
    if (strlist[0] == "REJECT")
    {
        VERBOSE(VB_GENERAL, QString("Protocol version mismatch (frontend=%1,"
                                    "backend=%2)\n")
                                    .arg(MYTH_PROTO_VERSION).arg(strlist[1]));

        if (d->m_height && d->m_width)
        {
            qApp->lock();
            MythPopupBox::showOkPopup(d->mainWindow, 
                                      "Connection failure",
                                      tr(QString("The server uses network "
                                                 "protocol version %1, "
                                                 "but this client only "
                                                 "understands version %2.  "
                                                 "Make sure you are running "
                                                 "compatible versions of "
                                                 "the backend and frontend."))
                                                 .arg(strlist[1])
                                                 .arg(MYTH_PROTO_VERSION));
            qApp->unlock();
        }
        return false;
    }
    else if (strlist[0] == "ACCEPT")
    {
        VERBOSE(VB_ALL, QString("Using protocol version %1")
                               .arg(MYTH_PROTO_VERSION));
        return true;
    }

    VERBOSE(VB_GENERAL, QString("Unexpected response to MYTH_PROTO_VERSION: %1")
                               .arg(strlist[0]));
    return false;
}

bool MythContext::CheckProtoVersion(QSocket* socket) 
{
    return CheckProtoVersion(socket->socketDevice());
}

void MythContext::EventSocketConnected(void)
{
/* Unnecessary, just checked on the serverSock connect */
#if 0
    if (!CheckProtoVersion(d->eventSock))
        return;
#endif

    QString str = QString("ANN Playback %1 %2")
                         .arg(d->m_localhostname).arg(true);
    QStringList strlist = str;
    WriteStringList(d->eventSock, strlist);
//    ReadStringList(d->eventSock, strlist);
}

void MythContext::EventSocketClosed(void)
{
            VERBOSE(VB_IMPORTANT, QString("Event socket closed. "
                    "No connection to the backend."));
}

void MythContext::addListener(QObject *obj)
{
    if (d->listeners.find(obj) == -1)
        d->listeners.append(obj);
}

void MythContext::removeListener(QObject *obj)
{
    if (d->listeners.find(obj) != -1)
        d->listeners.remove(obj);
}

void MythContext::dispatch(MythEvent &e)
{
    QObject *obj = d->listeners.first();
    while (obj)
    {
        QApplication::postEvent(obj, new MythEvent(e));
        obj = d->listeners.next();
    }
}

void MythContext::dispatchNow(MythEvent &e)
{
    QObject *obj = d->listeners.first();
    while (obj)
    {
        QApplication::sendEvent(obj, &e);
        obj = d->listeners.next();
    }
}

QFont MythContext::GetBigFont(void)
{
    return QFont("Arial", (int)floor(d->bigfontsize * d->m_hmult), QFont::Bold);
}

QFont MythContext::GetMediumFont(void)
{
    return QFont("Arial", (int)floor(d->mediumfontsize * d->m_hmult), 
                 QFont::Bold);
}

QFont MythContext::GetSmallFont(void)
{
    return QFont("Arial", (int)floor(d->smallfontsize * d->m_hmult), 
                 QFont::Bold);
}

QString MythContext::GetLanguage(void)
{
    if (d->language == QString::null || d->language == "")
        d->language = GetSetting("Language", "EN").lower();

    return d->language;
}

void MythContext::SetMainWindow(MythMainWindow *mainwin)
{
    d->mainWindow = mainwin;
}

MythMainWindow *MythContext::GetMainWindow(void)
{
    return d->mainWindow;
}

bool MythContext::TestPopupVersion(const QString &name, 
                                   const QString &libversion,
                                   const QString &pluginversion)
{
    if (libversion == pluginversion)
        return true;

    QString err = "The " + name + " plugin was compiled against libmyth " +
                  "version: " + pluginversion + ", but the installed " +
                  "libmyth is at version: " + libversion + ".  You probably " +
                  "want to recompile the " + name + " plugin after doing a " +
                  "make distclean.";

    if (GetMainWindow() && !d->disablelibrarypopup)
    {    
        DialogBox dbox(gContext->GetMainWindow(), err);
        dbox.AddButton("OK");
        dbox.exec();
    }

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


void MythContext::DisableScreensaver(void)
{
    QApplication::postEvent(GetMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetDisable));
}

void MythContext::RestoreScreensaver(void)
{
    QApplication::postEvent(GetMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetRestore));
}

void MythContext::ResetScreensaver(void)
{
    QApplication::postEvent(GetMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetReset));
}

void MythContext::DoDisableScreensaver(void)
{
    d->screensaver->Disable();
    d->screensaverEnabled = false;
}

void MythContext::DoRestoreScreensaver(void)
{
    d->screensaver->Restore();
    d->screensaverEnabled = true;
}

void MythContext::DoResetScreensaver(void)
{
    d->screensaver->Reset();
    d->screensaverEnabled = false;
}

bool MythContext::GetScreensaverEnabled(void)
{
    return d->screensaverEnabled;
}


void MythContext::LogEntry(const QString &module, int priority,
                           const QString &message, const QString &details)
{
    unsigned int logid;
    int howmany;

    if (d->m_logenable == -1) // Haven't grabbed the settings yet
        d->LoadLogSettings();
    if (d->m_logenable == 1)
    {
        if (message.left(21) != "Last message repeated")
        {
            if (message == d->lastLogStrings[module])
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
                d->lastLogStrings[module] = message;
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
        query.bindValue(":DETAILS", details.utf8());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("LogEntry", query);

        if (d->m_logmaxcount > 0)
        {
            query.prepare("SELECT logid FROM mythlog WHERE "
                          "module= :MODULE ORDER BY logdate ASC ;"); 
            query.bindValue(":MODULE", module);
            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError("DelLogEntry#1", query);
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
                            MythContext::DBError("DelLogEntry#2", delquery);
                        }
                        howmany--;
                    }
                }
            }
        }

        if (priority <= d->m_logprintlevel)
            VERBOSE(VB_ALL, module + ": " + message);
    }
}

void MythContext::addPrivRequest(MythPrivRequest::Type t, void *data)
{
    QMutexLocker lockit(d->m_priv_mutex);
    d->m_priv_requests.push(MythPrivRequest(t, data));
    d->m_priv_queued.wakeAll();
}

void MythContext::waitPrivRequest() const
{
    while (true) 
    {
        d->m_priv_queued.wait();
        if (!d->m_priv_requests.empty())
            return;
    }
}

MythPrivRequest MythContext::popPrivRequest()
{
    QMutexLocker lockit(d->m_priv_mutex);
    MythPrivRequest ret_val(MythPrivRequest::PrivEnd, NULL);
    if (!d->m_priv_requests.empty()) {
        ret_val = d->m_priv_requests.front();
        d->m_priv_requests.pop();
    }
    return ret_val;
}

DatabaseParams MythContext::GetDatabaseParams(void)
{
    DatabaseParams params;
    
    params.dbHostName = d->m_settings->GetSetting("DBHostName", "localhost");
    params.dbUserName = d->m_settings->GetSetting("DBUserName", "mythtv");
    params.dbPassword = d->m_settings->GetSetting("DBPassword", "mythtv");
    params.dbName     = d->m_settings->GetSetting("DBName",     "mythconverg");
    params.dbType     = d->m_settings->GetSetting("DBType",     "QMYSQL3");
    
    params.localHostName = d->m_settings->GetSetting("LocalHostName", "");
    if (params.localHostName.isEmpty())
    {
        params.localEnabled = false;
        params.localHostName = "my-unique-identifier-goes-here";
    }
    else
        params.localEnabled = true;
    
    params.wolReconnect = d->m_settings->GetNumSetting("WOLsqlReconnectWaitTime", -1);
    if (params.wolReconnect == -1)
    {
        params.wolEnabled = false;
        params.wolReconnect = 0;
    }
    else
        params.wolEnabled = true;

    params.wolRetry = d->m_settings->GetNumSetting("WOLsqlConnectRetry", 5);
    params.wolCommand = d->m_settings->GetSetting("WOLsqlCommand", "echo 'WOLsqlServerCommand not set'");
    
    return params;
}

bool MythContext::SaveDatabaseParams(const DatabaseParams &params)
{
    bool ret = true;
    DatabaseParams cur_params = GetDatabaseParams();
    
    // only rewrite file if it has changed
    if (params.dbHostName   != cur_params.dbHostName          ||
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
            ret = d->LoadDatabaseSettings(true);
    }
    return ret;
}
