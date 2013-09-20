#ifndef GALLERYDATABASEHELPER_H
#define GALLERYDATABASEHELPER_H

// Qt headers

// MythTV headers
#include "mythdbcon.h"
#include "imagemetadata.h"

#include "gallerytypedefs.h"



class GalleryDatabaseHelper
{
public:
    GalleryDatabaseHelper();
    ~GalleryDatabaseHelper();

    QList<int> GetStorageDirIDs();
    void LoadParentDirectory(QList<ImageMetadata *>*, int);
    void LoadDirectories(QMap<QString, ImageMetadata *>*);
    void LoadDirectories(QList<ImageMetadata *>*, int);
    void LoadFiles(QMap<QString, ImageMetadata *>*);
    void LoadFiles(QList<ImageMetadata *>*, int);
    void RemoveDirectory(ImageMetadata *);
    void RemoveFile(ImageMetadata *);
    int InsertDirectory(ImageMetadata *);
    int InsertFile(ImageMetadata *);
    void UpdateDirectory(ImageMetadata *);
    void UpdateFile(ImageMetadata *);

    void InsertData(ImageMetadata *);
    void UpdateData(ImageMetadata *);
    void RemoveData(ImageMetadata *);

    void ClearDatabase();

private:
    void LoadDirectoryValues(MSqlQuery &, ImageMetadata *);
    void LoadFileValues(MSqlQuery &, ImageMetadata *);

    void LoadDirectoryThumbnailValues(ImageMetadata *);
    void LoadFileThumbnailValues(ImageMetadata *);

    QString GetSortOrder();
};

#endif // GALLERYDATABASEHELPER_H
