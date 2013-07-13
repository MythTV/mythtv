#include <QImageReader>
#include <QApplication>
#include <QUrl>

#include "mythcontext.h"
#include "mythscreenstack.h"
#include "mythprogressdialog.h"
#include "mythdialogbox.h"
#include "globals.h"
#include "dbaccess.h"
#include "dirscan.h"
#include "videometadatalistmanager.h"
#include "videoscan.h"
#include "videoutils.h"
#include "mythevent.h"
#include "remoteutil.h"
#include "mythlogging.h"
#include "mythdate.h"

QEvent::Type VideoScanChanges::kEventType =
    (QEvent::Type) QEvent::registerEventType();

namespace
{
    template <typename DirListType>
    class dirhandler : public DirectoryHandler
    {
      public:
        dirhandler(DirListType &video_files,
                   const QStringList &image_extensions) :
            m_video_files(video_files)
        {
            for (QStringList::const_iterator p = image_extensions.begin();
                 p != image_extensions.end(); ++p)
            {
                m_image_ext.insert((*p).toLower());
            }
        }

        DirectoryHandler *newDir(const QString &dir_name,
                                 const QString &fq_dir_name)
        {
            (void) dir_name;
            (void) fq_dir_name;
            return this;
        }

        void handleFile(const QString &file_name,
                        const QString &fq_file_name,
                        const QString &extension,
                        const QString &host)

        {
#if 0
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("handleFile: %1 :: %2").arg(fq_file_name).arg(host));
#endif
            (void) file_name;
            if (m_image_ext.find(extension.toLower()) == m_image_ext.end())
            {
                m_video_files[fq_file_name].check = false;
                m_video_files[fq_file_name].host = host;
            }
        }

      private:
        typedef std::set<QString> image_ext;
        image_ext m_image_ext;
        DirListType &m_video_files;
    };
}

class VideoMetadataListManager;
class MythUIProgressDialog;

VideoScannerThread::VideoScannerThread(QObject *parent) :
    MThread("VideoScanner"),
    m_RemoveAll(false), m_KeepAll(false), m_dialog(NULL),
    m_DBDataChanged(false)
{
    m_parent = parent;
    m_dbmetadata = new VideoMetadataListManager;
    m_HasGUI = gCoreContext->HasGUI();
    m_ListUnknown = gCoreContext->GetNumSetting("VideoListUnknownFiletypes", 0);
}

VideoScannerThread::~VideoScannerThread()
{
    delete m_dbmetadata;
}

void VideoScannerThread::SetHosts(const QStringList &hosts)
{
    m_liveSGHosts.clear();
    QStringList::const_iterator iter = hosts.begin();
    for (; iter != hosts.end(); ++iter)
        m_liveSGHosts << iter->toLower();
}

void VideoScannerThread::SetDirs(QStringList dirs)
{
    QString master = gCoreContext->GetMasterHostName().toLower();
    QStringList searchhosts, mdirs;
    m_offlineSGHosts.clear();

    QStringList::iterator iter = dirs.begin(), iter2;
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
            else if ((host == master) &&  (!mdirs.contains(path)))
                // collect paths defined on master backend so other
                // online backends can be set to fall through to them
                mdirs.append(path);
            else if (!searchhosts.contains(host))
                // mark host as having directories defined so it
                // does not fall through to those on the master
                searchhosts.append(host);
        }

        ++iter;
    }

    for (iter = m_liveSGHosts.begin(); iter != m_liveSGHosts.end(); ++iter)
        if ((!searchhosts.contains(*iter)) && (master != *iter))
            for (iter2 = mdirs.begin(); iter2 != mdirs.end(); ++iter2)
                // backend is online, but has no directories listed
                // fall back to those on the master backend
                dirs.append(gCoreContext->GenMythURL(*iter,
                                            0, *iter2, "Videos"));

    m_directories = dirs;
}

