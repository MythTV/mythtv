#include <cstdlib>
#include <cerrno>

// POSIX C headers
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <QFileInfo>
#include <QDir>

#include "threadedfilewriter.h"
#include "fileringbuffer.h"
#include "mythcontext.h"
#include "remotefile.h"
#include "mythconfig.h" // gives us HAVE_POSIX_FADVISE
#include "mythtimer.h"
#include "mythdate.h"
#include "compat.h"
#include "mythcorecontext.h"

#if HAVE_POSIX_FADVISE < 1
static int posix_fadvise(int, off_t, off_t, int) { return 0; }
#define POSIX_FADV_SEQUENTIAL 0
#define POSIX_FADV_WILLNEED 0
#define POSIX_FADV_DONTNEED 0
#endif

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define LOC      QString("FileRingBuf(%1): ").arg(filename)

FileRingBuffer::FileRingBuffer(const QString &lfilename,
                               bool write, bool readahead, int timeout_ms)
  : RingBuffer(kRingBuffer_File)
{
    startreadahead = readahead;
    safefilename = lfilename;
    filename = lfilename;

    if (write)
    {
        if (filename.startsWith("myth://"))
        {
            remotefile = new RemoteFile(filename, true);
            if (!remotefile->isOpen())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("RingBuffer::RingBuffer(): Failed to open "
                            "remote file (%1) for write").arg(filename));
                delete remotefile;
                remotefile = NULL;
            }
            else
                writemode = true;
        }
        else
        {
            tfw = new ThreadedFileWriter(
                filename, O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);

            if (!tfw->Open())
            {
                delete tfw;
                tfw = NULL;
            }
            else
                writemode = true;
        }
    }
    else if (timeout_ms >= 0)
    {
        OpenFile(filename, timeout_ms);
    }
}

FileRingBuffer::~FileRingBuffer()
{
    KillReadAheadThread();

    delete remotefile;
    remotefile = NULL;

    delete tfw;
    tfw = NULL;

    if (fd2 >= 0)
    {
        close(fd2);
        fd2 = -1;
    }
}

/** \fn check_permissions(const QString&)
 *  \brief Returns false iff file exists and has incorrect permissions.
 *  \param filename File (including path) that we want to know about
 */
static bool check_permissions(const QString &filename)
{
    QFileInfo fileInfo(filename);
    if (fileInfo.exists() && !fileInfo.isReadable())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "File exists but is not readable by MythTV!");
        return false;
    }
    return true;
}

static bool is_subtitle_possible(const QString &extension)
{
    QMutexLocker locker(&RingBuffer::subExtLock);
    bool no_subtitle = false;
    for (uint i = 0; i < (uint)RingBuffer::subExtNoCheck.size(); i++)
    {
        if (extension.contains(RingBuffer::subExtNoCheck[i].right(3)))
        {
            no_subtitle = true;
            break;
        }
    }
    return !no_subtitle;
}

static QString local_sub_filename(QFileInfo &fileInfo)
{
    // Subtitle handling
    QString vidFileName = fileInfo.fileName();
    QString dirName = fileInfo.absolutePath();

    QString baseName = vidFileName;
    int suffixPos = vidFileName.lastIndexOf(QChar('.'));
    if (suffixPos > 0)
        baseName = vidFileName.left(suffixPos);

    QStringList el;
    {
        // The dir listing does not work if the filename has the
        // following chars "[]()" so we convert them to the wildcard '?'
        const QString findBaseName = baseName
            .replace("[", "?")
            .replace("]", "?")
            .replace("(", "?")
            .replace(")", "?");

        QMutexLocker locker(&RingBuffer::subExtLock);
        QStringList::const_iterator eit = RingBuffer::subExt.begin();
        for (; eit != RingBuffer::subExt.end(); ++eit)
            el += findBaseName + *eit;
    }

    // Some Qt versions do not accept paths in the search string of
    // entryList() so we have to set the dir first
    QDir dir;
    dir.setPath(dirName);

    const QStringList candidates = dir.entryList(el);

    QStringList::const_iterator cit = candidates.begin();
    for (; cit != candidates.end(); ++cit)
    {
        QFileInfo fi(dirName + "/" + *cit);
        if (fi.exists() && (fi.size() >= kReadTestSize))
            return fi.absoluteFilePath();
    }

    return QString::null;
}

