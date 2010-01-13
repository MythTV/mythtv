#ifndef IMAGEDOWNLOADMANAGER_H
#define IMAGEDOWNLOADMANAGER_H

#include <QThread>
#include <QString>
#include <QStringList>

#include "parse.h"

class QObject;

typedef struct {
    QString filename;
    QString title;
    QString url;
    uint pos;
} ImageData;

const int kImageDLEventType = QEvent::User + 4000;

class ImageDLEvent : public QEvent
{
  public:
    ImageDLEvent(ImageData *id)
         : QEvent((QEvent::Type)kImageDLEventType) { imageData=id; }
    ~ImageDLEvent() {}

    ImageData *imageData;
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

    bool moreWork();

    QObject            *m_parent;
    QList<ImageData*>  m_fileList;
    QMutex             m_mutex;

};

#endif /* IMAGEDOWNLOADMANAGER_H */
