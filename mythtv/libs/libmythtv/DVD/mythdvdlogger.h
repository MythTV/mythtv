#ifndef MYTHDVDLOGGER_H
#define MYTHDVDLOGGER_H

extern "C" {
#include "dvdread/dvd_reader.h"
#include "dvdnav/dvdnav.h"
}

extern dvd_logger_cb s_dvdread_logger;
extern dvdnav_logger_cb s_dvdnav_logger;
#endif // MYTHDVDLOGGER_H