bool FileRingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("OpenFile(%1, %2 ms)")
            .arg(lfilename).arg(retry_ms));

    rwlock.lockForWrite();

    filename = lfilename;
    safefilename = lfilename;
    subtitlefilename.clear();

    if (remotefile)
    {
        delete remotefile;
        remotefile = NULL;
    }

    if (fd2 >= 0)
    {
        close(fd2);
        fd2 = -1;
    }

    bool is_local = 
        (!filename.startsWith("/dev")) &&
        ((filename.startsWith("/")) || QFile::exists(filename));

    if (is_local)
    {
        char buf[kReadTestSize];
        int lasterror = 0;

        MythTimer openTimer;
        openTimer.start();

        uint openAttempts = 0;
        do
        {
            openAttempts++;
            lasterror = 0;

            fd2 = open(filename.toLocal8Bit().constData(),
                       O_RDONLY|O_LARGEFILE|O_STREAMING|O_BINARY);

            if (fd2 < 0)
            {
                if (!check_permissions(filename))
                {
                    lasterror = 3;
                    break;
                }

                lasterror = 1;
                usleep(10 * 1000);
            }
            else
            {
                int ret = read(fd2, buf, kReadTestSize);
                if (ret != (int)kReadTestSize)
                {
                    lasterror = 2;
                    close(fd2);
                    fd2 = -1;
                    if (oldfile)
                        break; // if it's an old file it won't grow..
                    usleep(10 * 1000);
                }
                else
                {
                    if (0 == lseek(fd2, 0, SEEK_SET))
                    {
#ifndef _MSC_VER
                        if (posix_fadvise(fd2, 0, 0,
                                          POSIX_FADV_SEQUENTIAL) < 0)
                        {
                            LOG(VB_FILE, LOG_DEBUG, LOC +
                                QString("OpenFile(): fadvise sequential "
                                        "failed: ") + ENO);
                        }
                        if (posix_fadvise(fd2, 0, 128*1024,
                                          POSIX_FADV_WILLNEED) < 0)
                        {
                            LOG(VB_FILE, LOG_DEBUG, LOC +
                                QString("OpenFile(): fadvise willneed "
                                        "failed: ") + ENO);
                        }
#endif
                        lasterror = 0;
                        break;
                    }
                    lasterror = 4;
                    close(fd2);
                    fd2 = -1;
                }
            }
        }
        while ((uint)openTimer.elapsed() < retry_ms);

        switch (lasterror)
        {
            case 0:
            {
                QFileInfo fi(filename);
                oldfile = QDateTime(fi.lastModified().toUTC())
                    .secsTo(MythDate::current()) > 60;
                QString extension = fi.completeSuffix().toLower();
                if (is_subtitle_possible(extension))
                    subtitlefilename = local_sub_filename(fi);
                break;
            }
            case 1:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("OpenFile(): Could not open."));

                //: %1 is the filename
                lastError = tr("Could not open %1").arg(filename);
                break;
            case 2:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("OpenFile(): File too small (%1B).")
                        .arg(QFileInfo(filename).size()));

                //: %1 is the file size
                lastError = tr("File too small (%1B)")
                            .arg(QFileInfo(filename).size());
                break;
            case 3:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        "OpenFile(): Improper permissions.");

                lastError = tr("Improper permissions");
                break;
            case 4:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        "OpenFile(): Cannot seek in file.");

                lastError = tr("Cannot seek in file");
                break;
            default:
                break;
        }
        LOG(VB_FILE, LOG_INFO, 
            LOC + QString("OpenFile() made %1 attempts in %2 ms")
                .arg(openAttempts).arg(openTimer.elapsed()));

    }
    else
    {
        QString tmpSubName = filename;
        QString dirName  = ".";

        int dirPos = filename.lastIndexOf(QChar('/'));
        if (dirPos > 0)
        {
            tmpSubName = filename.mid(dirPos + 1);
            dirName = filename.left(dirPos);
        }

        QString baseName  = tmpSubName;
        QString extension = tmpSubName;
        QStringList auxFiles;

        int suffixPos = tmpSubName.lastIndexOf(QChar('.'));
        if (suffixPos > 0)
        {
            baseName = tmpSubName.left(suffixPos);
            extension = tmpSubName.right(suffixPos-1);
            if (is_subtitle_possible(extension))
            {
                QMutexLocker locker(&subExtLock);
                QStringList::const_iterator eit = subExt.begin();
                for (; eit != subExt.end(); ++eit)
                    auxFiles += baseName + *eit;
            }
        }

        remotefile = new RemoteFile(filename, false, true,
                                    retry_ms, &auxFiles);
        if (!remotefile->isOpen())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(filename));

            //: %1 is the filename
            lastError = tr("Failed to open remote file %1").arg(filename);
            delete remotefile;
            remotefile = NULL;
        }
        else
        {
            QStringList aux = remotefile->GetAuxiliaryFiles();
            if (aux.size())
                subtitlefilename = dirName + "/" + aux[0];
        }
    }

    setswitchtonext = false;
    ateof = false;
    commserror = false;
    numfailures = 0;

    rawbitrate = 800;
    CalcReadAheadThresh();

    bool ok = fd2 >= 0 || remotefile;

    rwlock.unlock();

    return ok;
}

