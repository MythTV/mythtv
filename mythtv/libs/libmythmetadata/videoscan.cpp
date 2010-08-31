#include <set>
#include <map>
#include <vector>

#include <QImageReader>
#include <QApplication>
#include <QThread>
#include <QStringList>
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
		    //VERBOSE(VB_GENERAL,QString("handleFile: %1 :: %2").arg(fq_file_name).arg(host));
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

class VideoScannerThread : public QThread
{
    Q_OBJECT
  public:
    VideoScannerThread() : m_RemoveAll(false), m_KeepAll(false),
        m_DBDataChanged(false)
    {
        m_dbmetadata = new VideoMetadataListManager;
        VideoMetadataListManager::metadata_list ml;
        VideoMetadataListManager::loadAllFromDatabase(ml);
        m_dbmetadata->setList(ml);

        m_ListUnknown = gCoreContext->GetNumSetting("VideoListUnknownFiletypes", 0);
    }

    ~VideoScannerThread()
    {
        delete m_dbmetadata;
    }

    void run()
    {
        QList<QByteArray> image_types = QImageReader::supportedImageFormats();
        QStringList imageExtensions;
        for (QList<QByteArray>::const_iterator p = image_types.begin();
             p != image_types.end(); ++p)
        {
            imageExtensions.push_back(QString(*p));
        }

        uint counter = 0;
        FileCheckList fs_files;
        failedSGHosts.clear();

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
                    QString host = sgurl.host();
                    QString path = sgurl.path();

                    failedSGHosts.append(host);

                    VERBOSE(VB_GENERAL, QString("Failed to scan :%1:").arg(*iter));
                }
            }

            SendProgressEvent(++counter);
        }

        PurgeList db_remove;
        verifyFiles(fs_files, db_remove);
        m_DBDataChanged = updateDB(fs_files, db_remove);
    }

    void SetDirs(const QStringList &dirs)
    {
        m_directories = dirs;
    }

    void SetProgressDialog(MythUIProgressDialog *dialog)
    {
        m_dialog = dialog;
    }

    QStringList GetFailedSGHosts(void)
    {
        return failedSGHosts;
    }

    bool getDataChanged()
    {
        return m_DBDataChanged;
    }

  private:

    struct CheckStruct
    {
        bool check;
        QString host;
    };

    typedef std::vector<std::pair<unsigned int, QString> > PurgeList;
    typedef std::map<QString, CheckStruct> FileCheckList;

    void removeOrphans(unsigned int id, const QString &filename)
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

    void verifyFiles(FileCheckList &files, PurgeList &remove)
    {
        int counter = 0;
        FileCheckList::iterator iter;

        SendProgressEvent(counter, (uint)m_dbmetadata->getList().size(),
                          tr("Verifying video files"));

        // For every file we know about, check to see if it still exists.
        for (VideoMetadataListManager::metadata_list::const_iterator p =
             m_dbmetadata->getList().begin();
             p != m_dbmetadata->getList().end(); ++p)
        {
            QString lname = (*p)->GetFilename();
            QString lhost = (*p)->GetHost();
            if (lname != QString::null)
            {
                iter = files.find(lname);
                if (iter != files.end())
                {
                    // If it's both on disk and in the database we're done with
                    // it.
                    iter->second.check= true;
                }
                else
                {
                    // If it's only in the database, and not on a host we cannot reach,
                    //  mark it as for removal later.
                    if (lhost == "")
                    {
                        remove.push_back(std::make_pair((*p)->GetID(), lname));
                    }
                    else
                    {
                        if (!failedSGHosts.contains(lhost))
                        {
                            VERBOSE(VB_GENERAL, QString("Removing file SG(%1) :%2: ").arg(lhost).arg(lname));
                            remove.push_back(std::make_pair((*p)->GetID(), lname));
                        }
                        else
                            VERBOSE(VB_GENERAL, QString("SG(%1) not available. Not removing file :%2: ").arg(lhost).arg(lname));
                    }
                }
            }

            SendProgressEvent(++counter);
        }
    }

    // Returns true if DB changes were made (additions or deletions).
    bool updateDB(const FileCheckList &add, const PurgeList &remove)
    {
        int ret = 0;
        uint counter = 0;
        QList<int> preservelist;
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
                        VERBOSE(VB_IMPORTANT, QString("Hash %1 already exists in the "
                                                      "database, updating record %2 "
                                                      "with new filename %3")
                                                      .arg(hash).arg(id).arg(p->first));
                        preservelist.append(id);
                    }
                }
                if (id == -1)
                {
                    VideoMetadata newFile(p->first, hash,
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
                                     VIDEO_INETREF_DEFAULT, QString(),
                                     VIDEO_DIRECTOR_DEFAULT, VIDEO_PLOT_DEFAULT,
                                     0.0, VIDEO_RATING_DEFAULT, 0,
                                     VideoMetadata::FilenameToMeta(p->first, 2).toInt(), 
                                     VideoMetadata::FilenameToMeta(p->first, 3).toInt(), 
                                     QDate::currentDate(),
                                     0, ParentalLevel::plLowest);

                    VERBOSE(VB_GENERAL, QString("Adding : %1 : %2 : %3")
                            .arg(newFile.GetHost()).arg(newFile.GetFilename())
                            .arg(hash));
                    newFile.SetHost(p->second.host);
                    newFile.SaveToDatabase();
                }
                ret += 1;
            }
            SendProgressEvent(++counter);
        }

        // When prompting is restored, account for the answer here.
        ret += remove.size();
        for (PurgeList::const_iterator p = remove.begin(); p != remove.end();
                ++p)
        {
            if (!preservelist.contains(p->first))
                removeOrphans(p->first, p->second);
            SendProgressEvent(++counter);
        }

        return ret;
    }

    bool buildFileList(const QString &directory,
                                        const QStringList &imageExtensions,
                                        FileCheckList &filelist)
    {
        VERBOSE(VB_GENERAL, QString("buildFileList directory = %1").arg(directory));
        FileAssociations::ext_ignore_list ext_list;
        FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

        dirhandler<FileCheckList> dh(filelist, imageExtensions);
        return ScanVideoDirectory(directory, &dh, ext_list, m_ListUnknown);
    }

    void SendProgressEvent(uint progress, uint total = 0,
            QString messsage = QString())
    {
        if (!m_dialog)
            return;

        ProgressUpdateEvent *pue = new ProgressUpdateEvent(progress, total,
                                                           messsage);
        QApplication::postEvent(m_dialog, pue);
    }


    bool m_ListUnknown;
    bool m_RemoveAll;
    bool m_KeepAll;
    QStringList m_directories;
    QStringList failedSGHosts;

    VideoMetadataListManager *m_dbmetadata;
    MythUIProgressDialog *m_dialog;

    bool m_DBDataChanged;
};

