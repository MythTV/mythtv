#include <iostream>
#include <cstdlib>

#include <QDir>
#include <QCoreApplication>

#include "mythconfig.h"  // for CONFIG_DARWIN
#include "mythdirs.h"
#include "mythlogging.h"

static QString installprefix = QString::null;
static QString appbindir = QString::null;
static QString sharedir = QString::null;
static QString libdir = QString::null;
static QString confdir = QString::null;
static QString themedir = QString::null;
static QString pluginsdir = QString::null;
static QString translationsdir = QString::null;
static QString filtersdir = QString::null;

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

    sharedir = qgetenv( "ProgramData"  ) + "\\mythtv\\";

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
#else

    if (installprefix.length() == 0)
        installprefix = QString(RUNPREFIX);

  #if CONFIG_DARWIN
    // Work around bug in OS X where applicationDirPath() can crash
    // (if binary is not in a bundle, and is daemon()ized)

    QDir prefixDir = QFileInfo(qApp->argv()[0]).dir();
  #else
    QDir prefixDir = qApp->applicationDirPath();
  #endif

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

    themedir        = sharedir + "themes/";
    pluginsdir      = libdir   + "plugins/";
    translationsdir = sharedir + "i18n/";
    filtersdir      = libdir   + "filters/";

    LOG(VB_GENERAL, LOG_NOTICE, "Using runtime prefix = " + installprefix);
    LOG(VB_GENERAL, LOG_NOTICE, QString("Using configuration directory = %1")
                                   .arg(confdir));

    LOG(VB_GENERAL, LOG_DEBUG, QString( "appbindir      = "+ appbindir      ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "sharedir       = "+ sharedir       ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "libdir         = "+ libdir         ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "themedir       = "+ themedir       ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "pluginsdir     = "+ pluginsdir     ));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "translationsdir= "+ translationsdir));
    LOG(VB_GENERAL, LOG_DEBUG, QString( "filtersdir     = "+ filtersdir     ));
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

// These defines provide portability for different
// plugin file names.
#if CONFIG_DARWIN
static const QString kPluginLibPrefix = "lib";
static const QString kPluginLibSuffix = ".dylib";
#elif _WIN32
static const QString kPluginLibPrefix = "lib";
static const QString kPluginLibSuffix = ".dll";
#else
static const QString kPluginLibPrefix = "lib";
static const QString kPluginLibSuffix = ".so";
#endif

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

