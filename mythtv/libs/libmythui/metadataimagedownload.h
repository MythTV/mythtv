#ifndef METADATAIMAGEDOWNLOAD_H
#define METADATAIMAGEDOWNLOAD_H

#include <QThread>
#include <QString>
#include <QStringList>

#include "mythexp.h"
#include "metadatacommon.h"

typedef struct {
    QString title;
    QVariant data;
    QString url;
} ThumbnailData;

class MPUBLIC ImageDLEvent : public QEvent
{
  public:
    ImageDLEvent(MetadataLookup *lookup) :
                 QEvent(kEventType),
                 item(lookup) {}
    ~ImageDLEvent() {}

    MetadataLookup *item;

    static Type kEventType;
};

class MPUBLIC ThumbnailDLEvent : public QEvent
{
  public:
    ThumbnailDLEvent(ThumbnailData *data) :
                 QEvent(kEventType),
                 thumb(data) {}
    ~ThumbnailDLEvent() {}

    ThumbnailData *thumb;

    static Type kEventType;
};

class MPUBLIC MetadataImageDownload : public QThread
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

MPUBLIC QString getDownloadFilename(QString title, QString url);
MPUBLIC QString getDownloadFilename(ArtworkType type, MetadataLookup *lookup,
                                    QString url);

MPUBLIC QString getLocalWritePath(MetadataType metadatatype, ArtworkType type);
MPUBLIC QString getStorageGroupURL(ArtworkType type, QString host);

MPUBLIC QByteArray getImageFile(QString url);

#endif /* METADATAIMAGEDOWNLOAD_H */
