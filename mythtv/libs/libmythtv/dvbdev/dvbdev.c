/* 

dvbstream - RTP-ize a DVB transport stream.
(C) Dave Chapman <dave@dchapman.com> 2001, 2002.

The latest version can be found at <http://www.linuxstb.org/dvbstream>

Forked and stripped by Ben Bucksch <http://www.bucksch.org> for MythTV

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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <resolv.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <values.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "dvbdev.h"
#include "transform.h"

// There seems to be a limit of 8 simultaneous filters in the driver
#define MAX_CHANNELS 8
#define IPACKS 2048

const char* devicenodename(int type, int cardnum)
{
#ifdef NEWSTRUCT
char* frontenddev[4]={"/dev/dvb/adapter0/frontend0","/dev/dvb/adapter1/frontend0","/dev/dvb/adapter2/frontend0","/dev/dvb/adapter3/frontend0"};
char* dvrdev[4]={"/dev/dvb/adapter0/dvr0","/dev/dvb/adapter1/dvr0","/dev/dvb/adapter2/dvr0","/dev/dvb/adapter3/dvr0"};
char* demuxdev[4]={"/dev/dvb/adapter0/demux0","/dev/dvb/adapter1/demux0","/dev/dvb/adapter2/demux0","/dev/dvb/adapter3/demux0"};
#else
char* frontenddev[4]={"/dev/ost/frontend0","/dev/ost/frontend1","/dev/ost/frontend2","/dev/ost/frontend3"};
char* dvrdev[4]={"/dev/ost/dvr0","/dev/ost/dvr1","/dev/ost/dvr2","/dev/ost/dvr3"};
char* secdev[4]={"/dev/ost/sec0","/dev/ost/sec1","/dev/ost/sec2","/dev/ost/sec3"};
char* demuxdev[4]={"/dev/ost/demux0","/dev/ost/demux1","/dev/ost/demux2","/dev/ost/demux3"};
#endif

  if (cardnum > 3)
    return "no such device number";

  switch(type)
  {
    case dvbdev_frontend:
      return frontenddev[cardnum];
    case dvbdev_dvr:
      return dvrdev[cardnum];
    case dvbdev_sec:
#ifdef NEWSTRUCT
      return "not valid in newstruct";
#else
      return secdev[cardnum];
#endif
    case dvbdev_demux:
      return demuxdev[cardnum];
  }
  return "not reached";
}

void set_ts_filt(int fd,uint16_t pid, dmx_pes_type_t pestype)
{
  struct dmx_pes_filter_params pesFilterParams;

  pesFilterParams.pid     = pid;
  pesFilterParams.input   = DMX_IN_FRONTEND;
  pesFilterParams.output  = DMX_OUT_TS_TAP;
#ifdef NEWSTRUCT
  pesFilterParams.pes_type = pestype;
#else
  pesFilterParams.pesType = pestype;
#endif
  pesFilterParams.flags   = DMX_IMMEDIATE_START;

  if (ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)  {
    fprintf(stderr,"FILTER %i: ",pid);
    perror("DMX SET PES FILTER");
  }
}

struct append_buffer
{
  uint8_t* memory;
  int buf_len;
  int content_len;
};

// utility for ts_to_ps
void ts_to_ps_write_out(uint8_t *buf, int count, void *p)
{
  struct append_buffer* app_buf = (struct append_buffer*)p;
  if (app_buf->content_len + count > app_buf->buf_len)
  {
    fprintf(stderr, "ts_to_ps() output buffer too short\n");
    return;
  }
  memcpy(app_buf->memory + app_buf->content_len,
         buf,
         count);
  app_buf->content_len += count;
}

/**
 * Demux
 * Accepts a transport stream, filters everything uninteresting out,
 * converts the rest into a program stream and writes the result into
 * result_buf.
 *
 * @param buf        Transport stream packet read from the dvr device.
 *                   Length TS_SIZE
 * @param pids       Array of length npids.
 *                   Program ID for video and audio streams of the channel
 * @param npids      Number of PIDs to filter for
 * @param ipacks[]   (inout) Array of length npids.
 *                   Provide initialized ipacks for internal use, using
 *                   ts_to_ps_write_out() as callback and 1 as ps.
 *                   This *must* stay the same object over subsequent
 *                   calls of this function. E.g.
 *                        ipacks pv[1];
 *                        init_ipack(&(pv[0]), IPACKS, ts_to_ps_write_out, 1);
 *                        while (read())
 *                            ts_to_ps(...,1, &pv, ...);
 * @param out_buf    (out) The program stream will be written there.
 * @param out_buf_max_len      Length of the buffer, amount of allocated memory
 * @param out_buf_content_len  (out) The length of the actual data in out_buf
 *                             will be written here.
 */
void ts_to_ps(uint8_t* buf, uint16_t *pids, int npids, ipack **ipacks,
              uint8_t* out_buf, int out_buf_max_len,
              int* out_buf_content_len)
{
  /* This will be called a lot (even for channels we're not interested in),
     so efficiency is important until the |else return;|. */
  uint16_t pid;
  uint8_t off = 0;
  ipack *p = 0;
  int i;
  struct append_buffer app_buf;

  if (!(buf[3] & 0x10)) // no payload?
    return;
  pid = get_pid(buf+1);
  for (i = 0; i < npids; i++)
    if (pid == pids[i])
      p = &((*ipacks)[i]);
  if (p == 0)
    return;

  app_buf.memory = out_buf;
  app_buf.buf_len = out_buf_max_len;
  app_buf.content_len = 0;

  p->data = &app_buf;

  if (buf[1] & 0x40 && p->plength == MMAX_PLENGTH-6)
  {
    p->plength = p->found-6;
    p->found = 0;
    send_ipack(p);
    reset_ipack(p);
  }

  if (buf[3] & 0x20)  // adaptation field?
    off = buf[4] + 1;

  /* This *CRAPPY* function wants to write the data to a callback, although
     it's syncronous. But I just want to pass back the result data to the
     caller immediately and directly. In order not to have to make massive
     changes to transform.c, I'll do a little workaround: I'll play his game
     and use a callback, but that will only write to a buffer that I create
     here and reads immediately after instant_repack returned. I have to
     avoid globals vars, though, to allow concurrent processing of different
     channels, so I'll have to pass the buffer to the callback. Luckily, it
     accepts a void*, which is filled by instant_repack's helper functions
     with p->data, which luckily is not used for any other purpose in this
     case. So, I'll put a reference to the buffer into p->data and get it
     later in the callback using the void pointer. To avoid excessive
     creation/destruction of buffers, I'll create the buffer in the caller
     and pass it here using result_buf. */
  instant_repack(buf+4+off, TS_SIZE-4-off, p);

  *out_buf_content_len = app_buf.content_len;
}
