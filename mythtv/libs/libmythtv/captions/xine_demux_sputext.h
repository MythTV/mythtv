#ifndef XINE_DEMUX_SPUTEXT_H
#define XINE_DEMUX_SPUTEXT_H

#include <vector>

#include "io/mythmediabuffer.h"

#define SUB_BUFSIZE   1024
#define MAX_TIMEOUT 4

#define DEBUG_XINE_DEMUX_SPUTEXT 0

#define FORMAT_UNKNOWN   (-1)
#define FORMAT_MICRODVD   0
#define FORMAT_SUBRIP     1
#define FORMAT_SUBVIEWER  2
#define FORMAT_SAMI       3 /* Microsoft Synchronized Accessible Media Interchange */
#define FORMAT_VPLAYER    4 /* Windows Application */
#define FORMAT_RT         5 /* RealText */
#define FORMAT_SSA        6 /* Sub Station Alpha */
#define FORMAT_PJS        7 /* Phoenix Japanimation Society */
#define FORMAT_MPSUB      8 /* MPlayer */
#define FORMAT_AQTITLE    9 /* Czech subtitling community */
#define FORMAT_JACOBSUB   10 /* Amiga - Japanese Animation Club of Orlando */
#define FORMAT_SUBVIEWER2 11
#define FORMAT_SUBRIP09   12
#define FORMAT_MPL2       13 /*Mplayer sub 2 ?*/

struct subtitle_t {

    long start; ///< Starting time in msec or starting frame
    long end;   ///< Ending time in msec or starting frame

    std::vector<std::string> text; ///< The subtitle text lines.
};

struct demux_sputext_t {

  char              *rbuffer_text;
  off_t              rbuffer_len;
  off_t              rbuffer_cur;

  int                status;

  char               buf[SUB_BUFSIZE];
  off_t              buflen;

  float              mpsub_position;

  int                uses_time;
  int                errs;
  std::vector<subtitle_t> subtitles;
  int                num;            /* number of subtitle structs */
  int                cur;            /* current subtitle           */
  int                format;         /* constants see below        */
  char               next_line[SUB_BUFSIZE]; /* a buffer for next line read from file */

};


bool sub_read_file (demux_sputext_t *demuxstr);

#endif
