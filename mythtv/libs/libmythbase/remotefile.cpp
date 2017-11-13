#include <iostream>
using namespace std;

#include <QUrl>
#include <QFile>
#include <QFileInfo>

// POSIX C headers
#include <unistd.h>
#include <fcntl.h>

#include "mythconfig.h"

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "mythdb.h"
#include "remotefile.h"
#include "mythcorecontext.h"
#include "mythsocket.h"
#include "compat.h"
#include "mythtimer.h"
#include "mythdate.h"
#include "mythmiscutil.h"
#include "threadedfilewriter.h"
#include "storagegroup.h"

#define MAX_FILE_CHECK 500  // in ms

static bool RemoteSendReceiveStringList(const QString &host, QStringList &strlist)
{
    bool ok = false;

    if (gCoreContext->IsMasterBackend())
    {
        // since the master backend cannot connect back around to
        // itself, and the libraries do not have access to the list
        // of connected slave backends to query an existing connection
        // start up a new temporary connection directly to the slave
        // backend to query the file list
        QString ann = QString("ANN Playback %1 0")
                        .arg(gCoreContext->GetHostName());
        QString addr = gCoreContext->GetBackendServerIP(host);
        int port = gCoreContext->GetBackendServerPort(host);
        bool mismatch = false;

        MythSocket *sock = gCoreContext->ConnectCommandSocket(
                                            addr, port, ann, &mismatch);
        if (sock)
        {
            ok = sock->SendReceiveStringList(strlist);
            sock->DecrRef();
        }
        else
            strlist.clear();
    }
    else
        ok = gCoreContext->SendReceiveStringList(strlist);

    return ok;
}

RemoteFile::RemoteFile(const QString &_path, bool write, bool useRA,
                       int _timeout_ms,
                       const QStringList *possibleAuxiliaryFiles) :
    path(_path),
    usereadahead(useRA),  timeout_ms(_timeout_ms),
    filesize(-1),         timeoutisfast(false),
    readposition(0LL),    lastposition(0LL),
    canresume(false),     recordernum(0),
    lock(QMutex::NonRecursive),
    controlSock(NULL),    sock(NULL),
    query("QUERY_FILETRANSFER %1"),
    writemode(write),     completed(false),
    localFile(-1),        fileWriter(NULL)
{
    if (writemode)
    {
        usereadahead = false;
        timeout_ms = -1;
    }
    else if (possibleAuxiliaryFiles)
        possibleauxfiles = *possibleAuxiliaryFiles;

    if (!path.isEmpty())
        Open();

    LOG(VB_FILE, LOG_DEBUG, QString("RemoteFile(%1)").arg(path));
}

RemoteFile::~RemoteFile()
{
    Close();
    if (controlSock)
    {
        controlSock->DecrRef();
        controlSock = NULL;
    }
    if (sock)
    {
        sock->DecrRef();
        sock = NULL;
    }
}

bool RemoteFile::isLocal(const QString &lpath)
{
    bool is_local = !lpath.isEmpty() &&
        !lpath.startsWith("myth:") &&
        (lpath.startsWith("/") || QFile::exists(lpath));
    return is_local;
}

bool RemoteFile::isLocal(void) const
{
    return isLocal(path);
}

MythSocket *RemoteFile::openSocket(bool control)
{
    QUrl qurl(path);
    QString dir;

    QString host = qurl.host();
    int port = qurl.port();

    dir = qurl.path();

    if (qurl.hasQuery())
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
        dir += "?" + QUrl::fromPercentEncoding(
            qurl.query(QUrl::FullyEncoded).toLocal8Bit());
#else
        dir += "?" + QUrl::fromPercentEncoding(qurl.encodedQuery());
#endif

    if (qurl.hasFragment())
        dir += "#" + qurl.fragment();

    QString sgroup = qurl.userName();

    MythSocket *lsock = new MythSocket();
    QString stype = (control) ? "control socket" : "file data socket";

    QString loc = QString("RemoteFile::openSocket(%1): ").arg(stype);

    if (port <= 0)
    {
        port = gCoreContext->GetBackendServerPort(host);
    }

    if (!lsock->ConnectToHost(host, port))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Could not connect to server %1:%2") .arg(host).arg(port));
        lsock->DecrRef();
        return NULL;
    }

    QString hostname = GetMythDB()->GetHostName();

    QStringList strlist;

