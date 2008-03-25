#include <set>
#include <map>

#include <QImage>
#include <QImageReader>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

#include "globals.h"
#include "videoscan.h"
#include "metadata.h"

#include "metadatalistmanager.h"
#include "dbaccess.h"
#include "dirscan.h"

class VideoScannerImp
{
  public:
    VideoScannerImp();
    ~VideoScannerImp();
    void doScan(const QStringList &dirs);

  private:
    typedef std::vector<std::pair<unsigned int, QString> > PurgeList;
    typedef std::map<QString, bool> FileCheckList;

  private:
    bool m_ListUnknown;
    bool m_RemoveAll;
    bool m_KeepAll;

    MetadataListManager *m_dbmetadata;

  private:
    void promptForRemoval(unsigned int id, const QString &filename);
    void verifyFiles(FileCheckList &files, PurgeList &remove);
    void updateDB(const FileCheckList &add, const PurgeList &remove);
    void buildFileList(const QString &directory,
                       const QStringList &imageExtensions,
                       FileCheckList &filelist);
};

VideoScanner::VideoScanner()
{
    m_imp = new VideoScannerImp();
}

VideoScanner::~VideoScanner()
{
    delete m_imp;
}

void VideoScanner::doScan(const QStringList &dirs)
{
    m_imp->doScan(dirs);
}

VideoScannerImp::VideoScannerImp() : m_RemoveAll(false), m_KeepAll(false)
{
    m_dbmetadata = new MetadataListManager;
    MetadataListManager::metadata_list ml;
    MetadataListManager::loadAllFromDatabase(ml);
    m_dbmetadata->setList(ml);

    m_ListUnknown = gContext->GetNumSetting("VideoListUnknownFileTypes", 1);
}

VideoScannerImp::~VideoScannerImp()
{
    delete m_dbmetadata;
}

void VideoScannerImp::doScan(const QStringList &dirs)
{
    MythProgressDialog *progressDlg =
        new MythProgressDialog(QObject::tr("Searching for video files"),
                               dirs.size());

    QList<QByteArray> image_types = QImageReader::supportedImageFormats();
    QStringList imageExtensions;
    for (QList<QByteArray>::const_iterator p = image_types.begin();
         p != image_types.end(); ++p)
    {
        imageExtensions.push_back(QString(*p));
    }
    int counter = 0;

    FileCheckList fs_files;

    for (QStringList::const_iterator iter = dirs.begin(); iter != dirs.end();
         ++iter)
    {
        buildFileList(*iter, imageExtensions, fs_files);
        progressDlg->setProgress(++counter);
    }

    progressDlg->close();
    progressDlg->deleteLater();

    PurgeList db_remove;
    verifyFiles(fs_files, db_remove);
    updateDB(fs_files, db_remove);
}

void VideoScannerImp::promptForRemoval(unsigned int id, const QString &filename)
{
    // TODO: use single DB connection for all calls
    if (m_RemoveAll)
        m_dbmetadata->purgeByID(id);

    if (m_KeepAll || m_RemoveAll)
        return;

    QStringList buttonText;
    buttonText += QObject::tr("No");
    buttonText += QObject::tr("No to all");
    buttonText += QObject::tr("Yes");
    buttonText += QObject::tr("Yes to all");

    DialogCode result = MythPopupBox::ShowButtonPopup(
        gContext->GetMainWindow(),
        QObject::tr("File Missing"),
        QObject::tr("%1 appears to be missing.\n"
                    "Remove it from the database?").arg(filename),
        buttonText, kDialogCodeButton0);

    switch (result)
    {
        case kDialogCodeRejected:
        case kDialogCodeButton0:
        default:
            break;
        case kDialogCodeButton1:
            m_KeepAll = true;
            break;
        case kDialogCodeButton2:
            m_dbmetadata->purgeByID(id);
            break;
        case kDialogCodeButton3:
            m_RemoveAll = true;
            m_dbmetadata->purgeByID(id);
            break;
    };
}

void VideoScannerImp::updateDB(const FileCheckList &add,
                               const PurgeList &remove)
{
    int counter = 0;
    MythProgressDialog *progressDlg = new MythProgressDialog(
        QObject::tr("Updating video database"), add.size() + remove.size());

    for (FileCheckList::const_iterator p = add.begin(); p != add.end(); ++p)
    {
        // add files not already in the DB
        if (!p->second)
        {
            Metadata newFile(p->first, VIDEO_COVERFILE_DEFAULT,
                             Metadata::FilenameToTitle(p->first),
                             VIDEO_YEAR_DEFAULT,
                             VIDEO_INETREF_DEFAULT, VIDEO_DIRECTOR_DEFAULT,
                             VIDEO_PLOT_DEFAULT, 0.0, VIDEO_RATING_DEFAULT,
                             0, 0, ParentalLevel::plLowest);

            newFile.dumpToDatabase();
        }

        progressDlg->setProgress(++counter);
    }

    for (PurgeList::const_iterator p = remove.begin(); p != remove.end(); ++p)
    {
        promptForRemoval(p->first, p->second);

        progressDlg->setProgress(++counter);
    }

    progressDlg->Close();
    progressDlg->deleteLater();
}

void VideoScannerImp::verifyFiles(FileCheckList &files, PurgeList &remove)
{
    int counter = 0;
    FileCheckList::iterator iter;

    MythProgressDialog *progressDlg = new MythProgressDialog(
        QObject::tr("Verifying video files"), m_dbmetadata->getList().size());

    // For every file we know about, check to see if it still exists.
    for (MetadataListManager::metadata_list::const_iterator p =
         m_dbmetadata->getList().begin();
         p != m_dbmetadata->getList().end(); ++p)
    {
        QString name = (*p)->Filename();
        if (name != QString::null)
        {
            iter = files.find(name);
            if (iter != files.end())
            {
                // If it's both on disk and in the database we're done with it.
                iter->second = true;
            }
            else
            {
                // If it's only in the database mark it as such for removal
                // later
                remove.push_back(std::make_pair((*p)->ID(), name));
            }
        }

        progressDlg->setProgress(++counter);
    }

    progressDlg->Close();
    progressDlg->deleteLater();
}

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
                m_image_ext.insert((*p).lower());
            }
        }

        DirectoryHandler *newDir(const QString &dir_name,
                                 const QString &fq_dir_name)
        {
            (void)dir_name;
            (void)fq_dir_name;
            return this;
        }

        void handleFile(const QString &file_name,
                        const QString &fq_file_name,
                        const QString &extension)

        {
            (void)file_name;
            if (m_image_ext.find(extension.lower()) == m_image_ext.end())
                m_video_files[fq_file_name] = false;
        }

      private:
        typedef std::set<QString> image_ext;
        image_ext m_image_ext;
        DirListType &m_video_files;
    };
}

void VideoScannerImp::buildFileList(const QString &directory,
                                    const QStringList &imageExtensions,
                                    FileCheckList &filelist)
{
    FileAssociations::ext_ignore_list ext_list;
    FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

    dirhandler<FileCheckList> dh(filelist, imageExtensions);
    ScanVideoDirectory(directory, &dh, ext_list, m_ListUnknown);
}
