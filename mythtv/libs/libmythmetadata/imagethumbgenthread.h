#ifndef GALLERYTHUMBGENTHREAD_H
#define GALLERYTHUMBGENTHREAD_H

// Qt headers
#include <QThread>
#include <QMutex>

// MythTV headers
#include "mythuibuttontree.h"
#include "imagemetadata.h"
#include "storagegroup.h"
#include "mythmetaexp.h"


class META_PUBLIC ImageThumbGenThread : public QThread
{
    Q_OBJECT

public:
    ImageThumbGenThread();
    ~ImageThumbGenThread();

    void cancel();
    void Pause();
    void Resume();
    void AddToThumbnailList(ImageMetadata *);
    void RecreateThumbnail(ImageMetadata *);
    void SetThumbnailSize(int, int);

signals:
    void ThumbnailCreated(ImageMetadata *, int);
    void UpdateThumbnailProgress(int, int);

protected:
    void run();

private:
    void CreateImageThumbnail(ImageMetadata *, int);
    void CreateVideoThumbnail(ImageMetadata *);

    void Resize(QImage &);
    void Rotate(QImage &);
    void Combine(QImage &, QImage &, QPoint);
    void DrawBorder(QImage &);

    QList<ImageMetadata *>    m_fileList;
    QMutex              m_mutex;

    int m_width;
    int m_height;
    bool m_pause;
    int m_fileListSize;

    QWaitCondition      m_condition;
    StorageGroup        m_storageGroup;
};

#endif // GALLERYTHUMBGENTHREAD_H
