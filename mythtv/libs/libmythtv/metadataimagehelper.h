#ifndef METADATAIMAGEHELPER_H
#define METADATAIMAGEHELPER_H

#include <QObject>
#include <QMultiMap>
#include <QMetaType>

#include "mythtvexp.h"

enum VideoArtworkType : std::uint8_t {
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
    uint    width     {0};
    uint    height    {0};
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

#endif // METADATAIMAGEHELPER_H
