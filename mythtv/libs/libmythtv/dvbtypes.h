/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBTYPES_H
#define DVBTYPES_H

#include <vector>
#include <map>
using namespace std;

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <cerrno>
#include <unistd.h>
#include <qdatetime.h>
#include <qstringlist.h>

#include <linux/dvb/version.h>
#if (DVB_API_VERSION != 3)
#error "DVB driver includes with API version 3 not found!"
#endif

#ifndef DVB_API_VERSION_MINOR
#define DVB_API_VERSION_MINOR 0
#endif

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "transform.h"
#include "sitypes.h"

#define MPEG_TS_PKT_SIZE 188
#define DEF_DMX_BUF_SIZE  64 * 1024
#define MAX_SECTION_SIZE 4096
#define DMX_DONT_FILTER 0x2000

typedef vector<uint16_t> dvb_pid_t;
// needs to add provider id so dvbcam doesnt require parsing
// of the pmt and or the pmtcache
typedef vector<uint16_t> dvb_caid_t;

typedef struct dvbtuning
{
    struct dvb_frontend_parameters params;
    fe_sec_voltage_t    voltage;
    fe_sec_tone_mode_t  tone;
    unsigned int diseqc_type;
    unsigned int diseqc_port;
    float        diseqc_pos;
    unsigned int lnb_lof_switch;
    unsigned int lnb_lof_hi;
    unsigned int lnb_lof_lo;
} dvb_tuning_t;

typedef struct dvbchannel
{
    dvb_tuning_t    tuning;

    PMTObject       pmt;
    bool            PMTSet;

    uint16_t        serviceID;
    uint16_t        networkID;
    uint16_t        providerID;
    uint16_t        transportID;

    QString         sistandard;
    uint8_t         version;
    pthread_mutex_t lock;
} dvb_channel_t;

typedef struct dvbstats
{
    uint32_t snr;
    uint32_t ss;
    uint32_t ber;
    uint32_t ub;

    fe_status_t  status;
} dvb_stats_t;

typedef map<uint16_t, ipack*> pid_ipack_t;

#define ERROR(args...) \
    VERBOSE(VB_IMPORTANT, QString("DVB#%1 ERROR - ").arg(cardnum) << args);

#define ERRNO(args...) \
    VERBOSE(VB_IMPORTANT, QString("DVB#%1 ERROR - ").arg(cardnum) << args\
               << endl << QString("          (%1) ").arg(errno) << strerror(errno));

#define WARNING(args...) \
    VERBOSE(VB_GENERAL, QString("DVB#%1 WARNING - ").arg(cardnum) << args);

#define GENERAL(args...) \
    VERBOSE(VB_GENERAL, QString("DVB#%1 ").arg(cardnum) << args);

#define CHANNEL(args...) \
    VERBOSE(VB_CHANNEL, QString("DVB#%1 ").arg(cardnum) << args);

#define RECORD(args...) \
    VERBOSE(VB_RECORD, QString("DVB#%1 ").arg(cardnum) << args);

#endif // DVB_TYPES_H