#ifndef IGNORE_PROTO_VER_MISMATCH
    if (!gCoreContext->CheckProtoVersion(lsock, 5000))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Failed validation to server %1:%2").arg(host).arg(port));
        lsock->DecrRef();
        return NULL;
    }
#endif

    if (control)
    {
        strlist.append(QString("ANN Playback %1 %2").arg(hostname).arg(false));
        if (!lsock->SendReceiveStringList(strlist))
        {
            LOG(VB_GENERAL, LOG_ERR, loc +
                QString("Could not read string list from server %1:%2")
                    .arg(host).arg(port));
            lsock->DecrRef();
            return NULL;
        }
    }
    else
    {
        strlist.push_back(QString("ANN FileTransfer %1 %2 %3 %4")
                          .arg(hostname).arg(writemode)
                          .arg(usereadahead).arg(timeout_ms));
        strlist << QString("%1").arg(dir);
        strlist << sgroup;

        QStringList::const_iterator it = possibleauxfiles.begin();
        for (; it != possibleauxfiles.end(); ++it)
            strlist << *it;

        if (!lsock->SendReceiveStringList(strlist))
        {
            LOG(VB_GENERAL, LOG_ERR, loc +
                QString("Did not get proper response from %1:%2")
                    .arg(host).arg(port));
            strlist.clear();
            strlist.push_back("ERROR");
            strlist.push_back("invalid response");
        }

        if (strlist.size() >= 3)
        {
            it = strlist.begin(); ++it;
            recordernum = (*it).toInt(); ++it;
            filesize = (*(it)).toLongLong(); ++it;
            for (; it != strlist.end(); ++it)
                auxfiles << *it;
        }
        else if (!strlist.isEmpty() && strlist.size() < 3 &&
                 strlist[0] != "ERROR")
        {
            LOG(VB_GENERAL, LOG_ERR, loc +
                QString("Did not get proper response from %1:%2")
                    .arg(host).arg(port));
            strlist.clear();
            strlist.push_back("ERROR");
            strlist.push_back("invalid response");
        }
    }

    if (strlist.isEmpty() || strlist[0] == "ERROR")
    {
        lsock->DecrRef();
        lsock = NULL;
        if (strlist.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, loc + "Failed to open socket, timeout");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, loc + "Failed to open socket" +
                ((strlist.size() >= 2) ?
                QString(", error was %1").arg(strlist[1]) :
                QString(", remote error")));
        }
    }

    return lsock;
}

bool RemoteFile::isOpen() const
{
    if (isLocal())
    {
        return writemode ? (fileWriter != NULL) : (localFile != -1);
    }
    return sock && controlSock;
}

bool RemoteFile::Open(void)
{
    if (isOpen())
        return true;

    QMutexLocker locker(&lock);
    return OpenInternal();
}

/**
 *  \brief Attempts to resume from a disconnected step. Must have lock
 *  \return True if reconnection succeeded
 */
bool RemoteFile::OpenInternal()
{
    if (isLocal())
    {
        if (writemode)
        {
            // make sure the directories are created if necessary
            QFileInfo fi(path);
            QDir dir(fi.path());
            if (!dir.exists())
            {
                LOG(VB_FILE, LOG_WARNING, QString("RemoteFile::Open(%1) creating directories")
                    .arg(path));

                if (!dir.mkpath(fi.path()))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("RemoteFile::Open(%1) failed to create the directories")
                        .arg(path));
                    return false;
                }
            }

            fileWriter = new ThreadedFileWriter(path,
                                                O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                                0644);

            if (!fileWriter->Open())
            {
                delete fileWriter;
                fileWriter = NULL;
                LOG(VB_FILE, LOG_ERR, QString("RemoteFile::Open(%1) write mode error")
                    .arg(path));
                return false;
            }
            SetBlocking();
            return true;
        }

        // local mode, read only
        if (!Exists(path))
        {
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Open(%1) Error: Does not exist").arg(path));
            return false;
        }

        localFile = ::open(path.toLocal8Bit().constData(), O_RDONLY);
        if (localFile == -1)
        {
            LOG(VB_FILE, LOG_ERR, QString("RemoteFile::Open(%1) Error: %2")
                .arg(path).arg(strerror(errno)));
            return false;
        }
        return true;
    }
    controlSock = openSocket(true);
    if (!controlSock)
        return false;

    sock = openSocket(false);
    if (!sock)
    {
        // Close the sockets if we received an error so that isOpen() will
        // return false if the caller tries to use the RemoteFile.
        Close(true);
        return false;
    }
    canresume = true;

    return true;
}

