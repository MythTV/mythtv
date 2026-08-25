/*
 * Copyright (C) 2001-2004 Billy Biggs <vektor@dumbterm.net>,
 *                         Håkan Hjort <d95hjort@dtek.chalmers.se>,
 *                         Björn Englund <d4bjorn@dtek.chalmers.se>
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

#include "config.h"
#include <time.h>           /* For the timing of dvdcss_title crack. */
#include <stdlib.h>         /* free */
#include <stdio.h>          /* fprintf */
#include <errno.h>          /* errno, EIN* */
#include <string.h>         /* memcpy, strlen */
#include <limits.h>         /* PATH_MAX */
#include <ctype.h>          /* isalpha */

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__bsdi__) || defined(__APPLE__)
# define SYS_BSD 1
#endif

#if defined(__sun)
# include <sys/mnttab.h>
#elif defined(__APPLE__)
# include <sys/param.h>
# include <sys/ucred.h>
# include <sys/mount.h>
#elif defined(SYS_BSD)
# include <fstab.h>
#elif defined(__linux__)
# include <mntent.h>
# include <paths.h>
#endif

#include "dvdread/dvd_udf.h"
#include "dvdread/dvd_reader.h"
#include "dvd_input.h"
#include "dvdread_internal.h"
#include "md5.h"
#include "dvdread/ifo_read.h"
#include "file/filesystem.h"

#define BYTES_TO_DVD_BLOCKS_CEIL(bytes) \
  (((uint64_t)(bytes) + DVD_VIDEO_LB_LEN - 1) / DVD_VIDEO_LB_LEN)

#if defined(_MSC_VER)
#define PATH_MAX _MAX_PATH
#endif

#define DEFAULT_UDF_CACHE_LEVEL 1

/* DVD-Audio allows up to 99 audio title sets. */
#define AUDIO_LINKED_VTS_MAX 100

struct dvd_reader_device_s {
  /* Basic information. */
  int isImageFile;

  /* Hack for keeping track of the css status.
   * 0: no css, 1: perhaps (need init of keys), 2: have done init */
  int css_state;
  int css_title; /* Last title that we have called dvdinpute_title for. */

  /* Information required for an image file. */
  dvd_input_t dev;

  /* Information required for a directory path drive. */
  char *path_root;

  /* Filesystem cache */
  int udfcache_level; /* 0 - turned off, 1 - on */
  void *udfcache;

  /* Cache of the video title set each audio title set borrows on a hybrid
   * disc, indexed by ATS number: 0 = not resolved yet, -1 = no link,
   * > 0 = the linked VTS number. See DVDAudioLinkedVTS(). */
  int audio_linked_vts[ AUDIO_LINKED_VTS_MAX ];
};

#define TITLES_MAX 9

struct dvd_file_s {
  /* Basic information. */
  dvd_reader_t *ctx;

  /* Hack for selecting the right css title. */
  int css_title;

  /* Zone the file's content lives in, selects the decryption scheme. */
  dvd_type_t stream_type;

  /* Information required for an image file. */
  uint32_t lb_start;
  uint32_t seek_pos;

  /* Information required for a directory path drive. */
  size_t title_sizes[ TITLES_MAX ];
  dvd_input_t title_devs[ TITLES_MAX ];

  /* Calculated at open-time, size in blocks. */
  ssize_t filesize;

  /* Cache of the dvd_file. If not NULL, the cache corresponds to the whole
   * dvd_file. Used only for IFO and BUP. */
  unsigned char *cache;
};

/**
 * Set the level of caching on udf
 * level = 0 (no caching)
 * level = 1 (caching filesystem info)
 */
int DVDUDFCacheLevel(dvd_reader_t *reader, int level)
{
  dvd_reader_device_t *dev = reader->rd;

  if(level > 0) {
    level = 1;
  } else if(level < 0) {
    return dev->udfcache_level;
  }

  dev->udfcache_level = level;

  return level;
}

void *GetUDFCacheHandle(dvd_reader_t *reader)
{
  dvd_reader_device_t *dev = reader->rd;

  return dev->udfcache;
}

void SetUDFCacheHandle(dvd_reader_t *reader, void *cache)
{
  dvd_reader_device_t *dev = reader->rd;

  dev->udfcache = cache;
}



/* Loop over all titles and call dvdcss_title to crack the keys. */
static int initAllCSSKeys( dvd_reader_t *ctx )
{
  dvd_reader_device_t *dvd = ctx->rd;
  time_t all_s, all_e;
  time_t t_s, t_e;
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint32_t start, len;
  int title;

  const char *nokeys_str = getenv("DVDREAD_NOKEYS");
  if(nokeys_str != NULL)
    return 0;

  Log2(ctx,"Attempting to retrieve all CSS keys" );
  Log2(ctx,"This can take a _long_ time, please be patient" );
  all_s = time(NULL);

  for( title = 0; title < 100; title++ ) {
    t_s = time(NULL);
    if( title == 0 ) {
      strcpy( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
    } else {
      sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 0 );
    }
    start = UDFFindFile( ctx, filename, &len );
    if( start != 0 && len != 0 ) {
      /* Perform CSS key cracking for this title. */
      Log3(ctx,"Get key for %s at 0x%08x",filename, start );
      if( ctx->dvdinput_title( dvd->dev, (int)start ) < 0 ) {
        Log1(ctx,"Error cracking CSS key for %s (0x%08x)", filename, start);
      }
      t_e = time(NULL);
      Log3(ctx,"Elapsed time %.0f", difftime(t_e, t_s) );
    }

    if( title == 0 ) continue;

    t_s = time(NULL);
    sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 1 );
    start = UDFFindFile( ctx, filename, &len );
    if( start == 0 || len == 0 ) break;

    /* Perform CSS key cracking for this title. */
    Log3(ctx,"Get key for %s at 0x%08x",filename, start );
    if( ctx->dvdinput_title( dvd->dev, (int)start ) < 0 ) {
      Log1(ctx,"Error cracking CSS key for %s (0x%08x)", filename, start);
    }
    t_e = time(NULL);
    Log3(ctx,"Elapsed time %.0f", difftime(t_e, t_s) );
  }
  title--;

  Log3(ctx,"Found %d VTS's", title );
  all_e = time(NULL);
  Log3(ctx,"Elapsed time %.0f", difftime(all_e, all_s) );

  return 0;
}



/**
 * Open a DVD image or block device file or use stream_cb functions.
 */
static dvd_reader_device_t *DVDOpenImageFile( dvd_reader_t *ctx,
                                              const char *location,
                                        dvd_reader_stream_cb *stream_cb,
                                       int have_css )
{
  dvd_reader_device_t *dvd;
  dvd_input_t dev;

  dev = ctx->dvdinput_open( ctx->priv, &ctx->logcb, location, stream_cb, ctx->fs );
  if( !dev ) {
    Log0(ctx,"Can't open %s for reading", location );
    return NULL;
  }

  dvd = calloc( 1, sizeof( dvd_reader_device_t ) );
  if( !dvd ) {
    ctx->dvdinput_close(dev);
    return NULL;
  }
  dvd->isImageFile = 1;
  dvd->dev = dev;

  dvd->udfcache_level = DEFAULT_UDF_CACHE_LEVEL;

  if( have_css ) {
    /* Only if DVDCSS_METHOD = title, a bit if it's disc or if
     * DVDCSS_METHOD = key but region mismatch. Unfortunately we
     * don't have that information. */

    dvd->css_state = 1; /* Need key init. */
  }

  return dvd;
}

static dvd_reader_device_t *DVDOpenPath( const char *path_root )
{
  dvd_reader_device_t *dvd;

  dvd = calloc( 1, sizeof( dvd_reader_device_t ) );
  if( !dvd ) return NULL;
  dvd->path_root = strdup( path_root );
  if(!dvd->path_root) {
    free(dvd);
    return NULL;
  }
  dvd->udfcache_level = DEFAULT_UDF_CACHE_LEVEL;

  return dvd;
}

#if defined(__sun)
/* /dev/rdsk/c0t6d0s0 (link to /devices/...)
   /vol/dev/rdsk/c0t6d0/??
   /vol/rdsk/<name> */
static char *sun_block2char( const char *path )
{
  char *new_path;

  /* Must contain "/dsk/" */
  if( !strstr( path, "/dsk/" ) ) return (char *) strdup( path );

  /* Replace "/dsk/" with "/rdsk/" */
  new_path = malloc( strlen(path) + 2 );
  if(!new_path) return NULL;
  strcpy( new_path, path );
  strcpy( strstr( new_path, "/dsk/" ), "" );
  strcat( new_path, "/rdsk/" );
  strcat( new_path, strstr( path, "/dsk/" ) + strlen( "/dsk/" ) );

  return new_path;
}
#endif

#if defined(SYS_BSD)
/* FreeBSD /dev/(r)(a)cd0c (a is for atapi), recommended to _not_ use r
   update: FreeBSD and DragonFly no longer uses the prefix so don't add it.
   OpenBSD /dev/rcd0c, it needs to be the raw device
   NetBSD  /dev/rcd0[d|c|..] d for x86, c (for non x86), perhaps others
   Darwin  /dev/rdisk0,  it needs to be the raw device
   BSD/OS  /dev/sr0c (if not mounted) or /dev/rsr0c ('c' any letter will do)
   returns a string allocated with strdup. It should be freed when no longer
   used. */
static char *bsd_block2char( const char *path )
{
#if defined(__FreeBSD__) || defined(__DragonFly__)
  return (char *) strdup( path );
#else
  char *new_path;

  /* If it doesn't start with "/dev/" or does start with "/dev/r" exit */
  if( strncmp( path, "/dev/",  5 ) || !strncmp( path, "/dev/r", 6 ) )
    return (char *) strdup( path );

  /* Replace "/dev/" with "/dev/r" */
  new_path = malloc( strlen(path) + 2 );
  if(!new_path) return NULL;
  strcpy( new_path, "/dev/r" );
  strcat( new_path, path + strlen( "/dev/" ) );

  return new_path;
#endif /* __FreeBSD__ || __DragonFly__ */
}
#endif

