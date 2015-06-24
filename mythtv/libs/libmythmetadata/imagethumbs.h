//! \file
//! \brief Creates and manages thumbnails in the cache
//! \details Uses two worker threads to process thumbnail requests that are queued.
//! One for pictures and a one for videos, which are off-loaded to previewgenerator,
//! and time-consuming. Both background threads are low-priority to avoid recording issues.
//! Requests are handled by client-assigned priority so that on-demand display requests
//! are serviced before background pre-generation requests.
//! When images are removed, their thumbnails are also deleted (thumbnail cache is
//! synchronised to database). Obselete thumbnails are broadcast to enable clients to
//! also manage/synchronise their caches.

#ifndef IMAGETHUMBGEN_H
#define IMAGETHUMBGEN_H

// Qt headers
#include <QMutex>
#include <QList>
#include <QMap>
#include <QDir>
#include <QImage>

// MythTV headers
#include <mthread.h>
#include <imageutils.h>


//! \brief Priority of a thumbnail request. First/lowest are handled before later/higher
//! \details Ordered to optimise perceived client performance, ie. pictures will be
//! displayed before directories (4 thumbnails), then videos (slow to generate) are filled
//! in last.
typedef enum priorities {
    kPicRequestPriority    = 0, //!< Client request to display an image thumbnail
    kFolderRequestPriority = 1, //!< Client request to display a directory thumbnail
    kVideoRequestPriority  = 2, //!< Client request to display a video preview
    kScannerUrgentPriority = 3, //!< Scanner request needed to complete a scan
    kBackgroundPriority    = 4  //!< Scanner background request
} ImageThumbPriority;


//! A generator request that is queued
class META_PUBLIC ThumbTask : public ImageList
{
public:
    ThumbTask(const QString &,
              ImageThumbPriority = kBackgroundPriority, bool = false);
    ThumbTask(QString, ImageItem*,
              ImageThumbPriority = kBackgroundPriority, bool = false);
    ThumbTask(const QString &, ImageMap&,
              ImageThumbPriority = kBackgroundPriority, bool = false);
    ThumbTask(QString, ImageList&,
              ImageThumbPriority = kBackgroundPriority, bool = false);

    //! Request action: Create, delete etc.
    QString m_action;
    //! Request reason/priority
    ImageThumbPriority m_priority;
    //! If true, a "THUMBNAIL_CREATED" event is broadcast
    bool m_notify;
};


//! A generator worker thread
class META_PUBLIC ThumbThread : public MThread
{
  public:
    ThumbThread(QString name);
    ~ThumbThread();

    void QueueThumbnails(ThumbTask *);
    void ClearThumbnails();
    int GetQueueSize(ImageThumbPriority);

  protected:
    void run();
    void cancel();

  private:
    void CreateImageThumbnail(ImageItem *);
    void CreateVideoThumbnail(ImageItem *);
    bool RemoveDirContents(QString);
    void Orientate(ImageItem *im, QImage &image);

    //! A queue of generator requests
    typedef QList<ThumbTask *> ThumbQueue;
    //! A priority queue where 0 is highest priority
    QMap<ImageThumbPriority, ThumbQueue *> m_thumbQueue;
    //! Queue protection
    QMutex m_mutex;

    //! Storage Group accessor
    ImageSg *m_sg;

    //! Path of backend thumbnail cache
    QDir m_thumbDir;
    //! Path of backend temp
    QDir m_tempDir;
};


//! Singleton creating/managing image thumbnails
class META_PUBLIC ImageThumb
{
  public:
    static ImageThumb* getInstance();

    void        CreateThumbnail(ImageItem *, ImageThumbPriority);
    void        HandleCreateThumbnails(QStringList imlist);
    int         GetQueueSize(ImageThumbPriority);
    void        ClearAllThumbs();
    QStringList DeleteThumbs(ImageList, ImageList);

  private:
    ImageThumb();
    ~ImageThumb();

    //! Singleton
    static ImageThumb *m_instance;

    //! Worker thread generating picture thumbnails
    ThumbThread       *m_imageThumbThread;
    //! Worker thread generating video previews
    ThumbThread       *m_videoThumbThread;
};

#endif // IMAGETHUMBGEN_H
