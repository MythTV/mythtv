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
#include <qregexp.h>

#include <qhostaddress.h>

#include <cmath>
#include <queue>

#include "config.h"
#include "mythcontext.h"
#include "exitcodes.h"
#include "oldsettings.h"
#include "util.h"
#include "remotefile.h"
#include "dialogbox.h"
#include "mythdialogs.h"
#include "mythplugin.h"
#include "screensaver.h"
#include "DisplayRes.h"
#include "backendselect.h"
#include "dbsettings.h"
#include "langsettings.h"
#include "mythdbcon.h"
#include "util-x11.h"
#include "mythsocket.h"
#include "themeinfo.h"

#include "libmythui/mythmainwindow.h"
#include "libmythupnp/mythxmlclient.h"
#include "libmythupnp/upnp.h"

#ifdef USING_MINGW
#include <winsock2.h>
#undef DialogBox
#include "compat.h"
#endif

// These defines provide portability for different
// plugin file names.
#ifdef CONFIG_DARWIN
const QString kPluginLibPrefix = "lib";
const QString kPluginLibSuffix = ".dylib";
#elif USING_MINGW
const QString kPluginLibPrefix = "lib";
const QString kPluginLibSuffix = ".dll";
#else
const QString kPluginLibPrefix = "lib";
const QString kPluginLibSuffix = ".so";
#endif

MythContext *gContext = NULL;
QMutex MythContext::verbose_mutex(true);
QString MythContext::x11_display = QString::null;

unsigned int print_verbose_messages = VB_IMPORTANT | VB_GENERAL;
QString verboseString = QString(" important general");

QMutex avcodeclock(true);

// Some common UPnP search and XML value strings
const QString gBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";
const QString kDefaultBE  = "UPnP/MythFrontend/DefaultBackend/";
const QString kDefaultPIN = kDefaultBE + "SecurityPin";
const QString kDefaultUSN = kDefaultBE + "USN";