bool FileRingBuffer::ReOpen(QString newFilename)
{
    if (!writemode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tried to ReOpen a read only file.");
        return false;
    }

    bool result = false;

    rwlock.lockForWrite();

    if (tfw && tfw->ReOpen(newFilename))
        result = true;
    else if (remotefile && remotefile->ReOpen(newFilename))
        result = true;

    if (result)
    {
        filename = newFilename;
        poslock.lockForWrite();
        writepos = 0;
        poslock.unlock();
    }

    rwlock.unlock();
    return result;
}

bool FileRingBuffer::IsOpen(void) const
{
    rwlock.lockForRead();
    bool ret = tfw || (fd2 > -1) || remotefile;
    rwlock.unlock();
    return ret;
}

/** \fn FileRingBuffer::safe_read(int, void*, uint)
 *  \brief Reads data from the file-descriptor.
 *
 *   This will re-read the file forever until the
 *   end-of-file is reached or the buffer is full.
 *
 *  \param fd   File descriptor to read from
 *  \param data Pointer to where data will be written
 *  \param sz   Number of bytes to read
 *  \return Returns number of bytes read
 */
int FileRingBuffer::safe_read(int fd, void *data, uint sz)
{
    int ret;
    unsigned tot = 0;
    unsigned errcnt = 0;
    unsigned zerocnt = 0;

    if (fd2 < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Invalid file descriptor in 'safe_read()'");
        return 0;
    }

    if (stopreads)
        return 0;

    struct stat sb;

    while (tot < sz)
    {
        uint toread     = sz - tot;
        bool read_ok    = true;
        bool eof        = false;

        // check that we have some data to read,
        // so we never attempt to read past the end of file
        // if fstat errored or isn't a regular file, default to previous behavior
        ret = fstat(fd2, &sb);
        if (ret == 0 && S_ISREG(sb.st_mode))
        {
            if ((internalreadpos + tot) >= sb.st_size)
            {
                // We're at the end, don't attempt to read
                read_ok = false;
                eof     = true;
                LOG(VB_FILE, LOG_DEBUG, LOC + "not reading, reached EOF");
            }
            else
            {
                toread  =
                    min(sb.st_size - (internalreadpos + tot), (long long)toread);
                if (toread < (sz-tot))
                {
                    eof = true;
                    LOG(VB_FILE, LOG_DEBUG,
                        LOC + QString("About to reach EOF, reading %1 wanted %2")
                        .arg(toread).arg(sz-tot));
                }
            }
        }

        if (read_ok)
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("read(%1) -- begin").arg(toread));
            ret = read(fd2, (char *)data + tot, toread);
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("read(%1) -> %2 end").arg(toread).arg(ret));
        }
        if (ret < 0)
        {
            if (errno == EAGAIN)
                continue;

            LOG(VB_GENERAL, LOG_ERR,
                LOC + "File I/O problem in 'safe_read()'" + ENO);

            errcnt++;
            numfailures++;
            if (errcnt == 3)
                break;
        }
        else if (ret > 0)
        {
            tot += ret;
        }

        if (oldfile)
            break;

        if (eof)
        {
            // we can exit now, if file is still open for writing in another
            // instance, RingBuffer will retry
            break;
        }

        if (ret == 0)
        {
            if (tot > 0)
                break;

            zerocnt++;

            // 0.36 second timeout for livetvchain with usleep(60000),
            // or 2.4 seconds if it's a new file less than 30 minutes old.
            if (zerocnt >= (livetvchain ? 6 : 40))
            {
                break;
            }
        }
        if (stopreads)
            break;
        if (tot < sz)
            usleep(60000);
    }
    return tot;
}

