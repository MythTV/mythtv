#include <stdio.h>

#ifdef RTJPEG_INTERNAL

int            rtjpeg_file=0;
int            rtjpeg_eof=0;
int            rtjpeg_video_width;
int            rtjpeg_video_height;
double         rtjpeg_video_frame_rate;
unsigned char *rtjpeg_rgb=0;
unsigned char *rtjpeg_buf=0;
int            rtjpeg_keyframedist;
int            rtjpeg_effdsp;
int            rtjpeg_filesize;
int            rtjpeg_startpos;
int            rtjpeg_audiodelay;
struct rtfileheader rtjpeg_fileheader;
lame_global_flags *gf;
#else
extern int            rtjpeg_file;
extern int            rtjpeg_eof;
extern int            rtjpeg_video_width;
extern int            rtjpeg_video_height;
extern double         rtjpeg_video_frame_rate;
extern int            rtjpeg_effdsp;
extern int            rtjpeg_audiodelay;
extern struct rtfileheader rtjpeg_fileheader;
#endif


int            rtjpeg_open(char *tplorg);
int            rtjpeg_close();
int            rtjpeg_get_video_width();
int            rtjpeg_get_video_height();
double         rtjpeg_get_video_frame_rate();
unsigned char *rtjpeg_get_frame(int *timecode, int onlyvideo,
                                unsigned char **audiodata, int *alen);

int            rtjpeg_end_of_video();
int            rtjpeg_check_sig(char *fname);

