
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QString>
#include <QWriteLocker>
#include <QReadLocker>

#include "mythmiscutil.h"
#include "mythdb.h"
#include "ringbuffer.h"
#include "mythsocket.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "storagegroup.h"
#include "mythcorecontext.h"
#include "mythdownloadmanager.h"

#include "sockethandler/filetransfer.h"
#include "requesthandler/deletethread.h"
#include "requesthandler/fileserverhandler.h"
#include "requesthandler/fileserverutil.h"

DeleteThread *deletethread = NULL;

void FileServerHandler::connectionClosed(MythSocket *socket)
{
    // iterate through transfer list and close if
    // socket matches connected transfer
    {
        QWriteLocker wlock(&m_ftLock);
        QMap<int, FileTransfer*>::iterator i;
        for (i = m_ftMap.begin(); i != m_ftMap.end(); ++i)
        {
            if ((*i)->GetSocket() == socket)
            {
                (*i)->DecrRef();
                m_ftMap.remove(i.key());
                return;
            }
        }
    }

    // iterate through file server list and close 
    // if socket matched connected server
    {
        QWriteLocker wlock(&m_fsLock);
        QMap<QString, SocketHandler*>::iterator i;
        for (i = m_fsMap.begin(); i != m_fsMap.end(); ++i)
        {
            if ((*i)->GetSocket() == socket)
            {
                (*i)->DecrRef();
                m_fsMap.remove(i.key());
                return;
            }
        }
    }
}

QString FileServerHandler::LocalFilePath(const QUrl &url,
                                           const QString &wantgroup)
{
    QString lpath = url.path();

    if (lpath.section('/', -2, -2) == "channels")
    {
        // This must be an icon request. Check channel.icon to be safe.
        QString querytext;

        QString file = lpath.section('/', -1);
        lpath = "";

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT icon FROM channel WHERE icon LIKE :FILENAME ;");
        query.bindValue(":FILENAME", QString("%/") + file);

        if (query.exec() && query.next())
        {
            lpath = query.value(0).toString();
        }
        else
        {
            MythDB::DBError("Icon path", query);
        }
    }
    else
    {
        lpath = lpath.section('/', -1);

        QString fpath = lpath;
        if (fpath.endsWith(".png"))
            fpath = fpath.left(fpath.length() - 4);

        ProgramInfo pginfo(fpath);
        if (pginfo.GetChanID())
        {
            QString pburl = GetPlaybackURL(&pginfo);
            if (pburl.startsWith("/"))
            {
                lpath = pburl.section('/', 0, -2) + "/" + lpath;
                LOG(VB_FILE, LOG_INFO,
                    QString("Local file path: %1").arg(lpath));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                        QString("LocalFilePath unable to find local "
                                "path for '%1', found '%2' instead.")
                                .arg(lpath).arg(pburl));
                lpath = "";
            }
        }
        else if (!lpath.isEmpty())
        {
            // For securities sake, make sure filename is really the pathless.
            QString opath = lpath;
            StorageGroup sgroup;

            if (!wantgroup.isEmpty())
            {
                sgroup.Init(wantgroup);
                lpath = url.toString();
            }
            else
            {
                lpath = QFileInfo(lpath).fileName();
            }

            QString tmpFile = sgroup.FindFile(lpath);
            if (!tmpFile.isEmpty())
            {
                lpath = tmpFile;
                LOG(VB_FILE, LOG_INFO,
                        QString("LocalFilePath(%1 '%2'), found through "
                                "exhaustive search at '%3'")
                            .arg(url.toString()).arg(opath).arg(lpath));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("LocalFilePath unable to "
                                                 "find local path for '%1'.")
                                .arg(opath));
                lpath = "";
            }

        }
        else
        {
            lpath = "";
        }
    }

    return lpath;
}

void FileServerHandler::RunDeleteThread(void)
{
    if (deletethread != NULL)
    {
		if (deletethread->isRunning())
			return;

        delete deletethread;
        deletethread = NULL;
    }

    deletethread = new DeleteThread();
    deletethread->start();
}

