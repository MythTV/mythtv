/*
 *  filter_invert
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "frame.h"

#define FILTER_NAME "Invert";

typedef struct ThisFilter
{
  int (*filter)(Frame *);
  void (*cleanup)(VideoFilter *);

  char *name;
  void *handle; // Library handle;

  /* functions and variables below here considered "private" */

} ThisFilter;


int invert(Frame *frame)
{  
  int size;
  unsigned char *buf=frame->buf;

  if (frame->codec == CODEC_RGB)
    size = frame->width*3 * frame->height;
  else 
    size = frame->width*3/2 * frame->height;

  while(size--)
    *buf = 255 - *(buf++);

  return 0;
}

void cleanup(VideoFilter *filter)
{
  free(filter);
}

VideoFilter *new_filter(char *options)
{
  ThisFilter *filter = malloc(sizeof(ThisFilter));

  if (filter == NULL)
  {
    fprintf(stderr,"Couldn't allocate memory for filter\n");
    return NULL;
  }

  filter->filter=&invert;
  filter->cleanup=&cleanup;
  filter->name = FILTER_NAME;
  return (VideoFilter *)filter;
}

