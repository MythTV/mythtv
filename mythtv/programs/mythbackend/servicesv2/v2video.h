#ifndef V2VIDEO_H
#define V2VIDEO_H

#include "libmythbase/http/mythhttpservice.h"
#include "libmythmetadata/videometadatalistmanager.h"

#include "v2videoMetadataInfoList.h"
#include "v2videoLookupInfoList.h"
#include "v2blurayInfo.h"
#include "v2videoStreamInfoList.h"
#include "v2cutList.h"

#define VIDEO_SERVICE QString("/Video/")
#define VIDEO_HANDLE  QString("Video")

class V2Video : public MythHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",      "1.5")
    Q_CLASSINFO("description",  "Methods to access and update Video metadata and related topics")
    Q_CLASSINFO("LookupVideo",  "methods=GET,POST,HEAD")
    Q_CLASSINFO("GetSavedBookmark",  "name=long")
    Q_CLASSINFO("GetLastPlayPos",  "name=long")
    Q_CLASSINFO("RemoveVideoFromDB",  "methods=POST;name=bool")
    Q_CLASSINFO("AddVideo",  "methods=POST;name=bool")
    Q_CLASSINFO("UpdateVideoWatchedStatus",  "methods=POST;name=bool")
    Q_CLASSINFO("UpdateVideoMetadata",  "methods=POST;name=bool")

  public:
    V2Video();
   ~V2Video() override = default;
    static void RegisterCustomTypes();

  public slots:

    static V2VideoMetadataInfo*   GetVideo   (int Id);
    static V2VideoMetadataInfo*   GetVideoByFileName ( const QString  &FileName  );
    static long                   GetSavedBookmark (int Id );
    static long                   GetLastPlayPos (int Id );
    static V2VideoMetadataInfoList*  GetVideoList    ( const QString  &Folder,
                                                       const QString  &Sort,
                                                       bool           Descending,
                                                       int            StartIndex,
                                                       int            Count,
                                                       bool           CollapseSubDirs );
    static V2VideoLookupList*  LookupVideo        ( const QString    &Title,
                                                    const QString    &Subtitle,
                                                    const QString    &Inetref,
                                                    int              Season,
                                                    int              Episode,
                                                    const QString    &GrabberType,
                                                    bool             AllowGeneric );
    static bool             RemoveVideoFromDB  ( int      Id );
    static bool             AddVideo           ( const QString  &FileName,
                                                    const QString  &HostName  );

    static bool             UpdateVideoWatchedStatus ( int  Id,
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
                                                    QDate         ReleaseDate,
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
                                                    QDate          InsertDate,
                                                    const QString &ContentType,
                                                    const QString &Genres,
                                                    const QString &Cast,
                                                    const QString &Countries
    );

    static bool            SetSavedBookmark (         int   Id,
                                                          long  Offset );

    static bool            SetLastPlayPos   (         int   Id,
                                                          long  Offset );

    static V2BlurayInfo*   GetBluray          ( const QString  &Path      );

    static V2VideoStreamInfoList* GetStreamInfo ( const QString &StorageGroup,
                                                  const QString &FileName  );

    static V2CutList* GetVideoCutList     ( int              Id,
                                            const QString   &OffsetType,
                                            bool IncludeFps );

    static V2CutList* GetVideoCommBreak   ( int              Id,
                                            const QString   &OffsetType,
                                            bool IncludeFps );

  private:
    Q_DISABLE_COPY(V2Video)
};

#endif // V2VIDEO_H
