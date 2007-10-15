// -*- Mode: c++ -*-

#ifndef _UTIL_XV_H_
#define _UTIL_XV_H_

#include <qmap.h>

#include "videobuffers.h"
#include "exitcodes.h"


class port_info
{
  public:
    Display *disp;
    int port;
};

extern QMap<int,port_info> open_xv_ports;

extern void close_all_xv_ports_signal_handler(int sig);
extern void add_open_xv_port(Display *disp, int port);
extern void del_open_xv_port(int port);
extern bool has_open_xv_port(int port);
extern uint cnt_open_xv_port(void);
extern QString xvflags2str(int flags);
extern bool xv_is_attrib_supported(
    Display *disp, int port, const char *name,
    int *current_value = NULL, int *min_val = NULL, int *max_val = NULL);
extern bool xv_set_attrib(Display *disp, int port, const char *name, int val);
extern bool xv_get_attrib(Display *disp, int port, const char *name, int &val);


#endif // _UTIL_XV_H_
