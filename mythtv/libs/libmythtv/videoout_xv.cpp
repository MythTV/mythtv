/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>

#include <iostream>
using namespace std;

#include "XJ.h"
#include "../libmyth/mythcontext.h"
#include "../libmyth/util.h"

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>


extern "C" {
extern int      XShmQueryExtension(Display*);
extern int      XShmGetEventBase(Display*);
extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*, int, int, XShmSegmentInfo*);
}

struct XvData
{
    Window XJ_root;
    Window XJ_win;
    Window XJ_curwin;
    GC XJ_gc;
    XSizeHints hints;
    Screen *XJ_screen;
    Display *XJ_disp;
    XShmSegmentInfo *XJ_SHMInfo;
    map<unsigned char *, XvImage *> buffers;
};

#define GUID_I420_PLANAR 0x30323449
#define GUID_YV12_PLANAR 0x32315659

int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
  d = d; 
  xeev = xeev;
  return 0;
}

XvVideoOutput::XvVideoOutput(MythContext *context)
{
    m_context = context;
    XJ_started = 0; 
    xv_port = -1; 
    scratchspace = NULL; 

    data = new XvData();
}

XvVideoOutput::~XvVideoOutput()
{
    Exit();
    delete data;
}

void XvVideoOutput::sizehint(int x, int y, int width, int height, int max)
{
    data->hints.flags = PPosition | PSize | PWinGravity | PBaseSize;
    data->hints.x = x; 
    data->hints.y = y; 
    data->hints.width = width; 
    data->hints.height = height;
    if (max)
    {
        data->hints.max_width = width; 
        data->hints.max_height = height;
        data->hints.flags |= PMaxSize;
    }
    else
    {
        data->hints.max_width = 0; 
        data->hints.max_height = 0; 
    }
    data->hints.base_width = 160;
    data->hints.base_height = 120;
    data->hints.win_gravity = StaticGravity;
    XSetWMNormalHints(data->XJ_disp, data->XJ_win, &(data->hints));
}

