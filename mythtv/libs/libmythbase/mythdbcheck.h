#ifndef MYTHDBCHECK_H_
#define MYTHDBCHECK_H_

#include <string>
#include <vector>

#include "mythbaseexp.h"

using DBUpdates = std::vector<std::string>;

MBASE_PUBLIC bool performActualUpdate(
    const QString &component, const QString &versionkey,
    const DBUpdates &updates, const QString &version, QString &dbver);

MBASE_PUBLIC bool performUpdateSeries(const QString &component, DBUpdates updates);

MBASE_PUBLIC bool UpdateDBVersionNumber(const QString &component, const QString &versionkey,
                                        const QString &newnumber, QString &dbver);

#endif
