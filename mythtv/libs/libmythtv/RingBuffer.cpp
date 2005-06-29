#include <qapplication.h>
#include <qdatetime.h>
#include <qfileinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <qsocket.h>
#include <qfile.h>
#include <qregexp.h>

using namespace std;

#include "exitcodes.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "remotefile.h"
#include "remoteencoder.h"

#define TFW_DEF_BUF_SIZE (2*1024*1024)
#define TFW_MAX_WRITE_SIZE (TFW_DEF_BUF_SIZE / 4)
#define TFW_MIN_WRITE_SIZE (TFW_DEF_BUF_SIZE / 8)

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
#define HAVE_FDATASYNC
#endif

#define READ_TEST_SIZE 204
#define OPEN_READ_ATTEMPTS 5

#define DVD_BLOCK_SIZE 2048
#ifdef HAVE_DVDNAV
#  include <dvdnav/dvdnav.h>
#  include <dvdnav/nav_read.h>

static const char *dvdnav_menu_table[] = {
  NULL,
  NULL,
  "Title",
  "Root",
  "Subpicture",
  "Audio",
  "Angle",
  "Part"
};

#define DVD_MENU_MAX 7

// A spiffy little class to allow a RingBuffer to read from DVDs.
// This should really be a specialized version of the RingBuffer, but
// until it's all done and tested I'm not real sure about what stuff
// needs to be in a RingBufferBase class.
class DVDRingBufferPriv
{
    public:
    
        DVDRingBufferPriv() 
        {
            dvdnav = NULL;
            dvdBlockRPos = 0;
            dvdBlockWPos = 0;
            part = 0;
            title = 0;           
        }
        
        long long Seek(long long pos, int whence)
        {
            dvdnav_sector_search(this->dvdnav, pos / DVD_BLOCK_SIZE , whence);
            return GetReadPosition();
        }
            
        int getTitle() { return title;}
        
        int getPart() { return part;}
        
        void getPartAndTitle( int& _part, int& _title)
        {
            _part = part;
            _title = _title;
        }
        
        bool isInMenu() { return (title == 0); }
        
        void getDescForPos(QString& desc)
        {
            if (isInMenu())
            {
                if((part <= DVD_MENU_MAX) && dvdnav_menu_table[part] )
                {
                    desc = QString("%1 Menu").arg(dvdnav_menu_table[part]);
                }
            }
            else
            {
                desc = QObject::tr("Title %1 chapter %2").arg(title).arg(part);
            }
        }
        
        
        bool open(const QString& filename)  
        {
            dvdnav_status_t dvdRet = dvdnav_open(&dvdnav, filename);
            if (dvdRet == DVDNAV_STATUS_ERR)
            {
                VERBOSE(VB_IMPORTANT, QString("Failed to open DVD device at %1").arg(filename));
                return false;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Opened DVD device at %1").arg(filename));
                dvdnav_set_readahead_flag(dvdnav, 1);
                dvdnav_set_PGC_positioning_flag(dvdnav, 1);
                
                int numTitles = 0;
                maxPart = 0;
                mainTitle = 0;
                int titleParts = 0;
                dvdnav_title_play(dvdnav, 0);
                dvdRet = dvdnav_get_number_of_titles(dvdnav, &numTitles);
                if (numTitles == 0 )
                {
                    char buf[DVD_BLOCK_SIZE * 5];
                    VERBOSE(VB_IMPORTANT, QString("Reading %1 bytes from the drive").arg(DVD_BLOCK_SIZE * 5));
                    safe_read(buf, DVD_BLOCK_SIZE * 5);
                    dvdRet = dvdnav_get_number_of_titles(dvdnav, &numTitles);
                }
                
                if( dvdRet == DVDNAV_STATUS_ERR)
                {
                    VERBOSE(VB_IMPORTANT, QString("Failed to get the number of titles on the DVD" ));
                } 
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("There are %1 titles on the disk").arg(numTitles));
                    
                    for( int curTitle = 0; curTitle < numTitles; curTitle++)
                    {
                        dvdnav_get_number_of_parts(dvdnav, curTitle, &titleParts);
                        VERBOSE(VB_IMPORTANT, QString("There are title %1 has %2 parts.")
                                              .arg(curTitle).arg(titleParts));
                        if (titleParts > maxPart)
                        {
                            maxPart = titleParts;
                            mainTitle = curTitle;
                        }
                    }
                    VERBOSE(VB_IMPORTANT, QString("%1 selected as the main title.").arg(mainTitle));
                }                
                
