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

#ifdef USING_OPENGL_VSYNC
/* OpenGL video sync */
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glxext.h>
#endif

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
#ifdef USING_OPENGL_VSYNC
/* OpenGL video sync */
static Display *s_sync_display;
static GLXDrawable s_sync_drawable;
static GLXContext s_sync_context;
#endif
static int s_glsync_good;

static struct timeval lastvbi;

static int drmWaitVBlank( int fd, drmVBlankPtr vbl )
{
    struct timeval curvbi;
    int ret;

    do {
       ret = ioctl( fd, DRM_IOCTL_WAIT_VBLANK, vbl );
       vbl->request.type &= ~DRM_VBLANK_RELATIVE;
    } while (ret && errno == EINTR);

    curvbi.tv_sec = vbl->reply.tval_sec;
    curvbi.tv_usec = vbl->reply.tval_usec;
    lastvbi = curvbi;

    return ret;
}

static int init_try_nvidia(void) 
{
    nvidia_fd = open( nvidia_dev, O_RDONLY );
    /* int saved_errno = errno; */
    if (nvidia_fd >= 0) {
        /* Try to poll it to see if we're actually getting data */
        struct pollfd polldata;
        polldata.fd = nvidia_fd;
        polldata.events = 0xff;
        polldata.revents = 0;
        int ret = poll( &polldata, 1, 100 );
        if (!ret)
            printf("vblank nvidia retrace timeout\n");
        if (ret < 0)
            perror("vblank nvidia");
        if (ret <= 0) {
            close(nvidia_fd);
            nvidia_fd = -1;
        }
    }
    if( nvidia_fd < 0 ) {
        /* fprintf( stderr, "vsync: Can't open nVidia device %s: %s\n", nvidia_dev, strerror( saved_errno ) ); */
        /* fprintf( stderr, "vsync: Assuming non-nVidia hardware.\n" ); */
        return 0;
    }
    return 1;
}

static int init_try_drm(void)
{
    drmVBlank blank;
    int ret;

    dri_fd = open( dri_dev, O_RDWR );
    if( dri_fd < 0 ) {
        /* fprintf( stderr, "vsync: Can't open device %s: %s\n", dri_dev, strerror( errno ) ); */
        return 0;
    }
    blank.request.type = DRM_VBLANK_RELATIVE;
    blank.request.sequence = 1;
    if( (ret = drmWaitVBlank( dri_fd, &blank )) ) {
        /* fprintf( stderr, "vsync: Error %d from VBLANK ioctl, not supported by your DRI driver?\n", ret ); */
        close( dri_fd );
        dri_fd = -1;
        return 0;
    }
    return 1;
}

static int init_try_gl(void)
{
    /* Try to create an OpenGL surface so we can use glXWaitVideoSyncSGI:
     * http://osgcvs.no-ip.com/osgarchiver/archives/June2002/0022.html
     * http://www.ac3.edu.au/SGI_Developer/books/OpenGLonSGI/sgi_html/ch10.html#id37188
     * http://www.inb.mu-luebeck.de/~boehme/xvideo_sync.html
     */
    s_glsync_good = 0;
#ifdef USING_OPENGL_VSYNC
    s_sync_drawable = 0;
    s_sync_context = 0;
    s_sync_display = NULL;
    Display *disp = XOpenDisplay(NULL);
    /* Look for GLX at all */
    if (glXQueryExtension(disp, NULL, NULL)) {
        /* Look for video sync extension */
        const char *xt = glXQueryExtensionsString(disp, 0);
        if (strstr(xt, "GLX_SGI_video_sync") != NULL) {
            int attribList[] = {GLX_RGBA, 
                                GLX_RED_SIZE, 1,
                                GLX_GREEN_SIZE, 1,
                                GLX_BLUE_SIZE, 1,
                                None};
            XVisualInfo *vis;
            XSetWindowAttributes swa;
            Window w;
            
            vis = glXChooseVisual(disp, 0, attribList);
            swa.colormap = XCreateColormap(disp, RootWindow(disp, 0),
                                           vis->visual, AllocNone);
            w = XCreateWindow(disp, RootWindow(disp, 0), 0, 0, 1, 1,
                              0, vis->depth, InputOutput, vis->visual, 
                              CWColormap, &swa);
            s_sync_context = glXCreateContext(disp, vis, NULL, GL_TRUE);
            s_sync_drawable = w;
            s_glsync_good = 1;
            s_sync_display = disp;
            glXMakeCurrent(s_sync_display, s_sync_drawable, s_sync_context);
        }
    }
#endif
    return s_glsync_good;
}

/* Returns:
 * 0: no vsync method
 * 1: nVidia device polling
 * 2: DRM device polling
 * 3: OpenGL video sync
 */
int vsync_init( void )
{
    gettimeofday( &lastvbi, 0 );

    dri_fd = -1;
    nvidia_fd = -1;
    s_glsync_good = 0;

    if (init_try_nvidia()) {
        printf("Init found nVidia polling\n");
        return 1;
    }

    if (init_try_drm()) {
        printf("Init found DRM polling\n");
        return 2;
    }

    if (init_try_gl()) {
        printf("Init found OpenGL vsync\n");
        return 3;
    }

    return 0;
}

void vsync_shutdown( void )
{
    if( dri_fd >= 0 ) close( dri_fd );
    if( nvidia_fd >= 0 ) close( nvidia_fd );
}

void vsync_wait_for_retrace( void )
{
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
    } else if (s_glsync_good) {
        unsigned int count;
        int r;
#ifdef USING_OPENGL_VSYNC
        r = glXMakeCurrent(s_sync_display, s_sync_drawable, s_sync_context);
        //printf("glxmc: %d ", r);
        //glXSwapBuffers(s_sync_display, s_sync_drawable);
        r = glXGetVideoSyncSGI(&count);
        //printf("glxgvs: %d ", r);
        r = glXWaitVideoSyncSGI(2, (count+1)%2 ,&count);
        //printf("glxwvs: %d ", r);
        //printf("Count now %d\n", count);
#endif
    } else {
        usleep( 2000 );
    }
}

