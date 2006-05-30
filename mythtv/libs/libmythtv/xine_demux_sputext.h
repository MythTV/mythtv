#ifndef XINE_DEMUX_SPUTEXT_H
#define XINE_DEMUX_SPUTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <stdio.h>

#define SUB_BUFSIZE   1024
#define SUB_MAX_TEXT  5
#define MAX_TIMEOUT 4

#define DEBUG_XINE_DEMUX_SPUTEXT 1

typedef struct {

    int lines; ///< Count of text lines in this subtitle set.

    long start; ///< Starting time in msec or starting frame
    long end;   ///< Ending time in msec or starting frame
    
    char *text[SUB_MAX_TEXT]; ///< The subtitle text lines.
} subtitle_t;

typedef struct {

  FILE*              file_ptr;

  int                status;

  char               buf[SUB_BUFSIZE];
  off_t              buflen;

  float              mpsub_position;  

  int                uses_time;  
  int                errs;  
  subtitle_t        *subtitles;
  int                num;            /* number of subtitle structs */
  int                cur;            /* current subtitle           */
  int                format;         /* constants see below        */
  char               next_line[SUB_BUFSIZE]; /* a buffer for next line read from file */

} demux_sputext_t;


subtitle_t *sub_read_file (demux_sputext_t*);

#ifdef __cplusplus
}
#endif

#endif