int parse_verbose_arg(QString arg)
{
    QString option;
    bool reverseOption;

    if (arg.startsWith("-"))
    {
        cerr << "Invalid or missing argument to -v/--verbose option\n";
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    else
    {
        QStringList verboseOpts = QStringList::split(',', arg);
        for (QStringList::Iterator it = verboseOpts.begin();
             it != verboseOpts.end(); ++it )
        {
            option = *it;
            reverseOption = false;

            if (option != "none" && option.left(2) == "no")
            {
                reverseOption = true;
                option = option.right(option.length() - 2);
            }

            if (!strcmp(option,"help"))
            {
                QString m_verbose = verboseString;
                m_verbose.replace(QRegExp(" "), ",");
                m_verbose.replace(QRegExp("^,"), "");
                cerr <<
                  "Verbose debug levels.\n" <<
                  "Accepts any combination (separated by comma) of:\n\n" <<

#define VERBOSE_ARG_HELP(ARG_ENUM, ARG_VALUE, ARG_STR, ARG_ADDITIVE, ARG_HELP) \
                QString("  %1").arg(ARG_STR).leftJustify(15, ' ', true) << \
                " - " << ARG_HELP << "\n" <<

                  VERBOSE_MAP(VERBOSE_ARG_HELP)

                  "\n" <<
                  "The default for this program appears to be: '-v " <<
                  m_verbose << "'\n\n" <<
                  "Most options are additive except for none, all, and important.\n" <<
                  "These three are semi-exclusive and take precedence over any\n" <<
                  "prior options given.  You can however use something like\n" <<
                  "'-v none,jobqueue' to get only JobQueue related messages\n" <<
                  "and override the default verbosity level.\n" <<
                  "\n" <<
                  "The additive options may also be subtracted from 'all' by \n" <<
                  "prefixing them with 'no', so you may use '-v all,nodatabase'\n" <<
                  "to view all but database debug messages.\n" <<
                  "\n" <<
                  "Some debug levels may not apply to this program.\n" <<
                  endl;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

#define VERBOSE_ARG_CHECKS(ARG_ENUM, ARG_VALUE, ARG_STR, ARG_ADDITIVE, ARG_HELP) \
            else if (option == ARG_STR) \
            { \
                if (reverseOption) \
                { \
                    print_verbose_messages &= ~(ARG_VALUE); \
                    verboseString = verboseString + " no" + ARG_STR; \
                } \
                else \
                { \
                    if (ARG_ADDITIVE) \
                    { \
                        print_verbose_messages |= ARG_VALUE; \
                        verboseString = verboseString + " " + ARG_STR; \
                    } \
                    else \
                    { \
                        print_verbose_messages = ARG_VALUE; \
                        verboseString = ARG_STR; \
                    } \
                } \
            }

            VERBOSE_MAP(VERBOSE_ARG_CHECKS)

            else
            {
                cerr << "Unknown argument for -v/--verbose: "
                     << option << endl;;
            }
        }
    }

    return GENERIC_EXIT_OK;
}

// Verbose helper function for ENO macro
QString safe_eno_to_string(int errnum)
{
    return QString("%1 (%2)").arg(strerror(errnum)).arg(errnum);
}


class MythContextPrivate
{
  public:
    MythContextPrivate(MythContext *lparent);
   ~MythContextPrivate();

    bool Init        (const bool gui,    UPnp *UPnPclient,
                      const bool prompt, const bool noPrompt);
    bool FindDatabase(const bool prompt, const bool noPrompt);

    bool IsWideMode() const {return (m_baseWidth == 1280);}
    void SetWideMode() {m_baseWidth = 1280; m_baseHeight = 720;}
    bool IsSquareMode() const {return (m_baseWidth == 800);}
    void SetSquareMode() {m_baseWidth = 800; m_baseHeight = 600;}

    void TempMainWindow(bool languagePrompt = true);
    void EndTempWindow(void);

    void GetScreenBounds(void);
    void StoreGUIsettings(void);

    void LoadLogSettings(void);
    void LoadDatabaseSettings(void);

    bool LoadSettingsFile(void);
    bool WriteSettingsFile(const DatabaseParams &params,
                           bool overwrite = false);
    bool FindSettingsProbs(void);

    QString getResponse(const QString &query, const QString &def);
    int     intResponse(const QString &query, int def);
    bool    PromptForDatabaseParams(const QString &error);
    QString TestDBconnection(void);
    void    ResetDatabase(void);

    bool    InitUPnP(void);
    void    DeleteUPnP(void);
    int     ChooseBackend(const QString &error);
    int     UPnPautoconf(const int milliSeconds = 2000);
    void    StoreConnectionInfo(void);
    bool    DefaultUPnP(QString &error);
    bool    UPnPconnect(const DeviceLocation *device, const QString &PIN);


    MythContext *parent;

    Settings *m_settings;          ///< connection stuff, theme, button style
    Settings *m_qtThemeSettings;   ///< everything else theme-related

    QString   m_installprefix;     ///< Compile-time RUNPREFIX, or generated
                                   ///< from enviroment ($MYTHTVDIR or $cwd)
    QString   m_libname;           ///< Compile-time LIBDIRNAME

    bool      m_gui;               ///< Should this context use GUI elements?
    bool      m_backend;           ///< Is this host any sort of backend?

    bool      m_themeloaded;       ///< To we have a palette and pixmap to use?
    QString   m_menuthemepathname;
    QString   m_themepathname;
    QPixmap  *m_backgroundimage;   ///< Large or tiled image
    QPalette  m_palette;           ///< Colour scheme


    // Drawable area of the full screen. May cover several screens,
    // or exclude windowing system fixtures (like Mac menu bar)
    int m_xbase, m_ybase;
    int m_height, m_width;

    // Dimensions of the theme
    int m_baseWidth, m_baseHeight;


    QMutex  m_hostnamelock;      ///< Locking for thread-safe copying of:
    QString m_localhostname;     ///< hostname from mysql.txt or gethostname()

    DatabaseParams  m_DBparams;  ///< Current database host & WOL details
    QString         m_DBhostCp;  ///< dbHostName backup

    UPnp             *m_UPnP;    ///< For automatic backend discover
    XmlConfiguration *m_XML;
    HttpServer       *m_HTTP;

    QMutex serverSockLock;
    bool attemptingToConnect;

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

    MythSocket *serverSock;
    MythSocket *eventSock;

    bool disablelibrarypopup;

    MythPluginManager *pluginmanager;

    ScreenSaverControl *screensaver;

    int m_logenable, m_logmaxcount, m_logprintlevel;
    QMap<QString,int> lastLogCounts;
    QMap<QString,QString> lastLogStrings;

    bool screensaverEnabled;

    DisplayRes *display_res;

    QMutex m_priv_mutex;
    queue<MythPrivRequest> m_priv_requests;
    QWaitCondition m_priv_queued;

    bool useSettingsCache;
    QMutex settingsCacheLock;
    QMap <QString, QString> settingsCache;      // permanent settings in the DB
    QMap <QString, QString> overriddenSettings; // overridden this session only
};


MythContextPrivate::MythContextPrivate(MythContext *lparent)
    : parent(lparent),
      m_settings(new Settings()), m_qtThemeSettings(new Settings()),
      m_installprefix(RUNPREFIX), m_libname(LIBDIRNAME),
      m_gui(false), m_backend(false), m_themeloaded(false),
      m_menuthemepathname(QString::null), m_themepathname(QString::null),
      m_backgroundimage(NULL),
      m_xbase(0), m_ybase(0), m_height(0), m_width(0),
      m_baseWidth(800), m_baseHeight(600),
      m_localhostname(QString::null),
      m_UPnP(NULL), m_XML(NULL), m_HTTP(NULL),
      serverSockLock(false),
      attemptingToConnect(false),
      language(""),
      mainWindow(NULL),
      m_wmult(1.0), m_hmult(1.0),
      m_screenxbase(0), m_screenybase(0), m_screenwidth(0), m_screenheight(0),
      m_geometry_x(0), m_geometry_y(0), m_geometry_w(0), m_geometry_h(0),
      themecachedir(QString::null),
      bigfontsize(0), mediumfontsize(0), smallfontsize(0),
      serverSock(NULL), eventSock(NULL),
      disablelibrarypopup(false),
      pluginmanager(NULL),
      screensaver(NULL),
      m_logenable(-1), m_logmaxcount(-1), m_logprintlevel(-1),
      screensaverEnabled(false),
      display_res(NULL),
      useSettingsCache(false)
{
    char *tmp_installprefix = getenv("MYTHTVDIR");
    if (tmp_installprefix)
        m_installprefix = tmp_installprefix;

#if QT_VERSION >= 0x030200
    QDir prefixDir = qApp->applicationDirPath();
#else
    QString appPath = QFileInfo(qApp->argv()[0]).absFilePath();
    QDir prefixDir(appPath.left(appPath.findRev("/")));
#endif

    if (QDir(m_installprefix).isRelative())
    {
        // If the PREFIX is relative, evaluate it relative to our
        // executable directory. This can be fragile on Unix, so
        // use relative PREFIX values with care.

        VERBOSE(VB_IMPORTANT,
                "Relative PREFIX! (" + m_installprefix +
                ")\n\t\tappDir=" + prefixDir.canonicalPath());
        prefixDir.cd(m_installprefix);
        m_installprefix = prefixDir.canonicalPath();
    }

    VERBOSE(VB_IMPORTANT, "Using runtime prefix = " + m_installprefix);
}

MythContextPrivate::~MythContextPrivate()
{
    imageCache.clear();

    DeleteUPnP();
    if (m_settings)
        delete m_settings;
    if (m_qtThemeSettings)
        delete m_qtThemeSettings;
    if (serverSock)
        serverSock->DownRef();
    if (eventSock)
        eventSock->DownRef();
    if (screensaver)
        delete screensaver;
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
 * \bug   The default theme here is blue, but as of [12377],
 *        G.A.N.T is now meant to be the default?
 *        A simple change to make, but Nigel prefers
 *        "blue" for these initial screens and popups
 */
void MythContextPrivate::TempMainWindow(bool languagePrompt)
{
    if (mainWindow)
        return;

    // We clear the hostname so MSqlQuery will fail, instead of long
    // timeouts per DB value, or hundreds of lines of DB connect errors.
    // We save the value for later possible editing in the DbSettings pages
    if (m_DBparams.dbHostName.length())
    {
        m_DBhostCp = m_DBparams.dbHostName;
        m_DBparams.dbHostName = "";
    }

    m_settings->SetSetting("Theme", "blue");
#ifdef Q_WS_MACX
    // Myth looks horrible in default Mac style for Qt
    m_settings->SetSetting("Style", "Windows");
#endif
    parent->LoadQtConfig();

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
}

/**
 * \brief Get screen size from Qt, respecting for user's multiple screen prefs
 *
 * If the windowing system environment has multiple screens
 * (%e.g. Xinerama or Mac OS X), QApplication::desktop()->%width() will span
 * all of them, so we usually need to get the geometry of a specific screen.
 */
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
    bool             hasXinerama = GetNumberOfXineramaScreens() > 1;
    int              numScreens  = desktop->numScreens();
    int              screen;

    if (hasXinerama)
        VERBOSE(VB_GENERAL,
                QString("Total desktop dim: %1x%2, over %3 screen[s].")
                .arg(desktop->width()).arg(desktop->height()).arg(numScreens));
    if (numScreens > 1)
        for (screen = 0; screen < numScreens; ++screen)
        {
            QRect dim = desktop->screenGeometry(screen);
            VERBOSE(VB_GENERAL,
                    QString("Screen %1 dim: %2x%3.")
                    .arg(screen).arg(dim.width()).arg(dim.height()));
        }

    screen = desktop->primaryScreen();
    VERBOSE(VB_GENERAL, QString("Primary screen %1.").arg(screen));

    if (hasXinerama)
        screen = parent->GetNumSetting("XineramaScreen", screen);

    if (screen == -1)       // Special case - span all screens
    {
        VERBOSE(VB_GENERAL, QString("Using all %1 screens.")
                                     .arg(numScreens));
        m_xbase  = 0;
        m_ybase  = 0;
        m_width  = desktop->width();
        m_height = desktop->height();

        VERBOSE(VB_GENERAL, QString("Total width = %1, height = %2")
                            .arg(m_width).arg(m_height));
        return;
    }

    if (hasXinerama)        // User specified a single screen
    {
        if (screen < 0 || screen >= numScreens)
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Xinerama screen %1 was specified,"
                        " but only %2 available, so using screen 0.")
                    .arg(screen).arg(numScreens));
            screen = 0;
        }
    }


    {
        QRect bounds;

        bool inWindow = parent->GetNumSetting("RunFrontendInWindow", 0);

        if (inWindow)
            VERBOSE(VB_IMPORTANT, QString("Running in a window"));

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

bool MythContextPrivate::Init(const bool gui, UPnp *UPnPclient,
                              const bool promptForBackend, const bool noPrompt)
{
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
        screensaver = ScreenSaverControl::get();

    // ---- database connection stuff ----

    if (!FindDatabase(promptForBackend, noPrompt))
        return false;

    // ---- keep all DB-using stuff below this line ----

    if (gui)
    {
        GetScreenBounds();
        StoreGUIsettings();
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
    while (failure.length())
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
    ResetDatabase();
    DeleteUPnP();
    return true;

NoDBfound:
    //VERBOSE(VB_GENERAL, "FindDatabase() - failed");
    DeleteUPnP();
    return false;
}

/**
 * Apply any user overrides to the screen geometry
 */
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
        VERBOSE(VB_IMPORTANT, "Somehow, your screen size settings are bad.");
        VERBOSE(VB_IMPORTANT, QString("GuiResolution: %1")
                        .arg(parent->GetSetting("GuiResolution")));
        VERBOSE(VB_IMPORTANT, QString("  old GuiWidth: %1")
                        .arg(parent->GetNumSetting("GuiWidth")));
        VERBOSE(VB_IMPORTANT, QString("  old GuiHeight: %1")
                        .arg(parent->GetNumSetting("GuiHeight")));
        VERBOSE(VB_IMPORTANT, QString("m_width: %1").arg(m_width));
        VERBOSE(VB_IMPORTANT, QString("m_height: %1").arg(m_height));
        VERBOSE(VB_IMPORTANT, "Falling back to 640x480");

        m_screenwidth  = 640;
        m_screenheight = 480;
    }

    m_wmult = m_screenwidth  / (float)m_baseWidth;
    m_hmult = m_screenheight / (float)m_baseHeight;

    QFont font = QFont("Arial");
    if (!font.exactMatch())
        font = QFont();
    font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    font.setPointSize((int)floor(14 * m_hmult));

    QApplication::setFont(font);

    //VERBOSE(VB_IMPORTANT, QString("GUI multipliers are: width %1, height %2").arg(m_wmult).arg(m_hmult));
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
}

