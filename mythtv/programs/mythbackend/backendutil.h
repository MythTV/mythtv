#ifndef _BACKENDUTIL_H
#define _BACKENDUTIL_H

#include <qstringlist.h>

#include "libmythtv/remoteutil.h"
#include "encoderlink.h"

void BackendQueryDiskSpace(QStringList &strlist,
                           QMap <int, EncoderLink *> *encoderList,
                           bool consolidated = false, bool allHosts = false);

void GetFilesystemInfos(QMap<int, EncoderLink*> *tvList,
                        vector <FileSystemInfo> &fsInfos);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
