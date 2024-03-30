// Qt
#include <QFile>
#include <QMap>
#include <QUrl>
#include <QReadWriteLock>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"

#include "io/mythmediabuffer.h"
#include "mythiowrapper.h"

// Std
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class MythIOCallback
{
  public:
    MythIOCallback(void* Object, callback_t Callback)
      : m_object(Object),
        m_callback(Callback)
    {
    }
    bool operator==(MythIOCallback rhs) const
        { return (m_object == rhs.m_object) && (m_callback == rhs.m_callback); };

    void       *m_object;
    callback_t  m_callback;
};

static const int s_maxID = 1024 * 1024;

static QReadWriteLock          s_fileWrapperLock;
static QHash<int, MythMediaBuffer*> s_buffers;
static QHash<int, RemoteFile*> s_remotefiles;
static QHash<int, int>         s_localfiles;
static QHash<int, QString>     s_filenames;

static QReadWriteLock          s_dirWrapperLock;
static QHash<int, QStringList> s_remotedirs;
static QHash<int, int>         s_remotedirPositions;
static QHash<int, QString>     s_dirnames;
static QHash<int, DIR*>        s_localdirs;

static QMutex                        s_callbackLock;
static QMultiHash<QString, MythIOCallback> s_fileOpenCallbacks;

#define LOC QString("MythIOWrap: ")

extern "C" {

static int GetNextFileID(void)
{
    int id = 100000;

    for (; id < s_maxID; ++id)
        if (!s_localfiles.contains(id) && !s_remotefiles.contains(id) && !s_buffers.contains(id))
            break;

    if (id == s_maxID)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Too many files are open.");

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetNextFileID: '%1'").arg(id));
    return id;
}

void MythFileOpenRegisterCallback(const char *Pathname, void* Object, callback_t Func)
{
    QMutexLocker locker(&s_callbackLock);
    QString path(Pathname);
    if (s_fileOpenCallbacks.contains(path))
    {
        // if we already have a callback registered for this path with this
        // object then remove the callback and return (i.e. end callback)
        for (auto it = s_fileOpenCallbacks.begin();
             it != s_fileOpenCallbacks.end();
             it++)
        {
            if (Object == it.value().m_object)
            {
                s_fileOpenCallbacks.remove(path, { Object, it.value().m_callback });
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Removing fileopen callback for %1").arg(path));
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 callbacks remaining").arg(s_fileOpenCallbacks.size()));
                return;
            }
        }
    }

    s_fileOpenCallbacks.insert(path, { Object, Func });
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Added fileopen callback for %1").arg(path));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 callbacks open").arg(s_fileOpenCallbacks.size()));
}

int MythFileCheck(int Id)
{
    LOG(VB_FILE, LOG_DEBUG, QString("MythFileCheck: '%1')").arg(Id));
    QWriteLocker locker(&s_fileWrapperLock);
    return (s_localfiles.contains(Id) || s_remotefiles.contains(Id) || s_buffers.contains(Id)) ? 1 : 0;
}

int MythFileOpen(const char *Pathname, int Flags)
{
    LOG(VB_FILE, LOG_DEBUG, QString("MythFileOpen('%1', %2)").arg(Pathname).arg(Flags));

    struct stat fileinfo {};
    if (MythFileStat(Pathname, &fileinfo))
        return -1;

    // libmythdvdnav tries to open() a dir
    if (S_ISDIR(fileinfo.st_mode))
    {
        errno = EISDIR;
        return -1;
    }

    int fileID = -1;
    if (strncmp(Pathname, "myth://", 7) != 0)
    {
        int lfd = open(Pathname, Flags);
        if (lfd < 0)
            return -1;

        s_fileWrapperLock.lockForWrite();
        fileID = GetNextFileID();
        s_localfiles[fileID] = lfd;
        s_filenames[fileID] = Pathname;
        s_fileWrapperLock.unlock();
    }
    else
    {
        MythMediaBuffer *buffer = nullptr;
        RemoteFile *rf = nullptr;

        if ((fileinfo.st_size < 512) && (fileinfo.st_mtime < (time(nullptr) - 300)))
        {
            if (Flags & O_WRONLY)
                rf = new RemoteFile(Pathname, true, false); // Writeable
            else
                rf = new RemoteFile(Pathname, false, true); // Read-Only
            if (!rf)
                return -1;
        }
        else
        {
            if (Flags & O_WRONLY)
            {
                buffer = MythMediaBuffer::Create(Pathname, true, false,
                                                 MythMediaBuffer::kDefaultOpenTimeout, true); // Writeable
            }
            else
            {
                buffer = MythMediaBuffer::Create(Pathname, false, true,
                                                 MythMediaBuffer::kDefaultOpenTimeout, true); // Read-Only
            }

            if (!buffer)
                return -1;

            buffer->Start();
        }

        s_fileWrapperLock.lockForWrite();
        fileID = GetNextFileID();

        if (rf)
            s_remotefiles[fileID] = rf;
        else if (buffer)
            s_buffers[fileID] = buffer;

        s_filenames[fileID] = Pathname;
        s_fileWrapperLock.unlock();
    }

    s_callbackLock.lock();
    QString path(Pathname);
    for (auto it = s_fileOpenCallbacks.begin();
         it != s_fileOpenCallbacks.end();
         it++)
    {
        if (path.startsWith(it.key())) {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Calling callback %1 for %2")
                .arg(it.key()).arg((qulonglong)it.value().m_callback));
            it.value().m_callback(it.value().m_object);
        }
    }
    s_callbackLock.unlock();

    return fileID;
}

