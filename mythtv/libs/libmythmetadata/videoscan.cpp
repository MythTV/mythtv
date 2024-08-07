
#include "videoscan.h"

#include <QApplication>
#include <QImageReader>
#include <QUrl>
#include <utility>

// mythtv
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remoteutil.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythscreenstack.h"

// libmythmetadata
#include "videometadatalistmanager.h"
#include "videoutils.h"
#include "globals.h"
#include "dbaccess.h"
#include "dirscan.h"

const QEvent::Type VideoScanChanges::kEventType =
    (QEvent::Type) QEvent::registerEventType();

namespace
{
    template <typename DirListType>
    class dirhandler : public DirectoryHandler
    {
      public:
        dirhandler(DirListType &video_files,
                   const QStringList &image_extensions) :
            m_videoFiles(video_files)
        {
            for (const auto& ext : std::as_const(image_extensions))
                m_imageExt.insert(ext.toLower());
        }

        DirectoryHandler *newDir([[maybe_unused]] const QString &dir_name,
                                 [[maybe_unused]] const QString &fq_dir_name) override // DirectoryHandler
        {
            return this;
        }

        void handleFile([[maybe_unused]] const QString &file_name,
                        const QString &fq_file_name,
                        const QString &extension,
                        const QString &host) override // DirectoryHandler

        {
#if 0
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("handleFile: %1 :: %2").arg(fq_file_name).arg(host));
#endif
            if (m_imageExt.find(extension.toLower()) == m_imageExt.end())
            {
                m_videoFiles[fq_file_name].check = false;
                m_videoFiles[fq_file_name].host = host;
            }
        }

      private:
        using image_ext = std::set<QString>;
        image_ext    m_imageExt;
        DirListType &m_videoFiles;
    };
}

class VideoMetadataListManager;
class MythUIProgressDialog;

VideoScannerThread::VideoScannerThread(QObject *parent) :
    MThread("VideoScanner"),
    m_parent(parent),
    m_hasGUI(gCoreContext->HasGUI()),
    m_dbMetadata(new VideoMetadataListManager)
{
    m_listUnknown = gCoreContext->GetBoolSetting("VideoListUnknownFiletypes", false);
}

VideoScannerThread::~VideoScannerThread()
{
    delete m_dbMetadata;
}

void VideoScannerThread::SetHosts(const QStringList &hosts)
{
    m_liveSGHosts.clear();
    for (const auto& host : std::as_const(hosts))
        m_liveSGHosts << host.toLower();
}

void VideoScannerThread::SetDirs(QStringList dirs)
{
    QString master = gCoreContext->GetMasterHostName().toLower();
    QStringList searchhosts;
    QStringList mdirs;
    m_offlineSGHosts.clear();

    QStringList::iterator iter = dirs.begin();
    QStringList::iterator iter2;
    while ( iter != dirs.end() )
    {
        if (iter->startsWith("myth://"))
        {
            QUrl sgurl = *iter;
            QString host = sgurl.host().toLower();
            QString path = sgurl.path();

            if (!m_liveSGHosts.contains(host))
            {
                // mark host as offline to warn user
                if (!m_offlineSGHosts.contains(host))
                    m_offlineSGHosts.append(host);
                // erase from directory list to skip scanning
                iter = dirs.erase(iter);
                continue;
            }
            if ((host == master) &&  (!mdirs.contains(path)))
            {
                // collect paths defined on master backend so other
                // online backends can be set to fall through to them
                mdirs.append(path);
            }
            else if (!searchhosts.contains(host))
            {
                // mark host as having directories defined so it
                // does not fall through to those on the master
                searchhosts.append(host);
            }
        }

        ++iter;
    }

    for (iter = m_liveSGHosts.begin(); iter != m_liveSGHosts.end(); ++iter)
    {
        if ((!searchhosts.contains(*iter)) && (master != *iter))
        {
            for (iter2 = mdirs.begin(); iter2 != mdirs.end(); ++iter2)
            {
                // backend is online, but has no directories listed
                // fall back to those on the master backend
                dirs.append(MythCoreContext::GenMythURL(*iter,
                                                        0, *iter2, "Videos"));
            }
        }
    }

    m_directories = dirs;
}

