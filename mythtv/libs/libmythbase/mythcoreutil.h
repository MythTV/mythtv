#ifndef COREUTIL_H_
#define COREUTIL_H_

#include <cstdint>

#include <QString>

#include "mythbaseexp.h"

 MBASE_PUBLIC  int64_t getDiskSpace(const QString &file_on_disk, int64_t &total, int64_t &used);

 MBASE_PUBLIC  bool extractZIP(QString &zipFile, const QString &outDir);

 MBASE_PUBLIC  bool gzipFile(const QString &inFilename, const QString &zipFilename);
 MBASE_PUBLIC  bool gunzipFile(const QString &zipFilename, const QString &outFilename);

 MBASE_PUBLIC  QByteArray gzipCompress(const QByteArray &data);
 MBASE_PUBLIC  QByteArray gzipUncompress(const QByteArray &data);

 MBASE_PUBLIC  QString RemoteDownloadFile(const QString &url,
                                   const QString &storageGroup,
                                   const QString &filename = "");
 MBASE_PUBLIC  QString RemoteDownloadFileNow(const QString &url,
                                      const QString &storageGroup,
                                      const QString &filename = "");

#endif // COREUTIL_H_
