#ifndef MYTHHTTPREWRITE_H
#define MYTHHTTPREWRITE_H

// MythTV
#include "http/mythhttprequest.h"

class MBASE_PUBLIC MythHTTPRewrite
{
  public:
    static HTTPResponse RewriteFile(HTTPRequest2 Request, const QString& File);
    static HTTPResponse RewriteToSPA(HTTPRequest2 Request, const QString& File);
};

#endif
