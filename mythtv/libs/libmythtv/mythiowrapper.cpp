#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <QFile>
#include <QMap>
#include <QUrl>
#include <QReadWriteLock>

#include "mythconfig.h"
#include "compat.h"
#include "mythcorecontext.h"
#include "mythverbose.h"
#include "remotefile.h"
#include "ringbuffer.h"

#include "mythiowrapper.h"

const int maxID = 1024 * 1024;

QReadWriteLock            m_fileWrapperLock;
QHash <int, RingBuffer *> m_ringbuffers;
QHash <int, RemoteFile *> m_remotefiles;
QHash <int, int>          m_localfiles;
QHash <int, QString>      m_filenames;

QReadWriteLock            m_dirWrapperLock;
QHash <int, QStringList>  m_remotedirs;
QHash <int, int>          m_remotedirPositions;
QHash <int, DIR *>        m_localdirs;
QHash <int, QString>      m_dirnames;

class Callback
{
  public:
    Callback(void* object, callback_t callback)
      : m_object(object), m_callback(callback) { }
    void       *m_object;
    callback_t  m_callback;
};

QMutex                        m_callbackLock;
QMultiHash<QString, Callback> m_fileOpenCallbacks;

#define LOC     QString("mythiowrapper: ")
#define LOC_ERR QString("mythiowrapper: ERROR: ")

/////////////////////////////////////////////////////////////////////////////

extern "C" {

static int getNextFileID(void)
{
    int id = 100000;

    for (; id < maxID; ++id)
    {
        if ((!m_localfiles.contains(id)) &&
            (!m_remotefiles.contains(id)) &&
            (!m_ringbuffers.contains(id)))
            break;
    }

    if (id == maxID)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "getNextFileID(), too "
                "many files are open.");
    }

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("getNextFileID() = %1").arg(id));

    return id;
}

void mythfile_open_register_callback(const QString path, void* object,
                                     callback_t func)
{
    m_callbackLock.lock();
    if (m_fileOpenCallbacks.contains(path))
    {
        // if we already have a callback registered for this path with this
        // object then remove the callback and return (i.e. end callback)
        QMutableHashIterator<QString,Callback> it(m_fileOpenCallbacks);
        while (it.hasNext())
        {
            it.next();
            if (object == it.value().m_object)
            {
                it.remove();
                VERBOSE(VB_PLAYBACK, LOC +
                    QString("Removing fileopen callback for %1").arg(path));
                VERBOSE(VB_PLAYBACK, LOC + QString("%1 callbacks remaining")
                    .arg(m_fileOpenCallbacks.size()));
                m_callbackLock.unlock();
                return;
            }
        }
    }

    QString new_path = path;
    new_path.detach();
    Callback new_callback(object, func);
    m_fileOpenCallbacks.insert(path, new_callback);
    VERBOSE(VB_PLAYBACK, LOC +
        QString("Added fileopen callback for %1").arg(path));
    VERBOSE(VB_PLAYBACK, LOC + QString("%1 callbacks open")
        .arg(m_fileOpenCallbacks.size()));

    m_callbackLock.unlock();
}

int mythfile_check(int id)
{
    VERBOSE(VB_FILE+VB_EXTRA,
            QString("mythfile_check(%1)").arg(id));
    int result = 0;

    m_fileWrapperLock.lockForWrite();
    if (m_localfiles.contains(id))
        result = 1;
    else if (m_remotefiles.contains(id))
        result = 1;
    else if (m_ringbuffers.contains(id))
        result = 1;
    m_fileWrapperLock.unlock();

    return result;
}

int mythfile_open(const char *pathname, int flags)
{
    VERBOSE(VB_FILE+VB_EXTRA,
            QString("mythfile_open('%1', %2)").arg(pathname).arg(flags));

    struct stat fileinfo;
    if (mythfile_stat(pathname, &fileinfo))
        return -1;

    if (S_ISDIR( fileinfo.st_mode )) // libmythdvdnav tries to open() a dir
        return errno = EISDIR, -1;

    int fileID = -1;
    if (strncmp(pathname, "myth://", 7))
    {
        int lfd = open(pathname, flags);
        if (lfd < 0)
            return -1;

        m_fileWrapperLock.lockForWrite();
        fileID = getNextFileID();
        m_localfiles[fileID] = lfd;
        m_filenames[fileID] = pathname;
        m_fileWrapperLock.unlock();
    }
    else
    {
        RingBuffer *rb = NULL;
        RemoteFile *rf = NULL;

        if ((fileinfo.st_size < 51200) &&
            (fileinfo.st_mtime < (time(NULL) - 300)))
        {
            if (flags & O_WRONLY)
                rf = new RemoteFile(pathname, true, false); // Writeable
            else
                rf = new RemoteFile(pathname, false, true); // Read-Only

            if (!rf)
                return -1;
        }
        else
        {
            if (flags & O_WRONLY)
                rb = RingBuffer::Create(
                    pathname, true, false,
                    RingBuffer::kDefaultOpenTimeout, true); // Writeable
            else
                rb = RingBuffer::Create(
                    pathname, false, true,
                    RingBuffer::kDefaultOpenTimeout, true); // Read-Only

            if (!rb)
                return -1;

            rb->Start();
        }

        m_fileWrapperLock.lockForWrite();
        fileID = getNextFileID();

        if (rf)
            m_remotefiles[fileID] = rf;
        else if (rb)
            m_ringbuffers[fileID] = rb;

        m_filenames[fileID] = pathname;
        m_fileWrapperLock.unlock();
    }

    m_callbackLock.lock();
    if (!m_fileOpenCallbacks.isEmpty())
    {
        QString path(pathname);
        QHashIterator<QString,Callback> it(m_fileOpenCallbacks);
        while (it.hasNext())
        {
            it.next();
            if (path.startsWith(it.key()))
                it.value().m_callback(it.value().m_object);
        }
    }
    m_callbackLock.unlock();

    return fileID;
}