bool RemoteFile::ReOpen(QString newFilename)
{
    if (isLocal())
    {
        if (isOpen())
        {
            Close();
        }
        path = newFilename;
        return Open();
    }

    QMutexLocker locker(&lock);

    if (!CheckConnection(false))
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::ReOpen(): Couldn't connect");
        return false;
    }

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "REOPEN";
    strlist << newFilename;

    controlSock->SendReceiveStringList(strlist);

    lock.unlock();

    bool retval = false;
    if (!strlist.isEmpty())
        retval = strlist[0].toInt();

    return retval;
}

void RemoteFile::Close(bool haslock)
{
    if (isLocal())
    {
        ::close(localFile);
        localFile = -1;
        delete fileWriter;
        fileWriter = NULL;
        return;
    }
    if (!controlSock)
        return;

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "DONE";

    if (!haslock)
    {
        lock.lock();
    }
    if (controlSock->IsConnected() && !controlSock->SendReceiveStringList(
            strlist, 0, MythSocket::kShortTimeout))
    {
        LOG(VB_GENERAL, LOG_ERR, "Remote file timeout.");
    }

    if (sock)
    {
        sock->DecrRef();
        sock = NULL;
    }
    if (controlSock)
    {
        controlSock->DecrRef();
        controlSock = NULL;
    }

    if (!haslock)
    {
        lock.unlock();
    }
}

bool RemoteFile::DeleteFile(const QString &url)
{
    if (isLocal(url))
    {
        QFile file(url);
        return file.remove();
    }

    bool result      = false;
    QUrl qurl(url);
    QString filename = qurl.path();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.endsWith("#"))
        filename = filename + "#" + qurl.fragment();

    if (filename.startsWith("/"))
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty() || sgroup.isEmpty())
        return false;

    QStringList strlist("DELETE_FILE");
    strlist << filename;
    strlist << sgroup;

    gCoreContext->SendReceiveStringList(strlist);

    if (!strlist.isEmpty() && strlist[0] == "1")
        result = true;

    return result;
}

bool RemoteFile::Exists(const QString &url)
{
    if (url.isEmpty())
        return false;

    struct stat fileinfo;
    return Exists(url, &fileinfo);
}

bool RemoteFile::Exists(const QString &url, struct stat *fileinfo)
{
    if (url.isEmpty())
        return false;

    QUrl qurl(url);
    QString filename = qurl.path();
    QString sgroup   = qurl.userName();
    QString host     = qurl.host();

    if (isLocal(url) || gCoreContext->IsThisBackend(host))
    {
       LOG(VB_FILE, LOG_INFO,
           QString("RemoteFile::Exists(): looking for local file: %1").arg(url));

        bool fileExists = false;
        QString fullFilePath = "";

        if (url.startsWith("myth:"))
        {
            StorageGroup sGroup(sgroup, gCoreContext->GetHostName());
            fullFilePath = sGroup.FindFile(filename);
            if (!fullFilePath.isEmpty())
                fileExists = true;
        }
        else
        {
            QFileInfo info(url);
            fileExists = info.exists() /*&& info.isFile()*/;
            fullFilePath = url;
        }

        if (fileExists)
        {
            if (stat(fullFilePath.toLocal8Bit().constData(), fileinfo) == -1)
            {
                LOG(VB_FILE, LOG_ERR,
                    QString("RemoteFile::Exists(): failed to stat file: %1").arg(fullFilePath) + ENO);
            }
        }

        return fileExists;
    }

    LOG(VB_FILE, LOG_INFO,
        QString("RemoteFile::Exists(): looking for remote file: %1").arg(url));

    if (!qurl.fragment().isEmpty() || url.endsWith("#"))
        filename = filename + "#" + qurl.fragment();

    if (filename.startsWith("/"))
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty())
        return false;

    QStringList strlist("QUERY_FILE_EXISTS");
    strlist << filename;
    if (!sgroup.isEmpty())
        strlist << sgroup;

    bool result = false;
    if (RemoteSendReceiveStringList(host, strlist) && strlist[0] == "1")
    {
        if ((strlist.size() >= 15) && fileinfo)
        {
            fileinfo->st_dev       = strlist[2].toLongLong();
            fileinfo->st_ino       = strlist[3].toLongLong();
            fileinfo->st_mode      = strlist[4].toLongLong();
            fileinfo->st_nlink     = strlist[5].toLongLong();
            fileinfo->st_uid       = strlist[6].toLongLong();
            fileinfo->st_gid       = strlist[7].toLongLong();
            fileinfo->st_rdev      = strlist[8].toLongLong();
            fileinfo->st_size      = strlist[9].toLongLong();
#ifndef _WIN32
            fileinfo->st_blksize   = strlist[10].toLongLong();
            fileinfo->st_blocks    = strlist[11].toLongLong();
#endif
            fileinfo->st_atime     = strlist[12].toLongLong();
            fileinfo->st_mtime     = strlist[13].toLongLong();
            fileinfo->st_ctime     = strlist[14].toLongLong();
            result = true;
        }
        else if (!fileinfo)
        {
            result = true;
        }
    }

    return result;
}