/** \fn FileRingBuffer::safe_read(RemoteFile*, void*, uint)
 *  \brief Reads data from the RemoteFile.
 *
 *  \param rf   RemoteFile to read from
 *  \param data Pointer to where data will be written
 *  \param sz   Number of bytes to read
 *  \return Returns number of bytes read
 */
int FileRingBuffer::safe_read(RemoteFile *rf, void *data, uint sz)
{
    int ret = rf->Read(data, sz);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "safe_read(RemoteFile* ...): read failed");
            
        poslock.lockForRead();
        rf->Seek(internalreadpos - readAdjust, SEEK_SET);
        poslock.unlock();
        numfailures++;
    }
    else if (ret == 0)
    {
        LOG(VB_FILE, LOG_INFO, LOC +
            "safe_read(RemoteFile* ...): at EOF");
    }

    return ret;
}

long long FileRingBuffer::GetReadPosition(void) const
{
    poslock.lockForRead();
    long long ret = readpos;
    poslock.unlock();
    return ret;
}

long long FileRingBuffer::GetRealFileSize(void) const
{
    rwlock.lockForRead();
    long long ret = -1;
    if (remotefile)
    {
        ret = remotefile->GetRealFileSize();
    }
    else
    {
        if (fd2 >= 0)
        {
            struct stat sb;

            ret = fstat(fd2, &sb);
            if (ret == 0 && S_ISREG(sb.st_mode))
            {
                rwlock.unlock();
                return sb.st_size;
            }
        }
        ret = QFileInfo(filename).size();
    }
    rwlock.unlock();
    return ret;
}

long long FileRingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("Seek(%1,%2,%3)")
            .arg(pos).arg((SEEK_SET==whence)?"SEEK_SET":
                          ((SEEK_CUR==whence)?"SEEK_CUR":"SEEK_END"))
            .arg(has_lock?"locked":"unlocked"));

    long long ret = -1;

    StopReads();

    // lockForWrite takes priority over lockForRead, so this will
    // take priority over the lockForRead in the read ahead thread.
    if (!has_lock)
        rwlock.lockForWrite();

    StartReads();

    if (writemode)
    {
        ret = WriterSeek(pos, whence, true);
        if (!has_lock)
            rwlock.unlock();
        return ret;
    }

    poslock.lockForWrite();

    // Optimize no-op seeks
    if (readaheadrunning &&
        ((whence == SEEK_SET && pos == readpos) ||
         (whence == SEEK_CUR && pos == 0)))
    {
        ret = readpos;

        poslock.unlock();
        if (!has_lock)
            rwlock.unlock();

        return ret;
    }

    // only valid for SEEK_SET & SEEK_CUR
    long long new_pos = (SEEK_SET==whence) ? pos : readpos + pos;

