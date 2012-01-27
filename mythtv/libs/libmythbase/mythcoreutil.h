#ifndef COREUTIL_H_
#define COREUTIL_H_

#include <stdint.h>

#include <QString>

#include "mythbaseexp.h"

 MBASE_PUBLIC  int64_t getDiskSpace(const QString&,int64_t&,int64_t&);

 MBASE_PUBLIC  bool extractZIP(const QString &zipFile, const QString &outDir);

 MBASE_PUBLIC  QString RemoteDownloadFile(const QString &url,
                                   const QString &storageGroup,
                                   const QString &filename = "");
 MBASE_PUBLIC  QString RemoteDownloadFileNow(const QString &url,
                                      const QString &storageGroup,
                                      const QString &filename = "");

#endif // COREUTIL_H_
