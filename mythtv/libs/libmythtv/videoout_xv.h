/* Based on xqcam.c by Paul Chinn <loomer@svpal.org> */
#include <X11/Xlib.h> 
extern int XJ_depth;
extern char *XJ_init(int width, int height, char *window_name, char *icon_name);
extern void XJ_exit(void);
extern void XJ_show(int width, int height);
extern int XJ_CheckEvents(void);

#define wsUp            0x52 + 256
#define wsDown          0x54 + 256
#define wsLeft          0x51 + 256
#define wsRight         0x53 + 256
#define wsEscape        0x1b + 256