/* function is called on encrypted DVD-Audio discs */
/* with the cppm encryption scheme */
static uint8_t *cppm_get_mkb_or_backup( dvd_reader_t *ctx, int backup );

/* Since the dvd needs to be open in order to read the MKB ( media key block ) it was neccesary to split cpxm initilization into two steps, with one occuring after it's opened */
/* this should only be called if libdvdcss is available and if the disc type is DVD-Audio */
static int cpxm_init_condition( dvd_reader_t* ctx, dvd_type_t type, int have_css )
{
  if ( !ctx->rd->dev )
    return 0;

  if ( type == DVD_A && have_css )
  {
    uint8_t *p_mkb = NULL;
    p_mkb = cppm_get_mkb_or_backup( ctx, 0 );
    if ( !p_mkb )
      p_mkb = cppm_get_mkb_or_backup( ctx, 1 );
    if ( !p_mkb )
      Log2(ctx, "There is no MKB on this DVD-Audio disc, so there likely no encryption");
    return ctx->dvdinput_init( ctx->rd->dev, p_mkb );
  } else if ( type == DVD_VR && have_css ) {
    /* open ifo to supply decryption context */
    ifo_handle_t* ifo = ifoOpen( ctx, 0 );
    if( !ifo )
      return 0;
    if( !ifo->rtav_vmgi ) {
      ifoClose(ifo);
      return 0;
    }
    rtav_vmgi_t* rtav_vmgi = ifo->rtav_vmgi;
    /* pass the cprm title key to libdvdcss for decryption */
    int ret = ctx->dvdinput_init( ctx->rd->dev, rtav_vmgi->cprm_info.title_key );
    ifoClose(ifo);
    return ret;
  } else
    return 0;
}

/* not for the DVDOpenFiles filesystem which the caller owns */
static dvd_reader_t *DVDFreeContext( dvd_reader_t *ctx )
{
  if( ctx ) {
    if( ctx->fs )
      ctx->fs->close( ctx->fs );
    free( ctx );
  }
  return NULL;
}

/* like findDirFile but quiet, used to probe the disc type */
static int dir_has_file( dvd_reader_t *ctx, const char *subdir, const char *name )
{
  char path[ PATH_MAX + 1 ];
  dvd_dirent_t entry;
  int found = 0;
  void *dir;

  snprintf( path, sizeof(path), "%s/%s", ctx->rd->path_root, subdir );
  dir = ctx->fs->dir_open( ctx->fs, path );
  if( !dir )
    return 0;
  for( ;; ) {
    int r = ctx->fs->dir_read( dir, &entry );
    if( r != 0 )
      break;
    if( !strcasecmp( entry.d_name, name ) ) {
      found = 1;
      break;
    }
  }
  ctx->fs->dir_close( dir );
  return found;
}

/* like dir_has_file but also tries the lowercase spelling of subdir, as findDVDFile does */
static int disc_dir_has( dvd_reader_t *ctx, const char *subdir, const char *name )
{
  char low[ 16 ];
  size_t i;

  if( dir_has_file( ctx, subdir, name ) )
    return 1;
  for( i = 0; subdir[i] && i + 1 < sizeof(low); i++ )
    low[i] = tolower( (unsigned char)subdir[i] );
  low[i] = 0;
  return dir_has_file( ctx, low, name );
}

