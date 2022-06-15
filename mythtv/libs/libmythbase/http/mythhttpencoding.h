#ifndef MYTHHTTPENCODING_H
#define MYTHHTTPENCODING_H

// MythTV
#include "libmythbase/http/mythhttptypes.h"

class MythHTTPEncoding
{
  public:
    static QStringList    GetMimeTypes(const QString& Accept);
    static void           GetContentType(MythHTTPRequest* Request);
    static MythMimeType   GetMimeType(HTTPVariant Content);
    static MythHTTPEncode Compress(MythHTTPResponse* Response, int64_t& Size);

  protected:
    static void           GetURLEncodedParameters(MythHTTPRequest* Request);
    static void           GetXMLEncodedParameters(MythHTTPRequest* Request);
    static void           GetJSONEncodedParameters(MythHTTPRequest* Request);
};

#endif