int MythfileClose(int FileID)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythfileClose: '%1").arg(FileID));

    // FIXME - surely this needs to hold write lock?
    QReadLocker locker(&s_fileWrapperLock);
    if (s_buffers.contains(FileID))
    {
        MythMediaBuffer *buffer = s_buffers[FileID];
        s_buffers.remove(FileID);
        delete buffer;
        return 0;
    }

    if (s_remotefiles.contains(FileID))
    {
        RemoteFile *rf = s_remotefiles[FileID];
        s_remotefiles.remove(FileID);
        delete rf;
        return 0;
    }

    if (s_localfiles.contains(FileID))
    {
        close(s_localfiles[FileID]);
        s_localfiles.remove(FileID);
        return 0;
    }

    return -1;
}

#ifdef _WIN32
#   undef  lseek
#   define lseek  _lseeki64
#   undef  off_t
#   define off_t off64_t
#endif
off_t MythFileSeek(int FileID, off_t Offset, int Whence)
{
    off_t result = -1;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythFileSeek(%1, %2, %3)")
        .arg(FileID).arg(Offset).arg(Whence));

    s_fileWrapperLock.lockForRead();
    if (s_buffers.contains(FileID))
        result = s_buffers[FileID]->Seek(Offset, Whence);
    else if (s_remotefiles.contains(FileID))
        result = s_remotefiles[FileID]->Seek(Offset, Whence);
    else if (s_localfiles.contains(FileID))
        result = lseek(s_localfiles[FileID], Offset, Whence);
    s_fileWrapperLock.unlock();

    return result;
}

off_t MythFileTell(int FileID)
{
    off_t result = -1;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythFileTell(%1)").arg(FileID));

    s_fileWrapperLock.lockForRead();
    if (s_buffers.contains(FileID))
        result = s_buffers[FileID]->Seek(0, SEEK_CUR);
    else if (s_remotefiles.contains(FileID))
        result = s_remotefiles[FileID]->Seek(0, SEEK_CUR);
    else if (s_localfiles.contains(FileID))
        result = lseek(s_localfiles[FileID], 0, SEEK_CUR);
    s_fileWrapperLock.unlock();

    return result;
}

#ifdef _WIN32
#   undef  lseek
#   undef  off_t
#endif

ssize_t MythFileRead(int FileID, void *Buffer, size_t Count)
{
    ssize_t result = -1;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythFileRead(%1, %2, %3)")
        .arg(FileID).arg(reinterpret_cast<long long>(Buffer)).arg(Count));

    s_fileWrapperLock.lockForRead();
    if (s_buffers.contains(FileID))
        result = s_buffers[FileID]->Read(Buffer, static_cast<int>(Count));
    else if (s_remotefiles.contains(FileID))
        result = s_remotefiles[FileID]->Read(Buffer, static_cast<int>(Count));
    else if (s_localfiles.contains(FileID))
        result = read(s_localfiles[FileID], Buffer, Count);
    s_fileWrapperLock.unlock();

    return result;
}

ssize_t MythFileWrite(int FileID, void *Buffer, size_t Count)
{
    ssize_t result = -1;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythFileWrite(%1, %2, %3)")
        .arg(FileID).arg(reinterpret_cast<long long>(Buffer)).arg(Count));

    s_fileWrapperLock.lockForRead();
    if (s_buffers.contains(FileID))
        result = s_buffers[FileID]->Write(Buffer, static_cast<uint>(Count));
    else if (s_remotefiles.contains(FileID))
        result = s_remotefiles[FileID]->Write(Buffer, static_cast<int>(Count));
    else if (s_localfiles.contains(FileID))
        result = write(s_localfiles[FileID], Buffer, Count);
    s_fileWrapperLock.unlock();

    return result;
}

