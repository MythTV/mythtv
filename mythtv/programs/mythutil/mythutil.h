#ifndef MYTHUTIL_H_
#define MYTHUTIL_H_

// Qt includes
#include <QMap>
#include <QString>

// libmyth* includes
#include "programinfo.h"

// Local includes
#include "commandlineparser.h"

using UtilFunc = int (*)(const MythUtilCommandLineParser &cmdline);
using UtilMap  = QMap<QString, UtilFunc>;

bool GetProgramInfo(const MythUtilCommandLineParser &cmdline,
                    ProgramInfo &pginfo);

#endif
