#include <map>

#include <QDir>
#include <QUrl>

#include "mythcorecontext.h"
#include "dbaccess.h"
#include "dirscan.h"
#include "remoteutil.h"
#include "mythcontext.h"
#include "mythlogging.h"
#include "videoutils.h"
#include "storagegroup.h"

DirectoryHandler::~DirectoryHandler()
{
}

namespace
{
    class ext_lookup
    {
      private:
        typedef std::map<QString, bool> ext_map;
        ext_map m_extensions;
        bool m_list_unknown;

      public:
        ext_lookup(const FileAssociations::ext_ignore_list &ext_disposition,
                   bool list_unknown) : m_list_unknown(list_unknown)
        {
            for (FileAssociations::ext_ignore_list::const_iterator p =
                 ext_disposition.begin(); p != ext_disposition.end(); ++p)
            {
                m_extensions.insert(ext_map::value_type(p->first.toLower(),
                                                        p->second));
            }
        }

        bool extension_ignored(const QString &extension) const
        {
            ext_map::const_iterator p = m_extensions.find(extension.toLower());
            if (p != m_extensions.end())
                return p->second;
            return !m_list_unknown;
        }
    };

    bool scan_dir(const QString &start_path, DirectoryHandler *handler,
                  const ext_lookup &ext_settings)
    {
        QDir d(start_path);

        // Return a fail if directory doesn't exist.
        if (!d.exists())
            return false;
        
        d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        QFileInfoList list = d.entryInfoList();
        // An empty directory is fine
        if (list.isEmpty())
            return true;

        QDir dir_tester;

        for (QFileInfoList::iterator p = list.begin(); p != list.end(); ++p)
        {
            if (p->fileName() == "Thumbs.db")
                continue;

            if (!p->isDir() &&
                ext_settings.extension_ignored(p->suffix())) continue;

            bool add_as_file = true;

            if (p->isDir())
            {
                add_as_file = false;

                dir_tester.setPath(p->absoluteFilePath() + "/VIDEO_TS");
                QDir bd_dir_tester;
                bd_dir_tester.setPath(p->absoluteFilePath() + "/BDMV");
                if (dir_tester.exists() || bd_dir_tester.exists())
                {
                    add_as_file = true;
                }
                else
                {
#if 0
                    LOG(VB_GENERAL, LOG_DEBUG, 
                        QString(" -- Dir : %1").arg(p->absoluteFilePath()));
#endif
                    DirectoryHandler *dh =
                            handler->newDir(p->fileName(),
                                            p->absoluteFilePath());

                    // Since we are dealing with a subdirectory failure is fine,
                    // so we'll just ignore the failue and continue
                    (void) scan_dir(p->absoluteFilePath(), dh, ext_settings);
                }
            }

            if (add_as_file)
            {
#if 0
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString(" -- File : %1").arg(p->fileName()));
#endif
                handler->handleFile(p->fileName(), p->absoluteFilePath(),
                                    p->suffix(), "");
            }
        }

        return true;
    }

    bool scan_sg_dir(const QString &start_path, const QString &host,
                     const QString &base_path, DirectoryHandler *handler,
                     const ext_lookup &ext_settings, bool isMaster = false)
    {
        QString path = start_path;

        if (path.startsWith(base_path))
            path.remove(0, base_path.length());

        if (!path.endsWith("/"))
            path += "/";

        if (path == "/")
            path = "";

        QStringList list;
        bool ok = false;

        if (isMaster)
        {
            StorageGroup sg("Videos", host);
            list = sg.GetFileInfoList(start_path);
            ok = true;
        }
        else
            ok = RemoteGetFileList(host, start_path, &list, "Videos");

        if (!ok || (!list.isEmpty() && list.at(0).startsWith("SLAVE UNREACHABLE")))
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Backend : %1 : Is currently Unreachable. Skipping "
                        "this one.") .arg(host));
            return false;
        }

        if (list.isEmpty() || (list.at(0) == "EMPTY LIST"))
            return true;

        for (QStringList::iterator p = list.begin(); p != list.end(); ++p)
        {
            QStringList fInfo = p->split("::");
            QString type = fInfo.at(0);
            QString fileName = fInfo.at(1);

            if (type == "nop")
                continue;

            QFileInfo fi(fileName);

            if ((type != "dir") &&
                ext_settings.extension_ignored(fi.suffix())) continue;

            if (type == "dir" &&
                !fileName.endsWith("VIDEO_TS") &&
                !fileName.endsWith("BDMV"))
            {
#if 0
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString(" -- Dir : %1").arg(fileName));
#endif
                DirectoryHandler *dh =
                        handler->newDir(fileName,
                                        start_path);

                // Same as a normal scan_dir we don't care if we can't read
                // subdirectories so ignore the results and continue. As long
                // as we reached it once to make it this far than we know the 
                // SG/Path exists
                (void) scan_sg_dir(start_path + "/" + fileName, host, base_path,
                             dh, ext_settings, isMaster);
            }
            else
            {
                QString suffix;
                QString URL;

                if (fileName.endsWith("VIDEO_TS") || fileName.endsWith("BDMV"))
                {
                    if (path.startsWith("/"))
                        path = path.right(path.length() - 1);
                    if (path.endsWith("/"))
                        path = path.left(path.length() - 1);
                    QStringList upDirs = path.split("/");
                    if (upDirs.count() > 1)
                        fileName = upDirs.takeLast();
                    else
                        fileName = path;
                    suffix = "";
                    URL = path;
                }
                else
                {
                    suffix = fi.suffix();
                    URL = QString("%1/%2").arg(path).arg(fileName);
                }

                URL.replace("//","/");

                if (URL.startsWith("/"))
                    URL = URL.right(URL.length() - 1);
#if 0
                LOG(VB_GENERAL, LOG_GENERAL,
                    QString(" -- File Filename: %1 URL: %2 Suffix: %3 Host: %4")
                        .arg(fileName).arg(URL).arg(suffix).arg(QString(host)));
#endif
                handler->handleFile(fileName, URL, fi.suffix(), QString(host));
            }
        }

        return true;
    }
}

bool ScanVideoDirectory(const QString &start_path, DirectoryHandler *handler,
        const FileAssociations::ext_ignore_list &ext_disposition,
        bool list_unknown_extensions)
{
    ext_lookup extlookup(ext_disposition, list_unknown_extensions);

    bool pathScanned = true;

    if (!start_path.startsWith("myth://"))
    {
        LOG(VB_GENERAL, LOG_INFO, 
            QString("MythVideo::ScanVideoDirectory Scanning (%1)")
                .arg(start_path));

        if (!scan_dir(start_path, handler, extlookup))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythVideo::ScanVideoDirectory failed to scan %1")
                    .arg(start_path));
            pathScanned = false;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("MythVideo::ScanVideoDirectory Scanning Group (%1)")
                .arg(start_path));
        QUrl sgurl = start_path;
        QString host = sgurl.host();
        QString path = sgurl.path();

        if (!scan_sg_dir(path, host, path, handler, extlookup, 
                (gCoreContext->IsMasterHost(host) &&
                 (gCoreContext->GetHostName().toLower() == host.toLower()))))
        {
            LOG(VB_GENERAL, LOG_ERR, 
                QString("MythVideo::ScanVideoDirectory failed to scan %1 ")
                    .arg(host));
            pathScanned = false;
        }
    }

    return pathScanned;
}
