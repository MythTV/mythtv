// QT
#include <QFileInfo>
#include <QDir>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/threadedfilewriter.h"

#include "io/mythfilebuffer.h"

// Std
#include <array>
#include <cstdlib>
#include <cerrno>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if HAVE_POSIX_FADVISE < 1
static int posix_fadvise(int, off_t, off_t, int) { return 0; }
static constexpr int8_t POSIX_FADV_SEQUENTIAL { 0 };
static constexpr int8_t POSIX_FADV_WILLNEED { 0 };
#endif

#ifndef O_STREAMING
static constexpr int8_t O_STREAMING { 0 };
#endif

#ifndef O_LARGEFILE
static constexpr int8_t O_LARGEFILE { 0 };
#endif

#ifndef O_BINARY
static constexpr int8_t O_BINARY { 0 };
#endif

#define LOC QString("FileRingBuf(%1): ").arg(m_filename)

static const QStringList kSubExt        {".ass", ".srt", ".ssa", ".sub", ".txt"};
static const QStringList kSubExtNoCheck {".ass", ".srt", ".ssa", ".sub", ".txt", ".gif", ".png"};


MythFileBuffer::MythFileBuffer(const QString &Filename, bool Write, bool UseReadAhead, std::chrono::milliseconds Timeout)
  : MythMediaBuffer(kMythBufferFile)
{
    m_startReadAhead = UseReadAhead;
    m_safeFilename = Filename;
    m_filename = Filename;

    if (Write)
    {
        if (m_filename.startsWith("myth://"))
        {
            m_remotefile = new RemoteFile(m_filename, true);
            if (!m_remotefile->isOpen())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open remote file (%1) for write").arg(m_filename));
                delete m_remotefile;
                m_remotefile = nullptr;
            }
            else
            {
                m_writeMode = true;
            }
        }
        else
        {
            m_tfw = new ThreadedFileWriter(m_filename, O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);
            if (!m_tfw->Open())
            {
                delete m_tfw;
                m_tfw = nullptr;
            }
            else
            {
                m_writeMode = true;
            }
        }
    }
    else if (Timeout >= 0ms)
    {
        MythFileBuffer::OpenFile(m_filename, Timeout);
    }
}

MythFileBuffer::~MythFileBuffer()
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
static bool CheckPermissions(const QString &Filename)
{
    QFileInfo fileInfo(Filename);
    if (fileInfo.exists() && !fileInfo.isReadable())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("FileRingBuf(%1): File exists but is not readable by MythTV!")
            .arg(Filename));
        return false;
    }
    return true;
}

static bool IsSubtitlePossible(const QString &Extension)
{
    auto it = std::find_if(kSubExtNoCheck.cbegin(), kSubExtNoCheck.cend(),
                           [Extension] (const QString& ext) -> bool
                               {return ext.contains(Extension);});
    return (it != nullptr);
}

static QString LocalSubtitleFilename(QFileInfo &FileInfo)
{
    // Subtitle handling
    QString vidFileName = FileInfo.fileName();
    QString dirName = FileInfo.absolutePath();

    QString baseName = vidFileName;
    int suffixPos = vidFileName.lastIndexOf(QChar('.'));
    if (suffixPos > 0)
        baseName = vidFileName.left(suffixPos);

    QStringList list;
    {
        // The dir listing does not work if the filename has the
        // following chars "[]()" so we convert them to the wildcard '?'
        const QString findBaseName = baseName.replace("[", "?")
                                             .replace("]", "?")
                                             .replace("(", "?")
                                             .replace(")", "?");

        for (const auto & ext : kSubExt)
            list += findBaseName + ext;
    }

    // Some Qt versions do not accept paths in the search string of
    // entryList() so we have to set the dir first
    QDir dir;
    dir.setPath(dirName);
    const QStringList candidates = dir.entryList(list);
    for (const auto & candidate : candidates)
    {
        QFileInfo file(dirName + "/" + candidate);
        if (file.exists() && (file.size() >= kReadTestSize))
            return file.absoluteFilePath();
    }

    return {};
}

