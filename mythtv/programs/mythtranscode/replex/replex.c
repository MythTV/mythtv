/*
 * replex.c
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
//#define IN_DEBUG 


#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "replex.h"
#include "pes.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

#ifdef USING_MINGW
# define S_IRGRP 0
# define S_IWGRP 0
# define S_IROTH 0
# define S_IWOTH 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#include "mythlogging.h"

static int replex_all_set(struct replex *rx);

static int replex_check_id(struct replex *rx, uint16_t id)
{
	int i;

	if (id==rx->vpid)
		return 0;

	for (i=0; i<rx->apidn; i++)
		if (id==rx->apid[i])
			return i+1;

	for (i=0; i<rx->ac3n; i++)
		if (id==rx->ac3_id[i])
			return i+0x80;

	return -1;
}

static int encode_mp2_audio(audio_frame_t *aframe, uint8_t *buffer, int bufsize)
{
	AVCodec *codec;
	AVCodecContext *c= NULL;
	int frame_size, j, out_size;
	short *samples;
	
	LOG(VB_GENERAL, LOG_INFO, "encoding an MP2 audio frame");

	/* find the MP2 encoder */
	codec = avcodec_find_encoder(CODEC_ID_MP2);
	if (!codec) {
		LOG(VB_GENERAL, LOG_ERR, "codec not found");
		return 1;
	}
 
	c = avcodec_alloc_context();

	/* put sample parameters */
	c->bit_rate = aframe->bit_rate;
	c->sample_rate = aframe->frequency;
	c->channels = 2;
	c->sample_fmt = AV_SAMPLE_FMT_S16;

    /* open it */
	if (avcodec_open(c, codec) < 0) {
		LOG(VB_GENERAL, LOG_ERR, "could not open codec");
		av_free(c);
		return 1;
	}

	/* the codec gives us the frame size, in samples */
	frame_size = c->frame_size;
	samples = malloc(frame_size * 2 * c->channels);
	
	/* create samples for a single blank frame */
	for (j=0;j<frame_size;j++) {
		samples[2*j] = 0;
		samples[2*j+1] = 0;
	}
		
	/* encode the samples */
	out_size = avcodec_encode_audio(c, buffer, bufsize, samples);
	
	if (out_size != bufsize) {
		LOG(VB_GENERAL, LOG_ERR,
		    "frame size (%d) does not equal required size (%d)?",
			out_size, bufsize);
		free(samples);
		avcodec_close(c);
		av_free(c);
		return 1;
	}

	free(samples);
	avcodec_close(c);
	av_free(c);
	
	return 0;
}