bool XvVideoOutput::Init(int width, int height, char *window_name, 
                         char *icon_name, int num_buffers, 
                         unsigned char **out_buffers, unsigned int wid)
{
    pthread_mutex_init(&lock, NULL);

    XWMHints wmhints;
    XTextProperty windowName, iconName;
    XClassHint hint;
    int (*old_handler)(Display *, XErrorEvent *);
    int i, ret;
    XJ_caught_error = 0;
  
    unsigned int p_version, p_release, p_request_base, p_event_base, 
                 p_error_base;
    int p_num_adaptors;

    XvAdaptorInfo *ai;

    hint.res_name = window_name;
    hint.res_class = window_name;

    XJ_width = width;
    XJ_height = height;

    XInitThreads();

    data->XJ_disp = XOpenDisplay(NULL);
    if (!data->XJ_disp) 
    {
        printf("open display failed\n"); 
        return false;
    }
  
    data->XJ_screen = DefaultScreenOfDisplay(data->XJ_disp);
    XJ_screen_num = DefaultScreen(data->XJ_disp);

    QString HorizScanMode = m_context->GetSetting("HorizScanMode", "overscan");
    QString VertScanMode = m_context->GetSetting("VertScanMode", "overscan");

    img_hscanf = m_context->GetNumSetting("HorizScanPercentage", 5) / 100.0;
    img_vscanf = m_context->GetNumSetting("VertScanPercentage",5) / 100.0;
   
    img_xoff = m_context->GetNumSetting("xScanDisplacement", 0);
    img_yoff = m_context->GetNumSetting("yScanDisplacement",0);


    if (VertScanMode == "underscan") 
    {
        img_vscanf = 0 - img_vscanf;
    }
    if (HorizScanMode == "underscan") 
    {
        img_hscanf = 0 - img_hscanf;
    }

    printf("Over/underscanning. V: %f, H: %f, XOff: %d, YOff: %d\n", 
           img_vscanf, img_hscanf, img_xoff, img_yoff);
 
    XJ_aspect = m_context->GetNumSetting("FixedAspectRatio", 0);

    XJ_white = XWhitePixel(data->XJ_disp, XJ_screen_num);
    XJ_black = XBlackPixel(data->XJ_disp, XJ_screen_num);
  
    XJ_fullscreen = 0;
  
    data->XJ_root = DefaultRootWindow(data->XJ_disp);

    GetMythTVGeometry(m_context, data->XJ_disp, XJ_screen_num,
                      &XJ_screenx, &XJ_screeny, 
                      &XJ_screenwidth, &XJ_screenheight);

    curx = XJ_screenx + 4; cury = XJ_screeny + 20;
    curw = XJ_width; curh = XJ_height;

    dispx = 0; dispy = 0;
    dispw = curw; disph = curh;

    embedding = false;

    bool createwindow = true;
    if (wid > 0)
    {
        data->XJ_win = wid;
        data->XJ_curwin = data->XJ_win;
        createwindow = false;
    }

    if (createwindow)
    {
        if (XJ_aspect)
            curw = 4 * curh / 3;

        data->XJ_win = XCreateSimpleWindow(data->XJ_disp, data->XJ_root, curx, 
                                           cury, XJ_width, XJ_height, 0, 
                                           XJ_white, XJ_black);
        data->XJ_curwin = data->XJ_win;

        if (!data->XJ_win) 
        {  
            printf("create window failed\n");
            return false; 
        } 

        XSetClassHint(data->XJ_disp, data->XJ_win, &hint);
 
        /* tell window manager about our window */

        sizehint(curx, cury, curw, curh, 0);
  
        wmhints.input=True;
        wmhints.flags=InputHint;

        XStringListToTextProperty(&window_name, 1 ,&windowName);
        XStringListToTextProperty(&icon_name, 1 ,&iconName);


        XSetWMProperties(data->XJ_disp, data->XJ_win, 
                         &windowName, &iconName,
                         NULL, 0, &(data->hints), &wmhints, NULL);
  
        XSelectInput(data->XJ_disp, data->XJ_win,
                     ExposureMask|KeyPressMask|StructureNotifyMask);
        XMapRaised(data->XJ_disp, data->XJ_win);
    }
 
    old_handler = XSetErrorHandler(XJ_error_catcher);
    XSync(data->XJ_disp, 0);

    ret = XvQueryExtension(data->XJ_disp, &p_version, &p_release, 
                           &p_request_base, &p_event_base, &p_error_base);
    if (ret != Success) 
    {
        printf("XvQueryExtension failed.\n");
    }

    ret = XvQueryAdaptors(data->XJ_disp, data->XJ_root,
                          (unsigned int *)&p_num_adaptors, &ai);

    if (ret != Success) 
    {
        printf("XvQueryAdaptors failed.\n");
    }

    xv_port = -1;
    for (i = 0; i < p_num_adaptors; i++) 
    {
        if ((ai[i].type & XvInputMask) &&
            (ai[i].type & XvImageMask))
        {
            xv_port = ai[i].base_id;
            break;
        }
    }
 
    if (p_num_adaptors > 0)
        XvFreeAdaptorInfo(ai);
    if (xv_port == -1)
    {
        printf("Couldn't find Xv support, exiting\n");
        exit (0);
    }

    int formats;
    XvImageFormatValues *fo;
    bool foundimageformat = false;

    fo = XvListImageFormats(data->XJ_disp, xv_port, &formats);
    for (i = 0; i < formats; i++)
    {
        if (fo[i].id == GUID_I420_PLANAR)
        {
            foundimageformat = true;
            colorid = GUID_I420_PLANAR;
        }
    }

    if (!foundimageformat)
    {
        for (i = 0; i < formats; i++)
        {
            if (fo[i].id == GUID_YV12_PLANAR)
            {
                foundimageformat = true;
                colorid = GUID_YV12_PLANAR;
            }
        }
    } 

    if (!foundimageformat)
    {
        printf("Couldn't find the proper Xv image format\n");
        exit(0);
    }

    if (fo)
        XFree(fo);

    printf("Using XV port %d\n", xv_port);
    XvGrabPort(data->XJ_disp, xv_port, CurrentTime);

    data->XJ_gc = XCreateGC(data->XJ_disp, data->XJ_win, 0, 0);
    XJ_depth = DefaultDepthOfScreen(data->XJ_screen);
    data->XJ_SHMInfo = new XShmSegmentInfo[num_buffers];
    for (int i = 0; i < num_buffers; i++)
    {
        XvImage *image = XvShmCreateImage(data->XJ_disp, xv_port, colorid, 0,
                                          XJ_width, XJ_height, 
                                          &(data->XJ_SHMInfo)[i]);
        (data->XJ_SHMInfo)[i].shmid = shmget(IPC_PRIVATE, image->data_size,
                                             IPC_CREAT|0777);
        if ((data->XJ_SHMInfo)[i].shmid < 0)
        {
            perror("shmget failed:");
            return false;
        }
 
        image->data = (data->XJ_SHMInfo)[i].shmaddr = 
                         (char *)shmat((data->XJ_SHMInfo)[i].shmid, 0, 0);
        data->buffers[(unsigned char *)image->data] = image;
        out_buffers[i] = (unsigned char *)image->data;

        (data->XJ_SHMInfo)[i].readOnly = False;

        XShmAttach(data->XJ_disp, &(data->XJ_SHMInfo)[i]);
        XSync(data->XJ_disp, 0);
    }

    XSetErrorHandler(old_handler);

    if (XJ_caught_error) 
    {
        return false;
    }
  
    XJ_started = 1;

    if (createwindow)
        ToggleFullScreen();

    if (colorid != GUID_I420_PLANAR)
    {
        scratchspace = new unsigned char[width * height * 3 / 2];
    }

    return true;
}

