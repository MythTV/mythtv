#ifndef METADATAIMAGEDOWNLOAD_H
#define METADATAIMAGEDOWNLOAD_H

#include <QThread>
#include <QString>
#include <QStringList>

#include "mythmetaexp.h"
#include "metadatacommon.h"

typedef struct {
    QString title;
    QVariant data;
    QString url;
} ThumbnailData;

class META_PUBLIC ImageDLEvent : public QEvent
{
  public:
    ImageDLEvent(MetadataLookup *lookup) :
                 QEvent(kEventType),
                 item(lookup) {}
    ~ImageDLEvent() {}

    MetadataLookup *item;

    static Type kEventType;
};

class META_PUBLIC ThumbnailDLEvent : public QEvent
{
  public:
    ThumbnailDLEvent(ThumbnailData *data) :
                 QEvent(kEventType),
                 thumb(data) {}
    ~ThumbnailDLEvent() {}

    ThumbnailData *thumb;

    static Type kEventType;
};

class META_PUBLIC MetadataImageDownload : public QThread
{
  public:

    MetadataImageDownload(QObject *parent);
    ~MetadataImageDownload();

    void addThumb(QString title, QString url, QVariant data);
    void addDownloads(MetadataLookup *lookup);
    void cancel();

  protected:

    void run();

  private:

    ThumbnailData*             moreThumbs();
    MetadataLookup*            moreDownloads();

    QObject                   *m_parent;
    QList<MetadataLookup*>     m_downloadList;
    QList<ThumbnailData*>      m_thumbnailList;
    QMutex                     m_mutex;
};

META_PUBLIC QString getDownloadFilename(QString title, QString url);
META_PUBLIC QString getDownloadFilename(VideoArtworkType type, MetadataLookup *lookup,
                                    QString url);

META_PUBLIC QString getLocalWritePath(MetadataType metadatatype, VideoArtworkType type);
META_PUBLIC QString getStorageGroupURL(VideoArtworkType type, QString host);

META_PUBLIC void cleanThumbnailCacheDir(void);

#endif /* METADATAIMAGEDOWNLOAD_H */
