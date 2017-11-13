#include <iostream>
#include <cstdlib>

#include <QDir>
#include <QCoreApplication>

#include "mythconfig.h"  // for CONFIG_DARWIN
#include "mythdirs.h"
#include "mythlogging.h"

#ifdef Q_OS_ANDROID
#include <QStandardPaths>
#include <sys/statfs.h>
#endif

static QString installprefix = QString::null;
static QString appbindir = QString::null;
static QString sharedir = QString::null;
static QString libdir = QString::null;
static QString confdir = QString::null;
static QString themedir = QString::null;
static QString pluginsdir = QString::null;
static QString translationsdir = QString::null;
static QString filtersdir = QString::null;
static QString cachedir = QString::null;
static QString remotecachedir = QString::null;
static QString themebasecachedir = QString::null;
static QString thumbnaildir = QString::null;

void InitializeMythDirs(void)
{
    installprefix = qgetenv( "MYTHTVDIR"   );
    confdir       = qgetenv( "MYTHCONFDIR" );

    if (confdir.length())
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Read conf dir = %1").arg(confdir));
        confdir.replace("$HOME", QDir::homePath());
    }

#ifdef _WIN32

    if (installprefix.length() == 0)
        installprefix = QDir( qApp->applicationDirPath() )
                            .absolutePath();

    appbindir = installprefix + "/";
    libdir    = appbindir;

    // Turn into Canonical Path for consistent compares

    QDir sDir(qgetenv("ProgramData") + "\\mythtv\\");
    sharedir = sDir.canonicalPath() + "/";

    if (confdir.length() == 0)
        confdir  = qgetenv( "LOCALAPPDATA" ) + "\\mythtv";

  #if 0
    // The following code only works for Qt 5.0 and above, but it may
    // be a generic solution for both windows & linux.
    // Re-evalute use oce min Qt == 5.0

    // QStringList sorted by least public to most.
    // Assume first is private user data and last is shared program data.

    qApp->setOrganizationName( "mythtv" );

    QStringList lstPaths = QStandardPaths::standardLocations(
                                        QStandardPaths::DataLocation);

    // Remove AppName from end of path

    if (lstPaths.length() > 0)
    {
        QString sAppName = qApp->applicationName();

        sharedir = lstPaths.last();

        if (sharedir.endsWith( sAppName ))
            sharedir = sharedir.left( sharedir.length() - sAppName.length());

        // Only use if user didn't override with env variable.
        if (confdir.length() == 0)
        {
            confdir = lstPaths.first();

            if (confdir.endsWith( sAppName ))
                confdir = confdir.left( confdir.length() - sAppName.length());
        }
    }

  #endif

    if (sharedir.length() == 0)
        sharedir = confdir;

#elif defined(Q_OS_ANDROID)
    if (installprefix.isEmpty())
        installprefix = QDir( qApp->applicationDirPath() )
                            .absolutePath();
    QString extdir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Mythtv";
    if (!QDir(extdir).exists())
        QDir(extdir).mkdir(".");

    if (confdir.isEmpty())
        confdir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

#if 0
    // TODO allow choice of base fs or the SD card for data
    QStringList appLocs = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    uint64_t maxFreeSpace = 0;
    for(auto s : appLocs)
    {
        struct statfs statFs;
        memset(&statFs, 0, sizeof(statFs));
        int ret = statfs(s.toLocal8Bit().data(), &statFs);
        if (ret == 0 && statFs.f_bavail >= maxFreeSpace)
        {
            maxFreeSpace = statFs.f_bavail;
            confdir = s;
        }
        LOG(VB_GENERAL, LOG_NOTICE, QString(" appdatadir      = %1 (%2, %3, %4)")
            .arg(s)
            .arg(statFs.f_bavail)
            .arg(statFs.f_bfree)
            .arg(statFs.f_bsize));
    }
    QStringList cacheLocs = QStandardPaths::standardLocations(QStandardPaths::CacheLocation);
    maxFreeSpace = 0;
    for(auto s : cacheLocs)
    {
        struct statfs statFs;
        memset(&statFs, 0, sizeof(statFs));
        int ret = statfs(s.toLocal8Bit().data(), &statFs);
        if (ret == 0 && statFs.f_bavail >= maxFreeSpace)
        {
            maxFreeSpace = statFs.f_bavail;
            //confdir = s;
        }
        LOG(VB_GENERAL, LOG_NOTICE, QString(" cachedir      = %1 (%2, %3, %4)")
                                            .arg(s)
                                            .arg(statFs.f_bavail)
                                            .arg(statFs.f_bfree)
                                            .arg(statFs.f_bsize));
    }
#endif

    appbindir = installprefix + "/";
    sharedir  = "assets:/mythtv/";
    libdir    = installprefix + "/";