/**
 * Load mysql.txt and parse its values into m_DBparams
 */
bool MythContextPrivate::LoadSettingsFile(void)
{
    if (!m_settings->LoadSettingsFiles("mysql.txt", m_installprefix))
        return false;

    m_DBparams.dbHostName = m_settings->GetSetting("DBHostName");
    m_DBparams.dbHostPing = m_settings->GetSetting("DBHostPing") != "no";
    m_DBparams.dbPort     = m_settings->GetNumSetting("DBPort");
    m_DBparams.dbUserName = m_settings->GetSetting("DBUserName");
    m_DBparams.dbPassword = m_settings->GetSetting("DBPassword");
    m_DBparams.dbName     = m_settings->GetSetting("DBName");
    m_DBparams.dbType     = m_settings->GetSetting("DBType");

    m_DBparams.localHostName = m_settings->GetSetting("LocalHostName");
    m_DBparams.localEnabled  = m_DBparams.localHostName.length() > 0;

    m_DBparams.wolReconnect
        = m_settings->GetNumSetting("WOLsqlReconnectWaitTime");
    m_DBparams.wolEnabled = m_DBparams.wolReconnect > 0;

    m_DBparams.wolRetry   = m_settings->GetNumSetting("WOLsqlConnectRetry");
    m_DBparams.wolCommand = m_settings->GetSetting("WOLsqlCommand");

    return true;
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
      << "# will need to reconfigure mythtv (or futz with the DB) every time.\n"
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

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
    {
        cout << endl << "[console is not interactive, using default '"
             << def  << "']" << endl;
        return def;
    }

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
        QString response;

        // give user chance to skip config
        cout << endl << error << endl << endl;
        response = getResponse("Would you like to configure the database "
                               "connection now?",
                               "yes");
        if (!response || response.left(1).lower() != "y")
            return false;

        params.dbHostName = getResponse("Database host name:",
                                        params.dbHostName);
        response = getResponse("Should I test connectivity to this host "
                               "using the ping command?", "yes");
        params.dbHostPing = (!response || response.left(1).lower() != "y");

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
        if (response)
            params.wolEnabled  = (response.left(1).lower() == "y");

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
 */
QString MythContextPrivate::TestDBconnection(void)
{
    bool    doPing = m_DBparams.dbHostPing;
    QString err;
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
            system(m_DBparams.wolCommand);

            VERBOSE(VB_GENERAL,
                    QString("Waiting for %1 seconds").arg(wakeupTime));
            sleep(m_DBparams.wolReconnect);
        }

    if (doPing)
    {
        VERBOSE(VB_GENERAL,
                QString("Testing network connectivity to %1").arg(host));
    }

    if (doPing && !ping(host, 3))  // Fail after trying for 3 seconds
    {
        // Save, to display in DatabaseSettings screens
        m_DBhostCp = m_DBparams.dbHostName;

        // Cause MSqlQuery to fail, instead of minutes timeout per DB value
        m_DBparams.dbHostName = "";

        err = QObject::tr("Cannot find (ping) database host %1 on the network");
        return err.arg(host);
    }


    // 2. Check that the supplied DBport is listening:

    if (port && !telnet(host, port))
    {
        err = QObject::tr("Cannot connect to port %1 on database host %2");
        return err.arg(port).arg(host);
    }


    // 3. Finally, try to login, et c:

    if (!MSqlQuery::testDBConnection())
        return QObject::tr(QString("Cannot login to database?"));


    return QString::null;
}

/**
 * Called when the user changes the DB connection settings
 *
 * The current DB connections way be invalid (<I>e.g.</I> wrong password),
 * or the user may have changed to a different database host. Either way,
 * any current connections need to be closed so that the new connection
 * can be attempted.
 *
 * Any cached settings also need to be cleared,
 * so that they can be re-read from the new database
 */