static dvd_reader_t *DVDOpenCommon( void *priv,
                                    const dvd_logger_cb *logcb,
                                    const char *ppath,
                                    dvd_reader_stream_cb *stream_cb,
                                    dvd_type_t type,
                                    dvd_reader_filesystem_h *fs )
{
  dvdstat_t fileinfo;
  int ret, have_css;
  char *dev_name = NULL;
  char *path = NULL, *new_path = NULL, *path_copy = NULL;
  dvd_reader_t *ctx = calloc(1, sizeof(*ctx));
  if(!ctx)
      return NULL;
  ctx->dvd_type = type;

  ctx->priv = priv;
  if(logcb)
    ctx->logcb = *logcb;

  /* Try to open libdvdcss or fall back to standard functions */
  have_css = dvdinput_setup( ctx, &ctx->logcb, type );

  // open files using the provided filesystem implementation
  if (fs != NULL && ppath != NULL)
  {
    ctx->fs = fs;
    dvdinput_setup_builtin(ctx, &ctx->logcb);
    /* If the path is not a directory, assume it is an image file. */
    if( ctx->fs->stat( ctx->fs, ppath, &fileinfo ) < 0 ||
        ( fileinfo.st_mode & DVD_S_IFMT ) != DVD_S_IFDIR )
    {
      ctx->rd = DVDOpenImageFile( ctx, ppath, NULL, have_css );
      if(!ctx->rd)
          return DVDFreeContext(ctx);
      cpxm_init_condition( ctx, type, have_css );
      return ctx;
    }
    ctx->rd = DVDOpenPath(ppath);
    if (!ctx->rd)
    {
      free(ctx);
      return NULL;
    }
    /* detect the disc type from the directory layout like DVDProbeType does */
    if( disc_dir_has( ctx, "AUDIO_TS", "AUDIO_TS.IFO" ) &&
        !disc_dir_has( ctx, "VIDEO_TS", "VIDEO_TS.IFO" ) )
      ctx->dvd_type = DVD_A;
    else if( disc_dir_has( ctx, "DVD_RTAV", "VR_MANGR.IFO" ) )
      ctx->dvd_type = DVD_VR;
    return ctx;
  }

  // create the internal filesystem
  ctx->fs = InitInternalFilesystem();
  if (!ctx->fs)
  {
    free(ctx);
    return NULL;
  }

  /* Try to open DVD using stream_cb functions */
  if( priv != NULL && stream_cb != NULL )
  {
    ctx->rd = DVDOpenImageFile( ctx, NULL, stream_cb, have_css );
    if(!ctx->rd)
        return DVDFreeContext(ctx);
    cpxm_init_condition( ctx, type, have_css );
    return ctx;
  }

  if( ppath == NULL )
    goto DVDOpen_error;

  path = strdup(ppath);
  if( path == NULL )
    goto DVDOpen_error;


#if defined(_WIN32) || defined(__OS2__)
  /* Strip off the trailing \ if it is not a drive */
  size_t len = strlen(path);
  if ((len > 1) &&
      (path[len - 1] == '\\')  &&
      (path[len - 2] != ':'))
  {
    path[len-1] = '\0';
  }
#endif

  ret = ctx->fs->stat(ctx->fs, path, &fileinfo);

  if( ret < 0 ) {

    /* maybe "host:port" url? try opening it with acCeSS library */
    if( strchr(path,':') ) {
      ctx->rd = DVDOpenImageFile( ctx, path, NULL, have_css );
      free(path);
      if(!ctx->rd)
          return DVDFreeContext(ctx);
      cpxm_init_condition( ctx, type, have_css );
      return ctx;
    }

    /* If we can't stat the file, give up */
    Log0(ctx, "Can't stat %s", path );
    perror("");
    goto DVDOpen_error;
  }

  /* First check if this is a block/char device or a file*/
  if( (fileinfo.st_mode & DVD_S_IFMT) == DVD_S_IFBLK ||
      (fileinfo.st_mode & DVD_S_IFMT) == DVD_S_IFCHR ||
      (fileinfo.st_mode & DVD_S_IFMT) == DVD_S_IFREG ) {

    /**
     * Block devices and regular files are assumed to be DVD-Video images.
     */
#if defined(__sun)
    dev_name = sun_block2char( path );
#elif defined(SYS_BSD)
    dev_name = bsd_block2char( path );
#else
    dev_name = strdup( path );
#endif
    if(!dev_name)
        goto DVDOpen_error;
    ctx->rd = DVDOpenImageFile( ctx, dev_name, NULL, have_css );
    free( dev_name );
    free(path);
    if(!ctx->rd)
        return DVDFreeContext(ctx);
    cpxm_init_condition( ctx, type, have_css );
    return ctx;
  } else if ((fileinfo.st_mode & DVD_S_IFMT) == DVD_S_IFDIR ) {
#if defined(SYS_BSD) && !defined(__APPLE__)
    struct fstab* fe;
#elif defined(__sun) || defined(__linux__)
    FILE *mntfile;
#endif

    /* XXX: We should scream real loud here. */
    if( !(path_copy = strdup( path ) ) )
      goto DVDOpen_error;

#ifndef _WIN32 /* win32 doesn't have realpath */
              /* Also WIN32 does not have symlinks, so we don't need this bit of code. */

    /* Resolve any symlinks and get the absolute dir name. */
    {
        new_path = realpath( path_copy, NULL );
        if( new_path == NULL ) {
          goto DVDOpen_error;
        }
        free(path_copy);
        path_copy = new_path;
        new_path = NULL;
    }
#endif

    /**
     * If we're being asked to open a directory, check if that directory
     * is the mount point for a DVD-ROM which we can use instead.
     */

    if( strlen( path_copy ) > 1 ) {
      if( path_copy[ strlen( path_copy ) - 1 ] == '/' ) {
        path_copy[ strlen( path_copy ) - 1 ] = '\0';
      }
    }

#if defined(_WIN32) || defined(__OS2__)
    if( strlen( path_copy ) > 9 ) {
      if( !strcasecmp( &(path_copy[ strlen( path_copy ) - 9 ]),
                       "\\video_ts"))
        path_copy[ strlen( path_copy ) - (9-1) ] = '\0';
    }
#endif
    if( strlen( path_copy ) > 9 ) {
      if( !strcasecmp( &(path_copy[ strlen( path_copy ) - 9 ]),
                       "/video_ts" ) ) {
        path_copy[ strlen( path_copy ) - 9 ] = '\0';
      }
    }

    if(path_copy[0] == '\0') {
      free( path_copy );
      if( !(path_copy = strdup( "/" ) ) )
        goto DVDOpen_error;
    }

#if defined(__APPLE__)
    struct statfs s[128];
    int r = getfsstat(NULL, 0, MNT_NOWAIT);
    if (r > 0) {
        if (r > 128)
            r = 128;
        r = getfsstat(s, r * sizeof(s[0]), MNT_NOWAIT);
        int i;
        for (i=0; i<r; i++) {
            if (!strcmp(path_copy, s[i].f_mntonname)) {
                dev_name = bsd_block2char(s[i].f_mntfromname);
                Log3(ctx, "Attempting to use device %s"
                          " mounted on %s for CSS authentication",
                        dev_name,
                        s[i].f_mntonname);
                ctx->rd = DVDOpenImageFile( ctx, dev_name, NULL, have_css );
                break;
            }
        }
    }
#elif defined(SYS_BSD)
    if( ( fe = getfsfile( path_copy ) ) ) {
      dev_name = bsd_block2char( fe->fs_spec );
      Log3(ctx, "Attempting to use device %s"
               " mounted on %s for CSS authentication",
               dev_name,
               fe->fs_file );
      ctx->rd = DVDOpenImageFile( ctx, dev_name, NULL, have_css );
    }
#elif defined(__sun)
    mntfile = fopen( MNTTAB, "r" );
    if( mntfile ) {
      struct mnttab mp;
      int res;

      while( ( res = getmntent( mntfile, &mp ) ) != -1 ) {
        if( res == 0 && !strcmp( mp.mnt_mountp, path_copy ) ) {
          dev_name = sun_block2char( mp.mnt_special );
          Log3(ctx, "Attempting to use device %s"
                   " mounted on %s for CSS authentication",
                   dev_name,
                   mp.mnt_mountp );
          ctx->rd = DVDOpenImageFile( ctx, dev_name, NULL, have_css );
          break;
        }
      }
      fclose( mntfile );
    }
#elif defined(__linux__)
    mntfile = fopen( _PATH_MOUNTED, "r" );
    if( mntfile ) {

#ifdef HAVE_GETMNTENT_R
      struct mntent *me, mbuf;
      char buf [8192];
      while( ( me = getmntent_r( mntfile, &mbuf, buf, sizeof(buf) ) ) ) {
#else
      struct mntent *me;
      while( ( me = getmntent( mntfile ) ) ) {
#endif
        if( !strcmp( me->mnt_dir, path_copy ) ) {
          Log3(ctx, "Attempting to use device %s"
                   " mounted on %s for CSS authentication",
                   me->mnt_fsname,
                   me->mnt_dir );
          ctx->rd = DVDOpenImageFile( ctx, me->mnt_fsname, NULL, have_css );
          dev_name = strdup(me->mnt_fsname);
          break;
        }
      }
      fclose( mntfile );
    }
#elif defined(_WIN32) || defined(__OS2__)
#ifdef __OS2__
    /* Use DVDOpenImageFile() only if it is a drive */
    if(isalpha(path[0]) && path[1] == ':' &&
        ( !path[2] ||
          ((path[2] == '\\' || path[2] == '/') && !path[3])))
#endif
    ctx->rd = DVDOpenImageFile( ctx, path, NULL, have_css );
#endif

#if !defined(_WIN32) && !defined(__OS2__)
    if( !dev_name ) {
      Log0(ctx, "Couldn't find device name." );
    } else if( !ctx->rd ) {
      Log0(ctx, "Device %s inaccessible, "
                "CSS authentication not available.", dev_name );
    }
#else
    if( !ctx->rd ) {
        Log0(ctx, "Device %s inaccessible, "
                 "CSS authentication not available.", path );
    }
#endif

    free( dev_name );
    dev_name = NULL;
    free( path_copy );
    path_copy = NULL;

    /**
     * If we've opened a drive, just use that.
     */
    if(ctx->rd)
    {
        free(path);
        cpxm_init_condition( ctx, type, have_css );
        return ctx;
    }
    /**
     * Otherwise, we now try to open the directory tree instead.
     */
    ctx->rd = DVDOpenPath( path );
    free( path );
    if(!ctx->rd)
        return DVDFreeContext(ctx);
    cpxm_init_condition( ctx, type, have_css );
    return ctx;
  }

DVDOpen_error:
  /* If it's none of the above, screw it. */
  Log0( ctx, "Could not open %s", path );
  free( path );
  free( path_copy );
  free( new_path );
  return DVDFreeContext( ctx );
}

/* opens the disc temporarily to peek at the file structure
 * helps determine how to treat the disc (as DVD-VR, DVD-Audio or DVD-Video)
 */
static dvd_type_t DVDProbeType( const char *ppath, void *stream,
                                dvd_reader_stream_cb *stream_cb  )
{
  /* set DVD_V as the fallback */
  dvd_type_t ret =  DVD_V;
  int have_css;
  uint32_t HAS_AUDIO_TS = 0, HAS_VIDEO_TS = 0, HAS_DVD_RTAV = 0;

  /* open the disc temporarily for probing */
  dvd_reader_t *ctx = calloc(1, sizeof(*ctx));
  if(!ctx)
    return ret;

  /* the builtin input reads through ctx->fs so set it before probing */
  ctx->fs = InitInternalFilesystem();
  if(!ctx->fs) {
    free(ctx);
    return ret;
  }

  have_css = dvdinput_setup( ctx, &ctx->logcb, DVD_V );

  if (stream && stream_cb)
    ctx->rd = DVDOpenImageFile( ctx, NULL, stream_cb, have_css );
  else if (ppath)
    ctx->rd = DVDOpenImageFile( ctx, ppath, NULL, have_css );

  if (!ctx->rd) {
    DVDFreeContext(ctx);
    return ret;
  }

  /* check for ifos */
  UDFFindFile( ctx, "/AUDIO_TS/AUDIO_TS.IFO", &HAS_AUDIO_TS );
  UDFFindFile( ctx, "/DVD_RTAV/VR_MANGR.IFO", &HAS_DVD_RTAV );
  UDFFindFile( ctx, "/VIDEO_TS/VIDEO_TS.IFO", &HAS_VIDEO_TS );

  /* if the disc only has dvd-a, play that.
   * if the disc has dvd-vr, play that 
   * otherwise default to dvd-v */
  if ( HAS_AUDIO_TS && !HAS_VIDEO_TS )
    ret = DVD_A;
  else if ( HAS_DVD_RTAV )
    ret = DVD_VR;

  DVDClose(ctx);

  return ret;
}

dvd_reader_t *DVDOpen( const char *ppath )
{
  dvd_type_t type_flag = DVDProbeType( ppath, NULL, NULL );
  return DVDOpenCommon( NULL, NULL, ppath, NULL, type_flag, NULL );
}

dvd_reader_t *DVDOpenStream( void *stream,
                             dvd_reader_stream_cb *stream_cb )
{
  dvd_type_t type_flag = DVDProbeType( NULL, stream, stream_cb );
  return DVDOpenCommon( stream, NULL, NULL, stream_cb, type_flag, NULL );
}

dvd_reader_t *DVDOpen2( void *priv, const dvd_logger_cb *logcb,
                        const char *ppath)
{
  dvd_type_t type_flag = DVDProbeType( ppath, NULL, NULL );
  return DVDOpenCommon( priv, logcb, ppath, NULL, type_flag, NULL );
}

dvd_reader_t *DVDOpenStream2( void *priv, const dvd_logger_cb *logcb,
                              dvd_reader_stream_cb *stream_cb )
{
  /* Fix: Pass NULL for stream, do NOT pass priv as stream */
  dvd_type_t type_flag = DVDProbeType( NULL, NULL, stream_cb );
  return DVDOpenCommon( priv, logcb, NULL, stream_cb, type_flag, NULL );
}

dvd_reader_t *DVDOpenAudio( void *priv, const dvd_logger_cb *logcb,
                            const char *ppath )
{
  return DVDOpenCommon( priv, logcb, ppath, NULL, DVD_A, NULL );
}

dvd_reader_t *DVDOpenStreamAudio( void *priv, const dvd_logger_cb *logcb,
                                  dvd_reader_stream_cb *stream_cb )
{
  return DVDOpenCommon( priv, logcb, NULL, stream_cb, DVD_A, NULL );
}

dvd_reader_t *DVDOpenVideoRecording( void *priv, const dvd_logger_cb *logcb,
                                     const char *ppath )
{
  return DVDOpenCommon( priv, logcb, ppath, NULL, DVD_VR, NULL );
}

dvd_reader_t *DVDOpenStreamVideoRecording( void *priv, const dvd_logger_cb *logcb,
                                           dvd_reader_stream_cb *stream_cb )
{
  return DVDOpenCommon( priv, logcb, NULL, stream_cb, DVD_VR, NULL );
}

dvd_reader_t *DVDOpenFiles( void *priv, const dvd_logger_cb *logcb,
                              const char *ppath, dvd_reader_filesystem_h *fs)
{
    return DVDOpenCommon( priv, logcb, ppath, NULL, DVD_V, fs );
}

void DVDClose( dvd_reader_t *dvd )
{
  if( dvd ) {
    if( dvd->rd ) {
      if( dvd->rd->dev ) dvd->dvdinput_close( dvd->rd->dev );
      if( dvd->rd->path_root ) free( dvd->rd->path_root );
      if( dvd->rd->udfcache ) FreeUDFCache( dvd->rd->udfcache );
      free( dvd->rd );
    }
    if (dvd->fs) {
      dvd->fs->close(dvd->fs);
    }
    free( dvd );
  }
}

/* The decryption scheme follows the zone a file lives in, which on a hybrid
 * disc is not always the zone implied by the disc type. */
static dvd_type_t DVDFileZone( dvd_reader_t *ctx, const char *filename )
{
  if( !strncmp( filename, "/VIDEO_TS/", 10 ) )
    return DVD_V;
  if( !strncmp( filename, "/AUDIO_TS/", 10 ) )
    return DVD_A;
  if( !strncmp( filename, "/DVD_RTAV/", 10 ) )
    return DVD_VR;
  return ctx->dvd_type;
}

/**
 * Open an unencrypted file on a DVD image file.
 */
static dvd_file_t *DVDOpenFileUDF( dvd_reader_t *ctx, const char *filename,
                                   int do_cache )
{
  uint32_t start, len;
  dvd_file_t *dvd_file;

  start = UDFFindFile( ctx, filename, &len );
  if( !start ) {
    Log0( ctx, "DVDOpenFileUDF:UDFFindFile %s failed", filename );
    return NULL;
  }

  dvd_file = calloc( 1, sizeof( dvd_file_t ) );
  if( !dvd_file ) {
    Log0( ctx, "DVDOpenFileUDF:malloc failed" );
    return NULL;
  }
  dvd_file->ctx = ctx;
  dvd_file->stream_type = DVDFileZone( ctx, filename );
  dvd_file->lb_start = start;
  dvd_file->filesize = BYTES_TO_DVD_BLOCKS_CEIL(len);

  /* Read the whole file in cache (unencrypted) if asked and if it doesn't
   * exceed 128KB */
  if( do_cache && len < 64 * DVD_VIDEO_LB_LEN ) {
    int ret;
    size_t cache_bytes = (size_t)dvd_file->filesize * DVD_VIDEO_LB_LEN;

    dvd_file->cache = malloc( cache_bytes );
    if( !dvd_file->cache )
        return dvd_file;

    ret = InternalUDFReadBlocksRaw( ctx, dvd_file->lb_start,
                                    dvd_file->filesize, dvd_file->cache,
                                    DVDINPUT_NOFLAGS );
    if( ret != dvd_file->filesize ) {
        free( dvd_file->cache );
        dvd_file->cache = NULL;
    }
  }

  return dvd_file;
}

/**
 * Searches for <file> in directory <path>, ignoring case.
 * Returns 0 and full filename in <filename>.
 *     or -1 on file not found.
 *     or -2 on path not found.
 */
static int findDirFile(dvd_reader_t *ctx, const char *path, const char *file, char *filename )
{
  dvd_dirent_t entry;
  void *dir = ctx->fs->dir_open(ctx->fs, path);
  if( !dir ) {
    Log0(ctx, "findDirFile: Could not open dir %s ", path);
    return -2;
  }

  int ret = -1;
  for( ;; ) {
    int result = ctx->fs->dir_read(dir, &entry);
    if( result < 0 ) {
      Log0(ctx, "findDirFile: Error reading dir %s (error: %d)", path, result);
      break;
    }
    if( result > 0 )
      break;
    if( !strcasecmp( entry.d_name, file ) ) {
      sprintf( filename, "%s%s%s", path,
               ( ( path[ strlen( path ) - 1 ] == '/' ) ? "" : "/" ),
               entry.d_name );
      ret = 0;
      break;
    }
  }

  ctx->fs->dir_close(dir);
  return ret;
}

static int findDVDFile( dvd_reader_t *dvd, const char *file, char *filename )
{
  const char *nodirfile;
  const char *subdir;
  int ret;

  /* Strip off the directory for our search. An explicit directory selects
   * the zone to search in (a hybrid disc has several), otherwise the zone
   * is implied by the disc type. */
  if( !strncasecmp( "/VIDEO_TS/", file, 10 ) ) {
    nodirfile = &(file[ 10 ]);
    subdir = "VIDEO_TS";
  } else if( !strncasecmp( "/AUDIO_TS/", file, 10 ) ) {
    nodirfile = &(file[ 10 ]);
    subdir = "AUDIO_TS";
  /* DVD-VR check */
  } else if ( !strncasecmp( "/DVD_RTAV/", file, 10 ) ) {
    nodirfile = &(file[ 10 ]);
    subdir = "DVD_RTAV";
  } else {
    nodirfile = file;
    subdir = dvd->dvd_type == DVD_VR ? "DVD_RTAV" :
             dvd->dvd_type == DVD_A ? "AUDIO_TS" : "VIDEO_TS";
  }

  ret = findDirFile(dvd, dvd->rd->path_root, nodirfile, filename );
  if( ret < 0 ) {
    char video_path[ PATH_MAX + 1 ];
    char lower[ 16 ];
    int i;

    /* Try also with adding the path, just in case. */
    sprintf( video_path, "%s/%s/", dvd->rd->path_root, subdir );
    ret = findDirFile( dvd, video_path, nodirfile, filename );
    if( ret < 0 ) {
      /* Try with the path, but in lower case. */
      for( i = 0; subdir[ i ]; i++ )
        lower[ i ] = tolower( (unsigned char)subdir[ i ] );
      lower[ i ] = '\0';
      sprintf( video_path, "%s/%s/", dvd->rd->path_root, lower );
      ret = findDirFile( dvd, video_path, nodirfile, filename );
      if( ret < 0 ) {
        return 0;
      }
    }
  }

  return 1;
}

/**
 * Open an unencrypted file from a DVD directory tree.
 */
static dvd_file_t *DVDOpenFilePath( dvd_reader_t *ctx, const char *filename )
{
  char full_path[ PATH_MAX + 1 ];
  dvd_file_t *dvd_file;
  dvdstat_t fileinfo;
  dvd_input_t dev;

  /* Get the full path of the file. */
  if( !findDVDFile( ctx, filename, full_path ) ) {
    Log0(ctx, "DVDOpenFilePath:findDVDFile %s failed", filename );
    return NULL;
  }

  dev = ctx->dvdinput_open( ctx->priv, &ctx->logcb, full_path, NULL, ctx->fs );
  if( !dev ) {
    Log0(ctx, "DVDOpenFilePath:dvdinput_open %s failed", full_path );
    return NULL;
  }

  dvd_file = calloc( 1, sizeof( dvd_file_t ) );
  if( !dvd_file ) {
    Log0(ctx, "DVDOpenFilePath:dvd_file malloc failed" );
    ctx->dvdinput_close(dev);
    return NULL;
  }
  dvd_file->ctx = ctx;
  dvd_file->stream_type = DVDFileZone( ctx, filename );

  if (ctx->fs->stat(ctx->fs, full_path, &fileinfo) < 0) {
    Log0(ctx, "Can't stat() %s.", filename );
    free( dvd_file );
    ctx->dvdinput_close( dev );
    return NULL;
  }
  dvd_file->title_sizes[ 0 ] = BYTES_TO_DVD_BLOCKS_CEIL(fileinfo.size);
  dvd_file->title_devs[ 0 ] = dev;
  dvd_file->filesize = dvd_file->title_sizes[ 0 ];

  return dvd_file;
}

static uint8_t *cppm_get_mkb_or_backup( dvd_reader_t *ctx, int backup )
{
  uint8_t* p_mkb;
  dvd_file_t* mkb_file;
  char filename[ MAX_UDF_FILE_NAME_LEN ];

  dvd_reader_device_t *dvd = ctx->rd;
  switch( backup )
  {
    case 0:
      strcpy( filename,  "/AUDIO_TS/DVDAUDIO.MKB" );
      break;
    case 1:
      strcpy( filename,  "/AUDIO_TS/DVDAUDIO.BUP" );
      break;
  }

  uint32_t len; 

  /* exits early if there is no MKB file */
  if( dvd->isImageFile ) 
  {
    if( !UDFFindFile( ctx, filename, &len ) ) return NULL;
    mkb_file= DVDOpenFileUDF( ctx, filename, 0 );
  } else
  {
    if( !findDVDFile( ctx, filename, filename ) ) return NULL;
    mkb_file= DVDOpenFilePath( ctx, filename );
  }

  if ( !mkb_file )
    return NULL;

  p_mkb = malloc( mkb_file->filesize * DVD_VIDEO_LB_LEN );
  if ( !p_mkb ) {
    DVDCloseFile( mkb_file );
    return NULL;
  }

  if ( !DVDReadBytes( mkb_file, p_mkb, mkb_file->filesize * DVD_VIDEO_LB_LEN ) )
  {
    free( p_mkb );
    DVDCloseFile( mkb_file );
    return NULL;
  }

  /* checking header */
  if( ( !memcmp( p_mkb, "DVDAUDIO.MKB", 12 ) && !backup )
        || ( !memcmp( p_mkb, "DVDAUDIO.BUP", 12 ) && backup ) ) {
    free( p_mkb );
    DVDCloseFile( mkb_file );
    return NULL;
  }

  DVDCloseFile( mkb_file );
  return p_mkb;
}

/* ts_type names the title set to open: it matches ctx->dvd_type except for
 * a hybrid DVD-Audio title set that borrows the title VOBs of a video title
 * set, which is opened with ts_type DVD_V. */
static dvd_file_t *DVDOpenVOBUDF( dvd_reader_t *ctx, int title, int menu,
                                  dvd_type_t ts_type )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint32_t start, len;
  dvd_file_t *dvd_file;
  /* the zone selects the decryption method:
   * DVD_V = VOB with CSS, DVD_A = AOB with CPPM, DVD_VR = VRO with CPRM */
  dvd_type_t stream_type = ts_type;

  if ( ts_type == DVD_VR && menu )
    return NULL;

  if ( ts_type == DVD_VR ) {
    sprintf( filename, "/DVD_RTAV/VR_MOVIE.VRO" );
  } else if( title == 0 ) {
    sprintf( filename, "/%s_TS/%s_TS.VOB", DVD_TYPE_STRING( ts_type ), DVD_TYPE_STRING( ts_type ) );
  } else if(!menu) {
    /* DVD Content - Tracks/Chapters  */
    sprintf( filename, "/%s_TS/%cTS_%02d_1.%cOB", DVD_TYPE_STRING( ts_type ),
            STREAM_TYPE_STRING( ts_type ), title, STREAM_TYPE_STRING( ts_type ) );
  } else {
    if ( ts_type == DVD_V )
      /* DVD_Video title menu */
      sprintf( filename, "/VIDEO_TS/VTS_%02d_0.VOB", title );
    else if ( ts_type == DVD_A )
      /* DVD_Audio title menu */
      sprintf( filename, "/AUDIO_TS/AUDIO_SV.VOB" );
  }

  start = UDFFindFile( ctx, filename, &len );
  if( start == 0 ) return NULL;

  dvd_file = calloc( 1, sizeof( dvd_file_t ) );
  if( !dvd_file ) return NULL;
  dvd_file->ctx = ctx;

  /* css vars not used in CPXM */
  if( stream_type == DVD_V )
      /*Hack*/ dvd_file->css_title = title << 1 | menu;

  dvd_file->stream_type = stream_type;
  dvd_file->lb_start = start;
  dvd_file->filesize = BYTES_TO_DVD_BLOCKS_CEIL(len);

  /* Calculate the complete file size for every file in the VOBS, AOBS */
  /* DVD-VR uses UDF 2.0 which allows for larger file sizes, 1GB limit does not exist */
  if( !menu && ts_type != DVD_VR ) {
    int cur;

    for( cur = 2; cur < 10; cur++ ) {
      sprintf( filename, "/%s_TS/%cTS_%02d_%d.%cOB", DVD_TYPE_STRING( ts_type ),
              STREAM_TYPE_STRING( ts_type ), title, cur, STREAM_TYPE_STRING( ts_type ) );
      if( !UDFFindFile( ctx, filename, &len ) ) break;
      dvd_file->filesize += BYTES_TO_DVD_BLOCKS_CEIL(len);
    }
  }

  /* Set the stream type before cracking keys: initAllCSSKeys() requests the
   * title keys on this same device and css_title() keys off its stream type. */
  dvdinput_set_stream( ctx->rd->dev, stream_type );

  if( stream_type == DVD_V && ctx->rd->css_state == 1 /* Need key init */ ) {
    initAllCSSKeys( ctx );
    ctx->rd->css_state = 2;
  }

  return dvd_file;
}

