#ifndef IMAGESCANTHREAD_H
#define IMAGESCANTHREAD_H

// Qt headers
#include <QApplication>
#include <QFileInfo>
#include <QMap>

// MythTV headers
#include "mthread.h"
#include "imagemetadata.h"



class ImageScanThread : public MThread
{
public:
    ImageScanThread();
    ~ImageScanThread();

    bool m_continue;
    int  m_progressCount;
    int  m_progressTotalCount;

protected:
    void run();

private slots:

private:
    void SyncFilesFromDir(QString &path, int parentId);
    int  SyncDirectory(QFileInfo &fileInfo, int parentId);
    void SyncFile(QFileInfo &fileInfo, int parentId);

    QMap<QString, ImageMetadata *> *m_dbDirList;
    QMap<QString, ImageMetadata *> *m_dbFileList;
};

#endif // IMAGESCANTHREAD_H
