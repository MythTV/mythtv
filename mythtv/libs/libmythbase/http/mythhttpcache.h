#ifndef MYTHHTTPCACHE_H
#define MYTHHTTPCACHE_H

// MythTV
#include "http/mythhttpresponse.h"

class MythHTTPCache
{
  public:
    static void PreConditionCheck   (HTTPResponse Response);
    static void PreConditionHeaders (HTTPResponse Response);
};

#endif
