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

#endif // _UTIL_XV_H_
