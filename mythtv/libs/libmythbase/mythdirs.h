#ifndef MYTHDIRS_H_
#define MYTHDIRS_H_

#include <QString>
#include "mythexp.h"

MPUBLIC void InitializeMythDirs(void);

MPUBLIC QString GetInstallPrefix(void);
MPUBLIC QString GetShareDir(void);
MPUBLIC QString GetLibraryDir(void);
MPUBLIC QString GetConfDir(void);
MPUBLIC QString GetThemesParentDir(void);
MPUBLIC QString GetPluginsDir(void);
MPUBLIC QString GetTranslationsDir(void);
MPUBLIC QString GetFiltersDir(void);

MPUBLIC QString GetPluginsNameFilter(void);
MPUBLIC QString FindPluginName(const QString &plugname);
MPUBLIC QString GetTranslationsNameFilter(void);
MPUBLIC QString FindTranslation(const QString &translation);
MPUBLIC QString GetFontsDir(void);

#endif