/* see DVDOpenVOBUDF for the ts_type semantics */
static dvd_file_t *DVDOpenVOBPath( dvd_reader_t *ctx, int title, int menu,
                                   dvd_type_t ts_type )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  char full_path[ PATH_MAX + 1 ];
  dvdstat_t fileinfo;
  dvd_file_t *dvd_file;
  /* DVD_V = VOB with css, DVD_A = AOB with CPPM, DVD_VR = VRO with CPRM */
  dvd_type_t stream_type = ts_type;

  dvd_file = calloc( 1, sizeof( dvd_file_t ) );
  if( !dvd_file ) return NULL;
  dvd_file->ctx = ctx;

  /* DVD-VR has no menu vobs */
  if ( ts_type == DVD_VR && menu ) {
    free( dvd_file );
    return NULL;
  }


  /* css vars aren't used in CPXM */
  if ( stream_type == DVD_V )
  /*Hack*/ dvd_file->css_title = title << 1 | menu;

  dvd_file->stream_type = stream_type;

  if( menu ) {
    dvd_input_t dev;

    if( title == 0 ) {
      /* the root menu is AUDIO_TS.VOB or VIDEO_TS.VOB */
      sprintf(filename, "%s_TS.VOB", DVD_TYPE_STRING( ts_type ) );
    } else {
      /* there are no ATS_%02i_0.AOB's */
      if ( ts_type == DVD_V )
        sprintf( filename, "VTS_%02i_0.VOB", title );
      else
        /* Remaining title menus would be in the still videos VOB */
        sprintf( filename, "AUDIO_SV.VOB" );
    }
    if( !findDVDFile( ctx, filename, full_path ) ) {
      free( dvd_file );
      return NULL;
    }

    dev = ctx->dvdinput_open( ctx->priv, &ctx->logcb, full_path, NULL, ctx->fs );
    if( dev == NULL ) {
      free( dvd_file );
      return NULL;
    }

    if (ctx->fs->stat(ctx->fs, full_path, &fileinfo) < 0) {
      Log0(ctx, "Can't stat() %s.", filename );
      ctx->dvdinput_close(dev);
      free( dvd_file );
      return NULL;
    }
    dvd_file->title_sizes[ 0 ] = BYTES_TO_DVD_BLOCKS_CEIL(fileinfo.size);
    dvd_file->title_devs[ 0 ] = dev;
    dvdinput_set_stream( dev, stream_type );
    ctx->dvdinput_title( dvd_file->title_devs[0], 0);
    dvd_file->filesize = dvd_file->title_sizes[ 0 ];

  } else {

    /* DVD-VR uses UDF 2.0 which allows for larger file sizes, 1GB limit does not exist */
    /* will only return one file, treating it as a single fragment with one title dev */
    if ( ts_type == DVD_VR ) {
      sprintf( filename, "VR_MOVIE.VRO");

      if( !findDVDFile( ctx, filename, full_path ) ) {
        free( dvd_file );
        return NULL;
      }
      if (ctx->fs->stat(ctx->fs, full_path, &fileinfo) < 0) {
        Log0(ctx, "Can't stat() %s.", filename );
        free( dvd_file );
        return NULL;
      }

      dvd_file->title_sizes[ 0 ] = BYTES_TO_DVD_BLOCKS_CEIL(fileinfo.size);
      dvd_file->title_devs[ 0 ] = ctx->dvdinput_open( ctx->priv, &ctx->logcb, full_path, NULL, ctx->fs );

      if( !dvd_file->title_devs[ 0 ] ) {
        free( dvd_file );
        return NULL;
      }

      dvdinput_set_stream( dvd_file->title_devs[ 0 ], stream_type );

      /* Initialize CPRM */
      if( ctx->dvdinput_init )
        ctx->dvdinput_init( dvd_file->title_devs[ 0 ], NULL );

      ctx->dvdinput_title( dvd_file->title_devs[ 0 ], 0 );
      dvd_file->filesize = dvd_file->title_sizes[ 0 ];

    } else {

      int i;

      for( i = 0; i < TITLES_MAX; ++i ) {

        /* the zone directory matters when opening a title set of the other
         * zone of a hybrid disc */
        sprintf( filename, "/%s_TS/%cTS_%02i_%i.%cOB", DVD_TYPE_STRING( ts_type ),
                STREAM_TYPE_STRING( ts_type ), title, i + 1 , STREAM_TYPE_STRING( ts_type ));
        if( !findDVDFile( ctx, filename, full_path ) ) {
          break;
        }

        if( ctx->fs->stat(ctx->fs, full_path, &fileinfo) < 0 ) {
          Log0(ctx, "Can't stat() %s.", filename );
          break;
        }

        dvd_file->title_devs[ i ] = ctx->dvdinput_open( ctx->priv, &ctx->logcb, full_path, NULL, ctx->fs );
        if( !dvd_file->title_devs[ i ] )
          break;
        dvd_file->title_sizes[ i ] = BYTES_TO_DVD_BLOCKS_CEIL(fileinfo.size);
        /* setting type of stream will determine what decryption to use */
        dvdinput_set_stream( dvd_file->title_devs[ i ], stream_type );

        /* if function is defined, cpxm was imported so call init */
        /* should have already been initialized in  dvdinput_setup, will copy over
         * decryption context to each dev dvdcss instance */
        if( ctx->dvdinput_init && stream_type == DVD_A )
          ctx->dvdinput_init( dvd_file->title_devs[ i ], NULL );

        ctx->dvdinput_title( dvd_file->title_devs[ i ], 0 );
        dvd_file->filesize += dvd_file->title_sizes[ i ];
      }
      if( !dvd_file->title_devs[ 0 ] ) {
        free( dvd_file );
        return NULL;
      }
    }
  }

  return dvd_file;
}

