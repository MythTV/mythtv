#ifndef VIDEO_META_DATA_UTIL_H
#define VIDEO_META_DATA_UTIL_H

// Qt headers
#include <QString>

// MythTV headers
#include "mythtvexp.h"

class MTV_PUBLIC VideoMetaDataUtil
{
  public:
    static QString GetArtPath(const QString &pathname, const QString &type);
};

#endif // VIDEO_META_DATA_UTIL_H