void MythContextPrivate::ResetDatabase(void)
{
    m_dbmanager.CloseDatabases();
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
    m_HTTP = new HttpServer(port);

    if (!m_HTTP->ok())
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
    QString           LOC = "UPnPautoconf() - ";
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
        VERBOSE(VB_GENERAL, LOC + "No UPnP backends found");
        return 0;
    }


    // This could be tied to VB_UPNP?
    //m_UPnP->g_SSDPCache.Dump();


    count = backends->Count();
    switch (count)
    {
        case 0:
            VERBOSE(VB_IMPORTANT,
                    LOC + "No UPnP backends found, but SSDP::Find() not NULL!");
            break;
        case 1:
            VERBOSE(VB_GENERAL, LOC + "Found one UPnP backend");
            break;
        default:
            VERBOSE(VB_GENERAL,
                    (LOC + "More than one UPnP backend found (%1)").arg(count));
    }

    if (count != 1)
    {
        backends->Release();
        return count;
    }


    // Get this backend's location:
    backends->Lock();
    DeviceLocation *BE = backends->GetEntryMap()->begin().data();
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
    QString        LOC = "UPnPconnect() - ";
    QString        URL = backend->m_sLocation;
    MythXMLClient  XML(URL);

    VERBOSE(VB_UPNP, LOC + QString("Trying host at %1").arg(URL));
    switch (XML.GetConnectionInfo(PIN, &m_DBparams, error))
    {
        case UPnPResult_Success:
            break;

        case UPnPResult_ActionNotAuthorized:
            // The stored PIN is probably not correct.
            // We could prompt for the PIN and try again, but that needs a UI.
            // Easier to fail for now, and put up the full UI selector later
            VERBOSE(VB_UPNP, LOC + error + ". Wrong PIN?");
            return false;

        default:
            VERBOSE(VB_UPNP, LOC + error);
            return false;
    }

    QString DBhost = m_DBparams.dbHostName;
    VERBOSE(VB_UPNP, LOC + QString("Got database hostname: %1").arg(DBhost));

    return true;
}


MythContext::MythContext(const QString &binversion)
    : QObject(), d(NULL), app_binary_version(binversion)
{
#ifdef USING_MINGW
    static bool WSAStarted = false;
    if (!WSAStarted) {
        WSADATA wsadata;
        int res = WSAStartup(MAKEWORD(2, 0), &wsadata);
        VERBOSE(VB_SOCKET, QString("WSAStartup returned %1").arg(res));
    }
#endif

    qInitNetworkProtocols();

    d = new MythContextPrivate(this);
}

bool MythContext::Init(const bool gui, UPnp *UPnPclient,
                       const bool promptForBackend,
                       const bool disableAutoDiscovery)
{
    if (app_binary_version != MYTH_BINARY_VERSION)
    {
        QString warning =
                QString("This app was compiled against libmyth version: %1"
                "\n\t\t\tbut the library is version: %2"
                "\n\t\t\tYou probably want to recompile everything, and do a"
                "\n\t\t\t'make distclean' first.")
                .arg(app_binary_version).arg(MYTH_BINARY_VERSION);

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

        _putenv(QString("HOME=%1").arg(home));
    }
#endif

    if (QDir::homeDirPath() == "/" && ! getenv("MYTHCONFDIR"))
    {
        QString warning = "Cannot locate your home directory."
                          " Please set the environment variable HOME";
        if (gui)
        {
            d->TempMainWindow(false);
            MythPopupBox::showOkPopup(d->mainWindow, "HOME error", warning);
        }
        VERBOSE(VB_IMPORTANT, warning + " or MYTHCONFDIR");

        return false;
    }

    if (!d->Init(gui, UPnPclient, promptForBackend, disableAutoDiscovery))
        return false;

    ActivateSettingsCache(true);

    return true;
}

MythContext::~MythContext()
{
    if (d)
        delete d;
}

bool MythContext::ConnectToMasterServer(bool blockingClient)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);

    if (!d->eventSock)
        d->eventSock = new MythSocket();

    if (!d->serverSock)
        d->serverSock = ConnectServer(d->eventSock, server,
                                      port, blockingClient);

    if (d->serverSock)
        d->eventSock->setCallbacks(this);

    return (bool) (d->serverSock);
}

MythSocket *MythContext::ConnectServer(MythSocket *eventSock,
                                       const QString &hostname,
                                       int port,
                                       bool blockingClient)
{
    MythSocket *serverSock = NULL;
    int cnt = 1;

    int sleepTime = GetNumSetting("WOLbackendReconnectWaitTime", 0);
    int maxConnTry = GetNumSetting("WOLbackendConnectRetry", 1);

    do
    {
        VERBOSE(VB_GENERAL, QString("Connecting to backend server: "
                                    "%1:%2 (try %3 of %4)")
                                    .arg(hostname).arg(port).arg(cnt)
                                    .arg(maxConnTry));

        serverSock = new MythSocket();

        if (!serverSock->connect(hostname, port))
        {
            serverSock->DownRef();
            serverSock = NULL;

            if (d->attemptingToConnect)
                break;
            d->attemptingToConnect = true;

            // only inform the user of a failure if WOL is disabled
            if (sleepTime <= 0)
            {
                VERBOSE(
                    VB_IMPORTANT, "Connection timed out.          \n\t\t\t"
                    "You probably should modify the Master Server \n\t\t\t"
                    "settings in the setup program and set the    \n\t\t\t"
                    "proper IP address.");
                if (d->m_gui && d->m_height && d->m_width)
                {
                    bool manageLock = false;
                    if (!blockingClient && d->serverSockLock.locked())
                    {
                        manageLock = true;
                        d->serverSockLock.unlock();
                    }
                    MythPopupBox::showOkPopup(d->mainWindow,
                                              "connection failure",
                                              tr("Could not connect to the "
                                                 "master backend server -- is "
                                                 "it running?  Is the IP "
                                                 "address set for it in the "
                                                 "setup program correct?"));
                    if (manageLock)
                        d->serverSockLock.lock();
                }

                d->attemptingToConnect = false;
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
                myth_system(wol_cmd);

                VERBOSE(VB_GENERAL, QString("Waiting for %1 seconds until I "
                                            "try to reconnect again.")
                                            .arg(sleepTime));
                sleep(sleepTime);
                ++cnt;
            }
            d->attemptingToConnect = false;
        }
        else
            break;
    }
    while (cnt <= maxConnTry);

#ifndef IGNORE_PROTO_VER_MISMATCH
    if (serverSock && !CheckProtoVersion(serverSock))
    {
        serverSock->DownRef();
        serverSock = NULL;
        return false;
    }
#endif

    if (serverSock)
    {
        // called with the lock
        QString str = QString("ANN %1 %2 %3")
            .arg(blockingClient ? "Playback" : "Monitor")
            .arg(d->m_localhostname).arg(false);
        QStringList strlist = str;
        serverSock->writeStringList(strlist);
        serverSock->readStringList(strlist, true);

        if (eventSock && eventSock->state() == MythSocket::Idle)
        {
            // Assume that since we _just_ connected the one socket, this one
            // will work, too.
            eventSock->connect(hostname, port);

            eventSock->Lock();

            QString str = QString("ANN Monitor %1 %2")
                                 .arg(d->m_localhostname).arg(true);
            QStringList strlist = str;
            eventSock->writeStringList(strlist);
            eventSock->readStringList(strlist);

            eventSock->Unlock();
        }
    }
    return serverSock;
}

bool MythContext::IsConnectedToMaster(void)
{
    return d->serverSock;
}

