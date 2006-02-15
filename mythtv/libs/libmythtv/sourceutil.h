// -*- Mode: c++ -*-
#ifndef _SOURCEUTIL_H_
#define _SOURCEUTIL_H_

// Qt headers
#include <qstring.h>

class SourceUtil
{
  public:
    static QString GetChannelSeparator(uint sourceid);
    static QString GetChannelFormat(uint sourceid);
    static uint    GetChannelCount(uint sourceid);
};

#endif //_SOURCEUTIL_H_
