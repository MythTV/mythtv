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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XShm.h>

extern "C" {
extern int      XShmQueryExtension(Display*);
extern int      XShmGetEventBase(Display*);
extern XvImage  *XvShmCreateImage(Display*, XvPortID, int, char*, int, int, XShmSegmentInfo*);
}

XShmSegmentInfo XJ_SHMInfo;
Screen *XJ_screen;
Display *XJ_disp;
Window XJ_root,XJ_win;
XvImage *XJ_image;

#define GUID_YUV12_PLANAR 0x32315659

XEvent XJ_ev;
GC XJ_gc;
int XJ_screen_num;
unsigned long XJ_white,XJ_black;
int XJ_started=0; 
int XJ_depth;
int XJ_caught_error;
int XJ_width, XJ_height;
int XJ_screenwidth, XJ_screenheight;
int XJ_fullscreen;

int oldx, oldy, oldw, oldh;
int curx, cury, curw, curh;

XSizeHints hints;

int xv_port = -1;

void XJ_toggleFullscreen(void);

int XJ_error_catcher(Display * d, XErrorEvent * xeev)
{
  ++XJ_caught_error;
  return 0;
}

void sizehint(int x, int y, int width, int height, int max)
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

char *XJ_init(int width, int height, char *window_name, char *icon_name)
{
  XWMHints wmhints;
  char *sbuf;
  XTextProperty windowName, iconName;
  int (*old_handler)(Display *, XErrorEvent *);
  int i, ret;
  
  unsigned int p_version, p_release, p_request_base, p_event_base, p_error_base;
  int p_num_adaptors;

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

  for (i = 0; i < p_num_adaptors; i++) {
    /*printf(" name:        %s\n"
           " type:        %s%s%s%s%s\n"
           " ports:       %ld\n"
           " first port:  %ld\n",
           ai[i].name,
           (ai[i].type & XvInputMask)   ? "input | "    : "",
           (ai[i].type & XvOutputMask)  ? "output | "   : "",
           (ai[i].type & XvVideoMask)   ? "video | "    : "",
           (ai[i].type & XvStillMask)   ? "still | "    : "",
           (ai[i].type & XvImageMask)   ? "image | "    : "",
           ai[i].num_ports,
           ai[i].base_id);*/
    xv_port = ai[i].base_id;

    /*
    printf("adaptor %d ; format list:\n", i);
    for (j = 0; j < ai[i].num_formats; j++) {
      printf(" depth=%d, visual=%ld\n",
             ai[i].formats[j].depth,
             ai[i].formats[j].visual_id);
    }
    for (p = ai[i].base_id; p < ai[i].base_id+ai[i].num_ports; p++) {

      printf(" encoding list for port %d\n", p);
      if (XvQueryEncodings(XJ_disp, p, &encodings, &ei) != Success) {
        printf("XvQueryEncodings failed.\n");
        continue;
      }
      for (j = 0; j < encodings; j++) {
        printf("  id=%ld, name=%s, size=%ldx%ld, numerator=%d, denominator=%d\n"
,
               ei[j].encoding_id, ei[j].name, ei[j].width, ei[j].height,
               ei[j].rate.numerator, ei[j].rate.denominator);
      }
      XvFreeEncodingInfo(ei);

      printf(" attribute list for port %d\n", p);
      at = XvQueryPortAttributes(XJ_disp, p, &attributes);
      for (j = 0; j < attributes; j++) {
        printf("  name:       %s\n"
               "  flags:     %s%s\n"
               "  min_color:  %i\n"
               "  max_color:  %i\n",
               at[j].name,
               (at[j].flags & XvGettable) ? " get" : "",
               (at[j].flags & XvSettable) ? " set" : "",

               at[j].min_value, at[j].max_value);
      }
      if (at)
        XFree(at);

      printf(" image format list for port %d\n", p);
      fo = XvListImageFormats(XJ_disp, p, &formats);
      for (j = 0; j < formats; j++) {
        printf("  0x%x (%4.4s) %s\n",
               fo[j].id,
               (char *)&fo[j].id,
               (fo[j].format == XvPacked) ? "packed" : "planar");
      }
      if (fo)
        XFree(fo);
    }
    */
    //printf("\n");
  }
  
  XvGrabPort(XJ_disp, xv_port, CurrentTime);
		  
  if (p_num_adaptors > 0)
    XvFreeAdaptorInfo(ai);
  if (xv_port == -1)
    exit (0);

  XJ_gc = XCreateGC(XJ_disp, XJ_win, 0, 0);
  XJ_depth = DefaultDepthOfScreen(XJ_screen);
  XJ_image = XvShmCreateImage(XJ_disp, xv_port, GUID_YUV12_PLANAR, 0, 
                              XJ_width, XJ_height, &XJ_SHMInfo);
  XJ_SHMInfo.shmid=shmget(IPC_PRIVATE, XJ_image->data_size,
		          IPC_CREAT|0777);

  if(XJ_SHMInfo.shmid < 0) {
    perror("shmget failed:");
    return (NULL);
  }
 
  sbuf = XJ_image->data = XJ_SHMInfo.shmaddr = (char *)shmat(XJ_SHMInfo.shmid, 0, 0);
  XJ_SHMInfo.readOnly = False;
  
  XShmAttach(XJ_disp, &XJ_SHMInfo);

  XSync(XJ_disp, 0);
  XSetErrorHandler(old_handler);

  if (XJ_caught_error) {
      return 0;
  }
  
  XJ_started=1;

  XJ_toggleFullscreen();

  return(sbuf);
}


