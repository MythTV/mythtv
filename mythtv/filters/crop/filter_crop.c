/*
 * crop v 0.1
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as crop=top:left:bottom:right as number of 16 pixel blocks.
 * 
 */

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
#include <string.h>

#include "filter.h"
#include "frame.h"

//#define TIME_FILTER
#ifdef TIME_FILTER
#include <sys/time.h>
#ifndef TIME_INTERVAL
#define TIME_INTERVAL 300
#endif /* undef TIME_INTERVAL */
#endif /* TIME_FILTER */

static const char FILTER_NAME[] = "crop";

typedef struct ThisFilter
{
  VideoFilter vf;

  int Luma_size;
  int UV_size;
  int width;
  int height;
  int Chroma_plane_size;
  int yc1,yc2,xc1,xc2;

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

int crop(VideoFilter *f, VideoFrame *frame)
{  
  ThisFilter *tf = (ThisFilter *)f;
  uint64_t *buf=(uint64_t *)frame->buf;
  int x,y; 
  const uint64_t UV_black=0x7f7f7f7f7f7f7f7fLL;
  
#ifdef TIME_FILTER
  struct timeval t1;
  gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */

  // Y Luma
  for(y = 0;y < (tf->yc1 * 2 * tf->height);y += 2) {
    buf[y]=0;
    buf[y + 1]=0;
  }
  for(y = tf->yc2 * 2 *tf->height;y < tf->Luma_size;y += 2) {
    buf[y]=0;
    buf[y + 1]=0;
  }
  // Y Chroma
  for(y = tf->Luma_size ;y < (tf->Luma_size + ((tf->height * tf->yc1 *4) >> 3));y++) {
    buf[y]= UV_black;
    buf[y + tf->Chroma_plane_size]= UV_black;
  }
  for(y = (tf->Luma_size + ((tf->height * tf->yc2 * 4) >>3));y < (tf->Luma_size + tf->Chroma_plane_size);y++) {
    buf[y]= UV_black;
    buf[y + tf->Chroma_plane_size]= UV_black;
  }
  // X Luma
  for(y = tf->yc1 * 2 * tf->height;y < tf->yc2 * 2 * tf->height;y += (tf->height >> 3)) {
    for(x = 0;x < tf->xc1;x++) {
      buf[y + x * 2]=0;
      buf[y + x * 2 + 1]=0;
    }
    for(x = tf->xc2;x < (tf->width >> 4);x++) {
      buf[y + x * 2]=0;
      buf[y + x * 2 + 1]=0;
    }
  }
  // X Chroma
  for(y = (tf->Luma_size+((tf->height * tf->yc1 * 4) >> 3)); y < (tf->Luma_size + ((tf->height * tf->yc2 * 4) >> 3)); y += (tf->height >> 4)) {
    for(x = 0;x < tf->xc1;x++) {
      buf[y + x]= UV_black;
      buf[y + x + tf->Chroma_plane_size]= UV_black;
    }
    for(x = tf->xc2;x < (tf->width >> 4);x++) {
      buf[y + x]= UV_black;
      buf[y + x + tf->Chroma_plane_size]= UV_black;
    }
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
	       "crop: filter timed at %3f frames/sec for %dx%d\n",
	       TIME_INTERVAL / tf->seconds, tf->width,
	       tf->height);
      tf->seconds = 0;
    }
#endif /* TIME_FILTER */
 
  return 0;
}

int cropMMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;  
  uint64_t *buf=(uint64_t *)frame->buf;
  int y,x; 
  const uint64_t UV_black=0x7f7f7f7f7f7f7f7fLL;

