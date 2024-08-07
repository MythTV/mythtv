#include <iostream>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

// POSIX C headers
#include <unistd.h>
#include <fcntl.h>

#include "mythconfig.h"

#ifndef O_LARGEFILE
static constexpr int8_t O_LARGEFILE { 0 };
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

static constexpr std::chrono::milliseconds MAX_FILE_CHECK { 500ms };

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
        {
            strlist.clear();
        }
    }
    else
    {
        ok = gCoreContext->SendReceiveStringList(strlist);
    }

    return ok;
}

RemoteFile::RemoteFile(QString url, bool write, bool usereadahead,
                       std::chrono::milliseconds timeout,
                       const QStringList *possibleAuxiliaryFiles) :
    m_path(std::move(url)),
    m_useReadAhead(usereadahead),  m_timeoutMs(timeout),
    m_writeMode(write)
{
    if (m_writeMode)
    {
        m_useReadAhead = false;
        m_timeoutMs = -1ms;
    }
    else if (possibleAuxiliaryFiles)
    {
        m_possibleAuxFiles = *possibleAuxiliaryFiles;
    }

    if (!m_path.isEmpty())
        Open();

    LOG(VB_FILE, LOG_DEBUG, QString("RemoteFile(%1)").arg(m_path));
}

RemoteFile::~RemoteFile()
{
    Close();
    if (m_controlSock)
    {
        m_controlSock->DecrRef();
        m_controlSock = nullptr;
    }
    if (m_sock)
    {
        m_sock->DecrRef();
        m_sock = nullptr;
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
    return isLocal(m_path);
}

MythSocket *RemoteFile::openSocket(bool control)
{
    QUrl qurl(m_path);
    QString dir;

    QString host = qurl.host();
    int port = qurl.port();

    dir = qurl.path();

    if (qurl.hasQuery())
        dir += "?" + QUrl::fromPercentEncoding(
            qurl.query(QUrl::FullyEncoded).toLocal8Bit());

    if (qurl.hasFragment())
        dir += "#" + qurl.fragment();

    QString sgroup = qurl.userName();

    auto *lsock = new MythSocket();
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
        return nullptr;
    }

    QString hostname = GetMythDB()->GetHostName();

    QStringList strlist;

#ifndef IGNORE_PROTO_VER_MISMATCH
    if (!gCoreContext->CheckProtoVersion(lsock, 5s))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Failed validation to server %1:%2").arg(host).arg(port));
        lsock->DecrRef();
        return nullptr;
    }
#endif

    if (control)
    {
        strlist.append(QString("ANN Playback %1 %2")
                       .arg(hostname).arg(static_cast<int>(false)));
        if (!lsock->SendReceiveStringList(strlist))
        {
            LOG(VB_GENERAL, LOG_ERR, loc +
                QString("Could not read string list from server %1:%2")
                    .arg(host).arg(port));
            lsock->DecrRef();
            return nullptr;
        }
    }
    else
    {
        strlist.push_back(QString("ANN FileTransfer %1 %2 %3 %4")
                          .arg(hostname).arg(static_cast<int>(m_writeMode))
                          .arg(static_cast<int>(m_useReadAhead)).arg(m_timeoutMs.count()));
        strlist << QString("%1").arg(dir);
        strlist << sgroup;

        for (const auto& fname : std::as_const(m_possibleAuxFiles))
            strlist << fname;

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
            auto it = strlist.begin(); ++it;
            m_recorderNum = (*it).toInt(); ++it;
            m_fileSize = (*(it)).toLongLong(); ++it;
            for (; it != strlist.end(); ++it)
                m_auxFiles << *it;
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
        lsock = nullptr;
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
        return m_writeMode ? (m_fileWriter != nullptr) : (m_localFile != -1);
    }
    return m_sock && m_controlSock;
}

