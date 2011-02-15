// ANSI C headers
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

// POSIX C headers
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

// Qt headers
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

using namespace std;

#include "mythcontext.h" // for VERBOSE
#include "mythconfig.h"
#include "exitcodes.h"
#include "RingBuffer.h"
#include "remotefile.h"
#include "ThreadedFileWriter.h"
#include "livetvchain.h"
#include "DVDRingBuffer.h"
#include "BDRingBuffer.h"
#include "util.h"
#include "compat.h"

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

// about one second at 35mbit
const uint RingBuffer::kBufferSize = 4 * 1024 * 1024;
const int  RingBuffer::kDefaultOpenTimeout = 2000; // ms
const int  RingBuffer::kLiveTVOpenTimeout  = 10000;

#define CHUNK 32768 /* readblocksize increments */

#define LOC      QString("RingBuf(%1): ").arg(filename)
#define LOC_WARN QString("RingBuf(%1) Warning: ").arg(filename)
#define LOC_ERR  QString("RingBuf(%1) Error: ").arg(filename)

#define PNG_MIN_SIZE   20 /* header plus one empty chunk */
#define NUV_MIN_SIZE  204 /* header size? */
#define MPEG_MIN_SIZE 376 /* 2 TS packets */

/* should be minimum of the above test sizes */
const uint RingBuffer::kReadTestSize = PNG_MIN_SIZE;

QMutex      RingBuffer::subExtLock;
QStringList RingBuffer::subExt;
QStringList RingBuffer::subExtNoCheck;

/*
  Locking relations:
    rwlock->poslock->rbrlock->rbwlock

  A child should never lock any of the parents without locking
  the parent lock before the child lock.
  void RingBuffer::Example1()
  {
      poslock.lockForWrite();
      rwlock.lockForRead(); // error!
      blah(); // <- does not implicitly aquire any locks
      rwlock.unlock();
      poslock.unlock();
  }
  void RingBuffer::Example2()
  {
      rwlock.lockForRead();
      rbrlock.lockForWrite(); // ok!
      blah(); // <- does not implicitly aquire any locks
      rbrlock.unlock();
      rwlock.unlock();
  }
*/

/** \class RingBuffer
 *  \brief Implements a file/stream reader/writer.
 *
 *  This class, despite its name, no-longer provides a ring buffer.
 *  It can buffer reads and provide support for streaming files.
 *  It also provides a wrapper for the ThreadedFileWriter which
 *  makes sure that the file reader does not read past where the
 *  wrapped TFW has written new data.
 *
 */

/** \brief Creates a RingBuffer instance.
 *
 *   You can explicitly disable the readahead thread by setting
 *   readahead to false, or by just not calling Start(void).
 *
 *  \param lfilename    Name of file to read or write.
 *  \param write        If true an encapsulated writer is created
 *  \param readahead    If false a call to Start(void) will not
 *                      a pre-buffering thread, otherwise Start(void)
 *                      will start a pre-buffering thread.
 *  \param timeout_ms   if < 0, then we will not open the file.
 *                      Otherwise it's how long to try opening 
 *                      the file after the first failure in
 *                      milliseconds before giving up.
 */
RingBuffer::RingBuffer(const QString &lfilename,
                       bool write, bool readahead,
                       int timeout_ms)
    : readpos(0),               writepos(0),
      internalreadpos(0),       ignorereadpos(-1),
      rbrpos(0),                rbwpos(0),
      stopreads(false),
      filename(lfilename),      subtitlefilename(QString::null),
      tfw(NULL),                fd2(-1),
      writemode(false),         remotefile(NULL),
      startreadahead(readahead),readAheadBuffer(NULL),
      readaheadrunning(false),  reallyrunning(false),
      request_pause(false),     paused(false),
      ateof(false),             readsallowed(false),
      setswitchtonext(false),   streamOnly(false),
      rawbitrate(8000),         playspeed(1.0f),
      fill_threshold(65536),    fill_min(-1),
      readblocksize(CHUNK),     wanttoread(0),
      numfailures(0),           commserror(false),
      dvdPriv(NULL),            bdPriv(NULL),
      oldfile(false),           livetvchain(NULL),
      ignoreliveeof(false),     readAdjust(0)
{
    filename.detach();

    {
        QMutexLocker locker(&subExtLock);
        if (!subExt.size())
        {
            // Possible subtitle file extensions '.srt', '.sub', '.txt'
            subExt += ".srt";
            subExt += ".sub";
            subExt += ".txt";

            // Extensions for which a subtitle file should not exist
            subExtNoCheck = subExt;
            subExtNoCheck += ".gif";
            subExtNoCheck += ".png";
        }
    }

    if (write)
    {
        if (filename.startsWith("myth://"))
        {
            remotefile = new RemoteFile(filename, true);
            if (!remotefile->isOpen())
            {
                VERBOSE(VB_IMPORTANT,
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
        return;
    }

    if (timeout_ms >= 0)
        OpenFile(filename, timeout_ms);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
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
        for (; eit != RingBuffer::subExt.end(); eit++)
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
        if (fi.exists() && (fi.size() >= RingBuffer::kReadTestSize))
            return fi.absoluteFilePath();
    }

    return QString::null;
}

/** \brief Opens a file for reading.
 *
 *  \param lfilename  Name of file to read
 *  \param retry_ms   How many ms to retry reading the file
 *                    after the first try before giving up.
 */
void RingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("OpenFile(%1, %2 ms)")
            .arg(lfilename).arg(retry_ms));

    if (retry_ms >= 1 && retry_ms <= 20)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("OpenFile(%1, %2 ms)")
                .arg(lfilename).arg(retry_ms) +
                "Timeout small, assuming caller meant for this "
                "to be retries." + "\n\t\t\t"
                "WARNING: This workaround will be removed after the "
                "0.24 release, and this client may stop working.");
        retry_ms *= 150;
        qWarning("Applying temporary OpenFile() hack");
    }

    rwlock.lockForWrite();

    filename = lfilename;

    if (remotefile)
    {
        delete remotefile;
    }

    if (fd2 >= 0)
    {
        close(fd2);
        fd2 = -1;
    }

    bool is_local = false;
    bool is_dvd = false;
    (void) is_dvd; // not used when frontend is disabled.
    bool is_bd = false;
    (void) is_bd;

    if (!streamOnly && filename.startsWith("myth://"))
    {
        struct stat fileInfo;
        if ((RemoteFile::Exists(filename, &fileInfo)) &&
            (S_ISDIR(fileInfo.st_mode)))
        {
            QString tmpFile = filename + "/VIDEO_TS";
            if (RemoteFile::Exists(tmpFile))
            {
                is_dvd = true;
            }
            else
            {
                tmpFile = filename + "/BDMV";
                if (RemoteFile::Exists(tmpFile))
                    is_bd = true;
            }
        }
    }

    if ((filename.left(1) == "/") ||
        (QFile::exists(filename)))
        is_local = true;

