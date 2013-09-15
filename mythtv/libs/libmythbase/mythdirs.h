#ifndef MYTHDIRS_H_
#define MYTHDIRS_H_

#include <QString>
#include "mythbaseexp.h"

 MBASE_PUBLIC  void InitializeMythDirs(void);

 MBASE_PUBLIC  QString GetInstallPrefix(void);
 MBASE_PUBLIC  QString GetAppBinDir(void);
 MBASE_PUBLIC  QString GetShareDir(void);
 MBASE_PUBLIC  QString GetLibraryDir(void);
 MBASE_PUBLIC  QString GetConfDir(void);
 MBASE_PUBLIC  QString GetThemesParentDir(void);
 MBASE_PUBLIC  QString GetPluginsDir(void);
 MBASE_PUBLIC  QString GetTranslationsDir(void);
 MBASE_PUBLIC  QString GetFiltersDir(void);

 MBASE_PUBLIC  QString GetPluginsNameFilter(void);
 MBASE_PUBLIC  QString FindPluginName(const QString &plugname);
 MBASE_PUBLIC  QString GetTranslationsNameFilter(void);
 MBASE_PUBLIC  QString FindTranslation(const QString &translation);
 MBASE_PUBLIC  QString GetFontsDir(void);

#endif

