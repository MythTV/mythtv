/*
 * Quick DNR 0.5
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as quickdnr=Luma_threshold,Chroma_threshold (0-255)
 * 
 */

#define LUMA_THRESHOLD_DEFAULT 20
#define CHROMA_THRESHOLD_DEFAULT 30
//#define QUICKDNR_DEBUG

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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "filter.h"
#include "frame.h"

static const char FILTER_NAME[] = "quickdnr";

typedef struct ThisFilter
{
  int (*filter)(VideoFilter *, VideoFrame *);
  void (*cleanup)(VideoFilter *);

  char *name;
  void *handle; // Library handle;

  /* functions and variables below here considered "private" */
  int Luma_size;
  int UV_size;
  int sized;
  uint64_t Luma_threshold_mask;
  uint64_t Chroma_threshold_mask;
  uint8_t Luma_threshold;
  uint8_t Chroma_threshold;
  uint8_t *average;

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

  if (!tf->sized) {
    tf->sized = 1;
    if (frame->codec == FMT_YV12) { // Only support YV12 for now
      tf->average=malloc(sizeof(uint8_t) * frame->width * 3 / 2 * frame->height);
      if (tf->average == NULL)
	{
	  fprintf(stderr,"Couldn't allocate memory for QuickDNR buffer\n");
	  return -1;
	}
      tf->Luma_size = frame->width * frame->height;
      tf->UV_size = frame->width * frame->height / 2 + tf->Luma_size;
    }
    else
      return -1;
  }
  
  for(y = 0;y < tf->Luma_size;y++) {
    //Compare absolute value.
    if(((tf->average[y] - frame->buf[y]) | (frame->buf[y] - tf->average[y])) < tf->Luma_threshold) {
       tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }
  
  for(y = tf->Luma_size;y < tf->UV_size;y++) {
     if(((tf->average[y] - frame->buf[y]) | (frame->buf[y] - tf->average[y])) < tf->Chroma_threshold) {
      tf->average[y] = (tf->average[y] + frame->buf[y]) >> 1;
      frame->buf[y] = tf->average[y];
    }
    else tf->average[y] = frame->buf[y];
  }
  return 0;
}

int quickdnrMMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f; 
  int y;
  uint64_t *buf = (uint64_t *)frame->buf;
  uint64_t *av_p;
  const uint64_t sign_convert = 0x8080808080808080;

  if (!tf->sized) {
    tf->sized = 1;
    if (frame->codec == FMT_YV12) { // Only support YV12 for now
      tf->average=malloc(sizeof(uint8_t) * frame->width * 3 / 2 * frame->height);
      if (tf->average == NULL)
	{
	  fprintf(stderr,"Couldn't allocate memory for DNR buffer\n");
	  return -1;
	}
      tf->Luma_size = frame->width * frame->height;
      tf->UV_size = frame->width * frame->height / 2 + tf->Luma_size;
    }
    else
      return -1;
  }
  
  av_p = (uint64_t *)tf->average;

  asm volatile("prefetch 64(%0)     \n\t" //Experimental values from athlon
	       "prefetch 64(%1)     \n\t"
	       "prefetch 64(%2)     \n\t"
	       "movq (%0), %%mm4     \n\t" //av-p
	       "movq (%1), %%mm5     \n\t"
	       "movq (%2), %%mm6     \n\t"
	       : : "r" (&sign_convert), "r" (&tf->Luma_threshold_mask), "r" (&tf->Chroma_threshold_mask)		  
	       );

  for(y = 0;y < tf->Luma_size;y += 8) { //Luma
    asm volatile("prefetchw 256(%0)    \n\t" //Experimental values from athlon
		 "prefetch 384(%1)     \n\t"
		 "movq (%0), %%mm0     \n\t" //av-p
		 "movq (%1), %%mm1     \n\t" //buf
		 "movq %%mm0, %%mm2    \n\t" 
		 "movq %%mm1, %%mm3    \n\t"
		 "pavgb %%mm3, %%mm0   \n\t"
		 "movq %%mm0, (%0)     \n\t"
		 "psubb %%mm0, %%mm1   \n\t" //Absolute difference algorithm
		 "psubb %%mm1, %%mm2   \n\t"
		 "por %%mm1, %%mm2     \n\t"
		 "psubb %%mm4, %%mm2   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm5, %%mm1 \n\t" //compare select.
		 "pand %%mm1, %%mm0    \n\t"
		 "pandn %%mm3, %%mm1   \n\t"
		 "por %%mm0, %%mm1     \n\t"
		 "movq %%mm1, (%1)     \n\t"

		 : : "r" (av_p), "r" (buf)
		 );
    buf++;
    av_p++;
  }

  for(y = tf->Luma_size;y < tf->UV_size;y += 8) { //Chroma
    asm volatile("prefetchw 256(%0)    \n\t" //Experimental values for athlon
		 "prefetch 384(%1)     \n\t"
 
		 "movq (%0), %%mm0     \n\t" //av-p
		 "movq (%1), %%mm1     \n\t" //buf
		 "movq %%mm0, %%mm2    \n\t" 
		 "movq %%mm1, %%mm3    \n\t"
	 
		 "pavgb %%mm3, %%mm0   \n\t"
		 "movq %%mm0, (%0)     \n\t"
		 "psubb %%mm0, %%mm1   \n\t" //Absolute difference algorithm
		 "psubb %%mm1, %%mm2   \n\t"
		 "por %%mm1, %%mm2     \n\t"
		 "psubb %%mm4, %%mm2   \n\t" //hack! No proper unsigned mmx compares!
		 "pcmpgtb %%mm6, %%mm1 \n\t" //compare select.
		 "pand %%mm1, %%mm0    \n\t"
		 "pandn %%mm3, %%mm1   \n\t"
		 "por %%mm0, %%mm1     \n\t"
		 "movq %%mm1, (%1)     \n\t"

		 : : "r" (av_p), "r" (buf)
		 );
    buf++;
    av_p++;
  }

  asm volatile("emms\n\t");
  return 0;
}

