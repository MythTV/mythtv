/*
 * Copyright (C) 2002-2025 Samuel Hocevar <sam@zoy.org>,
 *                         Håkan Hjort <d95hjort@dtek.chalmers.se>
 *                         Jean-Baptiste Kempf <jb@videolan.org>
 *                         VideoLAN
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
#include <stdio.h>                   /* fprintf */
#include <stdlib.h>                  /* free */
#include <string.h>                  /* strerror */
#include <errno.h>
#include <limits.h>
#include <assert.h>

#if defined(_WIN32)
# include <winapifamily.h>
# if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#  if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x602
#   undef _WIN32_WINNT
#   define _WIN32_WINNT 0x602 /* LoadPackagedLibrary is Win8 APP Family */
#  endif
# endif
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "dvdread/dvd_reader.h"      /* DVD_VIDEO_LB_LEN */
#include "dvd_input.h"
#include "logger.h"


#ifdef HAVE_DVDCSS_DVDCSS_H
/* linking to libdvdcss */
# include <dvdcss/dvdcss.h>
# define DVDcss_open_stream(a, b) \
    dvdcss_open_stream((void*)(a), (dvdcss_stream_cb*)(b))
# define DVDcss_open(a) dvdcss_open((char*)(a))
# define DVDcss_close   dvdcss_close
# define DVDcss_seek    dvdcss_seek
# define DVDcss_read    dvdcss_read
/* linking cpxm functions*/
#ifdef HAVE_DVDCSS_DVDCPXM_H
# include <dvdcss/dvdcpxm.h>
# define DVDcpxm_read    dvdcpxm_read
# define DVDcpxm_init    dvdcpxm_init
#endif
#else

/* dlopening libdvdcss */
# if defined(HAVE_DLFCN_H)
#  include <dlfcn.h>
# else
#   if defined(_WIN32)
#    define dlsym(h, name)  (void*)GetProcAddress(h, name)
#    define dlclose(h)      FreeLibrary(h)
#   endif
# endif

typedef struct dvdcss_s *dvdcss_t;
typedef struct dvdcss_stream_cb dvdcss_stream_cb;
static dvdcss_t (*DVDcss_open_stream) (void *, dvdcss_stream_cb *);
static dvdcss_t (*DVDcss_open)  (const char *);
static int      (*DVDcss_close) (dvdcss_t);
static int      (*DVDcss_seek)  (dvdcss_t, int, int);
static int      (*DVDcss_read)  (dvdcss_t, void *, int, int);
#define DVDCSS_SEEK_KEY (1 << 1)
/* function to setup the cpxm struct */
#ifdef HAVE_DVDCSS_DVDCPXM_H
static int      (*DVDcpxm_read)  (dvdcss_t, void *, int, int);
static int      (*DVDcpxm_init)  (dvdcss_t, uint8_t* p_mkb);
#endif

#endif

/* The DVDinput handle, add stuff here for new input methods.
 * NOTE: All members of this structure must be initialized in dvd_input_New
 */
struct dvd_input_s {
  /* libdvdcss handle */
  dvdcss_t dvdcss;
  /* */
  void *priv;
  dvd_logger_cb *logcb;
  /* Current position as a block index, -1 after a failed read */
  int ipos;

  /* This variable keeps track of the current files stream_type,
   * and in turn determined the decryption method to use */
  /* DVD_A -> AOB */
  /* DVD_V -> VOB */
  /* DVD_VR -> VRO with cprm */
  dvd_type_t stream_type;

  /* stream input */
  dvd_reader_stream_cb *stream_cb;
  /* file input */
  dvd_reader_filesystem_h *fs;
  void *file;
};

static dvd_input_t dvd_input_New(void *priv, dvd_logger_cb *logcb)
{
  dvd_input_t dev = calloc(1, sizeof(*dev));
  if(dev)
  {
      dev->priv = priv;
      dev->logcb = logcb;
      dev->ipos = 0;
      dev->stream_type = DVD_V;

      /* Initialize all inputs to safe defaults */
      dev->dvdcss = NULL;
      dev->stream_cb = NULL;
      dev->fs = NULL;
      dev->file = NULL;
  }
  return dev;
}

/**
 * initialize and open a DVD (device or file or stream_cb)
 */