                dvdnav_title_play(dvdnav, mainTitle);
                dvdnav_current_title_info(dvdnav, &title, &part);
                return true;
            }
        }
        
        bool isOpen() { return dvdnav;}
        
        long long GetReadPosition()
        {
            uint32_t pos;
            uint32_t length;
            
            if (dvdnav)        
                dvdnav_get_position(dvdnav, &pos, &length);
            
            return pos * DVD_BLOCK_SIZE;
        }

        long long GetTotalReadPosition()
        {
            uint32_t pos;
            uint32_t length;
            
            if (dvdnav)        
                dvdnav_get_position(dvdnav, &pos, &length);
            
            return length * DVD_BLOCK_SIZE;
        }

        
        int safe_read(void *data, unsigned sz)
        {
            unsigned tot = 0;
            dvdnav_status_t dvdStat;
            unsigned char* blockBuf = NULL;
            int dvdEvent = 0;
            int dvdEventSize = 0;
            int needed = sz;
            char *dest = (char*)data;
            int offset = 0;
        
            while (needed)
            {
                blockBuf = dvdBlockWriteBuf;
                
                // Use the next_cache_block instead of the next_block to avoid a memcpy inside libdvdnav
                dvdStat = dvdnav_get_next_cache_block( dvdnav, &blockBuf, &dvdEvent, &dvdEventSize);
                if(dvdStat == DVDNAV_STATUS_ERR) 
                {
                    VERBOSE(VB_IMPORTANT, QString("Error reading block from DVD: %1")
                                                 .arg(dvdnav_err_to_string(dvdnav)));
                    return -1;
                }
                
                
                
                switch (dvdEvent)
                {
                    case DVDNAV_BLOCK_OK:
                        // We need at least a DVD blocks worth of data so copy it in.
                        memcpy(dest + offset, blockBuf, DVD_BLOCK_SIZE);
                        
                        tot += DVD_BLOCK_SIZE;
                        
                        if (blockBuf != dvdBlockWriteBuf)
                        {
                            dvdnav_free_cache_block(dvdnav, blockBuf);
                        }
              
                        break;
                    case DVDNAV_CELL_CHANGE:
                        {
                            dvdnav_cell_change_event_t *cell_event = (dvdnav_cell_change_event_t*) (blockBuf);
                            pgLength  = cell_event->pg_length;
                            pgcLength = cell_event->pgc_length;
                            cellStart = cell_event->cell_start;
                            pgStart   = cell_event->pg_start;
                            
                            VERBOSE(VB_PLAYBACK, QString("DVDNAV_CELL_CHANGE: pg_length == %1, pgc_length == %2, "
                                                  "cell_start == %3, pg_start == %4")
                                                  .arg(pgLength).arg(pgcLength).arg(cellStart).arg(pgStart));
                            
                            dvdnav_current_title_info(dvdnav, &title, &part);            
                            if (title == 0)
                            {
                                pci_t* pci = dvdnav_get_current_nav_pci(dvdnav);
                                dvdnav_button_select(dvdnav, pci, 1);
                            }
                            
                            if (blockBuf != dvdBlockWriteBuf)
                            {
                                dvdnav_free_cache_block(dvdnav, blockBuf);
                            }
                            
                        }
                        break;
                    case DVDNAV_SPU_CLUT_CHANGE:
                        VERBOSE(VB_PLAYBACK, "DVDNAV_SPU_CLUT_CHANGE happened.");
                        break;
                    
                    case DVDNAV_SPU_STREAM_CHANGE:
                        {
                        dvdnav_spu_stream_change_event_t* spu = (dvdnav_spu_stream_change_event_t*)(blockBuf);
                        VERBOSE(VB_PLAYBACK, QString( "DVDNAV_SPU_STREAM_CHANGE: "
                                                       "physical_wide==%1, physical_letterbox==%2, "
                                                       "physical_pan_scan==%3, logical==%4")
                                                .arg(spu->physical_wide).arg(spu->physical_letterbox)
                                                .arg(spu->physical_pan_scan).arg(spu->logical));
                        
                        if (blockBuf != dvdBlockWriteBuf)
                        {
                            dvdnav_free_cache_block(dvdnav, blockBuf);
                        }                                                   
                        }
                        break;
                    case DVDNAV_AUDIO_STREAM_CHANGE:
                        {
                        dvdnav_audio_stream_change_event_t* apu = (dvdnav_audio_stream_change_event_t*)(blockBuf);
                        VERBOSE(VB_PLAYBACK, QString( "DVDNAV_AUDIO_STREAM_CHANGE: "
                                                       "physical==%1, logical==%2")
                                               .arg(apu->physical).arg(apu->logical));
                            
                        if (blockBuf != dvdBlockWriteBuf)
                        {
                            dvdnav_free_cache_block(dvdnav, blockBuf);
                        }                                                   
                        }
                        break;
                    case DVDNAV_NAV_PACKET:
                        lastNav = (dvdnav_t *)blockBuf;
                        break;
                    case DVDNAV_HOP_CHANNEL:
                        VERBOSE(VB_PLAYBACK, "DVDNAV_HOP_CHANNEL happened.");
                        break;                        
                    case DVDNAV_NOP:
                        break;
                    case DVDNAV_VTS_CHANGE:
                        {
                        dvdnav_vts_change_event_t* vts = (dvdnav_vts_change_event_t*)(blockBuf);
                        VERBOSE(VB_PLAYBACK, QString( "DVDNAV_VTS_CHANGE: "
                                                       "old_vtsN==%1, new_vtsN==%2")
                                                        .arg(vts->old_vtsN).arg(vts->new_vtsN));
                            
                        if (blockBuf != dvdBlockWriteBuf)
                        {
                            dvdnav_free_cache_block(dvdnav, blockBuf);
                        }                                                   
                        }
                        break;
                    case DVDNAV_HIGHLIGHT:
                        {
                        dvdnav_highlight_event_t* hl = (dvdnav_highlight_event_t*)(blockBuf);
                        VERBOSE(VB_PLAYBACK, QString( "DVDNAV_HIGHLIGHT: "
                                                       "display==%1, palette==%2, "
                                                       "sx==%3, sy==%4, ex==%5, ey==%6, "
                                                       "pts==%7, buttonN==%8")
                                                       .arg(hl->display).arg(hl->palette)
                                                       .arg(hl->sx).arg(hl->sy)
                                                       .arg(hl->ex).arg(hl->ey)
                                                       .arg(hl->pts).arg(hl->buttonN));
                        if (blockBuf != dvdBlockWriteBuf)
                        {
                            dvdnav_free_cache_block(dvdnav, blockBuf);
                        }                                                   

                        }
                        break;
                    case DVDNAV_STILL_FRAME:
                        {
                        dvdnav_still_event_t* still = (dvdnav_still_event_t*)(blockBuf);
                        VERBOSE(VB_PLAYBACK, QString("DVDNAV_STILL_FRAME: needs displayed for %1 seconds")
                                             .arg(still->length));
                        if (blockBuf != dvdBlockWriteBuf)
                        {
                            dvdnav_free_cache_block(dvdnav, blockBuf);
                        }                                                   
                        
                        dvdnav_still_skip(dvdnav);
                        }
                        break;
                    case DVDNAV_WAIT:
                        VERBOSE(VB_PLAYBACK, "DVDNAV_WAIT recieved clearing it");
                        dvdnav_wait_skip(dvdnav);
                        break;
                    default:
                        cerr << "Got DVD event " << dvdEvent << endl;            
                        break;
                }
            
                needed = sz - tot;
                offset = tot;
            }
        
            return tot;
        }
        
        void nextTrack()
        {
            int newPart = part + 1;
            if (newPart < maxPart)
                dvdnav_part_play(dvdnav, title, newPart);
        }
        
        void prevTrack()
        {
            int newPart = part - 1;
            if (newPart < 0)
                newPart = 0;
            
            dvdnav_part_play(dvdnav, title, newPart);
        }
        
    protected:
        dvdnav_t *dvdnav;
        unsigned char dvdBlockWriteBuf[DVD_BLOCK_SIZE];
        unsigned char* dvdBlockReadBuf;
        int dvdBlockRPos;
        int dvdBlockWPos;
        long long pgLength;
        long long pgcLength;
        long long cellStart;
        long long pgStart;
        dvdnav_t *lastNav; // This really belongs in the player.
        int part;
        int title;
        int maxPart;
        int mainTitle;
};    