/* On a hybrid DVD-Audio/DVD-Video disc an audio title set may have no audio
 * objects of its own and instead borrow the title VOBs of a video title set.
 *
 * Returns the linked VTS number, or 0 when the ATS links to no video title set. */
static int DVDAudioResolveLinkedVTS( dvd_reader_t *ctx, int titlenum )
{
  ifo_handle_t *ifo;
  uint32_t vts_sa;
  int i, vtsn = 0;

  ifo = ifoOpenVTSI( ctx, titlenum );
  if( !ifo )
    return 0;
  vts_sa = ifo->atsi_mat->vts_sa;
  ifoClose( ifo );
  if( vts_sa == 0 )
    return 0;

  ifo = ifoOpenVMGI( ctx );
  if( !ifo )
    return 0;
  if( ifoRead_TIF( ifo, 1 ) ) {
    const tracks_info_table_t *att_srpt = ifo->info_table_first_sector;

    for( i = 0; i < att_srpt->nr_of_titles; i++ ) {
      const track_info_t *track = &att_srpt->tracks_info[ i ];

      if( !( track->type_and_rank & 0x80 ) &&
          track->ts_pointer_relative_sector == vts_sa ) {
        vtsn = track->group_property; /* the VTS number of a video track */
        break;
      }
    }
  }
  ifoClose( ifo );

  if( vtsn == 0 )
    Log1( ctx, "ATS %02d links to sector %u but no video title set starts "
               "there", titlenum, vts_sa );
  return vtsn;
}

/* The link is a fixed property of the disc, but both DVDOpenFile and
 * DVDFileStat resolve it per title, so an uncached lookup reopens and
 * reparses the ATS and AMG IFOs on every stat/open call and on every failed
 * probe of a nonexistent title. Memoize it per audio title set. */
static int DVDAudioLinkedVTS( dvd_reader_t *ctx, int titlenum )
{
  int *cached = NULL;
  int vtsn;

  if( titlenum >= 0 && titlenum < AUDIO_LINKED_VTS_MAX ) {
    cached = &ctx->rd->audio_linked_vts[ titlenum ];
    if( *cached != 0 )
      return *cached > 0 ? *cached : 0;
  }

  vtsn = DVDAudioResolveLinkedVTS( ctx, titlenum );

  if( cached )
    *cached = vtsn > 0 ? vtsn : -1;
  return vtsn;
}

