#ifndef COREUTIL_H_
#define COREUTIL_H_

#include <QString>

#include "mythbaseexp.h"

 MBASE_PUBLIC  long long getDiskSpace(const QString&,long long&,long long&);

 MBASE_PUBLIC  bool extractZIP(const QString &zipFile, const QString &outDir);

 MBASE_PUBLIC  QString RemoteDownloadFile(const QString &url,
                                   const QString &storageGroup,
                                   const QString &filename = "");
 MBASE_PUBLIC  QString RemoteDownloadFileNow(const QString &url,
                                      const QString &storageGroup,
                                      const QString &filename = "");

#endif // COREUTIL_H_