QString RemoteFile::GetFileHash(const QString &url)
{
    if (isLocal(url))
    {
        return FileHash(url);
    }
    QString result;
    QUrl qurl(url);
    QString filename = qurl.path();
    QString hostname = qurl.host();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.endsWith("#"))
        filename = filename + "#" + qurl.fragment();

    if (filename.startsWith("/"))
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty() || sgroup.isEmpty())
        return QString();

    QStringList strlist("QUERY_FILE_HASH");
    strlist << filename;
    strlist << sgroup;
    strlist << hostname;

    if (RemoteSendReceiveStringList(hostname, strlist))
    {
        result = strlist[0];
    }

    return result;
}

bool RemoteFile::CopyFile (const QString& src, const QString& dst,
                           bool overwrite, bool verify)
{
    LOG(VB_FILE, LOG_INFO,
        QString("RemoteFile::CopyFile: Copying file from '%1' to '%2'").arg(src).arg(dst));

    // sanity check
    if (src == dst)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::CopyFile: Cannot copy a file to itself");
        return false;
    }

    RemoteFile srcFile(src, false);
    if (!srcFile.isOpen())
    {
         LOG(VB_GENERAL, LOG_ERR,
             QString("RemoteFile::CopyFile: Failed to open file (%1) for reading.").arg(src));
         return false;
    }

    const int readSize = 2 * 1024 * 1024;
    char *buf = new char[readSize];
    if (!buf)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::CopyFile: ERROR, unable to allocate copy buffer");
        return false;
    }

    if (overwrite)
    {
        DeleteFile(dst);
    }
    else if (Exists(dst))
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::CopyFile: File already exists");
        delete[] buf;
        return false;
    }

    RemoteFile dstFile(dst, true);
    if (!dstFile.isOpen())
    {
         LOG(VB_GENERAL, LOG_ERR,
             QString("RemoteFile::CopyFile: Failed to open file (%1) for writing.").arg(dst));
         srcFile.Close();
         delete[] buf;
         return false;
    }

    dstFile.SetBlocking(true);

    bool success = true;
    int srcLen, dstLen;

    while ((srcLen = srcFile.Read(buf, readSize)) > 0)
    {
        dstLen = dstFile.Write(buf, srcLen);

        if (dstLen == -1 || srcLen != dstLen)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "RemoteFile::CopyFile: Error while trying to write to destination file.");
            success = false;
        }
    }

    srcFile.Close();
    dstFile.Close();
    delete[] buf;

    if (success && verify)
    {
        // Check written file is correct size
        struct stat fileinfo;
        long long dstSize = Exists(dst, &fileinfo) ? fileinfo.st_size : -1;
        long long srcSize = srcFile.GetFileSize();
        if (dstSize != srcSize)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("RemoteFile::CopyFile: Copied file is wrong size (%1 rather than %2)")
                    .arg(dstSize).arg(srcSize));
            success = false;
            DeleteFile(dst);
        }
    }

    return success;
}

