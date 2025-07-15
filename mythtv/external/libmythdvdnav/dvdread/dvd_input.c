/*
 * Copyright (C) 2002 Samuel Hocevar <sam@zoy.org>,
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"                  /* Required for HAVE_DVDCSS_DVDCSS_H */
#include <stdio.h>                               /* fprintf */
#include <stdlib.h>                              /* free */
#include <fcntl.h>                               /* open */
#include <unistd.h>                              /* lseek */

#include "dvdread/dvd_reader.h"      /* DVD_VIDEO_LB_LEN */
#include "dvd_input.h"
#include "libmythtv/io/mythiowrapper.h"
#include "mythdvdreadexp.h"

/* The function pointers that is the exported interface of this file. */
dvd_input_t (*dvdinput_open)  (const char *, void *, dvd_reader_stream_cb *);
int         (*dvdinput_close) (dvd_input_t);
int         (*dvdinput_seek)  (dvd_input_t, int, int);
int         (*dvdinput_title) (dvd_input_t, int);
int         (*dvdinput_read)  (dvd_input_t, void *, int, int);

#ifdef HAVE_DVDCSS_DVDCSS_H
/* linking to libdvdcss */
# include <dvdcss/dvdcss.h>
# define DVDcss_open_stream(a, b) \
    dvdcss_open_stream((void*)(a), (dvdcss_stream_cb*)(b))
# define DVDcss_open(a) dvdcss_open((char*)(a))
# define DVDcss_close   dvdcss_close
# define DVDcss_seek    dvdcss_seek
# define DVDcss_read    dvdcss_read
#else

/* dlopening libdvdcss */
# if defined(HAVE_DLFCN_H) && !defined(USING_BUILTIN_DLFCN)
#  include <dlfcn.h>
# else
#   if defined(WIN32)
/* Only needed on MINGW at the moment */
/*#    include "../msvc/contrib/dlfcn.c"*/
#    include "libmythbase/compat.h"
#   endif
# endif

#ifdef __APPLE__
# include <CoreFoundation/CFBundle.h>
#endif

typedef struct dvdcss_s *dvdcss_t;
typedef struct dvdcss_stream_cb dvdcss_stream_cb;
static dvdcss_t (*DVDcss_open_stream) (void *, dvdcss_stream_cb *);
static dvdcss_t (*DVDcss_open)  (const char *);
static int      (*DVDcss_close) (dvdcss_t);
static int      (*DVDcss_seek)  (dvdcss_t, int, int);
static int      (*DVDcss_read)  (dvdcss_t, void *, int, int);
#define DVDCSS_SEEK_KEY (1 << 1)
#endif

/* The DVDinput handle, add stuff here for new input methods. */
struct dvd_input_s {
  /* libdvdcss handle */
  dvdcss_t dvdcss;

  /* dummy file input */
  int fd;
};


/**
 * initialize and open a DVD (device or file or stream_cb)
 */
static dvd_input_t css_open(const char *target,
                            void *stream, dvd_reader_stream_cb *stream_cb)
{
  dvd_input_t dev;

  /* Allocate the handle structure */
  dev = malloc(sizeof(*dev));
  if(dev == NULL) {
    fprintf(stderr, "libdvdread: Could not allocate memory.\n");
    return NULL;
  }

  /* Really open it with libdvdcss */
  if(target)
      dev->dvdcss = DVDcss_open(target);
  else if(stream && stream_cb) {
#ifdef HAVE_DVDCSS_DVDCSS_H
      dev->dvdcss = DVDcss_open_stream(stream, (dvdcss_stream_cb *)stream_cb);
#else
      dev->dvdcss = DVDcss_open_stream ?
                    DVDcss_open_stream(stream, (dvdcss_stream_cb *)stream_cb) :
                    NULL;
#endif
  }
  if(dev->dvdcss == NULL) {
    fprintf(stderr, "libdvdread: Could not open %s with libdvdcss.\n", target);
    free(dev);
    return NULL;
  }

  return dev;
}

/**
 * seek into the device.
 */
