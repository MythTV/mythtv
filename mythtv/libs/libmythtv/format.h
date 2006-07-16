/* format.h  rh */

#ifndef FORMAT_H
#define FORMAT_H

#ifdef __GNUC__
#define MYTH_PACKED __attribute__((packed))
#else
#define MYTH_PACKED
#endif

typedef struct rtfileheader
{
  char finfo[12];     // "NuppelVideo" + \0
  char version[5];    // "0.05" + \0
  int  width;
  int  height;
  int  desiredwidth;  // 0 .. as it is
  int  desiredheight; // 0 .. as it is
  char pimode;        // P .. progressive
		      // I .. interlaced  (2 half pics) [NI]
  double aspect;      // 1.0 .. square pixel (1.5 .. e.g. width=480: width*1.5=720
                      // for capturing for svcd material
  double fps;
  int videoblocks;   // count of video-blocks -1 .. unknown   0 .. no video
  int audioblocks;   // count of audio-blocks -1 .. unknown   0 .. no audio
  int textsblocks;   // count of text-blocks  -1 .. unknown   0 .. no text
  int keyframedist;
} rtfileheader;

typedef struct rtframeheader
{
   char frametype;	// A .. Audio, V .. Video, S .. Sync, T .. Text
   			// R .. Seekpoint: String RTjjjjjjjj (use full packet)
			// D .. Addition Data for Compressors
   			//      ct: R .. RTjpeg Tables, F .. FFMpeg extradata
                        // X .. eXtended data, Q .. SeekTable
                        // K .. KFA table
   
   char comptype;	// V: 0 .. raw YUV420
			//    1 .. RTJpeg
			//    2 .. RTJpeg with lzo afterwards
			//    3 .. raw YUV420 with lzo afterwards
                        //    4 .. avcodec (fourcc in the extendeddata)
			//    N .. black frame
			//    L .. simply copy last frame (if lost frames)
    			// A: 0 .. Uncompressed (44100/sec 16bit 2ch)
    			//    1 .. lzo compression [NI]
    			//    2 .. layer2 (packet) [NI]
    			//    3 .. layer3 (packet)
    			//    F .. flac (lossless) [NI]
    			//    S .. shorten (lossless) [NI]
			//    N .. null frame loudless
			//    L .. simply copy last frame (may sound bad) NI
			// S: B .. Audio and Video sync point [NI]
                        //    A .. Audio Sync Information
		        //         timecode == effective dsp-frequency*100
			//         when reaching this audio sync point
			//         because many cheap soundcards are unexact 
			//         and have a range from 44000 to 44250
			//         instead of the expected exact 44100 S./sec
			//    V .. Next Video Sync 
			//         timecode == next video framenumber
			//    S .. Audio,Video,Text Correlation [NI]
			//    M .. New Header - format changed (only aspect now)
			// T: C .. Closed Caption (US)
			//    T .. Teletext Subtitles (Europe)
   char keyframe;	//    0 .. keyframe
			//    1 .. nr of frame in gop => no keyframe

   char filters;	//    Every bit stands for one type of filter
			//    1 .. Gauss 5 Pixel (8*m+2*l+2*r+2*a+2*b)/16 [NYI]
			//    2 .. Gauss 5 Pixel (8*m+1*l+1*r+1*a+1*b)/12 [NYI]
			//    4 .. Cartoon Filter   [NI]
			//    8 .. Reserverd Filter [NI]
			//   16 .. Reserverd Filter [NI]
			//   32 .. Reserverd Filter [NI]
			//   64 .. Reserverd Filter [NI]
			//  128 .. Reserverd Filter [NI]

   int  timecode;	// Timecodeinformation sec*1000 + msecs

   int  packetlength;   // V,A,T: length of following data in stream
   			// S:     length of packet correl. information [NI]
   			// R:     do not use here! (fixed 'RTjjjjjjjjjjjjjj')
} MYTH_PACKED rtframeheader;

// The fourcc's here are for the most part taken from libavcodec.
// As to their correctness, I have no idea.  The audio ones are surely wrong,
// but I suppose it doesn't really matter as long as I'm consistant.

typedef struct extendeddata
{
   int version;            // yes, this is repeated from the file header
   int video_fourcc;       // video encoding method used 
   int audio_fourcc;       // audio encoding method used
   // generic data
   int audio_sample_rate;
   int audio_bits_per_sample;
   int audio_channels;
   // codec specific
   // mp3lame
   int audio_compression_ratio;
   int audio_quality;
   // rtjpeg
   int rtjpeg_quality;
   int rtjpeg_luma_filter;
   int rtjpeg_chroma_filter;
   // libavcodec
   int lavc_bitrate;
   int lavc_qmin;
   int lavc_qmax;
   int lavc_maxqdiff;
   // seek table offset
   long long seektable_offset;
   // key frame adjust offset
   long long keyframeadjust_offset;
   // unused for later -- total size of 128 integers.
   // new fields must be added at the end, above this comment.
   int expansion[109];
} MYTH_PACKED extendeddata;

typedef struct seektable_entry
{
   long long file_offset;
   int keyframe_number; 
} MYTH_PACKED seektable_entry;

typedef struct kfatable_entry
{
   int adjust;
   int keyframe_number;
} MYTH_PACKED kfatable_entry;

#define FRAMEHEADERSIZE sizeof(rtframeheader)
#define FILEHEADERSIZE  sizeof(rtfileheader)
#define EXTENDEDSIZE sizeof(extendeddata)

#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

typedef struct vidbuffertype
{
    int sample;
    int timecode;
    int freeToEncode;
    int freeToBuffer;
    unsigned char *buffer;
    int bufferlen;
    int forcekey;
} vidbuffertyp;

typedef struct audbuffertype
{
    int sample;
    int timecode;
    int freeToEncode;
    int freeToBuffer;
    unsigned char *buffer;
} audbuffertyp;

typedef struct txtbuffertype
{
    int timecode;
    int pagenr;
    int freeToEncode;
    int freeToBuffer;
    unsigned char *buffer;
    int bufferlen;
} txtbuffertyp;

typedef struct teletextsubtitle
{
    unsigned char row;
    unsigned char col;
    unsigned char dbl;
    unsigned char fg;
    unsigned char bg;
    unsigned char len;
} teletextsubtitle;

typedef struct ccsubtitle
{
    unsigned char row;
    unsigned char rowcount;
    unsigned char resumedirect;
    unsigned char resumetext;
    unsigned char clr; // clear the display
    unsigned char len; //length of string to follow
} ccsubtitle;

// resumedirect codes
#define CC_STYLE_POPUP   0x00
#define CC_STYLE_PAINT   0x01
#define CC_STYLE_ROLLUP  0x02

// resumetext special codes
#define CC_LINE_CONT  0x02
#define CC_MODE_MASK  0xf0
#define CC_TXT_MASK   0x20
#define CC_CC1        0x00
#define CC_CC2        0x10
#define CC_TXT1       0x20
#define CC_TXT2       0x30
#define CC_CC3        0x40
#define CC_CC4        0x50
#define CC_TXT3       0x60
#define CC_TXT4       0x70

#endif
