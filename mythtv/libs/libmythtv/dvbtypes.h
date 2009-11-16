/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBTYPES_H
#define DVBTYPES_H

// POSIX headers
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <stdint.h>
#include <unistd.h>

// DVB headers
#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#ifndef DVB_API_VERSION_MINOR
#define DVB_API_VERSION_MINOR 0
#endif

#if !(DVB_API_VERSION == 3 && DVB_API_VERSION_MINOR >= 1) && DVB_API_VERSION != 5
#    error "DVB driver includes with API version 3.1 or later not found!"
#endif

// Qt headers
#include <QString>

QString toString(fe_status);

#endif // DVB_TYPES_H