static int css_seek(dvd_input_t dev, int blocks, int flags)
{
  return DVDcss_seek(dev->dvdcss, blocks, flags);
}

/**
 * set the block for the beginning of a new title (key).
 */
static int css_title(dvd_input_t dev, int block)
{
  return DVDcss_seek(dev->dvdcss, block, DVDCSS_SEEK_KEY);
}

/**
 * read data from the device.
 */
static int css_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
  return DVDcss_read(dev->dvdcss, buffer, blocks, flags);
}

/**
 * close the DVD device and clean up the library.
 */
static int css_close(dvd_input_t dev)
{
  int ret;

  ret = DVDcss_close(dev->dvdcss);

  free(dev);

  return ret;
}

/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t file_open(const char *target,
                             void *stream UNUSED,
                             dvd_reader_stream_cb *stream_cb UNUSED)
{
  dvd_input_t dev;

  if(target == NULL)
    return NULL;
  /* Allocate the library structure */
  dev = malloc(sizeof(*dev));
  if(dev == NULL) {
    fprintf(stderr, "libdvdread: Could not allocate memory.\n");
    return NULL;
  }

  /* Open the device */
#if !defined(__OS2__)
  dev->fd = MythFileOpen(target, O_RDONLY);
#else
  dev->fd = mythfile_open(target, O_RDONLY | O_BINARY);
#endif
  if(dev->fd < 0) {
    perror("libdvdread: Could not open input");
    free(dev);
    return NULL;
  }

  return dev;
}

/**
 * seek into the device.
 */
static int file_seek(dvd_input_t dev, int blocks, int flags)
{
  off_t pos;
  (void)flags;

  pos = MythFileSeek(dev->fd, (off_t)blocks * (off_t)DVD_VIDEO_LB_LEN, SEEK_SET);
  if(pos < 0) {
    return pos;
  }
  /* assert pos % DVD_VIDEO_LB_LEN == 0 */
  return (int) (pos / DVD_VIDEO_LB_LEN);
}

/**
 * set the block for the beginning of a new title (key).
 */
static int file_title(dvd_input_t dev UNUSED, int block UNUSED)
{
  return -1;
}

/**
 * read data from the device.
 */
static int file_read(dvd_input_t dev, void *buffer, int blocks,
		     int flags UNUSED)
{
  size_t len, bytes;

  len = (size_t)blocks * DVD_VIDEO_LB_LEN;
  bytes = 0;

  while(len > 0) {
    ssize_t ret = MythFileRead(dev->fd, ((char*)buffer) + bytes, len);

    if(ret < 0) {
      /* One of the reads failed, too bad.  We won't even bother
       * returning the reads that went OK, and as in the POSIX spec
       * the file position is left unspecified after a failure. */
      return ret;
    }

    if(ret == 0) {
      /* Nothing more to read.  Return all of the whole blocks, if any.
       * Adjust the file position back to the previous block boundary. */
      off_t over_read = -(bytes % DVD_VIDEO_LB_LEN);
      off_t pos = MythFileSeek(dev->fd, over_read, SEEK_CUR);
      if(pos % 2048 != 0)
        fprintf( stderr, "libdvdread: lseek not multiple of 2048! Something is wrong!\n" );
      return (int) (bytes / DVD_VIDEO_LB_LEN);
    }

    len -= ret;
    bytes += ret;
  }

  return blocks;
}

/**
 * close the DVD device and clean up.
 */
static int file_close(dvd_input_t dev)
{
  int ret;

  ret = MythfileClose(dev->fd);

  free(dev);

  return ret;
}


/**
 * Setup read functions with either libdvdcss or minimal DVD access.
 */