int mythfile_close(int fileID)
{
    int result = -1;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythfile_close(%1)").arg(fileID));

    m_fileWrapperLock.lockForRead();
    if (m_ringbuffers.contains(fileID))
    {
        RingBuffer *rb = m_ringbuffers[fileID];
        m_ringbuffers.remove(fileID);
        delete rb;

        result = 0;
    }
    else if (m_remotefiles.contains(fileID))
    {
        RemoteFile *rf = m_remotefiles[fileID];
        m_remotefiles.remove(fileID);
        delete rf;

        result = 0;
    }
    else if (m_localfiles.contains(fileID))
    {
        close(m_localfiles[fileID]);
        m_localfiles.remove(fileID);
        result = 0;
    }
    m_fileWrapperLock.unlock();

    return result;
}

#ifdef USING_MINGW
#   undef  lseek
#   define lseek  _lseeki64
#   undef  off_t
#   define off_t off64_t
#endif
off_t mythfile_seek(int fileID, off_t offset, int whence)
{
    off_t result = -1;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythfile_seek(%1, %2, %3)")
                                      .arg(fileID).arg(offset).arg(whence));
    
    m_fileWrapperLock.lockForRead();
    if (m_ringbuffers.contains(fileID))
        result = m_ringbuffers[fileID]->Seek(offset, whence);
    else if (m_remotefiles.contains(fileID))
        result = m_remotefiles[fileID]->Seek(offset, whence);
    else if (m_localfiles.contains(fileID))
        result = lseek(m_localfiles[fileID], offset, whence);
    m_fileWrapperLock.unlock();

    return result;
}

off_t mythfile_tell(int fileID)
{
    off_t result = -1;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythfile_tell(%1)").arg(fileID));

    m_fileWrapperLock.lockForRead();
    if (m_ringbuffers.contains(fileID))
        result = m_ringbuffers[fileID]->Seek(0, SEEK_CUR);
    else if (m_remotefiles.contains(fileID))
        result = m_remotefiles[fileID]->Seek(0, SEEK_CUR);
    else if (m_localfiles.contains(fileID))
        result = lseek(m_localfiles[fileID], 0, SEEK_CUR);
    m_fileWrapperLock.unlock();

    return result;
}
#ifdef USING_MINGW
#   undef  lseek
#   undef  off_t
#endif

ssize_t mythfile_read(int fileID, void *buf, size_t count)
{
    ssize_t result = -1;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythfile_read(%1, %2, %3)").arg(fileID)
                                      .arg((long long)buf).arg(count));

    m_fileWrapperLock.lockForRead();
    if (m_ringbuffers.contains(fileID))
        result = m_ringbuffers[fileID]->Read(buf, count);
    else if (m_remotefiles.contains(fileID))
        result = m_remotefiles[fileID]->Read(buf, count);
    else if (m_localfiles.contains(fileID))
        result = read(m_localfiles[fileID], buf, count);
    m_fileWrapperLock.unlock();

    return result;
}

ssize_t mythfile_write(int fileID, void *buf, size_t count)
{
    ssize_t result = -1;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythfile_write(%1, %2, %3)").arg(fileID)
                                      .arg((long long)buf).arg(count));

    m_fileWrapperLock.lockForRead();
    if (m_ringbuffers.contains(fileID))
        result = m_ringbuffers[fileID]->Write(buf, count);
    else if (m_remotefiles.contains(fileID))
        result = m_remotefiles[fileID]->Write(buf, count);
    else if (m_localfiles.contains(fileID))
        result = write(m_localfiles[fileID], buf, count);
    m_fileWrapperLock.unlock();

    return result;
}

int mythfile_stat(const char *path, struct stat *buf)
{
    VERBOSE(VB_FILE+VB_EXTRA,
            QString("mythfile_stat('%1', %2)").arg(path).arg((long long)buf));

    if (!strncmp(path, "myth://", 7))
    {
        bool res = RemoteFile::Exists(path, buf);
        if (res)
            return 0;
    }

    return stat(path, buf);
}