void XvVideoOutput::Exit(void)
{
    if (XJ_started) 
    {
        XJ_started = false;

        int i = 0;
        map<unsigned char *, XvImage *>::iterator iter = data->buffers.begin();
        for(; iter != data->buffers.end(); ++iter, ++i) 
        {
            XShmDetach(data->XJ_disp, &(data->XJ_SHMInfo)[i]);
            if ((data->XJ_SHMInfo)[i].shmaddr)
                shmdt((data->XJ_SHMInfo)[i].shmaddr);
            if ((data->XJ_SHMInfo)[i].shmid > 0)
            shmctl((data->XJ_SHMInfo)[i].shmid, IPC_RMID, 0);
            XFree(iter->second);
        }

        delete [] (data->XJ_SHMInfo);

        if (scratchspace)
            delete [] scratchspace;

        XvUngrabPort(data->XJ_disp, xv_port, CurrentTime);
        XDestroyWindow(data->XJ_disp, data->XJ_win);
        XCloseDisplay(data->XJ_disp);
    }
}

void XvVideoOutput::decorate(int dec)
{
   dec = dec;
}

void XvVideoOutput::hide_cursor(void)
{
    Cursor no_ptr;
    Pixmap bm_no;
    XColor black, dummy;
    Colormap colormap;
    static char no_data[] = { 0,0,0,0,0,0,0,0 };

    colormap = DefaultColormap(data->XJ_disp, DefaultScreen(data->XJ_disp));
    XAllocNamedColor(data->XJ_disp, colormap, "black", &black, &dummy);
    bm_no = XCreateBitmapFromData(data->XJ_disp, data->XJ_win, no_data, 8, 8);
    no_ptr = XCreatePixmapCursor(data->XJ_disp, bm_no, bm_no, &black, &black, 
                                 0, 0);

    XDefineCursor(data->XJ_disp, data->XJ_win, no_ptr);
    XFreeCursor(data->XJ_disp, no_ptr);
}

void XvVideoOutput::show_cursor(void)
{
    XDefineCursor(data->XJ_disp, data->XJ_win, 0);
}