dvd_file_t *DVDOpenFile( dvd_reader_t *ctx, int titlenum,
                         dvd_read_domain_t domain )
{
  dvd_reader_device_t *dvd = ctx->rd;
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  int do_cache = 0;

  /* Check arguments. */
  if( dvd == NULL || titlenum < 0 )
    return NULL;

  switch( domain ) {
  case DVD_READ_INFO_FILE:
    if ( ctx->dvd_type == DVD_VR )
      sprintf( filename, "/DVD_RTAV/VR_MANGR.IFO" );
    else if( titlenum == 0 ) {
      sprintf( filename, "/%s_TS/%s_TS.IFO", DVD_TYPE_STRING( ctx->dvd_type ),
              DVD_TYPE_STRING( ctx->dvd_type ) );
    } else {
      sprintf( filename, "/%s_TS/%cTS_%02i_0.IFO", DVD_TYPE_STRING( ctx->dvd_type ),
              STREAM_TYPE_STRING( ctx->dvd_type ), titlenum );
    }
    do_cache = 1;
    break;
  case DVD_READ_INFO_BACKUP_FILE:
    if ( ctx->dvd_type == DVD_VR )
      sprintf( filename, "/DVD_RTAV/VR_MANGR.BUP" );
    else if( titlenum == 0 ) {
      sprintf( filename,  "/%s_TS/%s_TS.BUP", DVD_TYPE_STRING( ctx->dvd_type ),
              DVD_TYPE_STRING( ctx->dvd_type ) );
    } else {
      sprintf( filename, "/%s_TS/%cTS_%02i_0.BUP", DVD_TYPE_STRING( ctx->dvd_type ),
              STREAM_TYPE_STRING( ctx->dvd_type ), titlenum );
    }
    do_cache = 1;
    break;
  case DVD_READ_MENU_VOBS:
    if( dvd->isImageFile ) {
      /* there is only two DVD-Audio menu vobs, and the second is optional
       * in the case of DVD-Audio, this should return 0 for AUDIO_TS.VOB, which is the main menu
       * AUDIO_SV.VOB is the Audio Still Video Set (ASVS), and contains the title menus */
      if ( ctx->dvd_type == DVD_VR ) {
        Log1( ctx, "There is no DVD-VR menu VOB" );
        return NULL; 
      } else if ( ctx->dvd_type == DVD_A && titlenum > 1 )
        Log2( ctx, "Defaulting to the only menu on DVD-Audio discs" );
      return DVDOpenVOBUDF( ctx, ( ctx->dvd_type == DVD_V ? titlenum : titlenum > 0 ), 1,
                            ctx->dvd_type );
    } else {
      return DVDOpenVOBPath( ctx, ( ctx->dvd_type == DVD_V ? titlenum : titlenum > 0 ), 1,
                             ctx->dvd_type );
    }
    break;
  case DVD_READ_TITLE_VOBS: {
    dvd_file_t *vobs;
    if( titlenum == 0 && ctx->dvd_type != DVD_VR ) return NULL;
    if( dvd->isImageFile ) {
      vobs = DVDOpenVOBUDF( ctx, titlenum, 0, ctx->dvd_type );
    } else {
      vobs = DVDOpenVOBPath( ctx, titlenum, 0, ctx->dvd_type );
    }
    /* a hybrid disc's ATS without AOBs borrows a video title set instead */
    if( !vobs && ctx->dvd_type == DVD_A ) {
      int vts = DVDAudioLinkedVTS( ctx, titlenum );
      if( vts > 0 ) {
        Log2( ctx, "ATS %02d has no AOBs, opening the title VOBs of VTS %02d",
              titlenum, vts );
        if( dvd->isImageFile ) {
          vobs = DVDOpenVOBUDF( ctx, vts, 0, DVD_V );
        } else {
          vobs = DVDOpenVOBPath( ctx, vts, 0, DVD_V );
        }
      }
    }
    return vobs;
  }
  case DVD_READ_SAMG_INFO:
    /* no other way to reach SAMG menu*/
    if( ctx->dvd_type != DVD_A ) {
      Log1( ctx, "SAMG IFO is exclusive to DVD-Audio" );
      return NULL;
    }
    strcpy( filename, "/AUDIO_TS/AUDIO_PP.IFO" );
    break;
  case DVD_READ_ASVS_INFO:
    /* no other way to reach ASVS menu*/
    if( ctx->dvd_type != DVD_A ) {
      Log1( ctx, "ASVS IFO is exclusive to DVD-Audio" );
      return NULL;
    }
    strcpy( filename, "/AUDIO_TS/AUDIO_SV.IFO" );

    break;
  case DVD_READ_ASVS_INFO_BACKUP:
    /* no other way to reach ASVS menu*/
    if( ctx->dvd_type != DVD_A ) {
      Log1( ctx, "ASVS IFO Backup is exclusive to DVD-Audio" );
      return NULL;
    }
    strcpy( filename, "/AUDIO_TS/AUDIO_SV.BUP" );

    break;
  default:
    Log1( ctx, "Invalid domain for file open." );
    return NULL;
  }

  if( dvd->isImageFile ) {
    return DVDOpenFileUDF( ctx, filename, do_cache );
  } else {
    return DVDOpenFilePath( ctx, filename );
  }
}

void DVDCloseFile( dvd_file_t *dvd_file )
{
  dvd_reader_device_t *dvd;
  if( !dvd_file )
    return;
  dvd = dvd_file->ctx->rd;
  if( dvd ) {
    if( !dvd->isImageFile ) {
      int i;

      for( i = 0; i < TITLES_MAX; ++i ) {
        if( dvd_file->title_devs[ i ] ) {
          dvd_file->ctx->dvdinput_close( dvd_file->title_devs[i] );
        }
      }
    }

    free( dvd_file->cache );
    free( dvd_file );
    dvd_file = NULL;
  }
}

/* see DVDOpenVOBUDF for the ts_type semantics */
static int DVDFileStatVOBUDF( dvd_reader_t *dvd, int title, int menu,
                              dvd_type_t ts_type, dvd_statistics_t *statbuf )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint32_t size;
  int64_t tot_size;
  int64_t parts_size[ 9 ];
  int nr_parts = 0;
  int n;

  if( title == 0 )
    sprintf( filename, "/%s_TS/%s_TS.VOB", DVD_TYPE_STRING( ts_type ),
            DVD_TYPE_STRING( ts_type ) );
  else
    sprintf( filename, "/%s_TS/%cTS_%02d_%d.%cOB", DVD_TYPE_STRING( ts_type ),
            STREAM_TYPE_STRING( ts_type ), title, menu ? 0 : 1, STREAM_TYPE_STRING( ts_type ) );

  if( !UDFFindFile( dvd, filename, &size ) )
    return -1;

  tot_size = size;
  nr_parts = 1;
  parts_size[ 0 ] = size;

  if( !menu ) {
    int cur;

    for( cur = 2; cur < 10; cur++ ) {
      sprintf( filename, "/%s_TS/%cTS_%02d_%d.%cOB", DVD_TYPE_STRING( ts_type ),
              STREAM_TYPE_STRING( ts_type ), title, cur , STREAM_TYPE_STRING( ts_type ) );
      if( !UDFFindFile( dvd, filename, &size ) )
        break;

      parts_size[ nr_parts ] = size;
      tot_size += size;
      nr_parts++;
    }
  }

  statbuf->size = tot_size;
  statbuf->nr_parts = nr_parts;
  for( n = 0; n < nr_parts; n++ )
    statbuf->parts_size[ n ] = parts_size[ n ];

  return 0;
}


/* see DVDOpenVOBUDF for the ts_type semantics */
static int DVDFileStatVOBPath( dvd_reader_t *dvd, int title, int menu,
                               dvd_type_t ts_type, dvd_statistics_t *statbuf )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  char full_path[ PATH_MAX + 1 ];
  dvdstat_t fileinfo;
  int64_t tot_size;
  int64_t parts_size[ 9 ];
  int nr_parts = 0;
  int n;

  /* the zone directory matters when the title set is not of the zone
   * implied by the disc type */
  if( title == 0 )
    sprintf( filename, "/%s_TS/%s_TS.VOB", DVD_TYPE_STRING( ts_type ),
            DVD_TYPE_STRING( ts_type ) );
  else
    sprintf( filename, "/%s_TS/%cTS_%02d_%d.%cOB", DVD_TYPE_STRING( ts_type ),
            STREAM_TYPE_STRING( ts_type ), title, menu ? 0 : 1,
            STREAM_TYPE_STRING( ts_type ) );

  if( !findDVDFile( dvd, filename, full_path ) )
    return -1;

  if (dvd->fs->stat(dvd->fs, full_path, &fileinfo) < 0) {
    Log1(dvd, "Can't stat() %s.", filename );
    return -1;
  }

  tot_size = fileinfo.size;
  nr_parts = 1;
  parts_size[ 0 ] = fileinfo.size;

  if( !menu ) {
    int cur;
    for( cur = 2; cur < 10; cur++ ) {
      sprintf( filename, "/%s_TS/%cTS_%02d_%d.%cOB", DVD_TYPE_STRING( ts_type ),
              STREAM_TYPE_STRING( ts_type ), title, cur,
              STREAM_TYPE_STRING( ts_type ) );
      if( !findDVDFile( dvd, filename, full_path ) )
        break;

      if (dvd->fs->stat(dvd->fs, full_path, &fileinfo) < 0) {
        Log1(dvd, "Can't stat() %s.", filename );
        break;
      }

      parts_size[ nr_parts ] = fileinfo.size;
      tot_size += parts_size[ nr_parts ];
      nr_parts++;
    }
  }

  statbuf->size = tot_size;
  statbuf->nr_parts = nr_parts;
  for( n = 0; n < nr_parts; n++ )
    statbuf->parts_size[ n ] = parts_size[ n ];

  return 0;
}

int DVDFileStat( dvd_reader_t *reader, int titlenum,
                 dvd_read_domain_t domain, dvd_stat_t *statbuf )
{
  dvd_statistics_t stats = {0};
  int ret = DVDFileStat2(reader, titlenum, domain, &stats);
  if (ret == 0) {
    if (sizeof(off_t) < 8 &&
        (stats.size > INT_MAX
         || stats.parts_size[0] > INT_MAX
         || stats.parts_size[1] > INT_MAX
         || stats.parts_size[2] > INT_MAX
         || stats.parts_size[3] > INT_MAX
         || stats.parts_size[4] > INT_MAX
         || stats.parts_size[5] > INT_MAX
         || stats.parts_size[6] > INT_MAX
         || stats.parts_size[7] > INT_MAX
         || stats.parts_size[8] > INT_MAX
         )
        ) {
      return -1;
    } else {
      statbuf->size = stats.size;
      statbuf->nr_parts = stats.nr_parts;
      for (int i = 0; i < 9; i++) {
        statbuf->parts_size[i] = stats.parts_size[i];
      }
    }
  }
  return ret;
}