bool RingBuffer::IsOpen(void) 
{ 
    return (tfw || fd2 > -1 || remotefile || (dvdPriv && dvdPriv->isOpen())); 
}

#else

// Stub version to cut down on the ifdefs.
class DVDRingBufferPriv
{
    public:
        DVDRingBufferPriv(){}
        bool open(const QString& filename){return false;}
        bool isOpen() { return false;}
        long long Seek(long long pos, int whence) { return 0; }
        int safe_read(void *data, unsigned sz) { return -1; }
        long long GetReadPosition() { return 0; }
        long long GetTotalReadPosition() { return 0; }
        void getPartAndTitle( int& title, int& part) { title = 0; part = 0; }
        void nextTrack(){};
        void prevTrack(){};
        void getDescForPos(QString& desc){}; 
};



#endif // DVDNAV



class ThreadedFileWriter
{
public:
    ThreadedFileWriter(const char *filename, int flags, mode_t mode);
    ~ThreadedFileWriter();      /* commits all writes and closes the file. */

    bool Open();

    long long Seek(long long pos, int whence);
    unsigned Write(const void *data, unsigned count);

    // Note, this doesn't even try to flush our queue, only ensure that
    // data which has already been sent to the kernel is written to disk
    void Sync(void);

    void SetWriteBufferSize(int newSize = TFW_DEF_BUF_SIZE);
    void SetWriteBufferMinWriteSize(int newMinSize = TFW_MIN_WRITE_SIZE);

    unsigned BufUsed();  /* # of bytes queued for write by the write thread */
    unsigned BufFree();  /* # of bytes that can be written, without blocking */

protected:
    static void *boot_writer(void *);
    void DiskLoop(); /* The thread that actually calls write(). */

private:
    // allow DiskLoop() to flush buffer completely ignoring low watermark
    void Flush(void);

    const char     *filename;
    int             flags;
    mode_t          mode;
    int             fd;

    bool            no_writes;
    bool            flush;

    unsigned long   tfw_buf_size;
    unsigned long   tfw_min_write_size;
    unsigned int    rpos;
    unsigned int    wpos;
    char           *buf;
    int             in_dtor;
    pthread_t       writer;
    pthread_mutex_t buflock;
};

static unsigned safe_write(int fd, const void *data, unsigned sz)
{
    int ret;
    unsigned tot = 0;
    unsigned errcnt = 0;

    while (tot < sz)
    {
        ret = write(fd, (char *)data + tot, sz - tot);
        if (ret < 0)
        {
            if (errno == EAGAIN)
                continue;

            char msg[128];

            errcnt++;
            snprintf(msg, 127,
                     "ERROR: file I/O problem in safe_write(), errcnt = %d",
                     errcnt);
            perror(msg);
            if (errcnt == 3)
                break;
        }
        else
        {
            tot += ret;
        }
        if (tot < sz)
            usleep(1000);
    }
    return tot;
}

void *ThreadedFileWriter::boot_writer(void *wotsit)
{
    ThreadedFileWriter *fw = (ThreadedFileWriter *)wotsit;
    fw->DiskLoop();
    return NULL;
}

ThreadedFileWriter::ThreadedFileWriter(const char *fname,
                                       int pflags, mode_t pmode)
    : filename(fname), flags(pflags), mode(pmode), fd(-1),
      no_writes(false), flush(false), tfw_buf_size(0),
      tfw_min_write_size(0), rpos(0), wpos(0), buf(NULL),
      in_dtor(0)
{
    pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;
    buflock = init;
}

bool ThreadedFileWriter::Open()
{
    fd = open(filename, flags, mode);

    if (fd < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("ERROR opening file '%1' in ThreadedFileWriter. %2")
            .arg(filename).arg(strerror(errno)));
        return false;
    }
    else
    {
        buf = new char[TFW_DEF_BUF_SIZE + 20];
        tfw_buf_size = TFW_DEF_BUF_SIZE;
        tfw_min_write_size = TFW_MIN_WRITE_SIZE;
        pthread_create(&writer, NULL, boot_writer, this);
        return true;
    }
}

ThreadedFileWriter::~ThreadedFileWriter()
{
    no_writes = true;

    if (fd >= 0)
    {
        Flush();
        in_dtor = 1; /* tells child thread to exit */
        pthread_join(writer, NULL);
        close(fd);
        fd = -1;
    }

    if (buf)
    {
        delete [] buf;
        buf = NULL;
    }
}