#ifdef TIME_FILTER
  struct timeval t1;
  gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */

  asm volatile("pxor %%mm0,%%mm0    \n\t"
	       "movq (%0),%%mm1    \n\t"
	       : : "r" (&UV_black)
	       );
  // Y Luma
  for(y = 0;y < (tf->yc1*tf->height << 1);y+=2) { 
    asm volatile("movq %%mm0, (%0)    \n\t"
		 "movq %%mm0, 8(%0)    \n\t"
		 : : "r" (buf+y)
		 );
  }
  for(y = tf->yc2*tf->height << 1;y < tf->Luma_size;y+=2) {
    asm volatile("movq %%mm0, (%0)    \n\t"
		 "movq %%mm0, 8(%0)    \n\t"
		 : : "r" (buf+y)
		 );
  }
  // Y Chroma
  for(y = tf->Luma_size ;y < (tf->Luma_size+((tf->height * tf->yc1 << 2) >> 3));y++) {
    asm volatile("movq %%mm1, (%0)    \n\t"
		 "movq %%mm1, (%1)    \n\t"
		 : : "r" (buf+y), "r" (buf + y + tf->Chroma_plane_size)
		 );
  }
  for(y = (tf->Luma_size+((tf->height*tf->yc2 << 2) >> 3));y < (tf->Luma_size + tf->Chroma_plane_size);y++) {
    asm volatile("movq %%mm1, (%0)    \n\t"
		 "movq %%mm1, (%1)    \n\t"
		 : : "r" (buf+y), "r" (buf + y + tf->Chroma_plane_size)
		 );
  }
  // X Luma
  for(y = (tf->yc1 * tf->height << 1);y < tf->yc2 * 2 * tf->height;y += (tf->height >> 3)) {
    for(x = 0;x < tf->xc1;x++) {
      asm volatile("movq %%mm0, (%0)    \n\t"
		   "movq %%mm0, 8(%0)    \n\t"
		   : : "r" (buf + y +(x << 1))
		   );
    }
    for(x = tf->xc2;x < (tf->width >> 4);x++) {
      asm volatile("movq %%mm0, (%0)    \n\t"
		   "movq %%mm0, 8(%0)    \n\t"
		   : : "r" (buf + y + (x << 1))
		   );
    }
  }
  // X Chroma
  for(y = (tf->Luma_size + ((tf->height * tf->yc1 * 4) >> 3)); y < (tf->Luma_size + ((tf->height * tf->yc2 * 4) >> 3)); y+=(tf->height >> 4)) {
    for(x=0;x < tf->xc1;x++) {
       
      asm volatile("movq %%mm1, (%0)    \n\t"
		   "movq %%mm1, (%1)    \n\t"
		   : : "r" (buf + y + x), "r" (buf + y + x + tf->Chroma_plane_size)
		   );
    }
    for(x= tf->xc2;x < (tf->width >> 4);x++) {
      asm volatile("movq %%mm1, (%0)    \n\t"
		   "movq %%mm1, (%1)    \n\t"
		   : : "r" (buf + y + x), "r" (buf + y + x + tf->Chroma_plane_size)
		   );
    }
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
	       "crop: filter timed at %3f frames/sec for %dx%d\n",
	       TIME_INTERVAL / tf->seconds, tf->width,
	       tf->height);
      tf->seconds = 0;
    }
#endif /* TIME_FILTER */

  return 0;
}


VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
  unsigned int Param1, Param2, Param3, Param4;
  ThisFilter *filter;

  if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
      fprintf(stderr,"crop: attempt to initialize with unsupported format\n");
      return NULL;
    }

  filter = malloc(sizeof(ThisFilter));

  if (filter == NULL)
    {
      fprintf(stderr,"Couldn't allocate memory for filter\n");
      return NULL;
    }


  filter->Luma_size = (*width * *height) / 8;
  filter->UV_size = *width * *height / 2 + filter->Luma_size;
  filter->width = *width;
  filter->height = *height;
  filter->Chroma_plane_size = (*height * *width / 4) / 8;

  if (options && (sscanf(options, "%u:%u:%u:%u", &Param1, &Param2, &Param3, &Param4) == 4)) { 
    filter->yc1 = (uint8_t) Param1;
    filter->xc1 = (uint8_t) Param2;
    filter->yc2 = (*height/16) - (uint8_t) Param3;
    filter->xc2 = (*width/16) - (uint8_t) Param4;
  }
  else {
    filter->yc1 = 1;
    filter->xc1 = 1;
    filter->yc2 = (*height/16) - 1;
    filter->xc2 = (*width/16) - 1;
  }
  
  
  if(mm_support() > MM_MMXEXT)
    filter->vf.filter = &cropMMX;
  else filter->vf.filter = &crop;

  
  filter->vf.cleanup = NULL;
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
      name:       "crop",
      descript:   "crops picture by macroblock (intervals",
      formats:    FmtList,
      libname:    NULL
    },
    FILT_NULL
  };
