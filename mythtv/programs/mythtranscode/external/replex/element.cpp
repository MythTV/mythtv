/*
 * element.c: MPEG ELEMENTARY STREAM functions for replex
 *        
 *
 * Copyright (C) 2003 Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
 * Changes to use MythTV logging
 * Copyright (C) 2011 Gavin Hurlbut <ghurlbut@mythtv.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <cstdio>
#include <cstring>

#include "element.h"
#include "mpg_common.h"
#include "pes.h"

#include "libmythbase/mythlogging.h"

std::array<unsigned int,4> slotsPerLayer {12, 144, 0, 0};
std::array<std::array<unsigned int,16>,3> bitrates {{
 {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}
}};

static const std::array<const uint32_t,4> freq    {441, 480, 320, 0};
static const std::array<const uint64_t,4> samples {384, 1152, 1152, 1536};

static const std::array<const uint16_t,32> ac3_bitrates
    {32,40,48,56,64,80,96,112,128,160,192,224,256,320,384,448,512,576,640,
     0,0,0,0,0,0,0,0,0,0,0,0,0};
static const std::array<const uint8_t,12> ac3half  {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
static const std::array<const uint32_t,4> ac3_freq {480, 441, 320, 0};

#define DEBUG true

uint64_t add_pts_audio(uint64_t pts, audio_frame_t *aframe, uint64_t frames)
{
	int64_t newpts=0;

	newpts = (pts + ((frames *samples [3-aframe->layer] * 27000000ULL) 
		  / aframe->frequency));
	return newpts>0 ? newpts%MAX_PTS2: MAX_PTS2+newpts;
}

void fix_audio_count(uint64_t *acount, audio_frame_t *aframe, uint64_t origpts, uint64_t pts)
{
	uint64_t di = (samples [3-aframe->layer] * 27000000ULL);
	int64_t diff = ptsdiff(origpts,pts);
	int c=(aframe->frequency * diff+di/2)/di;
	if (c)
            LOG(VB_GENERAL, LOG_INFO, QString("fix audio frames %1").arg(c));
	*acount += c;
}


uint64_t next_ptsdts_video(uint64_t *pts, sequence_t *s, uint64_t fcount, uint64_t gcount)
{
	int64_t newdts = 0;
	int64_t newpts = 0;
	int64_t fnum = s->current_tmpref - gcount + fcount;

	
	if ( s->pulldown == NOPULLDOWN ) {
		newdts = ( ((fcount-1) * SEC_PER) + *pts);
		newpts = (((fnum ) * SEC_PER) + *pts);
	} else {
		uint64_t extra_time = 0;
#if 0
		LOG(VB_GENERAL, LOG_INFO, QString("pulldown %1 %2")
		    .arg(fcount-1).arg(fnum-1));
#endif

		if ( s->pulldown == PULLDOWN32)
			extra_time = SEC_PER;
		else 
			extra_time = 3*SEC_PER/2;

		newdts = ((fcount - 1) * 5ULL * SEC_PER / 4ULL) + 
			(((fcount - 1)%2)*extra_time) + 
			*pts;

		if ((s->pulldown == PULLDOWN23) && (fcount-1))
			newdts -= SEC_PER/2;

		newpts = SEC_PER +
			((fnum -1) * 5ULL * SEC_PER / 4ULL) + 
			(((fnum - 1)%2)*extra_time) + 
			*pts;
		
	}


	*pts = newpts >= 0 ? newpts%MAX_PTS2: MAX_PTS2+newpts;
	return newdts >= 0 ? newdts%MAX_PTS2: MAX_PTS2+newdts;
}

void fix_video_count(sequence_t *s, uint64_t *frame, uint64_t origpts, uint64_t pts,
		     uint64_t origdts, uint64_t dts)
{
	int64_t pdiff = 0;
	int64_t ddiff = 0;
	int64_t pframe = 0;
	int64_t dframe = 0;
	int psig=0;
	int dsig=0;
	int64_t fr=0;

	pdiff = ptsdiff(origpts,pts);
	ddiff = ptsdiff(origdts,dts);
	psig = static_cast<int>(pdiff > 0);
	dsig = static_cast<int>(ddiff > 0);
	if (!psig) pdiff = -pdiff;
	if (!dsig) ddiff = -ddiff;

	if ( s->pulldown == NOPULLDOWN ) {
		dframe = (ddiff+SEC_PER/2ULL) / SEC_PER;
		pframe = (pdiff+SEC_PER/2ULL) / SEC_PER;
	} else {
		dframe = (4ULL*ddiff/5ULL+SEC_PER/2ULL) / SEC_PER;
		pframe = (4ULL*pdiff/5ULL+SEC_PER/2ULL) / SEC_PER;
	}

	if (!psig) fr = -(int)pframe;
	else fr = (int)pframe;
	if (!dsig) fr -= (int)dframe;
	else fr += (int)dframe;
	*frame = *frame + (fr/2);
	if (fr/2)
		LOG(VB_GENERAL, LOG_INFO,
			QString("fixed video frame %1").arg(fr/2));
}

	
void pts2time(uint64_t pts, uint8_t *buf, int len)
{
	int c = 0;

	pts = (pts/300)%MAX_PTS;
	uint8_t h = (uint8_t)(pts/90000)/3600;
	uint8_t m = (uint8_t)((pts/90000)%3600)/60;
	uint8_t s = (uint8_t)((pts/90000)%3600)%60;

	while (c+7 < len){
		if (buf[c] == 0x00 && buf[c+1] == 0x00 && buf[c+2] == 0x01 && 
		    buf[c+3] == GROUP_START_CODE && (buf[c+5] & 0x08)){
			buf[c+4] &= ~(0x7F);
			buf[c+4] |= (h & 0x1F) << 2;
			buf[c+4] |= (m & 0x30) >> 4;

			buf[c+5] &= ~(0xF7);
			buf[c+5] |= (m & 0x0F) << 4;
			buf[c+5] |= (s & 0x38) >> 3;

			buf[c+6] &= ~(0xE0);
			buf[c+6] |= (s & 0x07) << 5;

/* 1hhhhhmm|mmmm1sss|sss */
#if 0
			c+=4;
			LOG(VB_GENERAL, LOG_INFO, "fixed time");
			LOG(VB_GENERAL, LOG_INFO, QString("%1:%2:%3")
			    .arg((int)((buf[c]>>2)& 0x1F), 2,10,QChar('0'))
			    .arg((int)(((buf[c]<<4)& 0x30)|((buf[c+1]>>4)& 0x0F)),   2,10,QChar('0'))
			    .arg((int)(((buf[c+1]<<3)& 0x38)|((buf[c+2]>>5)& 0x07)), 2,10,QChar('0')));
