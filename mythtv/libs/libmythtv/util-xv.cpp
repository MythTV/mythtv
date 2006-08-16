// POSIX headers
#include <signal.h>

// C++ headers
#include <iostream>
using namespace std;

// MythTH headers
#include "util-x11.h"
#include "util-xv.h"

// X11 headers
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

QMap<int,port_info> open_xv_ports;

void close_all_xv_ports_signal_handler(int sig)
{
    cerr<<"Signal: "<<sys_siglist[sig]<<endl;
    QMap<int,port_info>::iterator it;
    for (it = open_xv_ports.begin(); it != open_xv_ports.end(); ++it)
    {
        cerr<<"Ungrabbing XVideo port: "<<(*it).port<<endl;
        XvUngrabPort((*it).disp, (*it).port, CurrentTime);
    }
    exit(GENERIC_EXIT_NOT_OK);
}

void add_open_xv_port(Display *disp, int port)
{
    if (port >= 0)
    {
        open_xv_ports[port].disp = disp;
        open_xv_ports[port].port = port;
        // TODO enable more catches after 0.19 is out -- dtk
        signal(SIGINT,  close_all_xv_ports_signal_handler);
    }
}

void del_open_xv_port(int port)
{
    if (port >= 0)
    {
        open_xv_ports.remove(port);

        if (!open_xv_ports.count())
        {
            // TODO enable more catches 0.19 is out -- dtk
            signal(SIGINT, SIG_DFL);
        }
    }
}

bool has_open_xv_port(int port)
{
    return open_xv_ports.find(port) != open_xv_ports.end();
}

uint cnt_open_xv_port(void)
{
    return open_xv_ports.count();
}

QString xvflags2str(int flags)
{
    QString str("");
    if (XvInputMask == (flags & XvInputMask))
        str.append("XvInputMask ");
    if (XvOutputMask == (flags & XvOutputMask))
        str.append("XvOutputMask ");
    if (XvVideoMask == (flags & XvVideoMask))
        str.append("XvVideoMask ");
    if (XvStillMask == (flags & XvStillMask))
        str.append("XvStillMask ");
    if (XvImageMask == (flags & XvImageMask))
        str.append("XvImageMask ");
    return str;
}
