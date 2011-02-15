#ifndef _VIDEO_META_DATA_UTIL_H_
#define _VIDEO_META_DATA_UTIL_H_

// Qt headers
#include <QString>

// MythTV headers
#include "mythtvexp.h"

class MTV_PUBLIC VideoMetaDataUtil
{
  public:
    static QString GetArtPath(const QString &basename, const QString &type);
};

#endif // _VIDEO_META_DATA_UTIL_H_