void XvVideoOutput::ToggleFullScreen(void)
{
    pthread_mutex_lock(&lock);

    if (XJ_fullscreen)
    {
        XJ_fullscreen = 0; 

        curx = oldx; cury = oldy; curw = oldw; curh = oldh;
        show_cursor();
        decorate(1);
    } 
    else 
    {
        XJ_fullscreen = 1;
        oldx = curx; oldy = cury; oldw = curw; oldh = curh;

        curx = XJ_screenx;
        cury = XJ_screeny;
        curw = XJ_screenwidth;
        curh = XJ_screenheight + 4;
        hide_cursor();
        decorate(0);
    }

    dispx = 0;
    dispy = 0;
    dispw = curw;
    disph = curh;

    MoveResize();

    pthread_mutex_unlock(&lock);
}
  
void XvVideoOutput::EmbedInWidget(unsigned long wid, int x, int y, int w, int h)
{
    if (embedding)
        return;

    pthread_mutex_lock(&lock);
    data->XJ_curwin = wid;

    olddispx = dispx;
    olddispy = dispy;
    olddispw = dispw;
    olddisph = disph;

    dispxoff = dispx = x;
    dispyoff = dispy = y;
    dispwoff = dispw = w;
    disphoff = disph = h;

    embedding = true;

    pthread_mutex_unlock(&lock);
}
 
void XvVideoOutput::StopEmbedding(void)
{
    if (!embedding)
        return;

    pthread_mutex_lock(&lock);

    dispx = olddispx;
    dispy = olddispy;
    dispw = olddispw;
    disph = olddisph;

    data->XJ_curwin = data->XJ_win;

    embedding = false;

    MoveResize();

    pthread_mutex_unlock(&lock);
}

void XvVideoOutput::Show(unsigned char *buffer, int width, int height)
{
    XvImage *image = data->buffers[buffer];

    if (colorid == GUID_YV12_PLANAR)
    {
        memcpy(scratchspace, (unsigned char *)image->data + (width * height),
               width * height / 4);
        memcpy((unsigned char *)image->data + (width * height),
               (unsigned char *)image->data + (width * height) * 5 / 4,
               width * height / 4);
        memcpy((unsigned char *)image->data + (width * height) * 5 / 4,
               scratchspace, width * height / 4);
    }
   
    pthread_mutex_lock(&lock);
 
    XvShmPutImage(data->XJ_disp, xv_port, data->XJ_curwin, data->XJ_gc, image, 
                  imgx, imgy, imgw, imgh, dispxoff, dispyoff, dispwoff, 
                  disphoff, False);
    XSync(data->XJ_disp, False);

    pthread_mutex_unlock(&lock);
}

int XvVideoOutput::CheckEvents(void)
{
    if (!XJ_started)
        return 0;

    XEvent Event;
    char buf[100];
    KeySym keySym;
    static XComposeStatus stat;
    int key = 0;
 
    while (XPending(data->XJ_disp))
    {
        XNextEvent(data->XJ_disp, &Event);

        switch (Event.type)
        {
            case KeyPress: 
            {
                XLookupString(&Event.xkey, buf, sizeof(buf), &keySym, &stat);
                key = ((keySym&0xff00) != 0?((keySym&0x00ff) + 256):(keySym));
                return key;
            } break;
            case ConfigureNotify: 
            {
                ReConfigure(Event.xconfigure.x, Event.xconfigure.y,
                            Event.xconfigure.width, Event.xconfigure.height);
                return key;
            } break;
            default: break;
        }
    }

    return key;
}

void XvVideoOutput::ReConfigure(int x, int y, int w, int h)
{
    if (XEventsQueued(data->XJ_disp, QueuedAlready))
        return;

    if (XJ_fullscreen || h >= XJ_screenheight && w >= XJ_screenwidth)
        return;

    if (oldx == x && oldy == y && oldw == w && oldh == h)
        return;

    if (XJ_aspect)
    {
        if (w * 3 / 4 > h)
        {
            w = (int) (h * 4 / 3);
        }
        else
        {
            h = (int) (w * 3 / 4);
        }
    }

    oldx = curx = x;
    oldy = cury = y;
    oldw = curw = w;
    oldh = curh = h;

    dispx = 0;
    dispy = 0;
    dispw = curw;
    disph = curh;
    
    MoveResize();
    return;
}