#ifdef USING_FRONTEND
    else if ((!streamOnly) &&
             ((filename.startsWith("dvd:")) || is_dvd ||
              ((filename.startsWith("myth://")) &&
               ((filename.endsWith(".img")) ||
                (filename.endsWith(".iso"))))))
    {
        is_dvd = true;
        dvdPriv = new DVDRingBufferPriv();
        startreadahead = false;

        if (filename.left(6) == "dvd://")      // 'Play DVD' sends "dvd:/" + dev
            filename.remove(0,5);              //             e.g. "dvd://dev/sda"
        else if (filename.left(5) == "dvd:/")  // Less correct URI "dvd:" + path
            filename.remove(0,4);              //             e.g. "dvd:/videos/ET"

        if (QFile::exists(filename) || filename.startsWith("myth://"))
            VERBOSE(VB_PLAYBACK, "OpenFile() trying DVD at " + filename);
        else
        {
            filename = "/dev/dvd";
        }
    }
    else if ((!streamOnly) && (filename.left(3) == "bd:" || is_bd))
    {
        is_bd = true;
        bdPriv = new BDRingBufferPriv();
        startreadahead = false;

        if (filename.left(5) == "bd://")      // 'Play DVD' sends "bd:/" + dev
            filename.remove(0,4);             //             e.g. "bd://dev/sda"
        else if (filename.left(4) == "bd:/")  // Less correct URI "bd:" + path
            filename.remove(0,3);             //             e.g. "bd:/videos/ET"

        if (QFile::exists(filename) || filename.startsWith("myth://"))
            VERBOSE(VB_PLAYBACK, "OpenFile() trying BD at " + filename);
        else
        {
            filename = "/dev/dvd";
        }
    }
#endif // USING_FRONTEND

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
            QByteArray fname = filename.toLocal8Bit();
            fd2 = open(fname.constData(),
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
#if HAVE_POSIX_FADVISE
                        posix_fadvise(fd2, 0, 0, POSIX_FADV_SEQUENTIAL);
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
                oldfile = fi.lastModified()
                    .secsTo(QDateTime::currentDateTime()) > 60;
                QString extension = fi.completeSuffix().toLower();
                if (is_subtitle_possible(extension))
                    subtitlefilename = local_sub_filename(fi);
                break;
            }
            case 1:
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("OpenFile(): Could not open."));
                break;
            case 2:
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("OpenFile(): File too small (%1B).")
                        .arg(QFileInfo(filename).size()));
                break;
            case 3:
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "OpenFile(): Improper permissions.");
                break;
            case 4:
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "OpenFile(): Cannot seek in file.");
                break;
            default:
                break;
        }
        VERBOSE(VB_FILE, LOC + QString("OpenFile() made %1 attempts in %2 ms")
                .arg(openAttempts).arg(openTimer.elapsed()));

    }
#ifdef USING_FRONTEND
    else if (is_dvd)
    {
        dvdPriv->OpenFile(filename);
        readblocksize = DVD_BLOCK_SIZE * 62;
    }
    else if (is_bd)
    {
        bdPriv->OpenFile(filename);
        readblocksize = BD_BLOCK_SIZE * 62;
    }
#endif // USING_FRONTEND
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
                for (; eit != subExt.end(); eit++)
                    auxFiles += baseName + *eit;
            }
        }

        remotefile = new RemoteFile(filename, false, true,
                                    retry_ms, &auxFiles);
        if (!remotefile->isOpen())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(filename));
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

    rawbitrate = 8000;
    CalcReadAheadThresh();

    rwlock.unlock();
}

/** \fn RingBuffer::IsOpen(void) const
 *  \brief Returns true if the file is open for either reading or writing.
 */
bool RingBuffer::IsOpen(void) const
{
    rwlock.lockForRead();
    bool ret;
#ifdef USING_FRONTEND
    ret = tfw || (fd2 > -1) || remotefile || (dvdPriv && dvdPriv->IsOpen()) ||
           (bdPriv && bdPriv->IsOpen());
#else // if !USING_FRONTEND
    ret = tfw || (fd2 > -1) || remotefile;
#endif // !USING_FRONTEND
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::~RingBuffer(void)
 *  \brief Shuts down any threads and closes any files.
 */
RingBuffer::~RingBuffer(void)
{
    KillReadAheadThread();

    rwlock.lockForWrite();

    if (remotefile)
    {
        delete remotefile;
        remotefile = NULL;
    }

    if (tfw)
    {
        delete tfw;
        tfw = NULL;
    }

    if (fd2 >= 0)
    {
        close(fd2);
        fd2 = -1;
    }

#ifdef USING_FRONTEND
    if (dvdPriv)
    {
        delete dvdPriv;
    }
    if (bdPriv)
    {
        delete bdPriv;
    }
#endif // USING_FRONTEND

    if (readAheadBuffer) // this only runs if thread is terminated
    {
        delete [] readAheadBuffer;
        readAheadBuffer = NULL;
    }

    rwlock.unlock();
}

/** \fn RingBuffer::Reset(bool, bool, bool)
 *  \brief Resets the read-ahead thread and our position in the file
 */
void RingBuffer::Reset(bool full, bool toAdjust, bool resetInternal)
{
    VERBOSE(VB_FILE, LOC + QString("Reset(%1,%2,%3)")
            .arg(full).arg(toAdjust).arg(resetInternal));

    rwlock.lockForWrite();
    poslock.lockForWrite();

    numfailures = 0;
    commserror = false;
    setswitchtonext = false;

    writepos = 0;
    readpos = (toAdjust) ? (readpos - readAdjust) : 0;

    if (readpos != 0)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString(
                "RingBuffer::Reset() nonzero readpos.  toAdjust: %1 readpos: %2"
                " readAdjust: %3").arg(toAdjust).arg(readpos).arg(readAdjust));
    }

    readAdjust = 0;
    readpos = (readpos < 0) ? 0 : readpos;

    if (full)
        ResetReadAhead(readpos);

    if (resetInternal)
        internalreadpos = readpos;

    generalWait.wakeAll();
    poslock.unlock();
    rwlock.unlock();
}

