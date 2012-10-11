#include <iostream>
using namespace std;

#include <QUrl>

#include "mythconfig.h"
#include "mythdb.h"
#include "remotefile.h"
#include "mythcorecontext.h"
#include "mythsocket.h"
#include "compat.h"
#include "mythtimer.h"
#include "mythdate.h"

RemoteFile::RemoteFile(const QString &_path, bool write, bool useRA,
                       int _timeout_ms,
                       const QStringList *possibleAuxiliaryFiles) :
    path(_path),
    usereadahead(useRA),  timeout_ms(_timeout_ms),
    filesize(-1),         timeoutisfast(false),
    readposition(0),      recordernum(0),
    lock(QMutex::NonRecursive),
    controlSock(NULL),    sock(NULL),
    query("QUERY_FILETRANSFER %1"),
    writemode(write)
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

MythSocket *RemoteFile::openSocket(bool control)
{
    QUrl qurl(path);
    QString dir;

    QString host = qurl.host();
    int port = qurl.port();

    dir = qurl.path();

    if (qurl.hasQuery())
        dir += "?" + QUrl::fromPercentEncoding(qurl.encodedQuery());

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
    if (!gCoreContext->CheckProtoVersion(lsock))
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
        else if (0 < strlist.size() && strlist.size() < 3 &&
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

    if (strlist.empty() || strlist[0] == "ERROR")
    {
        lsock->DecrRef();
        lsock = NULL;
        if (strlist.empty())
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

bool RemoteFile::Open(void)
{
    QMutexLocker locker(&lock);
    controlSock = openSocket(true);
    if (!controlSock)
        return false;

    sock = openSocket(false);
    if (!sock)
    {
        // Close the sockets if we received an error so that isOpen() will
        // return false if the caller tries to use the RemoteFile.
        locker.unlock();
        Close();
        return false;
    }
    return true;
}

bool RemoteFile::ReOpen(QString newFilename)
{
    lock.lock();
    if (!sock)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::ReOpen(): Called with no socket");
        return false;
    }

    if (!sock->IsConnected() || !controlSock->IsConnected())
        return -1;

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "REOPEN";
    strlist << newFilename;

    controlSock->SendReceiveStringList(strlist);

    lock.unlock();

    bool retval = false;
    if (!strlist.empty())
        retval = strlist[0].toInt();

    return retval;
}

void RemoteFile::Close(void)
{
    if (!controlSock)
        return;

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "DONE";

    lock.lock();
    if (!controlSock->SendReceiveStringList(
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

    lock.unlock();
}

bool RemoteFile::DeleteFile(const QString &url)
{
    bool result      = false;
    QUrl qurl(url);
    QString filename = qurl.path();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.right(1) == "#")
        filename = filename + "#" + qurl.fragment();

    if (filename.left(1) == "/")
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty() || sgroup.isEmpty())
        return false;

    QStringList strlist("DELETE_FILE");
    strlist << filename;
    strlist << sgroup;

    gCoreContext->SendReceiveStringList(strlist);

    if (strlist[0] == "1")
        result = true;

    return result;
}

bool RemoteFile::Exists(const QString &url)
{
    struct stat fileinfo;
    return Exists(url, &fileinfo);
}

bool RemoteFile::Exists(const QString &url, struct stat *fileinfo)
{
    QUrl qurl(url);
    QString filename = qurl.path();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.right(1) == "#")
        filename = filename + "#" + qurl.fragment();

    if (filename.left(1) == "/")
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty())
        return false;

    QStringList strlist("QUERY_FILE_EXISTS");
    strlist << filename;
    if (!sgroup.isEmpty())
        strlist << sgroup;

    gCoreContext->SendReceiveStringList(strlist);

    bool result = false;
    if ((strlist.size() >= 1) && strlist[0] == "1")
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
#ifndef USING_MINGW
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
    QString result;
    QUrl qurl(url);
    QString filename = qurl.path();
    QString hostname = qurl.host();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.right(1) == "#")
        filename = filename + "#" + qurl.fragment();

    if (filename.left(1) == "/")
        filename = filename.right(filename.length()-1);

    if (filename.isEmpty() || sgroup.isEmpty())
        return QString();

    QStringList strlist("QUERY_FILE_HASH");
    strlist << filename;
    strlist << sgroup;
    strlist << hostname;

    gCoreContext->SendReceiveStringList(strlist);
    result = strlist[0];

    return result;
}

