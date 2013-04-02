#ifndef GALLERYFILEHELPER_H
#define GALLERYFILEHELPER_H

// Qt headers
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QDirIterator>
#include <QMap>
#include <QList>
#include <QUrl>

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
    bool        RemoveFile(ImageMetadata *);
    bool        RenameFile(ImageMetadata *, const QString &);
    bool        SetImageOrientation(ImageMetadata *);

    GallerySyncStatus   GetSyncStatus();
    QByteArray          GetExifValues(ImageMetadata *);

private:
    QByteArray  SendRequest(QUrl &, QNetworkAccessManager::Operation);

    int                         m_backendPort;
    QString                     m_backendHost;
    QNetworkAccessManager      *m_manager;
};

#endif // GALLERYFILEHELPER_H
