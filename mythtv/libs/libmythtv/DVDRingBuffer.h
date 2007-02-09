// -*- Mode: c++ -*-
#ifndef DVD_RING_BUFFER_H_
#define DVD_RING_BUFFER_H_

#define DVD_BLOCK_SIZE 2048LL
#define DVD_MENU_MAX 7

#include <qstring.h>
#include <qobject.h>
#include <qmutex.h>
#include "util.h"
extern "C" {
#include "../libavcodec/avcodec.h"
}

#define DVDNAV_COMPILE
#include "../libmythdvdnav/dvdnav.h"

/** \class DVDRingBufferPriv
 *  \brief RingBuffer class for DVD's
 *
 *   A spiffy little class to allow a RingBuffer to read from DVDs.
 */

class NuppelVideoPlayer;

class MPUBLIC DVDRingBufferPriv
{
  public:
    DVDRingBufferPriv();
    virtual ~DVDRingBufferPriv();

    // gets
    int  GetTitle(void) const { return title;        }
    int  GetPart(void)  const { return part;         }
    bool IsInMenu(void) const; 
    bool IsOpen(void)   const { return dvdnav;       }
    long long GetReadPosition(void);
    long long GetTotalReadPosition(void) { return titleLength; }
    void GetDescForPos(QString &desc) const;
    void GetPartAndTitle(int &_part, int &_title) const
        { _part  = part; _title = title; }
    uint GetTotalTimeOfTitle(void);
    uint GetChapterLength(void) { return pgLength / 90000; }
    uint GetCellStart(void);
    bool InStillFrame(void) { return cellHasStillFrame; }
    bool IsWaiting(void) { return dvdWaiting; }
    int  NumPartsInTitle(void) { return titleParts; }
    void GetMenuSPUPkt(uint8_t *buf, int len, int stream_id);

    AVSubtitleRect *GetMenuButton(void);
    void ReleaseMenuButton(void);

    bool IgnoringStillorWait(void) { return skipstillorwait; }
    uint ButtonPosX(void)   { return hl_startx; }
    uint ButtonPosY(void)   { return hl_starty; }
    uint ButtonWidth(void)  { return hl_width; }
    uint ButtonHeight(void) { return hl_height; }
    uint GetAudioLanguage(int id);
    uint GetSubtitleLanguage(int id);
    void SetMenuPktPts(long long pts) { menupktpts = pts; }
    long long GetMenuPktPts(void) { return menupktpts; }
    bool DecodeSubtitles(AVSubtitle * sub, int * gotSubtitles, 
                         const uint8_t * buf, int buf_size);
    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);
    bool JumpToTitle(void) { return jumptotitle; }
    double GetFrameRate(void);
    bool StartOfTitle(void) { return (part == 0); }
    bool EndOfTitle(void)   { return    ((!titleParts) || 
                                        (part == (titleParts - 1)) ||
                                        (titleParts == 1)); }
    int GetCellID(void) { return cellid; }
    int GetVobID(void)  { return vobid; }
    bool IsSameChapter(int tmpcellid, int tmpvobid);
    
    // commands
    bool OpenFile(const QString &filename);
    void PlayTitleAndPart(int _title, int _part) 
        { dvdnav_part_play(dvdnav, _title, _part); }
    void CloseDVD(void);
    bool nextTrack(void);
    void prevTrack(void);
    int  safe_read(void *data, unsigned sz);
    long long NormalSeek(long long time);
    void SkipStillFrame(void);
    void WaitSkip(void);
    bool GoToMenu(const QString str);
    void GoToNextProgram(void);
    void GoToPreviousProgram(void);
    void MoveButtonLeft(void);
    void MoveButtonRight(void);
    void MoveButtonUp(void);
    void MoveButtonDown(void);
    void ActivateButton(void);
    int NumMenuButtons(void) const;
    void IgnoreStillOrWait(bool skip) { skipstillorwait = skip; }
    uint GetCurrentTime(void) { return (currentTime / 90000); }
    void  SetTrack(uint type, int trackNo);
    int   GetTrack(uint type);
    uint8_t GetNumAudioChannels(int id);
    void JumpToTitle(bool change) { jumptotitle = change; }
    void SeekCellStart(void);
    void SetDVDSpeed(const char *device, int speed);

    void SetParent(NuppelVideoPlayer *p) { parent = p; }


    
  protected:
    dvdnav_t      *dvdnav;
    unsigned char  dvdBlockWriteBuf[DVD_BLOCK_SIZE];
    unsigned char *dvdBlockReadBuf;
    const char *dvdFilename;
    int            dvdBlockRPos;
    int            dvdBlockWPos;
    long long      pgLength;
    long long      pgcLength;
    long long      cellStart;
    long long      pgStart;
    long long      currentpos;
    dvdnav_t      *lastNav; // This really belongs in the player.
    int32_t        part;
    int32_t        title;
    int32_t        titleParts;
    bool           gotStop;

    bool           cellHasStillFrame;
    bool           dvdWaiting;
    long long      titleLength;
    MythTimer      stillFrameTimer;
    uint32_t       clut[16];
    uint8_t        button_color[4];
    uint8_t        button_alpha[4];
    uint16_t       hl_startx;
    uint16_t       hl_width;
    uint16_t       hl_starty;
    uint16_t       hl_height;
    uint8_t       *menuSpuPkt;
    int            menuBuflength;
    uint8_t       *buttonBitmap;
    AVSubtitle     dvdMenuButton;
    bool           skipstillorwait;
    long long      cellstartPos;
    bool           buttonSelected;
    bool           buttonExists;
    int            cellid;
    int            lastcellid;
    int            vobid;
    int            lastvobid;
    bool           cellRepeated;
    int            buttonstreamid;
    bool           gotoCellStart;
    /// this pts is of the first video frame decoded just after reading a menu pkt. 
    /// menu pkt pts is not reliable
    long long      menupktpts;
    int            curAudioTrack;
    int8_t         curSubtitleTrack;
    bool           autoselectaudio;
    bool           autoselectsubtitle;
    bool           jumptotitle;
    long long      seekpos;
    int            seekwhence;
    QString        dvdname;
    QString        serialnumber;
    bool           seeking;
    uint64_t       seekTime;
    uint           currentTime;

    NuppelVideoPlayer *parent;

    QMutex menuBtnLock;
    QMutex seekLock;

    long long Seek(long long time);
    bool DrawMenuButton(uint8_t *spu_pkt, int buf_size);
    bool DVDButtonUpdate(bool b_mode);
    void ClearMenuSPUParameters(void);
    void ClearMenuButton(void);
    bool MenuButtonChanged(void);
    uint ConvertLangCode(uint16_t code); /// converts 2char key to 3char key
    void SelectDefaultButton(void);
    void ClearSubtitlesOSD(void);
    
    int get_nibble(const uint8_t *buf, int nibble_offset);
    int decode_rle(uint8_t *bitmap, int linesize, int w, int h,
                    const uint8_t *buf, int nibble_offset, int buf_size);
    void guess_palette(uint32_t *rgba_palette,uint8_t *palette,
                        uint8_t *alpha);
};

#endif // DVD_RING_BUFFER_H_