bool RemoteFile::MoveFile (const QString& src, const QString& dst, bool overwrite)
{
    LOG(VB_FILE, LOG_INFO,
        QString("RemoteFile::MoveFile: Moving file from '%1' to '%2'").arg(src).arg(dst));

    // sanity check
    if (src == dst)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::MoveFile: Cannot move a file to itself");
        return false;
    }

    if (isLocal(src) != isLocal(dst))
    {
        // Moving between local & remote requires a copy & delete
        bool ok = CopyFile(src, dst, overwrite, true);
        if (ok)
        {
            if (!DeleteFile(src))
                LOG(VB_FILE, LOG_ERR,
                    "RemoteFile::MoveFile: Failed to delete file after successful copy");
        }
        return ok;
    }

    if (overwrite)
    {
        DeleteFile(dst);
    }
    else if (Exists(dst))
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::MoveFile: File already exists");
        return false;
    }

    if (isLocal(src))
    {
        // Moving local -> local
        QFileInfo fi(dst);
        if (QDir().mkpath(fi.path()) && QFile::rename(src, dst))
            return true;

        LOG(VB_FILE, LOG_ERR, "RemoteFile::MoveFile: Rename failed");
        return false;
    }

    // Moving remote -> remote
    QUrl srcUrl(src);
    QUrl dstUrl(dst);

    if (srcUrl.userName() != dstUrl.userName())
    {
        LOG(VB_FILE, LOG_ERR, "RemoteFile::MoveFile: Cannot change a file's Storage Group");
        return false;
    }

    QStringList strlist("MOVE_FILE");
    strlist << srcUrl.userName() << srcUrl.path() << dstUrl.path();

    gCoreContext->SendReceiveStringList(strlist);

    if (!strlist.isEmpty() && strlist[0] == "1")
        return true;

    LOG(VB_FILE, LOG_ERR, QString("RemoteFile::MoveFile: MOVE_FILE failed with: %1")
        .arg(strlist.join(",")));
    return false;
}

void RemoteFile::Reset(void)
{
    if (isLocal())
        return;
    QMutexLocker locker(&lock);
    if (!sock)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Reset(): Called with no socket");
        return;
    }
    sock->Reset();
}

long long RemoteFile::Seek(long long pos, int whence, long long curpos)
{
    QMutexLocker locker(&lock);

    return SeekInternal(pos, whence, curpos);
}

long long RemoteFile::SeekInternal(long long pos, int whence, long long curpos)
{
    if (isLocal())
    {
        if (!isOpen())
        {
            LOG(VB_FILE, LOG_ERR, "RemoteFile::Seek(): Called with no file opened");
            return -1;
        }
        if (writemode)
            return fileWriter->Seek(pos, whence);

        long long offset = 0LL;
        if (whence == SEEK_SET)
        {
            QFileInfo info(path);
            offset = min(pos, info.size());
        }
        else if (whence == SEEK_END)
        {
            QFileInfo info(path);
            offset = info.size() + pos;
        }
        else if (whence == SEEK_CUR)
        {
            offset = ((curpos > 0) ? curpos : ::lseek64(localFile, 0, SEEK_CUR)) + pos;
        }
        else
            return -1;

        off64_t localpos = ::lseek64(localFile, (off64_t)pos, whence);
        if (localpos != (off64_t)pos)
        {
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Seek(): Couldn't seek to offset %1")
                .arg(offset));
            return -1;
        }
        return (long long)localpos;
    }

    if (!CheckConnection(false))
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Seek(): Couldn't connect");
        return -1;
    }

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "SEEK";
    strlist << QString::number(pos);
    strlist << QString::number(whence);
    if (curpos > 0)
        strlist << QString::number(curpos);
    else
        strlist << QString::number(readposition);

    bool ok = controlSock->SendReceiveStringList(strlist);

    if (ok && !strlist.isEmpty())
    {
        lastposition = readposition = strlist[0].toLongLong();
        sock->Reset();
        return strlist[0].toLongLong();
    }
    else
    {
        lastposition = 0LL;
    }

    return -1;
}

int RemoteFile::Write(const void *data, int size)
{
    int recv = 0;
    int sent = 0;
    unsigned zerocnt = 0;
    bool error = false;
    bool response = false;

    if (!writemode)
    {
        LOG(VB_NETWORK, LOG_ERR,
                "RemoteFile::Write(): Called when not in write mode");
        return -1;
    }
    if (isLocal())
    {
        if (!isOpen())
        {
            LOG(VB_FILE, LOG_ERR,
                "RemoteFile::Write(): File not opened");
            return -1;
        }
        return (int)fileWriter->Write(data, size);
    }

    QMutexLocker locker(&lock);

    if (!CheckConnection())
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Write(): Couldn't connect");
        return -1;
    }

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "WRITE_BLOCK";
    strlist << QString::number(size);
    bool ok = controlSock->WriteStringList(strlist);
    if (!ok)
    {
        LOG(VB_NETWORK, LOG_ERR,
            "RemoteFile::Write(): Block notification failed");
        return -1;
    }

    recv = size;
    while (sent < recv && !error && zerocnt++ < 50)
    {
        int ret = sock->Write((char*)data + sent, recv - sent);
        if (ret > 0)
        {
            sent += ret;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "RemoteFile::Write(): socket error");
            error = true;
            break;
        }

        if (controlSock->IsDataAvailable() &&
            controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.isEmpty())
        {
            recv = strlist[0].toInt(); // -1 on backend error
            response = true;
        }
    }

    if (!error && !response)
    {
        if (controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.isEmpty())
        {
            recv = strlist[0].toInt(); // -1 on backend error
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                    "RemoteFile::Write(): No response from control socket.");
            recv = -1;
        }
    }

    LOG(VB_NETWORK, LOG_DEBUG,
        QString("RemoteFile::Write(): reqd=%1, sent=%2, rept=%3, error=%4")
        .arg(size).arg(sent).arg(recv).arg(error));

    if (recv < 0)
        return recv;

    if (error || recv != sent)
    {
        sent = -1;
    }
    else
    {
        lastposition += sent;
    }

    return sent;
}