/** \fn RingBuffer::safe_read(int, void*, uint)
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
int RingBuffer::safe_read(int fd, void *data, uint sz)
{
    int ret;
    unsigned tot = 0;
    unsigned errcnt = 0;
    unsigned zerocnt = 0;

    if (fd2 < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Invalid file descriptor in 'safe_read()'");
        return 0;
    }

    if (stopreads)
        return 0;

    while (tot < sz)
    {
        ret = read(fd2, (char *)data + tot, sz - tot);
        if (ret < 0)
        {
            if (errno == EAGAIN)
                continue;

            VERBOSE(VB_IMPORTANT,
                    LOC_ERR + "File I/O problem in 'safe_read()'" + ENO);

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

        if (ret == 0) // EOF returns 0
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

/** \fn RingBuffer::safe_read(RemoteFile*, void*, uint)
 *  \brief Reads data from the RemoteFile.
 *
 *  \param rf   RemoteFile to read from
 *  \param data Pointer to where data will be written
 *  \param sz   Number of bytes to read
 *  \return Returns number of bytes read
 */
int RingBuffer::safe_read(RemoteFile *rf, void *data, uint sz)
{
    int ret = rf->Read(data, sz);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "RingBuffer::safe_read(RemoteFile* ...): read failed");
            
        poslock.lockForRead();
        rf->Seek(internalreadpos - readAdjust, SEEK_SET);
        poslock.unlock();
        numfailures++;
    }
    else if (ret == 0)
    {
        VERBOSE(VB_FILE, LOC +
                "RingBuffer::safe_read(RemoteFile* ...): at EOF");
    }

    return ret;
}

/** \fn RingBuffer::UpdateRawBitrate(uint)
 *  \brief Set the raw bit rate, to allow RingBuffer adjust effective bitrate.
 *  \param raw_bitrate Streams average number of kilobits per second when
 *                     playspeed is 1.0
 */
void RingBuffer::UpdateRawBitrate(uint raw_bitrate)
{
    VERBOSE(VB_FILE, LOC + QString("UpdateRawBitrate(%1Kb)").arg(raw_bitrate));
    if (raw_bitrate < 2500)
    {
        VERBOSE(VB_FILE, LOC +
                QString("UpdateRawBitrate(%1Kb) - ignoring bitrate,")
                .arg(raw_bitrate) +
                "\n\t\t\tappears to be abnormally low.");
        return;
    }

    rwlock.lockForWrite();
    rawbitrate = raw_bitrate;
    CalcReadAheadThresh();
    rwlock.unlock();
}

/** \fn RingBuffer::UpdatePlaySpeed(float)
 *  \brief Set the play speed, to allow RingBuffer adjust effective bitrate.
 *  \param play_speed Speed to set. (1.0 for normal speed)
 */
void RingBuffer::UpdatePlaySpeed(float play_speed)
{
    rwlock.lockForWrite();
    playspeed = play_speed;
    CalcReadAheadThresh();
    rwlock.unlock();
}

/** \fn RingBuffer::CalcReadAheadThresh(void)
 *  \brief Calculates fill_min, fill_threshold, and readblocksize
 *         from the estimated effective bitrate of the stream.
 *
 *   WARNING: Must be called with rwlock in write lock state.
 *
 */
void RingBuffer::CalcReadAheadThresh(void)
{
    uint estbitrate = 0;

    readsallowed   = false;
    readblocksize  = max(readblocksize, CHUNK);

    // loop without sleeping if the buffered data is less than this
    fill_threshold = kBufferSize / 8;

    const uint KB32  =  32*1024;
    const uint KB64  =  64*1024;
    const uint KB128 = 128*1024;
    const uint KB256 = 256*1024;
    const uint KB512 = 512*1024;

    estbitrate     = (uint) max(abs(rawbitrate * playspeed),
                                0.5f * rawbitrate);
    estbitrate     = min(rawbitrate * 3, estbitrate);
    int rbs        = (estbitrate > 2500)  ? KB64  : KB32;
    rbs            = (estbitrate > 5000)  ? KB128 : rbs;
    rbs            = (estbitrate > 9000)  ? KB256 : rbs;
    rbs            = (estbitrate > 18000) ? KB512 : rbs;
    readblocksize  = max(rbs,readblocksize);

    // minumum seconds of buffering before allowing read
    float secs_min = 0.25;
    // set the minimum buffering before allowing ffmpeg read
    fill_min        = (uint) ((estbitrate * secs_min) * 0.125f);
    // make this a multiple of ffmpeg block size..
    fill_min        = ((fill_min / KB32) + 1) * KB32;

    VERBOSE(VB_FILE, LOC +
            QString("CalcReadAheadThresh(%1 Kb)\n\t\t\t -> "
                    "threshhold(%2 KB) min read(%3 KB) blk size(%4 KB)")
            .arg(estbitrate).arg(fill_threshold/1024)
            .arg(fill_min/1024).arg(readblocksize/1024));
}

bool RingBuffer::IsNearEnd(double fps, uint vvf) const
{
    rwlock.lockForRead();
    int    sz  = ReadBufAvail();
    uint   rbs = readblocksize;
    // telecom kilobytes (i.e. 1000 per k not 1024)
    uint   tmp = (uint) max(abs(rawbitrate * playspeed), 0.5f * rawbitrate);
    uint   kbits_per_sec = min(rawbitrate * 3, tmp);
    rwlock.unlock();

    // WARNING: readahead_frames can greatly overestimate or underestimate
    //          the number of frames available in the read ahead buffer
    //          when rh_frames is less than the keyframe distance.
    double bytes_per_frame = kbits_per_sec * (1000.0/8.0) / fps;
    double readahead_frames = sz / bytes_per_frame;

    bool near_end = ((vvf + readahead_frames) < 10.0) || (sz < rbs*1.5);

    VERBOSE(VB_PLAYBACK, LOC + "IsReallyNearEnd()"
            <<" br("<<(kbits_per_sec/8)<<"KB)"
            <<" sz("<<(sz / 1000)<<"KB)"
            <<" vfl("<<vvf<<")"
            <<" frh("<<((uint)readahead_frames)<<")"
            <<" ne:"<<near_end);

    return near_end;
}

/// \brief Returns number of bytes available for reading into buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufFree(void) const
{
    rbrlock.lockForRead();
    rbwlock.lockForRead();
    int ret = ((rbwpos >= rbrpos) ? rbrpos + kBufferSize : rbrpos) - rbwpos - 1;
    rbwlock.unlock();
    rbrlock.unlock();
    return ret;
}

/// \brief Returns number of bytes available for reading from buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufAvail(void) const
{
    rbrlock.lockForRead();
    rbwlock.lockForRead();
    int ret = (rbwpos >= rbrpos) ? rbwpos - rbrpos : kBufferSize - rbrpos + rbwpos;
    rbwlock.unlock();
    rbrlock.unlock();
    return ret;
}

/** \fn RingBuffer::ResetReadAhead(long long)
 *  \brief Restart the read-ahead thread at the 'newinternal' position.
 *
 *   This is called after a Seek(long long, int) so that the read-ahead
 *   buffer doesn't contain any stale data, and so that it will read
 *   any new data from the new position in the file.
 *
 *   WARNING: Must be called with rwlock and poslock in write lock state.
 *
 *  \param newinternal Position in file to start reading data from
 */