void VideoScannerThread::run()
{
    RunProlog();

    VideoMetadataListManager::metadata_list ml;
    VideoMetadataListManager::loadAllFromDatabase(ml);
    m_dbmetadata->setList(ml);

    QList<QByteArray> image_types = QImageReader::supportedImageFormats();
    QStringList imageExtensions;
    for (QList<QByteArray>::const_iterator p = image_types.begin();
         p != image_types.end(); ++p)
    {
        imageExtensions.push_back(QString(*p));
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Beginning Video Scan."));

    uint counter = 0;
    FileCheckList fs_files;

    if (m_HasGUI)
        SendProgressEvent(counter, (uint)m_directories.size(),
                          tr("Searching for video files"));
    for (QStringList::const_iterator iter = m_directories.begin();
         iter != m_directories.end(); ++iter)
    {
        if (!buildFileList(*iter, imageExtensions, fs_files))
        {
            if (iter->startsWith("myth://"))
            {
                QUrl sgurl = *iter;
                QString host = sgurl.host().toLower();
                QString path = sgurl.path();

                m_liveSGHosts.removeAll(host);

                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to scan :%1:").arg(*iter));
            }
        }
        if (m_HasGUI)
            SendProgressEvent(++counter);
    }

    PurgeList db_remove;
    verifyFiles(fs_files, db_remove);
    m_DBDataChanged = updateDB(fs_files, db_remove);

    if (m_DBDataChanged)
    {
        QCoreApplication::postEvent(m_parent,
            new VideoScanChanges(m_addList, m_movList,
                                 m_delList));

        QStringList slist;

        QList<int>::const_iterator i;
        for (i = m_addList.begin(); i != m_addList.end(); ++i)
            slist << QString("added::%1").arg(*i);
        for (i = m_movList.begin(); i != m_movList.end(); ++i)
            slist << QString("moved::%1").arg(*i);
        for (i = m_delList.begin(); i != m_delList.end(); ++i)
            slist << QString("deleted::%1").arg(*i);

        MythEvent me("VIDEO_LIST_CHANGE", slist);

        gCoreContext->SendEvent(me);
    }
    else
        gCoreContext->SendMessage("VIDEO_LIST_NO_CHANGE");

    RunEpilog();
}


void VideoScannerThread::removeOrphans(unsigned int id,
                                       const QString &filename)
{
    (void) filename;

    // TODO: use single DB connection for all calls
    if (m_RemoveAll)
        m_dbmetadata->purgeByID(id);

    if (!m_KeepAll && !m_RemoveAll)
    {
        m_RemoveAll = true;
        m_dbmetadata->purgeByID(id);
    }
}

void VideoScannerThread::verifyFiles(FileCheckList &files,
                                     PurgeList &remove)
{
    int counter = 0;
    FileCheckList::iterator iter;

    if (m_HasGUI)
        SendProgressEvent(counter, (uint)m_dbmetadata->getList().size(),
                          tr("Verifying video files"));

    // For every file we know about, check to see if it still exists.
    for (VideoMetadataListManager::metadata_list::const_iterator p =
         m_dbmetadata->getList().begin();
         p != m_dbmetadata->getList().end(); ++p)
    {
        QString lname = (*p)->GetFilename();
        QString lhost = (*p)->GetHost().toLower();
        if (lname != QString::null)
        {
            iter = files.find(lname);
            if (iter != files.end())
            {
                if (lhost != iter->second.host)
                    // file has changed hosts
                    // add to delete list for further processing
                    remove.push_back(std::make_pair((*p)->GetID(), lname));
                else
                    // file is on disk on the proper host and in the database
                    // we're done with it
                    iter->second.check = true;
            }
            else if (lhost.isEmpty())
            {
                // If it's only in the database, and not on a host we
                // cannot reach, mark it as for removal later.
                remove.push_back(std::make_pair((*p)->GetID(), lname));
            }
            else if (m_liveSGHosts.contains(lhost))
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Removing file SG(%1) :%2:")
                        .arg(lhost).arg(lname));
                remove.push_back(std::make_pair((*p)->GetID(), lname));
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("SG(%1) not available. Not removing file :%2:")
                        .arg(lhost).arg(lname));
                if (!m_offlineSGHosts.contains(lhost))
                    m_offlineSGHosts.append(lhost);
            }
        }
        if (m_HasGUI)
            SendProgressEvent(++counter);
    }
}