int RemoteFile::Read(void *data, int size)
{
    int recv = 0;
    int sent = 0;
    bool error = false;
    bool response = false;

    QMutexLocker locker(&lock);

    if (isLocal())
    {
        if (writemode)
        {
            LOG(VB_FILE, LOG_ERR, "RemoteFile:Read() called in writing mode");
            return -1;
        }
        if (isOpen())
        {
            return ::read(localFile, data, size);
        }
        LOG(VB_FILE, LOG_ERR, "RemoteFile:Read() called when local file not opened");
        return -1;
    }

    if (!CheckConnection())
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Read(): Couldn't connect");
        return -1;
    }

    if (sock->IsDataAvailable())
    {
        LOG(VB_NETWORK, LOG_ERR,
                "RemoteFile::Read(): Read socket not empty to start!");
        sock->Reset();
    }

    while (controlSock->IsDataAvailable())
    {
        LOG(VB_NETWORK, LOG_WARNING,
                "RemoteFile::Read(): Control socket not empty to start!");
        controlSock->Reset();
    }

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "REQUEST_BLOCK";
    strlist << QString::number(size);
    bool ok = controlSock->WriteStringList(strlist);
    if (!ok)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Read(): Block request failed");
        return -1;
    }

    sent = size;

    int waitms = 30;
    MythTimer mtimer;
    mtimer.start();

    while (recv < sent && !error && mtimer.elapsed() < 10000)
    {
        int ret = sock->Read(((char *)data) + recv, sent - recv, waitms);

        if (ret > 0)
            recv += ret;
        else if (ret < 0)
            error = true;

        waitms += (waitms < 200) ? 20 : 0;

        if (controlSock->IsDataAvailable() &&
            controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.isEmpty())
        {
            sent = strlist[0].toInt(); // -1 on backend error
            response = true;
            if (ret < sent)
            {
                // We have received less than what the server sent, retry immediately
                ret = sock->Read(((char *)data) + recv, sent - recv, waitms);
                if (ret > 0)
                    recv += ret;
                else if (ret < 0)
                    error = true;
            }
        }
    }

    if (!error && !response)
    {
        // Wait up to 1.5s for the backend to send the size
        // MythSocket::ReadString will drop the connection
        if (controlSock->ReadStringList(strlist, 1500) &&
            !strlist.isEmpty())
        {
            sent = strlist[0].toInt(); // -1 on backend error
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                   "RemoteFile::Read(): No response from control socket.");
            // If no data was received from control socket, and we got what we asked for
            // assume everything is okay
            if (recv == size)
            {
                sent = recv;
            }
            else
            {
                sent = -1;
            }
            // The TCP socket is dropped if there's a timeout, so we reconnect
            if (!Resume())
            {
                sent = -1;
            }
        }
    }

    LOG(VB_NETWORK, LOG_DEBUG,
        QString("Read(): reqd=%1, rcvd=%2, rept=%3, error=%4")
            .arg(size).arg(recv).arg(sent).arg(error));

    if (sent < 0)
        return sent;

    if (error || sent != recv)
    {
        recv = -1;
    }
    else
    {
        lastposition += recv;
    }

    return recv;
}

/**
 * GetFileSize: returns the remote file's size at the time it was first opened
 * Will query the server in order to get the size. If file isn't being modified
 * by the server, that value will be cached
 */
long long RemoteFile::GetFileSize(void) const
{
    if (isLocal())
    {
        if (isOpen() && writemode)
        {
            fileWriter->Flush();
        }
        if (Exists(path))
        {
            QFileInfo info(path);
            return info.size();
        }
        return -1;
    }

    QMutexLocker locker(&lock);
    return filesize;
}

