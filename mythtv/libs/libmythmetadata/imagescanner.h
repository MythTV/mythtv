//! \file
//! \brief Synchronises image database to storage group
//! \details Detects supported pictures and videos within storage group and populates
//! the image database with metadata for each, including directory structure.
//! After a scan completes, a background task then creates thumbnails for each new image
//! to improve client performance.

#ifndef IMAGESCAN_H
#define IMAGESCAN_H

#include <QFileInfo>
#include <QMap>
#include <QDir>
#include <QRegExp>
#include <QMutex>

#include <mthread.h>
#include <imageutils.h>


//! \brief Current/last requested scanner state
//! \details Valid state transitions are:
//!  Scan -> Dormant : Scan requested
//!  Clear -> Dormant : Clear db requested
//!  Scan -> Interrupt -> Dormant : Scan requested, then interrupted
//!  Scan -> Clear -> Dormant : Clear db requested during scan
//!  Scan -> Interrupt -> Clear -> Dormant : Scan interrupted, then Clear Db requested
enum ScannerState
{
    kScan,      //!< sync is pending/in effect
    kInterrupt, //!< cancelled sync is pending/in effect
    kClear,     //!< clear db is pending/in effect
    kDormant    //!< doing nothing
};


//! Scanner worker thread
class META_PUBLIC ImageScanThread : public MThread
{
public:
    ImageScanThread();
    ~ImageScanThread();

    void cancel();
    QStringList GetProgress();
    ScannerState GetState();
    void ChangeState(ScannerState to);

protected:
    void run();

private:
    ImageItem* LoadDirectoryData(QFileInfo, int, QString);
    ImageItem* LoadFileData(QFileInfo, QString);

    void SyncFilesFromDir(QString, int, QString);
    int  SyncDirectory(QFileInfo, int, QString);
    void SyncFile(QFileInfo, int, QString);
    void WaitForThumbs();
    void BroadcastStatus(QString);
    void CountFiles(QStringList paths);
    void CountTree(QDir &);

    //! The latest state from all clients.
    ScannerState m_state;
    //! Mutex protecting state
    QMutex m_mutexState;

    // Global working vars
    ImageDbWriter  m_db;
    ImageSg       *m_sg;

    //! Maps dir paths (relative to SG) to dir metadata
    ImageMap *m_dbDirMap;
    //! Maps file paths (relative to SG) to file metadata
    ImageMap *m_dbFileMap;

    //! Number of images scanned
    int  m_progressCount;
    //! Total number of images to scan
    int  m_progressTotalCount;
    //! Progress counts mutex
    QMutex m_mutexProgress;

    //! Global working dir for file detection
    QDir m_dir;
    //! Pattern of dir names to ignore whilst scanning
    QRegExp m_exclusions;
};


//! Synchronises database to the filesystem
class META_PUBLIC ImageScan
{
public:
    static ImageScan*    getInstance();

    QStringList HandleScanRequest(QStringList);

private:
    ImageScan();
    ~ImageScan();

    //! Scanner singleton
    static ImageScan    *m_instance;
    //! Internal thread
    ImageScanThread     *m_imageScanThread;
};

#endif // IMAGESCAN_H
