/*
 * Quick DNR 0.8
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as:
 * quickdnr=quality (0-255 scale adjusted)
 * quickdnr=Luma_threshold:Chroma_threshold (0-255) for single threshold
 * quickdnr=Luma_threshold1:Luma_threshold2:Chroma_threshold1:Chroma_threshold2 for double
 * 
 */

#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "filter.h"
#include "frame.h"
#include "dsputil.h"

#ifdef MMX
#include "i386/mmx.h"
#endif

//Regular filter
#define LUMA_THRESHOLD_DEFAULT 15
#define CHROMA_THRESHOLD_DEFAULT 25

//Double thresholded filter
#define LUMA_THRESHOLD1_DEFAULT 10
#define LUMA_THRESHOLD2_DEFAULT 1
#define CHROMA_THRESHOLD1_DEFAULT 20
#define CHROMA_THRESHOLD2_DEFAULT 2

//#define QUICKDNR_DEBUG

static const char FILTER_NAME[] = "quickdnr";

typedef struct ThisFilter
{
  VideoFilter vf;

  int Luma_size;
  int UV_size;
  int first;
  uint64_t Luma_threshold_mask1, Luma_threshold_mask2;
  uint64_t Chroma_threshold_mask1, Chroma_threshold_mask2;
  uint8_t Luma_threshold1, Luma_threshold2;
  uint8_t Chroma_threshold1, Chroma_threshold2;
  uint8_t *average;

  TF_STRUCT;

} ThisFilter;

int quickdnr(VideoFilter *f, VideoFrame *frame)
{  
  ThisFilter *tf = (ThisFilter *)f; 
  int y; 

  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }
  for(y = 0;y < tf->Luma_size;y++) {
    if (abs(tf->average[y] - frame->buf[y]) < tf->Luma_threshold1) {
      tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }
  
  for(y = tf->Luma_size;y < tf->UV_size;y++) {
    if (abs(tf->average[y] - frame->buf[y]) < tf->Chroma_threshold1) {
      tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }
  return 0;
}

int quickdnr2(VideoFilter *f, VideoFrame *frame)
{  
  ThisFilter *tf = (ThisFilter *)f; 
  int y,t; 
  TF_VARS;
 
  TF_START;
  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }

  for(y = 0; y < tf->Luma_size; y++) {
    t = abs(tf->average[y] - frame->buf[y]);
    if (t < tf->Luma_threshold1) {
      if (t > tf->Luma_threshold2)
	tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }

 for(y = tf->Luma_size; y < (tf->UV_size); y++) {
    t = abs(tf->average[y] - frame->buf[y]);
    if (t < tf->Chroma_threshold1) {
      if (t > tf->Chroma_threshold2)
	tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
 }

 TF_END(tf, "QuickDNR: ");
 
 return 0;
}

#ifdef MMX

int quickdnrMMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;
  int y;
  uint64_t *buf = (uint64_t *)frame->buf;
  uint64_t *av_p = (uint64_t *)tf->average;
  const uint64_t sign_convert = 0x8080808080808080LL;
  TF_VARS;

  TF_START;
  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }

  asm volatile("prefetch 64(%0)     \n\t" //Experimental values from athlon
	       "prefetch 64(%1)     \n\t"
	       "prefetch 64(%2)     \n\t"
	       "movq (%0), %%mm4    \n\t"
	       "movq (%1), %%mm5    \n\t"
	       "movq (%2), %%mm6    \n\t"
	       : : "r" (&sign_convert), "r" (&tf->Luma_threshold_mask1), "r" (&tf->Chroma_threshold_mask1)
	       );

  for(y = 0;y < (tf->Luma_size);y += 8) { //Luma
    asm volatile("prefetchw 384(%0)    \n\t" //Experimental values from athlon
		 "prefetch 384(%1)     \n\t"

		 "movq (%0), %%mm0     \n\t" //av-p
		 "movq (%1), %%mm1     \n\t" //buf
		 "movq %%mm0, %%mm2    \n\t"
		 "movq %%mm1, %%mm3    \n\t"
		 "movq %%mm1, %%mm7    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //1 if av greater
		 "psubb %%mm0, %%mm3   \n\t" //mm3=buf-av
		 "psubb %%mm7, %%mm0   \n\t" //mm0=av-buf
		 "pand %%mm1, %%mm3    \n\t" //select buf
		 "pandn %%mm0,%%mm1    \n\t" //select av
		 "por %%mm1, %%mm3     \n\t" //mm3=abs()

		 "paddb %%mm4, %%mm3   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm5, %%mm3 \n\t" //compare buf with mask

		 "pavgb %%mm7, %%mm2   \n\t"
		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm2,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t"
		 "movq %%mm3, (%0)     \n\t"
		 "movq %%mm3, (%1)     \n\t"
		 : : "r" (av_p), "r" (buf)
		 );
    buf++;
    av_p++;
  }

  for(y = tf->Luma_size;y < tf->UV_size;y += 8) { //Chroma
    asm volatile("prefetchw 384(%0)    \n\t" //Experimental values for athlon
		 "prefetch 384(%1)     \n\t"

		 "movq (%0), %%mm0     \n\t" //av-p
		 "movq (%1), %%mm1     \n\t" //buf
		 "movq %%mm0, %%mm2    \n\t"
		 "movq %%mm1, %%mm3    \n\t"
		 "movq %%mm1, %%mm7    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //1 if av greater
		 "psubb %%mm0, %%mm3   \n\t" //mm3=buf-av
		 "psubb %%mm7, %%mm0   \n\t" //mm0=av-buf
		 "pand %%mm1, %%mm3    \n\t" //select buf
		 "pandn %%mm0,%%mm1    \n\t" //select av
		 "por %%mm1, %%mm3     \n\t" //mm3=abs()

		 "paddb %%mm4, %%mm3   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm6, %%mm3 \n\t" //compare buf with mask

		 "pavgb %%mm7, %%mm2   \n\t"
		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm2,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t"
		 "movq %%mm3, (%0)     \n\t"
		 "movq %%mm3, (%1)     \n\t"
		 : : "r" (av_p), "r" (buf)
		 );
    buf++;
    av_p++;
  }

  asm volatile("emms\n\t");

  TF_END(tf, "QuickDNR: ");

  return 0;
}


