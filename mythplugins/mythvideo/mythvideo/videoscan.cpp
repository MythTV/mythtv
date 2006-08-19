#include <qimage.h>

#include <set>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

#include "globals.h"
#include "videoscan.h"
#include "metadata.h"

#include "metadatalistmanager.h"
#include "dbaccess.h"
#include "dirscan.h"

VideoScanner::VideoScanner()
{
    m_dbmetadata = new MetadataListManager;
    MetadataListManager::metadata_list ml;
    MetadataListManager::loadAllFromDatabase(ml);
    m_dbmetadata->setList(ml);

    m_RemoveAll = false;
    m_KeepAll = false;
    m_ListUnknown = gContext->GetNumSetting("VideoListUnknownFileTypes", 1);
}

VideoScanner::~VideoScanner()
{
    delete m_dbmetadata;
}

void VideoScanner::doScan(const QStringList &dirs)
{
    MythProgressDialog progressDlg(QObject::tr("Searching for video files"),
                                   dirs.size());

    QStringList imageExtensions = QImage::inputFormatList();
    int counter = 0;

    for (QStringList::const_iterator iter = dirs.begin(); iter != dirs.end();
         ++iter)
    {
        buildFileList(*iter, imageExtensions);
        progressDlg.setProgress(++counter);
    }

    progressDlg.close();
    verifyFiles();
    updateDB();
}

void VideoScanner::promptForRemoval(const QString &filename)
{
    // TODO: use single DB connection for all calls
    if (m_RemoveAll)
        m_dbmetadata->purgeByFilename(filename);

    if (m_KeepAll || m_RemoveAll)
        return;

    QStringList buttonText;
    buttonText += QObject::tr("No");
    buttonText += QObject::tr("No to all");
    buttonText += QObject::tr("Yes");
    buttonText += QObject::tr("Yes to all");

    int result = MythPopupBox::showButtonPopup(gContext->GetMainWindow(),
            QObject::tr("File Missing"),
            QString(QObject::tr("%1 appears to be missing.\nRemove it "
                                "from the database?")).arg(filename),
            buttonText, 1);
    switch (result)
    {
        case 1:
            m_KeepAll = true;
            break;
        case 2:
            m_dbmetadata->purgeByFilename(filename);
            break;
        case 3:
            m_RemoveAll = true;
            m_dbmetadata->purgeByFilename(filename);
            break;
    };
}


void VideoScanner::updateDB()
{
    int counter = 0;
    MythProgressDialog progressDlg(QObject::tr("Updating video database"),
                                   m_VideoFiles.size());
    VideoLoadedMap::Iterator iter;

    for (iter = m_VideoFiles.begin(); iter != m_VideoFiles.end(); iter++)
    {
        if (*iter == kFileSystem)
        {
            Metadata newFile(iter.key(), VIDEO_COVERFILE_DEFAULT,
                             Metadata::FilenameToTitle(iter.key()),
                             VIDEO_YEAR_DEFAULT,
                             VIDEO_INETREF_DEFAULT, VIDEO_DIRECTOR_DEFAULT,
                             VIDEO_PLOT_DEFAULT, 0.0, VIDEO_RATING_DEFAULT,
                             0, 0, 1);

            newFile.dumpToDatabase();
        }

        if (*iter == kDatabase)
        {
            promptForRemoval(iter.key());
        }

        progressDlg.setProgress(++counter);
    }

    progressDlg.Close();
}

void VideoScanner::verifyFiles()
{
    int counter = 0;
    VideoLoadedMap::Iterator iter;

    MythProgressDialog progressDlg(QObject::tr("Verifying video files"),
                                   m_dbmetadata->getList().size());

    // For every file we know about, check to see if it still exists.
    for (MetadataListManager::metadata_list::const_iterator p =
         m_dbmetadata->getList().begin();
         p != m_dbmetadata->getList().end(); ++p)
    {
        QString name = (*p)->Filename();
        if (name != QString::null)
        {
            if ((iter = m_VideoFiles.find(name)) != m_VideoFiles.end())
            {
                // If it's both on disk and in the database we're done with it.
                m_VideoFiles.remove(iter);
            }
            else
            {
                // If it's only in the database mark it as such for removal
                // later
                m_VideoFiles[name] = kDatabase;
            }
        }
        progressDlg.setProgress(++counter);
    }

    progressDlg.Close();
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
                m_video_files[fq_file_name] = kFileSystem;
        }

      private:
        typedef std::set<QString> image_ext;
        image_ext m_image_ext;
		DirListType &m_video_files;
    };
}

void VideoScanner::buildFileList(const QString &directory,
                                 const QStringList &imageExtensions)
{
    FileAssociations::ext_ignore_list ext_list;
    FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

    dirhandler<VideoLoadedMap> dh(m_VideoFiles, imageExtensions);
    ScanVideoDirectory(directory, &dh, ext_list, m_ListUnknown);
}