int MythFileStat(const char *Path, struct stat *Buf)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythfileStat('%1', %2)")
        .arg(Path).arg(reinterpret_cast<long long>(Buf)));

    if (strncmp(Path, "myth://", 7) == 0)
    {
        bool res = RemoteFile::Exists(Path, Buf);
        if (res)
            return 0;
    }

    return stat(Path, Buf);
}

int MythFileStatFD(int FileID, struct stat *Buf)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythFileStatFD(%1, %2)")
        .arg(FileID).arg(reinterpret_cast<long long>(Buf)));

    s_fileWrapperLock.lockForRead();
    if (!s_filenames.contains(FileID))
    {
        s_fileWrapperLock.unlock();
        return -1;
    }
    QString filename = s_filenames[FileID];
    s_fileWrapperLock.unlock();

    return MythFileStat(filename.toLocal8Bit().constData(), Buf);
}

/*
 * This function exists for the use of dvd_reader.c, thus the return
 * value of int instead of bool.  C doesn't have a bool type.
 */
int MythFileExists(const char *Path, const char *File)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythFileExists('%1', '%2')")
        .arg(Path, File));

    bool ret = false;
    if (strncmp(Path, "myth://", 7) == 0)
        ret = RemoteFile::Exists(QString("%1/%2").arg(Path, File));
    else
        ret = QFile::exists(QString("%1/%2").arg(Path, File));
    return static_cast<int>(ret);
}

static int GetNextDirID(void)
{
    int id = 100000;
    for (; id < s_maxID; ++id)
        if (!s_localdirs.contains(id) && !s_remotedirs.contains(id))
            break;

    if (id == s_maxID)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Too many directories are open.");
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("GetNextDirID: '%1'").arg(id));

    return id;
}

int MythDirCheck(int DirID)
{
    LOG(VB_FILE, LOG_DEBUG, QString("MythDirCheck: '%1'").arg(DirID));
    s_dirWrapperLock.lockForWrite();
    int result = ((s_localdirs.contains(DirID) || s_remotedirs.contains(DirID))) ? 1 : 0;
    s_dirWrapperLock.unlock();
    return result;
}

int MythDirOpen(const char *DirName)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythDirOpen: '%1'").arg(DirName));

    if (strncmp(DirName, "myth://", 7) != 0)
    {
        DIR *dir = opendir(DirName);
        if (dir)
        {
            s_dirWrapperLock.lockForWrite();
            int id = GetNextDirID();
            s_localdirs[id] = dir;
            s_dirnames[id] = DirName;
            s_dirWrapperLock.unlock();
            return id;
        }
    }
    else
    {
        QStringList list;
        QUrl qurl(DirName);
        QString storageGroup = qurl.userName();

        list.clear();
        if (storageGroup.isEmpty())
            storageGroup = "Default";

        list << "QUERY_SG_GETFILELIST" << qurl.host() << storageGroup;

        QString path = qurl.path();
        if (!qurl.fragment().isEmpty())
            path += "#" + qurl.fragment();

        list << path << "1";

        bool ok = gCoreContext->SendReceiveStringList(list);

        if ((!ok) || ((list.size() == 1) && (list[0] == "EMPTY LIST")))
            list.clear();

        s_dirWrapperLock.lockForWrite();
        int id = GetNextDirID();
        s_remotedirs[id] = list;
        s_remotedirPositions[id] = 0;
        s_dirnames[id] = DirName;
        s_dirWrapperLock.unlock();
        return id;
    }

    return 0;
}

int MythDirClose(int DirID)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythDirClose: '%1'").arg(DirID));

    QReadLocker locker(&s_dirWrapperLock);
    if (s_remotedirs.contains(DirID))
    {
        s_remotedirs.remove(DirID);
        s_remotedirPositions.remove(DirID);
        return 0;
    }

    if (s_localdirs.contains(DirID))
    {
        int result = closedir(s_localdirs[DirID]);
        if (result == 0)
            s_localdirs.remove(DirID);
        return result;
    }

    return -1;
}

char *MythDirRead(int DirID)
{
    LOG(VB_FILE, LOG_DEBUG, LOC + QString("MythDirRead: '%1'").arg(DirID));

    QReadLocker locker(&s_dirWrapperLock);
    if (s_remotedirs.contains(DirID))
    {
        int pos = s_remotedirPositions[DirID];
        if (s_remotedirs[DirID].size() >= (pos + 1))
        {
            char* result = strdup(s_remotedirs[DirID][pos].toLocal8Bit().constData());
            pos++;
            s_remotedirPositions[DirID] = pos;
            return result;
        }
    }
    else if (s_localdirs.contains(DirID))
    {
        struct dirent *dir = readdir(s_localdirs[DirID]);
        if (dir != nullptr)
            return strdup(dir->d_name);
    }

    return nullptr;
}

} // extern "C"

