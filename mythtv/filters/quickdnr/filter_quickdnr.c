/*
 * Quick DNR 0.7
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as:
 * quickdnr=quality (0-255 scale adjusted)
 * quickdnr=Luma_threshold:Chroma_threshold (0-255) for single threshold
 * quickdnr=Luma_threshold1:Luma_threshold2:Chroma_threshold1:Chroma_threshold2 for double
 * 
 */

//Regular filter
#define LUMA_THRESHOLD_DEFAULT 15
#define CHROMA_THRESHOLD_DEFAULT 25

//Double thresholded filter
#define LUMA_THRESHOLD1_DEFAULT 10
#define LUMA_THRESHOLD2_DEFAULT 2
#define CHROMA_THRESHOLD1_DEFAULT 20
#define CHROMA_THRESHOLD2_DEFAULT 4

//#define QUICKDNR_DEBUG
//#define TIME_FILTER

//From linearblend
#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

#define MM_MMX    0x0001 /* standard MMX */
#define MM_3DNOW  0x0004 /* AMD 3DNOW */
#define MM_MMXEXT 0x0002 /* SSE integer functions or AMD MMX ext */
#define MM_SSE    0x0008 /* SSE functions */
#define MM_SSE2   0x0010 /* PIV SSE2 functions */

#ifdef TIME_FILTER
#include <sys/time.h>
#ifndef TIME_INTERVAL
#define TIME_INTERVAL 300
#endif /* undef TIME_INTERVAL */
#endif /* TIME_FILTER */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "filter.h"
#include "frame.h"

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

#ifdef TIME_FILTER
    int frames;
    double seconds;
#endif /* TIME_FILTER */

} ThisFilter;

