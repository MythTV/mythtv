#ifndef MEDIASERVERUTIL_H_
#define MEDIASERVERUTIL_H_

using namespace std;

#include <QString>

#include "programinfo.h"

QString GetPlaybackURL(ProgramInfo *pginfo, bool storePath = true);

#endif
