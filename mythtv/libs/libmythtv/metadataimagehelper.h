#ifndef _METADATAIMAGEHELPER_H_
#define _METADATAIMAGEHELPER_H_

#include <QObject>

#include "mythtvexp.h"

enum VideoArtworkType {
    kArtworkCoverart = 0,
    kArtworkFanart = 1,
    kArtworkBanner = 2,
    kArtworkScreenshot = 3,
    kArtworkPoster = 4,
    kArtworkBackCover = 5,
    kArtworkInsideCover = 6,
    kArtworkCDImage = 7
};

struct ArtworkInfo
{
    QString label;
    QString thumbnail;
    QString url;
    uint width;
    uint height;
};

typedef QList< ArtworkInfo > ArtworkList;

typedef QMultiMap< VideoArtworkType, ArtworkInfo > ArtworkMap;

MTV_PUBLIC ArtworkMap GetArtwork(QString inetref,
                                       uint season,
                                       bool strict = false);
MTV_PUBLIC bool SetArtwork(const QString &inetref,
                                   uint season,
                                   const QString &host,
                                   const QString &coverart,
                                   const QString &fanart,
                                   const QString &banner);
MTV_PUBLIC bool SetArtwork(const QString &inetref,
                                   uint season,
                                   const QString &host,
                                   const ArtworkMap map);

#include "storagegroup.h"
#include "mythcorecontext.h"
inline QString generate_myth_url(
    const QString &storage_group, const QString &host, const QString &path)
{
    QString ip = gCoreContext->GetBackendServerIP(host);
    uint port = gCoreContext->GetBackendServerPort(host);

    return gCoreContext->GenMythURL(ip,port,path,
                                    StorageGroup::GetGroupToUse(host, storage_group));

}

Q_DECLARE_METATYPE(VideoArtworkType)
Q_DECLARE_METATYPE(ArtworkInfo)

#endif
