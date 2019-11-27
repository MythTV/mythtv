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

#define LOC      QString("FileRingBuf(%1): ").arg(m_filename)

FileRingBuffer::FileRingBuffer(const QString &lfilename,
                               bool write, bool readahead, int timeout_ms)
  : RingBuffer(kRingBuffer_File)
{
    m_startReadAhead = readahead;
    m_safeFilename = lfilename;
    m_filename = lfilename;

    if (write)
    {
        if (m_filename.startsWith("myth://"))
        {
            m_remotefile = new RemoteFile(m_filename, true);
            if (!m_remotefile->isOpen())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("RingBuffer::RingBuffer(): Failed to open "
                            "remote file (%1) for write").arg(m_filename));
                delete m_remotefile;
                m_remotefile = nullptr;
            }
            else
                m_writeMode = true;
        }
        else
        {
            m_tfw = new ThreadedFileWriter(
                m_filename, O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);

            if (!m_tfw->Open())
            {
                delete m_tfw;
                m_tfw = nullptr;
            }
            else
                m_writeMode = true;
        }
    }
    else if (timeout_ms >= 0)
    {
        FileRingBuffer::OpenFile(m_filename, timeout_ms);
    }
}

FileRingBuffer::~FileRingBuffer()
{
    KillReadAheadThread();

    delete m_remotefile;
    m_remotefile = nullptr;

    delete m_tfw;
    m_tfw = nullptr;

    if (m_fd2 >= 0)
    {
        close(m_fd2);
        m_fd2 = -1;
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
        LOG(VB_GENERAL, LOG_ERR, QString("FileRingBuf(%1): ").arg(filename) +
                "File exists but is not readable by MythTV!");
        return false;
    }
    return true;
}

static bool is_subtitle_possible(const QString &extension)
{
    QMutexLocker locker(&RingBuffer::s_subExtLock);
    bool no_subtitle = false;
    for (uint i = 0; i < (uint)RingBuffer::s_subExtNoCheck.size(); i++)
    {
        if (extension.contains(RingBuffer::s_subExtNoCheck[i].right(3)))
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

        QMutexLocker locker(&RingBuffer::s_subExtLock);
        QStringList::const_iterator eit = RingBuffer::s_subExt.begin();
        for (; eit != RingBuffer::s_subExt.end(); ++eit)
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

    return QString();
}

bool FileRingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("OpenFile(%1, %2 ms)")
            .arg(lfilename).arg(retry_ms));

    m_rwLock.lockForWrite();

    m_filename = lfilename;
    m_safeFilename = lfilename;
    m_subtitleFilename.clear();

    if (m_remotefile)
    {
        delete m_remotefile;
        m_remotefile = nullptr;
    }

    if (m_fd2 >= 0)
    {
        close(m_fd2);
        m_fd2 = -1;
    }

    bool is_local =
        (!m_filename.startsWith("/dev")) &&
        ((m_filename.startsWith("/")) || QFile::exists(m_filename));

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

            m_fd2 = open(m_filename.toLocal8Bit().constData(),
                       O_RDONLY|O_LARGEFILE|O_STREAMING|O_BINARY);

            if (m_fd2 < 0)
            {
                if (!check_permissions(m_filename))
                {
                    lasterror = 3;
                    break;
                }

                lasterror = 1;
                usleep(10 * 1000);
            }
            else
            {
                int ret = read(m_fd2, buf, kReadTestSize);
                if (ret != kReadTestSize)
                {
                    lasterror = 2;
                    close(m_fd2);
                    m_fd2 = -1;
                    if (ret == 0 && openAttempts > 5 &&
                        !gCoreContext->IsRegisteredFileForWrite(m_filename))
                    {
                        // file won't grow, abort early
                        break;
                    }

                    if (m_oldfile)
                        break; // if it's an old file it won't grow..
                    usleep(10 * 1000);
                }
                else
                {
                    if (0 == lseek(m_fd2, 0, SEEK_SET))
                    {
#ifndef _MSC_VER
                        if (posix_fadvise(m_fd2, 0, 0,
                                          POSIX_FADV_SEQUENTIAL) != 0)
                        {
                            LOG(VB_FILE, LOG_DEBUG, LOC +
                                QString("OpenFile(): fadvise sequential "
                                        "failed: ") + ENO);
                        }
                        if (posix_fadvise(m_fd2, 0, 128*1024,
                                          POSIX_FADV_WILLNEED) != 0)
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
                    close(m_fd2);
                    m_fd2 = -1;
                }
            }
        }
        while ((uint)openTimer.elapsed() < retry_ms);

        switch (lasterror)
        {
            case 0:
            {
                QFileInfo fi(m_filename);
                m_oldfile = fi.lastModified().toUTC()
                    .secsTo(MythDate::current()) > 60;
                QString extension = fi.completeSuffix().toLower();
                if (is_subtitle_possible(extension))
                    m_subtitleFilename = local_sub_filename(fi);
                break;
            }
            case 1:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("OpenFile(): Could not open."));

                //: %1 is the filename
                m_lastError = tr("Could not open %1").arg(m_filename);
                break;
            case 2:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("OpenFile(): File too small (%1B).")
                        .arg(QFileInfo(m_filename).size()));

                //: %1 is the file size
                m_lastError = tr("File too small (%1B)")
                              .arg(QFileInfo(m_filename).size());
                break;
            case 3:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        "OpenFile(): Improper permissions.");

                m_lastError = tr("Improper permissions");
                break;
            case 4:
                LOG(VB_GENERAL, LOG_ERR, LOC +
                        "OpenFile(): Cannot seek in file.");

                m_lastError = tr("Cannot seek in file");
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
        QString tmpSubName = m_filename;
        QString dirName  = ".";

        int dirPos = m_filename.lastIndexOf(QChar('/'));
        if (dirPos > 0)
        {
            tmpSubName = m_filename.mid(dirPos + 1);
            dirName = m_filename.left(dirPos);
        }

        QStringList auxFiles;

        int suffixPos = tmpSubName.lastIndexOf(QChar('.'));
        if (suffixPos > 0)
        {
            QString baseName = tmpSubName.left(suffixPos);
            int extnleng = tmpSubName.size() - baseName.size() - 1;
            QString extension = tmpSubName.right(extnleng);

            if (is_subtitle_possible(extension))
            {
                QMutexLocker locker(&s_subExtLock);
                QStringList::const_iterator eit = s_subExt.begin();
                for (; eit != s_subExt.end(); ++eit)
                    auxFiles += baseName + *eit;
            }
        }

        m_remotefile = new RemoteFile(m_filename, false, true,
                                      retry_ms, &auxFiles);
        if (!m_remotefile->isOpen())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(m_filename));

            //: %1 is the filename
            m_lastError = tr("Failed to open remote file %1").arg(m_filename);
            delete m_remotefile;
            m_remotefile = nullptr;
        }
        else
        {
            QStringList aux = m_remotefile->GetAuxiliaryFiles();
            if (!aux.empty())
                m_subtitleFilename = dirName + "/" + aux[0];
        }
    }

    m_setSwitchToNext = false;
    m_ateof = false;
    m_commsError = false;
    m_numFailures = 0;

    CalcReadAheadThresh();

    bool ok = m_fd2 >= 0 || m_remotefile;

    m_rwLock.unlock();

    return ok;
}