static dvd_input_t css_open(void *priv, dvd_logger_cb *logcb,
                            const char *target,
                            dvd_reader_stream_cb *stream_cb,
                            dvd_reader_filesystem_h *fs UNUSED)
{
  dvd_input_t dev;

  /* Allocate the handle structure */
  dev = dvd_input_New(priv, logcb);
  if(dev == NULL) {
    DVDReadLog(priv, logcb, DVD_LOGGER_LEVEL_ERROR,
               "Could not allocate memory.");
    return NULL;
  }

  /* Really open it with libdvdcss */
  if(target)
      dev->dvdcss = DVDcss_open(target);
  else if(priv && stream_cb) {
#ifdef HAVE_DVDCSS_DVDCSS_H
      dev->dvdcss = DVDcss_open_stream(priv, (dvdcss_stream_cb *)stream_cb);
#else
      dev->dvdcss = DVDcss_open_stream ?
                    DVDcss_open_stream(priv, (dvdcss_stream_cb *)stream_cb) :
                    NULL;
#endif
  }
  if(dev->dvdcss == NULL) {
    DVDReadLog(priv, logcb, DVD_LOGGER_LEVEL_ERROR,
               "Could not open %s with libdvdcss.", target);
    free(dev);
    return NULL;
  }

  return dev;
}

/**
 * seek into the device.
 */
static int css_seek(dvd_input_t dev, int blocks)
{
  return DVDcss_seek(dev->dvdcss, blocks, DVDINPUT_NOFLAGS);
}

/**
 * set the block for the beginning of a new title (key).
 */
static int css_title(dvd_input_t dev, int block)
{
#ifdef HAVE_DVDCSS_DVDCPXM_H
  if (dev->stream_type == DVD_A || dev->stream_type == DVD_VR)
    return DVDcss_seek(dev->dvdcss, block, DVDINPUT_NOFLAGS);
  else
#endif
  return DVDcss_seek(dev->dvdcss, block, DVDCSS_SEEK_KEY);
}

/**
 * read data from the device.
 */
static int css_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
#ifdef HAVE_DVDCSS_DVDCPXM_H
  if (dev->stream_type == DVD_A || dev->stream_type == DVD_VR)
    return DVDcpxm_read(dev->dvdcss, buffer, blocks, flags);
  else
#endif
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

#ifdef HAVE_DVDCSS_DVDCPXM_H
/**
 * Setup Datastructure.
 */
static int cpxm_init(dvd_input_t dev, uint8_t* p_mkb )
{
  return DVDcpxm_init(dev->dvdcss, p_mkb);
}
#endif /* CPXM */

/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t file_open(void *priv, dvd_logger_cb *logcb,
                             const char *target,
                             dvd_reader_stream_cb *stream_cb,
                             dvd_reader_filesystem_h *fs)
{
  dvd_input_t dev;

  /* Allocate the library structure */
  dev = dvd_input_New(priv, logcb);
  if(dev == NULL) {
    DVDReadLog(priv, logcb, DVD_LOGGER_LEVEL_ERROR, "Could not allocate memory.");
    return NULL;
  }

  /* Initialize with stream callback if it is specified */
  if (stream_cb) {
    if (!stream_cb->pf_read || !stream_cb->pf_seek) {
      DVDReadLog(priv, logcb, DVD_LOGGER_LEVEL_ERROR,
              "Stream callback provided but lacks of pf_read or pf_seek methods.");
      free(dev);
      return NULL;
    }
    dev->stream_cb = stream_cb;
    return dev;
  }

  /* Open the device */
  if(target == NULL) {
    free(dev);
    return NULL;
  }
  dev->fs = fs;
  dev->file = fs->file_open(fs, target);
  if(!dev->file) {
    char buf[256];
#if defined(HAVE_STRERROR_R)
    if(strerror_r(errno, buf, 256) != 0)
        *buf=0;
#elif defined(HAVE_STRERROR_S)
    if(strerror_s(buf, 256, errno) != 0)
        *buf=0;
#else
    *buf=0;
#endif
    DVDReadLog(priv, logcb, DVD_LOGGER_LEVEL_ERROR,
               "Could not open input: %s", buf);
    free(dev);
    return NULL;
  }

  return dev;
}

/**
 * seek into the device.
 */
