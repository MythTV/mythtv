// -*- Mode: c++ -*-

#ifndef _UTIL_XV_H_
#define _UTIL_XV_H_

#include <QString>
#include <QMap>

#include "videobuffers.h"
#include "exitcodes.h"

class port_info
{
  public:
    MythXDisplay *disp;
    int port;
    QMap<QString,int> attribs;
};

extern QMap<int,port_info> open_xv_ports;

extern void close_all_xv_ports_signal_handler(int sig);
extern bool add_open_xv_port(MythXDisplay *disp, int port);
extern void del_open_xv_port(int port);
extern bool has_open_xv_port(int port);
extern uint cnt_open_xv_port(void);
extern QString xvflags2str(int flags);
extern bool xv_is_attrib_supported(
    MythXDisplay *disp, int port, const char *name,
    int *current_value = NULL, int *min_val = NULL, int *max_val = NULL);
extern bool xv_set_attrib(MythXDisplay *disp, int port,
                          const char *name, int val);
extern bool xv_get_attrib(MythXDisplay *disp, int port,
                          const char *name, int &val);
extern void save_port_attributes(int port);
extern void restore_port_attributes(int port, bool clear = true);

#endif // _UTIL_XV_H_