void VideoScannerThread::run()
{
    RunProlog();

    VideoMetadataListManager::metadata_list ml;
    VideoMetadataListManager::loadAllFromDatabase(ml);
    m_dbMetadata->setList(ml);

    QList<QByteArray> image_types = QImageReader::supportedImageFormats();
    QStringList imageExtensions;
    for (const auto & format : std::as_const(image_types))
        imageExtensions.push_back(QString(format));

    LOG(VB_GENERAL, LOG_INFO, QString("Beginning Video Scan."));

    uint counter = 0;
    FileCheckList fs_files;

    if (m_hasGUI)
        SendProgressEvent(counter, (uint)m_directories.size(),
                          tr("Searching for video files"));
    for (const auto & dir : std::as_const(m_directories))
    {
        if (!buildFileList(dir, imageExtensions, fs_files))
        {
            if (dir.startsWith("myth://"))
            {
                QUrl sgurl = dir;
                QString host = sgurl.host().toLower();

                m_liveSGHosts.removeAll(host);

                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to scan :%1:").arg(dir));
            }
        }
        if (m_hasGUI)
            SendProgressEvent(++counter);
    }

    PurgeList db_remove;
    verifyFiles(fs_files, db_remove);
    m_dbDataChanged = updateDB(fs_files, db_remove);

    if (m_dbDataChanged)
    {
        QCoreApplication::postEvent(m_parent,
            new VideoScanChanges(m_addList, m_movList,
                                 m_delList));

        QStringList slist;

        for (int id : std::as_const(m_addList))
            slist << QString("added::%1").arg(id);
        for (int id : std::as_const(m_movList))
            slist << QString("moved::%1").arg(id);
        for (int id : std::as_const(m_delList))
            slist << QString("deleted::%1").arg(id);

        MythEvent me("VIDEO_LIST_CHANGE", slist);

        gCoreContext->SendEvent(me);
    }
    else
    {
        gCoreContext->SendMessage("VIDEO_LIST_NO_CHANGE");
    }

    RunEpilog();
}


void VideoScannerThread::removeOrphans(unsigned int id,
                                       [[maybe_unused]] const QString &filename)
{
    // TODO: use single DB connection for all calls
    if (m_removeAll)
        m_dbMetadata->purgeByID(id);

    if (!m_keepAll && !m_removeAll)
    {
        m_removeAll = true;
        m_dbMetadata->purgeByID(id);
    }
}

void VideoScannerThread::verifyFiles(FileCheckList &files,
                                     PurgeList &remove)
{
    int counter = 0;
    FileCheckList::iterator iter;

    if (m_hasGUI)
        SendProgressEvent(counter, (uint)m_dbMetadata->getList().size(),
                          tr("Verifying video files"));

    // For every file we know about, check to see if it still exists.
    for (const auto & file : m_dbMetadata->getList())
    {
        QString lname = file->GetFilename();
        QString lhost = file->GetHost().toLower();
        if (!lname.isEmpty())
        {
            iter = files.find(lname);
            if (iter != files.end())
            {
                if (lhost != iter->second.host)
                {
                    // file has changed hosts
                    // add to delete list for further processing
                    remove.emplace_back(file->GetID(), lname);
                }
                else
                {
                    // file is on disk on the proper host and in the database
                    // we're done with it
                    iter->second.check = true;
                }
            }
            else if (lhost.isEmpty())
            {
                // If it's only in the database, and not on a host we
                // cannot reach, mark it as for removal later.
                remove.emplace_back(file->GetID(), lname);
            }
            else if (m_liveSGHosts.contains(lhost))
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Removing file SG(%1) :%2:")
                        .arg(lhost, lname));
                remove.emplace_back(file->GetID(), lname);
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("SG(%1) not available. Not removing file :%2:")
                        .arg(lhost, lname));
                if (!m_offlineSGHosts.contains(lhost))
                    m_offlineSGHosts.append(lhost);
            }
        }
        if (m_hasGUI)
            SendProgressEvent(++counter);
    }
}

