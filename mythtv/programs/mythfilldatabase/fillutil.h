#ifndef FILLUTIL_H
#define FILLUTIL_H

// Std C headers
#include <cstdio>

// Qt headers
#include <QFile>
#include <QString>

bool dash_open(QFile &file, const QString &filename,
               QIODevice::OpenMode m, FILE *handle = nullptr);

QString SetupIconCacheDirectory(void);

#endif // FILLUTIL_H
