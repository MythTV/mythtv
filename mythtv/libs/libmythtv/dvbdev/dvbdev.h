/* 

API for libdvbdev

(C) 2003 Ben Bucksch <http://www.bucksch.org>, written, eh, hacked for MythTV

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

// Linux includes:

// DVB includes:
#ifndef OLDSTRUCT
#define NEWSTRUCT
#endif

#ifdef NEWSTRUCT
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#else
#include <ost/dmx.h>
#include <ost/sec.h>
#include <ost/frontend.h>
#endif

#include "transform.h"
#include "tune.h"
#define IPACKS 2048

#ifdef __cplusplus
extern "C" {
#endif

#define TS_SIZE 188
// for devicenodename's parameter |type|
#define dvbdev_frontend 1
#define dvbdev_dvr 2
#define dvbdev_sec 3
#define dvbdev_demux 4
const char* devicenodename(int type, int cardnum);

// here is the meat

void set_ts_filt(int fd,uint16_t pid, dmx_pes_type_t pestype);
void ts_to_ps(uint8_t* buf, uint16_t *pids, int npids, ipack **ipacks,
              uint8_t* out_buf, int out_buf_max_len,
              int* out_buf_content_len);
void ts_to_ps_write_out(uint8_t *buf, int count, void *p);
// See also tune_it() in tune.h

#ifdef __cplusplus
}
#endif