unsigned ThreadedFileWriter::Write(const void *data, unsigned count)
{
    if (count == 0)
        return 0;

    int first = 1;

    while (count > BufFree())
    {
        if (first)
             VERBOSE(VB_IMPORTANT, "IOBOUND - blocking in ThreadedFileWriter::Write()");
        first = 0;
        usleep(5000);
    }

    if (no_writes)
        return 0;

    if ((wpos + count) > tfw_buf_size)
    {
        int first_chunk_size = tfw_buf_size - wpos;
        int second_chunk_size = count - first_chunk_size;
        memcpy(buf + wpos, data, first_chunk_size );
        memcpy(buf, (char *)data + first_chunk_size, second_chunk_size );
    }
    else
    {
        memcpy(buf + wpos, data, count);
    }

    pthread_mutex_lock(&buflock);
    wpos = (wpos + count) % tfw_buf_size;
    pthread_mutex_unlock(&buflock);

    return count;
}

/** \fn ThreadedFileWriter::Seek(long long pos, int whence)
 *  \brief Seek to a position within stream; May be unsafe.
 *
 *   This method is unsafe if Start() has been called and
 *   the call us not preceeded by StopReads(). You probably
 *   want to follow Seek() with a StartReads() in this case.
 *
 *   This method assumes that we don't seek very often. It does
 *   not use a high performance approach... we just block until
 *   the write thread empties the buffer.
 */
long long ThreadedFileWriter::Seek(long long pos, int whence)
{
    Flush();

    return lseek(fd, pos, whence);
}

void ThreadedFileWriter::Flush(void)
{
    flush = true;
    while (BufUsed() > 0)
        usleep(5000);
    flush = false;
}

void ThreadedFileWriter::Sync(void)
{
#ifdef HAVE_FDATASYNC
    fdatasync(fd);
#else
    fsync(fd);
#endif
}

void ThreadedFileWriter::SetWriteBufferSize(int newSize)
{
    if (newSize <= 0)
        return;

    Flush();

    pthread_mutex_lock(&buflock);
    delete [] buf;
    rpos = wpos = 0;
    buf = new char[newSize + 20];
    tfw_buf_size = newSize;
    pthread_mutex_unlock(&buflock);
}

void ThreadedFileWriter::SetWriteBufferMinWriteSize(int newMinSize)
{
    if (newMinSize <= 0)
        return;

    tfw_min_write_size = newMinSize;
}

void ThreadedFileWriter::DiskLoop()
{
    int size;
    int written = 0;
    QTime timer;
    timer.start();

    while (!in_dtor || BufUsed() > 0)
    {
        size = BufUsed();

        if (!size || (!in_dtor && !flush &&
            (((unsigned)size < tfw_min_write_size) && 
             ((unsigned)written >= tfw_min_write_size))))
        {
            usleep(500);
            continue;
        }

        if (timer.elapsed() > 1000)
        {
            Sync();
            timer.restart();
        }

        /* cap the max. write size. Prevents the situation where 90% of the
           buffer is valid, and we try to write all of it at once which
           takes a long time. During this time, the other thread fills up
           the 10% that was free... */
        if (size > TFW_MAX_WRITE_SIZE)
            size = TFW_MAX_WRITE_SIZE;

        if ((rpos + size) > tfw_buf_size)
        {
            int first_chunk_size  = tfw_buf_size - rpos;
            int second_chunk_size = size - first_chunk_size;
            size = safe_write(fd, buf+rpos, first_chunk_size);
            if (size == first_chunk_size)
                size += safe_write(fd, buf, second_chunk_size);
        }
        else
        {
            size = safe_write(fd, buf+rpos, size);
        }
        
        if ((unsigned) written < tfw_min_write_size)
        {
            written += size;
        }
        
        pthread_mutex_lock(&buflock);
        rpos = (rpos + size) % tfw_buf_size;
        pthread_mutex_unlock(&buflock);
    }
}

unsigned ThreadedFileWriter::BufUsed()
{
    unsigned ret;
    pthread_mutex_lock(&buflock);

    if (wpos >= rpos)
        ret = wpos - rpos;
    else
        ret = tfw_buf_size - rpos + wpos;

    pthread_mutex_unlock(&buflock);
    return ret;
}

unsigned ThreadedFileWriter::BufFree()
{
    unsigned ret;
    pthread_mutex_lock(&buflock);

    if (wpos >= rpos)
        ret = rpos + tfw_buf_size - wpos - 1;
    else
        ret = rpos - wpos - 1;

    pthread_mutex_unlock(&buflock);
    return ret;
}

/**********************************************************************/

RingBuffer::RingBuffer(const QString &lfilename, bool write, bool usereadahead)
{
    
    int openAttempts = OPEN_READ_ATTEMPTS;
    Init();
    
    startreadahead = usereadahead;

    normalfile = true;
    filename = (QString)lfilename;

    if (filename.right(4) != ".nuv")
        openAttempts = 1;

    if (write)
    {
        tfw = new ThreadedFileWriter(filename.ascii(),
                                     O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                     0644);
        if (tfw->Open())
            writemode = true;
        else
        {
            delete tfw;
            tfw = NULL;
        }
    }
    else
    {
        bool is_local = false;
        bool is_dvd = false;
        if ((filename.left(7) == "myth://") &&
            (filename.length() > 7 ))
        {
            QString local_pathname = gContext->GetSetting("RecordFilePrefix");
            int hostlen = filename.find(QRegExp("/"), 7);

            if (hostlen != -1)
            {
                local_pathname += filename.right(filename.length() - hostlen);

                QFile checkFile(local_pathname);

                if (checkFile.exists())
                {
                    is_local = true;
                    filename = local_pathname;
                }
            }
        }
        else if (filename.left(4) == "dvd:")
        {
            is_dvd = true;
            dvdPriv = new DVDRingBufferPriv;
            startreadahead = false;
            int pathLen = filename.find(QRegExp("/"), 4);
            if (pathLen != -1)
            {
                QString tempFilename = filename.right(filename.length() -  pathLen);

                QFile checkFile(tempFilename);

                if (!checkFile.exists())
                    filename = "/dev/dvd";
                else
                    filename = tempFilename;
            }

            else
            {
                filename = "/dev/dvd";
            }
            
        }
        else
            is_local = true;

        if (is_local)
        {
            char buf[READ_TEST_SIZE];
            while (openAttempts > 0)
            {
                int ret;
                openAttempts--;
                
                fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
                
                if (fd2 < 0)
                {
                    VERBOSE( VB_IMPORTANT, QString("Could not open %1.  %2 retries remaining.")
                                           .arg(filename).arg(openAttempts));
                    usleep(500000);
                }
                else
                {
                    if ((ret = read(fd2, buf, READ_TEST_SIZE)) != READ_TEST_SIZE)
                    {
                        VERBOSE( VB_IMPORTANT, QString("Invalid file handle when opening %1.  "
                                                       "%2 retries remaining.")
                                                       .arg(filename).arg(openAttempts));
                        close(fd2);
                        fd2 = -1;
                        usleep(500000);
                    }
                    else
                    {
                        lseek(fd2, 0, SEEK_SET);
                        openAttempts = 0;
                    }
                }
            }

            QFileInfo fileInfo(filename);
            if (fileInfo.lastModified().secsTo(QDateTime::currentDateTime()) >
                30 * 60)
            {
                oldfile = true;
            }
        }
        else if (is_dvd)
        {
            dvdPriv->open(filename);    
            readblocksize = DVD_BLOCK_SIZE * 62;
        }
        else
        {
            remotefile = new RemoteFile(filename);
            if (!remotefile->isOpen())
            {
                VERBOSE(VB_IMPORTANT,
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(filename));
                delete remotefile;
                remotefile = NULL;
            }
        }

        writemode = false;
    }

    smudgeamount = 0;
}

