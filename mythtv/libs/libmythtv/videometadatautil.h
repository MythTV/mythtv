#ifndef _VIDEO_META_DATA_UTIL_H_
#define _VIDEO_META_DATA_UTIL_H_

// Qt headers
#include <QString>

// MythTV headers
#include "mythexp.h" // for MPUBLIC

class MPUBLIC VideoMetaDataUtil
{
  public:
    static QString GetCoverArtPath(const QString &pathname);
};

#endif // _VIDEO_META_DATA_UTIL_H_