#else

    if (installprefix.length() == 0)
        installprefix = QString(RUNPREFIX);

    QDir prefixDir = qApp->applicationDirPath();

    if (QDir(installprefix).isRelative())
    {
        // If the PREFIX is relative, evaluate it relative to our
        // executable directory. This can be fragile on Unix, so
        // use relative PREFIX values with care.

        LOG(VB_GENERAL, LOG_DEBUG, QString("Relative PREFIX! (%1), appDir=%2")
            .arg(installprefix) .arg(prefixDir.canonicalPath()));

        if (!prefixDir.cd(installprefix))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Relative PREFIX does not resolve, using %1")
                .arg(prefixDir.canonicalPath()));
        }
        installprefix = prefixDir.canonicalPath();
    }

    appbindir = installprefix + "/bin/";
    sharedir  = installprefix + "/share/mythtv/";
    libdir    = installprefix + '/' + QString(LIBDIRNAME) + "/mythtv/";

#endif

    if (confdir.length() == 0)
        confdir = QDir::homePath() + "/.mythtv";
    cachedir = confdir + "/cache";
    remotecachedir = cachedir + "/remotecache";
    themebasecachedir = cachedir + "/themecache";
    thumbnaildir = cachedir + "/thumbnails";

#if defined(Q_OS_ANDROID)
    themedir        = sharedir + "themes/";
    pluginsdir      = libdir;
    translationsdir = sharedir + "i18n/";
    filtersdir      = libdir;
#else
    themedir        = sharedir + "themes/";
    pluginsdir      = libdir   + "plugins/";
    translationsdir = sharedir + "i18n/";
    filtersdir      = libdir   + "filters/";
#endif

    LOG(VB_GENERAL, LOG_NOTICE, "Using runtime prefix = " + installprefix);
    LOG(VB_GENERAL, LOG_NOTICE, QString("Using configuration directory = %1")
                                   .arg(confdir));

    LOG(VB_GENERAL, LOG_DEBUG, QString( "appbindir         = "+ appbindir        ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "sharedir          = "+ sharedir         ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "libdir            = "+ libdir           ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "themedir          = "+ themedir         ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "pluginsdir        = "+ pluginsdir       ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "translationsdir   = "+ translationsdir  ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "filtersdir        = "+ filtersdir       ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "cachedir          = "+ cachedir         ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "remotecachedir    = "+ remotecachedir   ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "themebasecachedir = "+ themebasecachedir));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "thumbnaildir      = "+ thumbnaildir));
}

QString GetInstallPrefix(void) { return installprefix; }
QString GetAppBinDir(void) { return appbindir; }
QString GetShareDir(void) { return sharedir; }
QString GetLibraryDir(void) { return libdir; }
QString GetConfDir(void) { return confdir; }
QString GetThemesParentDir(void) { return themedir; }
QString GetPluginsDir(void) { return pluginsdir; }
QString GetTranslationsDir(void) { return translationsdir; }
QString GetFiltersDir(void) { return filtersdir; }

/**
 * Returns the base directory for all cached files.  On linux this
 * will default to ~/.mythtv/cache.
 */
QString GetCacheDir(void) { return cachedir; }

/**
 * Returns the directory for all files cached from the backend.  On
 * linux this will default to ~/.mythtv/cache/remotecache.  Items in
 * this directory will be expired after a certain amount of time.
 */
QString GetRemoteCacheDir(void) { return remotecachedir; }

/**
 * Returns the directory where all non-theme thumbnail files should be
 * cached.  On linux this will default to ~/.mythtv/cache/thumbnails.
 * Items in this directory will be expired after a certain amount of
 * time.
 */
QString GetThumbnailDir(void) { return thumbnaildir; }

/**
 * Returns the base directory where all theme related files should be
 * cached.  On linux this will default to ~/.mythtv/cache/themecache.
 * Within this directory, a sub-directory will be created for each
 * theme used.
 */
QString GetThemeBaseCacheDir(void) { return themebasecachedir; }

// These defines provide portability for different
// plugin file names.
#if CONFIG_DARWIN
static const QString kPluginLibPrefix = "lib";
static const QString kPluginLibSuffix = ".dylib";
static const QString kFilterLibPrefix = "lib";
static const QString kFilterLibSuffix = ".dylib";
#elif _WIN32
static const QString kPluginLibPrefix = "lib";
static const QString kPluginLibSuffix = ".dll";
static const QString kFilterLibPrefix = "lib";
static const QString kFilterLibSuffix = ".dll";
#elif ANDROID
static const QString kPluginLibPrefix = "libmythplugin";
static const QString kPluginLibSuffix = ".so";
static const QString kFilterLibPrefix = "libmythfilter";
static const QString kFilterLibSuffix = ".so";
#else
static const QString kPluginLibPrefix = "lib";
static const QString kPluginLibSuffix = ".so";
static const QString kFilterLibPrefix = "lib";
static const QString kFilterLibSuffix = ".so";
#endif

QString GetFiltersNameFilter(void)
{
    return kFilterLibPrefix + '*' + kFilterLibSuffix;
}

QString GetPluginsNameFilter(void)
{
    return kPluginLibPrefix + '*' + kPluginLibSuffix;
}

QString FindPluginName(const QString &plugname)
{
    return GetPluginsDir() + kPluginLibPrefix + plugname + kPluginLibSuffix;
}

QString GetTranslationsNameFilter(void)
{
    return "mythfrontend_*.qm";
}

QString FindTranslation(const QString &translation)
{
    return GetTranslationsDir()
           + "mythfrontend_" + translation.toLower() + ".qm";
}

QString GetFontsDir(void)
{
    return GetShareDir() + "fonts/";
}