#if 1
    // Optimize short seeks where the data for
    // them is in our ringbuffer already.
    if (readaheadrunning &&
        (SEEK_SET==whence || SEEK_CUR==whence))
    {
        rbrlock.lockForWrite();
        rbwlock.lockForRead();
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("Seek(): rbrpos: %1 rbwpos: %2"
                    "\n\t\t\treadpos: %3 internalreadpos: %4")
                .arg(rbrpos).arg(rbwpos)
                .arg(readpos).arg(internalreadpos));
        bool used_opt = false;
        if ((new_pos < readpos))
        {
            int min_safety = max(fill_min, readblocksize);
            int free = ((rbwpos >= rbrpos) ?
                        rbrpos + bufferSize : rbrpos) - rbwpos;
            int internal_backbuf =
                (rbwpos >= rbrpos) ? rbrpos : rbrpos - rbwpos;
            internal_backbuf = min(internal_backbuf, free - min_safety);
            long long sba = readpos - new_pos;
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): internal_backbuf: %1 sba: %2")
                    .arg(internal_backbuf).arg(sba));
            if (internal_backbuf >= sba)
            {
                rbrpos = (rbrpos>=sba) ? rbrpos - sba :
                    bufferSize + rbrpos - sba;
                used_opt = true;
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("Seek(): OPT1 rbrpos: %1 rbwpos: %2"
                                "\n\t\t\treadpos: %3 internalreadpos: %4")
                        .arg(rbrpos).arg(rbwpos)
                        .arg(new_pos).arg(internalreadpos));
            }
        }
        else if ((new_pos >= readpos) && (new_pos <= internalreadpos))
        {
            rbrpos = (rbrpos + (new_pos - readpos)) % bufferSize;
            used_opt = true;
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): OPT2 rbrpos: %1 sba: %2")
                    .arg(rbrpos).arg(readpos - new_pos));
        }
        rbwlock.unlock();
        rbrlock.unlock();

        if (used_opt)
        {
            if (ignorereadpos >= 0)
            {
                // seek should always succeed since we were at this position
                int ret;
                if (remotefile)
                    ret = remotefile->Seek(internalreadpos, SEEK_SET);
                else
                {
                    ret = lseek64(fd2, internalreadpos, SEEK_SET);
#ifndef _MSC_VER
                    if (posix_fadvise(fd2, internalreadpos,
                                  128*1024, POSIX_FADV_WILLNEED) < 0)
                    {
                        LOG(VB_FILE, LOG_DEBUG, LOC +
                            QString("Seek(): fadvise willneed "
                            "failed: ") + ENO);
                    }
#endif
                }
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("Seek to %1 from ignore pos %2 returned %3")
                        .arg(internalreadpos).arg(ignorereadpos).arg(ret));
                ignorereadpos = -1;
            }
            // if we are seeking forward we may now be too close to the
            // end, so we need to recheck if reads are allowed.
            if (new_pos > readpos)
            {
                ateof = false;
                readsallowed = false;
            }
            readpos = new_pos;
            poslock.unlock();
            generalWait.wakeAll();
            if (!has_lock)
                rwlock.unlock();
            return new_pos;
        }
    }
#endif

