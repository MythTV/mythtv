#ifndef MYTHHTTPREWRITE_H
#define MYTHHTTPREWRITE_H

// MythTV
#include "libmythbase/http/mythhttprequest.h"

class MBASE_PUBLIC MythHTTPRewrite
{
  public:
    static HTTPResponse RewriteFile(const HTTPRequest2& Request, const QString& File);
    static HTTPResponse RewriteToSPA(const HTTPRequest2& Request, const QString& File);
};

#endif
