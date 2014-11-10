#ifndef GALLERYFILEHELPER_H
#define GALLERYFILEHELPER_H

// Qt headers
#include <QNetworkProxy>
#include <QMap>
#include <QList>

// MythImage headers
#include "imagemetadata.h"

struct GallerySyncStatus {
    bool running;
    int  current;
    int  total;
};

class GalleryFileHelper
{
public:
    GalleryFileHelper();
    ~GalleryFileHelper();

    void        StartSyncImages();
    void        StopSyncImages();
    void        AddToThumbnailList(ImageMetadata *, bool);
    bool        RemoveFile(ImageMetadata *);
    bool        RenameFile(ImageMetadata *, const QString &);
    bool        SetImageOrientation(ImageMetadata *);

    GallerySyncStatus      GetSyncStatus();
    QMap<QString, QString> GetExifValues(ImageMetadata *);
};

#endif // GALLERYFILEHELPER_H