int mythfile_stat_fd(int fileID, struct stat *buf)
{
    VERBOSE(VB_FILE+VB_EXTRA,
            QString("mythfile_stat_fd(%1, %2)").arg(fileID).arg((long long)buf));

    m_fileWrapperLock.lockForRead();
    if (!m_filenames.contains(fileID))
    {
        m_fileWrapperLock.unlock();
        return -1;
    }
    QString filename = m_filenames[fileID];
    m_fileWrapperLock.unlock();

    return mythfile_stat(filename.toLocal8Bit().constData(), buf);
}

int mythfile_exists(const char *path, const char *file)
{
    VERBOSE(VB_FILE+VB_EXTRA,
            QString("mythfile_exists('%1', '%2')").arg(path).arg(file));

    if (!strncmp(path, "myth://", 7))
        return RemoteFile::Exists(QString("%1/%2").arg(path).arg(file));

    return QFile::exists(QString("%1/%2").arg(path).arg(file));
}

//////////////////////////////////////////////////////////////////////////////

static int getNextDirID(void)
{
    int id = 100000;

    for (; id < maxID; ++id)
    {
        if (!m_localdirs.contains(id) && !m_remotedirs.contains(id))
            break;
    }

    if (id == maxID)
        VERBOSE(VB_IMPORTANT, "ERROR: mythiowrapper getNextDirID(), too "
                "many files are open.");

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("getNextDirID() = %1").arg(id));

    return id;
}

int mythdir_check(int id)
{
    VERBOSE(VB_FILE+VB_EXTRA,
            QString("mythdir_check(%1)").arg(id));
    int result = 0;

    m_dirWrapperLock.lockForWrite();
    if (m_localdirs.contains(id))
        result = 1;
    else if (m_remotedirs.contains(id))
        result = 1;
    m_dirWrapperLock.unlock();

    return result;
}

int mythdir_opendir(const char *dirname)
{
    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythdir_opendir(%1)").arg(dirname));

    int id = 0;
    if (strncmp(dirname, "myth://", 7))
    {
        DIR *dir = opendir(dirname);

        m_dirWrapperLock.lockForWrite();
        id = getNextDirID();
        m_localdirs[id] = dir;
        m_dirnames[id] = dirname;
        m_dirWrapperLock.unlock();
    }
    else
    {
        QStringList list;
        QUrl qurl(dirname);
        QString storageGroup = qurl.userName();

        list.clear();

        if (storageGroup.isEmpty())
            storageGroup = "Default";

        list << "QUERY_SG_GETFILELIST";
        list << qurl.host();
        list << storageGroup;

        QString path = qurl.path();
        if (!qurl.fragment().isEmpty())
            path += "#" + qurl.fragment();

        list << path;
        list << "1";

        bool ok = gCoreContext->SendReceiveStringList(list);

        if ((!ok) ||
            ((list.size() == 1) && (list[0] == "EMPTY LIST")))
            list.clear();

        m_dirWrapperLock.lockForWrite();
        id = getNextDirID();
        m_remotedirs[id] = list;
        m_remotedirPositions[id] = 0;
        m_dirnames[id] = dirname;
        m_dirWrapperLock.unlock();
    }

    return id;
}

int mythdir_closedir(int dirID)
{
    int result = -1;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythdir_closedir(%1)").arg(dirID));

    m_dirWrapperLock.lockForRead();
    if (m_remotedirs.contains(dirID))
    {
        m_remotedirs.remove(dirID);
        m_remotedirPositions.remove(dirID);
        result = 0;
    }
    else if (m_localdirs.contains(dirID))
    {
        closedir(m_localdirs[dirID]);
        m_localdirs.remove(dirID);
        result = 0;
    }
    m_dirWrapperLock.unlock();

    return -1;
}

char *mythdir_readdir(int dirID)
{
    char *result = NULL;

    VERBOSE(VB_FILE+VB_EXTRA, LOC + QString("mythdir_readdir(%1)").arg(dirID));

    m_dirWrapperLock.lockForRead();
    if (m_remotedirs.contains(dirID))
    {
        int pos = m_remotedirPositions[dirID];
        if (m_remotedirs[dirID].size() >= (pos+1))
        {
            result = strdup(m_remotedirs[dirID][pos].toLocal8Bit().constData());
            pos++;
            m_remotedirPositions[dirID] = pos;
        }
    }
    else if (m_localdirs.contains(dirID))
    {
        struct dirent *entry = readdir(m_localdirs[dirID]);
        if (entry)
        {
            result = strdup(entry->d_name);
        }
    }
    m_dirWrapperLock.unlock();

    return result;
}

} // extern "C"

/////////////////////////////////////////////////////////////////////////////

/* vim: set expandtab tabstop=4 shiftwidth=4: */