bool FileServerHandler::HandleAnnounce(MythSocket *socket,
                  QStringList &commands, QStringList &slist)
{
    if (commands[1] == "FileServer")
    {
        if (slist.size() >= 3)
        {
            SocketHandler *handler =
                new SocketHandler(socket, m_parent, commands[2]);

            handler->BlockShutdown(true);
            handler->AllowStandardEvents(true);
            handler->AllowSystemEvents(true);

            handler->WriteStringList(QStringList("OK"));

            QWriteLocker wlock(&m_fsLock);
            m_fsMap.insert(commands[2], handler);
            m_parent->AddSocketHandler(handler);

            handler->DecrRef();
            
            return true;
        }
        return false;
    }

    if (commands[1] != "FileTransfer")
        return false;

    if (slist.size() < 3)
        return false;

    if ((commands.size() < 3) || (commands.size() > 6))
        return false;

    FileTransfer *ft    = NULL;
    QString hostname    = "";
    QString filename    = "";
    bool writemode      = false;
    bool usereadahead   = true;
    int timeout_ms      = 2000;
    switch (commands.size())
    {
      case 6:
        timeout_ms      = commands[5].toInt();
        [[clang::fallthrough]];
      case 5:
        usereadahead    = commands[4].toInt();
        [[clang::fallthrough]];
      case 4:
        writemode       = commands[3].toInt();
        [[clang::fallthrough]];
      default:
        hostname        = commands[2];
    }

    QStringList::const_iterator it = slist.begin();
    QUrl qurl           = *(++it);
    QString wantgroup   = *(++it);

    QStringList checkfiles;
    while (++it != slist.end())
        checkfiles += *(it);

    slist.clear();

    LOG(VB_GENERAL, LOG_DEBUG, "FileServerHandler::HandleAnnounce");
    LOG(VB_GENERAL, LOG_INFO, QString("adding: %1 as remote file transfer")
                            .arg(hostname));

    if (writemode)
    {
        if (wantgroup.isEmpty())
            wantgroup = "Default";

        StorageGroup sgroup(wantgroup, gCoreContext->GetHostName(), false);
        QString dir = sgroup.FindNextDirMostFree();
        if (dir.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Unable to determine directory "
                    "to write to in FileTransfer write command");

            slist << "ERROR" << "filetransfer_directory_not_found";
            socket->WriteStringList(slist);
            return true;
        }

        QString basename = qurl.path();
        if (basename.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("FileTransfer write "
                    "filename is empty in url '%1'.")
                    .arg(qurl.toString()));

            slist << "ERROR" << "filetransfer_filename_empty";
            socket->WriteStringList(slist);
            return true;
        }

        if ((basename.contains("/../")) ||
            (basename.startsWith("../")))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("FileTransfer write "
                    "filename '%1' does not pass sanity checks.")
                    .arg(basename));

            slist << "ERROR" << "filetransfer_filename_dangerous";
            socket->WriteStringList(slist);
            return true;
        }

        filename = dir + "/" + basename;
    }
    else
        filename = LocalFilePath(qurl, wantgroup);

    QFileInfo finfo(filename);
    if (finfo.isDir())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("FileTransfer filename "
                "'%1' is actually a directory, cannot transfer.")
                .arg(filename));

        slist << "ERROR" << "filetransfer_filename_is_a_directory";
        socket->WriteStringList(slist);
        return true;
    }

    if (writemode)
    {
        QString dirPath = finfo.absolutePath();
        QDir qdir(dirPath);
        if (!qdir.exists())
        {
            if (!qdir.mkpath(dirPath))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("FileTransfer "
                        "filename '%1' is in a subdirectory which does "
                        "not exist, but can not be created.")
                        .arg(filename));

                slist << "ERROR" << "filetransfer_unable_to_create_subdirectory";
                socket->WriteStringList(slist);
                return true;
            }
        }

        ft = new FileTransfer(filename, socket, m_parent, writemode);
    }
    else
        ft = new FileTransfer(filename, socket, m_parent, usereadahead, timeout_ms);

    ft->BlockShutdown(true);

    {
        QWriteLocker wlock(&m_ftLock);
        m_ftMap.insert(socket->GetSocketDescriptor(), ft);
    }

    slist << "OK"
          << QString::number(socket->GetSocketDescriptor())
          << QString::number(ft->GetFileSize());

    if (checkfiles.size())
    {
        QFileInfo fi(filename);
        QDir dir = fi.absoluteDir();
        for (it = checkfiles.begin(); it != checkfiles.end(); ++it)
        {
            if (dir.exists(*it) &&
                QFileInfo(dir, *it).size() >= kReadTestSize)
                    slist << *it;
        }
    }

    socket->WriteStringList(slist);
    m_parent->AddSocketHandler(ft);
    ft->DecrRef(); ft = NULL;

    return true;
}