RingBuffer::RingBuffer(const QString &lfilename, long long size,
                       long long smudge, RemoteEncoder *enc)
{
    Init();

    if (enc)
    {
        remoteencoder = enc;
        recorder_num = enc->GetRecorderNumber();
    }

    normalfile = false;
    filename = (QString)lfilename;
    filesize = size;

    if (recorder_num == 0)
    {
        tfw = new ThreadedFileWriter(filename.ascii(),
                                     O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE,
                                     0644);
        if (tfw->Open())
            fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
        else
        {
            delete tfw;
            tfw = NULL;
        }
    }
    else
    {
        remotefile = new RemoteFile(filename, recorder_num);
        if (!remotefile->isOpen())
        {
            VERBOSE(VB_IMPORTANT,
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(filename));
            delete remotefile;
            remotefile = NULL;
        }
    }

    wrapcount = 0;
    smudgeamount = smudge;
}

void RingBuffer::Init(void)
{
    oldfile = false; 
    startreadahead = true;
    dvdPriv = NULL;
    
    readaheadrunning = false;
    readaheadpaused = false;
    wantseek = false;
    fill_threshold = -1;
    fill_min = -1;

    readblocksize = 128000;
   
    recorder_num = 0;
    remoteencoder = NULL;
    tfw = NULL;
    remotefile = NULL;
    
    fd2 = -1;

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;

    stopreads = false;
    wanttoread = 0;

    numfailures = 0;
    commserror = false;

    pthread_rwlock_init(&rwlock, NULL);
}

RingBuffer::~RingBuffer(void)
{
    KillReadAheadThread();

    pthread_rwlock_wrlock(&rwlock);
    if (remotefile)
    {
        delete remotefile;
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
}

void RingBuffer::Start(void)
{
    if ((normalfile && writemode) || (!normalfile && !recorder_num))
    {
    }
    else if (!readaheadrunning && startreadahead)
        StartupReadAheadThread();
}

void RingBuffer::Reset(void)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    if (!normalfile)
    {
        if (remotefile)
        {
            remotefile->Reset();
        }
        else
        {
            tfw->Seek(0, SEEK_SET);
            lseek(fd2, 0, SEEK_SET);
        }

        totalwritepos = writepos = 0;
        totalreadpos = readpos = 0;

        wrapcount = 0;

        if (readaheadrunning)
            ResetReadAhead(0);
    }

    numfailures = 0;
    commserror = false;

    pthread_rwlock_unlock(&rwlock);
}


int RingBuffer::safe_read(int fd, void *data, unsigned sz)
{
    int ret;
    unsigned tot = 0;
    unsigned errcnt = 0;
    unsigned zerocnt = 0;

    while (tot < sz)
    {
        ret = read(fd, (char *)data + tot, sz - tot);
        if (ret < 0)
        {
            if (errno == EAGAIN)
                continue;

            VERBOSE(VB_IMPORTANT,
                    QString("ERROR: file I/O problem in 'safe_read()' %1")
                    .arg(strerror(errno)));

            errcnt++;
            numfailures++;
            if (errcnt == 3)
                break;
        }
        else if (ret > 0)
        {
            tot += ret;
        }

        if (ret == 0) // EOF returns 0
        {
            if (tot > 0)
                break;

            zerocnt++;
            if (zerocnt >= ((oldfile) ? 2 : 50)) // 3 second timeout with usleep(60000), or .12 if it's an old, unmodified file.
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

int RingBuffer::safe_read(RemoteFile *rf, void *data, unsigned sz)
{
    int ret = 0;

    ret = rf->Read(data, sz);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, "RemoteFile::Read() failed in RingBuffer::safe_read().");
        rf->Seek(internalreadpos, SEEK_SET);
        ret = 0;
        numfailures++;
     }

    return ret;
}

#define READ_AHEAD_SIZE (10 * 256000)

void RingBuffer::CalcReadAheadThresh(int estbitrate)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    fill_threshold = 0;
    fill_min = 0;

    VERBOSE(VB_PLAYBACK, QString("Estimated bitrate = %1").arg(estbitrate));

    if (remotefile)
        fill_threshold += 256000;

    if (estbitrate > 6000)
        fill_threshold += 256000;

    if (estbitrate > 10000)
        fill_threshold += 256000;

    if (estbitrate > 14000)
        fill_threshold += 256000;

    if (estbitrate > 17000)
        readblocksize = 256000;

    fill_min = 1;

    readsallowed = false;

    if (fill_threshold == 0)
        fill_threshold = -1;
    if (fill_min == 0)
        fill_min = -1;

    pthread_rwlock_unlock(&rwlock);
}