#endif
			c = len;


		} else {
			c++;
		}
	}

}


int get_video_info(ringbuffer *rbuf, sequence_t *s, int off, int le)
{
        std::vector<uint8_t> buf;
        buf.resize(150);
        int form = -1;
        int c = 0;

        s->set = 0;
        s->ext_set = 0;
	s->pulldown_set = 0;
        int re = ring_find_mpg_header(rbuf, SEQUENCE_HDR_CODE, off, le);
        if (re < 0)
		return re;
	uint8_t *headr = buf.data()+4;
	if (ring_peek(rbuf, buf, off) < 0) return -2;
	
	s->h_size	= ((headr[1] &0xF0) >> 4) | (headr[0] << 4);
	s->v_size	= ((headr[1] &0x0F) << 8) | (headr[2]);
    
        int sw = ((headr[3]&0xF0) >> 4) ;

	if (DEBUG){
		switch( sw ){
		case 1:
			LOG(VB_GENERAL, LOG_INFO, "Video: aspect ratio: 1:1");
			s->aspect_ratio = 100;        
			break;
		case 2:
			LOG(VB_GENERAL, LOG_INFO, "Video: aspect ratio: 4:3");
			s->aspect_ratio = 133;        
			break;
		case 3:
			LOG(VB_GENERAL, LOG_INFO, "Video: aspect ratio: 16:9");
			s->aspect_ratio = 177;        
			break;
		case 4:
			LOG(VB_GENERAL, LOG_INFO,
			    "Video: aspect ratio: 2.21:1");
			s->aspect_ratio = 221;        
			break;
			
		case 5 ... 15:
			LOG(VB_GENERAL, LOG_INFO,
			    "Video: aspect ratio: reserved");
			s->aspect_ratio = 0;        
			break;
			
		default:
			s->aspect_ratio = 0;        
			return -3;
		}
	}

        if (DEBUG)
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("  size = %1x%2").arg(s->h_size).arg(s->v_size));

        sw = (int)(headr[3]&0x0F);

        switch ( sw ) {
	case 1:
                s->frame_rate = 23976;
		form = -1;
		break;
	case 2:
                s->frame_rate = 24000;
		form = -1;
		break;
	case 3:
                s->frame_rate = 25000;
		form = VIDEO_PAL;
		break;
	case 4:
                s->frame_rate = 29970;
		form = VIDEO_NTSC;
		break;
	case 5:
                s->frame_rate = 30000;
		form = VIDEO_NTSC;
		break;
	case 6:
                s->frame_rate = 50000;
		form = VIDEO_PAL;
		break;
	case 7:
                s->frame_rate = 60000;
		form = VIDEO_NTSC;
		break;
	}
	if (DEBUG)
            LOG(VB_GENERAL, LOG_DEBUG, QString("  frame rate: %1 fps")
		    .arg(s->frame_rate/1000.0, 2,'f',3,QChar('0')));

	s->bit_rate = (((headr[4] << 10) & 0x0003FC00UL) 
		       | ((headr[5] << 2) & 0x000003FCUL) | 
		       (((headr[6] & 0xC0) >> 6) & 0x00000003UL));
	
        if (DEBUG)
            LOG(VB_GENERAL, LOG_DEBUG, QString("  bit rate: %1 Mbit/s")
                .arg(400*(s->bit_rate)/1000000.0, 0,'f',2,QChar('0')));

        s->video_format = form;

                

	s->vbv_buffer_size = (( headr[7] & 0xF8) >> 3 ) | (( headr[6] & 0x1F )<< 5);	
	s->flags	   = ( headr[7] & 0x06);	
	if (DEBUG)
            LOG(VB_GENERAL, LOG_DEBUG, QString("  vbvbuffer %1")
                .arg(16*1024*(s->vbv_buffer_size)));

	c += 8;
	if ( !(s->flags & INTRAQ_FLAG) ) 
		s->flags = ( headr[7] & 0x07);
	else {
		s->flags |= headr[c+63] & 0x01;
		memset(s->intra_quant, 0, 64);
		for (int i=0;i<64;i++)
			s->intra_quant[i] |= (headr[c+i] >> 1) |
				(( headr[c-1+i] & 0x01) << 7);

		c += 64;
	}
	if (s->flags & NONINTRAQ_FLAG){
		memcpy(s->non_intra_quant, headr+c, 64);
		c += 64;
	}
	s->set=1;

	return c;
}