void FileServerHandler::connectionAnnounced(MythSocket *socket,
                                QStringList &commands, QStringList &slist)
{
    if (commands[1] == "SlaveBackend")
    {
        // were not going to handle these, but we still want to track them
        // for commands that need access to these sockets
        if (slist.size() >= 3)
        {
            SocketHandler *handler = m_parent->GetConnectionBySocket(socket);
            if (handler == NULL)
                return;

            QWriteLocker wlock(&m_fsLock);
            m_fsMap.insert(commands[2], handler);
        }
    }

}

bool FileServerHandler::HandleQuery(SocketHandler *socket, QStringList &commands,
                                    QStringList &slist)
{
    bool handled = false;
    QString command = commands[0];

    if (command == "QUERY_FILETRANSFER")
        handled = HandleQueryFileTransfer(socket, commands, slist);
    else if (command == "QUERY_FREE_SPACE")
        handled = HandleQueryFreeSpace(socket);
    else if (command == "QUERY_FREE_SPACE_LIST")
        handled = HandleQueryFreeSpaceList(socket);
    else if (command == "QUERY_FREE_SPACE_SUMMARY")
        handled = HandleQueryFreeSpaceSummary(socket);
    else if (command == "QUERY_FILE_EXISTS")
        handled = HandleQueryFileExists(socket, slist);
    else if (command == "QUERY_FILE_HASH")
        handled = HandleQueryFileHash(socket, slist);
    else if (command == "DELETE_FILE")
        handled = HandleDeleteFile(socket, slist);
    else if (command == "QUERY_SG_GETFILELIST")
        handled = HandleGetFileList(socket, slist);
    else if (command == "QUERY_SG_FILEQUERY")
        handled = HandleFileQuery(socket, slist);
    else if (command == "DOWNLOAD_FILE" || command == "DOWNLOAD_FILE_NOW")
        handled = HandleDownloadFile(socket, slist);
    return handled;
}

bool FileServerHandler::HandleQueryFreeSpace(SocketHandler *socket)
{
    QStringList res;

    QList<FileSystemInfo> disks = QueryFileSystems();
    QList<FileSystemInfo>::const_iterator i;
    for (i = disks.begin(); i != disks.end(); ++i)
        i->ToStringList(res);

    socket->WriteStringList(res);
    return true;
}

bool FileServerHandler::HandleQueryFreeSpaceList(SocketHandler *socket)
{
    QStringList res;
    QStringList hosts;

    QList<FileSystemInfo> disks = QueryAllFileSystems();
    QList<FileSystemInfo>::const_iterator i;
    for (i = disks.begin(); i != disks.end(); ++i)
        if (!hosts.contains(i->getHostname()))
            hosts << i->getHostname();

    // TODO: get max bitrate from encoderlink
    FileSystemInfo::Consolidate(disks, true, 14000);

    long long total = 0;
    long long used = 0;
    for (i = disks.begin(); i != disks.end(); ++i)
    {
        i->ToStringList(res);
        total += i->getTotalSpace();
        used  += i->getUsedSpace();
    }

    res << hosts.join(",")
        << "TotalDiskSpace"
        << "0"
        << "-2"
        << "-2"
        << "0"
        << QString::number(total)
        << QString::number(used);

    socket->WriteStringList(res);
    return true;
}

