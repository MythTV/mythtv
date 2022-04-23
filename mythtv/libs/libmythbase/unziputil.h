#ifndef UNZIPUTIL_H_
#define UNZIPUTIL_H_

#include <QByteArray>
#include <QString>

#include "mythbaseexp.h"

MBASE_PUBLIC  bool extractZIP(QString zipFile, const QString& outDir);

MBASE_PUBLIC  bool gzipFile(const QString &inFilename, const QString &zipFilename);
MBASE_PUBLIC  bool gunzipFile(const QString &zipFilename, const QString &outFilename);

MBASE_PUBLIC  QByteArray gzipCompress(const QByteArray &data);
MBASE_PUBLIC  QByteArray gzipUncompress(const QByteArray &data);

#endif // UNZIPUTIL_H_
