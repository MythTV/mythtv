#ifndef MYTHUTIL_H_
#define MYTHUTIL_H_

// Qt includes
#include <QMap>
#include <QString>

// libmyth* includes
#include "programinfo.h"

// Local includes
#include "commandlineparser.h"

typedef int (*UtilFunc)(const MythUtilCommandLineParser &cmdline);

typedef QMap<QString, UtilFunc> UtilMap;

bool GetProgramInfo(const MythUtilCommandLineParser &cmdline,
                    ProgramInfo &pginfo);

#endif