static void analyze_audio( pes_in_t *p, struct replex *rx, int len, int num, int type)
{
	int c=0;
	int pos=0;
	audio_frame_t *aframe = NULL;
	index_unit *iu = NULL;
	ringbuffer *rbuf = NULL, *index_buf = NULL;
	uint64_t *acount=NULL;
	uint64_t *fpts=NULL;
	uint64_t *lpts=NULL;
	int bsize = 0;
	int first = 1;
	uint8_t buf[7];
	int off=0;
	int *apes_abort=NULL;
	int re=0;
	
	switch ( type ){
	case AC3:
#ifdef IN_DEBUG
		LOG(VB_GENERAL, LOG_DEBUG, "AC3");
#endif
		aframe = &rx->ac3frame[num];	
		iu = &rx->current_ac3index[num];
		rbuf = &rx->ac3rbuffer[num];
		index_buf = &rx->index_ac3rbuffer[num];
		acount = &rx->ac3frame_count[num];
		fpts = &rx->first_ac3pts[num];
		lpts = &rx->last_ac3pts[num];
		bsize = rx->ac3buf;
		apes_abort = &rx->ac3pes_abort[num];
		break;

	case MPEG_AUDIO:
#ifdef IN_DEBUG
		LOG(VB_GENERAL, LOG_DEBUG, "MPEG AUDIO");
#endif
		aframe = &rx->aframe[num];	
		iu = &rx->current_aindex[num];
		rbuf = &rx->arbuffer[num];
		index_buf = &rx->index_arbuffer[num];
		acount = &rx->aframe_count[num];
		fpts = &rx->first_apts[num];
		lpts = &rx->last_apts[num];
		bsize = rx->audiobuf;
		apes_abort = &rx->apes_abort[num];
		break;
	}
	
	*apes_abort = 0;
	off = ring_rdiff(rbuf, p->ini_pos);
	while (c < len){
		if ( (pos = find_audio_sync(rbuf, buf, c+off, type, len-c) ) 
		     >= 0 ){
			if (!aframe->set){
				switch( type ){
				case AC3:
					re = get_ac3_info(rbuf, aframe, 
							  pos+c+off,
							  len-c-pos);
					break;
				case MPEG_AUDIO:
					re = get_audio_info(rbuf, aframe, 
							    pos+c+off,
							    len-c-pos);
					break;
				}
				if ( re == -2){
					*apes_abort = len -c;
					return;
				}
				if (re < 0) return;


				if (!rx->ignore_pts){
					if ((p->flag2 & PTS_ONLY)){
						*fpts = trans_pts_dts(p->pts);
						LOG(VB_GENERAL, LOG_INFO,
							"starting audio PTS: ");
						printpts(*fpts);
					} else aframe->set = 0;
				}

				if (aframe->set && first)
					ring_skip(rbuf,pos+c);
				
			} else {
				int diff = ring_posdiff(rbuf, iu->start, 
							p->ini_pos + pos+c);
				
				if ( (re = check_audio_header(rbuf, aframe, 
							     pos+c+off,len-c-pos,
							     type)) < 0){
			
					if ( re == -2){
						*apes_abort = len -c;
						return;
					}
					
					if ((int) aframe->framesize > diff){
						if ( re == -3){
							c += pos+1;
							continue;
						}
					
						c += pos+2;
#ifdef IN_DEBUG
						LOG(VB_GENERAL, LOG_DEBUG,"
						    WRONG HEADER1 %d", diff);
#endif
						continue;
					}
				}
				if ((int) aframe->framesize > diff){
					c += pos+2;
#if 0
					LOG(VB_GENERAL, LOG_ERR,
					    "WRONG HEADER2 %d", diff);
#endif
					continue;
				}
			}
			
			// try to fix audio sync - only works for mpeg audio for now 
			if (aframe->set && rx->fix_sync && first && type == MPEG_AUDIO){
				int frame_time = aframe->frametime;
				int64_t diff;
				diff = ptsdiff(trans_pts_dts(p->pts), add_pts_audio(0, aframe,*acount + 1) + *fpts);
				if (abs ((int)diff) >= frame_time){
					LOG(VB_GENERAL, LOG_INFO,
					    "fixing audio PTS inconsistency - diff: ");
					printpts(abs(diff));

					if (diff < 0){
						diff = abs(diff);
						int framesdiff = diff / frame_time;
						LOG(VB_GENERAL, LOG_INFO,
						    " - need to remove %d frame(s)",
							framesdiff);
						
						// FIXME can only remove one frame at a time for now
						if (framesdiff > 1)
							framesdiff = 1;
						iu->pts = add_pts_audio(0, aframe, -framesdiff);
						c += aframe->framesize;
						continue;
					} else {
						int framesdiff = diff / frame_time;
						LOG(VB_GENERAL, LOG_INFO,
						    " - need to add %d frame(s)",
							framesdiff);
						
						// limit inserts to a maximum of 5 frames
						if (framesdiff > 5)
							framesdiff = 5;

						// alloc memmory for  audio frame
						uint8_t *framebuf;
						if ( !(framebuf = (uint8_t *) malloc(sizeof(uint8_t) * aframe->framesize))) {
							LOG(VB_GENERAL, LOG_ERR,
							    "Not enough memory for audio frame");
							exit(1);
						}

						// try to encode a blank frame
						if (encode_mp2_audio(aframe, framebuf, sizeof(uint8_t) * aframe->framesize) != 0) {
							// encode failed so just use a copy of the current frame
							int res;
							res = ring_peek(rbuf, framebuf, aframe->framesize, 0);
							if (res != (int) aframe->framesize) {
								LOG(VB_GENERAL, LOG_ERR,
								    "ring buffer failed to peek frame res: %d",
									res);
								exit(1);
							}
						}	
						
						// add each extra frame required direct to the output file
						int x;
						for (x = 0; x < framesdiff; x++){
                            if (rx->dmx_out[num+1]){
                                write(rx->dmx_out[num+1], framebuf, aframe->framesize);
                            }
							*acount += 1;
						}
						
						free(framebuf);
					}
				}
			}

			if (aframe->set){
				if(iu->active){
					iu->length = ring_posdiff(rbuf, 
								  iu->start, 
								  p->ini_pos + 
								  pos+c);

					if (iu->length < aframe->framesize ||
					    iu->length > aframe->framesize+1){
						LOG(VB_GENERAL, LOG_ERR,
						    "Wrong audio frame size: %d",
							 iu->length);
						iu->err= FRAME_ERR;
					}
					if (ring_write(index_buf, 
							(uint8_t *)iu,
							sizeof(index_unit)) < 0){
						LOG(VB_GENERAL, LOG_ERR,
						    "audio ring buffer overrun error");
						exit(1);
					}
					*acount += 1;
				} 
				
				init_index(iu);
				iu->active = 1;
				iu->pts = add_pts_audio(0, aframe,*acount);
				iu->framesize = aframe->framesize;

				if (!rx->ignore_pts &&
				    first && (p->flag2 & PTS_ONLY)){
					int64_t diff;

					diff = ptsdiff(trans_pts_dts(p->pts),
							iu->pts + *fpts);
					if( !rx->keep_pts && abs ((int)diff) > 40*CLOCK_MS){
						LOG(VB_GENERAL, LOG_ERR,
						    "audio PTS inconsistent:");
						printpts(trans_pts_dts(p->pts)-*fpts);
						printpts(iu->pts);
						LOG(VB_GENERAL, LOG_ERR,
						    "diff: ");
						printpts(abs(diff));
					}	
					if (rx->keep_pts){
						fix_audio_count(acount, aframe, 
								uptsdiff(trans_pts_dts(
										 p->pts),
									 *fpts),
								iu->pts);
						iu->pts = uptsdiff(trans_pts_dts(p->pts),
								   *fpts);
						if (*lpts && ptsdiff(iu->pts,*lpts)<0) 
							LOG(VB_GENERAL, LOG_WARNING,
							    "Warning negative audio PTS increase!\n");
						*lpts = iu->pts;
					}
					first = 0;
				}
				if (rx->analyze >1){
					if ((p->flag2 & PTS_ONLY)){
						iu->pts = trans_pts_dts(p->pts);
					} else {
						iu->pts = 0;
					}
				}
				iu->start = (p->ini_pos+pos+c)%bsize;
			}
			c += pos;
			if (c + (int) aframe->framesize > len){
#if 0
				LOG(VB_GENERAL, LOG_INFO, "SHORT %d", len -c);
#endif
				c = len;
			} else {
				c += aframe->framesize;
			}
		} else {
			*apes_abort = len-c;
			c=len;
		}
	}	
}

static void analyze_video( pes_in_t *p, struct replex *rx, int len)
{
	uint8_t buf[8];
	int c=0;
	int pos=0;
	uint8_t head;
	int seq_end = 0;
	uint8_t frame = 0;
	uint64_t newpts = 0;
	uint64_t newdts = 0;
	index_unit *iu;
	int off=0;
	ringbuffer *rbuf;
	ringbuffer *index_buf;
	sequence_t *s;
	int i;

	uint16_t tempref = 0;
	int seq_h = 0;
	int gop = 0;
	int frame_off = 0;
	int gop_off = 0;
	int seq_p = 0;
	int flush=0;

	rbuf = &rx->vrbuffer;
	index_buf = &rx->index_vrbuffer;
	iu = &rx->current_vindex;
	s = &rx->seq_head;

	rx->vpes_abort = 0;
	off = ring_rdiff(rbuf, p->ini_pos);
#ifdef IN_DEBUG
	LOG(VB_GENERAL, LOG_DEBUG, " ini pos %d", (p->ini_pos)%rx->videobuf);
#endif

	
#if 0
	LOG(VB_GENERAL, LOG_INFO, "len %d  %d",len,off);
#endif
	while (c < len){
		if ((pos = ring_find_any_header( rbuf, &head, c+off, len-c)) 
		    >=0 ){
			switch(head){
			case SEQUENCE_HDR_CODE:
#ifdef IN_DEBUG
				LOG(VB_GENERAL, LOG_DEBUG, " seq headr %d",
					(p->ini_pos+c+pos)%rx->videobuf);
#endif

				seq_h = 1;
				seq_p = c+pos;
				
				
				if (!s->set){ 
					int re=0;
					re = get_video_info(rbuf, &rx->seq_head, 
							    pos+c+off, len -c -pos);

#ifdef IN_DEBUG
					LOG(VB_GENERAL, LOG_DEBUG,
					    " seq headr result %d", re);
#endif
					if (re == -2){
						rx->vpes_abort = len -(c+pos-1);
						return;
					}
					if (s->set){
						ring_skip(rbuf, pos+c);
						off -= pos+c;
					}
					if (!rx->ignore_pts && 
					    !(p->flag2 & PTS_ONLY)) s->set = 0;
				}
				if (s->set){
					flush = 1;
				}
				break;
				
			case EXTENSION_START_CODE:{
				int ext_id = 0;

#ifdef IN_DEBUG
				LOG(VB_GENERAL, LOG_DEBUG, " seq ext headr %d",
					(p->ini_pos+c+pos)+rx->videobuf);
#endif
				ext_id = get_video_ext_info(rbuf, 
							    &rx->seq_head, 
							    pos+c+off, 
							    len -c -pos);
				if (ext_id == -2){
					rx->vpes_abort = len - (pos-1+c);
					return;
				}
					

				if(ext_id == PICTURE_CODING_EXTENSION 
				   && s->ext_set && iu->active){
					if (!rx->ignore_pts && 
					    !rx->vframe_count &&
					    !rx->first_vpts){
						rx->first_vpts = trans_pts_dts(
							p->pts);
						
						for (i = 0; i< s->current_tmpref;
						     i++) ptsdec(&rx->first_vpts,
								 SEC_PER);
						LOG(VB_GENERAL, LOG_INFO,
						    "starting with video PTS:");
						printpts(rx->first_vpts);
					}

					newpts = 0;
#ifdef IN_DEBUG
					
					LOG(VB_GENERAL, LOG_DEBUG,
					    "fcount %d  gcount %d  tempref %d  %d",
						(int)rx->vframe_count, (int)rx->vgroup_count, 
						(int)s->current_tmpref,
						(int)(s->current_tmpref - rx->vgroup_count 
						      + rx->vframe_count));
#endif
					newdts = next_ptsdts_video(
						&newpts,s, rx->vframe_count, 
						rx->vgroup_count);
					
					if (!rx->ignore_pts &&
					    (p->flag2 & PTS_ONLY)){
						int64_t diff;

						diff = ptsdiff(iu->pts, 
							       rx->first_vpts 
							       + newpts);

						if (!rx->keep_pts &&
						    abs((int)(diff)) > 3*SEC_PER/2){
							LOG(VB_GENERAL, LOG_INFO,
							    "video PTS inconsistent:");
							printpts(trans_pts_dts(p->pts));
							printpts(iu->pts);
							printpts(newpts+rx->first_vpts);
							printpts(newpts);
							LOG(VB_GENERAL, LOG_INFO,
							    " diff: ");
							printpts(abs((int)diff));
						}
					}

					if (!rx->ignore_pts &&
					    (p->flag2 & PTS_DTS) == PTS_DTS){ 
						uint64_t diff;
						diff = ptsdiff(iu->dts, 
							       newdts + 
							       rx->first_vpts); 
						if (!rx->keep_pts &&
						    abs((int)diff) > 3*SEC_PER/2){
							LOG(VB_GENERAL, LOG_INFO,
							    "video DTS inconsistent: ");
							printpts(trans_pts_dts(p->dts));
							printpts(iu->dts);
							printpts(newdts+rx->first_vpts);
							printpts(newdts);
							LOG(VB_GENERAL, LOG_INFO,
							    "diff: ");
							printpts(abs((int)diff));
						}
					}
					if (!rx->keep_pts){
						iu->pts = newpts;
						iu->dts = newdts;
					} else {
						if (p->flag2 & PTS_DTS){
							ptsdec(&iu->pts, 
							       rx->first_vpts);

							if ((p->flag2 & PTS_DTS) == PTS_DTS){
								ptsdec(&iu->dts,
								       rx->first_vpts);
							} else 	iu->dts = newdts;

							fix_video_count(s,&rx->vframe_count,
									iu->pts,newpts,
									iu->dts,newdts);
							
						} else {
							iu->pts = newpts;
							iu->dts = newdts;
						}
					}
					if (rx->last_vpts && 
					    ptsdiff(iu->dts, rx->last_vpts) <0)
						LOG(VB_GENERAL, LOG_WARNING,
						    "Warning negative video PTS increase!");
					rx->last_vpts = iu->dts;
				}

				if (rx->analyze){ 
					if ((p->flag2 & PTS_DTS) == PTS_DTS){
						iu->pts = trans_pts_dts(p->pts);
						iu->dts = trans_pts_dts(p->dts);
					}  else if (p->flag2 & PTS_ONLY){
						iu->pts = trans_pts_dts(p->pts);
						iu->dts = 0;
					}  else {
						iu->pts = 0;
						iu->dts = 0;
					}
				}


				break;
			}

			case SEQUENCE_END_CODE:
#ifdef IN_DEBUG
				LOG(VB_GENERAL, LOG_DEBUG, " seq end %d",
					(p->ini_pos+c+pos)%rx->videobuf);
#endif
				if (s->set)
					seq_end = 1;
				
				break;

			case GROUP_START_CODE:{
				int hour, min, sec;
//#define ANA
#ifdef ANA
				LOG(VB_GENERAL, LOG_DEBUG, "  %d",
				    (int)rx->vgroup_count);
#endif

				if (s->set){
					if (!seq_h) flush = 1;
				}
				gop = 1;
				gop_off = c+pos - seq_p;
				
				if (ring_peek(rbuf, (uint8_t *) buf, 7, 
					      off+c+pos) < 0){
					rx->vpes_abort = len -(c+pos-1);
					return;
				}				
				hour = (int)((buf[4]>>2)& 0x1F);
				min  = (int)(((buf[4]<<4)& 0x30)| 
					     ((buf[5]>>4)& 0x0F));
				sec  = (int)(((buf[5]<<3)& 0x38)|
					     ((buf[6]>>5)& 0x07));
#ifdef IN_DEBUG
				LOG(VB_GENERAL, LOG_DEBUG,
				    " gop %02d:%02d.%02d %d",
					hour,min,sec, 
					(p->ini_pos+c+pos)%rx->videobuf);
#endif
				rx->vgroup_count = 0;

				break;
			}

			case PICTURE_START_CODE:{		
				if (len-(c+pos) < 14){
					rx->vpes_abort = len - (pos-1+c);
					return;
				}
				
				if (ring_peek(rbuf, (uint8_t *) buf, 6,
                                              off+c+pos) < 0) return;


				frame = ((buf[5]&0x38) >>3);
				
				if (frame == I_FRAME){
					if( !rx->first_iframe){
						if (s->set){
							rx->first_iframe = 1;
						} else {
							s->set=0;
							flush = 0;
							break;
						}
					}
				}

				frame_off = c+pos - seq_p;
				if (s->set){
					if (!seq_h && !gop) flush = 1;
				}
				tempref = (buf[5]>>6) & 0x03;
				tempref |= buf[4] << 2;

				switch (frame){
				case I_FRAME:
#ifdef ANA
				LOG(VB_GENERAL, LOG_DEBUG, "I");
#endif
#ifdef IN_DEBUG
					LOG(VB_GENERAL, LOG_DEBUG,
					    " I-frame %d",
						(p->ini_pos+c+pos)%rx->videobuf);
#endif
					break;
				case B_FRAME:
#ifdef ANA
				LOG(VB_GENERAL, LOG_DEBUG, "B");
#endif
#ifdef IN_DEBUG
					LOG(VB_GENERAL, LOG_DEBUG,
					    " B-frame %d",
						(p->ini_pos+c+pos)%rx->videobuf);
#endif
					break;
				case P_FRAME:
#ifdef ANA
				LOG(VB_GENERAL, LOG_DEBUG, "P");
#endif
#ifdef IN_DEBUG
					LOG(VB_GENERAL, LOG_DEBUG,
					    " P-frame %d",
						(p->ini_pos+c+pos)%rx->videobuf);
#endif
					break;
				}
				s->current_frame = frame;
				s->current_tmpref = tempref;
				
				break;		
			}
			default:
#ifdef IN_DEBUG
#if 0
				LOG(VB_GENERAL, LOG_ERR,
				    "other header 0x%02x (%d+%d)", head, c, pos);
#endif
#endif
			  break;
			}
			
			if (flush && s->set && rx->first_iframe){
				if(iu->active){
					rx->vframe_count++;
					rx->vgroup_count++;

					iu->length = ring_posdiff(rbuf, 
								  iu->start, 
								  p->ini_pos+
								  pos+c-frame_off);

					if ( ring_write(index_buf, (uint8_t *)
							 &rx->current_vindex,
							 sizeof(index_unit))<0){
						LOG(VB_GENERAL, LOG_ERR,
						    "video ring buffer overrun error");
						exit(1);

					}
				} 
				init_index(&rx->current_vindex);
				flush = 0;
				iu->active = 1;
				if (!rx->ignore_pts){
					if ((p->flag2 & PTS_DTS) == PTS_DTS){
						iu->pts = trans_pts_dts(p->pts);
						iu->dts = trans_pts_dts(p->dts);
					}  else if (p->flag2 & PTS_ONLY){
						iu->pts = trans_pts_dts(p->pts);
					}  
				}
				iu->start =  (p->ini_pos+pos+c-frame_off)
					%rx->videobuf;
#ifdef IN_DEBUG
				LOG(VB_GENERAL, LOG_DEBUG, "START %d",
					iu->start);
#endif
			}

			if (s->set){
				if (frame)
					iu->frame = (frame&0xFF);
				if (seq_h)
					iu->seq_header = 1; 
				if (seq_end)
					iu->seq_end = 1; 
				if (gop)
					iu->gop = 1; 
				if (gop_off) 
					iu->gop_off = gop_off; 
				if (frame_off) 
					iu->frame_off = frame_off;
			} 
			c+=pos+4;
		} else {
			if (pos == -2){
				rx->vpes_abort = 4;
					
			}
			c = len;
		}
	}
}

static void es_out(pes_in_t *p)
{

	struct replex *rx;
	char t[80];
	int len;

	len = p->plength-3-p->hlength;

	rx = (struct replex *) p->priv;

	switch(p->type)
	{
	case 0xE0: {
		sprintf(t, "Video ");
		if (rx->vpes_abort){
			p->ini_pos = (p->ini_pos - rx->vpes_abort)%rx->vrbuffer.size;
			len += rx->vpes_abort;
		}
		analyze_video(p, rx, len);
		if (!rx->seq_head.set){
			ring_skip(&rx->vrbuffer, len);
		}
		break;
	}

	case 1 ... 32:{
		int l;
		l = p->type - 1;
		sprintf(t, "Audio%d ", l);
		if (rx->apes_abort[l]){
			p->ini_pos = (p->ini_pos - rx->apes_abort[l])
			  %rx->arbuffer[l].size;
			len += rx->apes_abort[l];
		}
		analyze_audio(p, rx, len, l, MPEG_AUDIO);
		if (!rx->aframe[l].set)
			ring_skip(&rx->arbuffer[l], len);
		
		break;
	}

	case 0x80 ... 0x87:{
		int l;
		l = p->type - 0x80;
		sprintf(t, "AC3 %d ", p->type);
		if (rx->ac3pes_abort[l]){
			p->ini_pos = (p->ini_pos - rx->ac3pes_abort[l])
				%rx->ac3rbuffer[l].size;
			len += rx->ac3pes_abort[l];
		}
		analyze_audio(p, rx, len, l, AC3);
		if (!rx->ac3frame[l].set)
			ring_skip(&rx->ac3rbuffer[l], len);
		break;
	}

	default:
                LOG(VB_GENERAL, LOG_ERR, "UNKNOWN AUDIO type %d", p->type);
		return;


	}

#ifdef IN_DEBUG
	LOG(VB_GENERAL, LOG_DEBUG, "%s PES", t);
#endif
}

static void pes_es_out(pes_in_t *p)
{

	struct replex *rx;
	char t[80];
	int len, i;
	int l=0;

	len = p->plength-3-p->hlength;
	rx = (struct replex *) p->priv;

	switch(p->cid){
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		if (rx->vpid != p->cid) break;
		p->type = 0xE0;
		p->ini_pos = ring_wpos(&rx->vrbuffer);

		if (ring_write(&rx->vrbuffer, p->buf+9+p->hlength, len)<0){
			LOG(VB_GENERAL, LOG_ERR,
			    "video ring buffer overrun error");
			exit(1);
		}
		if (rx->vpes_abort){
			p->ini_pos = (p->ini_pos - rx->vpes_abort)%rx->vrbuffer.size;
			len += rx->vpes_abort;
		}
		sprintf(t, "Video ");
		analyze_video(p, rx, len);
		if (!rx->seq_head.set){
			ring_skip(&rx->vrbuffer, len);
		}
		break;
		
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:
		p->type = p->cid - 0xc0 + 1;
		l = -1;
		for (i=0; i<rx->apidn; i++)
			if (p->cid == rx->apid[i])
				l = i;
		if (l < 0) break;
		p->ini_pos = ring_wpos(&rx->arbuffer[l]);
		if (ring_write(&rx->arbuffer[l], p->buf+9+p->hlength, len)<0){
			LOG(VB_GENERAL, LOG_ERR,
			    "video ring buffer overrun error");
			exit(1);
		}
		if (rx->apes_abort[l]){
			p->ini_pos = (p->ini_pos - rx->apes_abort[l])
			  %rx->arbuffer[l].size;
			len += rx->apes_abort[l];
		}

		sprintf(t, "Audio%d ", l);
		analyze_audio(p, rx, len, l, MPEG_AUDIO);
		if (!rx->aframe[l].set)
			ring_skip(&rx->arbuffer[l], len);
		
		break;
		
	case PRIVATE_STREAM1:{
		int hl=4;
		if (rx->vdr){
			hl=0;
			p->type=0x80;
			l = 0;
		} else {
			uint16_t fframe;
			
			fframe=0;
			fframe = p->buf[9+p->hlength+3];
			fframe |= (p->buf[9+p->hlength+2]<<8);
		
			if (fframe > p->plength) break;
			
			p->type = p->buf[9+p->hlength];
			l = -1;
			for (i=0; i<rx->ac3n; i++)
				if (p->type == rx->ac3_id[i])
					l = i;
			if (l < 0) break;
		}
		len -= hl;
		p->ini_pos = ring_wpos(&rx->ac3rbuffer[l]);
	
		if (ring_write(&rx->ac3rbuffer[l], p->buf+9+hl+p->hlength, len)<0){
			LOG(VB_GENERAL, LOG_ERR,
			    "video ring buffer overrun error");
			exit(1);
		}
		if (rx->ac3pes_abort[l]){
			p->ini_pos = (p->ini_pos - rx->ac3pes_abort[l])
				%rx->ac3rbuffer[l].size;
			len += rx->ac3pes_abort[l];
		}

		sprintf(t, "AC3 %d ", p->type);
		analyze_audio(p, rx, len, l, AC3);
		sprintf(t,"%d",rx->ac3frame[l].set);
		if (!rx->ac3frame[l].set)
			ring_skip(&rx->ac3rbuffer[l], len);
	}
		break;
		
	default:
		return;
	}
	
#ifdef IN_DEBUG
	LOG(VB_GENERAL, LOG_DEBUG, "%s PES %d", t, len);
#endif
}

static void avi_es_out(pes_in_t *p)
{

	struct replex *rx;
	char t[80];
	int len;

	len = p->plength;

	rx = (struct replex *) p->priv;


	switch(p->type)
	{
	case 0xE0: {
		sprintf(t, "Video ");
		if (!len){
			rx->vframe_count++;
			break;
		}
		analyze_video(p, rx, len);
		if (!rx->seq_head.set){
			ring_skip(&rx->vrbuffer, len);
		}
		break;
	}

	case 1 ... 32:{
		int l;
		l = p->type - 1;
		sprintf(t, "Audio%d ", l);
		if (!len){
			rx->aframe_count[l]++;
			break;
		}
		analyze_audio(p, rx, len, l, MPEG_AUDIO);
		if (!rx->aframe[l].set)
			ring_skip(&rx->arbuffer[l], len);
		
		break;
	}

	case 0x80 ... 0x87:{
		int l;

		l = p->type - 0x80;
		sprintf(t, "AC3 %d ", p->type);
		if (!len){
			rx->ac3frame_count[l]++;
			break;
		}
		analyze_audio(p, rx, len, l, AC3);
		if (!rx->ac3frame[l].set)
			ring_skip(&rx->ac3rbuffer[l], len);
		break;
	}

	default:
		return;


	}
#ifdef IN_DEBUG
	LOG(VB_GENERAL, LOG_DEBUG, "%s PES", t);
#endif

}


static int replex_tsp(struct replex *rx, uint8_t *tsp)
{
	uint16_t pid;
	int type;
	int off=0;
	pes_in_t *p=NULL;

	pid = get_pid(tsp+1);

	if ((type=replex_check_id(rx, pid))<0)
		return 0;
	
	switch(type){
	case 0:
		p = &rx->pvideo;
		break;

	case 1 ... 32:
		p = &rx->paudio[type-1];
		break;

	case 0x80 ... 0x87:
		p = &rx->pac3[type-0x80];
		break;
	default:
		return 0;
		
	}

	
	if ( tsp[1] & PAY_START) {
		if (p->plength == MMAX_PLENGTH-6){
			p->plength = p->found-6;
			es_out(p);
			init_pes_in(p, p->type, NULL, 0);
		}
	}

	if ( tsp[3] & ADAPT_FIELD) {  // adaptation field?
		off = tsp[4] + 1;
		if (off+4 >= TS_SIZE) return 0;
	}
        
	get_pes(p, tsp+4+off, TS_SIZE-4-off, es_out);
	
	return 0;
}


static ssize_t save_read(struct replex *rx, void *buf, size_t count)
{
	ssize_t neof = 1;
	size_t re = 0;
	int fd = rx->fd_in;

	if (rx->itype== REPLEX_AVI){
		int l = rx->inflength - rx->finread;
		if ( l <= 0) return 0;
		if ( (int) count > l) count = l;
	}
	while(neof >= 0 && re < count){
		neof = read(fd, ((char*)buf)+re, count - re);
		if (neof > 0) re += neof;
		else break;
	}
	rx->finread += re;
#ifndef OUT_DEBUG
	if (rx->inflength){
		uint8_t per=0;
		
		per = (uint8_t)(rx->finread*100/rx->inflength);
		if (per % 10 == 0 && rx->lastper < per){
			LOG(VB_GENERAL, LOG_DEBUG, "read %3d%%", (int)per);
			rx->lastper = per;
		}
	} else
		LOG(VB_GENERAL, LOG_DEBUG, "read %.2f MB",
		    rx->finread/1024.0/1024.0);
#endif
	if (neof < 0 && re == 0) return neof;
	else return re;
}

static int guess_fill( struct replex *rx)
{
	int vavail, aavail, ac3avail, i, fill;

	vavail = 0;
	ac3avail =0;
	aavail = 0;
	fill =0;
	
#define LIMIT 3
	if ((vavail = ring_avail(&rx->index_vrbuffer)/sizeof(index_unit))
	    < LIMIT) 
		fill = ring_free(&rx->vrbuffer);
	
	for (i=0; i<rx->apidn;i++){
		if ((aavail = ring_avail(&rx->index_arbuffer[i])
		     /sizeof(index_unit)) < LIMIT)
			if (fill < (int) ring_free(&rx->arbuffer[i]))
				fill = ring_free(&rx->arbuffer[i]);
	}

	for (i=0; i<rx->ac3n;i++){
		if ((ac3avail = ring_avail(&rx->index_ac3rbuffer[i])
		     /sizeof(index_unit)) < LIMIT)
			if (fill < (int) ring_free(&rx->ac3rbuffer[i]))
				fill = ring_free(&rx->ac3rbuffer[i]);
	}

#if 0
	LOG(VB_GENERAL, LOG_INFO, "free %d  %d %d %d", fill, vavail, aavail,
		ac3avail);
#endif

	if (!fill){ 
		if(vavail && (aavail || ac3avail)) return 0;
		else return -1;
	}

	return fill/2;
}



#define IN_SIZE (1000*TS_SIZE)
static void find_pids_file(struct replex *rx)
{
	uint8_t buf[IN_SIZE];
	int afound=0;
	int vfound=0;
	int count=0;
	int re=0;
	uint16_t vpid=0, apid=0, ac3pid=0;
		
	LOG(VB_GENERAL, LOG_INFO, "Trying to find PIDs");
	while (!afound && !vfound && count < (int) rx->inflength){
		if (rx->vpid) vfound = 1;
		if (rx->apidn) afound = 1;
		if ((re = save_read(rx,buf,IN_SIZE))<0)
			LOG(VB_GENERAL, LOG_ERR, "reading: %s",
				strerror(errno));
		else 
			count += re;
		if ( (re = find_pids(&vpid, &apid, &ac3pid, buf, re))){
			if (!rx->vpid && vpid){
				rx->vpid = vpid;
				LOG(VB_GENERAL, LOG_INFO,"vpid 0x%04x",
					(int)rx->vpid);
				vfound++;
			}
			if (!rx->apidn && apid){
				rx->apid[0] = apid;
				LOG(VB_GENERAL, LOG_INFO, "apid 0x%04x",
					(int)rx->apid[0]);
				rx->apidn++;
				afound++;
			}
			if (!rx->ac3n && ac3pid){
				rx->ac3_id[0] = ac3pid;
				LOG(VB_GENERAL, LOG_INFO, "ac3pid 0x%04x",
					(int)rx->ac3_id[0]);
				rx->ac3n++;
				afound++;
			}
			
		} 
		
	}
	
	lseek(rx->fd_in,0,SEEK_SET);
	if (!afound || !vfound){
		LOG(VB_GENERAL, LOG_ERR, "Couldn't find all pids");
		exit(1);
	}
	
}

#define MAXVPID 16
#define MAXAPID 32
#define MAXAC3PID 16
static void find_all_pids_file(struct replex *rx)
{
	uint8_t buf[IN_SIZE];
	int count=0;
	int j;
	int re=0;
	uint16_t vpid[MAXVPID], apid[MAXAPID], ac3pid[MAXAC3PID];
	int vn=0, an=0,ac3n=0;
	uint16_t vp,ap,cp;
	int vpos, apos, cpos;
		
	memset (vpid , 0 , MAXVPID*sizeof(uint16_t));
	memset (apid , 0 , MAXAPID*sizeof(uint16_t));
	memset (ac3pid , 0 , MAXAC3PID*sizeof(uint16_t));

	LOG(VB_GENERAL, LOG_INFO, "Trying to find PIDs");
	while (count < (int) rx->inflength-IN_SIZE){
		if ((re = save_read(rx,buf,IN_SIZE))<0)
			LOG(VB_GENERAL, LOG_ERR, "reading: %s",
				strerror(errno));
		else 
			count += re;
		if ( (re = find_pids_pos(&vp, &ap, &cp, buf, re, 
					 &vpos, &apos, &cpos))){
			if (vp){
				int old=0;
				for (j=0; j < vn; j++){
#if 0
					LOG(VB_GENERAL, LOG_INFO,
					    "%d. %d", j+1, vpid[j]);
#endif
					if (vpid[j] == vp){
						old = 1;
						break;
					}
				}
				if (!old){
					vpid[vn]=vp;
					
					LOG(VB_GENERAL, LOG_INFO,
					    "vpid %d: 0x%04x (%d)  PES ID: 0x%02x",
					       vn+1,
					       (int)vpid[vn], (int)vpid[vn],
					       buf[vpos]);
					if (vn+1 < MAXVPID) vn++;
				} 				
			}

			if (ap){
				int old=0;
				for (j=0; j < an; j++)
					if (apid[j] == ap){
						old = 1;
						break;
					}
				if (!old){
					apid[an]=ap;
					LOG(VB_GENERAL, LOG_INFO,
					    "apid %d: 0x%04x (%d)  PES ID: 0x%02x",
					       an +1,
					       (int)apid[an],(int)apid[an],
					       buf[apos]);
					if (an+1 < MAXAPID) an++;
				}				
			}

			if (cp){
				int old=0;
				for (j=0; j < ac3n; j++)
					if (ac3pid[j] == cp){
						old = 1;
						break;
					}
				if (!old){
					ac3pid[ac3n]=cp;
					LOG(VB_GENERAL, LOG_INFO,
					    "ac3pid %d: 0x%04x (%d)",
					       ac3n+1,
					       (int)ac3pid[ac3n],
					       (int)ac3pid[ac3n]);
					if (ac3n+1< MAXAC3PID) ac3n++;
				}				
			}
		} 
		
	}
	
	lseek(rx->fd_in,0,SEEK_SET);
}

static void find_pids_stdin(struct replex *rx, uint8_t *buf, int len)
{
	int afound=0;
	int vfound=0;
	uint16_t vpid=0, apid=0, ac3pid=0;
		
	if (rx->vpid) vfound = 1;
	if (rx->apidn) afound = 1;
	LOG(VB_GENERAL, LOG_INFO, "Trying to find PIDs");
	if ( find_pids(&vpid, &apid, &ac3pid, buf, len) ){
		if (!rx->vpid && vpid){
			rx->vpid = vpid;
			vfound++;
		}
		if (!rx->apidn && apid){
			rx->apid[0] = apid;
			rx->apidn++;
			afound++;
			ring_init(&rx->arbuffer[0], rx->audiobuf);
			init_pes_in(&rx->paudio[0], 1, &rx->arbuffer[0], 0);
			rx->paudio[0].priv = (void *) rx;
			ring_init(&rx->index_arbuffer[0], INDEX_BUF);	
			memset(&rx->aframe[0], 0, sizeof(audio_frame_t));
			init_index(&rx->current_aindex[0]);
			rx->aframe_count[0] = 0;
			rx->first_apts[0] = 0;
		}

		if (!rx->ac3n && ac3pid){
			rx->ac3_id[0] = ac3pid;
			rx->ac3n++;
			afound++;
			ring_init(&rx->ac3rbuffer[0], AC3_BUF);
			init_pes_in(&rx->pac3[0], 0x80, &rx->ac3rbuffer[0],0);
			rx->pac3[0].priv = (void *) rx;
			ring_init(&rx->index_ac3rbuffer[0], INDEX_BUF);
			memset(&rx->ac3frame[0], 0, sizeof(audio_frame_t));
			init_index(&rx->current_ac3index[0]);
			rx->ac3frame_count[0] = 0;
			rx->first_ac3pts[0] = 0;
		}
		
	} 

	if (afound && vfound){
		LOG(VB_GENERAL, LOG_INFO, "found");
		if (rx->vpid)
			LOG(VB_GENERAL, LOG_INFO, "vpid %d (0x%04x)",
			      rx->vpid, rx->vpid);
		if (rx->apidn)
			LOG(VB_GENERAL, LOG_INFO, "apid %d (0x%04x)",
			      rx->apid[0], rx->apid[0]);
		if (rx->ac3n)
			LOG(VB_GENERAL, LOG_INFO, "ac3pid %d (0x%04x)",
			      rx->ac3_id[0], rx->ac3_id[0]);
	} else {
		LOG(VB_GENERAL, LOG_ERR, "Couldn't find pids");
		exit(1);
	}

}


static void pes_id_out(pes_in_t *p)
{

	struct replex *rx;
	int len;

	len = p->plength-3-p->hlength;
	rx = (struct replex *) p->priv;

	rx->scan_found=0;
	switch(p->cid){
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		rx->scan_found=p->cid;
		break;
		
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:
		rx->scan_found=p->cid;
		break;
		
	case PRIVATE_STREAM1:
		if (rx->vdr){
			rx->scan_found = 0x80;
			break;
		} else {
			
			uint8_t id = p->buf[9+p->hlength];
			switch(id){
			case 0x80 ... 0x8f:
			{ 
				int c=0; 
				uint16_t fframe;
				
				fframe=0;
				fframe = p->buf[9+p->hlength+3];
				fframe |= (p->buf[9+p->hlength+2]<<8);
				
				if (fframe < p->plength){
					if ((c=find_audio_s(p->buf,
							    9+p->hlength+4+fframe, 
							    AC3, p->plength+6)) >= 0){
						rx->scan_found = id;
#if 0
						LOG(VB_GENERAL, LOG_INFO,
						    "0x%04x  0x%04x \n",
						    c-9-p->hlength-4, fframe);
						if (id>0x80)show_buf(p->buf+9+p->hlength,8);
						if (id>0x80)show_buf(p->buf+c,8);
#endif
					}
				}
				break;
			}
			}
			break;
		}
	default:
		p->cid = 0;
		p->type = 0;
		rx->scan_found=0;
		memset(p->buf,0,MAX_PLENGTH*sizeof(uint8_t));
		return;
	}
}

static void find_pes_ids(struct replex *rx)
{
	uint8_t buf[IN_SIZE];
	int count=0;
	int j;
	int re=0;
	uint16_t vpid[MAXVPID], apid[MAXAPID], ac3pid[MAXAC3PID];
	int vn=0, an=0,ac3n=0;
		
	memset (vpid , 0 , MAXVPID*sizeof(uint16_t));
	memset (apid , 0 , MAXAPID*sizeof(uint16_t));
	memset (ac3pid , 0 , MAXAC3PID*sizeof(uint16_t));

	LOG(VB_GENERAL, LOG_INFO, "Trying to find PES IDs");
	rx->scan_found=0;
	rx->pvideo.priv = rx ;
	while (count < (int) rx->inflength-IN_SIZE){
		if ((re = save_read(rx,buf,IN_SIZE))<0)
			LOG(VB_GENERAL, LOG_ERR, "reading: %s",
				strerror(errno));
		else 
			count += re;
		
		get_pes(&rx->pvideo, buf, re, pes_id_out);
		
		if ( rx->scan_found ){

			switch (rx->scan_found){
				
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			{
				int old=0;
				for (j=0; j < vn; j++){
#if 0
					LOG(VB_GENERAL, LOG_INFO,
					    "%d. %d", j+1, vpid[j]);
#endif
					if (vpid[j] == rx->scan_found){
						old = 1;
						break;
					}
				}
				if (!old){
					vpid[vn]=rx->scan_found;
					
					LOG(VB_GENERAL, LOG_INFO,
					    "MPEG VIDEO %d: 0x%02x (%d)",
					       vn+1,
					       (int)vpid[vn], (int)vpid[vn]);
					if (vn+1 < MAXVPID) vn++;
				}
			} 				
			break;

			
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			{
				int old=0;
				for (j=0; j < an; j++)
					if (apid[j] == rx->scan_found){
						old = 1;
						break;
					}
				if (!old){
					apid[an]=rx->scan_found;
					LOG(VB_GENERAL, LOG_INFO,
					    "MPEG AUDIO %d: 0x%02x (%d)",
					       an +1,
					       (int)apid[an],(int)apid[an]);
					if (an+1 < MAXAPID) an++;
				}				
			}
			break;

			case 0x80 ... 0x8f:
			{
				int old=0;
				for (j=0; j < ac3n; j++)
					if (ac3pid[j] == rx->scan_found){
						old = 1;
						break;
					}
				if (!old){
					ac3pid[ac3n]=rx->scan_found;
					if (rx->vdr){
						LOG(VB_GENERAL, LOG_INFO,
						    "possible AC3 AUDIO with private stream 1 pid (0xbd)");
					}else{
						LOG(VB_GENERAL, LOG_INFO,
						    "AC3 AUDIO %d: 0x%02x (%d)",
						       ac3n+1,
						       (int)ac3pid[ac3n],
						       (int)ac3pid[ac3n]);
					}
					if (ac3n+1< MAXAC3PID) ac3n++;
				}				
				rx->scan_found = 0;
			}
			break;
			}
			rx->scan_found = 0;
		}
	}
}



static void replex_finish(struct replex *rx)
{
	if (!replex_all_set(rx)){
		LOG(VB_GENERAL, LOG_ERR, "Can't find all required streams");
		if (rx->itype == REPLEX_PS){
			LOG(VB_GENERAL, LOG_ERR,
			    "Please check if audio and video have standard IDs (0xc0 or 0xe0)");
		}
		exit(1);
	}

	if (!rx->demux)
		finish_mpg((multiplex_t *)rx->priv);
	exit(0);
}

static int replex_fill_buffers(struct replex *rx, uint8_t *mbuf)
{
	uint8_t buf[IN_SIZE];
	int i,j;
	int count=0;
	int fill;
	int re;
	int rsize;
	int tries = 0;

	if (rx->finish) return 0;
	fill =  guess_fill(rx);
#if 0
	LOG(VB_GENERAL, LOG_INFO, "trying to fill buffers with %d", fill);
#endif
	if (fill < 0) return -1;

	memset(buf, 0, IN_SIZE);
	
	switch(rx->itype){
	case REPLEX_TS:
		if (fill < IN_SIZE){
			rsize = fill - (fill%188);
		} else rsize = IN_SIZE;
		
#if 0
	LOG(VB_GENERAL, LOG_INFO, "filling with %d", rsize);
#endif
		
		if (!rsize) return 0;
		
		memset(buf, 0, IN_SIZE);
		
		if ( mbuf ){
			for ( i = 0; i < 188 ; i++){
				if ( mbuf[i] == 0x47 ) break;
			}
			
			if ( i == 188){
				LOG(VB_GENERAL, LOG_ERR, "Not a TS");
				return -1;
			} else {
				memcpy(buf,mbuf+i,2*TS_SIZE-i);
				if ((count = save_read(rx,mbuf,i))<0)
					LOG(VB_GENERAL, LOG_ERR, "reading: %s",
						strerror(errno));
				memcpy(buf+2*TS_SIZE-i,mbuf,i);
				i = 188;
			}
		} else i=0;

	
#define MAX_TRIES 5
		while (count < rsize && tries < MAX_TRIES){
			if ((re = save_read(rx,buf+i,rsize-i)+i)<0)
				LOG(VB_GENERAL, LOG_ERR, "reading: %s",
					strerror(errno));
			else 
				count += re;
			tries++;
			
			if (!rx->vpid || !(rx->apidn || rx->ac3n)){
				find_pids_stdin(rx, buf, re);
			}

			for( j = 0; j < re; j+= TS_SIZE){
				
				if ( re - j < TS_SIZE) break;
				
				if ( replex_tsp( rx, buf+j) < 0){
					LOG(VB_GENERAL, LOG_ERR,
					    "Error reading TS");
					exit(1);
				}
			}
			i=0;
		}
		
		if (tries == MAX_TRIES)
			replex_finish(rx);
		return 0;
		break;
		
	case REPLEX_PS:
		rsize = fill;
		if (fill > IN_SIZE) rsize = IN_SIZE; 
		if (mbuf)
			get_pes(&rx->pvideo, mbuf, 2*TS_SIZE, pes_es_out);
		
		while (count < rsize && tries < MAX_TRIES){
			if ((re = save_read(rx, buf, rsize))<0)
				LOG(VB_GENERAL, LOG_ERR, "reading PS: %s",
					strerror(errno));
			else 
				count += re;
	
			get_pes(&rx->pvideo, buf, re, pes_es_out);
			
			tries++;
			
		}
		
		if (tries == MAX_TRIES)
			replex_finish(rx);
		return 0;
		break;


	case REPLEX_AVI:
		rsize = fill;
		if (fill > IN_SIZE) rsize = IN_SIZE; 
		
		if (!(rx->ac.avih_flags & AVI_HASINDEX)){

			if (mbuf){
				get_avi(&rx->pvideo, mbuf, rx->avi_rest, avi_es_out);
			}

			while (count < rsize && tries < MAX_TRIES){
				if ((re = save_read(rx, buf, rsize))<0)
					LOG(VB_GENERAL, LOG_ERR, "reading AVI: %s",
						strerror(errno));
				else 
					count += re;
				
				get_avi(&rx->pvideo, buf, re, avi_es_out);
				
				tries++;
			}
		} else {
			if (get_avi_from_index(&rx->pvideo, rx->fd_in,
					       &rx->ac, avi_es_out, rsize) < 0)
				tries = MAX_TRIES;
		}

		if (tries == MAX_TRIES)
			replex_finish(rx);
		return 0;
		break;
	}
	
	return -1;
}

static int fill_buffers(void *r, int finish)
{
	struct replex *rx = (struct replex *)r;
	
	rx->finish = finish;

	return replex_fill_buffers(rx, NULL);
}


void init_index(index_unit *iu)
{
	memset(iu,0, sizeof(index_unit));
	iu->frame_start = 1;
}

static int replex_all_set(struct replex *rx)
{
	int i;
	int set=0;

	for (i=0;  i < rx->ac3n ;i++){
		set += rx->ac3frame[i].set;
	}
	for (i=0; i<rx->apidn;i++){
		set += rx->aframe[i].set;
	}
	set += rx->seq_head.set;

	if (set == (rx->ac3n+ rx->apidn + 1)) return 1;
	return 0;
}


static int check_stream_type(struct replex *rx, uint8_t * buf, int len)
{
	int c=0;
	avi_context ac;
	uint8_t head;

	if (rx->itype != REPLEX_TS) return rx->itype;

	if (len< 2*TS_SIZE){
		LOG(VB_GENERAL, LOG_ERR, "cannot determine streamtype");
		exit(1);
	}

	LOG(VB_GENERAL, LOG_INFO, "Checking for TS: ");
	while (c < len && buf[c]!=0x47) c++;
	if (c<len && len-c>=TS_SIZE){
		if (buf[c+TS_SIZE] == 0x47){
			LOG(VB_GENERAL, LOG_INFO, "confirmed");
			return REPLEX_TS;
		} else  LOG(VB_GENERAL, LOG_INFO, "failed");
	} else  LOG(VB_GENERAL, LOG_INFO, "failed");

	LOG(VB_GENERAL, LOG_INFO, "Checking for AVI: ");
	if (check_riff(&ac, buf, len)>=0){
		LOG(VB_GENERAL, LOG_INFO, "confirmed");
		rx->itype = REPLEX_AVI;
		rx->vpid = 0xE0;
		rx->apidn = 1;
		rx->apid[0] = 0xC0;
		rx->ignore_pts =1;
		return REPLEX_AVI;
	} else LOG(VB_GENERAL, LOG_INFO, "failed");

	LOG(VB_GENERAL, LOG_INFO, "Checking for PS: ");
	if (find_any_header(&head, buf, len) >= 0){
		LOG(VB_GENERAL, LOG_INFO, "confirmed(maybe)");
	} else {
		LOG(VB_GENERAL, LOG_INFO,"failed, trying it anyway");
	}
	rx->itype=REPLEX_PS;
	if (!rx->vpid) rx->vpid = 0xE0;
	if (!(rx->apidn || rx->ac3n)){
		rx->apidn = 1;
		rx->apid[0] = 0xC0;
	}
	return REPLEX_PS;
}


static void init_replex(struct replex *rx)
{
	int i;
	uint8_t mbuf[2*TS_SIZE];
	
	rx->analyze=0;

	if (save_read(rx, mbuf, 2*TS_SIZE)<0)
		LOG(VB_GENERAL, LOG_ERR, "reading: %s",
			strerror(errno));
	
	check_stream_type(rx, mbuf, 2*TS_SIZE);
	if (rx->itype == REPLEX_TS){
		if (!rx->vpid || !(rx->apidn || rx->ac3n)){
			if (rx->inflength){
				find_pids_file(rx);
			}
		}
	}

	
	if (rx->otype==REPLEX_HDTV){
		rx->videobuf = 4*VIDEO_BUF;
	} else {
		rx->videobuf = VIDEO_BUF;
	}
	rx->audiobuf = AUDIO_BUF;
	rx->ac3buf = AC3_BUF;

	if (rx->itype == REPLEX_AVI){
		rx->videobuf = 4*VIDEO_BUF;
		rx->audiobuf = 2*rx->videobuf;
	}
	
	rx->vpes_abort = 0;
	rx->first_iframe = 0;
	ring_init(&rx->vrbuffer, rx->videobuf);
	if (rx->itype == REPLEX_TS || rx->itype == REPLEX_AVI)
		init_pes_in(&rx->pvideo, 0xE0, &rx->vrbuffer, 0);
	else {
		init_pes_in(&rx->pvideo, 0, NULL, 1);
	}
	rx->pvideo.priv = (void *) rx;
	ring_init(&rx->index_vrbuffer, INDEX_BUF);
	memset(&rx->seq_head, 0, sizeof(sequence_t));
	init_index(&rx->current_vindex);
	rx->vgroup_count = 0;
	rx->vframe_count = 0;
	rx->first_vpts = 0;
	rx->video_state = S_SEARCH;
	rx->last_vpts = 0;

	for (i=0; i<rx->apidn;i++){
		
		rx->apes_abort[i] = 0;
		rx->audio_state[i] = S_SEARCH;
		ring_init(&rx->arbuffer[i], rx->audiobuf);

		if (rx->itype == REPLEX_TS){
			init_pes_in(&rx->paudio[i], i+1, 
				    &rx->arbuffer[i], 0);
			rx->paudio[i].priv = (void *) rx;
		}
		ring_init(&rx->index_arbuffer[i], INDEX_BUF);	
		memset(&rx->aframe[i], 0, sizeof(audio_frame_t));
		init_index(&rx->current_aindex[i]);
		rx->aframe_count[i] = 0;
		rx->first_apts[i] = 0;
		rx->last_apts[i] = 0;
	}

	for (i=0; i<rx->ac3n;i++){
		rx->ac3pes_abort[i] = 0;
		rx->ac3_state[i] = S_SEARCH;
		ring_init(&rx->ac3rbuffer[i], AC3_BUF);
		if (rx->itype == REPLEX_TS){
			init_pes_in(&rx->pac3[i], 0x80+i, 
				    &rx->ac3rbuffer[i],0);
			rx->pac3[i].priv = (void *) rx;
		}
		ring_init(&rx->index_ac3rbuffer[i], INDEX_BUF);
		memset(&rx->ac3frame[i], 0, sizeof(audio_frame_t));
		init_index(&rx->current_ac3index[i]);
		rx->ac3frame_count[i] = 0;
		rx->first_ac3pts[i] = 0;
		rx->last_ac3pts[i] = 0;
	}	
	
	if (rx->itype == REPLEX_TS){
		if (replex_fill_buffers(rx, mbuf)< 0) {
			LOG(VB_GENERAL, LOG_ERR, "error filling buffer");
			exit(1);
		}
	} else if ( rx->itype == REPLEX_AVI){
#define AVI_S 1024
		avi_context *ac;
		uint8_t buf[AVI_S];
		int re=0;
		ssize_t read_count = 0;
		
		lseek(rx->fd_in, 0, SEEK_SET);
		ac = &rx->ac;
		memset(ac, 0, sizeof(avi_context));
		if ((read_count = save_read(rx, buf, 12)) != 12) {
			LOG(VB_GENERAL, LOG_ERR,
			    "Error reading in 12 bytes from replex. Read %d bytes",
				(int)read_count);
			exit(1);
		}
		
		if (check_riff(ac, buf, 12) < 0){
			LOG(VB_GENERAL, LOG_ERR, "Wrong RIFF header");
			exit(1);
		} else {
			LOG(VB_GENERAL, LOG_INFO, "Found RIFF header");
		}
		
		memset(ac, 0, sizeof(avi_context));
		re = read_avi_header(ac, rx->fd_in);
		if (avi_read_index(ac,rx->fd_in) < 0 || re < 0){
			LOG(VB_GENERAL, LOG_ERR, "Error reading index");
			exit(1);
		}
//		rx->aframe_count[0] = ac->ai[0].initial_frames;
		rx->vframe_count = ac->ai[0].initial_frames*ac->vi.fps/
			ac->ai[0].fps;

		rx->inflength = lseek(rx->fd_in, 0, SEEK_CUR)+ac->movi_length;

		LOG(VB_GENERAL, LOG_INFO, "AVI initial frames %d",
			(int)rx->vframe_count);
		if (!ac->done){
			LOG(VB_GENERAL, LOG_ERR, "Error reading AVI header");
			exit(1);
		}

		if (replex_fill_buffers(rx, buf+re)< 0) {
			LOG(VB_GENERAL, LOG_ERR, "error filling buffer");
			exit(1);
		}
	} else {
		if (replex_fill_buffers(rx, mbuf)< 0) {
			LOG(VB_GENERAL, LOG_ERR, "error filling buffer");
			exit(1);
		}
	}

}


static void fix_audio(struct replex *rx, multiplex_t *mx)
{
	int i;
	index_unit aiu;
	int size;

	size = sizeof(index_unit);

	for ( i = 0; i < rx->apidn; i++){
		do {
			while (ring_avail(&rx->index_arbuffer[i]) < 
			       sizeof(index_unit)){
				if (replex_fill_buffers(rx, 0)< 0) {
					LOG(VB_GENERAL, LOG_ERR,
					    "error in fix audio");
					exit(1);
				}	
			}
			ring_peek(&rx->index_arbuffer[i], (uint8_t *)&aiu, size, 0);
			if ( ptscmp(aiu.pts + rx->first_apts[i], rx->first_vpts) < 0){
				ring_skip(&rx->index_arbuffer[i], size);
				ring_skip(&rx->arbuffer[i], aiu.length);
			} else break;

		} while (1);
		mx->ext[i].pts_off = aiu.pts;
		
		LOG(VB_GENERAL, LOG_INFO, "Audio%d  offset: ", i);
		printpts(mx->ext[i].pts_off);
		printpts(rx->first_apts[i]+mx->ext[i].pts_off);
	}
			  
	for ( i = 0; i < rx->ac3n; i++){
		do {
			while (ring_avail(&rx->index_ac3rbuffer[i]) < 
			       sizeof(index_unit)){
				if (replex_fill_buffers(rx, 0)< 0) {
					LOG(VB_GENERAL, LOG_ERR,
					    "error in fix audio");
					exit(1);
				}	
			}
			ring_peek(&rx->index_ac3rbuffer[i], (uint8_t *)&aiu, 
				  size, 0);
			if ( ptscmp (aiu.pts+rx->first_ac3pts[i], rx->first_vpts) < 0){
				ring_skip(&rx->index_ac3rbuffer[i], size);
				ring_skip(&rx->ac3rbuffer[i], aiu.length);
			} else break;
		} while (1);
		mx->ext[i].pts_off = aiu.pts;
		
		LOG(VB_GENERAL, LOG_INFO, "AC3%d  offset: ", i);
		printpts(mx->ext[i].pts_off);
		printpts(rx->first_ac3pts[i]+mx->ext[i].pts_off);

	}
}



static int get_next_video_unit(struct replex *rx, index_unit *viu)
{
	if (ring_avail(&rx->index_vrbuffer)){
		ring_read(&rx->index_vrbuffer, (uint8_t *)viu, 
			  sizeof(index_unit));
		return 1;
	}
	return 0;
}

static int get_next_audio_unit(struct replex *rx, index_unit *aiu, int i)
{
	if(ring_avail(&rx->index_arbuffer[i])){
		ring_read(&rx->index_arbuffer[i], (uint8_t *)aiu, 
			  sizeof(index_unit));
		return 1;
	}
	return 0;
}

static int get_next_ac3_unit(struct replex *rx, index_unit *aiu, int i)
{
	if (ring_avail(&rx->index_ac3rbuffer[i])) {
		ring_read(&rx->index_ac3rbuffer[i], (uint8_t *)aiu, 
			  sizeof(index_unit));
		
		return 1;
	} 
	return 0;
}


static void do_analyze(struct replex *rx)
{
	index_unit dummy;
	index_unit dummy2;
	int i;
	uint64_t lastvpts;
	uint64_t lastvdts;
	uint64_t lastapts[N_AUDIO];
	uint64_t lastac3pts[N_AC3];
	int av;
	
	av = rx->analyze-1;

	lastvpts = 0;
	lastvdts = 0;
	memset(lastapts, 0, N_AUDIO*sizeof(uint64_t));
	memset(lastac3pts, 0, N_AC3*sizeof(uint64_t));
	
	LOG(VB_GENERAL, LOG_INFO, "STARTING ANALYSIS");
	
	
	while(!rx->finish){
		if (replex_fill_buffers(rx, 0)< 0) {
			LOG(VB_GENERAL, LOG_ERR,
			    "error in get next video unit");
			return;
		}
		for (i=0; i< rx->apidn; i++){
			while(get_next_audio_unit(rx, &dummy2, i)){
				ring_skip(&rx->arbuffer[i], 
					  dummy2.length);
				if (av>=1){
					LOG(VB_GENERAL, LOG_INFO,
					    "MPG2 Audio%d unit:  length %d  "
					    "PTS ", i, dummy2.length);
					printptss(dummy2.pts);
					
					if (lastapts[i]){
						LOG(VB_GENERAL, LOG_INFO,
						    "  diff:");
						printptss(ptsdiff(dummy2.pts,lastapts[i]));
					}
					lastapts[i] = dummy2.pts;
				}
			}
		}
		
		for (i=0; i< rx->ac3n; i++){
			while(get_next_ac3_unit(rx, &dummy2, i)){
				ring_skip(&rx->ac3rbuffer[i], 
					  dummy2.length);
				if (av>=1){
					LOG(VB_GENERAL, LOG_INFO,
					    "AC3 Audio%d unit:  length %d  "
					    "PTS ", i, dummy2.length);
					printptss(dummy2.pts);
					if (lastac3pts[i]){
						LOG(VB_GENERAL, LOG_INFO,
						    "  diff:");
						printptss(ptsdiff(dummy2.pts, lastac3pts[i]));
					}
					lastac3pts[i] = dummy2.pts;
				}
			}
		}
		
		while (get_next_video_unit(rx, &dummy)){
			ring_skip(&rx->vrbuffer,
				  dummy.length);
			if (av==0 || av==2){
				LOG(VB_GENERAL, LOG_INFO, "Video unit: ");
	                        if (dummy.seq_header){
					LOG(VB_GENERAL, LOG_INFO,
					    "Sequence header ");
				}
				if (dummy.gop){
					LOG(VB_GENERAL, LOG_INFO,
					    "GOP header ");
				}
				switch (dummy.frame){
				case I_FRAME:
					LOG(VB_GENERAL, LOG_INFO, "I-frame");
					break;
				case B_FRAME:
					LOG(VB_GENERAL, LOG_INFO, "B-frame");
					break;
				case P_FRAME:
					LOG(VB_GENERAL, LOG_INFO, "P-frame");
					break;
				}
				LOG(VB_GENERAL, LOG_INFO, " length %d  PTS ",
					dummy.length);
				printptss(dummy.pts);
				if (lastvpts){
					LOG(VB_GENERAL, LOG_INFO,"  diff:");
					printptss(ptsdiff(dummy.pts, lastvpts));
				}
				lastvpts = dummy.pts;
					
				LOG(VB_GENERAL, LOG_INFO, "  DTS ");
				printptss(dummy.dts);
				if (lastvdts){
					LOG(VB_GENERAL, LOG_INFO, "  diff:");
					printptss(ptsdiff(dummy.dts,lastvdts));
				}
				lastvdts = dummy.dts;
			}
		}
	}


}

static void do_scan(struct replex *rx)
{
	uint8_t mbuf[2*TS_SIZE];
	
	rx->analyze=0;

	if (save_read(rx, mbuf, 2*TS_SIZE)<0)
		LOG(VB_GENERAL, LOG_ERR, "reading: %s",
			strerror(errno));
	
	LOG(VB_GENERAL, LOG_INFO, "STARTING SCAN");
	
	check_stream_type(rx, mbuf, 2*TS_SIZE);

	switch(rx->itype){
	case REPLEX_TS:
		find_all_pids_file(rx);
		break;
		
	case REPLEX_PS:
		init_pes_in(&rx->pvideo, 0, NULL, 1);
		find_pes_ids(rx);
		break;

	case REPLEX_AVI:
		break;
	}
	
}

static void do_demux(struct replex *rx)
{
	index_unit dummy;
	index_unit dummy2;
	int i;
	LOG(VB_GENERAL, LOG_INFO, "STARTING DEMUX");
	
	while(!rx->finish){
		if (replex_fill_buffers(rx, 0)< 0) {
			LOG(VB_GENERAL, LOG_ERR,
			    "error in get next video unit");
			return;
		}
		for (i=0; i< rx->apidn; i++){
			while(get_next_audio_unit(rx, &dummy2, i)){
				ring_read_file(&rx->arbuffer[i], 
					       rx->dmx_out[i+1], 
					       dummy2.length);
			}
		}
		
		for (i=0; i< rx->ac3n; i++){
			while(get_next_ac3_unit(rx, &dummy2, i)){
				ring_read_file(&rx->ac3rbuffer[i], 
					       rx->dmx_out[i+1+rx->apidn], 
					       dummy2.length);
			}
		}
		
		while (get_next_video_unit(rx, &dummy)){
			ring_read_file(&rx->vrbuffer, rx->dmx_out[0], 
				       dummy.length);
		}
	}
}


static void do_replex(struct replex *rx)
{
	int video_ok = 0;
	int ext_ok[N_AUDIO];
	int start=1;
	multiplex_t mx;


	LOG(VB_GENERAL, LOG_INFO, "STARTING REPLEX");
	memset(&mx, 0, sizeof(mx));
	memset(ext_ok, 0, N_AUDIO*sizeof(int));

	while (!replex_all_set(rx)){
		if (replex_fill_buffers(rx, 0)< 0) {
			LOG(VB_GENERAL, LOG_INFO, "error filling buffer");
			exit(1);
		}
	}

 int i; 
   for (i = 0; i < rx->apidn; i++){
        rx->exttype[i] = 2;
        rx->extframe[i] = rx->aframe[i];
        rx->extrbuffer[i] = rx->arbuffer[i];
        rx->index_extrbuffer[i] = rx->index_arbuffer[i];
        rx->exttypcnt[i+1] = i;
    }

    int ac3Count = 1;
    for (i = rx->apidn; i < rx->apidn + rx->ac3n; i++){
        rx->exttype[i] = 1;
        rx->extframe[i] = rx->ac3frame[i];
        rx->extrbuffer[i] = rx->ac3rbuffer[i];
        rx->index_extrbuffer[i] = rx->index_ac3rbuffer[i];
        rx->exttypcnt[i] = ac3Count++;
    }

    mx.priv = (void *) rx;
	rx->priv = (void *) &mx;
    init_multiplex(&mx, &rx->seq_head, rx->extframe,
                 rx->exttype, rx->exttypcnt, rx->video_delay, 
                 rx->audio_delay, rx->fd_out, fill_buffers,
                 &rx->vrbuffer, &rx->index_vrbuffer,  
                 rx->extrbuffer, rx->index_extrbuffer,
                 rx->otype);

	if (!rx->ignore_pts){ 
		fix_audio(rx, &mx);
	}
	setup_multiplex(&mx);

	while(1){
		check_times( &mx, &video_ok, ext_ok, &start);

		write_out_packs( &mx, video_ok, ext_ok);
	}
}


static void usage(char *progname)
{
        printf ("usage: %s [options] <input files>\n\n",progname);
        printf ("options:\n");
        printf ("  --help,             -h:  print help message\n");
        printf ("  --type,             -t:  set output type (MPEG2, DVD, HDTV)\n");
        printf ("  --of,               -o:  set output file\n");
        printf ("  --input_stream,     -i:  set input stream type (TS(default), PS, AVI)\n");
        printf ("  --audio_pid,        -a:  audio PID for TS stream (also used for PS id)\n");
        printf ("  --ac3_id,           -c:  ID of AC3 audio for demux (also used for PS id)\n");
        printf ("  --video_pid,        -v:  video PID for TS stream (also used for PS id)\n");
        printf ("  --video_delay,      -d:  video delay in ms\n");
        printf ("  --audio_delay,      -e:  audio delay in ms\n");
        printf ("  --ignore_PTS,       -f:  ignore all PTS information of original\n");
        printf ("  --keep_PTS,         -k:  keep and don't correct PTS information of original\n");
        printf ("  --fix_sync,         -n:  try to fix audio sync while demuxing\n");
        printf ("  --demux,            -z:  demux only (-o is basename)\n");
        printf ("  --analyze,          -y:  analyze (0=video,1=audio, 2=both)\n");
        printf ("  --scan,             -s:  scan for streams\n");
        printf ("  --vdr,              -x:  handle AC3 for vdr input file\n");
        exit(1);
}

int main(int argc, char **argv)
{
	int c;
	int analyze=0;
	int scan =0;
	char *filename = NULL;
	const char *type = "SVCD";
	const char *inpt = "TS";

	struct replex rx;

	memset(&rx, 0, sizeof(struct replex));

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"type", required_argument, NULL, 't'},
			{"input_stream", required_argument, NULL, 'i'},
			{"video_pid", required_argument, NULL, 'v'},
			{"audio_pid", required_argument, NULL, 'a'},
			{"audio_delay", required_argument, NULL, 'e'},
			{"video_delay", required_argument, NULL, 'd'},
			{"ac3_id", required_argument, NULL, 'c'},
			{"of",required_argument, NULL, 'o'},
			{"ignore_PTS",required_argument, NULL, 'f'},
			{"keep_PTS",required_argument, NULL, 'k'},
			{"fix_sync",no_argument, NULL, 'n'},
			{"demux",no_argument, NULL, 'z'},
			{"analyze",required_argument, NULL, 'y'},
			{"scan",required_argument, NULL, 's'},
			{"vdr",required_argument, NULL, 'x'},
			{"help", no_argument , NULL, 'h'},
			{0, 0, 0, 0}
		};
		c = getopt_long (argc, argv, 
					"t:o:a:v:i:hp:q:d:c:n:fkd:e:zy:sx",
                                 long_options, &option_index);
		if (c == -1)
    		break;

		switch (c) {
		case 't':
			type = optarg;
			break;
		case 'i':
			inpt = optarg;
			break;
		case 'd':
			rx.video_delay = strtol(optarg,(char **)NULL, 0) 
				*CLOCK_MS;
			break;
		case 'e':
			rx.audio_delay = strtol(optarg,(char **)NULL, 0) 
				*CLOCK_MS;
			break;
		case 'a':
			if (rx.apidn==N_AUDIO){
				LOG(VB_GENERAL, LOG_ERR, "Too many audio PIDs");
				exit(1);
			}
                        rx.apid[rx.apidn] = strtol(optarg,(char **)NULL, 0);
			rx.apidn++;
			break;
		case 'v':
			rx.vpid = strtol(optarg,(char **)NULL, 0);
			break;
		case 'c':
			if (rx.ac3n==N_AC3){
				LOG(VB_GENERAL, LOG_ERR, "Too many audio PIDs");
				exit(1);
			}
			rx.ac3_id[rx.ac3n] = strtol(optarg,(char **)NULL, 0);
			rx.ac3n++;
			break;
		case 'o':
			filename = optarg;
			break;
		case 'f':
			rx.ignore_pts =1;
			break;
		case 'k':
			rx.keep_pts =1;
			break;
		case 'z':
			rx.demux =1;
			break;
		case 'n':
			rx.fix_sync =1;
			break;
		case 'y':
			analyze = strtol(optarg,(char **)NULL, 0);
			if (analyze>2) usage(argv[0]);
			analyze++;
			break;
		case 's':
			scan = 1;
			break;
		case 'x':
			rx.vdr=1;
			break;
		case 'h':
		case '?':
		default:
			usage(argv[0]);
		}
	}

	if (rx.fix_sync)
		av_register_all();
	
	if (optind == argc-1) {
		if ((rx.fd_in = open(argv[optind] ,O_RDONLY| O_LARGEFILE)) < 0) {
			perror("Error opening input file ");
			exit(1);
		}
		LOG(VB_GENERAL, LOG_INFO, "Reading from %s", argv[optind]);
		rx.inflength = lseek(rx.fd_in, 0, SEEK_END);
		LOG(VB_GENERAL, LOG_INFO, "Input file length: %.2f MB",
			rx.inflength/1024.0/1024.0);
		lseek(rx.fd_in,0,SEEK_SET);
		rx.lastper = 0;
		rx.finread = 0;
	} else {
		LOG(VB_GENERAL, LOG_INFO, "using stdin as input");
		rx.fd_in = STDIN_FILENO;
		rx.inflength = 0;
	}

	if (!rx.demux){
		if (filename){
			if ((rx.fd_out = open(filename,O_WRONLY|O_CREAT
					|O_TRUNC|O_LARGEFILE,
					S_IRUSR|S_IWUSR|S_IRGRP|
					S_IWGRP|
					S_IROTH|S_IWOTH)) < 0){
				perror("Error opening output file");
				exit(1);
			}
			LOG(VB_GENERAL, LOG_INFO, "Output File is: %s",
				filename);
		} else {
			rx.fd_out = STDOUT_FILENO;
			LOG(VB_GENERAL, LOG_INFO, "using stdout as output");
		}
	}
	if (scan){
		if (rx.fd_in == STDIN_FILENO){
			LOG(VB_GENERAL, LOG_ERR, "Can`t scan from pipe");
			exit(1);
		}
		do_scan(&rx);
		exit(0);
	}

	if (!strncmp(type,"MPEG2",6))
		rx.otype=REPLEX_MPEG2;
	else if (!strncmp(type,"DVD",4))
		rx.otype=REPLEX_DVD;
	else if (!strncmp(type,"HDTV",4))
		rx.otype=REPLEX_HDTV;
	else if (!rx.demux)
		usage(argv[0]);
	
	if (!strncmp(inpt,"TS",3)){
		rx.itype=REPLEX_TS;
	} else if (!strncmp(inpt,"PS",3)){
		rx.itype=REPLEX_PS;
		if (!rx.vpid) rx.vpid = 0xE0;
		if (!(rx.apidn || rx.ac3n)){
			rx.apidn = 1;
			rx.apid[0] = 0xC0;
		}
	} else if (!strncmp(inpt,"AVI",4)){
		rx.itype=REPLEX_AVI;
		rx.vpid = 0xE0;
		rx.apidn = 1;
		rx.apid[0] = 0xC0;
		rx.ignore_pts =1;
	} else {
		usage(argv[0]);
	}

	init_replex(&rx);
	rx.analyze= analyze;

	if (rx.demux) {
		int i;
		char fname[256];
		if (!filename){
			filename = malloc(4);
			strcpy(filename,"out");
		}
		if (strlen(filename) > 250){
			LOG(VB_GENERAL, LOG_ERR, "Basename too long");
			exit(0);
		}
			
		snprintf(fname,256,"%s.mv2",filename);
		if ((rx.dmx_out[0] = open(fname,O_WRONLY|
					  O_CREAT|O_TRUNC|
					  O_LARGEFILE,
					  S_IRUSR|S_IWUSR|
					  S_IRGRP|S_IWGRP|
					  S_IROTH|S_IWOTH)) 
		    < 0){
			perror("Error opening output file");
			exit(1);
		}
		LOG(VB_GENERAL, LOG_INFO, "Video output File is: %s",
			fname);
		
		for (i=0; i < rx.apidn; i++){
			snprintf(fname,256,"%s%d.mp2",filename
				 ,i);
			if ((rx.dmx_out[i+1] = 
			     open(fname,O_WRONLY|
				  O_CREAT|O_TRUNC|
				  O_LARGEFILE,
				  S_IRUSR|S_IWUSR|
				  S_IRGRP|S_IWGRP|
				  S_IROTH|S_IWOTH)) 
			    < 0){
				perror("Error opening output file");
				exit(1);
			}
			LOG(VB_GENERAL, LOG_INFO, "Audio%d output File is: %s",
				i, fname);
		}
		
		
		for (i=0; i < rx.ac3n; i++){
			snprintf(fname,256,"%s%d.ac3",filename
				 ,i);
			if ((rx.dmx_out[i+1+rx.apidn] = 
			     open(fname,O_WRONLY|
				  O_CREAT|O_TRUNC|
				  O_LARGEFILE,
				  S_IRUSR|S_IWUSR|
				  S_IRGRP|S_IWGRP|
				  S_IROTH|S_IWOTH)) 
			    < 0){
				perror("Error opening output file");
				exit(1);
			}
			LOG(VB_GENERAL, LOG_INFO, "AC3%d output File is: %s",
				i, fname);
		}
		do_demux(&rx);
	} else if (analyze){
		rx.demux=1;
		do_analyze(&rx);
	} else {
		do_replex(&rx);
	}

	return 0;
}

void LogPrintLine( uint64_t mask, LogLevel_t level, const char *file, int line,
                   const char *function, int fromQString, 
                   const char *format, ... )
{
	va_list         arguments;

	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fprintf(stderr, "\n");
}