bool VideoScannerThread::updateDB(const FileCheckList &add, const PurgeList &remove)
{
    int ret = 0;
    uint counter = 0;
    if (m_hasGUI)
        SendProgressEvent(counter, (uint)(add.size() + remove.size()),
                          tr("Updating video database"));

    for (auto p = add.cbegin(); p != add.cend(); ++p)
    {
        // add files not already in the DB
        if (!p->second.check)
        {
            int id = -1;

            // Are we sure this needs adding?  Let's check our Hash list.
            QString hash = VideoMetadata::VideoFileHash(p->first, p->second.host);
            if (hash != "NULL" && !hash.isEmpty())
            {
                id = VideoMetadata::UpdateHashedDBRecord(hash, p->first, p->second.host);
                if (id != -1)
                {
                    // Whew, that was close.  Let's remove that thing from
                    // our purge list, too.
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Hash %1 already exists in the "
                                "database, updating record %2 "
                                "with new filename %3")
                            .arg(hash).arg(id).arg(p->first));
                    m_movList.append(id);
                }
            }
            if (id == -1)
            {
                VideoMetadata newFile(
                    p->first, QString(), hash,
                    VIDEO_TRAILER_DEFAULT,
                    VIDEO_COVERFILE_DEFAULT,
                    VIDEO_SCREENSHOT_DEFAULT,
                    VIDEO_BANNER_DEFAULT,
                    VIDEO_FANART_DEFAULT,
                    QString(), QString(), QString(), QString(),
                    QString(),
                    VIDEO_YEAR_DEFAULT,
                    QDate::fromString("0000-00-00","YYYY-MM-DD"),
                    VIDEO_INETREF_DEFAULT, 0, QString(),
                    VIDEO_DIRECTOR_DEFAULT, QString(), VIDEO_PLOT_DEFAULT,
                    0.0, VIDEO_RATING_DEFAULT, 0, 0,
                    0, 0,
                    MythDate::current().date(),
                    0, ParentalLevel::plLowest);

                LOG(VB_GENERAL, LOG_INFO, QString("Adding : %1 : %2 : %3")
                    .arg(newFile.GetHost(), newFile.GetFilename(), hash));
                newFile.SetHost(p->second.host);
                newFile.SaveToDatabase();
                m_addList << newFile.GetID();
            }
            ret += 1;
        }
        if (m_hasGUI)
            SendProgressEvent(++counter);
    }

    // When prompting is restored, account for the answer here.
    ret += remove.size();
    for (const auto & item : remove)
    {
        if (!m_movList.contains(item.first))
        {
            removeOrphans(item.first, item.second);
            m_delList << item.first;
        }
        if (m_hasGUI)
            SendProgressEvent(++counter);
    }

    return ret > 0;
}

bool VideoScannerThread::buildFileList(const QString &directory,
                                       const QStringList &imageExtensions,
                                       FileCheckList &filelist) const
{
    // TODO: FileCheckList is a std::map, keyed off the filename. In the event
    // multiple backends have access to shared storage, the potential exists
    // for files to be scanned onto the wrong host. Add in some logic to prefer
    // the backend with the content stored in a storage group determined to be
    // local.

    LOG(VB_GENERAL,LOG_INFO, QString("buildFileList directory = %1")
                                 .arg(directory));
    FileAssociations::ext_ignore_list ext_list;
    FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

    dirhandler<FileCheckList> dh(filelist, imageExtensions);
    return ScanVideoDirectory(directory, &dh, ext_list, m_listUnknown);
}

void VideoScannerThread::SendProgressEvent(uint progress, uint total,
                                           QString messsage)
{
    if (!m_dialog)
        return;

    auto *pue = new ProgressUpdateEvent(progress, total, std::move(messsage));
    QApplication::postEvent(m_dialog, pue);
}

VideoScanner::VideoScanner()
  : m_scanThread(new VideoScannerThread(this))
{
}

VideoScanner::~VideoScanner()
{
    if (m_scanThread && m_scanThread->wait())
        delete m_scanThread;
}

void VideoScanner::doScan(const QStringList &dirs)
{
    if (m_scanThread->isRunning())
        return;

    if (gCoreContext->HasGUI())
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        auto *progressDlg = new MythUIProgressDialog("", popupStack,
                                                     "videoscanprogressdialog");

        if (progressDlg->Create())
        {
            popupStack->AddScreen(progressDlg, false);
            connect(m_scanThread->qthread(), &QThread::finished,
                    progressDlg, &MythScreenType::Close);
            connect(m_scanThread->qthread(), &QThread::finished,
                    this, &VideoScanner::finishedScan);
        }
        else
        {
            delete progressDlg;
            progressDlg = nullptr;
        }
        m_scanThread->SetProgressDialog(progressDlg);
    }

    QStringList hosts;
    if (!RemoteGetActiveBackends(&hosts))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Could not retrieve list of "
                            "available backends.");
        hosts.clear();
    }
    m_scanThread->SetHosts(hosts);
    m_scanThread->SetDirs(dirs);
    m_scanThread->start();
}

void VideoScanner::doScanAll()
{
    doScan(GetVideoDirs());
}

void VideoScanner::finishedScan()
{
    QStringList failedHosts = m_scanThread->GetOfflineSGHosts();
    if (!failedHosts.empty())
    {
        QString hosts = failedHosts.join(" ");
        QString msg = tr("Failed to Scan SG Video Hosts:\n\n%1\n\n"
                         "If they no longer exist please remove them")
                        .arg(hosts);

        ShowOkPopup(msg);
    }

    emit finished(m_scanThread->getDataChanged());
}

////////////////////////////////////////////////////////////////////////