void MythContext::BlockShutdown(void)
{
    QStringList strlist;

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
#if defined(CONFIG_DARWIN) || (__FreeBSD__) || defined(__OpenBSD__)
    char *command = "ps -ax | grep -i mythbackend | grep -v grep > /dev/null";
#else
    char *command = "ps -ae | grep mythbackend > /dev/null";
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

    QStringList strlist = "QUERY_IS_ACTIVE_BACKEND";
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

void MythContext::ClearSettingsCache(QString myKey, QString newVal)
{
    if (!d)
        return;

    d->settingsCacheLock.lock();
    if (myKey != "" && d->settingsCache.contains(myKey))
    {
        VERBOSE(VB_DATABASE, QString("Clearing Settings Cache for '%1'.")
                                    .arg(myKey));
        d->settingsCache.remove(myKey);
        d->settingsCache[myKey] = newVal;
    }
    else
    {
        VERBOSE(VB_DATABASE, "Clearing Settings Cache.");
        d->settingsCache.clear();
    }
    d->settingsCacheLock.unlock();
}

void MythContext::ActivateSettingsCache(bool activate)
{
    if (!d)
        return;

    if (activate)
        VERBOSE(VB_DATABASE, "Enabling Settings Cache.");
    else
        VERBOSE(VB_DATABASE, "Disabling Settings Cache.");

    d->useSettingsCache = activate;
    ClearSettingsCache();
}

QString MythContext::GetHostName(void)
{
    // The reference counting in QString isn't thread-safe, so we need
    // take care of it ourselves.
    d->m_hostnamelock.lock();
    QString tmp = QDeepCopy<QString>(d->m_localhostname);
    d->m_hostnamelock.unlock();
    return tmp;
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
        //VERBOSE(VB_IMPORTANT, QString("Read conf dir = %1").arg(dir));
        dir.replace("$HOME", QDir::homeDirPath());
    }
    else
        dir = QDir::homeDirPath() + "/.mythtv";

    //VERBOSE(VB_IMPORTANT, QString("Using conf dir = %1").arg(dir));
    return dir;
}

QString MythContext::GetShareDir(void)
{
    return d->m_installprefix + "/share/mythtv/";
}

QString MythContext::GetLibraryDir(void)
{
    return d->m_installprefix + "/" + d->m_libname + "/mythtv/";
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
    return GetTranslationsDir()
           + "mythfrontend_" + translation.lower() + ".qm";
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
    }

    // Note the possibly changed screen settings
    d->GetScreenBounds();


    if (d->m_qtThemeSettings)
        delete d->m_qtThemeSettings;

    d->m_qtThemeSettings = new Settings;

    QString style = GetSetting("Style", "");
    if (style != "")
        qApp->setStyle(style);

    QString themename = GetSetting("Theme");
    QString themedir = FindThemeDir(themename);

    ThemeInfo *themeinfo = new ThemeInfo(themedir);

    if (themeinfo && themeinfo->IsWide())
    {
        VERBOSE(VB_IMPORTANT,
                QString("Switching to wide mode (%1)").arg(themename));
        d->SetWideMode();
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Switching to square mode (%1)").arg(themename));
        d->SetSquareMode();
    }

    if (themeinfo)
        delete themeinfo;

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
    d->m_menuthemepathname = FindMenuThemeDir(themename) +"/";

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
    QMap<QDateTime, QString> dirtimes;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir() && !fi->isSymLink())
        {
            if (fi->absFilePath() == themecachedir)
                continue;
            dirtimes[fi->lastModified()] = fi->absFilePath();
        }
    }

    const size_t max_cached = GetNumSetting("ThemeCacheSize", 1);
    while (dirtimes.size() >= max_cached)
    {
        VERBOSE(VB_FILE, QString("Removing cache dir: %1")
                .arg(dirtimes.begin().data()));
        RemoveCacheDir(dirtimes.begin().data());
        dirtimes.erase(dirtimes.begin());
    }

    QMap<QDateTime, QString>::const_iterator dit = dirtimes.begin();
    for (; dit != dirtimes.end(); ++dit)
    {
        VERBOSE(VB_FILE, QString("Keeping cache dir: %1")
                .arg((*dit).data()));
    }
}

void MythContext::RemoveCacheDir(const QString &dirname)
{
    QString cachedirname = MythContext::GetConfDir() + "/themecache/";

    if (!dirname.startsWith(cachedirname))
        return;

    VERBOSE(VB_IMPORTANT,
            QString("Removing stale cache dir: %1").arg(dirname));

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
    if (d->m_screenwidth == d->m_baseWidth &&
            d->m_screenheight == d->m_baseHeight)
        return;

    CacheThemeImagesDirectory(d->m_themepathname);
    if (d->IsWideMode())
        CacheThemeImagesDirectory(GetThemesParentDir() + "default-wide/");
    CacheThemeImagesDirectory(GetThemesParentDir() + "default/");
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
                    VERBOSE(VB_IMPORTANT,
                            QString("Failed to save cached image: %1")
                            .arg(d->themecachedir + filename));
                }

                delete tmpimage;
            }
        }
    }

    if (caching)
    {
        caching->Close();
        caching->deleteLater();
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

/**
 * \brief Parse an X11 style command line geometry string
 *
 * Accepts strings like
 *  -geometry 800x600
 * or
 *  -geometry 800x600+112+22
 * to override the fullscreen and user default screen dimensions
 */
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

    if (!d)
    {
        VERBOSE(VB_IMPORTANT,
                "MythContextPrivate not initted, can't store geometry.");
        return false;
    }

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

    if (longForm)
    {
        d->m_geometry_x = geo[3].toInt(&parsed);
        if (!parsed)
        {
            VERBOSE(VB_IMPORTANT,
                    "Could not parse horizontal offset of geometry override");
            return false;
        }

        d->m_geometry_y = geo[4].toInt(&parsed);
        if (!parsed)
        {
            VERBOSE(VB_IMPORTANT,
                    "Could not parse vertical offset of geometry override");
            return false;
        }
    }

    VERBOSE(VB_IMPORTANT, QString("Overriding GUI, width=%1,"
                                  " height=%2 at %3,%4")
                          .arg(d->m_geometry_w).arg(d->m_geometry_h)
                          .arg(d->m_geometry_x).arg(d->m_geometry_y));
    return true;
}

/** \fn FindThemeDir(const QString &themename)
 *  \brief Returns the full path to the theme denoted by themename
 *
 *   If the theme cannot be found falls back to the G.A.N.T theme.
 *   If the G.A.N.T theme doesn't exist then returns an empty string.
 *  \param themename The theme name.
 *  \return Path to theme or empty string.
 */
QString MythContext::FindThemeDir(const QString &themename)
{
    QString testdir = MythContext::GetConfDir() + "/themes/" + themename;

    QDir dir(testdir);
    if (dir.exists())
        return testdir;
    else
        VERBOSE(VB_IMPORTANT, "No theme dir: " + dir.absPath());


    testdir = GetThemesParentDir() + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;
    else
        VERBOSE(VB_IMPORTANT, "No theme dir: " + dir.absPath());


    testdir = GetThemesParentDir() + "G.A.N.T";
    dir.setPath(testdir);
    if (dir.exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1 - "
                "Switching to G.A.N.T").arg(themename));
        SaveSetting("Theme", "G.A.N.T");
        return testdir;
    }
    else
        VERBOSE(VB_IMPORTANT, "No theme dir: " + dir.absPath());

    VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1").arg(themename));
    return "";
}

