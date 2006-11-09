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

// C++ headers
#include <vector>
#include <map>
using namespace std;

// Qt headers
#include <qdatetime.h>
#include <qstringlist.h>
#include <qmutex.h>

// MythTV headers
#include "mpegdescriptors.h"
#include "mpegtables.h"
#include "dtvchannel.h"

// DVB headers
#include <linux/dvb/version.h>
#if (DVB_API_VERSION != 3)
#error "DVB driver includes with API version 3 not found!"
#endif

#ifndef DVB_API_VERSION_MINOR
#define DVB_API_VERSION_MINOR 0
#endif

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#if (DVB_API_VERSION >= 3 && DVB_API_VERSION_MINOR >= 1)
#    define USE_ATSC
#else
#warning DVB API version < 3.1
#warning ATSC will not be supported using the Linux DVB drivers
#    define FE_ATSC       (FE_OFDM+1)
#    define FE_CAN_8VSB   0x200000
#    define FE_CAN_16VSB  0x400000
#    define VSB_8         (fe_modulation)(QAM_AUTO+1)
#    define VSB_16        (fe_modulation)(QAM_AUTO+2)
#endif

#define MPEG_TS_PKT_SIZE 188
#define DEF_DMX_BUF_SIZE  64 * 1024
#define MAX_SECTION_SIZE 4096
#define DMX_DONT_FILTER 0x2000

#ifdef FE_GET_EXTENDED_INFO
  #define dvb_fe_params dvb_frontend_parameters_new
#else
  #define dvb_fe_params dvb_frontend_parameters
#endif

QString toString(const fe_type_t);
QString toString(const struct dvb_fe_params&, const fe_type_t);
QString toString(fe_status);
QString toString(const struct dvb_frontend_event&, const fe_type_t);

typedef vector<uint16_t> dvb_pid_t;
// needs to add provider id so dvbcam doesnt require parsing
// of the pmt and or the pmtcache
typedef vector<uint16_t> dvb_caid_t;

#endif // DVB_TYPES_H