#if 1
    // This optimizes the seek end-250000, read, seek 0, read portion 
    // of the pattern ffmpeg performs at the start of playback to
    // determine the pts.
    // If the seek is a SEEK_END or is a seek where the position
    // changes over 100 MB we check the file size and if the
    // destination point is within 300000 bytes of the end of
    // the file we enter a special mode where the read ahead
    // buffer stops reading data and all reads are made directly
    // until another seek is performed. The point of all this is
    // to avoid flushing out the buffer that still contains all
    // the data the final seek 0, read will need just to read the
    // last 250000 bytes. A further optimization would be to buffer
    // the 250000 byte read, which is currently performed in 32KB
    // blocks (inefficient with RemoteFile).
    if ((remotefile || fd2 >= 0) && (ignorereadpos < 0))
    {
        long long off_end = 0xDEADBEEF;
        if (SEEK_END == whence)
        {
            off_end = pos;
            if (remotefile)
            {
                new_pos = remotefile->GetFileSize() - off_end;
            }
            else
            {
                QFileInfo fi(filename);
                new_pos = fi.size() - off_end;
            }
        }
        else
        {
            if (remotefile)
            {
                off_end = remotefile->GetFileSize() - new_pos;
            }
            else
            {
                QFileInfo fi(filename);
                off_end = fi.size() - new_pos;
            }
        }

        if (off_end != 0xDEADBEEF)
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): Offset from end: %1").arg(off_end));
        }

        if (off_end == 250000)
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): offset from end: %1").arg(off_end) +
                "\n\t\t\t -- ignoring read ahead thread until next seek.");

            ignorereadpos = new_pos;
            errno = EINVAL;
            long long ret;
            if (remotefile)
                ret = remotefile->Seek(ignorereadpos, SEEK_SET);
            else
                ret = lseek64(fd2, ignorereadpos, SEEK_SET);

            if (ret < 0)
            {
                int tmp_eno = errno;
                QString cmd = QString("Seek(%1, SEEK_SET) ign ")
                    .arg(ignorereadpos);

                ignorereadpos = -1;

                LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);

                // try to return to former position..
                if (remotefile)
                    ret = remotefile->Seek(internalreadpos, SEEK_SET);
                else
                    ret = lseek64(fd2, internalreadpos, SEEK_SET);
                if (ret < 0)
                {
                    QString cmd = QString("Seek(%1, SEEK_SET) int ")
                        .arg(internalreadpos);
                    LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);
                }
                else
                {
                    QString cmd = QString("Seek(%1, %2) int ")
                        .arg(internalreadpos)
                        .arg((SEEK_SET == whence) ? "SEEK_SET" :
                             ((SEEK_CUR == whence) ?"SEEK_CUR" : "SEEK_END"));
                    LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " succeeded");
                }
                ret = -1;
                errno = tmp_eno;
            }
            else
            {
                ateof = false;
                readsallowed = false;
            }

            poslock.unlock();

            generalWait.wakeAll();

            if (!has_lock)
                rwlock.unlock();

            return ret;
        }
    }
#endif

    // Here we perform a normal seek. When successful we
    // need to call ResetReadAhead(). A reset means we will
    // need to refill the buffer, which takes some time.
    if (remotefile)
    {
        ret = remotefile->Seek(pos, whence, readpos);
        if (ret<0)
            errno = EINVAL;
    }
    else
    {
        ret = lseek64(fd2, pos, whence);
    }

    if (ret >= 0)
    {
        readpos = ret;
        
        ignorereadpos = -1;

        if (readaheadrunning)
            ResetReadAhead(readpos);

        readAdjust = 0;
    }
    else
    {
        QString cmd = QString("Seek(%1, %2)").arg(pos)
            .arg((whence == SEEK_SET) ? "SEEK_SET" :
                 ((whence == SEEK_CUR) ? "SEEK_CUR" : "SEEK_END"));
        LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);
    }

    poslock.unlock();

    generalWait.wakeAll();

    if (!has_lock)
        rwlock.unlock();

    return ret;
}