/** \fn FindMenuThemeDir(const QString &menuname)
 *  \brief Returns the full path to the menu theme denoted by menuname
 *
 *   If the theme cannot be found falls back to the default menu.
 *   If the default menu theme doesn't exist then returns an empty string.
 *  \param menuname The menutheme name.
 *  \return Path to theme or empty string.
 */
QString MythContext::FindMenuThemeDir(const QString &menuname)
{
    QString testdir;
    QDir dir;

    if (menuname == "default")
    {
        testdir = GetShareDir();
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;
    }

    testdir = MythContext::GetConfDir() + "/themes/" + menuname;

    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = GetThemesParentDir() + menuname;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = GetShareDir();
    dir.setPath(testdir);
    if (dir.exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1 - "
                "Switching to default").arg(menuname));
        SaveSetting("MenuTheme", "default");
        return testdir;
    }
    else {
        VERBOSE(VB_IMPORTANT, QString("Could not find menu theme: %1 - Fallback to default failed.").arg(menuname));
    }

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

QValueList<QString> MythContext::GetThemeSearchPath(void)
{
    QValueList<QString> searchpath;

    searchpath.append(GetThemeDir());
    if (d->IsWideMode())
        searchpath.append(GetThemesParentDir() + "default-wide/");
    searchpath.append(GetThemesParentDir() + "default/");
    searchpath.append("/tmp/");
    return searchpath;
}

MDBManager *MythContext::GetDBManager(void)
{
    return &d->m_dbmanager;
}

void MythContext::DBError(const QString &where, const QSqlQuery& query)
{
    QString str = QString("DB Error (%1):\n").arg(where);

#if QT_VERSION >= 0x030200
    str += "Query was:\n";
    str += query.executedQuery() + "\n";
    str += QString::fromUtf8(DBErrorMessage(query.lastError()));
#else
    str += "Your version of Qt is too old to provide proper debugging\n";
    str += "Query may have been:\n";
    str += QString::fromUtf8(query.lastQuery()) + "\n";
    str += DBErrorMessage(query.lastError());
#endif
    VERBOSE(VB_IMPORTANT, QString("%1").arg(str));
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

Settings *MythContext::qtconfig(void)
{
    return d->m_qtThemeSettings;
}

void MythContext::SaveSetting(const QString &key, int newValue)
{
    (void) SaveSettingOnHost(key,
                             QString::number(newValue), d->m_localhostname);
}

void MythContext::SaveSetting(const QString &key, const QString &newValue)
{
    (void) SaveSettingOnHost(key, newValue, d->m_localhostname);
}

bool MythContext::SaveSettingOnHost(const QString &key,
                                    const QString &newValue,
                                    const QString &host)
{
    bool success = false;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {

        if ((host) && (host != ""))
            query.prepare("DELETE FROM settings WHERE value = :KEY "
                          "AND hostname = :HOSTNAME ;");
        else
            query.prepare("DELETE FROM settings WHERE value = :KEY "
                          "AND hostname is NULL;");

        query.bindValue(":KEY", key);
        query.bindValue(":HOSTNAME", host);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Clear setting", query);

        if ((host) && (host != ""))
            query.prepare("INSERT INTO settings (value,data,hostname) "
                          "VALUES ( :VALUE, :DATA, :HOSTNAME );");
        else
            query.prepare("INSERT INTO settings (value,data,hostname ) "
                          "VALUES ( :VALUE, :DATA, NULL );");

        query.bindValue(":VALUE", key);
        query.bindValue(":DATA", newValue);
        query.bindValue(":HOSTNAME", host);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("SaveSettingOnHost query failure: ", query);
        else
            success = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
             QString("Database not open while trying to save setting: %1")
                                .arg(key));
    }

    ClearSettingsCache(key, newValue);
    ClearSettingsCache(host + " " + key, newValue);

    return success;
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval)
{
    bool found = false;
    QString value;

    if (d && d->overriddenSettings.contains(key)) {
        value = d->overriddenSettings[key];
        return value;
    }

    if (d && d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        if (d->settingsCache.contains(key))
        {
            value = d->settingsCache[key];
            d->settingsCacheLock.unlock();
            return value;
        }
        d->settingsCacheLock.unlock();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT data FROM settings WHERE value "
                      "= :KEY AND hostname = :HOSTNAME ;");
        query.bindValue(":KEY", key);
        query.bindValue(":HOSTNAME", d->m_localhostname);
        query.exec();

        if (query.isActive() && query.size() > 0)
        {
            query.next();
            value = QString::fromUtf8(query.value(0).toString());
            found = true;
        }
        else
        {
            query.prepare("SELECT data FROM settings WHERE value = :KEY AND "
                          "hostname IS NULL;");
            query.bindValue(":KEY", key);
            query.exec();

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

    if (!found)
        return d->m_settings->GetSetting(key, defaultval);

    // Store the value (only if we have actually found it in the database)
    if (!value.isNull() && d && d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        d->settingsCache[key] = value;
        d->settingsCacheLock.unlock();
    }

    return value;
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
    QString myKey = host + " " + key;

    if (d)
    {
        if (d->overriddenSettings.contains(myKey))
        {
            value = d->overriddenSettings[myKey];
            return value;
        }

        if ((host == d->m_localhostname) &&
            (d->overriddenSettings.contains(key)))
        {
            value = d->overriddenSettings[key];
            return value;
        }
    }

    if (d && d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        if (d->settingsCache.contains(myKey))
        {
            value = d->settingsCache[myKey];
            d->settingsCacheLock.unlock();
            return value;
        }
        d->settingsCacheLock.unlock();
    }

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

    if (found && d && d->useSettingsCache)
    {
        d->settingsCacheLock.lock();
        d->settingsCache[host + " " + key] = value;
        d->settingsCacheLock.unlock();
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

double MythContext::GetFloatSettingOnHost(
    const QString &key, const QString &host, double defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSettingOnHost(key, host, val);

    return retval.toDouble();
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

bool MythContext::FindThemeFile(QString &filename)
{
    // Given a full path, or in current working directory?
    if (QFile::exists(filename))
        return true;

    int pathStart = filename.findRev('/');
    QString basename;
    if (pathStart > 0)
        basename = filename.mid(pathStart + 1);

    QString file;
    QValueList<QString> searchpath = GetThemeSearchPath();
    for (QValueList<QString>::const_iterator ii = searchpath.begin();
        ii != searchpath.end(); ii++)
    {
        if (QFile::exists((file = *ii + filename)))
            goto found;
        if (pathStart > 0 && QFile::exists((file = *ii + basename)))
            goto found;
    }

    return false;

found:
    filename = file;
    return true;
}

QImage *MythContext::LoadScaleImage(QString filename, bool fromcache)
{
    if (filename.left(5) == "myth:")
        return NULL;

    if (d->themecachedir != "" && fromcache)
    {
        QString cachefilepath;
        bool bFound = false;

        // Is absolute path in theme directory.
        if (!bFound)
        {
            if (!strcmp(filename.left(d->m_themepathname.length()),
                        d->m_themepathname))
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


    if (!FindThemeFile(filename))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to find image file: %1").arg(filename));

        return NULL;
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
            VERBOSE(VB_IMPORTANT,
                    QString("Error loading image file: %1").arg(filename));

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
            VERBOSE(VB_IMPORTANT,
                    QString("Error loading image file: %1").arg(filename));

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

    if (d->themecachedir != "" && fromcache)
    {
        QString cachefilepath;
        bool bFound = false;

        // Is absolute path in theme directory.
        if (!bFound)
        {
            if (!strcmp(filename.left(d->m_themepathname.length()),
                        d->m_themepathname))
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

    if (!FindThemeFile(filename))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to find image file: %1").arg(filename));

        return NULL;
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
            VERBOSE(VB_IMPORTANT,
                    QString("Error loading image file: %1").arg(filename));

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
            VERBOSE(VB_IMPORTANT,
                    QString("Error loading image file: %1").arg(filename));

            delete ret;
            return NULL;
        }
    }

    return ret;
}

QImage *MythContext::CacheRemotePixmap(const QString &url, bool reCache)
{
    QUrl qurl = url;
    if (qurl.host() == "")
        return NULL;

    if ((d->imageCache.contains(url)) && (reCache == false))
        return &(d->imageCache[url]);

    RemoteFile *rf = new RemoteFile(url, false, 0);

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
    ClearSettingsCache(key, newValue);
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
    d->overriddenSettings[key] = value;
}

bool MythContext::SendReceiveStringList(QStringList &strlist,
                                        bool quickTimeout, bool block)
{
    d->serverSockLock.lock();

    if (!d->serverSock)
    {
        ConnectToMasterServer(false);
        // should clear popup if it is currently active here. Not sure of the correct way. TBD
    }

    bool ok = false;

    if (d->serverSock)
    {
        d->serverSock->writeStringList(strlist);
        ok = d->serverSock->readStringList(strlist, quickTimeout);

        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, QString("Connection to backend server lost"));
            d->serverSock->DownRef();
            d->serverSock = NULL;

            ConnectToMasterServer(false);

            if (d->serverSock)
            {
                d->serverSock->writeStringList(strlist);
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

            qApp->lock();
            if (!block)
                d->serverSockLock.unlock();
            VERBOSE(VB_IMPORTANT,
                    QString("Reconnection to backend server failed"));
            if (d->m_height && d->m_width)
                MythPopupBox::showOkPopup(d->mainWindow, "connection failure",
                             tr("The connection to the master backend "
                                "server has gone away for some reason.. "
                                "Is it running?"));

            if (!block)
                d->serverSockLock.lock();
            qApp->unlock();
        }
    }

    d->serverSockLock.unlock();

    return ok;
}

void MythContext::readyRead(MythSocket *sock)
{
    (void)sock;

    while (d->eventSock->state() == MythSocket::Connected &&
           d->eventSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!d->eventSock->readStringList(strlist))
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

bool MythContext::CheckProtoVersion(MythSocket* socket)
{
    QStringList strlist = QString("MYTH_PROTO_VERSION %1")
                                 .arg(MYTH_PROTO_VERSION);
    socket->writeStringList(strlist);
    socket->readStringList(strlist, true);

    if (strlist.empty())
    {
        VERBOSE(VB_IMPORTANT, "Protocol version check failure. The response "
                "to MYTH_PROTO_VERSION was empty.");

        return false;
    }
    else if (strlist[0] == "REJECT")
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

    d->serverSockLock.lock();
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

    d->serverSockLock.unlock();
}

QFont MythContext::GetBigFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(d->bigfontsize));
    font.setWeight(QFont::Bold);

    return font;
}

QFont MythContext::GetMediumFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(d->mediumfontsize));
    font.setWeight(QFont::Bold);

    return font;
}

QFont MythContext::GetSmallFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(d->smallfontsize));
    font.setWeight(QFont::Bold);

    return font;
}

