#include <string>

#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "RTjpegN.h"
#include "format.h"

using namespace std;

#define MAXVBUFFER 20

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer();
   ~NuppelVideoPlayer();

    void SetAudioDevice(char *device) { audiodevice = device; }
    void SetFileName(char *lfilename) { filename = lfilename; }

    void StartPlaying(void);

 private:
    void InitSound(void);
    void WriteAudio(unsigned char *aubuf, int size);

    int InitSubs(void);
    
    int OpenFile(void);
    int CloseFile(void);

    unsigned char *DecodeFrame(struct rtframeheader *frameheader,
                               unsigned char *strm);
    unsigned char *GetFrame(int *timecode, int onlyvideo, 
                            unsigned char **audiodata, int *alen);
    
    int deinterlace;
    
    int audiofd;

    string filename;
    string audiodevice;
    
    /* rtjpeg_plugin stuff */
    int file;
    int eof;
    int video_width;
    int video_height;
    double video_frame_rate;
    unsigned char *buf;
    unsigned char *buf2;
    char lastct;
    int effdsp;
    int filesize;
    int startpos;
    int audiodelay;

    int lastaudiolen;
    unsigned char *strm;
    struct rtframeheader frameheader;
    int wpos;
    int rpos;
    int bufstat[MAXVBUFFER];
    int timecodes[MAXVBUFFER];
    unsigned char *vbuffer[MAXVBUFFER];
    unsigned char audiobuffer[512000];
    unsigned char tmpaudio[512000];
    int audiolen;
    int fafterseek;
    int audiotimecode;

    struct rtfileheader fileheader;
    lame_global_flags *gf;

    RTjpeg *rtjd;
    uint8_t *planes[3];

    int paused;
};
