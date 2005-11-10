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
#include "ThreadedFileWriter.h"
#include "livetvchain.h"

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define PNG_MIN_SIZE   20 /* header plus one empty chunk */
#define NUV_MIN_SIZE  204 /* header size? */
#define MPEG_MIN_SIZE 376 /* 2 TS packets */
#define READ_TEST_SIZE 20 /* should be minimum of the above file sizes */

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
        
        ~DVDRingBufferPriv()
        {
            close();
        }
        
        void close()
        {
            if (dvdnav)
            {
                dvdnav_close(dvdnav);
                dvdnav = NULL;
            }            
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
                if ((part <= DVD_MENU_MAX) && dvdnav_menu_table[part] )
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
                
                if ( dvdRet == DVDNAV_STATUS_ERR)
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
                if (dvdStat == DVDNAV_STATUS_ERR) 
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

bool RingBuffer::IsOpen(void) 
{ 
    return tfw || (fd2 > -1) || remotefile; 
}

#endif // DVDNAV

/**********************************************************************/

RingBuffer::RingBuffer(const QString &lfilename, bool write, bool usereadahead)
{
    Init();
    
    startreadahead = usereadahead;
    filename = (QString)lfilename;

    if (write)
    {
        tfw = new ThreadedFileWriter(filename,
                                     O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                     0644);
        if (tfw->Open())
            writemode = true;
        else
        {
            delete tfw;
            tfw = NULL;
        }
        writemode = true;
        return;
    }

    OpenFile(filename);
}

void RingBuffer::OpenFile(const QString &lfilename)
{
    int openAttempts = OPEN_READ_ATTEMPTS;

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
}

void RingBuffer::Init(void)
{
    tfw = NULL;

    writemode = false;
    livetvchain = NULL;
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
    remotefile = NULL;
    
    fd2 = -1;

    writepos = readpos = 0;

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
    
    if (dvdPriv)
    {
        delete dvdPriv;
    }
    
}

void RingBuffer::Start(void)
{
    if (writemode)
    {
    }
    else if (!readaheadrunning && startreadahead)
        StartupReadAheadThread();
}

void RingBuffer::Reset(bool full)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;
    numfailures = 0;
    commserror = false;

    writepos = 0;
    readpos = 0;

    if (full)
        ResetReadAhead(0);

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
            if (zerocnt >= ((oldfile) ? 2 : (livetvchain) ? 6 : 50)) // 3 second timeout with usleep(60000), or .12 if it's an old, unmodified file.
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

    if (estbitrate > 12000)
        fill_threshold += 256000;

    if (estbitrate > 12000)
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

int RingBuffer::DataInReadAhead(void)
{
    return ReadBufAvail();
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
        if (pausereadthread || writemode)
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

            readAheadLock.lock();
            if (ret > 0 )
                rbwpos = (rbwpos + ret) % READ_AHEAD_SIZE;
            readAheadLock.unlock();

            if (ret == 0 && !stopreads)
            {
                if (livetvchain)
                {
                    if (livetvchain->HasNext())
                    {
                        livetvchain->SwitchToNext(true);
                    }
                }
                else
                {
                    ateof = true;
                }
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
    int ret = -1;
    if (writemode)
    {
        fprintf(stderr, "Attempt to read from a write only file\n");
        return ret;
    }

    pthread_rwlock_rdlock(&rwlock);

    if (!readaheadrunning)
    {
        if (remotefile)
        {
            ret = safe_read(remotefile, buf, count);
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
            readpos += ret;
        }
    }
    else
    {
        ret = ReadFromBuf(buf, count);
        readpos += ret;
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
    if (!writemode)
    {
        fprintf(stderr, "Attempt to write to a read only file\n");
        return ret;
    }

    if (!tfw)
        return ret;

    pthread_rwlock_rdlock(&rwlock);

    ret = tfw->Write(buf, count);
    writepos += ret;

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

void RingBuffer::Sync(void)
{
    if (tfw)
        tfw->Sync();
}

long long RingBuffer::Seek(long long pos, int whence)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    long long ret = -1;
    if (remotefile)
        ret = remotefile->Seek(pos, whence, readpos);
    else if (dvdPriv)
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
        writepos = ret;
    }

    return ret;
}

void RingBuffer::WriterFlush(void)
{
    if (tfw)
    {
        tfw->Flush();
        tfw->Sync();
    }
}

void RingBuffer::SetWriteBufferSize(int newSize)
{
    tfw->SetWriteBufferSize(newSize);
}

void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    tfw->SetWriteBufferMinWriteSize(newMinSize);
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

long long RingBuffer::GetWritePosition(void)
{
    return writepos;
}

long long RingBuffer::GetRealFileSize(void)
{
    if (remotefile)
        return remotefile->GetFileSize();

    struct stat st;
    if (stat(filename.ascii(), &st) == 0)
        return st.st_size;
    return -1;
}

bool RingBuffer::LiveMode(void)
{
    return (livetvchain);
}

void RingBuffer::SetLiveMode(LiveTVChain *chain)
{
    livetvchain = chain;
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