int quickdnr2MMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;
  int y;
  uint64_t *buf = (uint64_t *)frame->buf;
  uint64_t *av_p = (uint64_t *)tf->average;
  const uint64_t sign_convert = 0x8080808080808080LL;
  TF_VARS;
  
  TF_START;
 
  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }

  asm volatile("prefetch 64(%0)     \n\t" //Experimental values from athlon
	       "prefetch 64(%1)     \n\t"
	       "movq (%0), %%mm4    \n\t"
	       "movq (%1), %%mm5    \n\t"
	       : : "r" (&sign_convert), "r" (&tf->Luma_threshold_mask1)
	       );

  for(y = 0;y < (tf->Luma_size);y += 8) { //Luma
    asm volatile("prefetchw 384(%0)    \n\t" //Experimental values from athlon
		 "prefetch 384(%1)     \n\t"

		 "movq (%0), %%mm0     \n\t" //av-p
		 "movq (%1), %%mm1     \n\t" //buf
		 "movq %%mm0, %%mm2    \n\t"
		 "movq %%mm1, %%mm3    \n\t"
		 "movq %%mm1, %%mm6    \n\t"
		 "movq %%mm1, %%mm7    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //1 if av greater
		 "psubb %%mm0, %%mm3   \n\t" //mm3=buf-av
		 "psubb %%mm7, %%mm0   \n\t" //mm0=av-buf
		 "pand %%mm1, %%mm3    \n\t" //select buf
		 "pandn %%mm0,%%mm1    \n\t" //select av
		 "por %%mm1, %%mm3     \n\t" //mm3=abs(buf-av)

		 "paddb %%mm4, %%mm3   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm5, %%mm3 \n\t" //compare diff with mask
		 
		 "movq %%mm2, %%mm0    \n\t" //reload registers
		 "movq %%mm7, %%mm1    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //Secondary threshold
		 "psubb %%mm0, %%mm6   \n\t"
		 "psubb %%mm7, %%mm0   \n\t"
		 "pand %%mm1, %%mm6    \n\t"
		 "pandn %%mm0,%%mm1    \n\t"
		 "por %%mm1, %%mm6     \n\t"

		 "paddb %%mm4, %%mm6   \n\t"
		 "pcmpgtb (%2), %%mm6  \n\t"

		 "movq %%mm2, %%mm0    \n\t"

 		 "pavgb %%mm7, %%mm2   \n\t"

		 "pand %%mm6, %%mm2    \n\t"
		 "pandn %%mm0,%%mm6    \n\t"
		 "por %%mm2, %%mm6     \n\t" // Combined new/keep average

		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm6,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t" // Combined new/keep average

		 "movq %%mm3, (%0)     \n\t"
		 "movq %%mm3, (%1)     \n\t"
		 : : "r" (av_p), "r" (buf), "r" (&tf->Luma_threshold_mask2)
		 );
    buf++;
    av_p++;
  }

  asm volatile("prefetch 64(%0)     \n\t" //Experimental values from athlon
	       "movq (%1), %%mm5    \n\t"
	       : : "r" (&sign_convert), "r" (&tf->Chroma_threshold_mask1)
	       );

  for(y = tf->Luma_size;y < tf->UV_size;y += 8) { //Chroma
    asm volatile("prefetchw 384(%0)    \n\t" //Experimental values for athlon
		 "prefetch 384(%1)     \n\t"

		 "movq (%0), %%mm0     \n\t" //av-p
		 "movq (%1), %%mm1     \n\t" //buf
		 "movq %%mm0, %%mm2    \n\t"
		 "movq %%mm1, %%mm3    \n\t"
		 "movq %%mm1, %%mm6    \n\t"
		 "movq %%mm1, %%mm7    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //1 if av greater
		 "psubb %%mm0, %%mm3   \n\t" //mm3=buf-av
		 "psubb %%mm7, %%mm0   \n\t" //mm0=av-buf
		 "pand %%mm1, %%mm3    \n\t" //select buf
		 "pandn %%mm0,%%mm1    \n\t" //select av
		 "por %%mm1, %%mm3     \n\t" //mm3=abs(buf-av)

		 "paddb %%mm4, %%mm3   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm5, %%mm3 \n\t" //compare diff with mask
		 
		 "movq %%mm2, %%mm0    \n\t" //reload registers
		 "movq %%mm7, %%mm1    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //Secondary threshold
		 "psubb %%mm0, %%mm6   \n\t"
		 "psubb %%mm7, %%mm0   \n\t"
		 "pand %%mm1, %%mm6    \n\t"
		 "pandn %%mm0,%%mm1    \n\t"
		 "por %%mm1, %%mm6     \n\t"

		 "paddb %%mm4, %%mm6   \n\t"
		 "pcmpgtb (%2), %%mm6  \n\t"

		 "movq %%mm2, %%mm0    \n\t"

 		 "pavgb %%mm7, %%mm2   \n\t"

		 "pand %%mm6, %%mm2    \n\t"
		 "pandn %%mm0,%%mm6    \n\t"
		 "por %%mm2, %%mm6     \n\t" //Combined new/keep average

		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm6,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t" //Combined new/keep average

		 "movq %%mm3, (%0)     \n\t"
		 "movq %%mm3, (%1)     \n\t"
		 : : "r" (av_p), "r" (buf), "r" (&tf->Chroma_threshold_mask2)
		 );
    buf++;
    av_p++;
  }

  asm volatile("emms\n\t");

  TF_END(tf, "QuickDNR: ");

  return 0;
}
#endif /* MMX */