bool RemoteFile::Open(void)
{
    if (isOpen())
        return true;

    QMutexLocker locker(&m_lock);
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
        if (m_writeMode)
        {
            // make sure the directories are created if necessary
            QFileInfo fi(m_path);
            QDir dir(fi.path());
            if (!dir.exists())
            {
                LOG(VB_FILE, LOG_WARNING, QString("RemoteFile::Open(%1) creating directories")
                    .arg(m_path));

                if (!dir.mkpath(fi.path()))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("RemoteFile::Open(%1) failed to create the directories")
                        .arg(m_path));
                    return false;
                }
            }

            m_fileWriter = new ThreadedFileWriter(m_path,
                                                  O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                                  0644);

            if (!m_fileWriter->Open())
            {
                delete m_fileWriter;
                m_fileWriter = nullptr;
                LOG(VB_FILE, LOG_ERR, QString("RemoteFile::Open(%1) write mode error")
                    .arg(m_path));
                return false;
            }
            SetBlocking();
            return true;
        }

        // local mode, read only
        if (!Exists(m_path))
        {
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Open(%1) Error: Does not exist").arg(m_path));
            return false;
        }

        m_localFile = ::open(m_path.toLocal8Bit().constData(), O_RDONLY);
        if (m_localFile == -1)
        {
            LOG(VB_FILE, LOG_ERR, QString("RemoteFile::Open(%1) Error: %2")
                .arg(m_path, strerror(errno)));
            return false;
        }
        return true;
    }
    m_controlSock = openSocket(true);
    if (!m_controlSock)
        return false;

    m_sock = openSocket(false);
    if (!m_sock)
    {
        // Close the sockets if we received an error so that isOpen() will
        // return false if the caller tries to use the RemoteFile.
        Close(true);
        return false;
    }
    m_canResume = true;

    return true;
}

bool RemoteFile::ReOpen(const QString& newFilename)
{
    if (isLocal())
    {
        if (isOpen())
        {
            Close();
        }
        m_path = newFilename;
        return Open();
    }

    QMutexLocker locker(&m_lock);

    if (!CheckConnection(false))
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::ReOpen(): Couldn't connect");
        return false;
    }

    QStringList strlist( m_query.arg(m_recorderNum) );
    strlist << "REOPEN";
    strlist << newFilename;

    m_controlSock->SendReceiveStringList(strlist);

    m_lock.unlock();

    bool retval = false;
    if (!strlist.isEmpty())
        retval = (strlist[0].toInt() != 0);

    return retval;
}

void RemoteFile::Close(bool haslock)
{
    if (isLocal())
    {
        if (m_localFile >= 0)
            ::close(m_localFile);
        m_localFile = -1;
        delete m_fileWriter;
        m_fileWriter = nullptr;
        return;
    }
    if (!m_controlSock)
        return;

    QStringList strlist( m_query.arg(m_recorderNum) );
    strlist << "DONE";

    if (!haslock)
    {
        m_lock.lock();
    }
    if (m_controlSock->IsConnected() && !m_controlSock->SendReceiveStringList(
            strlist, 0, MythSocket::kShortTimeout))
    {
        LOG(VB_GENERAL, LOG_ERR, "Remote file timeout.");
    }

    if (m_sock)
    {
        m_sock->DecrRef();
        m_sock = nullptr;
    }
    if (m_controlSock)
    {
        m_controlSock->DecrRef();
        m_controlSock = nullptr;
    }

    if (!haslock)
    {
        m_lock.unlock();
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

    struct stat fileinfo {};
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
        return {};

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
        QString("RemoteFile::CopyFile: Copying file from '%1' to '%2'").arg(src, dst));

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
    int srcLen = 0;

    while ((srcLen = srcFile.Read(buf, readSize)) > 0)
    {
        int dstLen = dstFile.Write(buf, srcLen);

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
        struct stat fileinfo {};
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
        QString("RemoteFile::MoveFile: Moving file from '%1' to '%2'").arg(src, dst));

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
    QMutexLocker locker(&m_lock);
    if (!m_sock)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Reset(): Called with no socket");
        return;
    }
    m_sock->Reset();
}

long long RemoteFile::Seek(long long pos, int whence, long long curpos)
{
    QMutexLocker locker(&m_lock);

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
        if (m_writeMode)
            return m_fileWriter->Seek(pos, whence);

        long long offset = 0LL;
        if (whence == SEEK_SET)
        {
            QFileInfo info(m_path);
            offset = std::min(pos, info.size());
        }
        else if (whence == SEEK_END)
        {
            QFileInfo info(m_path);
            offset = info.size() + pos;
        }
        else if (whence == SEEK_CUR)
        {
            offset = ((curpos > 0) ? curpos : ::lseek64(m_localFile, 0, SEEK_CUR)) + pos;
        }
        else
        {
            return -1;
        }

        off64_t localpos = ::lseek64(m_localFile, pos, whence);
        if (localpos != pos)
        {
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Seek(): Couldn't seek to offset %1")
                .arg(offset));
            return -1;
        }
        return localpos;
    }

    if (!CheckConnection(false))
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Seek(): Couldn't connect");
        return -1;
    }

    QStringList strlist( m_query.arg(m_recorderNum) );
    strlist << "SEEK";
    strlist << QString::number(pos);
    strlist << QString::number(whence);
    if (curpos > 0)
        strlist << QString::number(curpos);
    else
        strlist << QString::number(m_readPosition);

    bool ok = m_controlSock->SendReceiveStringList(strlist);

    if (ok && !strlist.isEmpty())
    {
        m_lastPosition = m_readPosition = strlist[0].toLongLong();
        m_sock->Reset();
        return strlist[0].toLongLong();
    }
    m_lastPosition = 0LL;
    return -1;
}