void RemoteFile::Reset(void)
{
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

    if (!sock)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Seek(): Called with no socket");
        return -1;
    }

    if (!sock->IsConnected() || !controlSock->IsConnected())
    {
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

    if (ok && !strlist.empty())
    {
        readposition = strlist[0].toLongLong();
        sock->Reset();
        return strlist[0].toLongLong();
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

    QMutexLocker locker(&lock);
    if (!sock)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Write(): Called with no socket");
        return -1;
    }

    if (!sock->IsConnected() || !controlSock->IsConnected())
    {
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
            !strlist.empty())
        {
            recv = strlist[0].toInt(); // -1 on backend error
            response = true;
        }
    }

    if (!error && !response)
    {
        if (controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.empty())
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
        sent = -1;

    return sent;
}

int RemoteFile::Read(void *data, int size)
{
    int recv = 0;
    int sent = 0;
    bool error = false;
    bool response = false;

    QMutexLocker locker(&lock);
    if (!sock)
    {
        LOG(VB_NETWORK, LOG_ERR, "RemoteFile::Read(): Called with no socket");
        return -1;
    }

    if (!sock->IsConnected() || !controlSock->IsConnected())
        return -1;

    if (sock->IsDataAvailable())
    {
        LOG(VB_NETWORK, LOG_ERR,
                "RemoteFile::Read(): Read socket not empty to start!");
        sock->Reset();
    }

    while (controlSock->IsDataAvailable())
    {
        LOG(VB_NETWORK, LOG_ERR,
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

    int waitms = 10;
    MythTimer mtimer;
    mtimer.start();

    while (recv < sent && !error && mtimer.elapsed() < 10000)
    {
        MythTimer ctt; // check control socket timer
        ctt.start();
        while ((recv < sent) && (ctt.elapsed() < 500))
        {
            int ret = sock->Read(
                ((char *)data) + recv, sent - recv, 500);
            if (ret > 0)
            {
                recv += ret;
            }
            else if (ret < 0)
            {
                error = true;
            }
            if (waitms < 200)
                waitms += 20;
        }

        if (controlSock->IsDataAvailable() &&
            controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.empty())
        {
            sent = strlist[0].toInt(); // -1 on backend error
            response = true;
        }
    }

    if (!error && !response)
    {
        if (controlSock->ReadStringList(strlist, MythSocket::kShortTimeout) &&
            !strlist.empty())
        {
            sent = strlist[0].toInt(); // -1 on backend error
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                   "RemoteFile::Read(): No response from control socket.");
            sent = -1;
        }
    }

    LOG(VB_NETWORK, LOG_DEBUG,
        QString("Read(): reqd=%1, rcvd=%2, rept=%3, error=%4")
            .arg(size).arg(recv).arg(sent).arg(error));

    if (sent < 0)
        return sent;

    if (error || sent != recv)
        recv = -1;

    return recv;
}

bool RemoteFile::SaveAs(QByteArray &data)
{
    if (filesize < 0)
        return false;

    data.resize(filesize);
    Read(data.data(), filesize);

    return true;
}

void RemoteFile::SetTimeout(bool fast)
{
    if (timeoutisfast == fast)
        return;

    QMutexLocker locker(&lock);
    if (!sock)
    {
        LOG(VB_NETWORK, LOG_ERR,
            "RemoteFile::SetTimeout(): Called with no socket");
        return;
    }

    if (!sock->IsConnected() || !controlSock->IsConnected())
        return;

    QStringList strlist( QString(query).arg(recordernum) );
    strlist << "SET_TIMEOUT";
    strlist << QString::number((int)fast);

    controlSock->SendReceiveStringList(strlist);

    timeoutisfast = fast;
}

QDateTime RemoteFile::LastModified(const QString &url)
{
    QDateTime result;
    QUrl qurl(url);
    QString filename = qurl.path();
    QString sgroup   = qurl.userName();

    if (!qurl.fragment().isEmpty() || url.right(1) == "#")
        filename = filename + "#" + qurl.fragment();

    if (filename.left(1) == "/")
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