bool MythFileBuffer::OpenFile(const QString &Filename, std::chrono::milliseconds Retry)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("OpenFile(%1, %2 ms)")
            .arg(Filename).arg(Retry.count()));

    m_rwLock.lockForWrite();

    m_filename = Filename;
    m_safeFilename = Filename;
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

    bool islocal = (!m_filename.startsWith("/dev")) &&
                   ((m_filename.startsWith("/")) || QFile::exists(m_filename));

    if (islocal)
    {
        std::array<char,kReadTestSize> buf {};
        int lasterror = 0;

        MythTimer openTimer;
        openTimer.start();

        uint openAttempts = 0;
        while ((openTimer.elapsed() < Retry) || (openAttempts == 0))
        {
            openAttempts++;

            m_fd2 = open(m_filename.toLocal8Bit().constData(),
                         // NOLINTNEXTLINE(misc-redundant-expression)
                         O_RDONLY|O_LARGEFILE|O_STREAMING|O_BINARY);

            if (m_fd2 < 0)
            {
                if (!CheckPermissions(m_filename))
                {
                    lasterror = 3;
                    break;
                }

                lasterror = 1;
                usleep(10ms);
                continue;
            }

            ssize_t ret = read(m_fd2, buf.data(), buf.size());
            if (ret != kReadTestSize)
            {
                lasterror = 2;
                close(m_fd2);
                m_fd2 = -1;
                if (ret == 0 && openAttempts > 5 && !gCoreContext->IsRegisteredFileForWrite(m_filename))
                {
                    // file won't grow, abort early
                    break;
                }

                if (m_oldfile)
                    break; // if it's an old file it won't grow..
                usleep(10ms);
                continue;
            }

            if (0 == lseek(m_fd2, 0, SEEK_SET))
            {
#ifndef _MSC_VER
                if (posix_fadvise(m_fd2, 0, 0, POSIX_FADV_SEQUENTIAL) != 0)
                {
                    LOG(VB_FILE, LOG_DEBUG, LOC +
                        QString("OpenFile(): fadvise sequential "
                                "failed: ") + ENO);
                }
                if (posix_fadvise(m_fd2, 0, static_cast<off_t>(128)*1024, POSIX_FADV_WILLNEED) != 0)
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

        switch (lasterror)
        {
            case 0:
            {
                QFileInfo file(m_filename);
                m_oldfile = MythDate::secsInPast(file.lastModified().toUTC()) > 60s;
                QString extension = file.completeSuffix().toLower();
                if (IsSubtitlePossible(extension))
                    m_subtitleFilename = LocalSubtitleFilename(file);
                break;
            }
            case 1:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("OpenFile(): Could not open."));
                //: %1 is the filename
                m_lastError = tr("Could not open %1").arg(m_filename);
                break;
            case 2:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("OpenFile(): File too small (%1B).")
                        .arg(QFileInfo(m_filename).size()));
                //: %1 is the file size
                m_lastError = tr("File too small (%1B)").arg(QFileInfo(m_filename).size());
                break;
            case 3:
                LOG(VB_GENERAL, LOG_ERR, LOC + "OpenFile(): Improper permissions.");
                m_lastError = tr("Improper permissions");
                break;
            case 4:
                LOG(VB_GENERAL, LOG_ERR, LOC + "OpenFile(): Cannot seek in file.");
                m_lastError = tr("Cannot seek in file");
                break;
            default: break;
        }
        LOG(VB_FILE, LOG_INFO, LOC + QString("OpenFile() made %1 attempts in %2 ms")
                .arg(openAttempts).arg(openTimer.elapsed().count()));
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

            if (IsSubtitlePossible(extension))
            {
                for (const auto & ext : kSubExt)
                    auxFiles += baseName + ext;
            }
        }

        m_remotefile = new RemoteFile(m_filename, false, true, Retry, &auxFiles);
        if (!m_remotefile->isOpen())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("RingBuffer::RingBuffer(): Failed to open remote file (%1)")
                .arg(m_filename));
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
    bool ok = (m_fd2 >= 0) || m_remotefile;
    m_rwLock.unlock();
    return ok;
}

bool MythFileBuffer::ReOpen(const QString& Filename)
{
    if (!m_writeMode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tried to ReOpen a read only file.");
        return false;
    }

    bool result = false;

    m_rwLock.lockForWrite();

    if ((m_tfw && m_tfw->ReOpen(Filename)) || (m_remotefile && m_remotefile->ReOpen(Filename)))
        result = true;

    if (result)
    {
        m_filename = Filename;
        m_posLock.lockForWrite();
        m_writePos = 0;
        m_posLock.unlock();
    }

    m_rwLock.unlock();
    return result;
}

bool MythFileBuffer::IsOpen(void) const
{
    m_rwLock.lockForRead();
    bool ret = m_tfw || (m_fd2 > -1) || m_remotefile;
    m_rwLock.unlock();
    return ret;
}

