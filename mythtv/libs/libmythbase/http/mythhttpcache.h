#ifndef MYTHHTTPCACHE_H
#define MYTHHTTPCACHE_H

// MythTV
#include "libmythbase/http/mythhttpresponse.h"

class MythHTTPCache
{
  public:
    static void PreConditionCheck   (const HTTPResponse& Response);
    static void PreConditionHeaders (const HTTPResponse& Response);
};

#endif