int RingBuffer::ReadBufFree(void)
{
    int ret = 0;

    readAheadLock.lock();

    if (rbwpos >= rbrpos)
        ret = rbrpos + READ_AHEAD_SIZE - rbwpos - 1;
    else
        ret = rbrpos - rbwpos - 1;

    readAheadLock.unlock();
    return ret;
}

int RingBuffer::ReadBufAvail(void)
{
    int ret = 0;

    readAheadLock.lock();

    if (rbwpos >= rbrpos)
        ret = rbwpos - rbrpos;
    else
        ret = READ_AHEAD_SIZE - rbrpos + rbwpos;
    readAheadLock.unlock();
    return ret;
}

// must call with rwlock in write lock
void RingBuffer::ResetReadAhead(long long newinternal)
{
    readAheadLock.lock();
    rbrpos = 0;
    rbwpos = 0;
    internalreadpos = newinternal;
    ateof = false;
    readsallowed = false;
    readAheadLock.unlock();
}

void RingBuffer::StartupReadAheadThread(void)
{
    readaheadrunning = false;

    pthread_create(&reader, NULL, startReader, this);

    while (!readaheadrunning)
        usleep(50);
}

void RingBuffer::KillReadAheadThread(void)
{
    if (!readaheadrunning)
        return;

    readaheadrunning = false;
    pthread_join(reader, NULL);
}

void RingBuffer::StopReads(void)
{
    stopreads = true;
    availWait.wakeAll();
}

void RingBuffer::StartReads(void)
{
    stopreads = false;
}

void RingBuffer::Pause(void)
{
    pausereadthread = true;
    StopReads();
}

void RingBuffer::Unpause(void)
{
    StartReads();
    pausereadthread = false;
}

bool RingBuffer::isPaused(void)
{
    if (!readaheadrunning)
        return true;

    return readaheadpaused;
}

void RingBuffer::WaitForPause(void)
{
    if (!readaheadrunning)
        return;

    if  (!readaheadpaused)
    {
        while (!pauseWait.wait(1000))
            VERBOSE(VB_IMPORTANT, "Waited too long for ringbuffer pause..");
    }
}

void *RingBuffer::startReader(void *type)
{
    RingBuffer *rbuffer = (RingBuffer *)type;
    rbuffer->ReadAheadThread();
    return NULL;
}

void RingBuffer::ReadAheadThread(void)
{
    long long totfree = 0;
    int ret = -1;
    int used = 0;
    int loops = 0;

    pausereadthread = false;

    readAheadBuffer = new char[READ_AHEAD_SIZE + 256000];

    ResetReadAhead(0);
    totfree = ReadBufFree();

    readaheadrunning = true;
    while (readaheadrunning)
    {
        if (pausereadthread)
        {
            readaheadpaused = true;
            pauseWait.wakeAll();
            usleep(5000);
            totfree = ReadBufFree();
            continue;
        }

        readaheadpaused = false;

        if (totfree < readblocksize)
        {
            usleep(50000);
            totfree = ReadBufFree();
            ++loops;
            // break out if we've spent lots of time here, just in case things
            // are waiting on a wait condition that never got triggered.
            if (readsallowed && loops < 10)
                continue;
        }
        loops = 0;

        pthread_rwlock_rdlock(&rwlock);
        if (totfree > readblocksize && !commserror)
        {
            // limit the read size
            totfree = readblocksize;

            if (rbwpos + totfree > READ_AHEAD_SIZE)
                totfree = READ_AHEAD_SIZE - rbwpos;

            if (normalfile)
            {
                if (!writemode)
                {
                    if (remotefile)
                    {
                        ret = safe_read(remotefile, readAheadBuffer + rbwpos,
                                        totfree);
                        internalreadpos += ret;
                    }
                    else if (dvdPriv)
                    {                        
                        ret = dvdPriv->safe_read(readAheadBuffer + rbwpos, totfree);
                        internalreadpos += ret;
                    }
                    else
                    {
                        ret = safe_read(fd2, readAheadBuffer + rbwpos, totfree);
                        internalreadpos += ret;
                    }
                }
            }
            else
            {
                if (remotefile)
                {
                    ret = safe_read(remotefile, readAheadBuffer + rbwpos,
                                    totfree);

                    if (internalreadpos + totfree > filesize)
                    {
                        int toread = filesize - readpos;
                        int left = totfree - toread;

                        internalreadpos = left;
                    }
                    else
                        internalreadpos += ret;
                }
                else if (internalreadpos + totfree > filesize)
                {
                    int toread = filesize - internalreadpos;

                    ret = safe_read(fd2, readAheadBuffer + rbwpos, toread);

                    int left = totfree - toread;
                    lseek(fd2, 0, SEEK_SET);

                    ret = safe_read(fd2, readAheadBuffer + rbwpos + toread,
                                    left);
                    ret += toread;

                    internalreadpos = left;
                }
                else if (dvdPriv)
                {                        
                        ret = dvdPriv->safe_read(readAheadBuffer + rbwpos, totfree);
                        internalreadpos += ret;
                }
                else
                {
                    ret = safe_read(fd2, readAheadBuffer + rbwpos, totfree);
                    internalreadpos += ret;
                }
            }

            readAheadLock.lock();
            if (ret > 0 )
                rbwpos = (rbwpos + ret) % READ_AHEAD_SIZE;
            readAheadLock.unlock();

            if (ret == 0 && normalfile && !stopreads)
            {
                ateof = true;
            }
        }

        if (numfailures > 5)
            commserror = true;

        totfree = ReadBufFree();
        used = READ_AHEAD_SIZE - totfree;

        if (ateof || commserror)
        {
            readsallowed = true;
            totfree = 0;
        }

        if (!readsallowed && used >= fill_min)
            readsallowed = true;

        if (readsallowed && used < fill_min && !ateof)
        {
            readsallowed = false;
            VERBOSE(VB_GENERAL, QString ("rebuffering (%1 %2)").arg(used)
                                                               .arg(fill_min));
        }

        if (readsallowed || stopreads)
            readsAllowedWait.wakeAll();

        availWaitMutex.lock();
        if (commserror || ateof || stopreads ||
            (wanttoread <= used && wanttoread > 0))
        {
            availWait.wakeAll();
        }
        availWaitMutex.unlock();

        pthread_rwlock_unlock(&rwlock);

        if ((used >= fill_threshold || wantseek) && !pausereadthread)
            usleep(500);
    }

    delete [] readAheadBuffer;
    readAheadBuffer = NULL;
    rbrpos = 0;
    rbwpos = 0;
}