int RemoteFile::Write(const void *data, int size)
{
    int recv = 0;
    int sent = 0;
    unsigned zerocnt = 0;
    bool error = false;
    bool response = false;

    if (!m_writeMode)
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
        return m_fileWriter->Write(data, size);
    }

    QMutexLocker locker(&m_lock);

    if (!CheckConnection())
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Write(): Couldn't connect");
        return -1;
    }

    QStringList strlist( m_query.arg(m_recorderNum) );
    strlist << "WRITE_BLOCK";
    strlist << QString::number(size);
    bool ok = m_controlSock->WriteStringList(strlist);
    if (!ok)
    {
        LOG(VB_NETWORK, LOG_ERR,
            "RemoteFile::Write(): Block notification failed");
        return -1;
    }

    recv = size;
    while (sent < recv && !error && zerocnt++ < 50)
    {
        int ret = m_sock->Write((char*)data + sent, recv - sent);
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

        if (m_controlSock->IsDataAvailable() &&
            m_controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.isEmpty())
        {
            recv = strlist[0].toInt(); // -1 on backend error
            response = true;
        }
    }

    if (!error && !response)
    {
        if (m_controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
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
        m_lastPosition += sent;
    }

    return sent;
}

int RemoteFile::Read(void *data, int size)
{
    int recv = 0;
    int sent = 0;
    bool error = false;
    bool response = false;

    QMutexLocker locker(&m_lock);

    if (isLocal())
    {
        if (m_writeMode)
        {
            LOG(VB_FILE, LOG_ERR, "RemoteFile:Read() called in writing mode");
            return -1;
        }
        if (isOpen())
        {
            return ::read(m_localFile, data, size);
        }
        LOG(VB_FILE, LOG_ERR, "RemoteFile:Read() called when local file not opened");
        return -1;
    }

    if (!CheckConnection())
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Read(): Couldn't connect");
        return -1;
    }

    if (m_sock->IsDataAvailable())
    {
        LOG(VB_NETWORK, LOG_ERR,
                "RemoteFile::Read(): Read socket not empty to start!");
        m_sock->Reset();
    }

    while (m_controlSock->IsDataAvailable())
    {
        LOG(VB_NETWORK, LOG_WARNING,
                "RemoteFile::Read(): Control socket not empty to start!");
        m_controlSock->Reset();
    }

    QStringList strlist( m_query.arg(m_recorderNum) );
    strlist << "REQUEST_BLOCK";
    strlist << QString::number(size);
    bool ok = m_controlSock->WriteStringList(strlist);
    if (!ok)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Read(): Block request failed");
        return -1;
    }

    sent = size;

    std::chrono::milliseconds waitms { 30ms };
    MythTimer mtimer;
    mtimer.start();

    while (recv < sent && !error && mtimer.elapsed() < 10s)
    {
        int ret = m_sock->Read(((char *)data) + recv, sent - recv, waitms);

        if (ret > 0)
            recv += ret;
        else if (ret < 0)
            error = true;

        waitms += (waitms < 200ms) ? 20ms : 0ms;

        if (m_controlSock->IsDataAvailable() &&
            m_controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.isEmpty())
        {
            sent = strlist[0].toInt(); // -1 on backend error
            response = true;
            if (ret < sent)
            {
                // We have received less than what the server sent, retry immediately
                ret = m_sock->Read(((char *)data) + recv, sent - recv, waitms);
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
        if (m_controlSock->ReadStringList(strlist, 1500ms) &&
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
        LOG(VB_GENERAL, LOG_WARNING,
            QString("RemoteFile::Read(): sent %1 != recv %2")
            .arg(sent).arg(recv));
        recv = -1;

        // The TCP socket is dropped if there's a timeout, so we reconnect
        if (!Resume())
        {
            LOG(VB_GENERAL, LOG_WARNING, "RemoteFile::Read(): Resume failed.");
        }
        else
        {
            LOG(VB_GENERAL, LOG_NOTICE, "RemoteFile::Read(): Resume success.");
        }
    }
    else
    {
        m_lastPosition += recv;
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
        if (isOpen() && m_writeMode)
        {
            m_fileWriter->Flush();
        }
        if (Exists(m_path))
        {
            QFileInfo info(m_path);
            return info.size();
        }
        return -1;
    }

    QMutexLocker locker(&m_lock);
    return m_fileSize;
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

    QMutexLocker locker(&m_lock);

    if (m_completed ||
        (m_lastSizeCheck.isRunning() && m_lastSizeCheck.elapsed() < MAX_FILE_CHECK))
    {
        return m_fileSize;
    }

    if (!CheckConnection())
    {
        // Can't establish a new connection, using system one
        struct stat fileinfo {};

        if (Exists(m_path, &fileinfo))
        {
            m_fileSize = fileinfo.st_size;
        }
        return m_fileSize;
    }

    QStringList strlist(m_query.arg(m_recorderNum));
    strlist << "REQUEST_SIZE";

    bool ok = m_controlSock->SendReceiveStringList(strlist);

    if (ok && !strlist.isEmpty())
    {
        bool validate = false;
        long long size = strlist[0].toLongLong(&validate);

        if (validate)
        {
            if (strlist.count() >= 2)
            {
                m_completed = (strlist[1].toInt() != 0);
            }
            m_fileSize = size;
        }
        else
        {
            struct stat fileinfo {};

            if (Exists(m_path, &fileinfo))
            {
                m_fileSize = fileinfo.st_size;
            }
        }
        m_lastSizeCheck.restart();
        return m_fileSize;
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
    if (m_timeoutIsFast == fast)
        return;

    QMutexLocker locker(&m_lock);

    // The m_controlSock variable is valid if the CheckConnection
    // function returns true.  The local case has already been
    // handled.  The CheckConnection function can call Resume which
    // calls Close, which deletes m_controlSock.  However, the
    // subsequent call to OpenInternal is guaranteed to recreate the
    // socket or return false for a non-local connection, and this must
    // be a non-local connection if this line of code is executed.
    if (!CheckConnection())
    {
        LOG(VB_NETWORK, LOG_ERR,
            "RemoteFile::SetTimeout(): Couldn't connect");
        return;
    }
    if (m_controlSock == nullptr)
        return;

    QStringList strlist( m_query.arg(m_recorderNum) );
    strlist << "SET_TIMEOUT";
    strlist << QString::number((int)fast);

    m_controlSock->SendReceiveStringList(strlist);

    m_timeoutIsFast = fast;
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

    if (strlist.size() > 1) {
        if (!strlist[1].isEmpty() && (strlist[1].toInt() != -1))
            result = MythDate::fromSecsSinceEpoch(strlist[1].toLongLong());
        else
            result = QDateTime();;
    }

    return result;
}

QDateTime RemoteFile::LastModified(void) const
{
    return LastModified(m_path);
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

    return {};
}

/**
 *  \brief Search all BE's for files in the give storage group
 *  \param filename the partial path and filename to look for or regular espression (QRegularExpression)
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
                                   .arg(filename, host, storageGroup)
                                   .arg(useRegex).arg(allowFallback));

    if (filename.isEmpty() || storageGroup.isEmpty())
        return {};

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
                                           .arg(fi.path(), fi.fileName()));

            for (int x = 0; x < files.size(); x++)
            {
                LOG(VB_FILE, LOG_INFO, QString("RemoteFile::FindFileList: Found '%1 - %2'")
                                               .arg(x).arg(files[x]));
            }

            QStringList filteredFiles = files.filter(QRegularExpression(fi.fileName()));
            for (const QString& file : std::as_const(filteredFiles))
            {
                strList << MythCoreContext::GenMythURL(gCoreContext->GetHostName(),
                                                       gCoreContext->GetBackendServerPort(),
                                                       fi.path() + '/' + file,
                                                       storageGroup);
            }
        }
        else
        {
            if (!sgroup.FindFile(filename).isEmpty())
            {
                strList << MythCoreContext::GenMythURL(hostName,
                                                       gCoreContext->GetBackendServerPort(hostName),
                                                       filename, storageGroup);
            }
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
            if (!strList.empty() && !strList[0].isEmpty() &&
                strList[0] != "NOT FOUND" && !strList[0].startsWith("ERROR: "))
                return strList;
        }
    }

    return {};
}

/**
 *  \brief Set write blocking mode for the ThreadedFileWriter instance
 *  \param block false if not blocking, true if blocking
 *  \return old mode value
 */
bool RemoteFile::SetBlocking(bool block)
{
    if (m_fileWriter)
    {
        return m_fileWriter->SetBlocking(block);
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
    if (!m_canResume)
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
    return m_sock && m_controlSock &&
           m_sock->IsConnected() && m_controlSock->IsConnected();
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
        m_readPosition = m_lastPosition;
        if (SeekInternal(m_lastPosition, SEEK_SET) < 0)
        {
            Close(true);
            LOG(VB_FILE, LOG_ERR,
                QString("RemoteFile::Resume: Enable to re-seek into last known "
                        "position (%1").arg(m_lastPosition));
            return false;
        }
    }
    m_readPosition = m_lastPosition = 0;
    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
