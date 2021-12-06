#ifndef V2VIDEO_H
#define V2VIDEO_H

#include "libmythbase/http/mythhttpservice.h"
#include "videometadatalistmanager.h"
#include "v2videoMetadataInfoList.h"
#include "v2videoLookupInfoList.h"
#include "v2blurayInfo.h"
#include "v2videoStreamInfoList.h"

#define VIDEO_SERVICE QString("/Video/")
#define VIDEO_HANDLE  QString("Video")

class V2Video : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.0")
    Q_CLASSINFO("description",  "Methods to access and update Video metadata and related topics")
    Q_CLASSINFO("LookupVideo",  "methods=GET,POST,HEAD")
    Q_CLASSINFO("GetSavedBookmark",  "name=long")
    Q_CLASSINFO("RemoveVideoFromDB",  "methods=POST;name=bool")
    Q_CLASSINFO("AddVideo",  "methods=POST;name=bool")
    Q_CLASSINFO("UpdateVideoWatchedStatus",  "methods=POST;name=bool")
    Q_CLASSINFO("UpdateVideoMetadata",  "methods=POST;name=bool")

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
    bool                    RemoveVideoFromDB  ( int      Id );
    bool                    AddVideo           ( const QString  &FileName,
                                                    const QString  &HostName  );

    bool                    UpdateVideoWatchedStatus ( int  Id,
                                                    bool Watched );
    bool                    UpdateVideoMetadata      ( int           Id,
                                                    const QString &Title,
                                                    const QString &SubTitle,
                                                    const QString &TagLine,
                                                    const QString &Director,
                                                    const QString &Studio,
                                                    const QString &Plot,
                                                    const QString &Rating,
                                                    const QString &Inetref,
                                                    int           CollectionRef,
                                                    const QString &HomePage,
                                                    int           Year,
                                                    const QDate   &ReleaseDate,
                                                    float         UserRating,
                                                    int           Length,
                                                    int           PlayCount,
                                                    int           Season,
                                                    int           Episode,
                                                    int           ShowLevel,
                                                    const QString &FileName,
                                                    const QString &Hash,
                                                    const QString &CoverFile,
                                                    int           ChildID,
                                                    bool          Browse,
                                                    bool          Watched,
                                                    bool          Processed,
                                                    const QString &PlayCommand,
                                                    int           Category,
                                                    const QString &Trailer,
                                                    const QString &Host,
                                                    const QString &Screenshot,
                                                    const QString &Banner,
                                                    const QString &Fanart,
                                                    const QDate   &InsertDate,
                                                    const QString &ContentType,
                                                    const QString &Genres,
                                                    const QString &Cast,
                                                    const QString &Countries
    );

    bool                   SetSavedBookmark (         int   Id,
                                                          long  Offset );

    V2BlurayInfo*          GetBluray          ( const QString  &Path      );

    V2VideoStreamInfoList* GetStreamInfo ( const QString &StorageGroup,
                                                  const QString &FileName  );



  private:
    Q_DISABLE_COPY(V2Video)
};

#endif // V2VIDEO_H
