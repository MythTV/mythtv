#ifndef GALLERYDATABASEHELPER_H
#define GALLERYDATABASEHELPER_H

// Qt headers

// MythTV headers
#include "mythdbcon.h"
#include "imagemetadata.h"

#include "gallerytypedefs.h"


// TODO: Merge this into libmythmetadata/imageutils.h and remove
class GalleryDatabaseHelper
{
public:
    GalleryDatabaseHelper();
    ~GalleryDatabaseHelper();

    void LoadParentDirectory(QList<ImageMetadata *>*, int);
    void LoadDirectories(QList<ImageMetadata *>*, int);
    void LoadFiles(QList<ImageMetadata *>*, int);

    void UpdateData(ImageMetadata *);

    void ClearDatabase();

private:
    int InsertDirectory(ImageMetadata *);
    int InsertFile(ImageMetadata *);
    void UpdateDirectory(ImageMetadata *);
    void UpdateFile(ImageMetadata *);

    void LoadDirectoryValues(MSqlQuery &, ImageMetadata *);
    void LoadFileValues(MSqlQuery &, ImageMetadata *);

    void LoadDirectoryThumbnailValues(ImageMetadata *);
    void LoadFileThumbnailValues(ImageMetadata *);

    QString GetSortOrder();
};

#endif // GALLERYDATABASEHELPER_H