int MythFileBuffer::SafeRead(void *Buffer, uint Size)
{
    if (m_remotefile)
        return SafeRead(m_remotefile, Buffer, Size);
    if (m_fd2 >= 0)
        return SafeRead(m_fd2, Buffer, Size);
    errno = EBADF;
    return -1;
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
int MythFileBuffer::SafeRead(int /*fd*/, void *Buffer, uint Size)
{
    uint tot = 0;
    uint errcnt = 0;
    uint zerocnt = 0;

    if (m_fd2 < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid file descriptor in 'safe_read()'");
        return 0;
    }

    if (m_stopReads)
        return 0;

    struct stat sb {};

    while (tot < Size)
    {
        uint toread  = Size - tot;
        bool read_ok = true;
        bool eof     = false;

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
                toread  = static_cast<uint>(std::min(sb.st_size - (m_internalReadPos + tot), static_cast<long long>(toread)));
                if (toread < (Size - tot))
                {
                    eof = true;
                    LOG(VB_FILE, LOG_DEBUG, LOC + QString("About to reach EOF, reading %1 wanted %2")
                        .arg(toread).arg(Size-tot));
                }
            }
        }

        if (read_ok)
        {
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("read(%1) -- begin").arg(toread));
            ret = static_cast<int>(read(m_fd2, static_cast<char*>(Buffer) + tot, toread));
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("read(%1) -> %2 end").arg(toread).arg(ret));
        }
        if (ret < 0)
        {
            if (errno == EAGAIN)
                continue;

            LOG(VB_GENERAL, LOG_ERR, LOC + "File I/O problem in 'safe_read()'" + ENO);
            errcnt++;
            m_numFailures++;
            if (errcnt == 3)
                break;
        }
        else if (ret > 0)
        {
            tot += static_cast<uint>(ret);
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
        if (tot < Size)
            usleep(60ms);
    }
    return static_cast<int>(tot);
}

/** \fn FileRingBuffer::safe_read(RemoteFile*, void*, uint)
 *  \brief Reads data from the RemoteFile.
 *
 *  \param rf   RemoteFile to read from
 *  \param data Pointer to where data will be written
 *  \param sz   Number of bytes to read
 *  \return Returns number of bytes read
 */
int MythFileBuffer::SafeRead(RemoteFile *Remote, void *Buffer, uint Size)
{
    int ret = Remote->Read(Buffer, static_cast<int>(Size));
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "safe_read(RemoteFile* ...): read failed");
        m_posLock.lockForRead();
        if (Remote->Seek(m_internalReadPos - m_readAdjust, SEEK_SET) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "safe_read() failed to seek reset");
        m_posLock.unlock();
        m_numFailures++;
    }
    else if (ret == 0)
    {
        LOG(VB_FILE, LOG_INFO, LOC + "safe_read(RemoteFile* ...): at EOF");
    }

    return ret;
}

long long MythFileBuffer::GetReadPosition(void) const
{
    m_posLock.lockForRead();
    long long ret = m_readPos;
    m_posLock.unlock();
    return ret;
}

long long MythFileBuffer::GetRealFileSizeInternal(void) const
{
    m_rwLock.lockForRead();
    long long result = -1;
    if (m_remotefile)
    {
        result = m_remotefile->GetRealFileSize();
    }
    else
    {
        if (m_fd2 >= 0)
        {
            struct stat sb {};

            result = fstat(m_fd2, &sb);
            if (result == 0 && S_ISREG(sb.st_mode))
            {
                m_rwLock.unlock();
                return sb.st_size;
            }
        }
        result = QFileInfo(m_filename).size();
    }
    m_rwLock.unlock();
    return result;
}

