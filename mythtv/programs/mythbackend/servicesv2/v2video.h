#ifndef V2VIDEO_H
#define V2VIDEO_H

#include "libmythbase/http/mythhttpservice.h"
#include "videometadatalistmanager.h"
#include "v2videoMetadataInfoList.h"

#define VIDEO_SERVICE QString("/Video/")
#define VIDEO_HANDLE  QString("Video")

class V2Video : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")

  public:
    V2Video();
   ~V2Video() override = default;
    static void RegisterCustomTypes();

  public slots:

    V2VideoMetadataInfo*   GetVideo   (int Id);
    V2VideoMetadataInfo*   GetVideoByFileName ( const QString  &FileName  );
    long                   GetSavedBookmark (int Id );
    V2VideoMetadataInfoList*  GetVideoList    ( const QString  &Folder,
                                                       const QString  &Sort,
                                                       bool           Descending,
                                                       int            StartIndex,
                                                       int            Count      );

  private:
    Q_DISABLE_COPY(V2Video)
};

#endif // V2VIDEO_H