void XvVideoOutput::MoveResize(void)
{
    int yoff, xoff;

    XMoveResizeWindow(data->XJ_disp, data->XJ_win, curx, cury, curw, curh);
    XMapRaised(data->XJ_disp, data->XJ_win);

    XRaiseWindow(data->XJ_disp, data->XJ_win);
    XFlush(data->XJ_disp);

    // Preset all image placement and sizing variables.
    imgx = 0; imgy = 0;
    imgw = XJ_width; imgh = XJ_height;
    xoff = img_xoff; yoff = img_yoff;
    dispxoff = dispx; dispyoff = dispy;
    dispwoff = dispw; disphoff = disph;

/*
    Here we apply playback over/underscanning and offsetting (if any apply).

    It doesn't make any sense to me to offset an image such that it is clipped.
    Therefore, we only apply offsets if there is an underscan or overscan which
    creates "room" to move the image around. That is, if we overscan, we can 
    move the "viewport". If we underscan, we change where we place the image 
    into the display window. If no over/underscanning is performed, you just 
    get the full original image scaled into the full display area.
*/

    if (img_vscanf > 0) {
        // Veritcal overscan. Move the Y start point in original image.
        imgy = (int)ceil(XJ_height * img_vscanf);
        imgh = (int)ceil(XJ_height * (1 - 2 * img_vscanf));

        // If there is an offset, apply it now that we have a room.
        // To move the image down, move the start point up.
        if(yoff > 0) {
            // Can't offset the image more than we have overscanned.
            if(yoff > imgy) yoff = imgy;
            imgy -= yoff;
        }
        // To move the image up, move the start point down.
        if(yoff < 0) {
            // Again, can't offset more than overscanned.
            if( abs(yoff) > imgy ) yoff = 0 - imgy;
            imgy -= yoff;
        }
    }

    if (img_hscanf > 0) {
        // Horizontal overscan. Move the X start point in original image.
        imgx = (int)ceil(XJ_width * img_hscanf);
        imgw = (int)ceil(XJ_width * (1 - 2 * img_hscanf));
        if(xoff > 0) {
            if(xoff > imgx) xoff = imgx;
            imgx -= xoff;
        }
        if(xoff < 0) {
            if( abs(xoff) > imgx ) xoff = 0 - imgx;
            imgx -= xoff;
        }
    }

    float vscanf, hscanf;
    if (img_vscanf < 0) {
        // Veritcal underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        vscanf = fabs(img_vscanf);
        dispyoff = (int)ceil(disph * vscanf);
        disphoff = (int)ceil(disph * (1 - 2 * vscanf));
        // Now offset the image within the extra blank space created by 
        // underscanning.
        // To move the image down, increase the Y offset inside the display 
        // window.
        if(yoff > 0) {
            // Can't offset more than we have underscanned.
            if(yoff > dispyoff) yoff = dispyoff;
            dispyoff += yoff;
        }
        if(yoff < 0) {
            if( abs(yoff) > dispyoff ) yoff = 0 - dispyoff;
            dispyoff += yoff;
        }
    }

    if (img_hscanf < 0) {
        hscanf = fabs(img_hscanf);
        dispxoff = (int)ceil(dispw * hscanf);
        dispwoff = (int)ceil(dispw * (1 - 2 * hscanf));
        if(xoff > 0) {
            if(xoff > dispxoff) xoff = dispxoff;
            dispxoff += xoff;
        }
        if(xoff < 0) {
            if( abs(xoff) > dispxoff ) xoff = 0 - dispxoff;
            dispxoff += xoff;
        }
    }

}
