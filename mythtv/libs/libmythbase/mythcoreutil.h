#ifndef COREUTIL_H_
#define COREUTIL_H_

#include <cstdint>

#include <QString>

#include "mythbaseexp.h"

 MBASE_PUBLIC  int64_t getDiskSpace(const QString &file_on_disk, int64_t &total, int64_t &used);

#endif // COREUTIL_H_
