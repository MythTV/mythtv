// -*- Mode: c++ -*-
#ifndef DVD_RING_BUFFER_H_
#define DVD_RING_BUFFER_H_

#define DVD_BLOCK_SIZE 2048LL
#define DVD_MENU_MAX 7

#include <qstring.h>
#include <qobject.h>

#ifdef HAVE_DVDNAV
#   include <dvdnav/dvdnav.h>
#endif // HAVE_DVDNAV

/** \class DVDRingBufferPriv
 *  \brief RingBuffer class for DVD's
 *
 *   A spiffy little class to allow a RingBuffer to read from DVDs.
 *   This should really be a specialized version of the RingBuffer, but
 *   until it's all done and tested I'm not real sure about what stuff
 *   needs to be in a RingBufferBase class.
 */

#ifndef HAVE_DVDNAV

// Stub version to cut down on the ifdefs.
class DVDRingBufferPriv
{
  public:
    DVDRingBufferPriv()                       {               }
    bool OpenFile(const QString& )            { return false; }
    bool IsOpen()                       const { return false; }
    long long Seek(long long, int)            { return  0;    }
    int safe_read(void *, unsigned)           { return -1;    }
    long long GetReadPosition()         const { return  0;    }
    long long GetTotalReadPosition()    const { return  0;    }
    void GetPartAndTitle(int &, int &)  const {               }
    bool nextTrack()                          {               }
    void prevTrack()                          {               }
    void GetDescForPos(QString&)              {               }
    uint GetTotalTimeOfTitle(void)            {               }
    uint GetCellStart(void)                   {               }
};

#else // if HAVE_DVDNAV

class DVDRingBufferPriv
{
  public:
    DVDRingBufferPriv();
    virtual ~DVDRingBufferPriv();

    // gets
    int  GetTitle(void) const { return title;        }
    int  GetPart(void)  const { return part;         }
    bool IsInMenu(void) const { return (title == 0); }
    bool IsOpen(void)   const { return dvdnav;       }
    long long GetReadPosition(void);
    long long GetTotalReadPosition(void);
    void GetDescForPos(QString &desc) const;
    void GetPartAndTitle(int &_part, int &_title) const
        { _part  = part; _title = _title; }
    uint GetTotalTimeOfTitle(void);
    uint GetCellStart(void);

    // commands
    bool OpenFile(const QString &filename);
    void close(void);
    bool nextTrack(void);
    void prevTrack(void);
    int  safe_read(void *data, unsigned sz);
    long long Seek(long long pos, int whence);
        
  protected:
    dvdnav_t      *dvdnav;
    unsigned char  dvdBlockWriteBuf[DVD_BLOCK_SIZE];
    unsigned char *dvdBlockReadBuf;
    int            dvdBlockRPos;
    int            dvdBlockWPos;
    long long      pgLength;
    long long      pgcLength;
    long long      cellStart;
    long long      pgStart;
    dvdnav_t      *lastNav; // This really belongs in the player.
    int            part;
    int            title;
    int            maxPart;
    int            mainTitle;
};
#endif // HAVE_DVDNAV
#endif // DVD_RING_BUFFER_H_
