// a linear blending deinterlacer yoinked from the mplayer sources.

#include <stdlib.h>
#include <stdio.h>

#include "filter.h"
#include "frame.h"
#include "postprocess.h"
#include "cpudetect.h"

#define FILTER_NAME "postprocess";

int verbose = 0;
int divx_quality = 0;

typedef struct ThisFilter
{
  int (*filter)(VideoFilter *, Frame *);
  void (*cleanup)(VideoFilter *);

  char *name;
  void *handle; // Library handle;

  /* functions and variables below here considered "private" */

  struct PPMode mode;
  int initialized;
  unsigned char *dest_page[3];
  unsigned char *pp_page[3];
  unsigned char *dest;
} ThisFilter;

int ppFilter(VideoFilter *vf, Frame *frame)
{
  ThisFilter *tf = (ThisFilter *)vf;
  if (!tf->initialized)
  {
      tf->dest = malloc(frame->width * frame->height * 3 / 2);
      tf->dest_page[0] = tf->dest;
      tf->dest_page[1] = tf->dest_page[0] + (frame->width * frame->height);
      tf->dest_page[2] = tf->dest_page[1] + (frame->width * frame->height) / 4;
      tf->initialized = 1;
  }
      
  tf->pp_page[0] = frame->buf;
  tf->pp_page[1] = tf->pp_page[0] + (frame->width * frame->height);
  tf->pp_page[2] = tf->pp_page[1] + (frame->width * frame->height) / 4;

  postprocess(tf->pp_page, frame->width, tf->dest_page, frame->width, 
              frame->width, frame->height, NULL, 0, GET_PP_QUALITY_MAX);

  memcpy(frame->buf, tf->dest, frame->width * frame->height * 3 / 2);

  return 0;
}

void cleanup(VideoFilter *filter)
{
  ThisFilter *tf = (ThisFilter *)filter;
  
  if (tf->initialized)
      free(tf->dest);
  free(tf);
}

VideoFilter *new_filter(char *options)
{
  ThisFilter *filter = malloc(sizeof(ThisFilter));

  if (filter == NULL)
  {
    fprintf(stderr,"Couldn't allocate memory for filter\n");
    return NULL;
  }

  GetCpuCaps(&gCpuCaps);

  filter->filter=&ppFilter;
  filter->cleanup=&cleanup;
  filter->name = FILTER_NAME;

  filter->initialized = 0;
  
  readNPPOpt("", "hb:a,vb:a,dr,al,lb");
  
  return (VideoFilter *)filter;
}