VideoScanner::VideoScanner()
{
    m_scanThread = new VideoScannerThread();
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

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIProgressDialog *progressDlg = new MythUIProgressDialog("",
            popupStack, "videoscanprogressdialog");

    if (progressDlg->Create())
    {
        popupStack->AddScreen(progressDlg, false);
        connect(m_scanThread, SIGNAL(finished()),
                progressDlg, SLOT(Close()));
        connect(m_scanThread, SIGNAL(finished()),
                SLOT(finishedScan()));
    }
    else
    {
        delete progressDlg;
        progressDlg = NULL;
    }

    m_scanThread->SetDirs(dirs);
    m_scanThread->SetProgressDialog(progressDlg);
    m_scanThread->start();
}

void VideoScanner::doScanAll()
{
    doScan(GetVideoDirs());
}

void VideoScanner::finishedScan()
{
    QStringList failedHosts = m_scanThread->GetFailedSGHosts();
    if (failedHosts.size() > 0)
    {
        QString msg = tr("Failed to Scan SG Video Hosts") + ":\n\n";

        for (int i = 0; i < failedHosts.size(); ++i)
            msg += " " + failedHosts.at(i);

        msg += "\n" + tr("If they no longer exist please remove them") + "\n\n";

        ShowOkPopup(msg);
    }

    emit finished(m_scanThread->getDataChanged());
}

////////////////////////////////////////////////////////////////////////

#include "videoscan.moc"
