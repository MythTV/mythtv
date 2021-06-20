#ifndef V2VIDEO_H
#define V2VIDEO_H

#include "libmythbase/http/mythhttpservice.h"
#include "videometadatalistmanager.h"
#include "v2videoMetadataInfo.h"

#define VIDEO_SERVICE QString("/Video/")
#define VIDEO_HANDLE  QString("Video")

class V2Video : public MythHTTPService
{
    Q_OBJECT

  public:
    V2Video();
   ~V2Video() override = default;
    static void RegisterCustomTypes();

  public slots:

    V2VideoMetadataInfo*   GetVideo   (int Id);

  private:
    Q_DISABLE_COPY(V2Video)
};

#endif // V2VIDEO_H