bool VideoScannerThread::updateDB(const FileCheckList &add, const PurgeList &remove)
{
    int ret = 0;
    uint counter = 0;
    if (m_HasGUI)
        SendProgressEvent(counter, (uint)(add.size() + remove.size()),
                          tr("Updating video database"));

    for (FileCheckList::const_iterator p = add.begin(); p != add.end(); ++p)
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
                    p->first, hash,
                    VIDEO_TRAILER_DEFAULT,
                    VIDEO_COVERFILE_DEFAULT,
                    VIDEO_SCREENSHOT_DEFAULT,
                    VIDEO_BANNER_DEFAULT,
                    VIDEO_FANART_DEFAULT,
                    VideoMetadata::FilenameToMeta(p->first, 1),
                    VideoMetadata::FilenameToMeta(p->first, 4),
                    QString(),
                    VIDEO_YEAR_DEFAULT,
                    QDate::fromString("0000-00-00","YYYY-MM-DD"),
                    VIDEO_INETREF_DEFAULT, 0, QString(),
                    VIDEO_DIRECTOR_DEFAULT, QString(), VIDEO_PLOT_DEFAULT,
                    0.0, VIDEO_RATING_DEFAULT, 0, 0,
                    VideoMetadata::FilenameToMeta(p->first, 2).toInt(),
                    VideoMetadata::FilenameToMeta(p->first, 3).toInt(),
                    MythDate::current().date(),
                    0, ParentalLevel::plLowest);

                LOG(VB_GENERAL, LOG_INFO, QString("Adding : %1 : %2 : %3")
                        .arg(newFile.GetHost()).arg(newFile.GetFilename())
                        .arg(hash));
                newFile.SetHost(p->second.host);
                newFile.SaveToDatabase();
                m_addList << newFile.GetID();
            }
            ret += 1;
        }
        if (m_HasGUI)
            SendProgressEvent(++counter);
    }

    // When prompting is restored, account for the answer here.
    ret += remove.size();
    for (PurgeList::const_iterator p = remove.begin(); p != remove.end();
            ++p)
    {
        if (!m_movList.contains(p->first))
        {
            removeOrphans(p->first, p->second);
            m_delList << p->first;
        }
        if (m_HasGUI)
            SendProgressEvent(++counter);
    }

    return ret;
}

bool VideoScannerThread::buildFileList(const QString &directory,
                                       const QStringList &imageExtensions,
                                       FileCheckList &filelist)
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
    return ScanVideoDirectory(directory, &dh, ext_list, m_ListUnknown);
}

void VideoScannerThread::SendProgressEvent(uint progress, uint total,
                                           QString messsage)
{
    if (!m_dialog)
        return;

    ProgressUpdateEvent *pue = new ProgressUpdateEvent(progress, total,
                                                       messsage);
    QApplication::postEvent(m_dialog, pue);
}

VideoScanner::VideoScanner() : m_cancel(false)
{
    m_scanThread = new VideoScannerThread(this);
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

        MythUIProgressDialog *progressDlg = new MythUIProgressDialog("",
                popupStack, "videoscanprogressdialog");

        if (progressDlg->Create())
        {
            popupStack->AddScreen(progressDlg, false);
            connect(m_scanThread->qthread(), SIGNAL(finished()),
                    progressDlg, SLOT(Close()));
            connect(m_scanThread->qthread(), SIGNAL(finished()),
                    SLOT(finishedScan()));
        }
        else
        {
            delete progressDlg;
            progressDlg = NULL;
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
    if (failedHosts.size() > 0)
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
