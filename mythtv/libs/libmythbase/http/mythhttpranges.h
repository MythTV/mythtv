#ifndef MYTHHTTPRANGES_H
#define MYTHHTTPRANGES_H

// MythTV
#include "libmythbase/http/mythhttpdata.h"
#include "libmythbase/http/mythhttpresponse.h"
#include "libmythbase/http/mythhttptypes.h"

using HTTPRange  = std::pair<uint64_t,uint64_t>;
using HTTPRanges = std::vector<HTTPRange>;

class MythHTTPRanges
{
  public:
    static void      HandleRangeRequest   (MythHTTPResponse* Response, const QString& Request);
    static void      BuildMultipartHeaders(MythHTTPResponse* Response);
    static QString   GetRangeHeader       (HTTPRanges& Ranges, int64_t Size);
    static QString   GetRangeHeader       (HTTPRange& Range, int64_t Size);
    static HTTPMulti HandleRangeWrite     (HTTPVariant Data, int64_t Available, int64_t& ToWrite, int64_t& Offset);

  protected:
    static MythHTTPStatus ParseRanges     (const QString& Request, int64_t TotalSize,
                                           HTTPRanges& Ranges, int64_t& PartialSize);
};

#endif
