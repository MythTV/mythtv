#ifndef V2VIDEO_H
#define V2VIDEO_H

#include "libmythbase/http/mythhttpservice.h"
#include "videometadatalistmanager.h"
#include "v2videoMetadataInfoList.h"
#include "v2videoLookupInfoList.h"

#define VIDEO_SERVICE QString("/Video/")
#define VIDEO_HANDLE  QString("Video")

class V2Video : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")
    Q_CLASSINFO("LookupVideo",  "methods=GET,POST,HEAD")
    Q_CLASSINFO("GetSavedBookmark",  "name=long")

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
    V2VideoLookupList*     LookupVideo        ( const QString    &Title,
                                                    const QString    &Subtitle,
                                                    const QString    &Inetref,
                                                    int              Season,
                                                    int              Episode,
                                                    const QString    &GrabberType,
                                                    bool             AllowGeneric );

  private:
    Q_DISABLE_COPY(V2Video)
};

#endif // V2VIDEO_H