bool FileServerHandler::HandleQueryFreeSpaceSummary(SocketHandler *socket)
{
    QStringList res;
    QList<FileSystemInfo> disks = QueryAllFileSystems();
    // TODO: get max bitrate from encoderlink
    FileSystemInfo::Consolidate(disks, true, 14000);

    QList<FileSystemInfo>::const_iterator i;
    long long total = 0;
    long long used = 0;
    for (i = disks.begin(); i != disks.end(); ++i)
    {
        total += i->getTotalSpace();
        used  += i->getUsedSpace();
    }

    res << QString::number(total) << QString::number(used);
    socket->WriteStringList(res);
    return true;
}

QList<FileSystemInfo> FileServerHandler::QueryFileSystems(void)
{
    QStringList groups(StorageGroup::kSpecialGroups);
    groups.removeAll("LiveTV");
    QString specialGroups = groups.join("', '");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT MIN(id),dirname "
                            "FROM storagegroup "
                           "WHERE hostname = :HOSTNAME "
                             "AND groupname NOT IN ( '%1' ) "
                           "GROUP BY dirname;").arg(specialGroups));
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    QList<FileSystemInfo> disks;
    if (query.exec() && query.isActive())
    {
        if (!query.size())
        {
            query.prepare("SELECT MIN(id),dirname "
                            "FROM storagegroup "
                           "WHERE groupname = :GROUP "
                           "GROUP BY dirname;");
            query.bindValue(":GROUP", "Default");
            if (!query.exec())
                MythDB::DBError("BackendQueryFileSystems", query);
        }

        QDir checkDir("");
        QString currentDir;
        FileSystemInfo disk;
        QMap <QString, bool>foundDirs;

        while (query.next())
        {
            disk.clear();
            disk.setHostname(gCoreContext->GetHostName());
            disk.setLocal();
            disk.setBlockSize(0);
            disk.setGroupID(query.value(0).toInt());

            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromAscii() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            currentDir = QString::fromUtf8(query.value(1)
                                           .toByteArray().constData());
            disk.setPath(currentDir);

            if (currentDir.endsWith("/"))
                currentDir.remove(currentDir.length() - 1, 1);

            checkDir.setPath(currentDir);
            if (!foundDirs.contains(currentDir))
            {
                if (checkDir.exists())
                {
                    disk.PopulateDiskSpace();
                    disk.PopulateFSProp();
                    disks << disk;

                    foundDirs[currentDir] = true;
                }
                else
                    foundDirs[currentDir] = false;
            }
        }
    }

    return disks;
}

QList<FileSystemInfo> FileServerHandler::QueryAllFileSystems(void)
{
    QList<FileSystemInfo> disks = QueryFileSystems();

    {
        QReadLocker rlock(&m_fsLock);

        QMap<QString, SocketHandler*>::iterator i;
        for (i = m_fsMap.begin(); i != m_fsMap.end(); ++i)
            disks << FileSystemInfo::RemoteGetInfo((*i)->GetSocket());
    }

    return disks;
}

/**
 * \addtogroup myth_network_protocol
 * \par QUERY_FILE_EXISTS \e filename \e storagegroup
 */
bool FileServerHandler::HandleQueryFileExists(SocketHandler *socket,
                                              QStringList &slist)
{
    QString storageGroup = "Default";
    QStringList res;

    if (slist.size() == 3)
    {
        if (!slist[2].isEmpty())
            storageGroup = slist[2];
    }
    else if (slist.size() != 2)
        return false;

    QString filename = slist[1];
    if ((filename.isEmpty()) || 
        (filename.contains("/../")) || 
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR, 
            QString("ERROR checking for file, filename '%1' "
                    "fails sanity checks").arg(filename));
        res << "";
        socket->WriteStringList(res);
        return true;
    }

    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());
    QString fullname = sgroup.FindFile(filename);

    if (!fullname.isEmpty())
    {
        res << "1"
            << fullname;

        // TODO: convert me to QFile
        struct stat fileinfo;
        if (stat(fullname.toLocal8Bit().constData(), &fileinfo) >= 0)
        {
            res << QString::number(fileinfo.st_dev)
                << QString::number(fileinfo.st_ino)
                << QString::number(fileinfo.st_mode)
                << QString::number(fileinfo.st_nlink)
                << QString::number(fileinfo.st_uid)
                << QString::number(fileinfo.st_gid)
                << QString::number(fileinfo.st_rdev)
                << QString::number(fileinfo.st_size)
#ifdef _WIN32
                << "0"
                << "0"
#else
                << QString::number(fileinfo.st_blksize)
                << QString::number(fileinfo.st_blocks)
#endif
                << QString::number(fileinfo.st_atime)
                << QString::number(fileinfo.st_mtime)
                << QString::number(fileinfo.st_ctime);
        }
    }
    else
        res << "0";

    socket->WriteStringList(res);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_FILE_HASH \e storagegroup \e filename
 */