/** \fn MythContext::GetLanguage()
 *  \brief Returns two character ISO-639 language descriptor for UI language.
 *  \sa iso639_get_language_list()
 */
QString MythContext::GetLanguage(void)
{
    return GetLanguageAndVariant().left(2);
}

/** \fn MythContext::GetLanguageAndVariant()
 *  \brief Returns the user-set language and variant.
 *
 *   The string has the form ll or ll_vv, where ll is the two character
 *   ISO-639 language code, and vv (which may not exist) is the variant.
 *   Examples include en_AU, en_CA, en_GB, en_US, fr_CH, fr_DE, pt_BR, pt_PT.
 */
QString MythContext::GetLanguageAndVariant(void)
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
    if (!d)
        return NULL;
    return d->mainWindow;
}

/**
 * \brief   Try to prevent silent, automatic database upgrades
 * \returns -1 to use the existing schema, 0 to exit, 1 to upgrade.
 *
 * The GUI prompts have no defaults, so that nothing dangerous
 * will happen if the user hits return a few times.
 * Hopefully this will force users to stop and think?
 * Similarly, the shell command prompting requires an explicit "yes".
 * Non-interactive shells default to old behaviour (upgrading)
 */
int MythContext::PromptForSchemaUpgrade(const QString &dbver,
                                        const QString &current,
                                        const QString &backupResult)
{
    bool    autoUpgrade = false;
    bool    connections = false;  // Are (other) FE/BEs connected?
    bool    expertMode  = false;  // Use existing schema? Multiple buttons?
    QString message;
    int     returnValue = MYTH_SCHEMA_UPGRADE;
    bool    upgradable  = (dbver.toUInt() < current.toUInt());
    QString warnOtherCl = tr("There are also other clients using this"
                             " database. They should be shut down first.");


    // No current DBSchemaVer? Empty database, so upgrade to create tables
    if (dbver.isEmpty() || dbver == "0")
    {
        VERBOSE(VB_GENERAL, "No current database version. Auto upgrading");
        return MYTH_SCHEMA_UPGRADE;
    }


    // Users and developers can choose to live dangerously,
    // either to silently and automatically upgrade,
    // or an expert option to allow use of existing:
    switch (gContext->GetNumSetting("DBSchemaAutoUpgrade"))
    {
        case  1: autoUpgrade = true; break;
        case -1: expertMode  = true; break;
        default: break;
    }


    // FIXME: Don't know how to determine this
    //if (getActiveConnections() > 1)
    //    connections = true;


    // Deal with the trivial case first (No user prompting required)
    if (autoUpgrade && upgradable && !connections)
        return MYTH_SCHEMA_UPGRADE;


    // Build up strings used both in GUI and command shell contexts:
    if (upgradable)
    {
        if (autoUpgrade && connections)
        {
            message = tr("Error: MythTV cannot upgrade the schema of this"
                         " datatase because other clients are using it.\n\n"
                         "Please shut them down before upgrading.");
            returnValue = MYTH_SCHEMA_ERROR;
        }
        else
        {
            message = tr("Warning: MythTV wants to upgrade"
                         " your database schema, from %1 to %2.");
            if (expertMode)
                message += "\n\n" +
                           tr("You can try using the old schema,"
                              " but that may cause problems.");
        }
    }
    else   // This client is too old
    {
        if (expertMode)
            message = tr("Warning: MythTV database has newer"
                         " schema (%1) than expected (%2).");
        else
        {
            message = tr("Error: MythTV database has newer"
                         " schema (%1) than expected (%2).");
            returnValue = MYTH_SCHEMA_ERROR;
        }
    }

    if (backupResult == "__FAILED__")
        message += "\n" + tr("MythTV was unable to backup your database.");

    if (message.contains("%1"))
        message = message.arg(dbver).arg(current);


    if (d->m_gui)
    {
        bool createdTempWindow = false;

        if (!d->mainWindow)
        {
            d->TempMainWindow();
            createdTempWindow = true;
        }

        if (returnValue == MYTH_SCHEMA_ERROR)
        {
            MythPopupBox::showOkPopup(
                d->mainWindow, "Database Upgrade Error",
                message, QObject::tr("Exit"));
        }
        else
        {
            QStringList buttonNames;

            buttonNames += QObject::tr("Exit");
            if (upgradable)
                buttonNames += QObject::tr("Upgrade");
            if (expertMode)
                buttonNames += QObject::tr("Use current schema");

            DialogCode selected = MythPopupBox::ShowButtonPopup(
                d->mainWindow, "Database Upgrade", message,
                buttonNames, kDialogCodeButton0);

            // The annoying extra confirmation:
            if (kDialogCodeButton1 == selected ||
                kDialogCodeButton2 == selected)
            {
                if ((backupResult != "__FAILED__") &&
                    (backupResult != ""))
                {
                    int dirPos = backupResult.findRev(QChar('/'));
                    QString dirName;
                    QString fileName;
                    if (dirPos > 0)
                    {
                        fileName = backupResult.mid(dirPos + 1);
                        dirName  = backupResult.left(dirPos);
                    }
                    message = tr("If your system becomes unstable, a database "
                                 "backup file called %1 is located in %2.")
                                 .arg(fileName).arg(dirName);
                }
                else
                    message = tr("This cannot be un-done, so having a"
                                 " database backup would be a good idea.");
                if (connections)
                    message += "\n\n" + warnOtherCl;

                selected = MythPopupBox::ShowButtonPopup(
                    d->mainWindow, "Database Upgrade", message,
                    buttonNames, kDialogCodeButton0);
            }

            switch (selected)
            {
                case kDialogCodeRejected:
                case kDialogCodeButton0:
                    returnValue = MYTH_SCHEMA_EXIT;         break;
                case kDialogCodeButton1:
                    returnValue = upgradable ?
                                  MYTH_SCHEMA_UPGRADE:
                                  MYTH_SCHEMA_USE_EXISTING; break;
                case kDialogCodeButton2:
                    returnValue = MYTH_SCHEMA_USE_EXISTING; break;
                default:
                    returnValue = MYTH_SCHEMA_ERROR;
            }
        }

        if (createdTempWindow)
        {
            d->EndTempWindow();
            d->m_DBparams.dbHostName = d->m_DBhostCp;
        }

        return returnValue;
    }

    // We are not in a GUI environment, so try to prompt the user in the shell

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
    {
        if (expertMode)
        {
            cout << "Console non-interactive. Using existing schema." << endl;
            return MYTH_SCHEMA_USE_EXISTING;
        }

        cout << "Console is not interactive, cannot ask user about"
             << " upgrading database schema." << endl << "Upgrading." << endl;
        return MYTH_SCHEMA_UPGRADE;
    }


    QString resp;

    cout << endl << message << endl << endl;

    if (backupResult == "__FAILED__")
        cout << "WARNING: MythTV was unable to backup your database."
             << endl << endl;
    else if (backupResult != "")
        cout << "If your system becomes unstable, a database backup is "
                "located in " << backupResult << endl << endl;

    if (expertMode)
    {
        resp = d->getResponse("Would you like to use the existing schema?",
                              "yes");
        if (!resp || resp.left(1).lower() == "y")
            return MYTH_SCHEMA_USE_EXISTING;
    }

    resp = d->getResponse("\nShall I upgrade this database?", "yes");
    if (resp && resp.left(1).lower() != "y")
        return MYTH_SCHEMA_EXIT;

    if (connections)
        cout << endl << warnOtherCl <<endl;

    if ((backupResult == "__FAILED__") ||
        (backupResult == ""))
    {
        resp = d->getResponse("\nA database backup might be a good idea"
                              "\nAre you sure you want to upgrade?", "no");
        if (!resp || resp.left(1).lower() == "n")
            return MYTH_SCHEMA_EXIT;
    }

    return MYTH_SCHEMA_UPGRADE;
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
        DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), err);
        dlg->AddButton("OK");
        dlg->exec();
        dlg->deleteLater();
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
    if (d && d->screensaver)
    {
        d->screensaver->Disable();
        d->screensaverEnabled = false;
    }
}

