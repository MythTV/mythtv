//! \file
//! \brief Creates and manages thumbnails
//! \details Uses two worker threads to process thumbnail requests that are queued
//! from the scanner and UI.
//! One thread generates picture thumbs; the other video thumbs, which are delegated
//! to previewgenerator and time-consuming.
//! Both background threads are low-priority to avoid recording issues.
//! Requests are handled by client-assigned priority so that UI display requests
//! are serviced before background scanner requests.
//! When images are removed, their thumbnails are also deleted (thumbnail cache is
//! synchronised to database). Obsolete images are broadcast to enable clients to
//! also cleanup/synchronise their caches.

#ifndef IMAGETHUMBS_H
#define IMAGETHUMBS_H

#include <utility>

// Qt headers
#include <QMap>
#include <QMutex>
#include <QWaitCondition>

// MythTV headers
#include "libmythbase/mthread.h"
#include "imagetypes.h"

//! \brief Priority of a thumbnail request. First/lowest are handled before later/higher
//! \details Ordered to optimise perceived client performance, ie. pictures will be
//! displayed before directories (4 thumbnails), then videos (slow to generate) are filled
//! in last.
enum ImageThumbPriority : std::int8_t {
    kUrgentPriority     = -10, //!< Scanner request needed to complete a scan
    kPicRequestPriority =  -7, //!< Client request to display an image thumbnail
    kDirRequestPriority =  -3, //!< Client request to display a directory thumbnail
    kBackgroundPriority =   0  //!< Scanner background request
};


//! A generator request that is queued
class META_PUBLIC ThumbTask
{
public:

    /*!
     \brief Construct request for a single image
     \param action Request action
     \param im Image object that will be deleted.
     \param priority Request priority
     \param notify If true a 'thumbnail exists' event will be broadcast when done.
    */
    ThumbTask(QString action, const ImagePtrK& im,
              int priority = kUrgentPriority, bool notify = false)
        : m_action(std::move(action)), m_priority(priority), m_notify(notify)
    { m_images.append(im); }

    /*!
     \brief Construct request for a list of images/dirs
     \details Assumes ownership of list contents. Items will be deleted after processing
     \param action Request action
     \param list Image objects that will be deleted.
     \param priority Request priority
     \param notify If true a 'thumbnail exists' event will be broadcast when done.
    */
    ThumbTask(QString action, ImageListK list,
              int priority = kUrgentPriority, bool notify = false)
        : m_images(std::move(list)),
          m_action(std::move(action)),
          m_priority(priority),
          m_notify(notify)      {}

    //! Images for thumbnail task
    ImageListK m_images;
    //! Request action: Create, delete etc.
    QString m_action;
    //! Request reason/priority
    int m_priority;
    //! If true, a "THUMBNAIL_CREATED" event is broadcast
    bool m_notify;
};

using TaskPtr = QSharedPointer<ThumbTask>;


//! A generator worker thread
template <class DBFS>
class ThumbThread : public MThread
{
public:
    /*!
     \brief Constructor
     \param name Thread name
     \param dbfs Filesystem/Database adapter
    */
    ThumbThread(const QString &name, DBFS *const dbfs)
        : MThread(name), m_dbfs(*dbfs) {}
    ~ThumbThread() override;

    void cancel();
    void Enqueue(const TaskPtr &task);
    void AbortDevice(int devId, const QString &action);
    void PauseBackground(bool pause);

protected:
    void run() override; // MThread

private:
    Q_DISABLE_COPY(ThumbThread)

    //! A priority queue where 0 is highest priority
    using ThumbQueue = QMultiMap<int, TaskPtr>;

    QString CreateThumbnail(const ImagePtrK& im, int thumbPriority);
    static void RemoveTasks(ThumbQueue &queue, int devId);

    DBFS &m_dbfs;               //!< Database/filesystem adapter
    QWaitCondition m_taskDone;  //! Synchronises completed tasks

    ThumbQueue m_requestQ;   //!< Priority queue of requests
    ThumbQueue m_backgroundQ;   //!< Priority queue of background tasks
    bool m_doBackground {true}; //!< Whether to process background tasks
    QMutex m_mutex;            //!< Queue protection
};


template <class DBFS>
class META_PUBLIC ImageThumb
{
public:
    explicit ImageThumb(DBFS *dbfs);
    ~ImageThumb();

    void    ClearThumbs(int devId, const QString &action);
    QString DeleteThumbs(const ImageList &images);
    void    CreateThumbnail(const ImagePtrK &im,
                            int priority = kBackgroundPriority,
                            bool notify = false);
    void    MoveThumbnail(const ImagePtrK &im);
    void    PauseBackground(bool pause);

private:
    Q_DISABLE_COPY(ImageThumb)

    //! Assign priority to a background task.
    // Major element = tree depth, so shallow thumbs are created before deep ones
    // Minor element = id, so thumbs are created in order they were scanned
    // If not unique, QMultiMap will process later requests before earlier ones
    int Priority(ImageItemK &im)
    { return (im.m_filePath.count('/') * 1000) + im.m_id; }

    //! Db/filesystem adapter
    DBFS              &m_dbfs;
    //! Thread generating picture thumbnails
    ThumbThread<DBFS> *m_imageThread;
    //! Thread generating video previews
    ThumbThread<DBFS> *m_videoThread;
};

#endif // IMAGETHUMBS_H
