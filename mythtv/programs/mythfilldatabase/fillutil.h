#ifndef FILLUTIL_H
#define FILLUTIL_H

// Std C headers
#include <cstdio>

// Qt headers
#include <QString>

class QFile;

bool dash_open(QFile &file, const QString &filename,
               int m, FILE *handle = nullptr);

QString SetupIconCacheDirectory(void);

#endif // FILLUTIL_H
