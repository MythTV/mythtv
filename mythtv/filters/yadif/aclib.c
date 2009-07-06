#include "config.h"

/*
  aclib - advanced C library ;)
  This file contains functions which improve and expand standard C-library
  see aclib_template.c ... this file only contains runtime cpu detection and config options stuff
  runtime cpu detection by michael niedermayer (michaelni@gmx.at) is under GPL
*/
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#undef memcpy

#define BLOCK_SIZE 256
#define CONFUSION_FACTOR 0
//Feel free to fine-tune the above 2, it might be possible to get some speedup with them :)

#if ARCH_X86_64
#  define REG_a "rax"
#  define REG_b "rbx"
#  define MOVX  "movq"
#else
#  define REG_a "eax"
#  define REG_b "ebx"
#  define MOVX  "movl"
#endif

#define COMPILE_MMX
#define COMPILE_MMX2
#define COMPILE_3DNOW
#define COMPILE_SSE

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2

//MMX versions
#ifdef COMPILE_MMX
#undef RENAME
#define HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define RENAME(a) a ## _MMX
#include "aclib_template.c"
#endif

//MMX2 versions
#ifdef COMPILE_MMX2
#undef RENAME
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define RENAME(a) a ## _MMX2
#include "aclib_template.c"
#endif

//3DNOW versions
#ifdef COMPILE_3DNOW
#undef RENAME
#define HAVE_MMX
#undef HAVE_MMX2
#define HAVE_3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define RENAME(a) a ## _3DNow
#include "aclib_template.c"
#endif

//SSE versions (only used on SSE2 cpus)
#ifdef COMPILE_SSE
#undef RENAME
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#define HAVE_SSE
#define HAVE_SSE2
#define RENAME(a) a ## _SSE
#include "aclib_template.c"
#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