int RingBuffer::ReadFromBuf(void *buf, int count)
{
    if (commserror)
        return 0;

    bool readone = false;
    int readErr = 0;
    
    if (readaheadpaused && stopreads)
    {
        readone = true;
        Unpause();
    }
    else
    {
        while (!readsallowed && !stopreads)
        {
            if (!readsAllowedWait.wait(1000))
            {
                 VERBOSE(VB_IMPORTANT, "taking too long to be allowed to read..");
                 readErr++;
                 
                 // HACK Sometimes the readhead thread gets borked on startup.
               /*  if ((readErr % 2) && (rbrpos ==0))
                 {
                    VERBOSE(VB_IMPORTANT, "restarting readhead thread..");
                    KillReadAheadThread();
                    StartupReadAheadThread();
                 }                    
                   */ 
                 if (readErr > 10)
                 {
                     VERBOSE(VB_IMPORTANT, "Took more than 10 seconds to be allowed to read, "
                                           "aborting.");
                     wanttoread = 0;
                     stopreads = true;
                     return 0;
                 }
            }
        }
    }
    
    int avail = ReadBufAvail();
    readErr = 0;
    
    while (avail < count && !stopreads)
    {
        availWaitMutex.lock();
        wanttoread = count;
        if (!availWait.wait(&availWaitMutex, 4000))
        {
            VERBOSE(VB_IMPORTANT,"Waited 4 seconds for data to become available, waiting "
                    "again...");
            readErr++;
            if (readErr > 7)
            {
                VERBOSE(VB_IMPORTANT,"Waited 14 seconds for data to become available, "
                        "aborting");
                wanttoread = 0;
                stopreads = true;
                availWaitMutex.unlock();
                return 0;
            }
        }

        wanttoread = 0;
        availWaitMutex.unlock();

        avail = ReadBufAvail();
        if (ateof && avail < count)
            count = avail;

        if (commserror)
            return 0;
    }

    if ((ateof || stopreads) && avail < count)
        count = avail;

    if (rbrpos + count > READ_AHEAD_SIZE)
    {
        int firstsize = READ_AHEAD_SIZE - rbrpos;
        int secondsize = count - firstsize;

        memcpy(buf, readAheadBuffer + rbrpos, firstsize);
        memcpy((char *)buf + firstsize, readAheadBuffer, secondsize);
    }
    else
        memcpy(buf, readAheadBuffer + rbrpos, count);

    readAheadLock.lock();
    rbrpos = (rbrpos + count) % READ_AHEAD_SIZE;
    readAheadLock.unlock();

    if (readone)
    {
        Pause();
        WaitForPause();
    }

    return count;
}

int RingBuffer::Read(void *buf, int count)
{
    pthread_rwlock_rdlock(&rwlock);

    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            if (!readaheadrunning)
            {
                if (remotefile)
                {
                    ret = safe_read(remotefile, buf, count);
                    totalreadpos += ret;
                    readpos += ret;
                }
                else if (dvdPriv)
                {                        
                    ret = dvdPriv->safe_read(buf, count);
                    readpos += ret;
                }
                else
                {
                    ret = safe_read(fd2, buf, count);
                    totalreadpos += ret;
                    readpos += ret;
                }
            }
            else
            {
                ret = ReadFromBuf(buf, count);
                totalreadpos += ret;
                readpos += ret;
            }
        }
        else
        {
            fprintf(stderr, "Attempt to read from a write only file\n");
        }
    }
    else
    {
//cout << "reading: " << totalreadpos << " " << readpos << " " << count << " " << filesize << endl;
        if (remotefile)
        {
            ret = ReadFromBuf(buf, count);

            if (readpos + ret > filesize)
            {
                int toread = filesize - readpos;
                int left = count - toread;

                readpos = left;
            }
            else
                readpos += ret;
            totalreadpos += ret;
        }
        else
        {
            if (stopreads)
            {
                pthread_rwlock_unlock(&rwlock);
                return 0;
            }

            availWaitMutex.lock();
            wanttoread = totalreadpos + count;

            while (totalreadpos + count > totalwritepos - tfw->BufUsed())
            {
                if (!availWait.wait(&availWaitMutex, 15000))
                {
                    VERBOSE(VB_IMPORTANT,"Couldn't read data from the capture card in 15 "
                            "seconds. Stopping.");
                    StopReads();
                }

                if (stopreads)
                {
                    availWaitMutex.unlock();
                    pthread_rwlock_unlock(&rwlock);
                    return 0;
                }
            }

            availWaitMutex.unlock();

            if (readpos + count > filesize)
            {
                int toread = filesize - readpos;

                ret = safe_read(fd2, buf, toread);

                int left = count - toread;
                lseek(fd2, 0, SEEK_SET);

                ret = safe_read(fd2, (char *)buf + toread, left);
                ret += toread;

                totalreadpos += ret;
                readpos = left;
            }
            else
            {
                ret = safe_read(fd2, buf, count);
                readpos += ret;
                totalreadpos += ret;
            }
        }
    }

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

