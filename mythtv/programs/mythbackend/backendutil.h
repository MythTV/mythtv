#ifndef _BACKENDUTIL_H
#define _BACKENDUTIL_H

#include <vector>
using namespace std;

#include <QMap>

#include "remoteutil.h"

class QStringList;
class EncoderLink;

void BackendQueryDiskSpace(QStringList &strlist,
                           QMap <int, EncoderLink *> *encoderList,
                           bool consolidated = false, bool allHosts = false);

QString GetPlaybackURL(ProgramInfo *pginfo, bool storePath = true);

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