long long MythFileBuffer::SeekInternal(long long Position, int Whence)
{
    long long ret = -1;

    // Ticket 12128
    StopReads();
    StartReads();

    if (m_writeMode)
        return WriterSeek(Position, Whence, true);

    m_posLock.lockForWrite();

    // Optimize no-op seeks
    if (m_readAheadRunning && ((Whence == SEEK_SET && Position == m_readPos) ||
                               (Whence == SEEK_CUR && Position == 0)))
    {
        ret = m_readPos;
        m_posLock.unlock();
        return ret;
    }

    // only valid for SEEK_SET & SEEK_CUR
    long long newposition = (SEEK_SET==Whence) ? Position : m_readPos + Position;

    // Optimize short seeks where the data for
    // them is in our ringbuffer already.
    if (m_readAheadRunning && (SEEK_SET==Whence || SEEK_CUR==Whence))
    {
        m_rbrLock.lockForWrite();
        m_rbwLock.lockForRead();
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("Seek(): rbrpos: %1 rbwpos: %2\n\t\t\treadpos: %3 internalreadpos: %4")
                .arg(m_rbrPos).arg(m_rbwPos).arg(m_readPos).arg(m_internalReadPos));
        bool used_opt = false;
        if ((newposition < m_readPos))
        {
            // Seeking to earlier than current buffer's start, but still in buffer
            int min_safety = std::max(m_fillMin, m_readBlockSize);
            int free = ((m_rbwPos >= m_rbrPos) ? m_rbrPos + static_cast<int>(m_bufferSize) : m_rbrPos) - m_rbwPos;
            int internal_backbuf = (m_rbwPos >= m_rbrPos) ? m_rbrPos : m_rbrPos - m_rbwPos;
            internal_backbuf = std::min(internal_backbuf, free - min_safety);
            long long sba = m_readPos - newposition;
            LOG(VB_FILE, LOG_INFO, LOC + QString("Seek(): internal_backbuf: %1 sba: %2")
                    .arg(internal_backbuf).arg(sba));
            if (internal_backbuf >= sba)
            {
                m_rbrPos = (m_rbrPos>=sba) ? m_rbrPos - static_cast<int>(sba) :
                    static_cast<int>(m_bufferSize) + m_rbrPos - static_cast<int>(sba);
                used_opt = true;
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("Seek(): OPT1 rbrPos: %1 rbwPos: %2"
                                "\n\t\t\treadpos: %3 internalreadpos: %4")
                        .arg(m_rbrPos).arg(m_rbwPos)
                        .arg(newposition).arg(m_internalReadPos));
            }
        }
        else if ((newposition >= m_readPos) && (newposition <= m_internalReadPos))
        {
            m_rbrPos = (m_rbrPos + (newposition - m_readPos)) % static_cast<int>(m_bufferSize);
            used_opt = true;
            LOG(VB_FILE, LOG_INFO, LOC + QString("Seek(): OPT2 rbrPos: %1 sba: %2")
                    .arg(m_rbrPos).arg(m_readPos - newposition));
        }
        m_rbwLock.unlock();
        m_rbrLock.unlock();

        if (used_opt)
        {
            if (m_ignoreReadPos >= 0)
            {
                // seek should always succeed since we were at this position
                if (m_remotefile)
                {
                    ret = m_remotefile->Seek(m_internalReadPos, SEEK_SET);
                }
                else
                {
                    ret = lseek64(m_fd2, m_internalReadPos, SEEK_SET);
#ifndef _MSC_VER
                    if (posix_fadvise(m_fd2, m_internalReadPos, static_cast<off_t>(128)*1024, POSIX_FADV_WILLNEED) != 0)
                        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Seek(): fadvise willneed failed: ") + ENO);
#endif
                }
                LOG(VB_FILE, LOG_INFO, LOC + QString("Seek to %1 from ignore pos %2 returned %3")
                    .arg(m_internalReadPos).arg(m_ignoreReadPos).arg(ret));
                m_ignoreReadPos = -1;
            }
            // if we are seeking forward we may now be too close to the
            // end, so we need to recheck if reads are allowed.
            if (newposition > m_readPos)
            {
                m_ateof         = false;
                m_readsAllowed  = false;
                m_readsDesired  = false;
                m_recentSeek    = true;
            }
            m_readPos = newposition;
            m_posLock.unlock();
            m_generalWait.wakeAll();

            return newposition;
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
        if (SEEK_END == Whence)
        {
            off_end = Position;
            if (m_remotefile)
            {
                newposition = m_remotefile->GetFileSize() - off_end;
            }
            else
            {
                QFileInfo fi(m_filename);
                newposition = fi.size() - off_end;
            }
        }
        else
        {
            if (m_remotefile)
            {
                off_end = m_remotefile->GetFileSize() - newposition;
            }
            else
            {
                QFileInfo file(m_filename);
                off_end = file.size() - newposition;
            }
        }

        if (off_end != 0xDEADBEEF)
        {
            LOG(VB_FILE, LOG_INFO, LOC + QString("Seek(): Offset from end: %1").arg(off_end));
        }

        if (off_end == 250000)
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Seek(): offset from end: %1").arg(off_end) +
                "\n\t\t\t -- ignoring read ahead thread until next seek.");

            m_ignoreReadPos = newposition;
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
                        .arg(seek2string(Whence));
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
        ret = m_remotefile->Seek(Position, Whence, m_readPos);
        if (ret < 0)
            errno = EINVAL;
    }
    else
    {
        ret = lseek64(m_fd2, Position, Whence);
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
        QString cmd = QString("Seek(%1, %2)").arg(Position)
            .arg(seek2string(Whence));
        LOG(VB_GENERAL, LOG_ERR, LOC + cmd + " Failed" + ENO);
    }

    m_posLock.unlock();
    m_generalWait.wakeAll();
    return ret;
}