static int file_seek(dvd_input_t dev, int blocks)
{
  int pos = -1;

  if(dev->ipos == blocks)
  {
    /* We are already in position */
    return blocks;
  }

  if (dev->stream_cb) {
    /* Returns 0 on successful completion and -1 on error */
    pos = dev->stream_cb->pf_seek(dev->priv, blocks * (uint64_t)DVD_VIDEO_LB_LEN);

    if (!pos) {
      dev->ipos = blocks;
    }
  } else {
    /* Returns the offset from the start of the file or -1 on error */
    dvd_off_t off = dev->fs->file_seek(dev->file, blocks * (dvd_off_t)DVD_VIDEO_LB_LEN, SEEK_SET);

    if (off >= 0) {
      dev->ipos = pos = (int)(off / DVD_VIDEO_LB_LEN);
    }
  }

  if(pos < 0) {
    return pos;
  }

  return dev->ipos;
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
  size_t len = blocks * (size_t)DVD_VIDEO_LB_LEN;
  ssize_t bytes = 0;
  int blocks_read = 0;

  while(len > 0) {
    ssize_t read_bytes = -1;

    /* Perform read based on the input type */
    if (dev->stream_cb) {
      /* Returns the number of bytes read or -1 on error */
      if (len > INT_MAX) {
        read_bytes = dev->stream_cb->pf_read(dev->priv, ((char*)buffer) + bytes, INT_MAX);
      } else {
        read_bytes = dev->stream_cb->pf_read(dev->priv, ((char*)buffer) + bytes, (int)len);
      }
    } else {
      /* Returns the number of bytes read or -1 on error */
      read_bytes = dev->fs->file_read(dev->file, ((char*)buffer) + bytes, len);
    }

    if(read_bytes < 0) {
      /* One of the reads failed, too bad.  We won't even bother
       * returning the reads that went OK, and as in the POSIX spec
       * the file position is left unspecified after a failure. */
      dev->ipos = -1;
      return -1;
    }

    if(read_bytes == 0) {
      /* Nothing more to read.  Return all of the whole blocks, if any.
       * Adjust the file position back to the previous block boundary. */
      int ret = file_seek(dev, dev->ipos + blocks_read);
      if(ret < 0)
        return ret;

      return blocks_read;
    }

    len -= read_bytes;
    bytes += read_bytes;
    blocks_read = (int)(bytes / DVD_VIDEO_LB_LEN);
  }

  dev->ipos += blocks_read;
  return blocks;
}

/**
 * close the DVD device and clean up.
 */
static int file_close(dvd_input_t dev)
{
  int ret = 0;

  /* close file if it was open */

  if (dev->file) {
    ret = dev->fs->file_close(dev->file);
  }

  free(dev);

  return ret;
}

/**
 * Setup read functions with either libdvdcss or minimal DVD access.
 */
