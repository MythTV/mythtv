#ifndef XJ_H_
#define XJ_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include <map>
using namespace std;

class XvVideoOutput
{
  public:
    XvVideoOutput() { XJ_started = 0; xv_port = -1; scratchspace = NULL; }
   ~XvVideoOutput() { Exit(); }

    bool Init(int width, int height, char *window_name, 
              char *icon_name, int num_buffers, 
              unsigned char **out_buffers);
    void Show(unsigned char *buffer, int width, int height);
    int CheckEvents(void);

    void ToggleFullScreen();

  private:
    void sizehint(int x, int y, int width, int height, int max);
    void Exit(void);

    void decorate(int dec); 
    void hide_cursor(void);
    void show_cursor(void);

    map<unsigned char *,XvImage *> buffers;
    XShmSegmentInfo *XJ_SHMInfo;
    Screen *XJ_screen;
    Display *XJ_disp;
    Window XJ_root,XJ_win;

    XEvent XJ_ev;
    GC XJ_gc;
    int XJ_screen_num;
    unsigned long XJ_white,XJ_black;
    int XJ_started;
    int XJ_depth;
    int XJ_caught_error;
    int XJ_width, XJ_height;
    int XJ_screenwidth, XJ_screenheight;
    int XJ_fullscreen;

    int oldx, oldy, oldw, oldh;
    int curx, cury, curw, curh;

    XSizeHints hints;

    int xv_port;
    int colorid;

    unsigned char *scratchspace;
};

#endif
