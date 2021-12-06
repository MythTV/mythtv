#ifndef MYTHHTTPROOT_H
#define MYTHHTTPROOT_H

// MythTV
#include "http/mythhttprequest.h"

class MBASE_PUBLIC MythHTTPRoot
{
  public:
    static HTTPResponse RedirectRoot(HTTPRequest2 Request, const QString& File);
};

#endif