/**
 * GetRealFileSize: returns the current remote file's size.
 * Will query the server in order to get the size. If file isn't being modified
 * by the server, that value will be cached.
 * A QUERY_SIZE myth request will be made. If the server doesn't support this command
 * the size will be queried using a QUERY_FILE_EXISTS request
 * Avoid using GetRealFileSize from the GUI thread
 */
long long RemoteFile::GetRealFileSize(void)
{
    if (isLocal())
    {
        return GetFileSize();
    }

    QMutexLocker locker(&lock);

    if (completed ||
        (lastSizeCheck.isRunning() && lastSizeCheck.elapsed() < MAX_FILE_CHECK))
    {
        return filesize;
    }

    if (!CheckConnection())
    {
        // Can't establish a new connection, using system one
        struct stat fileinfo;

        if (Exists(path, &fileinfo))
        {
            filesize = fileinfo.st_size;
        }
        return filesize;
    }

    QStringList strlist(QString(query).arg(recordernum));
    strlist << "REQUEST_SIZE";

    bool ok = controlSock->SendReceiveStringList(strlist);

    if (ok && !strlist.isEmpty())
    {
        bool validate;
        long long size = strlist[0].toLongLong(&validate);

        if (validate)
        {
            if (strlist.count() >= 2)
            {
                completed = strlist[1].toInt();
            }
            filesize = size;
        }
        else
        {
            struct stat fileinfo;

            if (Exists(path, &fileinfo))
            {
                filesize = fileinfo.st_size;
            }
        }
        lastSizeCheck.restart();
        return filesize;
    }

    return -1;
}

bool RemoteFile::SaveAs(QByteArray &data)
{
    long long fs = GetRealFileSize();

    if (fs < 0)
        return false;

    data.resize(fs);
    Read(data.data(), fs);

    return true;
}

void RemoteFile::SetTimeout(bool fast)
{
    if (isLocal())
    {
        // not much we can do with local accesses
        return;
    }
    if (timeoutisfast == fast)
        return;

    QMutexLocker locker(&lock);

    if (!CheckConnection())
    {
        LOG(VB_NETWORK, LOG_ERR,
            "RemoteFile::SetTimeout(): Couldn't connect");
        return;
    }

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "SET_TIMEOUT";
    strlist << QString::number((int)fast);

    controlSock->SendReceiveStringList(strlist);

    timeoutisfast = fast;
}

QDateTime RemoteFile::LastModified(const QString &url)
{
    if (isLocal(url))
    {
        QFileInfo info(url);
        return info.lastModified();
    }
    QDateTime result;
    QUrl qurl(url);
    QString filename = qurl.path();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.endsWith("#"))
        filename = filename + "#" + qurl.fragment();

    if (filename.startsWith("/"))
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty() || sgroup.isEmpty())
        return result;

    QStringList strlist("QUERY_SG_FILEQUERY");
    strlist << qurl.host();
    strlist << sgroup;
    strlist << filename;

    gCoreContext->SendReceiveStringList(strlist);

    if (strlist.size() > 1)
        result = MythDate::fromTime_t(strlist[1].toUInt());

    return result;
}

QDateTime RemoteFile::LastModified(void) const
{
    return LastModified(path);
}

/**
 *  \brief Search all BE's for a file in the give storage group
 *  \param filename the partial path and filename to look for
 *  \param host search this host first if given or default to the master BE if empty
 *  \param storageGroup the name of the storage group to search
 *  \param useRegex if true filename is assumed to be a regex expression of files to find
 *  \param allowFallback if false only 'host' will be searched otherwise all host will be searched until a match is found
 *  \return a QString containing the myth URL pointing to the first file found or empty list if not found
 */
QString RemoteFile::FindFile(const QString& filename, const QString& host,
                             const QString& storageGroup, bool useRegex,
                             bool allowFallback)
{
    QStringList files = RemoteFile::FindFileList(filename, host, storageGroup, useRegex, allowFallback);

    if (!files.isEmpty())
        return files[0];

    return QString();
}

/**
 *  \brief Search all BE's for files in the give storage group
 *  \param filename the partial path and filename to look for or regex
 *  \param host search this host first if given or default to the master BE if empty
 *  \param storageGroup the name of the storage group to search
 *  \param useRegex if true filename is assumed to be a regex expression of files to find
 *  \param allowFallback if false only 'host' will be searched otherwise all host will be searched until a match is found
 *  \return a QStringList list containing the myth URL's pointing to the file or empty list if not found
 */