int dvdinput_setup(const char *path)
{
  void *dvdcss_library = NULL;

#ifdef HAVE_DVDCSS_DVDCSS_H
  /* linking to libdvdcss */
  dvdcss_library = &dvdcss_library;  /* Give it some value != NULL */

#else
  /* dlopening libdvdcss */

#ifdef __APPLE__
  #define CSS_LIB "libdvdcss.2.dylib"
#elif defined(WIN32)
  #define CSS_LIB "libdvdcss-2.dll"
#elif defined(__OS2__)
  #define CSS_LIB "dvdcss2.dll"
#else
  #define CSS_LIB "libdvdcss.so.2"
#endif
  dvdcss_library = dlopen(CSS_LIB, RTLD_LAZY);

#ifdef __APPLE__
  if (!dvdcss_library)
  {
    fprintf(stderr, "libdvdread: dlopen(%s) failed.\n", CSS_LIB);
    CFURLRef     appUrlRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFStringRef  macPath   = CFURLCopyFileSystemPath(appUrlRef,
                                                    kCFURLPOSIXPathStyle);
    static char *paths[]   = {
        "%s/Contents/Frameworks/%s",
        "%s/Contents/PlugIns/%s", // proper spelling, important on case sensitive fs
        "%s/Contents/Plugins/%s", // to be compatible with old bundler
        NULL
    };
    char         pathname[FILENAME_MAX];
    for (int i = 0; paths[i] != NULL && dvdcss_library == NULL; i++)
    {
        snprintf(pathname, FILENAME_MAX-1, paths[i],
                 CFStringGetCStringPtr(macPath, CFStringGetSystemEncoding()),
                 CSS_LIB);
        fprintf(stderr, "Trying %s\n", pathname);
        dvdcss_library = dlopen(pathname, RTLD_LAZY);
    }
    CFRelease(appUrlRef);
    CFRelease(macPath);
  }
#endif

  if(dvdcss_library != NULL) {
#if defined(__OpenBSD__) && !defined(__ELF__) || defined(__OS2__)
#define U_S "_"
#else
#define U_S
#endif
    DVDcss_open_stream = (dvdcss_t (*)(void *, dvdcss_stream_cb *))
      dlsym(dvdcss_library, U_S "dvdcss_open_stream");
    DVDcss_open = (dvdcss_t (*)(const char*))
      dlsym(dvdcss_library, U_S "dvdcss_open");
    DVDcss_close = (int (*)(dvdcss_t))
      dlsym(dvdcss_library, U_S "dvdcss_close");
    DVDcss_seek = (int (*)(dvdcss_t, int, int))
      dlsym(dvdcss_library, U_S "dvdcss_seek");
    DVDcss_read = (int (*)(dvdcss_t, void*, int, int))
      dlsym(dvdcss_library, U_S "dvdcss_read");

    if(dlsym(dvdcss_library, U_S "dvdcss_crack")) {
      fprintf(stderr,
              "libdvdread: Old (pre-0.0.2) version of libdvdcss found.\n"
              "libdvdread: You should get the latest version from "
              "http://www.videolan.org/\n" );
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
    } else if(!DVDcss_open || !DVDcss_close || !DVDcss_seek
              || !DVDcss_read) {
      fprintf(stderr,  "libdvdread: Missing symbols in %s, "
              "this shouldn't happen !\n", CSS_LIB);
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
    }
  }
#endif /* HAVE_DVDCSS_DVDCSS_H */

  // CSS isn't possible over the myth:// protocol
  if(strncmp(path, "myth://", 7) && dvdcss_library != NULL) {
    /*
    char *psz_method = getenv( "DVDCSS_METHOD" );
    char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
    fprintf(stderr, "DVDCSS_METHOD %s\n", psz_method);
    fprintf(stderr, "DVDCSS_VERBOSE %s\n", psz_verbose);
    */

    /* libdvdcss wrapper functions */
    dvdinput_open  = css_open;
    dvdinput_close = css_close;
    dvdinput_seek  = css_seek;
    dvdinput_title = css_title;
    dvdinput_read  = css_read;
    return 1;

  } else {
    fprintf(stderr, "libdvdread: Encrypted DVD support unavailable.\n");

    /* libdvdcss replacement functions */
    dvdinput_open  = file_open;
    dvdinput_close = file_close;
    dvdinput_seek  = file_seek;
    dvdinput_title = file_title;
    dvdinput_read  = file_read;
    return 0;
  }
}
