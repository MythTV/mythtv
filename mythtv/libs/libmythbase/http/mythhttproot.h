#ifndef MYTHHTTPROOT_H
#define MYTHHTTPROOT_H

// MythTV
#include "libmythbase/http/mythhttprequest.h"

class MBASE_PUBLIC MythHTTPRoot
{
  public:
    static HTTPResponse RedirectRoot(const HTTPRequest2& Request, const QString& File);
};

#endif