int dvdinput_setup(dvd_reader_t *priv, dvd_logger_cb *logcb, dvd_type_t dvda_flag)
{
  void *dvdcss_library = NULL;

#ifdef HAVE_DVDCSS_DVDCPXM_H
  /* linking to libdvdcss */
  dvdcss_library = &dvdcss_library;  /* Give it some value != NULL */
#elif defined( HAVE_DVDCSS_DVDCSS_H )
  if (dvda_flag == DVD_V )
    dvdcss_library = &dvdcss_library;  /* Give it some value != NULL */
#else
  /* dlopening libdvdcss */

#ifdef __APPLE__
  #define CSS_LIB "libdvdcss.2.dylib"
#elif defined(_WIN32)
  #define CSS_LIB "libdvdcss-2.dll"
#elif defined(__OS2__)
  #define CSS_LIB "dvdcss2.dll"
#elif defined(__OpenBSD__)
  #define CSS_LIB "libdvdcss.so"
#else
  #define CSS_LIB "libdvdcss.so.2"
#endif

#define WIDEN_(x) L ## x
#define WIDEN(x) WIDEN_(x)

#ifdef __OS2__
#define U_S "_"
#else
#define U_S
#endif

/* Actually dlopen */
#if defined(_WIN32)
# if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    UINT em;
    /* First assume the dso/dll's required by -this- dso are sitting in the
     * same path or can be found in the usual places.  Failing that, let's
     * let that dso look in the apache root.
     */
    em = SetErrorMode(SEM_FAILCRITICALERRORS);
    dvdcss_library = LoadLibraryExA(CSS_LIB, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dvdcss_library)
    {
        SetLastError(0); // clear the last error
        dvdcss_library = LoadLibraryExA(CSS_LIB, NULL, 0);
    }
    SetErrorMode(em);
    SetLastError(0); // clear the last error
# else /* WINAPI_PARTITION_DESKTOP */
    dvdcss_library = LoadPackagedLibrary(WIDEN(CSS_LIB), 0);
# endif /* WINAPI_PARTITION_DESKTOP */
#else
  dvdcss_library = dlopen(CSS_LIB, RTLD_LAZY);
#endif

  /* Locate the functions, DVD_V or DVD_A */
  if(dvdcss_library != NULL) {
    /* functions should have the same template*/
    /* hybrid discs encrypt video tracks with css*/
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
    if (dvda_flag == DVD_A || dvda_flag == DVD_VR ) {
#ifdef HAVE_DVDCSS_DVDCPXM_H
      DVDcpxm_read = (int (*)(dvdcss_t, void*, int, int))
        dlsym(dvdcss_library, U_S "dvdcpxm_read");
      DVDcpxm_init = (int (*)(dvdcss_t, uint8_t *p_mkb))
        dlsym(dvdcss_library, U_S "dvdcpxm_init");
#else
      DVDReadLog(priv->priv, logcb, DVD_LOGGER_LEVEL_ERROR,
              "DVD-Audio headers not present, update the DVDCSS library");
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
#endif
    }
  }
  if(dvdcss_library != NULL) {
    if(dlsym(dvdcss_library, U_S "dvdcss_crack")) {
      DVDReadLog(priv->priv, logcb, DVD_LOGGER_LEVEL_ERROR,
                "Old (pre-0.0.2) version of libdvdcss found. "
                "libdvdread: You should get the latest version from "
                "https://www.videolan.org/" );
    } else if(!DVDcss_open || !DVDcss_close || !DVDcss_seek || !DVDcss_read) {
      DVDReadLog(priv->priv, logcb, DVD_LOGGER_LEVEL_ERROR,
                 "Missing symbols in %s, "
                "this shouldn't happen !", CSS_LIB);
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
#ifdef HAVE_DVDCSS_DVDCPXM_H
    } else if( (dvda_flag != DVD_V) && (!DVDcpxm_read || !DVDcpxm_init) ) {
      DVDReadLog(priv->priv, logcb, DVD_LOGGER_LEVEL_ERROR,
              "Missing symbols for DVD-Audio in %s, "
              "this shouldn't happen !", CSS_LIB);
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
#endif
    }
  }
#endif /* HAVE_DVDCSS_DVDCSS_H */

  if(dvdcss_library != NULL) {
    /*
    char *psz_method = getenv( "DVDCSS_METHOD" );
    char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
    fprintf(stderr, "DVDCSS_METHOD %s\n", psz_method);
    fprintf(stderr, "DVDCSS_VERBOSE %s\n", psz_verbose);
    */

    /* libdvdcss wrapper functions */
    priv->dvdinput_open  = css_open;
    priv->dvdinput_close = css_close;
    priv->dvdinput_seek  = css_seek;
    priv->dvdinput_title = css_title;
    priv->dvdinput_read  = css_read;

    /* additional setup function that must be run for DVD_A decryption */
    if (dvda_flag == DVD_A || dvda_flag == DVD_VR) {
#ifdef HAVE_DVDCSS_DVDCPXM_H
    priv->dvdinput_init  = cpxm_init;
#else
    priv->dvdinput_init  = NULL;
    assert(!"libdvdcss compiled without DVD-Audio (CPXM) support");
#endif
    }
    return 1;

  } else {
    DVDReadLog(priv->priv, logcb, DVD_LOGGER_LEVEL_WARN,
               "Encrypted DVD support unavailable.");

    dvdinput_setup_builtin(priv, logcb);
    return 0;
  }
}

/**
 * Setup read functions with the builtin libdvdread implementation (minimal DVD access without css).
 */
void dvdinput_setup_builtin(dvd_reader_t *priv, dvd_logger_cb *logcb)
{
    DVDReadLog(priv->priv, logcb, DVD_LOGGER_LEVEL_INFO,
               "Setting up builtin libdvdread implementation");

    /* libdvdcss replacement functions */
    priv->dvdinput_open  = file_open;
    priv->dvdinput_close = file_close;
    priv->dvdinput_seek  = file_seek;
    priv->dvdinput_title = file_title;
    priv->dvdinput_read  = file_read;
    priv->dvdinput_init  = NULL;
}

/* change the stream type, this will set the decryption method */
void dvdinput_set_stream(dvd_input_t dev, dvd_type_t decryption_type)
{
    dev->stream_type = decryption_type;
}