void cleanup(VideoFilter *vf)
{
  ThisFilter *tf;
  tf = (ThisFilter *)vf;
  free(tf->average);
}

VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
  unsigned int Param1, Param2, Param3, Param4;
  int i, double_threshold = 1;
  ThisFilter *filter;

  if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
      fprintf(stderr,"QuickDNR: attempt to initialize with unsupported format\n");
      return NULL;
    }

  filter = malloc(sizeof(ThisFilter));

  if (filter == NULL)
    {
      fprintf(stderr,"Couldn't allocate memory for filter\n");
      return NULL;
    }

  filter->average=malloc(sizeof(uint8_t) * (*width) * 3 / 2 * (*height));
  if (filter->average == NULL)
    {
      fprintf(stderr,"Couldn't allocate memory for DNR buffer\n");
      free(filter);
      return NULL;
    }
  filter->Luma_size = *width * *height;
  filter->UV_size = *width * *height / 2 + filter->Luma_size;
  
  if (options)
    switch(sscanf(options, "%u:%u:%u:%u", &Param1, &Param2, &Param3, &Param4)) {
    case 1:
      //These might be better as logarithmic if this gets used a lot.
      filter->Luma_threshold1 = ((uint8_t) Param1) * 40 / 255;
      filter->Luma_threshold2 = ((uint8_t)  Param1) * 4/255 > 2 ? 2 : ((uint8_t)  Param1) * 4/255;
      filter->Chroma_threshold1 = ((uint8_t) Param1) * 80 / 255;
      filter->Chroma_threshold2 = ((uint8_t)  Param1) * 8/255 > 4 ? 4 : ((uint8_t)  Param1) * 8/255;
      double_threshold = 1; 
      break;
    case 2:
      filter->Luma_threshold1 = (uint8_t) Param1;
      filter->Chroma_threshold1 = (uint8_t) Param2;
      double_threshold = 0;
      break;
    case 4:
      filter->Luma_threshold1 = (uint8_t) Param1;
      filter->Luma_threshold2 = (uint8_t) Param2;
      filter->Chroma_threshold1 = (uint8_t) Param3;
      filter->Chroma_threshold2 = (uint8_t) Param4;
      double_threshold = 1;
      break;
    default:
      filter->Luma_threshold1 = LUMA_THRESHOLD1_DEFAULT;
      filter->Chroma_threshold1 = CHROMA_THRESHOLD1_DEFAULT;
      filter->Luma_threshold2 = LUMA_THRESHOLD2_DEFAULT;
      filter->Chroma_threshold2 = CHROMA_THRESHOLD2_DEFAULT;
      double_threshold = 1;
      break;
    }
  else {
    filter->Luma_threshold1 = LUMA_THRESHOLD1_DEFAULT;
    filter->Chroma_threshold1 = CHROMA_THRESHOLD1_DEFAULT;
    filter->Luma_threshold2 = LUMA_THRESHOLD2_DEFAULT;
    filter->Chroma_threshold2 = CHROMA_THRESHOLD2_DEFAULT;
    double_threshold = 1;
  }