int DVDFileStat2( dvd_reader_t *reader, int titlenum,
                  dvd_read_domain_t domain, dvd_statistics_t *statbuf )
{
  dvd_reader_device_t *dvd = reader->rd;
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  dvdstat_t fileinfo;
  uint32_t size;

  /* Check arguments. */
  if( dvd == NULL || titlenum < 0 ) {
    errno = EINVAL;
    return -1;
  }

  switch( domain ) {

  case DVD_READ_INFO_FILE:
    if( titlenum == 0 )
      sprintf( filename, "/%s_TS/%s_TS.IFO", DVD_TYPE_STRING( reader->dvd_type ), DVD_TYPE_STRING( reader->dvd_type ) );
    else
      sprintf( filename, "/%s_TS/%cTS_%02i_0.IFO", DVD_TYPE_STRING( reader->dvd_type ), STREAM_TYPE_STRING( reader->dvd_type ) ,titlenum );

    break;
  case DVD_READ_INFO_BACKUP_FILE:
    if( titlenum == 0 )
      sprintf( filename, "/%s_TS/%s_TS.BUP", DVD_TYPE_STRING( reader->dvd_type ), DVD_TYPE_STRING( reader->dvd_type ) );
    else
      sprintf( filename, "/%s_TS/%cTS_%02i_0.BUP", DVD_TYPE_STRING( reader->dvd_type ), STREAM_TYPE_STRING( reader->dvd_type ), titlenum );

    break;
  case DVD_READ_MENU_VOBS:
    if( dvd->isImageFile )
      return DVDFileStatVOBUDF( reader, titlenum, 1, reader->dvd_type, statbuf );
    else
      return DVDFileStatVOBPath( reader, titlenum, 1, reader->dvd_type, statbuf );

    break;
  case DVD_READ_TITLE_VOBS: {
    int ret;
    if( titlenum == 0 )
      return -1;

    if( dvd->isImageFile )
      ret = DVDFileStatVOBUDF( reader, titlenum, 0, reader->dvd_type, statbuf );
    else
      ret = DVDFileStatVOBPath( reader, titlenum, 0, reader->dvd_type, statbuf );
    /* a hybrid disc's ATS without AOBs borrows a video title set instead */
    if( ret < 0 && reader->dvd_type == DVD_A ) {
      int vts = DVDAudioLinkedVTS( reader, titlenum );
      if( vts > 0 ) {
        if( dvd->isImageFile )
          ret = DVDFileStatVOBUDF( reader, vts, 0, DVD_V, statbuf );
        else
          ret = DVDFileStatVOBPath( reader, vts, 0, DVD_V, statbuf );
      }
    }
    return ret;
  }
  case DVD_READ_SAMG_INFO:
    /* no other way to reach SAMG menu*/
    if( reader->dvd_type != DVD_A ) {
      Log1( reader, "SAMG IFO is exclusive to DVD-Audio" );
      return -1;
    }
    strcpy( filename, "/AUDIO_TS/AUDIO_PP.IFO" );

    break;
  case DVD_READ_ASVS_INFO:
    /* no other way to reach ASVS menu*/
    if( reader->dvd_type != DVD_A ) {
      Log1( reader, "ASVS IFO is exclusive to DVD-Audio" );
      return -1;
    }
    strcpy( filename, "/AUDIO_TS/AUDIO_SV.IFO" );

    break;
  case DVD_READ_ASVS_INFO_BACKUP:
    /* no other way to reach ASVS menu*/
    if( reader->dvd_type != DVD_A ) {
      Log1( reader, "ASVS IFO Backup is exclusive to DVD-Audio" );
      return -1;
    }
    strcpy( filename, "/AUDIO_TS/AUDIO_SV.BUP" );

    break;
  default:
    Log1(reader, "Invalid domain for file stat." );
    errno = EINVAL;
    return -1;
  }

  if( dvd->isImageFile ) {
    if( UDFFindFile( reader, filename, &size ) ) {
      statbuf->size = size;
      statbuf->nr_parts = 1;
      statbuf->parts_size[ 0 ] = size;
      return 0;
    }
  } else {
    char full_path[ PATH_MAX + 1 ];

    if( findDVDFile( reader, filename, full_path ) ) {
      if (reader->fs->stat(reader->fs, full_path, &fileinfo) < 0)
        Log1(reader, "Can't stat() %s.", filename );
      else {
        statbuf->size = fileinfo.size;
        statbuf->nr_parts = 1;
        statbuf->parts_size[ 0 ] = statbuf->size;
        return 0;
      }
    }
  }
  return -1;
}

/* Internal, but used from dvd_udf.c */
int InternalUDFReadBlocksRaw( const dvd_reader_t *ctx, uint32_t lb_number,
                      size_t block_count, unsigned char *data,
                      int encrypted )
{
  int ret;

  if( !ctx->rd->dev ) {
    Log0( ctx, "Fatal error in block read." );
    return -1;
  }

  ret = ctx->dvdinput_seek( ctx->rd->dev, (int) lb_number, encrypted & DVDCSS_SEEK_KEY );
  if( ret != (int) lb_number ) {
    Log1( ctx, "Can't seek to block %u", lb_number );
    return ret;
  }

  ret = ctx->dvdinput_read( ctx->rd->dev, (char *) data,
                       (int) block_count, encrypted & DVDINPUT_READ_DECRYPT );
  return ret;
}

/* This is using a single input and starting from 'dvd_file->lb_start' offset.
 *
 * Reads 'block_count' blocks from 'dvd_file' at block offset 'offset'
 * into the buffer located at 'data' and if 'encrypted' is set
 * descramble the data if it's encrypted.  Returning either an
 * negative error or the number of blocks read. */
static int DVDReadBlocksUDF( const dvd_file_t *dvd_file, uint32_t offset,
                             size_t block_count, unsigned char *data,
                             int encrypted )
{
  /* If the cache is present and we don't need to decrypt, use the cache to
   * feed the data */
  if( dvd_file->cache && (encrypted & DVDINPUT_READ_DECRYPT) == 0 ) {
    /* Check if we don't exceed the cache (or file) size */
    if( block_count + offset > (size_t) dvd_file->filesize )
      return 0;

    /* Copy the cache at a specified offset into data. offset and block_count
     * must be converted into bytes */
    memcpy( data, dvd_file->cache + (int64_t)offset * (int64_t)DVD_VIDEO_LB_LEN,
            (int64_t)block_count * (int64_t)DVD_VIDEO_LB_LEN );

    /* return the amount of blocks copied */
    return block_count;
  } else {
    /* use dvdinput access */
    return InternalUDFReadBlocksRaw( dvd_file->ctx, dvd_file->lb_start + offset,
                             block_count, data, encrypted );
  }
}

/* This is using possibly several inputs and starting from an offset of '0'.
 *
 * Reads 'block_count' blocks from 'dvd_file' at block offset 'offset'
 * into the buffer located at 'data' and if 'encrypted' is set
 * descramble the data if it's encrypted.  Returning either an
 * negative error or the number of blocks read. */
static int DVDReadBlocksPath( const dvd_file_t *dvd_file, unsigned int offset,
                              size_t block_count, unsigned char *data,
                              int encrypted )
{
  const dvd_reader_t *ctx = dvd_file->ctx;
  int i;
  int ret, ret2, off;

  ret = 0;
  ret2 = 0;
  for( i = 0; i < TITLES_MAX; ++i ) {
    if( !dvd_file->title_sizes[ i ] ) return 0; /* Past end of file */

    if( offset < dvd_file->title_sizes[ i ] ) {
      if( ( offset + block_count ) <= dvd_file->title_sizes[ i ] ) {
        off = ctx->dvdinput_seek( dvd_file->title_devs[ i ], (int)offset, DVDINPUT_NOFLAGS );
        if( off < 0 || off != (int)offset ) {
          Log1( ctx, "Can't seek to block %u", offset );
          return off < 0 ? off : 0;
        }
        ret = ctx->dvdinput_read( dvd_file->title_devs[ i ], data,
                             (int)block_count, encrypted );
        break;
      } else {
        size_t part1_size = dvd_file->title_sizes[ i ] - offset;
        /* FIXME: Really needs to be a while loop.
         * (This is only true if you try and read >1GB at a time) */

        /* Read part 1 */
        off = ctx->dvdinput_seek( dvd_file->title_devs[ i ], (int)offset, DVDINPUT_NOFLAGS );
        if( off < 0 || off != (int)offset ) {
          Log1( ctx, "Can't seek to block %u", offset );
          return off < 0 ? off : 0;
        }
        ret = ctx->dvdinput_read( dvd_file->title_devs[ i ], data,
                             (int)part1_size, encrypted );
        if( ret < 0 ) return ret;
        /* FIXME: This is wrong if i is the last file in the set.
         * also error from this read will not show in ret. */

        /* Does the next part exist? If not then return now. */
        if( i + 1 >= TITLES_MAX || !dvd_file->title_devs[ i + 1 ] )
          return ret;

        /* Read part 2 */
        off = ctx->dvdinput_seek( dvd_file->title_devs[ i + 1 ], 0, DVDINPUT_NOFLAGS );
        if( off < 0 || off != 0 ) {
          Log1( ctx, "Can't seek to block %d", 0 );
          return off < 0 ? off : 0;
        }
        ret2 = ctx->dvdinput_read( dvd_file->title_devs[ i + 1 ],
                              data + ( part1_size
                                       * (int64_t)DVD_VIDEO_LB_LEN ),
                              (int)(block_count - part1_size),
                              encrypted );
        if( ret2 < 0 ) return ret2;
        break;
      }
    } else {
      offset -= dvd_file->title_sizes[ i ];
    }
  }

  return ret + ret2;
}