void RingBuffer::ResetReadAhead(long long newinternal)
{
    VERBOSE(VB_FILE, LOC + QString("ResetReadAhead(internalreadpos = %1->%2)")
            .arg(internalreadpos).arg(newinternal));

    rbrlock.lockForWrite();
    rbwlock.lockForWrite();

    CalcReadAheadThresh();
    rbrpos = 0;
    rbwpos = 0;
    internalreadpos = newinternal;
    ateof = false;
    readsallowed = false;
    setswitchtonext = false;
    generalWait.wakeAll();

    rbwlock.unlock();
    rbrlock.unlock();
}

/**
 *  \brief Starts the read-ahead thread.
 *
 *   If the RingBuffer constructor was not called with a usereadahead
 *   of true of if this was reset to false because we're dealing with
 *   a DVD the read ahead thread will not be started.
 *
 *   If this RingBuffer is in write-mode a warning will be printed and
 *   the read ahead thread will not be started.
 *
 *   If the read ahead thread is already running a warning will be printed
 *   and the read ahead thread will not be started.
 *
 */
void RingBuffer::Start(void)
{
    bool do_start = true;

    rwlock.lockForWrite();
    if (!startreadahead)
    {
        do_start = false;
    }
    else if (writemode)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Not starting read ahead thread, "
                "this is a write only RingBuffer");
        do_start = false;
    }
    else if (readaheadrunning)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Not starting read ahead thread, "
                "already running");
        do_start = false;
    }

    if (!do_start)
    {
        rwlock.unlock();
        return;
    }

    StartReads();

    QThread::start();

    while (readaheadrunning && !reallyrunning)
        generalWait.wait(&rwlock);

    rwlock.unlock();
}

/** \fn RingBuffer::KillReadAheadThread(void)
 *  \brief Stops the read-ahead thread, and waits for it to stop.
 */
void RingBuffer::KillReadAheadThread(void)
{
    while (isRunning())
    {
        rwlock.lockForWrite();
        readaheadrunning = false;
        StopReads();
        generalWait.wakeAll();
        rwlock.unlock();
        QThread::wait(5000);
    }
}

/** \fn RingBuffer::StopReads(void)
 *  \brief ????
 *  \sa StartReads(void), Pause(void)
 */
void RingBuffer::StopReads(void)
{
    stopreads = true;
    generalWait.wakeAll();
}

/** \fn RingBuffer::StartReads(void)
 *  \brief ????
 *  \sa StopReads(void), Unpause(void)
 */
void RingBuffer::StartReads(void)
{
    stopreads = false;
    generalWait.wakeAll();
}

/** \fn RingBuffer::Pause(void)
 *  \brief Pauses the read-ahead thread. Calls StopReads(void).
 *  \sa Unpause(void), WaitForPause(void)
 */
void RingBuffer::Pause(void)
{
    StopReads();

    rwlock.lockForWrite();
    request_pause = true;
    rwlock.unlock();
}

/** \fn RingBuffer::Unpause(void)
 *  \brief Unpauses the read-ahead thread. Calls StartReads(void).
 *  \sa Pause(void)
 */
void RingBuffer::Unpause(void)
{
    StartReads();

    rwlock.lockForWrite();
    request_pause = false;
    generalWait.wakeAll();
    rwlock.unlock();
}

/// Returns false iff read-ahead is not running and read-ahead is not paused.
bool RingBuffer::isPaused(void) const
{
    rwlock.lockForRead();
    bool ret = !readaheadrunning || paused;
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::WaitForPause(void)
 *  \brief Waits for Pause(void) to take effect.
 */
void RingBuffer::WaitForPause(void)
{
    MythTimer t;
    t.start();

    rwlock.lockForRead();
    while (readaheadrunning && !paused && request_pause)
    {
        generalWait.wait(&rwlock, 1000);
        if (readaheadrunning && !paused && request_pause && t.elapsed() > 1000)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    QString("Waited %1 ms for ringbuffer pause..")
                    .arg(t.elapsed()));
        }
    }
    rwlock.unlock();
}

bool RingBuffer::PauseAndWait(void)
{
    const uint timeout = 500; // ms

    if (request_pause)
    {
        if (!paused)
        {
            rwlock.unlock();
            rwlock.lockForWrite();

            if (request_pause)
            {
                paused = true;
                generalWait.wakeAll();
            }

            rwlock.unlock();
            rwlock.lockForRead();
        }

        if (request_pause && paused && readaheadrunning)
            generalWait.wait(&rwlock, timeout);
    }

    if (!request_pause && paused)
    {
        rwlock.unlock();
        rwlock.lockForWrite();

        if (!request_pause)
        {
            paused = false;
            generalWait.wakeAll();
        }

        rwlock.unlock();
        rwlock.lockForRead();
    }

    return request_pause || paused;
}

