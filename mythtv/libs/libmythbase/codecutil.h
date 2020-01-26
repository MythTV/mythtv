#ifndef CODEC_UTIL_H
#define CODEC_UTIL_H

#include "mythbaseexp.h"

#include <QByteArray>

class MBASE_PUBLIC CodecUtil
{
  public:
    static bool isValidUTF8(const QByteArray &data);
};

#endif // CODEC_UTIL_H
