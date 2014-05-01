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
    localFile(NULL),      fileWriter(NULL)
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

bool RemoteFile::isLocal(const QString &path)
{
    bool is_local = !path.isEmpty() &&
        !path.startsWith("/dev") && !path.startsWith("myth:") &&
        (path.startsWith("/") || QFile::exists(path));
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
        port = GetMythDB()->GetSettingOnHost("BackendServerPort", host).toInt();

        // if we still have no port use the default
        if (port <= 0)
            port = 6543;
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
        return writemode ? (fileWriter != NULL) : (localFile != NULL);
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

/** \fn RemoteFile::OpenInternal(void)
 *  \brief Attempts to resume from a disconnected step. Must have lock
 *  \return True if reconnection succeeded
 *  \param bool indicating we own the lock
 */
bool RemoteFile::OpenInternal()
{
    if (isLocal())
    {
        if (!Exists(path))
        {
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Open(%1) Error: Do not exist").arg(path));
            return false;
        }
        if (writemode)
        {
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
        localFile = new QFile(path);
        if (!localFile->open(QIODevice::ReadOnly))
        {
            LOG(VB_FILE, LOG_ERR, QString("RemoteFile::Open(%1) Error: %2")
                .arg(path).arg(localFile->error()));
            delete localFile;
            localFile = NULL;
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
        delete localFile;
        localFile = NULL;
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

    if (isLocal(url) || (gCoreContext->IsMasterBackend() &&
        host == gCoreContext->GetMasterHostName()))
    {
       LOG(VB_FILE, LOG_INFO,
           QString("RemoteFile::Exists(): looking for local file: %1").arg(url));

        bool fileExists = false;
        QString fullFilePath = "";

        if (url.startsWith("myth:"))
        {
            StorageGroup sGroup(sgroup, host);
            fullFilePath = sGroup.FindFile(filename);
            if (!fullFilePath.isEmpty())
                fileExists = true;
        }
        else
        {
            QFileInfo info(url);
            fileExists = info.exists() && info.isFile();
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

    gCoreContext->SendReceiveStringList(strlist);

    bool result = false;
    if (!strlist.isEmpty() && strlist[0] == "1")
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

    gCoreContext->SendReceiveStringList(strlist);
    if (!strlist.isEmpty())
    {
        result = strlist[0];
    }

    return result;
}

bool RemoteFile::CopyFile (const QString& src, const QString& dst)
{
    // sanity check
    if (src == dst)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::CopyFile: Cannot copy a file to itself");
        return false;
    }

    const int readSize = 2 * 1024 * 1024;
    char *buf = new char[readSize];
    if (!buf)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteFile::CopyFile: ERROR, unable to allocate copy buffer");
        return false;
    }

    RemoteFile srcFile(src, false);
    if (!srcFile.isOpen())
    {
         LOG(VB_GENERAL, LOG_ERR,
             QString("RemoteFile::CopyFile: Failed to open file (%1) for reading.").arg(src));
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

    int srcLen, dstLen;

    while ((srcLen = srcFile.Read(buf, readSize)) > 0)
    {
        dstLen = dstFile.Write(buf, srcLen);

        if (dstLen == -1 || srcLen != dstLen)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "RemoteFile::CopyFile:: Error while trying to write to destination file.");
            srcFile.Close();
            dstFile.Close();
            delete[] buf;
            return false;
        }
    }

    srcFile.Close();
    dstFile.Close();
    delete[] buf;

    return true;
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
            offset = min(pos, localFile->size());
        }
        else if (whence == SEEK_END)
        {
            offset = localFile->size() + pos;
        }
        else if (whence == SEEK_CUR)
        {
            offset = ((curpos > 0) ? curpos : localFile->pos()) + pos;
        }
        else
            return -1;
        if (!localFile->seek(offset))
        {
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Seek(): Couldn't seek to offset %1")
                .arg(offset));
            return -1;
        }
        return localFile->pos();
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
            QDataStream stream(localFile);
            return stream.readRawData(static_cast<char*>(data), size);
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
        if (isOpen())
        {
            if (writemode)
            {
                fileWriter->Flush();
                QFileInfo info(path);
                return info.size();
            }
            return localFile->size();
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

    if (completed || lastSizeCheck.elapsed() < MAX_FILE_CHECK)
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

/** \fn RemoteFile::FindFile(const QString& filename, const QString& host, const QString& storageGroup)
 *  \brief Search all BE's for a file in the give storage group
 *  \param filename the partial path and filename to look for
 *  \param host search this host first if given or default to the master BE if empty
 *  \param storageGroup the name of the storage group to search
 *  \return a myth URL pointing to the file or empty string if not found
 */
QString RemoteFile::FindFile(const QString& filename, const QString& host, const QString& storageGroup)
{
    LOG(VB_FILE, LOG_INFO, QString("RemoteFile::FindFile(): looking for '%1' on '%2' in group '%3'")
                                   .arg(filename).arg(host).arg(storageGroup));

    if (filename.isEmpty() || storageGroup.isEmpty())
        return QString();

    QStringList strList;
    QString hostName = host;

    if (hostName.isEmpty())
        hostName = gCoreContext->GetMasterHostName();

    if (gCoreContext->IsMasterBackend() &&
        hostName == gCoreContext->GetMasterHostName())
    {
        StorageGroup sGroup(storageGroup, hostName);
        if (!sGroup.FindFile(filename).isEmpty())
            return gCoreContext->GenMythURL(hostName, 0, filename, storageGroup);
    }
    else
    {
        // first check the given host
        strList << "QUERY_SG_FILEQUERY" << hostName << storageGroup << filename << 0;
        if (gCoreContext->SendReceiveStringList(strList))
        {
            if (strList.size() > 0 && strList[0] != "EMPTY LIST" && !strList[0].startsWith("SLAVE UNREACHABLE"))
                return gCoreContext->GenMythURL(hostName, 0, filename, storageGroup);
        }
    }

    // not found so search all hosts that has a directory defined for the give storage group

    // get a list of hosts
    MSqlQuery query(MSqlQuery::InitCon());

    QString sql = "SELECT DISTINCT hostname "
                  "FROM storagegroup "
                  "WHERE groupname = :GROUP "
                  "AND hostname != :HOSTNAME";
    query.prepare(sql);
    query.bindValue(":GROUP", storageGroup);
    query.bindValue(":HOSTNAME", hostName);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("RemoteFile::FindFile() get host list", query);
        return QString();
    }

    while(query.next())
    {
        hostName = query.value(0).toString();

        if (gCoreContext->IsMasterBackend() &&
            hostName == gCoreContext->GetMasterHostName())
        {
            StorageGroup sGroup(storageGroup, hostName);
            if (!sGroup.FindFile(filename).isEmpty())
                return gCoreContext->GenMythURL(hostName, 0, filename, storageGroup);
            else
                continue;
        }

        strList.clear();
        strList << "QUERY_SG_FILEQUERY" << hostName << storageGroup << filename << 0;

        if (gCoreContext->SendReceiveStringList(strList))
        {
            if (strList.size() > 0 && strList[0] != "EMPTY LIST" && !strList[0].startsWith("SLAVE UNREACHABLE"))
                return gCoreContext->GenMythURL(hostName, 0, filename, storageGroup);
        }
    }

    return QString();
}

/** \fn RemoteFile::SetBlocking(void)
 *  \brief Set write blocking mode for the ThreadedFileWriter instance
 *  \return old mode value
 *  \param false if not blocking, true if blocking
 */
bool RemoteFile::SetBlocking(bool block)
{
    if (fileWriter)
    {
        return fileWriter->SetBlocking(block);
    }
    return true;
}

/** \fn RemoteFile::CheckConnection(void)
 *  \brief Check current connection and re-establish it if lost
 *  \return True if connection is working
 *  \param bool indicating if we are to reposition to the last known location if reconnection is required
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

/** \fn RemoteFile::IsConnected(void)
 *  \brief Check if both the control and data sockets are currently connected
 *  \return True if both sockets are connected
 *  \param none
 */
bool RemoteFile::IsConnected(void)
{
    return sock && controlSock &&
           sock->IsConnected() && controlSock->IsConnected();
}

/** \fn RemoteFile::Resume(void)
 *  \brief Attempts to resume from a disconnected step. Must have lock
 *  \return True if reconnection succeeded
 *  \param bool indicating if we are to reposition to the last known location
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