void RingBuffer::run(void)
{
    // These variables are used to adjust the read block size
    struct timeval lastread, now;
    int readtimeavg = 300;
    bool ignore_for_read_timing = true;

    gettimeofday(&lastread, NULL); // this is just to keep gcc happy

    rwlock.lockForWrite();
    poslock.lockForWrite();
    request_pause = false;
    readAheadBuffer = new char[kBufferSize + 1024];
    ResetReadAhead(0);
    readaheadrunning = true;
    reallyrunning = true;
    generalWait.wakeAll();
    poslock.unlock();
    rwlock.unlock();

    // NOTE: this must loop at some point hold only
    // a read lock on rwlock, so that other functions
    // such as reset and seek can take priority.

    rwlock.lockForRead();

    VERBOSE(VB_FILE, LOC + QString("Initial readblocksize %1K & fill_min %2K")
            .arg(readblocksize/1024).arg(fill_min/1024));

    while (readaheadrunning)
    {
        if (PauseAndWait())
        {
            ignore_for_read_timing = true;
            continue;
        }

        long long totfree = ReadBufFree();

        // These are conditions where we don't want to go through
        // the loop if they are true.
        if (((totfree < readblocksize) && readsallowed) ||
            (ignorereadpos >= 0) || commserror || stopreads)
        {
            ignore_for_read_timing |=
                (ignorereadpos >= 0) || commserror || stopreads;
            generalWait.wait(&rwlock, (stopreads) ? 50 : 1000);
            continue;
        }

        // These are conditions where we want to sleep to allow
        // other threads to do stuff.
        if (setswitchtonext || (ateof && readsallowed))
        {
            ignore_for_read_timing = true;
            generalWait.wait(&rwlock, 1000);
            totfree = ReadBufFree();
        }

        int read_return = -1;
        if (totfree >= readblocksize && !commserror &&
            !ateof && !setswitchtonext)
        {
            // limit the read size
            totfree = readblocksize;

            // adapt blocksize
            gettimeofday(&now, NULL);
            if (!ignore_for_read_timing)
            {
                int readinterval = (now.tv_sec  - lastread.tv_sec ) * 1000 +
                    (now.tv_usec - lastread.tv_usec) / 1000;
                readtimeavg = (readtimeavg * 9 + readinterval) / 10;

                if (readtimeavg < 150 && (uint)readblocksize < (kBufferSize>>2))
                {
                    int old_block_size = readblocksize;
                    readblocksize = 3 * readblocksize / 2;
                    readblocksize = ((readblocksize+CHUNK-1) / CHUNK) * CHUNK;
                    VERBOSE(VB_FILE, LOC +
                            QString("Avg read interval was %1 msec. "
                                    "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg(old_block_size/1024)
                            .arg(readblocksize/1024));
                    readtimeavg = 225;
                }
                else if (readtimeavg > 300 && readblocksize > CHUNK)
                {
                    readblocksize -= CHUNK;
                    VERBOSE(VB_FILE, LOC +
                            QString("Avg read interval was %1 msec. "
                                    "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg((readblocksize+CHUNK)/1024)
                            .arg(readblocksize/1024));
                    readtimeavg = 225;
                }
            }
            ignore_for_read_timing = false;
            lastread = now;

            rbwlock.lockForRead();
            if (rbwpos + totfree > kBufferSize)
            {
                totfree = kBufferSize - rbwpos;
                VERBOSE(VB_FILE|VB_EXTRA, LOC +
                        "Shrinking read, near end of buffer");
            }

            if (internalreadpos == 0)
            {
                totfree = max(fill_min, readblocksize);
                VERBOSE(VB_FILE|VB_EXTRA, LOC +
                        "Reading enough data to start playback");
            }

            VERBOSE(VB_FILE|VB_EXTRA,
                    LOC + QString("safe_read(...@%1, %2) -- begin")
                    .arg(rbwpos).arg(totfree));
            if (remotefile)
            {
                if (livetvchain && livetvchain->HasNext())
                    remotefile->SetTimeout(true);
                read_return = safe_read(
                    remotefile, readAheadBuffer + rbwpos, totfree);
            }
#ifdef USING_FRONTEND
            else if (dvdPriv)
            {
                read_return = dvdPriv->safe_read(
                    readAheadBuffer + rbwpos, totfree);
            }
            else if (bdPriv)
            {
                read_return = bdPriv->safe_read(
                    readAheadBuffer + rbwpos, totfree);
            }
#endif // USING_FRONTEND
            else
            {
                read_return = safe_read(fd2, readAheadBuffer + rbwpos, totfree);
            }
            VERBOSE(VB_FILE|VB_EXTRA, LOC +
                    QString("safe_read(...@%1, %2) -> %3")
                    .arg(rbwpos).arg(totfree).arg(read_return));
            rbwlock.unlock();
        }

        if (read_return >= 0)
        {
            poslock.lockForWrite();
            rbwlock.lockForWrite();
            internalreadpos += read_return;
            rbwpos = (rbwpos + read_return) % kBufferSize;
            VERBOSE(VB_FILE|VB_EXTRA,
                    LOC + QString("rbwpos += %1K requested %2K in read")
                    .arg(read_return/1024,3).arg(totfree/1024,3));
            rbwlock.unlock();
            poslock.unlock();
        }

        int used = kBufferSize - ReadBufFree();

        if ((0 == read_return) || (numfailures > 5) ||
            (readsallowed != (used >= fill_min || ateof ||
                              setswitchtonext || commserror)))
        {
            // If readpos changes while the lock is released
            // we should not handle the 0 read_return now.
            long long old_readpos = readpos;

            rwlock.unlock();
            rwlock.lockForWrite();

            commserror |= (numfailures > 5);

            readsallowed = used >= fill_min || ateof ||
                setswitchtonext || commserror;

            if (0 == read_return && old_readpos == readpos)
            {
                if (livetvchain)
                {
                    if (!setswitchtonext && !ignoreliveeof &&
                        livetvchain->HasNext())
                    {
                        livetvchain->SwitchToNext(true);
                        setswitchtonext = true;
                    }
                }
                else
                {
                    VERBOSE(VB_FILE|VB_EXTRA,
                            LOC + "setting ateof (read_return == 0)");
                    ateof = true;
                }
            }

            rwlock.unlock();
            rwlock.lockForRead();
            used = kBufferSize - ReadBufFree();
        }

        VERBOSE(VB_FILE|VB_EXTRA, LOC + "@ end of read ahead loop");

        if (readsallowed || commserror || ateof || setswitchtonext ||
            (wanttoread <= used && wanttoread > 0))
        {
            // To give other threads a good chance to handle these
            // conditions, even if they are only requesting a read lock
            // like us, yield (currently implemented with short usleep).
            generalWait.wakeAll();
            rwlock.unlock();
            usleep(5 * 1000);
            rwlock.lockForRead();            
        }
        else
        {
            // yield if we have nothing to do...
            if (!request_pause &&
                (used >= fill_threshold || ateof || setswitchtonext))
            {
                generalWait.wait(&rwlock, 1000);
            }
        }
    }

    rwlock.unlock();

    rwlock.lockForWrite();
    rbrlock.lockForWrite();
    rbwlock.lockForWrite();

    rbrpos = 0;
    rbwpos = 0;
    reallyrunning = false;
    readsallowed = false;
    delete [] readAheadBuffer;

    readAheadBuffer = NULL;
    rbwlock.unlock();
    rbrlock.unlock();
    rwlock.unlock();
}

long long RingBuffer::SetAdjustFilesize(void)
{
    rwlock.lockForWrite();
    poslock.lockForRead();
    readAdjust += internalreadpos;
    long long ra = readAdjust;
    poslock.unlock();
    rwlock.unlock();
    return ra;
}

int RingBuffer::Peek(void *buf, int count)
{
    int ret = ReadPriv(buf, count, true);
    if (ret != count)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Peek() requested %1 bytes, but only returning %2")
                .arg(count).arg(ret));
    }
    return ret;
}

bool RingBuffer::WaitForReadsAllowed(void)
{
    MythTimer t;
    t.start();

    while (!readsallowed && !stopreads &&
           !request_pause && !commserror && readaheadrunning)
    {
        generalWait.wait(&rwlock, 1000);
        if (!readsallowed && t.elapsed() > 1000)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Taking too long to be allowed to read..");

            if (t.elapsed() > 10000)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Took more than "
                        "10 seconds to be allowed to read, aborting.");
                return false;
            }
        }
    }

    return readsallowed;
}

