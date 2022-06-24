//! \file
//! \brief Synchronises image database to filesystem
//! \details Detects supported pictures and videos and populates
//! the image database with metadata for each, including directory structure.
//! All images are passed to the associated thumbnail generator.
//! Db images that have disappeared are notified to frontends so that they can clean up.
//! Also clears database & removes devices (to prevent contention with running scans).
//!
//! Clone directories & duplicate files can only occur in a Storage Group/Backend scanner.
//! They can never occur with local devices/Frontend scanner

#ifndef IMAGESCANNER_H
#define IMAGESCANNER_H

#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QRegularExpression>

#include "imagethumbs.h"


//! Image Scanner thread requires a database/filesystem adapter
template <class DBFS>
class META_PUBLIC ImageScanThread : public MThread
{
public:
    ImageScanThread(DBFS *dbfs, ImageThumb<DBFS> *thumbGen);
    ~ImageScanThread() override;

    void        cancel();
    bool        IsScanning();
    bool        ClearsPending();
    void        ChangeState(bool scan);
    void        EnqueueClear(int devId, const QString &action);
    QStringList GetProgress();

protected:
    void run() override; // MThread

private:
    Q_DISABLE_COPY(ImageScanThread)

    void SyncSubTree(const QFileInfo &dirInfo, int parentId, int devId,
                     const QString &base);
    int  SyncDirectory(const QFileInfo &dirInfo, int devId,
                       const QString &base, int parentId);
    void PopulateMetadata(const QString &path, int type, QString &comment,
                          std::chrono::seconds &time,
                          int &orientation);
    void SyncFile(const QFileInfo &fileInfo, int devId,
                  const QString &base, int parentId);
    void CountTree(QDir &dir);
    void CountFiles(const QStringList &paths);
    void Broadcast(int progress);

    using ClearTask = QPair<int, QString>;

    bool              m_scanning {false}; //!< The requested scan state
    QMutex            m_mutexState; //!< Mutex protecting scan state
    QList<ClearTask>  m_clearQueue; //!< Queue of pending Clear requests
    QMutex            m_mutexQueue; //!< Mutex protecting Clear requests
    DBFS             &m_dbfs;       //!< Database/filesystem adapter
    ImageThumb<DBFS> &m_thumb;      //!< Companion thumbnail generator

    //! Dirs in the Db from last scan, Map<Db filepath, Db Image>
    ImageHash   m_dbDirMap;
    //! Files in the Db from last scan, Map<Db filepath, Db Image>
    ImageHash   m_dbFileMap;
    //! Dirs seen by current scan, Map<Db filepath, Earlier Image>
    ImageHash   m_seenDir;
    //! Files seen by current scan Map <Db filepath, Earlier abs filepath>
    NameHash    m_seenFile;
    //! Ids of dirs/files that have been updates/modified.
    QStringList m_changedImages;

    //! Elapsed time since last progress event generated
    QElapsedTimer m_bcastTimer;
    int           m_progressCount      {0}; //!< Number of images scanned
    int           m_progressTotalCount {0}; //!< Total number of images to scan
    QMutex        m_mutexProgress;      //!< Progress counts mutex

    //! Global working dir for file detection
    QDir m_dir;
    //! Pattern of dir names to ignore whilst scanning
    QRegularExpression m_exclusions;
};

#endif // IMAGESCANNER_H
