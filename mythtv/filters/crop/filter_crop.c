/*
 * crop v 0.2
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as crop=top:left:bottom:right as number of 16 pixel blocks.
 * 
 */

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

static const char FILTER_NAME[] = "crop";

typedef struct ThisFilter
{
  VideoFilter vf;

  int Chroma_plane_size;
  int Luma_plane_size;
 
  int width;
  int height;

  int cropY1, cropY2;
  int cropC1, cropC2, cropC3;
  int xcrop1, xcrop2;
  int xcropYint, xcropCint, xcropend;
  TF_STRUCT;

} ThisFilter;

#ifdef i386

//From linearblend
#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

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
    if(rval==0)
      goto inteltest;
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
  } else if (ebx == 0x756e6547 &&
             edx == 0x54656e69 &&
             ecx == 0x3638784d) {
     /* Tranmeta Crusoe */
     cpuid(0x80000000, eax, ebx, ecx, edx);
     if ((unsigned)eax < 0x80000001)
       return 0;
     cpuid(0x80000001, eax, ebx, ecx, edx);
     if ((edx & 0x00800000) == 0)
       return 0;
     return MM_MMX;
  } else {
    return 0;
  }
}
#else // i386
int mm_support(void) { return 0; };
#endif

int crop(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;
  uint64_t *buf=(uint64_t *)frame->buf;
  int x,y;
  const uint64_t Y_black=0x1010101010101010LL;
  const uint64_t UV_black=0x8080808080808080LL;
  TF_VARS;
  
  TF_START;
  for(y = 0; y < tf->cropY1; y += 2) {// Y Luma
    buf[y] = Y_black;
    buf[y + 1] = Y_black;
  }

  for(y = tf->cropY2; y < tf->Luma_plane_size; y += 2) {
    buf[y] = Y_black;
    buf[y + 1] = Y_black;
  }
  
  for(y = tf->Luma_plane_size; y < tf->cropC1; y++) {// Y Chroma
    buf[y] = UV_black;
    buf[y + tf->Chroma_plane_size] = UV_black;
  }

  for(y = tf->cropC2; y < tf->cropC3; y++) {
    buf[y] = UV_black;
    buf[y + tf->Chroma_plane_size] = UV_black;
  }
 
  for(y = tf->cropY1; y < tf->cropY2; y += tf->xcropYint) { // X Luma
    for(x = 0; x <  tf->xcrop1; x++) {
      buf[y + (x << 1)] = Y_black;
      buf[y + (x << 1) + 1] = Y_black;
    }
    for(x = tf->xcrop2; x < tf->xcropend; x++) {
      buf[y + (x << 1)] = Y_black;
      buf[y + (x << 1) + 1] = Y_black;
    }
  }

  for(y = tf->cropC1; y < tf->cropC2; y+= tf->xcropCint) { // X Chroma
    for(x = 0; x <  tf->xcrop1; x++) {
      buf[y + x] = UV_black;
      buf[y + x + tf->Chroma_plane_size] = UV_black;
    }
    for(x = tf->xcrop2; x < tf->xcropend; x++) {
      buf[y + x] = UV_black;
      buf[y + x + tf->Chroma_plane_size] = UV_black;
    }
  }
 
  TF_END(tf, "Crop: ");
  return 0;
}