void MythContext::DoRestoreScreensaver(void)
{
    if (d && d->screensaver)
    {
        d->screensaver->Restore();
        d->screensaverEnabled = true;
    }
}

void MythContext::DoResetScreensaver(void)
{
    if (d && d->screensaver)
    {
        d->screensaver->Reset();
        d->screensaverEnabled = false;
    }
}

bool MythContext::GetScreensaverEnabled(void)
{
    if (!d)
        return false;
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
        {
            VERBOSE(VB_IMPORTANT,
                    QString("%1: %2").arg(module).arg(fullMsg));
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

            // If database has changed, force its use:
            d->ResetDatabase();
        }
    }
    return ret;
}

void MythContext::addCurrentLocation(QString location)
{
    QMutexLocker locker(&locationLock);
    if (currentLocation.last() != location)
        currentLocation.push_back(location);
}

QString MythContext::removeCurrentLocation(void)
{
    QMutexLocker locker(&locationLock);

    if (currentLocation.isEmpty())
        return QString("UNKNOWN");

    QString result = currentLocation.last();
    currentLocation.pop_back();
    return result;
}

QString MythContext::getCurrentLocation(void)
{
    QMutexLocker locker(&locationLock);

    if (currentLocation.isEmpty())
        return QString("UNKNOWN");

    return currentLocation.last();
}

bool MythContext::GetScreenIsAsleep(void)
{
    if (!d->screensaver)
        return false;
    return d->screensaver->Asleep();
}

/// This needs to be set before MythContext is initialized so
/// that the MythContext::Init() can detect Xinerama setups.
void MythContext::SetX11Display(const QString &display)
{
    x11_display = QDeepCopy<QString>(display);
}

QString MythContext::GetX11Display(void)
{
    return QDeepCopy<QString>(x11_display);
}

void MythContext::dispatch(MythEvent &event)
{
    if (print_verbose_messages & VB_NETWORK)
        VERBOSE(VB_NETWORK, QString("MythEvent: %1").arg(event.Message()));

    MythObservable::dispatch(event);
}

void MythContext::dispatchNow(MythEvent &event)
{
    if (print_verbose_messages & VB_NETWORK)
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