bool RingBuffer::WaitForAvail(int count)
{
    int avail = ReadBufAvail();
    count = (ateof && avail < count) ? avail : count;

    if (livetvchain && setswitchtonext && avail < count)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Checking to see if there's a "
                              "new livetv program to switch to..");
        livetvchain->ReloadAll();
        return false;
    }

    MythTimer t;
    t.start();
    while ((avail < count) && !stopreads &&
           !request_pause && !commserror && readaheadrunning)
    {
        wanttoread = count;
        generalWait.wait(&rwlock, 250);
        avail = ReadBufAvail();

        if (ateof && avail < count)
            count = avail;

        if (avail < count)
        {
            int elapsed = t.elapsed();
            if  (((elapsed > 250)  && (elapsed < 500))  ||
                 ((elapsed > 500)  && (elapsed < 750))  ||
                 ((elapsed > 1000) && (elapsed < 1250)) ||
                 ((elapsed > 2000) && (elapsed < 2250)) ||
                 ((elapsed > 4000) && (elapsed < 4250)) ||
                 ((elapsed > 8000) && (elapsed < 8250)) ||
                 ((elapsed > 9000)))
            {
                VERBOSE(VB_IMPORTANT, LOC + "Waited " +
                        QString("%1").arg((elapsed / 250) * 0.25f, 3, 'f', 1) +
                        " seconds for data \n\t\t\tto become available..." +
                        QString(" %2 < %3")
                        .arg(avail).arg(count));
            }

            if (elapsed > 16000)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Waited " +
                        QString("%1").arg(elapsed/1000) +
                        " seconds for data, aborting.");
                return false;
            }
        }
    }

    wanttoread = 0;

    return avail >= count;
}

int RingBuffer::ReadDirect(void *buf, int count, bool peek)
{
    long long old_pos = 0;
    if (peek)
    {
        poslock.lockForRead();
        old_pos = (ignorereadpos >= 0) ? ignorereadpos : readpos;
        poslock.unlock();
    }

    int ret;
    if (remotefile)
        ret = safe_read(remotefile, buf, count);
#ifdef USING_FRONTEND
    else if (dvdPriv)
        ret = dvdPriv->safe_read(buf, count);
    else if (bdPriv)
        ret = bdPriv->safe_read(buf, count);
#endif // USING_FRONTEND
    else if (fd2 >= 0)
        ret = safe_read(fd2, buf, count);
    else
    {
        ret = -1;
        errno = EBADF;
    }

    poslock.lockForWrite();
    if (ignorereadpos >= 0 && ret > 0)
    {
        if (peek)
        {
            // seek should always succeed since we were at this position
            if (remotefile)
                remotefile->Seek(old_pos, SEEK_SET);
            else
                lseek64(fd2, old_pos, SEEK_SET);
        }
        else
        {
            ignorereadpos += ret;
        }
        poslock.unlock();
        return ret;
    }
    poslock.unlock();

    if (peek && ret > 0)
    {
        if (!dvdPriv && !bdPriv)
        {
            long long new_pos = Seek(old_pos, SEEK_SET, true);
            if (new_pos != old_pos)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("Peek() Failed to return from new "
                                "position %1 to old position %2, now "
                                "at position %3")
                        .arg(old_pos - ret).arg(old_pos).arg(new_pos));
            }
        }
        else if (old_pos != 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "DVD and Blu-Ray do not support arbitrary "
                    "peeks except when read-ahead is enabled."
                    "\n\t\t\tWill seek to beginning of video.");
        }
#ifdef USING_FRONTEND
        if (dvdPriv)
            dvdPriv->NormalSeek(old_pos);
        else if (bdPriv)
            bdPriv->Seek(old_pos);
#endif // USING_FRONTEND
    }

    return ret;
}

/** \brief When possible reads from the read-ahead buffer,
 *         otherwise reads directly from the device.
 *
 *  \param buf   Pointer to where data will be written
 *  \param count Number of bytes to read
 *  \param peek  If true, don't increment read count
 *  \return Returns number of bytes read
 */
int RingBuffer::ReadPriv(void *buf, int count, bool peek)
{
    QString loc_desc = 
            QString("ReadPriv(..%1, %2)")
        .arg(count).arg(peek?"peek":"normal");
    VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc +
            QString(" @%1 -- begin").arg(rbrpos));

    rwlock.lockForRead();
    if (writemode)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + loc_desc +
                ": Attempt to read from a write only file");
        errno = EBADF;
        rwlock.unlock();
        return -1;
    }

    if (commserror)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + loc_desc +
                ": Attempt to read after commserror set");
        errno = EIO;
        rwlock.unlock();
        return -1;
    }

    if (request_pause || stopreads || !readaheadrunning || (ignorereadpos>=0))
    {
        rwlock.unlock();
        rwlock.lockForWrite();
        // we need a write lock so the read-ahead thread
        // can't start mucking with the read position.
        // If the read ahead thread was started while we
        // didn't hold the lock, we proceed with a normal
        // read from the buffer, otherwise we read directly.
        if (request_pause || stopreads ||
            !readaheadrunning || (ignorereadpos >= 0))
        {
            int ret = ReadDirect(buf, count, peek);
            VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc +
                    QString(": ReadDirect checksum %1")
                    .arg(qChecksum((char*)buf,count)));
            rwlock.unlock();
            return ret;
        }
        rwlock.unlock();
        rwlock.lockForRead();
    }

    if (!WaitForReadsAllowed())
    {
        VERBOSE(VB_FILE, LOC + loc_desc + ": !WaitForReadsAllowed()");
        rwlock.unlock();
        rwlock.lockForWrite();
        wanttoread = 0;
        stopreads = true;
        rwlock.unlock();
        return 0;
    }

    if (!WaitForAvail(count))
    {
        VERBOSE(VB_FILE, LOC + loc_desc + ": !WaitForAvail()");
        rwlock.unlock();
        rwlock.lockForWrite();
        ateof = true;
        wanttoread = 0;
        stopreads = true;
        rwlock.unlock();
        return 0;
    }

    count = min(ReadBufAvail(), count);

    if (count <= 0)
    {
        // this can happen under a few conditions but the most
        // notable is an exit from the read ahead thread or
        // the end of the file stream has been reached.
        VERBOSE(VB_FILE, LOC + loc_desc + ": ReadBufAvail() == 0");
        rwlock.unlock();
        return count;
    }

    if (peek)
        rbrlock.lockForRead();
    else
        rbrlock.lockForWrite();

    VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc + " -- copying data");

    if (rbrpos + count > (int) kBufferSize)
    {
        int firstsize = kBufferSize - rbrpos;
        int secondsize = count - firstsize;

        memcpy(buf, readAheadBuffer + rbrpos, firstsize);
        memcpy((char *)buf + firstsize, readAheadBuffer, secondsize);
    }
    else
    {
        memcpy(buf, readAheadBuffer + rbrpos, count);
    }
    VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc + QString(" -- checksum %1")
            .arg(qChecksum((char*)buf,count)));

    if (!peek)
    {
        rbrpos = (rbrpos + count) % kBufferSize;
        generalWait.wakeAll();
    }
    rbrlock.unlock();
    rwlock.unlock();

    return count;
}

