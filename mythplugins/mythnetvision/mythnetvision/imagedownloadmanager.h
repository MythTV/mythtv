#ifndef IMAGEDOWNLOADMANAGER_H
#define IMAGEDOWNLOADMANAGER_H

#include <QThread>
#include <QString>
#include <QStringList>

#include <rssparse.h>

class QObject;

typedef struct {
    QString filename;
    QString title;
    QString url;
    uint pos;
} ImageData;

class ImageDLEvent : public QEvent
{
  public:
    ImageDLEvent(ImageData *id) : QEvent(kEventType), imageData(id) {}
    ~ImageDLEvent() {}

    ImageData *imageData;

    static Type kEventType;
};

class ImageDownloadManager : public QThread
{
  public:

    ImageDownloadManager(QObject *parent);
    ~ImageDownloadManager();

    void addURL(const QString& title, const QString& url,
                const uint& pos);
    void cancel();

  protected:

    void run();

  private:

    ImageData* moreWork();

    QObject            *m_parent;
    QList<ImageData*>  m_fileList;
    QMutex             m_mutex;

};

#endif /* IMAGEDOWNLOADMANAGER_H */