bool FileServerHandler::HandleQueryFileHash(SocketHandler *socket,
                                            QStringList &slist)
{
    QString storageGroup = "Default";
    QString hostname     = gCoreContext->GetHostName();
    QString filename     = "";
    QStringList res;

    switch (slist.size()) {
      case 4:
        if (!slist[3].isEmpty())
            hostname = slist[3];
        [[clang::fallthrough]];
      case 3:
        if (!slist[2].isEmpty())
            storageGroup = slist[2];
        [[clang::fallthrough]];
      case 2:
        filename = slist[1];
        if (filename.isEmpty() ||
            filename.contains("/../") ||
            filename.startsWith("../"))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("ERROR checking for file, filename '%1' "
                        "fails sanity checks").arg(filename));
            res << "";
            socket->WriteStringList(res);
            return true;
        }
        break;
      default:
        return false;
    }

    QString hash = "";

    if (gCoreContext->IsThisHost(hostname))
    {
        // looking for file on me, return directly
        StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());
        QString fullname = sgroup.FindFile(filename);
        hash = FileHash(fullname);
    }
    else
    {
        QReadLocker rlock(&m_fsLock);
        if (m_fsMap.contains(hostname))
        {
            // looking for file on connected host, query from it
            if (m_fsMap[hostname]->SendReceiveStringList(slist))
                hash = slist[0];
        }
        // I deleted the incorrect SQL select that was supposed to get
        // host name from ip address. Since it cannot work and has
        // been there 6 years I assume it is not important.
    }


    res << hash;
    socket->WriteStringList(res);

    return true;
}

bool FileServerHandler::HandleDeleteFile(SocketHandler *socket,
                                         QStringList &slist)
{
    if (slist.size() != 3)
        return false;

    return HandleDeleteFile(socket, slist[1], slist[2]);
}

bool FileServerHandler::DeleteFile(QString filename, QString storagegroup)
{
    return HandleDeleteFile( (SocketHandler *)NULL, filename, storagegroup);
}

bool FileServerHandler::HandleDeleteFile(SocketHandler *socket,
                                QString filename, QString storagegroup)
{
    StorageGroup sgroup(storagegroup, "", false);
    QStringList res;

    if ((filename.isEmpty()) ||
        (filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ERROR deleting file, filename '%1' fails sanity checks")
                .arg(filename));
        if (socket)
        {
            res << "0";
            socket->WriteStringList(res);
            return true;
        }
        return false;
    }

    QString fullfile = sgroup.FindFile(filename);

    if (fullfile.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unable to find %1 in HandleDeleteFile()") .arg(filename));
        if (socket)
        {
            res << "0";
            socket->WriteStringList(res);
            return true;
        }
        return false;
    }

    QFile checkFile(fullfile);
    if (checkFile.exists())
    {
        if (socket)
        {
            res << "1";
            socket->WriteStringList(res);
        }
        RunDeleteThread();
        deletethread->AddFile(fullfile);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error deleting file: '%1'")
                        .arg(fullfile));
        if (socket)
        {
            res << "0";
            socket->WriteStringList(res);
        }
    }

    return true;
}

bool FileServerHandler::HandleDeleteFile(DeleteHandler *handler)
{
    RunDeleteThread();
    return deletethread->AddFile(handler);
}

