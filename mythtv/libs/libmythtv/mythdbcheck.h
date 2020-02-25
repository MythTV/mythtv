#ifndef MYTHDBCHECK_H_
#define MYTHDBCHECK_H_

#include <string>
#include <vector>

#include "mythtvexp.h"

using DBUpdates = std::vector<std::string>;

MTV_PUBLIC bool performActualUpdate(
    const QString &component, const QString &versionkey,
    DBUpdates updates, const QString &version, QString &dbver);

MTV_PUBLIC bool UpdateDBVersionNumber(const QString &component, const QString &versionkey,
                                      const QString &newnumber, QString &dbver);

#endif