int find_audio_sync(ringbuffer *rbuf, audio_sync_buf &buf, int off, int type, int le)
{
	int found = 0;
	int l=0;

	buf.fill(0);
	uint8_t b1 = 0x00;
	uint8_t b2 = 0x00;
	uint8_t m2 = 0xFF;
	switch(type){
	case AC3:
		b1 = 0x0B;
		b2 = 0x77;
		l = 6;
		break;

	case MPEG_AUDIO:
		b1 = 0xFF;
		b2 = 0xF8;
		m2 = 0xF8;
		l = 4;
		break;

	default:
		return -1;
	}

	int c = off;
	while ( c-off < le){
		uint8_t b = 0;
		if (mring_peek(rbuf, &b, 1, c) <0) return -1;
		switch(found){

		case 0:
			if ( b == b1) found = 1;
			break;
			
		case 1:
			if ( (b&m2) == b2){
				if (mring_peek(rbuf, buf.data(), l, c-1) < -1)
					return -2;
				return c-1-off;	
			} else if ( b != b1) {
				found = 0;
			}
		}
		c++;
	}	
	if (found) return -2;
	return -1;
}

int find_audio_s(const uint8_t *rbuf, int off, int type, int le)
{
	int found = 0;

	uint8_t b1 = 0x00;
	uint8_t b2 = 0x00;
	uint8_t m2 = 0xFF;
	switch(type){
	case AC3:
		b1 = 0x0B;
		b2 = 0x77;
		break;

	case MPEG_AUDIO:
		b1 = 0xFF;
		b2 = 0xF8;
		m2 = 0xF8;
		break;

	default:
		return -1;
	}

	int c = off;
	while ( c < le){
		uint8_t b=rbuf[c];
		switch(found){
		case 0:
			if ( b == b1) found = 1;
			break;
			
		case 1:
			if ( (b&m2) == b2){
				return c-1;	
			} else if ( b != b1) {
				found = 0;
			}
		}
		c++;
	}	
	if (found) return -2;
	return -1;
}

