#include <QObject>

#include "metadatacommon.h"
#include "mythmetaexp.h"

META_PUBLIC ArtworkMap GetArtwork(QString inetref,
                                       uint season,
                                       bool strict = false);
META_PUBLIC bool SetArtwork(const QString &inetref,
                                   uint season,
                                   const QString &host,
                                   const QString &coverart,
                                   const QString &fanart,
                                   const QString &banner);
META_PUBLIC bool SetArtwork(const QString &inetref,
                                   uint season,
                                   const QString &host,
                                   const ArtworkMap map);
