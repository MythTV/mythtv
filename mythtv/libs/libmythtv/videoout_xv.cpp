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

#include <X11/keysym.h>
#include <X11/Xatom.h>

extern "C" {
extern int      XShmQueryExtension(Display*);
extern int      XShmGetEventBase(Display*);
extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*, int, int, XShmSegmentInfo*);
}

#define GUID_I420_PLANAR 0x30323449
#define GUID_YV12_PLANAR 0x32315659

int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
  d = d; 
  xeev = xeev;
  return 0;
}

void XvVideoOutput::sizehint(int x, int y, int width, int height, int max)
{
   hints.flags = PPosition | PSize | PWinGravity | PBaseSize;
   hints.x = x; hints.y = y; hints.width = width; hints.height = height;
   if (max)
   {
      hints.max_width = width; hints.max_height=height;
      hints.flags |= PMaxSize;
   }
   else
   {
      hints.max_width = 0; hints.max_height = 0; 
   }
   hints.base_width = width; hints.base_height = height;
   hints.win_gravity = StaticGravity;
   XSetWMNormalHints(XJ_disp, XJ_win, &hints);
}

unsigned char *XvVideoOutput::Init(int width, int height, char *window_name, 
                                   char *icon_name)
{
  XWMHints wmhints;
  XTextProperty windowName, iconName;
  int (*old_handler)(Display *, XErrorEvent *);
  int i, ret;
  XJ_caught_error = 0;
  
  unsigned int p_version, p_release, p_request_base, p_event_base, p_error_base;
  int p_num_adaptors;

  pthread_mutex_init(&eventlock, NULL);

  XvAdaptorInfo *ai;

  XJ_width=width;
  XJ_height=height;

  XInitThreads();

  XJ_disp=XOpenDisplay(NULL);
  if(!XJ_disp) {printf("open display failed\n"); return NULL;}
  
  XJ_screen=DefaultScreenOfDisplay(XJ_disp);
  XJ_screen_num=DefaultScreen(XJ_disp);

  XJ_screenwidth = DisplayWidth(XJ_disp, XJ_screen_num);
  XJ_screenheight = DisplayHeight(XJ_disp, XJ_screen_num);
  
  XJ_white=XWhitePixel(XJ_disp,XJ_screen_num);
  XJ_black=XBlackPixel(XJ_disp,XJ_screen_num);
  
  XJ_fullscreen = 0;
  
  XJ_root=DefaultRootWindow(XJ_disp);
  
  XJ_win=XCreateSimpleWindow(XJ_disp,XJ_root,0,0,XJ_width,XJ_height,0,XJ_white,XJ_black);
  if(!XJ_win) {  
    printf("create window failed\n");
    return(NULL); 
  }
 
  curx = 0; cury = 0;
  curw = XJ_width; curh = XJ_height;
  
  /* tell window manager about our window */

  sizehint(curx, cury, curw, curh, 0);
  
  wmhints.input=True;
  wmhints.flags=InputHint;

  XStringListToTextProperty(&window_name, 1 ,&windowName);
  XStringListToTextProperty(&icon_name, 1 ,&iconName);

  XSetWMProperties(XJ_disp, XJ_win, 
		   &windowName, &iconName,
		   NULL, 0,
		   &hints, &wmhints, NULL);
  
  XSelectInput(XJ_disp, XJ_win, ExposureMask|KeyPressMask);
  XMapRaised(XJ_disp, XJ_win);
  
  old_handler = XSetErrorHandler(XJ_error_catcher);
  XSync(XJ_disp, 0);

  ret = XvQueryExtension(XJ_disp, &p_version, &p_release, &p_request_base,
                         &p_event_base, &p_error_base);
  if (ret != Success) {
    printf("XvQueryExtension failed.\n");
  }

  ret = XvQueryAdaptors(XJ_disp, DefaultRootWindow(XJ_disp),
                        (unsigned int *)&p_num_adaptors, &ai);

  if (ret != Success) {
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

  fo = XvListImageFormats(XJ_disp, xv_port, &formats);
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
  XvGrabPort(XJ_disp, xv_port, CurrentTime);

  XJ_gc = XCreateGC(XJ_disp, XJ_win, 0, 0);
  XJ_depth = DefaultDepthOfScreen(XJ_screen);
  XJ_image = XvShmCreateImage(XJ_disp, xv_port, colorid, 0, 
                              XJ_width, XJ_height, &XJ_SHMInfo);
  XJ_SHMInfo.shmid=shmget(IPC_PRIVATE, XJ_image->data_size,
		          IPC_CREAT|0777);

  if(XJ_SHMInfo.shmid < 0) {
    perror("shmget failed:");
    return (NULL);
  }

  XJ_image->data = XJ_SHMInfo.shmaddr = (char *)shmat(XJ_SHMInfo.shmid, 0, 0);
  XJ_SHMInfo.readOnly = False;
 
  XShmAttach(XJ_disp, &XJ_SHMInfo);

  XSync(XJ_disp, 0);
  XSetErrorHandler(old_handler);

  if (XJ_caught_error) {
      return 0;
  }
  
  XJ_started=1;

  ToggleFullScreen();

  if (colorid != GUID_I420_PLANAR)
  {
      scratchspace = new unsigned char[width * height * 3 / 2];
  }

  return((unsigned char *)XJ_image->data);
}

void XvVideoOutput::Exit(void)
{
  if(XJ_started) {
    XJ_started = false;
    pthread_mutex_lock(&eventlock);

    //if (XJ_image)
    //  XvDestroyImage(XJ_image);
    XShmDetach(XJ_disp, &XJ_SHMInfo);

    if(XJ_SHMInfo.shmaddr)
      shmdt(XJ_SHMInfo.shmaddr);

    if(XJ_SHMInfo.shmid > 0)
      shmctl(XJ_SHMInfo.shmid, IPC_RMID, 0);

    if (scratchspace)
        delete [] scratchspace;

    XvUngrabPort(XJ_disp, xv_port, CurrentTime);
    XDestroyWindow(XJ_disp, XJ_win);
    XCloseDisplay(XJ_disp);
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

  colormap = DefaultColormap(XJ_disp, DefaultScreen(XJ_disp));
  XAllocNamedColor(XJ_disp, colormap, "black", &black, &dummy);
  bm_no = XCreateBitmapFromData(XJ_disp, XJ_win, no_data, 8, 8);
  no_ptr = XCreatePixmapCursor(XJ_disp, bm_no, bm_no, &black, &black, 0, 0);

  XDefineCursor(XJ_disp, XJ_win, no_ptr);
  XFreeCursor(XJ_disp, no_ptr);
}

void XvVideoOutput::show_cursor(void)
{
  XDefineCursor(XJ_disp, XJ_win, 0);
}

void XvVideoOutput::ToggleFullScreen(void)
{
  if (XJ_fullscreen) {
    XJ_fullscreen = 0; 
    curx = oldx; cury = oldy; curw = oldw; curh = oldh;
    show_cursor();
    decorate(1);
  } else {
    XJ_fullscreen = 1;
    oldx = curx; oldy = cury; oldw = curw; oldh = curh;
    curx = 0 - (int)ceil(XJ_screenwidth * .05); 
    cury = 0 - (int)ceil(XJ_screenheight * 0.05); 
    curw = (int)ceil(XJ_screenwidth * 1.10); 
    curh = (int)ceil(XJ_screenheight * 1.12);
    hide_cursor();
    decorate(0);
  }

  sizehint(curx, cury, curw, curh, 0);
 
  XMoveResizeWindow(XJ_disp, XJ_win, curx, cury, curw, curh);
  XMapRaised(XJ_disp, XJ_win);

  XRaiseWindow(XJ_disp, XJ_win);
  XFlush(XJ_disp);
}
   
void XvVideoOutput::Show(int width, int height)
{
  if (colorid == GUID_YV12_PLANAR)
  {
      memcpy(scratchspace, (unsigned char *)XJ_image->data + (width * height),
             width * height / 4);
      memcpy((unsigned char *)XJ_image->data + (width * height),
             (unsigned char *)XJ_image->data + (width * height) * 5 / 4,
             width * height / 4);
      memcpy((unsigned char *)XJ_image->data + (width * height) * 5 / 4,
             scratchspace, width * height / 4);
  }
     
  XvShmPutImage(XJ_disp, xv_port, XJ_win, XJ_gc, XJ_image, 0, 0, width, 
                height, 0, 0, curw, curh, False);
  XSync(XJ_disp, False);
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
 
  if (!pthread_mutex_trylock(&eventlock))
  {
    return key;
  }
 
  while (XPending(XJ_disp))
  {
    XNextEvent(XJ_disp, &Event);

    switch (Event.type)
    {
      case KeyPress: 
      {
	 XLookupString(&Event.xkey, buf, sizeof(buf), &keySym, &stat);
	 key = ((keySym&0xff00) != 0?((keySym&0x00ff) + 256):(keySym));
         pthread_mutex_unlock(&eventlock);
	 return key;
      } break;
      default: break;
    }
  }

  pthread_mutex_unlock(&eventlock);
  return key;
}
