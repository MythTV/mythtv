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
#define wsZero          0xb0 + 256
#define wsOne           0xb1 + 256
#define wsTwo           0xb2 + 256
#define wsThree         0xb3 + 256
#define wsFour          0xb4 + 256
#define wsFive          0xb5 + 256
#define wsSix           0xb6 + 256
#define wsSeven         0xb7 + 256
#define wsEight         0xb8 + 256
#define wsNine          0xb9 + 256
#define wsEnter         0x8d + 256

