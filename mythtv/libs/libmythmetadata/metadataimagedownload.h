#ifndef METADATAIMAGEDOWNLOAD_H
#define METADATAIMAGEDOWNLOAD_H

#include <QString>
#include <QStringList>

#include "mthread.h"
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
                 item(lookup)
    {
        if (item)
        {
            item->IncrRef();
        }
    }
    ~ImageDLEvent()
    {
        if (item)
        {
            item->DecrRef();
            item = NULL;
        }
    }

    MetadataLookup *item;

    static Type kEventType;
};

class META_PUBLIC ImageDLFailureEvent : public QEvent
{
  public:
    ImageDLFailureEvent(MetadataLookup *lookup) :
                 QEvent(kEventType),
                 item(lookup)
    {
        if (item)
        {
            item->IncrRef();
        }
    }
    ~ImageDLFailureEvent()
    {
        if (item)
        {
            item->DecrRef();
            item = NULL;
        }
    }

    MetadataLookup *item;

    static Type kEventType;
};

class META_PUBLIC ThumbnailDLEvent : public QEvent
{
  public:
    ThumbnailDLEvent(ThumbnailData *data) :
                 QEvent(kEventType),
                 thumb(data) {}
    ~ThumbnailDLEvent()
    {
        delete thumb;
        thumb = NULL;
    }

    ThumbnailData *thumb;

    static Type kEventType;
};

class META_PUBLIC MetadataImageDownload : public MThread
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

    QObject                   *m_parent;
    MetadataLookupList         m_downloadList;
    QList<ThumbnailData*>      m_thumbnailList;
    QMutex                     m_mutex;
};

META_PUBLIC QString getDownloadFilename(QString title, QString url);
META_PUBLIC QString getDownloadFilename(VideoArtworkType type, MetadataLookup *lookup,
                                    QString url);

META_PUBLIC QString getLocalWritePath(MetadataType metadatatype, VideoArtworkType type);
META_PUBLIC QString getStorageGroupURL(VideoArtworkType type, QString host);
META_PUBLIC QString getLocalStorageGroupPath(VideoArtworkType type, QString host);

META_PUBLIC void cleanThumbnailCacheDir(void);

#endif /* METADATAIMAGEDOWNLOAD_H */