QStringList RemoteFile::FindFileList(const QString& filename, const QString& host,
                             const QString& storageGroup, bool useRegex,
                             bool allowFallback)
{
    LOG(VB_FILE, LOG_INFO, QString("RemoteFile::FindFile(): looking for '%1' on '%2' in group '%3' "
                                   "(useregex: %4, allowfallback: %5)")
                                   .arg(filename).arg(host).arg(storageGroup)
                                   .arg(useRegex).arg(allowFallback));

    if (filename.isEmpty() || storageGroup.isEmpty())
        return QStringList();

    QStringList strList;
    QString hostName = host;

    if (hostName.isEmpty())
        hostName = gCoreContext->GetMasterHostName();

    // if we are looking for the file on this host just search the local storage group first
    if (gCoreContext->IsThisBackend(hostName))
    {
        // We could have made it this far with an IP when we really want
        // a hostname
        hostName = gCoreContext->GetHostName();
        StorageGroup sgroup(storageGroup, hostName);

        if (useRegex)
        {
            QFileInfo fi(filename);
            QStringList files = sgroup.GetFileList('/' + fi.path());

            LOG(VB_FILE, LOG_INFO, QString("RemoteFile::FindFileList: Looking in dir '%1' for '%2'")
                                           .arg(fi.path()).arg(fi.fileName()));

            for (int x = 0; x < files.size(); x++)
            {
                LOG(VB_FILE, LOG_INFO, QString("RemoteFile::FindFileList: Found '%1 - %2'")
                                               .arg(x).arg(files[x]));
            }

            QStringList filteredFiles = files.filter(QRegExp(fi.fileName()));
            for (int x = 0; x < filteredFiles.size(); x++)
            {
                strList << gCoreContext->GenMythURL(gCoreContext->GetHostName(),
                                                    gCoreContext->GetBackendServerPort(),
                                                    fi.path() + '/' + filteredFiles[x],
                                                    storageGroup);
            }
        }
        else
        {
            if (!sgroup.FindFile(filename).isEmpty())
                strList << gCoreContext->GenMythURL(hostName,
                                                    gCoreContext->GetBackendServerPort(hostName),
                                                    filename, storageGroup);
        }

        if (!strList.isEmpty() || !allowFallback)
            return strList;
    }

    // if we didn't find any files ask the master BE to find it
    if (strList.isEmpty() && !gCoreContext->IsMasterBackend())
    {
        strList << "QUERY_FINDFILE" << hostName << storageGroup << filename
                << (useRegex ? "1" : "0")
                << "1";

        if (gCoreContext->SendReceiveStringList(strList))
        {
            if (strList.size() > 0 && !strList[0].isEmpty() &&
                strList[0] != "NOT FOUND" && !strList[0].startsWith("ERROR: "))
                return strList;
        }
    }

    return QStringList();
}

/**
 *  \brief Set write blocking mode for the ThreadedFileWriter instance
 *  \param block false if not blocking, true if blocking
 *  \return old mode value
 */
bool RemoteFile::SetBlocking(bool block)
{
    if (fileWriter)
    {
        return fileWriter->SetBlocking(block);
    }
    return true;
}

/**
 *  \brief Check current connection and re-establish it if lost
 *  \return True if connection is working
 *  \param repos Bool indicating if we are to reposition to the last known
 *               location if reconnection is required
 */
bool RemoteFile::CheckConnection(bool repos)
{
    if (IsConnected())
    {
        return true;
    }
    if (!canresume)
    {
        return false;
    }
    return Resume(repos);
}

/**
 *  \brief Check if both the control and data sockets are currently connected
 *  \return True if both sockets are connected
 */
bool RemoteFile::IsConnected(void)
{
    return sock && controlSock &&
           sock->IsConnected() && controlSock->IsConnected();
}

/**
 *  \brief Attempts to resume from a disconnected step. Must have lock
 *  \return True if reconnection succeeded
 *  \param repos Bool indicating if we are to reposition to the last known location
 */
bool RemoteFile::Resume(bool repos)
{
    Close(true);
    if (!OpenInternal())
        return false;

    if (repos)
    {
        readposition = lastposition;
        if (SeekInternal(lastposition, SEEK_SET) < 0)
        {
            Close(true);
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Resume: Enable to re-seek into last known "
                        "position (%1").arg(lastposition));
            return false;
        }
    }
    readposition = lastposition = 0;
    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