void XJ_exit(void)
{
  if(XJ_started) {
    //if (XJ_image)
    //  XvDestroyImage(XJ_image);
    XShmDetach(XJ_disp, &XJ_SHMInfo);
    if(XJ_SHMInfo.shmaddr)
      shmdt(XJ_SHMInfo.shmaddr);
    if(XJ_SHMInfo.shmid > 0)
      shmctl(XJ_SHMInfo.shmid, IPC_RMID, 0);
    XvUngrabPort(XJ_disp, xv_port, CurrentTime);
  }
}

static void decorate(int dec)
{
}

static void hide_cursor(void)
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

static void show_cursor(void)
{
  XDefineCursor(XJ_disp, XJ_win, 0);
}

void XJ_toggleFullscreen(void)
{
  if (XJ_fullscreen) {
    XJ_fullscreen = 0; 
    curx = oldx; cury = oldy; curw = oldw; curh = oldh;
    show_cursor();
    decorate(1);
  } else {
    XJ_fullscreen = 1;
    oldx = curx; oldy = cury; oldw = curw; oldh = curh;
    curx = 0; cury = 0; curw = XJ_screenwidth; curh = XJ_screenheight;
    hide_cursor();
    decorate(0);
  }

  sizehint(curx, cury, curw, curh, 0);
 
  XMoveResizeWindow(XJ_disp, XJ_win, curx, cury, curw, curh);
  XMapRaised(XJ_disp, XJ_win);

  XRaiseWindow(XJ_disp, XJ_win);
  XFlush(XJ_disp);
}
   
void XJ_show(int width, int height)
{
  XvShmPutImage(XJ_disp, xv_port, XJ_win, XJ_gc, XJ_image, 0, 0, width, 
                height, 0, 0, curw, curh, False);
  XSync(XJ_disp, False);
}

int XJ_CheckEvents(void)
{
  XEvent Event;
  char buf[100];
  KeySym keySym;
  static XComposeStatus stat;
  int key = 0;
  
  while (XPending(XJ_disp))
  {
    XNextEvent(XJ_disp, &Event);

    switch (Event.type)
    {
      case KeyPress: 
      {
	 XLookupString(&Event.xkey, buf, sizeof(buf), &keySym, &stat);
	 key = ((keySym&0xff00) != 0?((keySym&0x00ff) + 256):(keySym));
	 return key;
      } break;
      default: break;
    }
  }

  return key;
}