/* This is broken reading more than 2Gb at a time is ssize_t is 32-bit. */
ssize_t DVDReadBlocks( dvd_file_t *dvd_file, int offset,
                       size_t block_count, unsigned char *data )
{
  dvd_reader_t *ctx;
  dvd_reader_device_t *dvd;
  int ret;

  /* Check arguments. */
  if( dvd_file == NULL || offset < 0 || data == NULL )
    return -1;

  ctx = dvd_file->ctx;
  dvd = ctx->rd;

  /* The decryption scheme follows the file, so it must be set before the
   * title key is requested below: css_title() decides whether to crack a
   * CSS key from the device's current stream type, and on a hybrid disc the
   * previous read may have left a different scheme selected. */
  if( dvd->isImageFile )
    dvdinput_set_stream( dvd->dev, dvd_file->stream_type );

  /* Hack, and it will still fail for multiple opens in a threaded app ! */
  if( dvd->css_title != dvd_file->css_title ) {
      dvd->css_title = dvd_file->css_title;
    if( dvd->isImageFile ) {
      ctx->dvdinput_title( dvd->dev, (int)dvd_file->lb_start );
    }
    /* Here each vobu has it's own dvdcss handle, so no need to update
    else {
      ctx->dvdinput_title( dvd_file->title_devs[ 0 ], (int)dvd_file->lb_start );
    }*/
  }

  if( dvd->isImageFile ) {
    ret = DVDReadBlocksUDF( dvd_file, (uint32_t)offset,
                            block_count, data, DVDINPUT_READ_DECRYPT );
  } else {
    ret = DVDReadBlocksPath( dvd_file, (unsigned int)offset,
                             block_count, data, DVDINPUT_READ_DECRYPT );
  }

  return (ssize_t)ret;
}

int32_t DVDFileSeek( dvd_file_t *dvd_file, int32_t offset )
{
  /* Check arguments. */
  if( dvd_file == NULL || offset < 0 )
    return -1;

  if( offset > dvd_file->filesize * DVD_VIDEO_LB_LEN ) {
    return -1;
  }
  dvd_file->seek_pos = (uint32_t) offset;
  return offset;
}

int DVDFileSeekForce(dvd_file_t *dvd_file, int offset, int force_size)
{
  dvd_reader_t *ctx = dvd_file->ctx;
  dvd_reader_device_t *dvd = ctx->rd;
  /* Check arguments. */
  if( dvd_file == NULL || offset <= 0 )
      return -1;

  if( dvd->isImageFile ) {
    if( force_size < 0 )
      force_size = (offset - 1) / DVD_VIDEO_LB_LEN + 1;
    if( dvd_file->filesize < force_size ) {
      dvd_file->filesize = force_size;
      free(dvd_file->cache);
      dvd_file->cache = NULL;
      Log2(ctx, "Ignored size of file indicated in UDF.");
    }
  }

  if( offset > dvd_file->filesize * DVD_VIDEO_LB_LEN )
    return -1;

  dvd_file->seek_pos = (uint32_t) offset;
  return offset;
}

ssize_t DVDReadBytes( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
  dvd_reader_t *ctx = dvd_file->ctx;
  dvd_reader_device_t *dvd = ctx->rd;
  unsigned char *secbuf_base, *secbuf;
  unsigned int numsec, seek_sector, seek_byte;
  int ret;

  /* Check arguments. */
  if( dvd_file == NULL || data == NULL || (ssize_t)byte_size < 0 )
    return -1;

  seek_sector = dvd_file->seek_pos / DVD_VIDEO_LB_LEN;
  seek_byte   = dvd_file->seek_pos % DVD_VIDEO_LB_LEN;

  numsec = ( ( seek_byte + byte_size ) / DVD_VIDEO_LB_LEN ) +
    ( ( ( seek_byte + byte_size ) % DVD_VIDEO_LB_LEN ) ? 1 : 0 );

  secbuf_base = malloc( numsec * DVD_VIDEO_LB_LEN + 2048 );
  if( !secbuf_base ) {
    Log0( ctx, "Can't allocate memory for file read" );
    return 0;
  }
  secbuf = (unsigned char *)(((uintptr_t)secbuf_base & ~((uintptr_t)2047)) + 2048);

  if( dvd->isImageFile ) {
    ret = DVDReadBlocksUDF( dvd_file, (uint32_t) seek_sector,
                            (size_t) numsec, secbuf, DVDINPUT_NOFLAGS );
  } else {
    ret = DVDReadBlocksPath( dvd_file, seek_sector,
                             (size_t) numsec, secbuf, DVDINPUT_NOFLAGS );
  }

  if( ret != (int) numsec ) {
    free( secbuf_base );
    return ret < 0 ? ret : 0;
  }

  memcpy( data, &(secbuf[ seek_byte ]), byte_size );
  free( secbuf_base );

  DVDFileSeekForce(dvd_file, dvd_file->seek_pos + byte_size, -1);
  return byte_size;
}

ssize_t DVDFileSize( dvd_file_t *dvd_file )
{
  /* Check arguments. */
  if( dvd_file == NULL )
    return -1;

  return dvd_file->filesize;
}

int DVDDiscID( dvd_reader_t *dvd, unsigned char *discid )
{
  struct md5_s ctx;
  int title;
  int title_sets;
  int nr_of_files = 0;
  ifo_handle_t *vmg_ifo;

  /* Check arguments. */
  if( dvd == NULL || discid == NULL )
    return 0;

  vmg_ifo = ifoOpen( dvd, 0 );
  if( !vmg_ifo ) {
    Log0(dvd, "DVDDiscId, failed to open VMG IFO" );
    return -1;
  }

  title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets + 1;
  ifoClose( vmg_ifo );

  if( title_sets > 10 )
  	title_sets = 10;

  /* Go through the first IFO:s, in order, up until the tenth,
   * and md5sum them, i.e  VIDEO_TS.IFO and VTS_0?_0.IFO */
  InitMD5( &ctx );
  for( title = 0; title < title_sets; title++ ) {
    dvd_file_t *dvd_file = DVDOpenFile( dvd, title, DVD_READ_INFO_FILE );
    if( dvd_file != NULL ) {
      ssize_t bytes_read;
      ssize_t file_size = dvd_file->filesize * DVD_VIDEO_LB_LEN;
      char *buffer_base = malloc( file_size + 2048 );

      if( buffer_base == NULL ) {
          DVDCloseFile( dvd_file );
          Log0(dvd, "DVDDiscId, failed to allocate memory for file read" );
          return -1;
      }

      char *buffer = (char *)(((uintptr_t)buffer_base & ~((uintptr_t)2047)) + 2048);

      bytes_read = DVDReadBytes( dvd_file, buffer, file_size );
      if( bytes_read != file_size ) {
          Log1(dvd, "DVDDiscId read returned %zd bytes"
                   ", wanted %zd", bytes_read, file_size );
          DVDCloseFile( dvd_file );
          free( buffer_base );
          return -1;
      }

      AddMD5( &ctx, buffer, file_size );

      DVDCloseFile( dvd_file );
      free( buffer_base );
      nr_of_files++;
    }
  }
  EndMD5( &ctx );
  memcpy( discid, ctx.buf, 16 );
  if(!nr_of_files)
    return -1;

  return 0;
}


int DVDISOVolumeInfo( dvd_reader_t *ctx,
                      char *volid, unsigned int volid_size,
                      unsigned char *volsetid, unsigned int volsetid_size )
{
  dvd_reader_device_t *dvd = ctx->rd;
  unsigned char *buffer, *buffer_base;
  int ret;

  /* Check arguments. */
  if( dvd == NULL )
    return 0;

  if( dvd->dev == NULL ) {
    /* No block access, so no ISO... */
    return -1;
  }

  buffer_base = malloc( DVD_VIDEO_LB_LEN + 2048 );

  if( buffer_base == NULL ) {
    Log0(ctx, "DVDISOVolumeInfo, failed to "
             "allocate memory for file read" );
    return -1;
  }

  buffer = (unsigned char *)(((uintptr_t)buffer_base & ~((uintptr_t)2047)) + 2048);

  ret = InternalUDFReadBlocksRaw( ctx, 16, 1, buffer, 0 );
  if( ret != 1 ) {
    Log0(ctx, "DVDISOVolumeInfo, failed to "
             "read ISO9660 Primary Volume Descriptor" );
    free( buffer_base );
    return -1;
  }

  if( (volid != NULL) && (volid_size > 0) ) {
    unsigned int n;
    for(n = 0; n < 32; n++) {
      if(buffer[40+n] == 0x20) {
        break;
      }
    }

    if(volid_size > n+1) {
      volid_size = n+1;
    }

    memcpy(volid, &buffer[40], volid_size-1);
    volid[volid_size-1] = '\0';
  }

  if( (volsetid != NULL) && (volsetid_size > 0) ) {
    if(volsetid_size > 128) {
      volsetid_size = 128;
    }
    memcpy(volsetid, &buffer[190], volsetid_size);
  }
  free( buffer_base );
  return 0;
}


int DVDUDFVolumeInfo( dvd_reader_t *ctx,
                      char *volid, unsigned int volid_size,
                      unsigned char *volsetid, unsigned int volsetid_size )
{
  int ret;
  /* Check arguments. */
  if( ctx == NULL || ctx->rd == NULL )
    return -1;

  if( ctx->rd->dev == NULL ) {
    /* No block access, so no UDF VolumeSet Identifier */
    return -1;
  }

  if( (volid != NULL) && (volid_size > 0) ) {
    ret = UDFGetVolumeIdentifier(ctx, volid, volid_size);
    if(!ret) {
      return -1;
    }
  }
  if( (volsetid != NULL) && (volsetid_size > 0) ) {
    ret =  UDFGetVolumeSetIdentifier(ctx, volsetid, volsetid_size);
    if(!ret) {
      return -1;
    }
  }

  return 0;
}