bool FileServerHandler::HandleGetFileList(SocketHandler *socket,
                                          QStringList &slist)
{
    QStringList res;

    bool fileNamesOnly = false;
    if (slist.size() == 5)
        fileNamesOnly = slist[4].toInt();
    else if (slist.size() != 4)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid Request. %1")
                                     .arg(slist.join("[]:[]")));
        res << "EMPTY LIST";
        socket->WriteStringList(res);
        return true;
    }

    QString host = gCoreContext->GetHostName();
    QString wantHost = slist[1];
    QString groupname = slist[2];
    QString path = slist[3];

    LOG(VB_FILE, LOG_INFO,
        QString("HandleSGGetFileList: group = %1  host = %2  "
                "path = %3 wanthost = %4")
            .arg(groupname).arg(host).arg(path).arg(wantHost));

    if (gCoreContext->IsThisHost(wantHost))
    {
        StorageGroup sg(groupname, host);
        LOG(VB_FILE, LOG_INFO, "Getting local info");
        if (fileNamesOnly)
            res = sg.GetFileList(path);
        else
            res = sg.GetFileInfoList(path);

        if (res.count() == 0)
            res << "EMPTY LIST";
    }
    else
    {
        // handle request on remote server
        SocketHandler *remsock = NULL;
        {
            QReadLocker rlock(&m_fsLock);
            if (m_fsMap.contains(wantHost))
            {
                remsock = m_fsMap[wantHost];
                remsock->IncrRef();
            }
        }

        if (remsock)
        {
            LOG(VB_FILE, LOG_INFO, "Getting remote info");
            res << "QUERY_SG_GETFILELIST" << wantHost << groupname << path
                << QString::number(fileNamesOnly);
            remsock->SendReceiveStringList(res);
            remsock->DecrRef();
        }
        else
        {
            LOG(VB_FILE, LOG_ERR, QString("Failed to grab slave socket : %1 :")
                     .arg(wantHost));
            res << "SLAVE UNREACHABLE: " << wantHost;
        }
    }

    socket->WriteStringList(res);
    return true;
}

bool FileServerHandler::HandleFileQuery(SocketHandler *socket,
                                        QStringList &slist)
{
    QStringList res;

    if (slist.size() != 4)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid Request. %1")
                .arg(slist.join("[]:[]")));
        res << "EMPTY LIST";
        socket->WriteStringList(res);
        return true;
    }

    QString wantHost  = slist[1];
    QString groupname = slist[2];
    QString filename  = slist[3];

    LOG(VB_FILE, LOG_DEBUG, QString("HandleSGFileQuery: myth://%1@%2/%3")
                             .arg(groupname).arg(wantHost).arg(filename));

    if (gCoreContext->IsThisHost(wantHost))
    {
        // handle request locally
        LOG(VB_FILE, LOG_DEBUG, QString("Getting local info"));
        StorageGroup sg(groupname, gCoreContext->GetHostName());
        res = sg.GetFileInfo(filename);

        if (res.count() == 0)
            res << "EMPTY LIST";
    }
    else
    {
        // handle request on remote server
        SocketHandler *remsock = NULL;
        {
            QReadLocker rlock(&m_fsLock);
            if (m_fsMap.contains(wantHost))
            {
                remsock = m_fsMap[wantHost];
                remsock->IncrRef();
            }
        }

        if (remsock)
        {
            res << "QUERY_SG_FILEQUERY" << wantHost << groupname << filename;
            remsock->SendReceiveStringList(res);
            remsock->DecrRef();
        }
        else
        {
            res << "SLAVE UNREACHABLE: " << wantHost;
        }
    }

    socket->WriteStringList(res);
    return true;
}

