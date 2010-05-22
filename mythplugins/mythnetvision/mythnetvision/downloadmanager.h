#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QThread>
#include <QString>
#include <QStringList>

#include <rssparse.h>

class QObject;

typedef struct {
    QString url;
    QString filename;
    QString download;
    QStringList downloadargs;
} VideoDL;

class VideoDLEvent : public QEvent
{
  public:
    VideoDLEvent(VideoDL *dl) : QEvent(kEventType), videoDL(dl) {}
    ~VideoDLEvent() {}

    VideoDL *videoDL;

    static Type kEventType;
};

class DownloadManager : public QThread
{
  public:

    DownloadManager(QObject *parent);
    ~DownloadManager();

    void addDL(ResultItem *video);
    void cancel();

    QString getDownloadFilename(ResultItem *item);

  protected:

    void run();
    
  private:

    bool moreWork();

    QObject            *m_parent;
    
    QList<VideoDL *>   m_fileList;
    QMutex             m_mutex;

};

#endif /* DOWNLOADMANAGER_H */