int mm_support(void) // From linearblend
{
  int rval;
  int eax, ebx, ecx, edx;

  __asm__ __volatile__ (
			/* See if CPUID instruction is supported ... */
			/* ... Get copies of EFLAGS into eax and ecx */
			"pushf\n\t"
			"popl %0\n\t"
			"movl %0, %1\n\t"

			/* ... Toggle the ID bit in one copy and store */
			/*     to the EFLAGS reg */
			"xorl $0x200000, %0\n\t"
			"push %0\n\t"
			"popf\n\t"

			/* ... Get the (hopefully modified) EFLAGS */
			"pushf\n\t"
			"popl %0\n\t"
			: "=a" (eax), "=c" (ecx)
			:
			: "cc"
			);

  if (eax == ecx)
    return 0; /* CPUID not supported */

  cpuid(0, eax, ebx, ecx, edx);
    
  if (ebx == 0x756e6547 &&
      edx == 0x49656e69 &&
      ecx == 0x6c65746e) {

    /* intel */
  inteltest:
    cpuid(1, eax, ebx, ecx, edx);
    if ((edx & 0x00800000) == 0)
      return 0;
    rval = MM_MMX;
    if (edx & 0x02000000)
      rval |= MM_MMXEXT | MM_SSE;
    if (edx & 0x04000000)
      rval |= MM_SSE2;
    return rval;
  } else if (ebx == 0x68747541 &&
	     edx == 0x69746e65 &&
	     ecx == 0x444d4163) {
    /* AMD */
    cpuid(0x80000000, eax, ebx, ecx, edx);
    if ((unsigned)eax < 0x80000001)
      goto inteltest;
    cpuid(0x80000001, eax, ebx, ecx, edx);
    if ((edx & 0x00800000) == 0)
      return 0;
    rval = MM_MMX;
    if (edx & 0x80000000)
      rval |= MM_3DNOW;
    if (edx & 0x00400000)
      rval |= MM_MMXEXT;
    return rval;
  } else if (ebx == 0x746e6543 &&
	     edx == 0x48727561 &&
	     ecx == 0x736c7561) {  /*  "CentaurHauls" */
    /* VIA C3 */
    cpuid(0x80000000, eax, ebx, ecx, edx);
    if ((unsigned)eax < 0x80000001)
      goto inteltest;
    cpuid(0x80000001, eax, ebx, ecx, edx);
    rval = 0;
    if( edx & ( 1 << 31) )
      rval |= MM_3DNOW;
    if( edx & ( 1 << 23) )
      rval |= MM_MMX;
    if( edx & ( 1 << 24) )
      rval |= MM_MMXEXT;
    return rval;
  } else if (ebx == 0x69727943 &&
	     edx == 0x736e4978 &&
	     ecx == 0x64616574) {
    /* Cyrix Section */
    /* See if extended CPUID level 80000001 is supported */
    /* The value of CPUID/80000001 for the 6x86MX is undefined
       according to the Cyrix CPU Detection Guide (Preliminary
       Rev. 1.01 table 1), so we'll check the value of eax for
       CPUID/0 to see if standard CPUID level 2 is supported.
       According to the table, the only CPU which supports level
       2 is also the only one which supports extended CPUID levels.
    */
    if (eax != 2)
      goto inteltest;
    cpuid(0x80000001, eax, ebx, ecx, edx);
    if ((eax & 0x00800000) == 0)
      return 0;
    rval = MM_MMX;
    if (eax & 0x01000000)
      rval |= MM_MMXEXT;
    return rval;
  } else {
    return 0;
  }
}

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
    if(abs(tf->average[y] - frame->buf[y]) < tf->Luma_threshold1) {
      tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }
  
  for(y = tf->Luma_size;y < tf->UV_size;y++) {
    if(abs(tf->average[y] - frame->buf[y]) < tf->Chroma_threshold1) {
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
 
  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */

  for(y = 0; y < tf->Luma_size; y++) {
    t = abs(tf->average[y] - frame->buf[y]);
    if(t < tf->Luma_threshold1) {
      if(t > tf->Luma_threshold2)
	tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }

 for(y = tf->Luma_size; y < (tf->UV_size); y++) {
    t = abs(tf->average[y] - frame->buf[y]);
    if(t < tf->Chroma_threshold1) {
      if(t > tf->Chroma_threshold2)
	tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }

#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    tf->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    tf->frames = (tf->frames + 1) % TIME_INTERVAL;
    if (tf->frames == 0)
    {
        fprintf (stderr,
                 "QuickDNR: filter timed at %3f frames/sec\n",
                 TIME_INTERVAL / tf->seconds);
        tf->seconds = 0;
    }
#endif /* TIME_FILTER */
 
  return 0;
}

int quickdnrMMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;
  int y;
  uint64_t *buf = (uint64_t *)frame->buf;
  uint64_t *av_p = (uint64_t *)tf->average;;
  const uint64_t sign_convert = 0x8080808080808080LL;

  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */

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

#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    tf->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    tf->frames = (tf->frames + 1) % TIME_INTERVAL;
    if (tf->frames == 0)
    {
        fprintf (stderr,
                 "QuickDNR: filter timed at %3f frames/sec\n",
                 TIME_INTERVAL / tf->seconds);
        tf->seconds = 0;
    }
#endif /* TIME_FILTER */

  return 0;
}


int quickdnr2MMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;
  int y;
  uint64_t *buf = (uint64_t *)frame->buf;
  uint64_t *av_p = (uint64_t *)tf->average;;
  const uint64_t sign_convert = 0x8080808080808080LL;

 
  if (tf->first)
  {
    memcpy (tf->average, frame->buf, frame->size);
    tf->first = 0;
  }

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */

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

		 "pcmpgtb %%mm0, %%mm1 \n\t" //Secondary threshold
		 "psubb %%mm0, %%mm3   \n\t" 
		 "psubb %%mm7, %%mm0   \n\t" 
		 "pand %%mm1, %%mm3    \n\t" 
		 "pandn %%mm0,%%mm1    \n\t"
		 "por %%mm1, %%mm3     \n\t"

		 "paddb %%mm4, %%mm3   \n\t"
		 "pcmpgtb (%2) , %%mm3 \n\t"

		 "pavgb %%mm2, %%mm7   \n\t"

		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm2,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t" //mm3 becomes av

		 "movq (%1), %%mm1     \n\t" //reload registers
		 "movq %%mm3, %%mm0    \n\t" 
		 "movq %%mm3, %%mm2    \n\t"
		 "movq %%mm1, %%mm3    \n\t"
		 "movq %%mm1, %%mm7    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //1 if av greater
		 "psubb %%mm0, %%mm3   \n\t" //mm3=buf-av
		 "psubb %%mm7, %%mm0   \n\t" //mm0=av-buf
		 "pand %%mm1, %%mm3    \n\t" //select buf
		 "pandn %%mm0,%%mm1    \n\t" //select av
		 "por %%mm1, %%mm3     \n\t" //mm3=abs()

		 "paddb %%mm4, %%mm3   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm5, %%mm3 \n\t" //compare diff with mask
	       
		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm2,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t"
		 "movq %%mm3, (%0)     \n\t"
		 "movq %%mm3, (%1)     \n\t"
		 : : "r" (av_p), "r" (buf), "r" (&tf->Luma_threshold_mask2)
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

		 "pcmpgtb %%mm0, %%mm1 \n\t" //Secondary threshold
		 "psubb %%mm0, %%mm3   \n\t" 
		 "psubb %%mm7, %%mm0   \n\t" 
		 "pand %%mm1, %%mm3    \n\t" 
		 "pandn %%mm0,%%mm1    \n\t"
		 "por %%mm1, %%mm3     \n\t"

		 "paddb %%mm4, %%mm3   \n\t"
		 "pcmpgtb (%2) , %%mm3 \n\t"

		 "pavgb %%mm2, %%mm7   \n\t"

		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm2,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t" //mm3 becomes av

		 "movq (%1), %%mm1     \n\t" //reload registers
		 "movq %%mm3, %%mm0    \n\t" 
		 "movq %%mm3, %%mm2    \n\t"
		 "movq %%mm1, %%mm3    \n\t"
		 "movq %%mm1, %%mm7    \n\t"

		 "pcmpgtb %%mm0, %%mm1 \n\t" //1 if av greater
		 "psubb %%mm0, %%mm3   \n\t" //mm3=buf-av
		 "psubb %%mm7, %%mm0   \n\t" //mm0=av-buf
		 "pand %%mm1, %%mm3    \n\t" //select buf
		 "pandn %%mm0,%%mm1    \n\t" //select av
		 "por %%mm1, %%mm3     \n\t" //mm3=abs()

		 "paddb %%mm4, %%mm3   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm6, %%mm3 \n\t" //compare diff with mask
	       
		 "pand %%mm3, %%mm7    \n\t"
		 "pandn %%mm2,%%mm3    \n\t"
		 "por %%mm7, %%mm3     \n\t"
		 "movq %%mm3, (%0)     \n\t"
		 "movq %%mm3, (%1)     \n\t"
		 : : "r" (av_p), "r" (buf), "r" (&tf->Chroma_threshold_mask2)
		 );
    buf++;
    av_p++;
  }

  asm volatile("emms\n\t");