/** \fn RingBuffer::Read(void*, int)
 *  \brief This is the public method for reading from a file,
 *         it calls the appropriate read method if the file
 *         is remote or buffered, or a BD/DVD.
 *  \param buf   Pointer to where data will be written
 *  \param count Number of bytes to read
 *  \return Returns number of bytes read
 */
int RingBuffer::Read(void *buf, int count)
{
    int ret = ReadPriv(buf, count, false);
    if (ret > 0)
    {
        poslock.lockForWrite();
        readpos += ret;
        poslock.unlock();
    }
    return ret;
}

/** \fn RingBuffer::IsIOBound(void) const
 *  \brief Returns true if a RingBuffer::Write(void*,int) is likely to block.
 */
bool RingBuffer::IsIOBound(void) const
{
    bool ret = false;
    int used, free;
    rwlock.lockForRead();

    if (!tfw)
    {
        rwlock.unlock();
        return ret;
    }

    used = tfw->BufUsed();
    free = tfw->BufFree();

    ret = (used * 5 > free);

    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::Write(const void*, uint)
 *  \brief Writes buffer to ThreadedFileWriter::Write(const void*,uint)
 *  \return Bytes written, or -1 on error.
 */
int RingBuffer::Write(const void *buf, uint count)
{
    rwlock.lockForRead();

    if (!writemode)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Tried to write to a read only file.");
        rwlock.unlock();
        return -1;
    }

    if (!tfw && !remotefile)
    {
        rwlock.unlock();
        return -1;
    }

    int ret = -1;
    if (tfw)
        ret = tfw->Write(buf, count);
    else
        ret = remotefile->Write(buf, count);

    if (ret > 0)
    {
        poslock.lockForWrite();
        writepos += ret;
        poslock.unlock();
    }

    rwlock.unlock();

    return ret;
}

/** \fn RingBuffer::Sync(void)
 *  \brief Calls ThreadedFileWriter::Sync(void)
 */
void RingBuffer::Sync(void)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->Sync();
    rwlock.unlock();
}

/** \brief Seeks to a particular position in the file.
 */
long long RingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    VERBOSE(VB_FILE, LOC + QString("Seek(%1,%2,%3)")
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
        VERBOSE(VB_FILE, LOC +
                QString("Seek(): rbrpos: %1 rbwpos: %2"
                        "\n\t\t\treadpos: %3 internalreadpos: %4")
                .arg(rbrpos).arg(rbwpos)
                .arg(readpos).arg(internalreadpos));
        bool used_opt = false;
        if ((new_pos < readpos))
        {
            int min_safety = max(fill_min, readblocksize);
            int free = ((rbwpos >= rbrpos) ?
                        rbrpos + kBufferSize : rbrpos) - rbwpos;
            int internal_backbuf =
                (rbwpos >= rbrpos) ? rbrpos : rbrpos - rbwpos;
            internal_backbuf = min(internal_backbuf, free - min_safety);
            long long sba = readpos - new_pos;
            VERBOSE(VB_FILE, LOC +
                    QString("Seek(): internal_backbuf: %1 sba: %2")
                    .arg(internal_backbuf).arg(sba));
            if (internal_backbuf >= sba)
            {
                rbrpos = (rbrpos>=sba) ? rbrpos - sba :
                    kBufferSize + rbrpos - sba;
                used_opt = true;
                VERBOSE(VB_FILE, LOC +
                        QString("Seek(): OPT1 rbrpos: %1 rbwpos: %2"
                                "\n\t\t\treadpos: %3 internalreadpos: %4")
                        .arg(rbrpos).arg(rbwpos)
                        .arg(new_pos).arg(internalreadpos));
            }
        }
        else if ((new_pos >= readpos) && (new_pos <= internalreadpos))
        {
            rbrpos = (rbrpos + (new_pos - readpos)) % kBufferSize;
            used_opt = true;
            VERBOSE(VB_FILE, LOC +
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
                    ret = lseek64(fd2, internalreadpos, SEEK_SET);
                VERBOSE(VB_FILE, LOC +
                        QString("Seek to %1 from ignore pos %2 returned %3")
                        .arg(internalreadpos).arg(ignorereadpos).arg(ret));
                ignorereadpos = -1;
            }
            readpos = new_pos;
            poslock.unlock();
            generalWait.wakeAll();
            ateof = false;
            readsallowed = false;
            if (!has_lock)
                rwlock.unlock();
            return new_pos;
        }
    }
#endif

#if 0
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
#ifdef __FreeBSD__
        else if (llabs(new_pos-readpos) > 100000000LL)
#else
        else if (abs(new_pos-readpos) > 100000000LL)
#endif
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
        if (off_end == 250000)
        {
            VERBOSE(VB_FILE, LOC +
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

                VERBOSE(VB_IMPORTANT, LOC_ERR + cmd + " Failed" + ENO);

                // try to return to former position..
                if (remotefile)
                    ret = remotefile->Seek(internalreadpos, SEEK_SET);
                else
                    ret = lseek64(fd2, internalreadpos, SEEK_SET);
                if (ret < 0)
                {
                    QString cmd = QString("Seek(%1, SEEK_SET) int ")
                        .arg(internalreadpos);
                    VERBOSE(VB_IMPORTANT, LOC_ERR + cmd + " Failed" + ENO);
                }
                else
                {
                    QString cmd = QString("Seek(%1, %2) int ")
                        .arg(internalreadpos)
                        .arg((SEEK_SET == whence) ? "SEEK_SET" :
                             ((SEEK_CUR == whence) ?"SEEK_CUR" : "SEEK_END"));
                    VERBOSE(VB_IMPORTANT, LOC_ERR + cmd + " succeeded");
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
#ifdef USING_FRONTEND
    else if ((dvdPriv || bdPriv) && (SEEK_END == whence))
    {
        errno = EINVAL;
        ret = -1;
    }
    else if (dvdPriv)
    {
        dvdPriv->NormalSeek(new_pos);
        ret = new_pos;
    }
    else if (bdPriv)
    {
        bdPriv->Seek(new_pos);
        ret = new_pos;
    }
#endif // USING_FRONTEND
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
            .arg((SEEK_SET == whence) ? "SEEK_SET" :
                 ((SEEK_CUR == whence) ?"SEEK_CUR" : "SEEK_END"));
        VERBOSE(VB_IMPORTANT, LOC_ERR + cmd + " Failed" + ENO);
    }

    poslock.unlock();

    generalWait.wakeAll();

    if (!has_lock)
        rwlock.unlock();

    return ret;
}

/** \brief Calls ThreadedFileWriter::Seek(long long,int).
 */
long long RingBuffer::WriterSeek(long long pos, int whence, bool has_lock)
{
    long long ret = -1;

    if (!has_lock)
        rwlock.lockForRead();

    poslock.lockForWrite();

    if (tfw)
    {
        ret = tfw->Seek(pos, whence);
        writepos = ret;
    }

    poslock.unlock();

    if (!has_lock)
        rwlock.unlock();

    return ret;
}

/** \fn RingBuffer::WriterFlush(void)
 *  \brief Calls ThreadedFileWriter::Flush(void) and
 *         ThreadedFileWriter::Sync(void).
 */
void RingBuffer::WriterFlush(void)
{
    rwlock.lockForRead();
    if (tfw)
    {
        tfw->Flush();
        tfw->Sync();
    }
    rwlock.unlock();
}

/** \fn RingBuffer::SetWriteBufferSize(int)
 *  \brief Calls ThreadedFileWriter::SetWriteBufferSize(int)
 */
void RingBuffer::SetWriteBufferSize(int newSize)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->SetWriteBufferSize(newSize);
    rwlock.unlock();
}

