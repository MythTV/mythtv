/*
 * replex.c
 *        
 *
 * Copyright (C) 2003 Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */
//#define IN_DEBUG 


#include <stdlib.h>
#include <getopt.h>
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

static int replex_all_set(struct replex *rx);

int replex_check_id(struct replex *rx, uint16_t id)
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

void analyze_audio( pes_in_t *p, struct replex *rx, int len, int num, int type)
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
		fprintf(stderr, "AC3\n");
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
		fprintf(stderr, "MPEG AUDIO\n");
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
						fprintf(stderr,
							"starting audio PTS: ");
						printpts(*fpts);
						fprintf(stderr,"\n");
					} else aframe->set = 0;
				}

				if (aframe->set && first)
					ring_skip(rbuf,pos+c);
				
			} else {
				int diff = ring_posdiff(rbuf, iu->start, 
							p->ini_pos + pos+c);
				
				if ( (re =check_audio_header(rbuf, aframe, 
							     pos+c+off,len-c-pos,
							     type)) < 0){
			
					if ( re == -2){
						*apes_abort = len -c;
						return;
					}
					
					if (aframe->framesize > diff){
						if ( re == -3){
							c+= pos+1;
							continue;
						}
					
						c += pos+2;
#ifdef IN_DEBUG
						fprintf(stderr,"WRONG HEADER1 %d\n", diff);
#endif
						continue;
					}
				}
				if (aframe->framesize > diff){
					c += pos+2;
					//fprintf(stderr,"WRONG HEADER2 %d\n", diff);
					continue;
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
						fprintf(stderr,"Wrong audio frame size: %d\n", iu->length);
						iu->err= FRAME_ERR;
					}
					if (ring_write(index_buf, 
							(uint8_t *)iu,
							sizeof(index_unit)) < 0){
						fprintf(stderr,"audio ring buffer overrun error\n");
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
						fprintf(stderr,"audio PTS inconsistent: ");
						printpts(trans_pts_dts(p->pts)-*fpts);
						printpts(iu->pts);
						fprintf(stderr,"diff: ");
						printpts(abs(diff));
						fprintf(stderr,"\n");
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
							fprintf(stderr, 
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
			if (c + aframe->framesize > len){
//				fprintf(stderr,"SHORT %d\n", len -c);
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

void analyze_video( pes_in_t *p, struct replex *rx, int len)
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
	fprintf(stderr, " ini pos %d\n",
		(p->ini_pos)%rx->videobuf);
#endif

	
//	fprintf(stderr, "len %d  %d\n",len,off);
	while (c < len){
		if ((pos = ring_find_any_header( rbuf, &head, c+off, len-c)) 
		    >=0 ){
			switch(head){
			case SEQUENCE_HDR_CODE:
#ifdef IN_DEBUG
				fprintf(stderr, " seq headr %d\n",
					(p->ini_pos+c+pos)%rx->videobuf);
#endif

				seq_h = 1;
				seq_p = c+pos;
				
				
				if (!s->set){ 
					int re=0;
					re = get_video_info(rbuf, &rx->seq_head, 
							    pos+c+off, len -c -pos);

#ifdef IN_DEBUG
					fprintf(stderr, " seq headr result %d\n",re);
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
				fprintf(stderr," seq ext headr %d\n",
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
						fprintf(stderr,"starting with video PTS: ");
						printpts(rx->first_vpts);
						fprintf(stderr,"\n");
					}

					newpts = 0;
#ifdef IN_DEBUG
					
					fprintf(stderr,"fcount %d  gcount %d  tempref %d  %d\n",
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
							fprintf(stderr,"video PTS inconsistent: ");
							printpts(trans_pts_dts(p->pts));
							printpts(iu->pts);
							printpts(newpts+rx->first_vpts);
							printpts(newpts);
							fprintf(stderr," diff: ");
							printpts(abs((int)diff));
							fprintf(stderr,"\n");
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
							fprintf(stderr,"video DTS inconsistent: ");
							printpts(trans_pts_dts(p->dts));
							printpts(iu->dts);
							printpts(newdts+rx->first_vpts);
							printpts(newdts);
							fprintf(stderr,"diff: ");
							printpts(abs((int)diff));
							fprintf(stderr,"\n");
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
						fprintf(stderr,
							"Warning negative video PTS increase!\n");
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
				fprintf(stderr, " seq end %d\n",
					(p->ini_pos+c+pos)%rx->videobuf);
#endif
				if (s->set)
					seq_end = 1;
				
				break;

			case GROUP_START_CODE:{
				int hour, min, sec;
//#define ANA
#ifdef ANA
				fprintf(stderr,"  %d", (int)rx->vgroup_count);
				fprintf(stderr,"\n");

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
				fprintf(stderr,	" gop %02d:%02d.%02d %d\n",
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
				fprintf(stderr,"I");
#endif
#ifdef IN_DEBUG
					fprintf(stderr, " I-frame %d\n",
						(p->ini_pos+c+pos)%rx->videobuf);
#endif
					break;
				case B_FRAME:
#ifdef ANA
				fprintf(stderr,"B");
#endif
#ifdef IN_DEBUG
					fprintf(stderr, " B-frame %d\n",
						(p->ini_pos+c+pos)%rx->videobuf);
#endif
					break;
				case P_FRAME:
#ifdef ANA
				fprintf(stderr,"P");
#endif
#ifdef IN_DEBUG
					fprintf(stderr,  " P-frame %d\n",
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
				//fprintf(stderr,"other header 0x%02x (%d+%d)\n",head,c,pos);
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
						fprintf(stderr,"video ring buffer overrun error\n");
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
				fprintf(stderr,"START %d\n", iu->start);
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

void es_out(pes_in_t *p)
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
		return;


	}

#ifdef IN_DEBUG
	fprintf(stderr,"%s PES\n", t);
#endif
}

void pes_es_out(pes_in_t *p)
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
			fprintf(stderr,"video ring buffer overrun error\n");
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
			fprintf(stderr,"video ring buffer overrun error\n");
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
			fprintf(stderr,"video ring buffer overrun error\n");
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
	fprintf(stderr,"%s PES %d\n", t,len);
#endif
}

void avi_es_out(pes_in_t *p)
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
	fprintf(stderr,"%s PES\n", t);
#endif

}


int replex_tsp(struct replex *rx, uint8_t *tsp)
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


ssize_t save_read(struct replex *rx, void *buf, size_t count)
{
	ssize_t neof = 1;
	size_t re = 0;
	int fd = rx->fd_in;

	if (rx->itype== REPLEX_AVI){
		int l = rx->inflength - rx->finread;
		if ( l <= 0) return 0;
		if ( count > l) count = l;
	}
	while(neof >= 0 && re < count){
		neof = read(fd, buf+re, count - re);
		if (neof > 0) re += neof;
		else break;
	}
	rx->finread += re;
#ifndef OUT_DEBUG
	if (rx->inflength){
		uint8_t per=0;
		
		per = (uint8_t)(rx->finread*100/rx->inflength);
		if (rx->lastper < per){
			fprintf(stderr,"read %3d%%\r", (int)per);
			rx->lastper = per;
		}
	} else fprintf(stderr,"read %.2f MB\r", rx->finread/1024./1024.);
#endif
	if (neof < 0 && re == 0) return neof;
	else return re;
}

int guess_fill( struct replex *rx)
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
			if (fill < ring_free(&rx->arbuffer[i]))
				fill = ring_free(&rx->arbuffer[i]);
	}

	for (i=0; i<rx->ac3n;i++){
		if ((ac3avail = ring_avail(&rx->index_ac3rbuffer[i])
		     /sizeof(index_unit)) < LIMIT)
			if (fill < ring_free(&rx->ac3rbuffer[i]))
				fill = ring_free(&rx->ac3rbuffer[i]);
	}

//	fprintf(stderr,"free %d  %d %d %d\n",fill, vavail, aavail, ac3avail);

	if (!fill){ 
		if(vavail && (aavail || ac3avail)) return 0;
		else return -1;
	}

	return fill/2;
}



#define IN_SIZE (1000*TS_SIZE)
void find_pids_file(struct replex *rx)
{
	uint8_t buf[IN_SIZE];
	int afound=0;
	int vfound=0;
	int count=0;
	int re=0;
	uint16_t vpid=0, apid=0, ac3pid=0;
		
	fprintf(stderr,"Trying to find PIDs\n");
	while (!afound && !vfound && count < rx->inflength){
		if (rx->vpid) vfound = 1;
		if (rx->apidn) afound = 1;
		if ((re = save_read(rx,buf,IN_SIZE))<0)
			perror("reading");
		else 
			count += re;
		if ( (re = find_pids(&vpid, &apid, &ac3pid, buf, re))){
			if (!rx->vpid && vpid){
				rx->vpid = vpid;
				fprintf(stderr,"vpid 0x%04x  \n",
					(int)rx->vpid);
				vfound++;
			}
			if (!rx->apidn && apid){
				rx->apid[0] = apid;
				fprintf(stderr,"apid 0x%04x  \n",
					(int)rx->apid[0]);
				rx->apidn++;
				afound++;
			}
			if (!rx->ac3n && ac3pid){
				rx->ac3_id[0] = ac3pid;
				fprintf(stderr,"ac3pid 0x%04x  \n",
					(int)rx->ac3_id[0]);
				rx->ac3n++;
				afound++;
			}
			
		} 
		
	}
	
	lseek(rx->fd_in,0,SEEK_SET);
	if (!afound || !vfound){
		fprintf(stderr,"Couldn't find all pids\n");
		exit(1);
	}
	
}

#define MAXVPID 16
#define MAXAPID 32
#define MAXAC3PID 16
void find_all_pids_file(struct replex *rx)
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

	fprintf(stderr,"Trying to find PIDs\n");
	while (count < rx->inflength-IN_SIZE){
		if ((re = save_read(rx,buf,IN_SIZE))<0)
			perror("reading");
		else 
			count += re;
		if ( (re = find_pids_pos(&vp, &ap, &cp, buf, re, 
					 &vpos, &apos, &cpos))){
			if (vp){
				int old=0;
				for (j=0; j < vn; j++){
//					printf("%d. %d\n",j+1,vpid[j]);
					if (vpid[j] == vp){
						old = 1;
						break;
					}
				}
				if (!old){
					vpid[vn]=vp;
					
					printf("vpid %d: 0x%04x (%d)  PES ID: 0x%02x\n",
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
					printf("apid %d: 0x%04x (%d)  PES ID: 0x%02x\n",
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
					printf("ac3pid %d: 0x%04x (%d) \n", 
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

void find_pids_stdin(struct replex *rx, uint8_t *buf, int len)
{
	int afound=0;
	int vfound=0;
	uint16_t vpid=0, apid=0, ac3pid=0;
		
	if (rx->vpid) vfound = 1;
	if (rx->apidn) afound = 1;
	fprintf(stderr,"Trying to find PIDs\n");
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
		fprintf(stderr,"found ");
		if (rx->vpid) fprintf(stderr,"vpid %d (0x%04x)  ",
				      rx->vpid, rx->vpid);
		if (rx->apidn) fprintf(stderr,"apid %d (0x%04x)  ",
				       rx->apid[0], rx->apid[0]);
		if (rx->ac3n) fprintf(stderr,"ac3pid %d (0x%04x)  ",
				      rx->ac3_id[0], rx->ac3_id[0]);
		fprintf(stderr,"\n");
	} else {
		fprintf(stderr,"Couldn't find pids\n");
		exit(1);
	}

}


void pes_id_out(pes_in_t *p)
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
						//fprintf(stderr,"0x%04x  0x%04x \n",
						//c-9-p->hlength-4,fframe);
//					if (id>0x80)show_buf(p->buf+9+p->hlength,8);
						//				if (id>0x80)show_buf(p->buf+c,8);
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

void find_pes_ids(struct replex *rx)
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

	fprintf(stderr,"Trying to find PES IDs\n");
	rx->scan_found=0;
	rx->pvideo.priv = rx ;
	while (count < rx->inflength-IN_SIZE){
		if ((re = save_read(rx,buf,IN_SIZE))<0)
			perror("reading");
		else 
			count += re;
		
		get_pes(&rx->pvideo, buf, re, pes_id_out);
		
		if ( rx->scan_found ){

			switch (rx->scan_found){
				
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			{
				int old=0;
				for (j=0; j < vn; j++){
//					printf("%d. %d\n",j+1,vpid[j]);
					if (vpid[j] == rx->scan_found){
						old = 1;
						break;
					}
				}
				if (!old){
					vpid[vn]=rx->scan_found;
					
					printf("MPEG VIDEO %d: 0x%02x (%d)\n",
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
					printf("MPEG AUDIO %d: 0x%02x (%d)\n",
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
						printf("possible AC3 AUDIO with private stream 1 pid (0xbd) \n");
					}else{
						printf("AC3 AUDIO %d: 0x%02x (%d) \n", 
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



void replex_finish(struct replex *rx)
{

	fprintf(stderr,"\n");
	if (!replex_all_set(rx)){
		fprintf(stderr,"Can't find all required streams\n");
		if (rx->itype == REPLEX_PS){
			fprintf(stderr,"Please check if audio and video have standard IDs (0xc0 or 0xe0)\n");
		}
		exit(1);
	}

	if (!rx->demux)
		finish_mpg((multiplex_t *)rx->priv);
	exit(0);
}

int replex_fill_buffers(struct replex *rx, uint8_t *mbuf)
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
	//fprintf(stderr,"trying to fill buffers with %d\n",fill);
	if (fill < 0) return -1;

	memset(buf, 0, IN_SIZE);
	
	switch(rx->itype){
	case REPLEX_TS:
		if (fill < IN_SIZE){
			rsize = fill - (fill%188);
		} else rsize = IN_SIZE;
		
//	fprintf(stderr,"filling with %d\n",rsize);
		
		if (!rsize) return 0;
		
		memset(buf, 0, IN_SIZE);
		
		if ( mbuf ){
			for ( i = 0; i < 188 ; i++){
				if ( mbuf[i] == 0x47 ) break;
			}
			
			if ( i == 188){
				fprintf(stderr,"Not a TS\n");
				return -1;
			} else {
				memcpy(buf,mbuf+i,2*TS_SIZE-i);
				if ((count = save_read(rx,mbuf,i))<0)
					perror("reading");
				memcpy(buf+2*TS_SIZE-i,mbuf,i);
				i = 188;
			}
		} else i=0;

	
#define MAX_TRIES 5
		while (count < rsize && tries < MAX_TRIES){
			if ((re = save_read(rx,buf+i,rsize-i)+i)<0)
				perror("reading");
			else 
				count += re;
			tries++;
			
			if (!rx->vpid || !(rx->apidn || rx->ac3n)){
				find_pids_stdin(rx, buf, re);
			}

			for( j = 0; j < re; j+= TS_SIZE){
				
				if ( re - j < TS_SIZE) break;
				
				if ( replex_tsp( rx, buf+j) < 0){
					fprintf(stderr, "Error reading TS\n");
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
				perror("reading PS");
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
					perror("reading AVI");
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

int fill_buffers(void *r, int finish)
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


int check_stream_type(struct replex *rx, uint8_t * buf, int len)
{
	int c=0;
	avi_context ac;
	uint8_t head;

	if (rx->itype != REPLEX_TS) return rx->itype;

	if (len< 2*TS_SIZE){
		fprintf(stderr,"cannot determine streamtype");
		exit(1);
	}

	fprintf(stderr, "Checking for TS: ");
	while (c < len && buf[c]!=0x47) c++;
	if (c<len && len-c>=TS_SIZE){
		if (buf[c+TS_SIZE] == 0x47){
			fprintf(stderr,"confirmed\n");
			return REPLEX_TS;
		} else  fprintf(stderr,"failed\n");
	} else  fprintf(stderr,"failed\n");

	fprintf(stderr, "Checking for AVI: ");
	if (check_riff(&ac, buf, len)>=0){
		fprintf(stderr,"confirmed\n");
		rx->itype = REPLEX_AVI;
		rx->vpid = 0xE0;
		rx->apidn = 1;
		rx->apid[0] = 0xC0;
		rx->ignore_pts =1;
		return REPLEX_AVI;
	} else fprintf(stderr,"failed\n");

	fprintf(stderr, "Checking for PS: ");
	if (find_any_header(&head, buf, len) >= 0){
		fprintf(stderr,"confirmed(maybe)\n");
	} else {
		fprintf(stderr,"failed, trying it anyway\n");
	}
	rx->itype=REPLEX_PS;
	if (!rx->vpid) rx->vpid = 0xE0;
	if (!(rx->apidn || rx->ac3n)){
		rx->apidn = 1;
		rx->apid[0] = 0xC0;
	}
	return REPLEX_PS;
}


void init_replex(struct replex *rx)
{
	int i;
	uint8_t mbuf[2*TS_SIZE];
	
	rx->analyze=0;

	if (save_read(rx, mbuf, 2*TS_SIZE)<0)
		perror("reading");
	
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
			fprintf(stderr,"error filling buffer\n");
			exit(1);
		}
	} else if ( rx->itype == REPLEX_AVI){
#define AVI_S 1024
		avi_context *ac;
		uint8_t buf[AVI_S];
		int re=0;
		
		lseek(rx->fd_in, 0, SEEK_SET);
		ac = &rx->ac;
		memset(ac, 0, sizeof(avi_context));
		save_read(rx, buf, 12);
		
		if (check_riff(ac, buf, 12) < 0){
			fprintf(stderr, "Wrong RIFF header\n");
			exit(1);
		} else {
			fprintf(stderr,"Found RIFF header\n");
		}
		
		memset(ac, 0, sizeof(avi_context));
		re = read_avi_header(ac, rx->fd_in);
		if (avi_read_index(ac,rx->fd_in) < 0){
			fprintf(stderr, "Error reading index\n");
			exit(1);
		}
//		rx->aframe_count[0] = ac->ai[0].initial_frames;
		rx->vframe_count = ac->ai[0].initial_frames*ac->vi.fps/
			ac->ai[0].fps;

		rx->inflength = lseek(rx->fd_in, 0, SEEK_CUR)+ac->movi_length;

		fprintf(stderr,"AVI initial frames %d\n",
			(int)rx->vframe_count);
		if (!ac->done){
			fprintf(stderr,"Error reading AVI header\n");
			exit(1);
		}

		if (replex_fill_buffers(rx, buf+re)< 0) {
			fprintf(stderr,"error filling buffer\n");
			exit(1);
		}
	} else {
		if (replex_fill_buffers(rx, mbuf)< 0) {
			fprintf(stderr,"error filling buffer\n");
			exit(1);
		}
	}

}


void fix_audio(struct replex *rx, multiplex_t *mx)
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
					fprintf(stderr,
						"error in fix audio\n");
					exit(1);
				}	
			}
			ring_peek(&rx->index_arbuffer[i], (uint8_t *)&aiu, size, 0);
			if ( ptscmp(aiu.pts + rx->first_apts[i], rx->first_vpts) < 0){
				ring_skip(&rx->index_arbuffer[i], size);
				ring_skip(&rx->arbuffer[i], aiu.length);
			} else break;

		} while (1);
		mx->extpts_off[i] = aiu.pts;
		mx->extframes[i] = aiu.framesize;
		
		fprintf(stderr,"Audio%d  offset: ",i);
		printpts(mx->extpts_off[i]);
		printpts(rx->first_apts[i]+mx->extpts_off[i]);
		fprintf(stderr,"\n");
	}
			  
	for ( i = 0; i < rx->ac3n; i++){
		do {
			while (ring_avail(&rx->index_ac3rbuffer[i]) < 
			       sizeof(index_unit)){
				if (replex_fill_buffers(rx, 0)< 0) {
					fprintf(stderr,
						"error in fix audio\n");
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
		mx->extpts_off[i] = aiu.pts;
		
		fprintf(stderr,"AC3%d  offset: ",i);
		printpts(mx->extpts_off[i]);
		printpts(rx->first_ac3pts[i]+mx->extpts_off[i]);
		fprintf(stderr,"\n");

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


void do_analyze(struct replex *rx)
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
	
	fprintf(stderr,"STARTING ANALYSIS\n");
	
	
	while(!rx->finish){
		if (replex_fill_buffers(rx, 0)< 0) {
			fprintf(stderr,"error in get next video unit\n");
			return;
		}
		for (i=0; i< rx->apidn; i++){
			while(get_next_audio_unit(rx, &dummy2, i)){
				ring_skip(&rx->arbuffer[i], 
					  dummy2.length);
				if (av>=1){
					fprintf(stdout,
						"MPG2 Audio%d unit: ",
						i);
					fprintf(stdout,
						" length %d  PTS ",
						dummy2.length);
					printptss(dummy2.pts);
					
					if (lastapts[i]){
						fprintf(stdout,"  diff:");
						printptss(ptsdiff(dummy2.pts,lastapts[i]));
					}
					lastapts[i] = dummy2.pts;
					
					fprintf(stdout,"\n");
				}
			}
		}
		
		for (i=0; i< rx->ac3n; i++){
			while(get_next_ac3_unit(rx, &dummy2, i)){
				ring_skip(&rx->ac3rbuffer[i], 
					  dummy2.length);
				if (av>=1){
					fprintf(stdout,
						"AC3 Audio%d unit: ",
						i);
					fprintf(stdout,
						" length %d  PTS ",
						dummy2.length);
					printptss(dummy2.pts);
					if (lastac3pts[i]){
						fprintf(stdout,"  diff:");
						printptss(ptsdiff(dummy2.pts, lastac3pts[i]));
					}
					lastac3pts[i] = dummy2.pts;
					
					fprintf(stdout,"\n");
				}
			}
		}
		
		while (get_next_video_unit(rx, &dummy)){
			ring_skip(&rx->vrbuffer,
				  dummy.length);
			if (av==0 || av==2){
				fprintf(stdout,
					"Video unit: ");
	                        if (dummy.seq_header){
					fprintf(stdout,"Sequence header ");
				}
				if (dummy.gop){
					fprintf(stdout,"GOP header ");
				}
				switch (dummy.frame){
				case I_FRAME:
					fprintf(stdout, "I-frame");
					break;
				case B_FRAME:
					fprintf(stdout, "B-frame");
					break;
				case P_FRAME:
					fprintf(stdout, "P-frame");
					break;
				}
				fprintf(stdout,
					" length %d  PTS ",
					dummy.length);
				printptss(dummy.pts);
				if (lastvpts){
					fprintf(stdout,"  diff:");
					printptss(ptsdiff(dummy.pts, lastvpts));
				}
				lastvpts = dummy.pts;
					
				fprintf(stdout,
					"  DTS ");
				printptss(dummy.dts);
				if (lastvdts){
					fprintf(stdout,"  diff:");
					printptss(ptsdiff(dummy.dts,lastvdts));
				}
				lastvdts = dummy.dts;
					
				fprintf(stdout,"\n");
			}
		}
	}


}

void do_scan(struct replex *rx)
{
	uint8_t mbuf[2*TS_SIZE];
	
	rx->analyze=0;

	if (save_read(rx, mbuf, 2*TS_SIZE)<0)
		perror("reading");
	
	fprintf(stderr,"STARTING SCAN\n");
	
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

void do_demux(struct replex *rx)
{
	index_unit dummy;
	index_unit dummy2;
	int i;
	fprintf(stderr,"STARTING DEMUX\n");
	
	while(!rx->finish){
		if (replex_fill_buffers(rx, 0)< 0) {
			fprintf(stderr,"error in get next video unit\n");
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


void do_replex(struct replex *rx)
{
	int video_ok = 0;
	int ext_ok[N_AUDIO];
	int start=1;
	multiplex_t mx;


	fprintf(stderr,"STARTING REPLEX\n");
	memset(&mx, 0, sizeof(mx));
	memset(ext_ok, 0, N_AUDIO*sizeof(int));

	while (!replex_all_set(rx)){
		if (replex_fill_buffers(rx, 0)< 0) {
			fprintf(stderr,"error filling buffer\n");
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


void usage(char *progname)
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
        char *type = "SVCD";
        char *inpt = "TS";

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
			{"demux",no_argument, NULL, 'z'},
			{"analyze",required_argument, NULL, 'y'},
			{"scan",required_argument, NULL, 's'},
			{"vdr",required_argument, NULL, 'x'},
			{"help", no_argument , NULL, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long (argc, argv, 
				 "t:o:a:v:i:hp:q:d:c:fkd:e:zy:sx",
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
				fprintf(stderr,"Too many audio PIDs\n");
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
				fprintf(stderr,"Too many audio PIDs\n");
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

        if (optind == argc-1) {
                if ((rx.fd_in = open(argv[optind] ,O_RDONLY| O_LARGEFILE)) < 0) {
                        perror("Error opening input file ");
                        exit(1);
                }
                fprintf(stderr,"Reading from %s\n", argv[optind]);
		rx.inflength = lseek(rx.fd_in, 0, SEEK_END);
		fprintf(stderr,"Input file length: %.2f MB\n",rx.inflength/1024./1024.);
		lseek(rx.fd_in,0,SEEK_SET);
		rx.lastper = 0;
		rx.finread = 0;
        } else {
		fprintf(stderr,"using stdin as input\n");
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
			fprintf(stderr,"Output File is: %s\n", 
				filename);
		} else {
			rx.fd_out = STDOUT_FILENO;
			fprintf(stderr,"using stdout as output\n");
		}
	}
	if (scan){
		if (rx.fd_in == STDIN_FILENO){
			fprintf(stderr,"Can`t scan from pipe\n");
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
			fprintf(stderr,"Basename too long\n");
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
		fprintf(stderr,"Video output File is: %s\n", 
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
			fprintf(stderr,"Audio%d output File is: %s\n",i,fname);
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
			fprintf(stderr,"AC3%d output File is: %s\n",i,fname);
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