int check_audio_header(ringbuffer *rbuf, audio_frame_t * af, int  off, int le, 
		       int type)
{
	audio_sync_buf headr {};
	uint8_t frame = 0;
	int fr = 0;
	int half = 0;

	int c = find_audio_sync(rbuf, headr, off, type, le);
	if (c != 0 ) {
		if (c==-2){
			LOG(VB_GENERAL, LOG_ERR, "Incomplete audio header");
			return -2;
		}
		LOG(VB_GENERAL, LOG_ERR, "Error in audio header");
		return -1;
	}
	switch (type){

	case MPEG_AUDIO:
		if ( af->layer != ((headr[1] & 0x06) >> 1) ){
			if ( headr[1] == 0xff){
				return -3;
			}
#ifdef IN_DEBUG
                        LOG(VB_GENERAL, LOG_ERR, "Wrong audio layer");
#endif
                        return -1;
		}
		if ( af->bit_rate != 
		     (bitrates[(3-af->layer)][(headr[2] >> 4 )]*1000)){
#ifdef IN_DEBUG
			LOG(VB_GENERAL, LOG_ERR, "Wrong audio bit rate");
#endif
			return -1;
		}
		break;
		
	case AC3:
		frame = (headr[4]&0x3F);
		if (af->bit_rate != ac3_bitrates[frame>>1]*1000){
#ifdef IN_DEBUG
			LOG(VB_GENERAL, LOG_ERR, "Wrong audio bit rate");
#endif
			return -1;
		}
		half = ac3half[headr[5] >> 3];
		fr = (headr[4] & 0xc0) >> 6;
		if (af->frequency != ((ac3_freq[fr] *100) >> half)){
#ifdef IN_DEBUG
			LOG(VB_GENERAL, LOG_ERR, "Wrong audio frequency");
#endif
			return -1;
		}
		
		break;

	}

	return 0;
}