bool RingBuffer::IsIOBound(void)
{
    bool ret = false;
    int used, free;
    pthread_rwlock_rdlock(&rwlock);

    if (!tfw)
    {
        pthread_rwlock_unlock(&rwlock);
        return ret;
    }

    used = tfw->BufUsed();
    free = tfw->BufFree();

    ret = (used * 5 > free);

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

int RingBuffer::Write(const void *buf, int count)
{
    int ret = -1;

    pthread_rwlock_rdlock(&rwlock);

    if (!tfw)
    {
        pthread_rwlock_unlock(&rwlock);
        return ret;
    }

    if (normalfile)
    {
        if (writemode)
        {
            ret = tfw->Write(buf, count);
            totalwritepos += ret;
            writepos += ret;
        }
        else
        {
            fprintf(stderr, "Attempt to write to a read only file\n");
        }
    }
    else
    {
//cout << "write: " << totalwritepos << " " << writepos << " " << count << " " << filesize << endl;
        if (writepos + count > filesize)
        {
            int towrite = filesize - writepos;
            ret = tfw->Write(buf, towrite);

            int left = count - towrite;
            tfw->Seek(0, SEEK_SET);

            ret = tfw->Write((char *)buf + towrite, left);
            writepos = left;

            ret += towrite;

            totalwritepos += ret;
            wrapcount++;
        }
        else
        {
            ret = tfw->Write(buf, count);
            writepos += ret;
            totalwritepos += ret;
        }

        availWaitMutex.lock();
        if (totalwritepos >= wanttoread)
            availWait.wakeAll();
        availWaitMutex.unlock();
    }

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

void RingBuffer::Sync(void)
{
    tfw->Sync();
}

long long RingBuffer::GetFileWritePosition(void)
{
    return totalwritepos;
}

long long RingBuffer::Seek(long long pos, int whence)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    long long ret = -1;
    if (normalfile)
    {
    
        if (remotefile)
            ret = remotefile->Seek(pos, whence, readpos);
        else if(dvdPriv)
        {
            dvdPriv->Seek(pos, whence);
        }
        else
        {
            if (whence == SEEK_SET)
                ret = lseek(fd2, pos, whence);
            else
            {
                long long realseek = readpos + pos;
                ret = lseek(fd2, realseek, SEEK_SET);
            }
        }

        if (whence == SEEK_SET)
            readpos = ret;
        else if (whence == SEEK_CUR)
            readpos += pos;
        totalreadpos = readpos;
    }
    else
    {
        if (remotefile)
            ret = remotefile->Seek(pos, whence, totalreadpos);
        else
        {
            if (whence == SEEK_SET)
            {
                long long tmpPos = pos;
                long long tmpRet;

                if (tmpPos > filesize)
                    tmpPos = tmpPos % filesize;

                tmpRet = lseek(fd2, tmpPos, whence);
                if (tmpRet == tmpPos)
                    ret = pos;
                else
                    ret = tmpRet;
            }
            else if (whence == SEEK_CUR)
            {
                long long realseek = totalreadpos + pos;
                while (realseek > filesize)
                    realseek -= filesize;

                ret = lseek(fd2, realseek, SEEK_SET);
            }
        }

        if (whence == SEEK_SET)
        {
            totalreadpos = pos; // only used for file open
            readpos = ret;
        }
        else if (whence == SEEK_CUR)
        {
            readpos += pos;
            totalreadpos += pos;
        }

        while (readpos > filesize && filesize > 0)
            readpos -= filesize;

        while (readpos < 0 && filesize > 0)
            readpos += filesize;
    }

    if (readaheadrunning)
        ResetReadAhead(readpos);

    pthread_rwlock_unlock(&rwlock);

    return ret;
}

long long RingBuffer::WriterSeek(long long pos, int whence)
{
    long long ret = -1;

    if (tfw)
    {
        ret = tfw->Seek(pos, whence);
        totalwritepos = ret;
    }

    return ret;
}

void RingBuffer::SetWriteBufferSize(int newSize)
{
    tfw->SetWriteBufferSize(newSize);
}

void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    tfw->SetWriteBufferMinWriteSize(newMinSize);
}

long long RingBuffer::GetFreeSpace(void)
{
    if (!normalfile)
    {
        if (remoteencoder)
            return remoteencoder->GetFreeSpace(totalreadpos);
        return totalreadpos + filesize - totalwritepos - smudgeamount;
    }
    else
        return -1;
}

long long RingBuffer::GetFreeSpaceWithReadChange(long long readchange)
{
    if (!normalfile)
    {
        if (readchange > 0)
            readchange = 0 - (filesize - readchange);

        return GetFreeSpace() + readchange;
    }
    else
    {
        return readpos + readchange;
    }
}

long long RingBuffer::GetReadPosition(void)
{
    if (dvdPriv)
    {
        return dvdPriv->GetReadPosition();
    }
    else
    {
        return readpos;
    }
}

long long RingBuffer::GetTotalReadPosition(void)
{
    if (dvdPriv)
    {
        return dvdPriv->GetTotalReadPosition();
    }
    else
    {
        return totalreadpos;
    }
}

long long RingBuffer::GetWritePosition(void)
{
    return writepos;
}

long long RingBuffer::GetTotalWritePosition(void)
{
    return totalwritepos;
}

long long RingBuffer::GetRealFileSize(void)
{
    if (remotefile)
    {
        if (normalfile)
            return remotefile->GetFileSize();
        else
            return -1;
    }

    struct stat st;
    if (stat(filename.ascii(), &st) == 0)
        return st.st_size;
    return -1;
}

void RingBuffer::getPartAndTitle(int& title, int& part)
{
    if (dvdPriv)
        dvdPriv->getPartAndTitle(title, part);
}


void RingBuffer::getDescForPos(QString& desc)
{
    if (dvdPriv)
        dvdPriv->getDescForPos(desc);
}


void RingBuffer::nextTrack()
{
   if (dvdPriv)
        dvdPriv->nextTrack();
}

void RingBuffer::prevTrack()
{
   if (dvdPriv)
        dvdPriv->prevTrack();
}