bool FileServerHandler::HandleQueryFileTransfer(SocketHandler *socket,
                        QStringList &commands, QStringList &slist)
{
    if (commands.size() != 2)
        return false;

    if (slist.size() < 2)
        return false;

    QStringList res;
    int recnum = commands[1].toInt();
    FileTransfer *ft;

    {
        QReadLocker rlock(&m_ftLock);
        if (!m_ftMap.contains(recnum))
        {
            if (slist[1] == "DONE")
                res << "OK";
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Unknown file transfer socket: %1").arg(recnum));
                res << "ERROR"
                    << "unknown_file_transfer_socket";
            }

            socket->WriteStringList(res);
            return true;
        }

        ft = m_ftMap[recnum];
        ft->IncrRef();
    }

    if (slist[1] == "REQUEST_BLOCK")
    {
        if (slist.size() != 3)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid QUERY_FILETRANSFER "
                                     "REQUEST_BLOCK call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            int size = slist[2].toInt();
            res << QString::number(ft->RequestBlock(size));
        }
    }
    else if (slist[1] == "WRITE_BLOCK")
    {
        if (slist.size() != 3)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid QUERY_FILETRANSFER "
                                     "WRITE_BLOCK call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            int size = slist[2].toInt();
            res << QString::number(ft->WriteBlock(size));
        }
    }
    else if (slist[1] == "SEEK")
    {
        if (slist.size() != 5)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid QUERY_FILETRANSFER SEEK call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            long long pos = slist[2].toLongLong();
            int whence = slist[3].toInt();
            long long curpos = slist[4].toLongLong();

            res << QString::number(ft->Seek(curpos, pos, whence));
        }
    }
    else if (slist[1] == "IS_OPEN")
    {
        res << QString::number(ft->isOpen());
    }
    else if (slist[1] == "DONE")
    {
        ft->Stop();
        res << "OK";
    }
    else if (slist[1] == "SET_TIMEOUT")
    {
        if (slist.size() != 3)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid QUERY_FILETRANSFER "
                                     "SET_TIMEOUT call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            bool fast = slist[2].toInt();
            ft->SetTimeout(fast);
            res << "OK";
        }
    }
    else if (slist[1] == "REQUEST_SIZE")
    {
        // return size and if the file is not opened for writing
        res << QString::number(ft->GetFileSize());
        res << QString::number(!gCoreContext->IsRegisteredFileForWrite(ft->GetFileName()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid QUERY_FILETRANSFER call");
        res << "ERROR" << "invalid_call";
    }

    ft->DecrRef();
    socket->WriteStringList(res);
    return true;
}

bool FileServerHandler::HandleDownloadFile(SocketHandler *socket,
                                           QStringList &slist)
{
    QStringList res;

    if (slist.size() != 4)
    {
        res << "ERROR" << QString("Bad %1 command").arg(slist[0]);
        socket->WriteStringList(res);
        return true;
    }

    bool synchronous = (slist[0] == "DOWNLOAD_FILE_NOW");
    QString srcURL = slist[1];
    QString storageGroup = slist[2];
    QString filename = slist[3];
    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName(), false);
    QString outDir = sgroup.FindNextDirMostFree();
    QString outFile;
    QStringList retlist;

    if (filename.isEmpty())
    {
        QFileInfo finfo(srcURL);
        filename = finfo.fileName();
    }

    if (outDir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to determine directory "
                "to write to in %1 write command").arg(slist[0]));
        res << "ERROR" << "downloadfile_directory_not_found";
        socket->WriteStringList(res);
        return true;
    }

    if ((filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("ERROR: %1 write "
                "filename '%2' does not pass sanity checks.")
                .arg(slist[0]).arg(filename));
        res << "ERROR" << "downloadfile_filename_dangerous";
        socket->WriteStringList(res);
        return true;
    }

    outFile = outDir + "/" + filename;

    if (synchronous)
    {
        if (GetMythDownloadManager()->download(srcURL, outFile))
        {
            res << "OK"
                << gCoreContext->GetMasterHostPrefix(storageGroup)
                       + filename;
        }
        else
            res << "ERROR";
    }
    else
    {
        QMutexLocker locker(&m_downloadURLsLock);
        m_downloadURLs[outFile] =
            gCoreContext->GetMasterHostPrefix(storageGroup) +
            StorageGroup::GetRelativePathname(outFile);

        GetMythDownloadManager()->queueDownload(srcURL, outFile, this);
        res << "OK"
            << gCoreContext->GetMasterHostPrefix(storageGroup) + filename;
    }

    socket->WriteStringList(res);
    return true;
}