int get_audio_info(ringbuffer *rbuf, audio_frame_t *af, int off, int le) 
{
	int fr =0;
	audio_sync_buf headr {};

	af->set=0;

	int c = find_audio_sync(rbuf, headr, off, MPEG_AUDIO,le);
	if (c < 0 )
		return c;

	af->layer = (headr[1] & 0x06) >> 1;

        if (DEBUG)
            LOG(VB_GENERAL, LOG_DEBUG, QString("Audiostream: layer: %1")
                .arg(4-af->layer));


	af->bit_rate = bitrates[(3-af->layer)][(headr[2] >> 4 )]*1000;

	if (DEBUG){
		if (af->bit_rate == 0)
			LOG(VB_GENERAL, LOG_DEBUG, "  Bit rate: free");
		else if (af->bit_rate == 0xf)
			LOG(VB_GENERAL, LOG_DEBUG, "  BRate: reserved");
		else
                        LOG(VB_GENERAL, LOG_DEBUG, QString("  BRate: %1 kb/s")
			    .arg(af->bit_rate/1000));
	}

	fr = (headr[2] & 0x0c ) >> 2;
	af->frequency = freq[fr]*100;
	
	if (DEBUG){
		if (af->frequency == 3)
			LOG(VB_GENERAL, LOG_DEBUG, "  Freq: reserved");
		else
                        LOG(VB_GENERAL, LOG_DEBUG, QString("  Freq: %1 kHz")
			    .arg(af->frequency/1000.0, 2,'f',1,QChar('0')));
	}
	af->off = c;
	af->set = 1;

	af->frametime = ((samples [3-af->layer] * 27000000ULL) / af->frequency);
	af->framesize = af->bit_rate * slotsPerLayer[3-af->layer]/ af->frequency;
	LOG(VB_GENERAL, LOG_INFO, QString(" frame size: %1").arg(af->framesize));
	printpts(af->frametime);

	return c;
}

int get_ac3_info(ringbuffer *rbuf, audio_frame_t *af, int off, int le)
{
	audio_sync_buf headr {};

	af->set=0;

	int c = find_audio_sync(rbuf, headr, off, AC3, le);
	if (c < 0) 
		return c;

	af->off = c;

	af->layer = 0;  // 0 for AC3

	if (DEBUG)
		LOG(VB_GENERAL, LOG_DEBUG, "AC3 stream:");
	uint8_t frame = (headr[4]&0x3F);
	af->bit_rate = ac3_bitrates[frame>>1]*1000;
	int half = ac3half[headr[5] >> 3];
	if (DEBUG)
                LOG(VB_GENERAL, LOG_DEBUG, QString("  bit rate: %1 kb/s")
		    .arg(af->bit_rate/1000));
	int fr = (headr[4] & 0xc0) >> 6;
	af->frequency = (ac3_freq[fr] *100) >> half;
	
	if (DEBUG)
		LOG(VB_GENERAL, LOG_DEBUG,
		    QString("  freq: %1 Hz").arg(af->frequency));

	switch (headr[4] & 0xc0) {
	case 0:
		af->framesize = 4 * af->bit_rate/1000;
		break;

	case 0x40:
		af->framesize = 2 * (320 * af->bit_rate / 147000 + (frame & 1));
		break;

	case 0x80:
		af->framesize = 6 * af->bit_rate/1000;
		break;
	}

	if (DEBUG)
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("  frame size %1").arg(af->framesize));

	af->off = c;
	af->set = 1;

	//FIXME calculate frametime
	af->frametime = 0;

    return c;
}