#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    tf->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    tf->frames = (tf->frames + 1) % TIME_INTERVAL;
    if (tf->frames == 0)
    {
        fprintf (stderr,
                 "QuickDNR: filter timed at %3f frames/sec\n",
                 TIME_INTERVAL / tf->seconds);
        tf->seconds = 0;
    }
#endif /* TIME_FILTER */

  return 0;
}

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
#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */

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
      filter->Luma_threshold2 = ((uint8_t) Param1) * 8 / 255;
      filter->Chroma_threshold1 = ((uint8_t) Param1) * 80 / 255;
      filter->Chroma_threshold2 = ((uint8_t) Param1) * 16 / 255;
      double_threshold = 1;
      break;
    case 2:
      filter->Luma_threshold1 = (uint8_t) Param1;
      filter->Chroma_threshold1 = (uint8_t) Param2;
      double_threshold = -1;
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

  if(mm_support() > MM_MMXEXT) {
    if(double_threshold) filter->vf.filter = &quickdnr2MMX;
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
  else if(double_threshold) filter->vf.filter = &quickdnr2;
  else filter->vf.filter = &quickdnr;

  filter->first = 1;
  
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
    descript:   "removes noise with a fast thresholded average filter",
    formats:    FmtList,
    libname:    NULL
  },
  FILT_NULL
};