bool FileRingBuffer::ReOpen(QString newFilename)
{
    if (!m_writeMode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tried to ReOpen a read only file.");
        return false;
    }

    bool result = false;

    m_rwLock.lockForWrite();

    if ((m_tfw && m_tfw->ReOpen(newFilename)) ||
        (m_remotefile && m_remotefile->ReOpen(newFilename)))
        result = true;

    if (result)
    {
        m_filename = newFilename;
        m_posLock.lockForWrite();
        m_writePos = 0;
        m_posLock.unlock();
    }

    m_rwLock.unlock();
    return result;
}

bool FileRingBuffer::IsOpen(void) const
{
    m_rwLock.lockForRead();
    bool ret = m_tfw || (m_fd2 > -1) || m_remotefile;
    m_rwLock.unlock();
    return ret;
}

/** \fn FileRingBuffer::safe_read(int, void*, uint)
 *  \brief Reads data from the file-descriptor.
 *
 *   This will re-read the file forever until the
 *   end-of-file is reached or the buffer is full.
 *
 *  \param fd Ignored. The File descriptor to read is now stored
 *            as part of the RingBuffer parent structure.
 *  \param data Pointer to where data will be written
 *  \param sz   Number of bytes to read
 *  \return Returns number of bytes read
 */
int FileRingBuffer::safe_read(int /*fd*/, void *data, uint sz)
{
    unsigned tot = 0;
    unsigned errcnt = 0;
    unsigned zerocnt = 0;

    if (m_fd2 < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Invalid file descriptor in 'safe_read()'");
        return 0;
    }

    if (m_stopReads)
        return 0;

    struct stat sb {};

    while (tot < sz)
    {
        uint toread     = sz - tot;
        bool read_ok    = true;
        bool eof        = false;

        // check that we have some data to read,
        // so we never attempt to read past the end of file
        // if fstat errored or isn't a regular file, default to previous behavior
        int ret = fstat(m_fd2, &sb);
        if (ret == 0 && S_ISREG(sb.st_mode))
        {
            if ((m_internalReadPos + tot) >= sb.st_size)
            {
                // We're at the end, don't attempt to read
                read_ok = false;
                eof     = true;
                LOG(VB_FILE, LOG_DEBUG, LOC + "not reading, reached EOF");
            }
            else
            {
                toread  =
                    min(sb.st_size - (m_internalReadPos + tot), (long long)toread);
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
            ret = read(m_fd2, (char *)data + tot, toread);
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
            m_numFailures++;
            if (errcnt == 3)
                break;
        }
        else if (ret > 0)
        {
            tot += ret;
        }

        if (m_oldfile)
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
            if (zerocnt >= (m_liveTVChain ? 6 : 40))
            {
                break;
            }
        }
        if (m_stopReads)
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

        m_posLock.lockForRead();
        rf->Seek(m_internalReadPos - m_readAdjust, SEEK_SET);
        m_posLock.unlock();
        m_numFailures++;
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
    m_posLock.lockForRead();
    long long ret = m_readPos;
    m_posLock.unlock();
    return ret;
}

long long FileRingBuffer::GetRealFileSizeInternal(void) const
{
    m_rwLock.lockForRead();
    long long ret = -1;
    if (m_remotefile)
    {
        ret = m_remotefile->GetRealFileSize();
    }
    else
    {
        if (m_fd2 >= 0)
        {
            struct stat sb {};

            ret = fstat(m_fd2, &sb);
            if (ret == 0 && S_ISREG(sb.st_mode))
            {
                m_rwLock.unlock();
                return sb.st_size;
            }
        }
        ret = QFileInfo(m_filename).size();
    }
    m_rwLock.unlock();
    return ret;
}

long long FileRingBuffer::SeekInternal(long long pos, int whence)
{
    long long ret = -1;

    // Ticket 12128
    StopReads();
    StartReads();

    if (m_writeMode)
    {
        ret = WriterSeek(pos, whence, true);

        return ret;
    }

    m_posLock.lockForWrite();

    // Optimize no-op seeks
    if (m_readAheadRunning &&
        ((whence == SEEK_SET && pos == m_readPos) ||
         (whence == SEEK_CUR && pos == 0)))
    {
        ret = m_readPos;

        m_posLock.unlock();

        return ret;
    }

    // only valid for SEEK_SET & SEEK_CUR
    long long new_pos = (SEEK_SET==whence) ? pos : m_readPos + pos;

    // Optimize short seeks where the data for
    // them is in our ringbuffer already.
    if (m_readAheadRunning &&
        (SEEK_SET==whence || SEEK_CUR==whence))
    {
        m_rbrLock.lockForWrite();
        m_rbwLock.lockForRead();
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("Seek(): rbrpos: %1 rbwpos: %2"
                    "\n\t\t\treadpos: %3 internalreadpos: %4")
                .arg(m_rbrPos).arg(m_rbwPos)
                .arg(m_readPos).arg(m_internalReadPos));
        bool used_opt = false;
        if ((new_pos < m_readPos))
        {
            // Seeking to earlier than current buffer's start, but still in buffer
            int min_safety = max(m_fillMin, m_readBlockSize);
            int free = ((m_rbwPos >= m_rbrPos) ?
                        m_rbrPos + m_bufferSize : m_rbrPos) - m_rbwPos;
            int internal_backbuf =
                (m_rbwPos >= m_rbrPos) ? m_rbrPos : m_rbrPos - m_rbwPos;
            internal_backbuf = min(internal_backbuf, free - min_safety);
            long long sba = m_readPos - new_pos;
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): internal_backbuf: %1 sba: %2")
                    .arg(internal_backbuf).arg(sba));
            if (internal_backbuf >= sba)
            {
                m_rbrPos = (m_rbrPos>=sba) ? m_rbrPos - sba :
                    m_bufferSize + m_rbrPos - sba;
                used_opt = true;
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("Seek(): OPT1 rbrPos: %1 rbwPos: %2"
                                "\n\t\t\treadpos: %3 internalreadpos: %4")
                        .arg(m_rbrPos).arg(m_rbwPos)
                        .arg(new_pos).arg(m_internalReadPos));
            }
        }
        else if ((new_pos >= m_readPos) && (new_pos <= m_internalReadPos))
        {
            m_rbrPos = (m_rbrPos + (new_pos - m_readPos)) % m_bufferSize;
            used_opt = true;
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): OPT2 rbrPos: %1 sba: %2")
                    .arg(m_rbrPos).arg(m_readPos - new_pos));
        }
        m_rbwLock.unlock();
        m_rbrLock.unlock();

        if (used_opt)
        {
            if (m_ignoreReadPos >= 0)
            {
                // seek should always succeed since we were at this position
                if (m_remotefile)
                    ret = m_remotefile->Seek(m_internalReadPos, SEEK_SET);
                else
                {
                    ret = lseek64(m_fd2, m_internalReadPos, SEEK_SET);
#ifndef _MSC_VER
                    if (posix_fadvise(m_fd2, m_internalReadPos,
                                  128*1024, POSIX_FADV_WILLNEED) != 0)
                    {
                        LOG(VB_FILE, LOG_DEBUG, LOC +
                            QString("Seek(): fadvise willneed "
                            "failed: ") + ENO);
                    }
#endif
                }
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("Seek to %1 from ignore pos %2 returned %3")
                        .arg(m_internalReadPos).arg(m_ignoreReadPos).arg(ret));
                m_ignoreReadPos = -1;
            }
            // if we are seeking forward we may now be too close to the
            // end, so we need to recheck if reads are allowed.
            if (new_pos > m_readPos)
            {
                m_ateof         = false;
                m_readsAllowed  = false;
                m_readsDesired  = false;
                m_recentSeek    = true;
            }
            m_readPos = new_pos;
            m_posLock.unlock();
            m_generalWait.wakeAll();

            return new_pos;
        }
    }

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
    if ((m_remotefile || m_fd2 >= 0) && (m_ignoreReadPos < 0))
    {
        long long off_end = 0xDEADBEEF;
        if (SEEK_END == whence)
        {
            off_end = pos;
            if (m_remotefile)
            {
                new_pos = m_remotefile->GetFileSize() - off_end;
            }
            else
            {
                QFileInfo fi(m_filename);
                new_pos = fi.size() - off_end;
            }
        }
        else
        {
            if (m_remotefile)
            {
                off_end = m_remotefile->GetFileSize() - new_pos;
            }
            else
            {
                QFileInfo fi(m_filename);
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

            m_ignoreReadPos = new_pos;
            errno = EINVAL;
            if (m_remotefile)
                ret = m_remotefile->Seek(m_ignoreReadPos, SEEK_SET);
            else
                ret = lseek64(m_fd2, m_ignoreReadPos, SEEK_SET);

            if (ret < 0)
            {
                int tmp_eno = errno;
                QString cmd = QString("Seek(%1, SEEK_SET) ign ")
                    .arg(m_ignoreReadPos);

                m_ignoreReadPos = -1;

                LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);

                // try to return to former position..
                if (m_remotefile)
                    ret = m_remotefile->Seek(m_internalReadPos, SEEK_SET);
                else
                    ret = lseek64(m_fd2, m_internalReadPos, SEEK_SET);
                if (ret < 0)
                {
                    QString cmd2 = QString("Seek(%1, SEEK_SET) int ")
                        .arg(m_internalReadPos);
                    LOG(VB_GENERAL, LOG_ERR, LOC + cmd2 + " Failed" + ENO);
                }
                else
                {
                    QString cmd2 = QString("Seek(%1, %2) int ")
                        .arg(m_internalReadPos)
                        .arg((SEEK_SET == whence) ? "SEEK_SET" :
                             ((SEEK_CUR == whence) ?"SEEK_CUR" : "SEEK_END"));
                    LOG(VB_GENERAL, LOG_ERR, LOC + cmd2 + " succeeded");
                }
                ret = -1;
                errno = tmp_eno;
            }
            else
            {
                m_ateof         = false;
                m_readsAllowed  = false;
                m_readsDesired  = false;
                m_recentSeek    = true;
            }

            m_posLock.unlock();

            m_generalWait.wakeAll();

            return ret;
        }
    }
#endif

    // Here we perform a normal seek. When successful we
    // need to call ResetReadAhead(). A reset means we will
    // need to refill the buffer, which takes some time.
    if (m_remotefile)
    {
        ret = m_remotefile->Seek(pos, whence, m_readPos);
        if (ret<0)
            errno = EINVAL;
    }
    else
    {
        ret = lseek64(m_fd2, pos, whence);
    }

    if (ret >= 0)
    {
        m_readPos = ret;

        m_ignoreReadPos = -1;

        if (m_readAheadRunning)
            ResetReadAhead(m_readPos);

        m_readAdjust = 0;
    }
    else
    {
        QString cmd = QString("Seek(%1, %2)").arg(pos)
            .arg((whence == SEEK_SET) ? "SEEK_SET" :
                 ((whence == SEEK_CUR) ? "SEEK_CUR" : "SEEK_END"));
        LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);
    }

    m_posLock.unlock();

    m_generalWait.wakeAll();

    return ret;
}
