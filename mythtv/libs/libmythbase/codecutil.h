#ifndef _CODEC_UTIL_H_
#define _CODEC_UTIL_H_

#include "mythbaseexp.h"

#include <QByteArray>

class MBASE_PUBLIC CodecUtil
{
  public:
    static bool isValidUTF8(const QByteArray &data);
};

#endif