#ifdef i386
int cropMMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;  
  uint64_t *buf=(uint64_t *)frame->buf;
  int y,x; 
  const uint64_t Y_black=0x1010101010101010LL;
  const uint64_t UV_black=0x8080808080808080LL;
  TF_VARS;

  TF_START;

  asm volatile("movq (%1),%%mm0    \n\t"	       
	       "movq (%0),%%mm1    \n\t"
	       : : "r" (&UV_black), "r"(&Y_black));
  
  for(y = 0; y < tf->cropY1; y += 2) { // Y Luma
    asm volatile("movq %%mm0, (%0)    \n\t"
		 "movq %%mm0, 8(%0)    \n\t"
		 : : "r" (buf + y));
  }

  for(y = tf->cropY2; y < tf->Luma_plane_size; y += 2) {
    asm volatile("movq %%mm0, (%0)    \n\t"
		 "movq %%mm0, 8(%0)    \n\t"
		 : : "r" (buf + y));
  }

  for(y = tf->Luma_plane_size; y < tf->cropC1; y++) // Y Chroma
    asm volatile("movq %%mm1, (%0)    \n\t"
		 "movq %%mm1, (%1)    \n\t"
		 : : "r" (buf + y), "r" (buf + y + tf->Chroma_plane_size));

  for(y = tf->cropC2; y < tf->cropC3; y++)
    asm volatile("movq %%mm1, (%0)    \n\t"
		 "movq %%mm1, (%1)    \n\t"
		 : : "r" (buf + y), "r" (buf + y + tf->Chroma_plane_size));
 
  for(y = tf->cropY1; y < tf->cropY2; y += tf->xcropYint) { // X Luma
    for(x = 0; x < tf->xcrop1; x++)
      asm volatile("movq %%mm0, (%0)    \n\t"
		   "movq %%mm0, 8(%0)    \n\t"
		   : : "r" (buf + y + (x << 1)));
    for(x = tf->xcrop2; x < tf->xcropend; x++)
      asm volatile("movq %%mm0, (%0)    \n\t"
		   "movq %%mm0, 8(%0)    \n\t"
		   : : "r" (buf + y + (x << 1)));
  }

  for(y = tf->cropC1; y < tf->cropC2; y += tf->xcropCint) { // X Chroma
    for(x=0; x < tf->xcrop1; x++)
      asm volatile("movq %%mm1, (%0)    \n\t"
		   "movq %%mm1, (%1)    \n\t"
		   : : "r" (buf + y + x), "r" (buf + y + x + tf->Chroma_plane_size));
    for(x = tf->xcrop2; x < tf->xcropend; x++)
      asm volatile("movq %%mm1, (%0)    \n\t"
		   "movq %%mm1, (%1)    \n\t"
		   : : "r" (buf + y + x), "r" (buf + y + x + tf->Chroma_plane_size));
  }

  asm volatile("emms\n\t");

  TF_END(tf, "Crop: ");
  return 0;
}
#else // i386
int cropMMX(VideoFilter *f, VideoFrame *frame)
{ 
  fprintf(stderr,"cropMMX: attempt to use MMX version of crop on non-mmx system\n");
  return crop(f, frame);
}
#endif

VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
  unsigned int Param1, Param2, Param3, Param4;
  ThisFilter *filter;

  int yp1,yp2,xp1,xp2;

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

  filter->Luma_plane_size = (*width * *height) / 8;
  filter->Chroma_plane_size = (*height * *width / 4) / 8;
  filter->cropC3 = filter->Luma_plane_size + filter->Chroma_plane_size;
  filter->xcropend = *width / 16;
  filter->xcropYint = *width / 8;
  filter->xcropCint = *width / 16;

  if (options && (sscanf(options, "%u:%u:%u:%u", &Param1, &Param2, &Param3, &Param4) == 4)) { 
    yp1 = (uint8_t) Param1;
    yp2 = (uint8_t) Param3;
    xp1 = (uint8_t) Param2;
    xp2 = (uint8_t) Param4;
  }
  else {
    yp1 = 1;
    yp2 = 1;
    xp1 = 1;
    xp2 = 1;
  }

  // linear addresses of 8-byte-block planes
  filter->cropY1 = yp1 * *width * 2;
  filter->cropY2 = ((*height/16) - yp2) * *width * 2;
  filter->cropC1 = filter->Luma_plane_size+(*width * (yp1 * 4) / 8);
  filter->cropC2 = filter->Luma_plane_size + ((*width * (*height/16 - yp2) * 4) / 8);
  filter->xcrop1 = xp1;
  filter->xcrop2 = (*width/16) - xp2;
  
  if(mm_support() > MM_MMX) filter->vf.filter = &cropMMX;
  else filter->vf.filter = &crop;

  filter->vf.cleanup = NULL;
  TF_INIT(filter);

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
      descript:   "crops picture by macroblock intervals",
      formats:    FmtList,
      libname:    NULL
    },
    FILT_NULL
  };