void cleanup(VideoFilter *vf)
{
  ThisFilter *tf = (ThisFilter *)vf;
  free(tf->average);
  free((ThisFilter *)vf);
}

VideoFilter *new_filter(char *options)
{
  unsigned int Param1, Param2;
  int i;
  ThisFilter *filter = malloc(sizeof(ThisFilter));

  if (filter == NULL)
    {
      fprintf(stderr,"Couldn't allocate memory for filter\n");
      return NULL;
    }

  if (options && (sscanf(options, "%u:%u", &Param1, &Param2) == 2)) { 
    filter->Luma_threshold = (uint8_t) Param1;
    filter->Chroma_threshold = (uint8_t) Param2;
  }
  else {
    filter->Luma_threshold = LUMA_THRESHOLD_DEFAULT;
    filter->Chroma_threshold = CHROMA_THRESHOLD_DEFAULT;
  }
  
  if(mm_support() > MM_MMXEXT) {
    filter->filter = &quickdnrMMX;
    
    filter->Luma_threshold_mask = 0;
    filter->Chroma_threshold_mask = 0;
    for(i = 0;i < 8;i++) {
      // 8 Inverse sign-shifted bytes! 0=0x80 255=0x7F
      filter->Luma_threshold_mask = (filter->Luma_threshold_mask << 8)\
	+((filter->Luma_threshold<128) ? (0x7F - filter->Luma_threshold) : (0xFF-( filter->Luma_threshold-0x80)));
      filter->Chroma_threshold_mask = (filter->Chroma_threshold_mask << 8)\
	+ ((filter->Chroma_threshold<128) ? (0x7F - filter->Chroma_threshold) : (0xFF-(filter->Chroma_threshold-0x80)));	   
    }
  }
  else filter->filter = &quickdnr;
  
#ifdef QUICKDNR_DEBUG
  fprintf(stderr,"DNR Loaded:%X Params: %u %u Luma: %d %X%X Chroma: %d %X%X\n",mm_support(), Param1, Param2, filter->Luma_threshold, ((int*)&filter->Luma_threshold_mask)[1], ((int*)&filter->Luma_threshold_mask)[0], filter->Chroma_threshold, ((int*)&filter->Chroma_threshold_mask)[1], ((int*)&filter->Chroma_threshold_mask)[0]);
#endif

  filter->cleanup = &cleanup;
  filter->name = (char *)FILTER_NAME;
  filter->sized = 0;
  filter->average = NULL;
  return (VideoFilter *)filter;
}