int get_video_ext_info(ringbuffer *rbuf, sequence_t *s, int off, int le)
{
        std::vector<uint8_t> buf;
        buf.resize(12);

        int re =ring_find_mpg_header(rbuf, EXTENSION_START_CODE, off, le);
        if (re < 0) {
		LOG(VB_GENERAL, LOG_ERR, "Error in find_mpg_header");
		return re;
	}

	if (ring_peek(rbuf, buf, 5, off) < 0) return -2;
	uint8_t *headr=buf.data()+4;
	
	uint8_t ext_id = (headr[0]&0xF0) >> 4;

	switch (ext_id){
	case SEQUENCE_EXTENSION:{
		if (s->ext_set || !s->set) break;
		if (ring_peek(rbuf, buf, 10, off) < 0) return -2;
		headr=buf.data()+4;

		if (DEBUG)
			LOG(VB_GENERAL, LOG_DEBUG, "Sequence Extension:");
		s->profile = ((headr[0]&0x0F) << 4) | ((headr[1]&0xF0) >> 4);
		if (headr[1]&0x08) s->progressive = 1;
		else s->progressive = 0;
		s->chroma = (headr[1]&0x06)>>1;
		if (DEBUG){
			switch(s->chroma){
			case 0:
				LOG(VB_GENERAL, LOG_DEBUG, " chroma reserved ");
				break;
			case 1:
				LOG(VB_GENERAL, LOG_DEBUG, " chroma 4:2:0 ");
				break;
			case 2:
				LOG(VB_GENERAL, LOG_DEBUG, " chroma 4:2:2 ");
				break;
			case 3:
				LOG(VB_GENERAL, LOG_DEBUG, " chroma 4:4:4 ");
				break;
			}
		}
		
		uint16_t hsize = ((headr[1]&0x01)<<12) | ((headr[2]&0x80)<<6);
		uint16_t vsize = ((headr[2]&0x60)<<7);
		s->h_size	|= hsize;
		s->v_size	|= vsize;
		if (DEBUG)
                        LOG(VB_GENERAL, LOG_DEBUG, QString("  size = %1x%2")
                            .arg(s->h_size).arg(s->v_size));
		
		uint32_t bitrate = ((headr[2]& 0x1F) << 25) | (( headr[3] & 0xFE ) << 17);
		s->bit_rate |= bitrate;
	
		if (DEBUG)
			LOG(VB_GENERAL, LOG_DEBUG, QString("  bit rate: %1 Mbit/s")
			    .arg(400.0*(s->bit_rate)/1000000.0, 0,'f',2,QChar('0')));


		uint32_t vbvb = (headr[4]<<10);
		s->vbv_buffer_size |= vbvb;
		if (DEBUG)
			LOG(VB_GENERAL, LOG_DEBUG, QString("  vbvbuffer %1")
			    .arg(16*1024*(s->vbv_buffer_size)));
		uint8_t fr_n = (headr[5] & 0x60) >> 6;
		uint8_t fr_d = (headr[5] & 0x1F);
	
		s->frame_rate = s->frame_rate * (fr_n+1) / (fr_d+1);
		if (DEBUG)
			LOG(VB_GENERAL, LOG_DEBUG, QString("  frame rate: %1")
			    .arg(s->frame_rate/1000.0, 2,'f',3,QChar('0')));
		s->ext_set=1;
		break;
	}

	case SEQUENCE_DISPLAY_EXTENSION:
		break;

	case PICTURE_CODING_EXTENSION:{
		int pulldown = 0;

		if (!s->set || s->pulldown_set) break;
		if (ring_peek(rbuf, buf, 10, off) < 0) return -2;
		headr=buf.data()+4;
		
		if ( (headr[2]&0x03) != 0x03 ) break; // not frame picture
		if ( (headr[3]&0x02) ) pulldown = 1; // repeat flag set => pulldown

		if (pulldown){
			if (s->current_tmpref)
				s->pulldown = PULLDOWN23;
			else
				s->pulldown = PULLDOWN32;
			s->pulldown_set = 1;
		} 
		if (DEBUG){
			switch (s->pulldown) {
			case PULLDOWN32:
				LOG(VB_GENERAL, LOG_DEBUG,
				    "Picture Coding Extension: "
				    "3:2 pulldown detected");
				break;
			case PULLDOWN23:
				LOG(VB_GENERAL, LOG_DEBUG,
				    "Picture Coding Extension: "
				    "2:3 pulldown detected");
				break;
#if 0
			default:
				LOG(VB_GENERAL, INFO_DEBUG,
				    " no pulldown detected");
#endif
			}
		}
		break;
	}
	case QUANT_MATRIX_EXTENSION:
	case PICTURE_DISPLAY_EXTENSION:
		break;
	}
	
	
	return ext_id;
}

