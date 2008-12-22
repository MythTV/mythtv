#include "mythapplication.h"
#include "util-x11.h"

#ifdef USING_X11
int MythApplication::x11ProcessEvent(XEvent *event)
{
    X11L;
    int ret = QApplication::x11ProcessEvent(event);
    X11U;
    return ret;
}
#endif // USING_X11
