/**
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/io.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "vsync.h"

#define BASEPORT 0x3da

void vgasync_cleanup( void )
{
    fprintf( stderr, "vgasync: Cleaning up.\n" );
    if( ioperm( BASEPORT, 1, 0 ) ) {
        perror( "ioperm" );
    }
}

int vgasync_init( int verbose )
{
    /* Get access to the ports */
    if( ioperm( BASEPORT, 1, 1 ) ) {
        if( verbose ) {
            fprintf( stderr, "vgasync: Can't access VGA port: %s\n",
                     strerror( errno ) );
        }
        return 0;
    }

    atexit( vgasync_cleanup );
    return 1;
}

void vgasync_spin_until_end_of_next_refresh( void )
{
    /* TODO: A better timeout. */
    int i = 0x00ffffff;

    for(;i;--i) {
        /* wait for the refresh to occur. */
        if( (inb( BASEPORT ) & 8) ) break;
    }
    /* now we're inside the refresh */
    for(;i;--i) {
        /* wait for the refresh to stop. */
        if( !(inb( BASEPORT ) & 8) ) break;
    }

    if( !i ) fprintf( stderr, "vgasync: Timeout hit.\n" );
}

void vgasync_spin_until_out_of_refresh( void )
{
    /* TODO: A better timeout. */
    int i = 0x00ffffff;

    if( inb( BASEPORT ) & 8 ) {
        for(;i;--i) {
            /* wait for the refresh to stop. */
            if( !(inb( BASEPORT ) & 8) ) break;
        }
    }

    if( !i ) fprintf( stderr, "vgasync: Timeout hit.\n" );
}


typedef enum {
    _DRM_VBLANK_ABSOLUTE = 0x0,         /* Wait for specific vblank sequence number */
    _DRM_VBLANK_RELATIVE = 0x1,         /* Wait for given number of vblanks */
    _DRM_VBLANK_SIGNAL   = 0x40000000   /* Send signal instead of blocking */
} drm_vblank_seq_type_t;

#define DRM_IOCTL_BASE                  'd'
#define DRM_IOWR(nr,type)               _IOWR(DRM_IOCTL_BASE,nr,type)
#define _DRM_VBLANK_FLAGS_MASK _DRM_VBLANK_SIGNAL

struct drm_wait_vblank_request {
    drm_vblank_seq_type_t type;
    unsigned int sequence;
    unsigned long signal;
};

struct drm_wait_vblank_reply {
    drm_vblank_seq_type_t type;
    unsigned int sequence;
    long tval_sec;
    long tval_usec;
};

typedef union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
} drm_wait_vblank_t;

#define DRM_IOCTL_WAIT_VBLANK           DRM_IOWR(0x3a, drm_wait_vblank_t)

typedef enum {
    DRM_VBLANK_ABSOLUTE = 0x0,          /* Wait for specific vblank sequence number */
    DRM_VBLANK_RELATIVE = 0x1,          /* Wait for given number of vblanks */
    DRM_VBLANK_SIGNAL   = 0x40000000    /* Send signal instead of blocking */
} drmVBlankSeqType;

typedef struct _drmVBlankReq {
    drmVBlankSeqType type;
    unsigned int sequence;
    unsigned long signal;
} drmVBlankReq, *drmVBlankReqPtr;

typedef struct _drmVBlankReply {
    drmVBlankSeqType type;
    unsigned int sequence;
    long tval_sec;
    long tval_usec;
} drmVBlankReply, *drmVBlankReplyPtr;

typedef union _drmVBlank {
    drmVBlankReq request;
    drmVBlankReply reply;
} drmVBlank, *drmVBlankPtr;

static int dri_fd = -1;
const char *dri_dev = "/dev/dri/card0";

static int nvidia_fd = -1;
const char *nvidia_dev = "/dev/nvidia0";

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

static struct timeval lastvbi;

int drmWaitVBlank( int fd, drmVBlankPtr vbl )
{
    struct timeval curvbi;
    struct timeval curtime;
    int ret;

    do {
       ret = ioctl( fd, DRM_IOCTL_WAIT_VBLANK, vbl );
       vbl->request.type &= ~DRM_VBLANK_RELATIVE;
    } while (ret && errno == EINTR);

    curvbi.tv_sec = vbl->reply.tval_sec;
    curvbi.tv_usec = vbl->reply.tval_usec;
    gettimeofday( &curtime, 0 );
    // fprintf( stderr, "time: %d, late %d\n", timediff( &curvbi, &lastvbi ), timediff( &curtime, &curvbi ) );
    lastvbi = curvbi;

    return ret;
}

int vsync_init( void )
{
    drmVBlank blank;
    int ret;

    gettimeofday( &lastvbi, 0 );

    nvidia_fd = open( nvidia_dev, O_RDONLY );
    if( nvidia_fd < 0 ) {
        fprintf( stderr, "vsync: Can't open nVidia device %s: %s\n", nvidia_dev, strerror( errno ) );
        fprintf( stderr, "vsync: Assuming non-nVidia hardware.\n" );

        dri_fd = open( dri_dev, O_RDWR );
        if( dri_fd < 0 ) {
            fprintf( stderr, "vsync: Can't open device %s: %s\n", dri_dev, strerror( errno ) );
        } else {
            blank.request.type = DRM_VBLANK_RELATIVE;
            blank.request.sequence = 1;
            if( (ret = drmWaitVBlank( dri_fd, &blank )) ) {
                fprintf( stderr, "vsync: Error %d from VBLANK ioctl, not supported by your DRI driver?\n", ret );
                close( dri_fd );
                dri_fd = -1;
            }
        }
    }

    return 1;
}

void vsync_shutdown( void )
{
    if( dri_fd >= 0 ) close( dri_fd );
    if( nvidia_fd >= 0 ) close( nvidia_fd );
}

void vsync_wait_for_retrace( void )
{
    struct timeval beforewait;
    struct timeval afterwait;

    gettimeofday( &beforewait, 0 );
    if( dri_fd >= 0 ) {
        drmVBlank blank;

        blank.request.type = DRM_VBLANK_RELATIVE;
        blank.request.sequence = 1;
        drmWaitVBlank( dri_fd, &blank );
    } else if( nvidia_fd >= 0 ) {
        struct pollfd polldata;
        polldata.fd = nvidia_fd;
        polldata.events = 0xff;
        polldata.revents = 0;
        poll( &polldata, 1, 50 );
    } else {
        usleep( 20000 );
    }
    gettimeofday( &afterwait, 0 );
    if( timediff( &afterwait, &beforewait ) < 1000 ) {
        fprintf( stderr, "vsync: Protection hit!  Sleep too short!  vsync unreliable!\n" );
        usleep( 10000 );
    }
}

