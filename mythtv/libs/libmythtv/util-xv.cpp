// C++ headers
#include <iostream>
#include <cstdlib>
using namespace std;

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "signalhandling.h"
#include "mythlogging.h"
#include "mythxdisplay.h"
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
    LOG(VB_GENERAL, LOG_CRIT, QString("Signal: %1").arg(sys_siglist[sig]));
    QMap<int,port_info>::iterator it;
    for (it = open_xv_ports.begin(); it != open_xv_ports.end(); ++it)
    {
        restore_port_attributes((*it).port);
        LOG(VB_GENERAL, LOG_CRIT, QString("Ungrabbing XVideo port: %1")
            .arg((*it).port));
        XvUngrabPort((*it).disp->GetDisplay(), (*it).port, CurrentTime);
    }
    QCoreApplication::exit(GENERIC_EXIT_NOT_OK);
}
static void close_all_xv_ports_signal_handler_SIGINT(void)
{
    close_all_xv_ports_signal_handler(SIGINT);
}
static void close_all_xv_ports_signal_handler_SIGTERM(void)
{
    close_all_xv_ports_signal_handler(SIGTERM);
}

void save_port_attributes(int port)
{
    if (!open_xv_ports.count(port))
        return;

    open_xv_ports[port].attribs.clear();

    int attrib_count = 0;

    MythXDisplay *disp = open_xv_ports[port].disp;
    MythXLocker lock(disp);
    XvAttribute *attributes = XvQueryPortAttributes(disp->GetDisplay(),
                                                    port, &attrib_count);
    if (!attributes || !attrib_count)
        return;

    for (int i = 0; i < attrib_count; i++)
    {
        if (!(attributes[i].flags & XvGettable))
            continue;

        int current;
        if (xv_get_attrib(disp, port, attributes[i].name, current))
            open_xv_ports[port].attribs[QString(attributes[i].name)] = current;
    }
}

void restore_port_attributes(int port, bool clear)
{
    if (!open_xv_ports.count(port))
        return;
    if (!open_xv_ports[port].attribs.size())
        return;

    MythXDisplay *disp = open_xv_ports[port].disp;
    MythXLocker lock(disp);

    QMap<QString,int>::iterator it;
    for (it  = open_xv_ports[port].attribs.begin();
         it != open_xv_ports[port].attribs.end(); ++it)
    {
        QByteArray ascii_name =  it.key().toLatin1();
        const char *cname = ascii_name.constData();
        xv_set_attrib(disp, port, cname, it.value());
    }

    if (clear)
        open_xv_ports[port].attribs.clear();
}

bool add_open_xv_port(MythXDisplay *disp, int port)
{
    bool ret = false;
    if (port >= 0)
    {
        open_xv_ports[port].disp = disp;
        open_xv_ports[port].port = port;
        QByteArray ascii_name = "XV_SET_DEFAULTS";
        const char *name = ascii_name.constData();
        ret = xv_is_attrib_supported(disp, port, name);
        SignalHandler::SetHandler(
            SIGINT, close_all_xv_ports_signal_handler_SIGINT);
        SignalHandler::SetHandler(
            SIGTERM, close_all_xv_ports_signal_handler_SIGTERM);
    }
    return ret;
}

void del_open_xv_port(int port)
{
    if (port >= 0)
    {
        open_xv_ports.remove(port);

        if (open_xv_ports.isEmpty())
        {
            SignalHandler::SetHandler(SIGINT, NULL);
            SignalHandler::SetHandler(SIGTERM, NULL);
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

bool xv_is_attrib_supported(
    MythXDisplay *disp, int port, const char *name,
    int *current_value, int *min_value, int *max_value)
{
    Atom xv_atom;
    XvAttribute *attributes;
    int attrib_count;
    int ret;

    int dummy;
    int *xv_val = (current_value) ? current_value : &dummy;

    MythXLocker lock(disp);
    attributes = XvQueryPortAttributes(disp->GetDisplay(),
                                       port, &attrib_count);
    for (int i = (attributes) ? 0 : attrib_count; i < attrib_count; i++)
    {
        if (strcmp(attributes[i].name, name))
            continue;

        if (min_value)
            *min_value = attributes[i].min_value;

        if (max_value)
            *max_value = attributes[i].max_value;

        if (!(attributes[i].flags & XvGettable))
        {
            XFree(attributes);
            return true;
        }

        xv_atom = XInternAtom(disp->GetDisplay(), name, False);
        if (None == xv_atom)
            continue;

        ret = XvGetPortAttribute(disp->GetDisplay(), port, xv_atom, xv_val);
        if (Success == ret)
        {
            XFree(attributes);
            return true;
        }
    }

    if (attributes)
        XFree(attributes);

    return false;
}

bool xv_set_attrib(MythXDisplay *disp, int port, const char *name, int val)
{
    Atom xv_atom;
    XLOCK(disp, xv_atom = XInternAtom(disp->GetDisplay(), name, False));
    if (xv_atom == None)
        return false;

    int ret;
    XLOCK(disp, ret = XvSetPortAttribute(disp->GetDisplay(),
                                         port, xv_atom, val));
    if (Success != ret)
        return false;

    return true;
}

bool xv_get_attrib(MythXDisplay *disp, int port, const char *name, int &val)
{
    Atom xv_atom;
    XLOCK(disp, xv_atom = XInternAtom(disp->GetDisplay(), name, False));
    if (xv_atom == None)
        return false;

    int ret;
    XLOCK(disp, ret = XvGetPortAttribute(disp->GetDisplay(),
                                         port, xv_atom, &val));
    if (Success != ret)
        return false;

    return true;
}
