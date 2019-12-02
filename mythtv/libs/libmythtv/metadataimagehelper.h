#ifndef _METADATAIMAGEHELPER_H_
#define _METADATAIMAGEHELPER_H_

#include <QObject>
#include <QMultiMap>
#include <QMetaType>

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

using ArtworkList = QList< ArtworkInfo >;
using ArtworkMap  = QMultiMap< VideoArtworkType, ArtworkInfo >;

MTV_PUBLIC ArtworkMap GetArtwork(const QString& inetref,
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
                                   const ArtworkMap& map);

Q_DECLARE_METATYPE(VideoArtworkType)
Q_DECLARE_METATYPE(ArtworkInfo)

#endif