/** \fn RingBuffer::SetWriteBufferMinWriteSize(int)
 *  \brief Calls ThreadedFileWriter::SetWriteBufferMinWriteSize(int)
 */
void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->SetWriteBufferMinWriteSize(newMinSize);
    rwlock.unlock();
}

/** \brief Tell RingBuffer if this is an old file or not.
 *
 *  Normally the RingBuffer determines that the file is old
 *  if it has not been modified in the last minute. This
 *  allows one to override that determination externally.
 *
 *  If for instance you are slowly writing to the file you
 *  could call this with the value of false. If you just
 *  finished writing the file you could call it with the
 *  value true. Knowing that the file is old allows MythTV
 *  to determine that a read at the end of the file is
 *  really an end-of-file condition more quickly. But if
 *  the file is growing it can also cause the RingBuffer to
 *  report an end-of-file condition prematurely.
 */
void RingBuffer::SetOldFile(bool is_old)
{
    rwlock.lockForWrite();
    oldfile = is_old;
    rwlock.unlock();
}

// This appears to allow direct access to the DVD/BD device
// when called with false (the default value), and enable
// the ring buffer when called with true. But I'm not entirely
// certain. -- dtk 2010-08-26
void RingBuffer::SetStreamOnly(bool stream)
{
    rwlock.lockForWrite();
    streamOnly = stream;
    rwlock.unlock();
}

/// Returns name of file used by this RingBuffer
QString RingBuffer::GetFilename(void) const
{
    rwlock.lockForRead();
    QString tmp = filename;
    tmp.detach();
    rwlock.unlock();
    return tmp;
}

QString RingBuffer::GetSubtitleFilename(void) const
{
    rwlock.lockForRead();
    QString tmp = subtitlefilename;
    tmp.detach();
    rwlock.unlock();
    return tmp;
}

/** \fn RingBuffer::GetReadPosition(void) const
 *  \brief Returns how far into the file we have read.
 */
long long RingBuffer::GetReadPosition(void) const
{
    rwlock.lockForRead();
    poslock.lockForRead();
    long long ret = readpos;
#ifdef USING_FRONTEND
    if (dvdPriv)
        ret = dvdPriv->GetReadPosition();
    else if (bdPriv)
        ret = bdPriv->GetReadPosition();
#endif // USING_FRONTEND
    poslock.unlock();
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::GetWritePosition(void) const
 *  \brief Returns how far into a ThreadedFileWriter file we have written.
 */
long long RingBuffer::GetWritePosition(void) const
{
    poslock.lockForRead();
    long long ret = writepos;
    poslock.unlock();
    return ret;
}

/** \fn RingBuffer::GetRealFileSize(void) const
 *  \brief Returns the size of the file we are reading/writing,
 *         or -1 if the query fails.
 */
long long RingBuffer::GetRealFileSize(void) const
{
    rwlock.lockForRead();
    long long ret = -1;
    if (remotefile)
        ret = remotefile->GetFileSize();
    else
        ret = QFileInfo(filename).size();
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::LiveMode(void) const
 *  \brief Returns true if this RingBuffer has been assigned a LiveTVChain.
 *  \sa SetLiveMode(LiveTVChain*)
 */
bool RingBuffer::LiveMode(void) const
{
    rwlock.lockForRead();
    bool ret = (livetvchain);
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::SetLiveMode(LiveTVChain*)
 *  \brief Assigns a LiveTVChain to this RingBuffer
 *  \sa LiveMode(void)
 */
void RingBuffer::SetLiveMode(LiveTVChain *chain)
{
    rwlock.lockForWrite();
    livetvchain = chain;
    rwlock.unlock();
}

/// Tells RingBuffer whether to igonre the end-of-file
void RingBuffer::IgnoreLiveEOF(bool ignore)
{
    rwlock.lockForWrite();
    ignoreliveeof = ignore;
    rwlock.unlock();
}

/** \brief Returns true if this is a DVD backed RingBuffer.
 *
 * NOTE: This is not locked because ReadDirect calls
 * DVD safe_read which sleeps with a write lock on
 * rwlock in the DVDNAV_WAIT condition.
 *
 * Due to the lack of locking is only safe to call once OpenFile()
 * has completed.
 */
bool RingBuffer::IsDVD(void) const
{
    //rwlock.lockForRead();
    bool ret = dvdPriv;
    //rwlock.unlock();
    return ret;
}

/** \brief Returns true if this is a DVD backed RingBuffer.
 *
 * NOTE: This is not locked because ReadDirect calls
 * DVD safe_read which sleeps with a write lock on
 * rwlock in the DVDNAV_WAIT condition.
 *
 * Due to the lack of locking is only safe to call once OpenFile()
 * has completed.
 */
bool RingBuffer::InDVDMenuOrStillFrame(void)
{
    //rwlock.lockForRead();
    bool ret = false;
#ifdef USING_FRONTEND
    if (dvdPriv)
        ret = (dvdPriv->IsInMenu() || dvdPriv->InStillFrame());
#endif // USING_FRONTEND
    //rwlock.unlock();
    return ret;
}

/// Returns true if this is a Blu-ray backed RingBuffer.
bool RingBuffer::IsBD(void) const
{
    //rwlock.lockForRead();
    bool ret = bdPriv;
    //rwlock.unlock();
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