#ifdef MMX
  if (mm_support() > MM_MMXEXT) {
    if (double_threshold) filter->vf.filter = &quickdnr2MMX;
    else filter->vf.filter = &quickdnrMMX;
    
    filter->Luma_threshold_mask1 = 0;
    filter->Chroma_threshold_mask1 = 0;
    filter->Luma_threshold_mask2 = 0;
    filter->Chroma_threshold_mask2 = 0;
    
    for(i = 0;i < 8;i++) {
      // 8 sign-shifted bytes!
      filter->Luma_threshold_mask1 = (filter->Luma_threshold_mask1 << 8)\
	+ ((filter->Luma_threshold1 > 0x80) ? (filter->Luma_threshold1 - 0x80)
	   : (filter->Luma_threshold1 + 0x80));
      filter->Chroma_threshold_mask1 = (filter->Chroma_threshold_mask1 << 8)\
	+ ((filter->Chroma_threshold1 > 0x80) ? (filter->Chroma_threshold1 - 0x80)
	   :  (filter->Chroma_threshold1 + 0x80));
      filter->Luma_threshold_mask2 = (filter->Luma_threshold_mask2 << 8)\
	+ ((filter->Luma_threshold2 > 0x80) ? (filter->Luma_threshold2 - 0x80)
	   : (filter->Luma_threshold2 + 0x80));
      filter->Chroma_threshold_mask2 = (filter->Chroma_threshold_mask2 << 8)\
	+ ((filter->Chroma_threshold2 > 0x80) ? (filter->Chroma_threshold2 - 0x80)
	   :  (filter->Chroma_threshold2 + 0x80));
    }

  }
  else
#endif
  if (double_threshold) filter->vf.filter = &quickdnr2;
  else filter->vf.filter = &quickdnr;

  filter->first = 1;
  TF_INIT(filter);
  
#ifdef QUICKDNR_DEBUG
  fprintf(stderr,"DNR Loaded:%X Params: %u %u Luma1: %d %X%X Luma2: %X%X Chroma1: %d %X%X Chroma2: %X%X\n",
	  mm_support(),Param1, Param2, filter->Luma_threshold1, ((int*)&filter->Luma_threshold_mask1)[1],
	  ((int*)&filter->Luma_threshold_mask1)[0], ((int*)&filter->Luma_threshold_mask2)[1],
	  ((int*)&filter->Luma_threshold_mask2)[0], filter->Chroma_threshold1,
	  ((int*)&filter->Chroma_threshold_mask1)[1], ((int*)&filter->Chroma_threshold_mask1)[0],
	  ((int*)&filter->Chroma_threshold_mask2)[1], ((int*)&filter->Chroma_threshold_mask2)[0]
	  );

  fprintf(stderr, "Options:%d:%d:%d:%d\n",filter->Luma_threshold1, filter->Luma_threshold2, filter->Chroma_threshold1,  filter->Chroma_threshold2);
#endif

  filter->vf.cleanup = &cleanup;
  return (VideoFilter *)filter;
}




static FmtConv FmtList[] = 
{
  { FMT_YV12, FMT_YV12 },
  FMT_NULL
};

FilterInfo filter_table[] = 
{
  {
    symbol:     "new_filter",
    name:       "quickdnr",
    descript:   "removes noise with a fast single/double thresholded average filter",
    formats:    FmtList,
    libname:    NULL
  },
  FILT_NULL
};
