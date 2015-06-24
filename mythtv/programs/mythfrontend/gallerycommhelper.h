//! \file
//! \brief Encapsulates BE requests that originate from FE or services client

#ifndef GALLERYCOMMHELPER_H
#define GALLERYCOMMHELPER_H

#include <QMap>
#include <QStringList>
#include <QString>

#include <mthread.h>
#include <imagemetadata.h>
#include <imageutils.h>
#include <mythprogressdialog.h>


typedef QMap<QString, QString> NameMap;

//! BE Requests
class GalleryBERequest
{
public:
    static QString     ScanImagesAction(bool);
    static QStringList ScanQuery();
    static void        CreateThumbnails(QStringList, bool);
    static QString     HideFiles(bool, ImageIdList);
    static QString     RemoveFiles(ImageIdList);
    static QString     RenameFile(int, QString);
    static QString     ChangeOrientation(ImageFileTransform transform,
                                         ImageIdList);
    static QString     ClearDatabase();
    static QString     SetCover(int, int);
    static NameMap     GetMetaData(const ImageItem *);
    static QString     MoveFiles(int, ImageIdList);
    static QString     MakeDirs(QStringList);
    static QString     IgnoreDirs(QString);
};


//! Base thread returning a result
class WorkerThread: public MThread
{
public:
    WorkerThread(QString name) : MThread(name) {}
    int GetResult(void) { return m_result; }
protected:
    int m_result;
};


//! Worker thread for running commands
class ShellWorker: public WorkerThread
{
public:
    ShellWorker(QString cmd) : WorkerThread("import"), m_command(cmd) {}
    virtual void run();
private:
    QString m_command;
};


//! Worker thread for copying/moving files
class FileTransferWorker : public WorkerThread
{
    Q_DECLARE_TR_FUNCTIONS(FileTransferWorker)
public:
    FileTransferWorker(bool, NameMap, MythUIProgressDialog *);
    virtual void run();

private:
    //! Whether to delete files after they have been copied
    bool m_delete;

    //! Maps source filepath to destination filepath
    NameMap m_files;

    //! Progress dialog for transfer
    MythUIProgressDialog *m_dialog;
};


int RunWorker(WorkerThread *);


#endif // GALLERYCOMMHELPER_H
