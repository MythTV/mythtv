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

#include "dvbdev.h"

const char* dvbdevice(int type, int cardnum)
{
    char* frontenddev[8] =
    {
        "/dev/dvb/adapter0/frontend0",
        "/dev/dvb/adapter1/frontend0",
        "/dev/dvb/adapter2/frontend0",
        "/dev/dvb/adapter3/frontend0",
        "/dev/dvb/adapter4/frontend0",
        "/dev/dvb/adapter5/frontend0",
        "/dev/dvb/adapter6/frontend0",
        "/dev/dvb/adapter7/frontend0",
    };

    char* dvrdev[8] =
    {
        "/dev/dvb/adapter0/dvr0",
        "/dev/dvb/adapter1/dvr0",
        "/dev/dvb/adapter2/dvr0",
        "/dev/dvb/adapter3/dvr0",
        "/dev/dvb/adapter4/dvr0",
        "/dev/dvb/adapter5/dvr0",
        "/dev/dvb/adapter6/dvr0",
        "/dev/dvb/adapter7/dvr0",
    };

    char* demuxdev[8] =
    {
        "/dev/dvb/adapter0/demux0",
        "/dev/dvb/adapter1/demux0",
        "/dev/dvb/adapter2/demux0",
        "/dev/dvb/adapter3/demux0",
        "/dev/dvb/adapter4/demux0",
        "/dev/dvb/adapter5/demux0",
        "/dev/dvb/adapter6/demux0",
        "/dev/dvb/adapter7/demux0",
    };

    char* cadev[8] =
    {
        "/dev/dvb/adapter0/ca0",
        "/dev/dvb/adapter1/ca0",
        "/dev/dvb/adapter2/ca0",
        "/dev/dvb/adapter3/ca0",
        "/dev/dvb/adapter4/ca0",
        "/dev/dvb/adapter5/ca0",
        "/dev/dvb/adapter6/ca0",
        "/dev/dvb/adapter7/ca0",
    };

    char* audiodev[8] =
    {
        "/dev/dvb/adapter0/audio0",
        "/dev/dvb/adapter1/audio0",
        "/dev/dvb/adapter2/audio0",
        "/dev/dvb/adapter3/audio0",
        "/dev/dvb/adapter4/audio0",
        "/dev/dvb/adapter5/audio0",
        "/dev/dvb/adapter6/audio0",
        "/dev/dvb/adapter7/audio0",
    };

    char* videodev[8] =
    {
        "/dev/dvb/adapter0/video0",
        "/dev/dvb/adapter1/video0",
        "/dev/dvb/adapter2/video0",
        "/dev/dvb/adapter3/video0",
        "/dev/dvb/adapter4/video0",
        "/dev/dvb/adapter5/video0",
        "/dev/dvb/adapter6/video0",
        "/dev/dvb/adapter7/video0",
    };

    if (cardnum > 7)
        return 0;

    switch(type)
    {
        case DVB_DEV_FRONTEND:
            return frontenddev[cardnum];
        case DVB_DEV_DVR:
            return dvrdev[cardnum];
        case DVB_DEV_DEMUX:
            return demuxdev[cardnum];
        case DVB_DEV_CA:
            return cadev[cardnum];
        case DVB_DEV_AUDIO:
            return audiodev[cardnum];
        case DVB_DEV_VIDEO:
            return videodev[cardnum];
    }

    return 0;
}

