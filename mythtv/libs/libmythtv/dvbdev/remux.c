/*
 *  dvb-mpegtools for the Siemens Fujitsu DVB PCI card
 *
 * Copyright (C) 2000, 2001 Marcus Metzler 
 *            for convergence integrated media GmbH
 * Copyright (C) 2002 Marcus Metzler 
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 

 * The author can be reached at mocm@metzlerbros.de, 
 */

#include "remux.h"

unsigned int bitrates[3][16] =
{{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

uint32_t freq[4] = {441, 480, 320, 0};
static uint32_t samples[4] = { 384, 1152, 0, 0};
char *frames[3] = {"I-Frame","P-Frame","B-Frame"};


void copy_ptslm(PTS_List *a, PTS_List *b)
{
	a->pos  = b->pos;
	a->PTS  = b->PTS;
	a->dts  = b->dts;
	a->spos = b->spos;
}

void clear_ptslm(PTS_List *a)
{
	a->pos  = 0;
	a->PTS  = 0;
	a->dts  = 0;
	a->spos = 0;	
}

void init_ptsl(PTS_List *ptsl)
{
	int i;
	for (i=0;i< MAX_PTS;i++){
		clear_ptslm(&ptsl[i]);
	}
}

int del_pts(PTS_List *ptsl, int pos, int nr)
{
	int i;
	int del = 0;

	for( i = 0; i < nr-1; i++){
		if(pos > ptsl[i].pos && pos >= ptsl[i+1].pos) del++;
	}

	if(del)
		for( i = 0; i < nr-del; i++){
			copy_ptslm(&ptsl[i], &ptsl[i+del]);
		}

	return nr-del;
}

int del_ptss(PTS_List *ptsl, int pts, int *nb)
{
	int i;
	int del = 0;
	int sum = 0;
	int nr = *nb;

	for( i = 0; i < nr; i++){
		if(pts > ptsl[i].PTS){
			del++;
			sum += ptsl[i].pos;
		}
	}

	if(del)
		for( i = 0; i < nr-del; i++){
			copy_ptslm(&ptsl[i], &ptsl[i+del]);
		}

	*nb = nr-del;
	return sum;
}

int add_pts(PTS_List *ptsl, uint32_t pts, int pos, int spos, int nr, uint32_t dts)
{
	int i;

	for ( i=0;i < nr; i++) if (spos &&  ptsl[i].pos == pos) return nr;
	if (nr == MAX_PTS) {
		nr = del_pts(ptsl, ptsl[1].pos+1, nr);
	} else nr++;
	i = nr-1;
	
	ptsl[i].pos  = pos;
	ptsl[i].spos = spos;
	ptsl[i].PTS  = pts;
	ptsl[i].dts  = dts;
	return nr;
}

void add_vpts(Remux *rem, uint8_t *pts)
{
	uint32_t PTS = trans_pts_dts(pts);
	rem->vptsn = add_pts(rem->vpts_list, PTS, rem->vwrite, rem->awrite,
			     rem->vptsn, PTS);
}

void add_apts(Remux *rem, uint8_t *pts)
{
	uint32_t PTS = trans_pts_dts(pts);
	rem->aptsn = add_pts(rem->apts_list, PTS, rem->awrite, rem->vwrite,
			     rem->aptsn, PTS);
}

void del_vpts(Remux *rem)
{
	rem->vptsn = del_pts(rem->vpts_list, rem->vread, rem->vptsn);
}

void del_apts(Remux *rem)
{
	rem->aptsn = del_pts(rem->apts_list, rem->aread, rem->aptsn);
}


void copy_framelm(FRAME_List *a, FRAME_List *b)
{
	a->type  = b->type;
	a->pos   = b->pos;
	a->FRAME = b->FRAME;
	a->time  = b->time;
	a->pts   = b->pts;
	a->dts   = b->dts;
}

void clear_framelm(FRAME_List *a)
{
	a->type  = 0;
	a->pos   = 0;
	a->FRAME = 0;
	a->time  = 0; 
	a->pts   = 0; 
	a->dts   = 0; 
}

void init_framel(FRAME_List *framel)
{
	int i;
	for (i=0;i< MAX_FRAME;i++){
		clear_framelm(&framel[i]);
	}
}

int del_frame(FRAME_List *framel, int pos, int nr)
{
	int i;
	int del = 0;

	for( i = 0; i < nr-1; i++){
		if(pos > framel[i].pos && pos >= framel[i+1].pos) del++;
	}

	if(del)
		for( i = 0; i < nr-del; i++){
			copy_framelm(&framel[i], &framel[i+del]);
		}

	return nr-del;
}

int add_frame(FRAME_List *framel, uint32_t frame, int pos, int type, int nr, 
	      uint32_t time, uint32_t pts, uint32_t dts)
{
	int i;

	if (nr == MAX_FRAME) {
		nr = del_frame(framel, framel[1].pos+1, nr);
	} else nr++;
	i = nr-1;
	
	framel[i].type  = type;
	framel[i].pos   = pos;
	framel[i].FRAME = frame;
	framel[i].time  = time;
	framel[i].pts   = pts;
	framel[i].dts   = dts;
	return nr;
}

void add_vframe(Remux *rem, uint32_t frame, long int pos, int type, int time, 
		uint32_t pts, uint32_t dts)
{
	rem->vframen = add_frame(rem->vframe_list, frame, pos, type,
				 rem->vframen, time, pts, dts);
}

void add_aframe(Remux *rem, uint32_t frame, long int pos, uint32_t pts)
{
	rem->aframen = add_frame(rem->aframe_list, frame, pos, 0, 
				 rem->aframen, 0, pts, pts);
}

void del_vframe(Remux *rem)
{
	rem->vframen = del_frame(rem->vframe_list, rem->vread, rem->vframen);
}

void del_aframe(Remux *rem)
{
	rem->aframen = del_frame(rem->aframe_list, rem->aread, rem->aframen);
}


void printpts(uint32_t pts)
{
	fprintf(stderr,"%2d:%02d:%02d.%03d",
		(int)(pts/90000.)/3600,
		((int)(pts/90000.)%3600)/60,
		((int)(pts/90000.)%3600)%60,
		(((int)(pts/90.)%3600000)%60000)%1000
		);
}


void find_vframes( Remux *rem, uint8_t *buf, int l)
{
 	int c = 0;
	int type;
	uint32_t time = 0;
	int hour;
	int min;
	int sec;
	uint64_t pts=0;
	uint64_t dts=0;
	uint32_t tempref = 0;

	while ( c < l - 6){
		if (buf[c] == 0x00 && 
		    buf[c+1] == 0x00 &&
		    buf[c+2] == 0x01 && 
		    buf[c+3] == 0xB8) {
			c += 4;
			hour = (int)((buf[c]>>2)& 0x1F);
			min  = (int)(((buf[c]<<4)& 0x30)| 
				     ((buf[c+1]>>4)& 0x0F));
			sec  = (int)(((buf[c+1]<<3)& 0x38)|
				      ((buf[c+2]>>5)& 0x07));
  
			time = 3600*hour + 60*min + sec;
			if ( rem->time_off){
				time = (uint32_t)((uint64_t)time - rem->time_off);
				hour = time/3600;
				min  = (time%3600)/60;
				sec  = (time%3600)%60;
				/*
				buf[c]   |= (hour & 0x1F) << 2;
				buf[c]   |= (min & 0x30) >> 4;
				buf[c+1] |= (min & 0x0F) << 4;
				buf[c+1] |= (sec & 0x38) >> 3;
				buf[c+2] |= (sec & 0x07) >> 5;*/
			}
			rem->group++;
			rem->groupframe = 0;
		}
		if ( buf[c] == 0x00 && 
		     buf[c+1] == 0x00 &&
		     buf[c+2] == 0x01 && 
		     buf[c+3] == 0x00) {
			c += 4;
			tempref = (buf[c+1]>>6) & 0x03;
			tempref |= buf[c] << 2;
			
			type = ((buf[c+1]&0x38) >>3);
			if ( rem->video_info.framerate){
				pts = ((uint64_t)rem->vframe + tempref + 1 
					- rem->groupframe ) * 90000ULL
					    /rem->video_info.framerate 
					+ rem->vpts_off;
				dts = (uint64_t)rem->vframe * 90000ULL/
					rem->video_info.framerate 
					+ rem->vpts_off;
			

fprintf(stderr,"MYPTS:");
printpts((uint32_t)pts-rem->vpts_off);
 fprintf(stderr,"   REALPTS:");
 printpts(rem->vpts_list[rem->vptsn-1].PTS-rem->vpts_off);
 fprintf(stderr,"   DIFF:");
 printpts(pts-(uint64_t)rem->vpts_list[rem->vptsn-1].PTS);
// fprintf(stderr,"   DIST: %4d",-rem->vpts_list[rem->vptsn-1].pos+(rem->vwrite+c-4));
 //fprintf(stderr,"   ERR: %3f",(double)(-rem->vpts_list[rem->vptsn-1].PTS+pts)/(rem->vframe+1));
 fprintf(stderr,"\r");

				
				
				rem->vptsn = add_pts(rem->vpts_list,(uint32_t)pts
						     ,rem->vwrite+c-4,
						     rem->awrite,
						     rem->vptsn,
						     (uint32_t)dts);

				
				
			}
			rem->vframe++;
			rem->groupframe++;
			add_vframe( rem, rem->vframe, rem->vwrite+c, type, 
				    time, pts, dts);
		} else c++;
	}
}

void find_aframes( Remux *rem, uint8_t *buf, int l)
{
 	int c = 0;
	uint64_t pts = 0;
	int sam;
	uint32_t fr;


	while ( c < l - 2){
		if ( buf[c] == 0xFF && 
		     (buf[c+1] & 0xF8) == 0xF8) {
			c += 2;
			if ( rem->audio_info.layer >= 0){
				sam = samples[3-rem->audio_info.layer];
				fr = freq[rem->audio_info.frequency] ;
		
			  pts = ( (uint64_t)rem->aframe * sam * 900ULL)/fr
				  + rem->apts_off;
				

fprintf(stderr,"MYPTS:");
printpts((uint32_t)pts-rem->apts_off);
 fprintf(stderr," REALPTS:");
 printpts(rem->apts_list[rem->aptsn-1].PTS-rem->apts_off);
 fprintf(stderr," DIFF:");
 printpts((uint32_t)((uint64_t)rem->apts_list[rem->aptsn-1].PTS-pts));
// fprintf(stderr," DIST: %4d",-rem->apts_list[rem->aptsn-1].pos+(rem->awrite+c-2));
 fprintf(stderr,"\r");

			  rem->aptsn = add_pts(rem->apts_list,(uint32_t)pts
					     ,rem->awrite+c-2,
					     rem->vwrite,
					     rem->aptsn,
					     (uint32_t)pts);
		}

			rem->aframe++;
			add_aframe( rem, rem->aframe, rem->awrite+c, pts);
			
		} else c++;
	}
}

int refill_buffy(Remux *rem)
{
	pes_packet pes;
	int count = 0;
	int acount, vcount;
	ringbuffy *vbuf = &rem->vid_buffy;
	ringbuffy *abuf = &rem->aud_buffy;
	int fin = rem->fin;

	acount = abuf->size-ring_rest(abuf);
	vcount = vbuf->size-ring_rest(vbuf);
	
	
	while ( acount > MAX_PLENGTH && vcount > MAX_PLENGTH && count < 10){
		int neof;
		count++;
		init_pes(&pes);
		if ((neof = read_pes(fin,&pes)) <= 0) return -1;
		switch(pes.stream_id){
		case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			rem->apes++;
			if( rem->audio_info.layer < 0 &&
			    (pes.flags2 & PTS_DTS) )
				add_apts(rem, pes.pts);
			find_aframes( rem, pes.pes_pckt_data, pes.length);
			ring_write(abuf,(char *)pes.pes_pckt_data,pes.length);
			rem->awrite += pes.length;
			break;
		case VIDEO_STREAM_S ... VIDEO_STREAM_E:
			rem->vpes++;
			if( !rem->video_info.framerate &&
			    (pes.flags2 & PTS_DTS) )
				add_vpts(rem, pes.pts);

			find_vframes( rem, pes.pes_pckt_data, pes.length);

			ring_write(vbuf,(char *)pes.pes_pckt_data,pes.length);
			rem->vwrite += pes.length;
			break;
		}
		acount = abuf->size-ring_rest(abuf);
		vcount = vbuf->size-ring_rest(vbuf);
		kill_pes(&pes);
	}
	if (count < 10)	return 0;
	return 1;
}

int vring_read( Remux *rem, uint8_t *buf, int l)
{
	int c = 0;
	int r = 0;

	if (ring_rest(&rem->vid_buffy) <= l)
		r = refill_buffy(rem);
	if (r) return -1;

	c = ring_read(&rem->vid_buffy, (char *) buf, l);
	rem->vread += c;
	del_vpts(rem);
	del_vframe(rem);
	return c;
}

int aring_read( Remux *rem, uint8_t *buf, int l)
{
	int c = 0;
	int r = 0;

	if (ring_rest(&rem->aud_buffy) <= l)
		r = refill_buffy(rem);
	if (r) return -1;
	
	c = ring_read(&rem->aud_buffy, (char *)buf, l);
	rem->aread += c;
	del_apts(rem);
	del_aframe(rem);
	return c;
}

int vring_peek( Remux *rem, uint8_t *buf, int l, long off)
{
	int c = 0;
	
	if (ring_rest(&rem->vid_buffy) <= l)
		refill_buffy(rem);

	c = ring_peek(&rem->vid_buffy, (char *) buf, l, off);
	return c;
}

int aring_peek( Remux *rem, uint8_t *buf, int l, long off)
{
	int c = 0;

	if (ring_rest(&rem->aud_buffy) <= l)
		refill_buffy(rem);

	c = ring_peek(&rem->aud_buffy, (char *)buf, l, off);
	return c;
}


int get_video_info(Remux *rem)
{
	uint8_t buf[12];
	uint8_t *headr;
	int found = 0;
        int sw;
	long off = 0;
	int form = -1;
	ringbuffy *vid_buffy = &rem->vid_buffy;
	VideoInfo *vi = &rem->video_info;

	while (found < 4 && ring_rest(vid_buffy)){
		uint8_t b[3];

		vring_peek( rem, b, 4, 0);
		if ( b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x01
		     && b[3] == 0xb3) found = 4;
		else {
			off++;
			vring_read( rem, b, 1);
		}
	}
	rem->vframe = rem->vframen-1;

	if (! found) return -1;
	buf[0] = 0x00; buf[1] = 0x00; buf[2] = 0x01; buf[3] = 0xb3;
	headr = buf+4;
	if(vring_peek(rem, buf, 12, 0) < 12) return -1;

	vi->horizontal_size	= ((headr[1] &0xF0) >> 4) | (headr[0] << 4);
	vi->vertical_size	= ((headr[1] &0x0F) << 8) | (headr[2]);
    
        sw = (int)((headr[3]&0xF0) >> 4) ;

        switch( sw ){
	case 1:
		fprintf(stderr,"Videostream: ASPECT: 1:1");
		vi->aspect_ratio = 100;        
		break;
	case 2:
		fprintf(stderr,"Videostream: ASPECT: 4:3");
                vi->aspect_ratio = 133;        
		break;
	case 3:
		fprintf(stderr,"Videostream: ASPECT: 16:9");
                vi->aspect_ratio = 177;        
		break;
	case 4:
		fprintf(stderr,"Videostream: ASPECT: 2.21:1");
                vi->aspect_ratio = 221;        
		break;

        case 5 ... 15:
		fprintf(stderr,"Videostream: ASPECT: reserved");
                vi->aspect_ratio = 0;        
		break;

        default:
                vi->aspect_ratio = 0;        
                return -1;
	}

        fprintf(stderr,"  Size = %dx%d",vi->horizontal_size,vi->vertical_size);

        sw = (int)(headr[3]&0x0F);

        switch ( sw ) {
	case 1:
		fprintf(stderr,"  FRate: 23.976 fps");
                vi->framerate = 24000/1001.;
		form = -1;
		break;
	case 2:
		fprintf(stderr,"  FRate: 24 fps");
                vi->framerate = 24;
		form = -1;
		break;
	case 3:
		fprintf(stderr,"  FRate: 25 fps");
                vi->framerate = 25;
		form = VIDEO_MODE_PAL;
		break;
	case 4:
		fprintf(stderr,"  FRate: 29.97 fps");
                vi->framerate = 30000/1001.;
		form = VIDEO_MODE_NTSC;
		break;
	case 5:
		fprintf(stderr,"  FRate: 30 fps");
                vi->framerate = 30;
		form = VIDEO_MODE_NTSC;
		break;
	case 6:
		fprintf(stderr,"  FRate: 50 fps");
                vi->framerate = 50;
		form = VIDEO_MODE_PAL;
		break;
	case 7:
		fprintf(stderr,"  FRate: 60 fps");
                vi->framerate = 60;
		form = VIDEO_MODE_NTSC;
		break;
	}

	rem->dts_delay = (int)(7.0/vi->framerate/2.0*90000);

	vi->bit_rate = 400*(((headr[4] << 10) & 0x0003FC00UL) 
			    | ((headr[5] << 2) & 0x000003FCUL) | 
			    (((headr[6] & 0xC0) >> 6) & 0x00000003UL));
	
        fprintf(stderr,"  BRate: %.2f Mbit/s",(vi->bit_rate)/1000000.);
	
        fprintf(stderr,"\n");
        vi->video_format = form;

	/*
	marker_bit (&video_bs, 1);
	vi->vbv_buffer_size	= getbits (&video_bs, 10);
	vi->CSPF		= get1bit (&video_bs);
	*/
	return form;
}


int get_audio_info( Remux *rem)
{
	uint8_t *headr;
	uint8_t buf[3];
	long off = 0;
	int found = 0;
	ringbuffy *aud_buffy = &rem->aud_buffy;
	AudioInfo *ai = &rem->audio_info;
	
	while(!ring_rest(aud_buffy) && !refill_buffy(rem));
	while (found < 2 && ring_rest(aud_buffy)){
		uint8_t b[2];
		refill_buffy(rem);
		aring_peek( rem, b, 2, 0);

		if ( b[0] == 0xff && (b[1] & 0xf8) == 0xf8)
			found = 2;
		else {
			off++;
			aring_read( rem, b, 1);
		}
	}	

	if (!found) return -1;
	rem->aframe = rem->aframen-1;
	
	if (aring_peek(rem, buf, 3, 0) < 1) return -1;
	headr = buf+2;

	ai->layer = (buf[1] & 0x06) >> 1;

        fprintf(stderr,"Audiostream: Layer: %d", 4-ai->layer);


	ai->bit_rate = bitrates[(3-ai->layer)][(headr[0] >> 4 )]*1000;

	if (ai->bit_rate == 0)
		fprintf (stderr,"  Bit rate: free");
	else if (ai->bit_rate == 0xf)
		fprintf (stderr,"  BRate: reserved");
	else
		fprintf (stderr,"  BRate: %d kb/s", ai->bit_rate/1000);
	

	ai->frequency = (headr[0] & 0x0c ) >> 2;
	if (ai->frequency == 3)
		fprintf (stderr, "  Freq: reserved\n");
	else
		fprintf (stderr,"  Freq: %2.1f kHz\n", 
			 freq[ai->frequency]/10.);

	return 0;
}



void init_remux(Remux *rem, int fin, int fout, int mult)
{
	rem->video_info.framerate = 0;
	rem->audio_info.layer = -1;
	rem->fin = fin;
	rem->fout = fout;
	ring_init(&rem->vid_buffy, 40*BUFFYSIZE*mult);
	ring_init(&rem->aud_buffy, BUFFYSIZE*mult);
	init_ptsl(rem->vpts_list);
	init_ptsl(rem->apts_list);
	init_framel(rem->vframe_list);
	init_framel(rem->aframe_list);

	rem->vptsn     = 0;
	rem->aptsn     = 0;
	rem->vframen   = 0;
	rem->aframen   = 0;
	rem->vframe    = 0;
	rem->aframe    = 0;
	rem->vcframe   = 0;
	rem->acframe   = 0;
	rem->vpts      = 0;
	rem->vdts      = 0;
	rem->apts_off  = 0;
	rem->vpts_off  = 0;
	rem->apts_delay= 0;
	rem->vpts_delay= 0;
	rem->dts_delay = 0;
	rem->apts      = 0;
	rem->vpes      = 0;
	rem->apes      = 0;
	rem->vpts_old  = 0;
	rem->apts_old  = 0;
	rem->SCR       = 0;
	rem->vwrite    = 0;
	rem->awrite    = 0;
	rem->vread     = 0;
	rem->aread     = 0;
	rem->group     = 0;
	rem->groupframe= 0;
	rem->pack_size = 0;
	rem->muxr      = 0;
	rem->time_off  = 0;
}

uint32_t bytes2pts(int bytes, int rate)
{
	if (bytes < 0xFFFFFFFFUL/720000UL)
		return (uint32_t)(bytes*720000UL/rate);
	else
		return (uint32_t)(bytes/rate*720000UL);
}

long pts2bytes( uint32_t pts, int rate)
{
	if (pts < 0xEFFFFFFFUL/rate)
		return (pts*rate/720000);
	else 
		return (pts* (rate/720000));
}

int write_audio_pes( Remux *rem, uint8_t *buf, int *alength)
{
	int add;
	int pos = 0;
	int p   = 0;
	uint32_t pts = 0;
	int stuff = 0;
	int length = *alength;

	if (!length) return 0;
	p = PS_HEADER_L1+PES_H_MIN;

	if (rem->apts_old != rem->apts){
		pts = (uint32_t)((uint64_t)rem->apts + rem->apts_delay - rem->apts_off);
		p += 5;
	}
	if ( length+p >= rem->pack_size){
		length = rem->pack_size;
	} else {
		if (rem->pack_size-length-p <= PES_MIN){
			stuff = rem->pack_size - length;
			length = rem->pack_size;
		} else 
			length = length+p;
	}
	pos = write_ps_header(buf,rem->SCR,rem->muxr, 1, 0, 0, 1, 1, 1, 
			      0, 0, 0, 0, 0, 0);

	pos += write_pes_header( 0xC0, length-pos, pts, buf+pos, stuff);
	add = aring_read( rem, buf+pos, length-pos);
	*alength = add;
	if (add < 0) return -1;
	pos += add;
	rem->apts_old = rem->apts;
	rem->apts = rem->apts_list[0].PTS;

	if (pos+PES_MIN < rem->pack_size){
		pos += write_pes_header( PADDING_STREAM, rem->pack_size-pos, 0,
					 buf+pos, 0);
		pos = rem->pack_size;
	}		
	if (pos != rem->pack_size) {
		fprintf(stderr,"apos: %d\n",pos);
		exit(1);
	}

	return pos;
}

int write_video_pes( Remux *rem, uint8_t *buf, int *vlength)
{
	int add;
	int pos = 0;
	int p   = 0;
	uint32_t pts = 0;
	uint32_t dts = 0;
	int stuff = 0;
	int length = *vlength;
	long diff = 0;

	if (! length) return 0;
	p = PS_HEADER_L1+PES_H_MIN;

	if (rem->vpts_old != rem->vpts){
		pts = (uint32_t)((uint64_t)rem->vpts + rem->vpts_delay - rem->vpts_off);
		p += 5;
	}
	if ( length+p >= rem->pack_size){
		length = rem->pack_size;
	} else {
		if (rem->pack_size - length - p <= PES_MIN){
			stuff = rem->pack_size - length;
			length = rem->pack_size;
		} else 
			length = length+p;
	}

	pos = write_ps_header(buf,rem->SCR,rem->muxr, 1, 0, 0, 1, 1, 1, 
			      0, 0, 0, 0, 0, 0);

	pos += write_pes_header( 0xE0, length-pos, pts, buf+pos, stuff);
	add = vring_read( rem, buf+pos, length-pos);
	*vlength = add;
	if (add < 0) return -1;
	pos += add;
	rem->vpts_old = rem->vpts;
	dts = rem->vdts;
	rem->vpts = rem->vpts_list[0].PTS;
	rem->vdts = rem->vpts_list[0].dts;
	if ( diff > 0) rem->SCR += diff;
	if (pos+PES_MIN < rem->pack_size){
		//  fprintf(stderr,"vstuffing: %d   \n",rem->pack_size-pos);
		pos += write_pes_header( PADDING_STREAM, rem->pack_size-pos, 0,
					 buf+pos, 0);
		pos = rem->pack_size;
	}		
	return pos;
}

void print_info( Remux *rem , int ret)
{
	int newtime = 0;
	static int time = 0;
	int i = 0;

	while(! newtime && i < rem->vframen) {
		if( (newtime = rem->vframe_list[i].time)) break;
		i++;
	}
	if (newtime) time = newtime;
	
	fprintf(stderr,"SCR:");
	printpts(rem->SCR);
	fprintf(stderr," VDTS:");
	printpts((uint32_t)((uint64_t)rem->vdts - rem->vpts_off + rem->vpts_delay));
	fprintf(stderr," APTS:");
	printpts((uint32_t)((uint64_t)rem->apts - rem->apts_off + rem->apts_delay));
	fprintf(stderr," TIME:%2d:", time/3600);
	fprintf(stderr,"%02d:", (time%3600)/60);
	fprintf(stderr,"%02d", (time%3600)%60);
	if (ret) fprintf(stderr,"\n");
	else fprintf(stderr,"\r");
}

void remux(int fin, int fout, int pack_size, int mult)
{
	Remux rem;
	long ptsdiff;
	uint8_t buf[MAX_PACK_L];
	long pos = 0;
	int r = 0;
	int i, r1, r2;
	long packets = 0;
	uint8_t mpeg_end[4] = { 0x00, 0x00, 0x01, 0xB9 };
	uint32_t SCR_inc = 0;
	int data_size;
	long vbuf, abuf;
	long vbuf_max, abuf_max;
	PTS_List abufl[MAX_PTS];
	PTS_List vbufl[MAX_PTS];
	int abufn = 0;
	int vbufn = 0;
	uint64_t pts_d = 0;
	int ok_audio; 
	int ok_video; 
	uint32_t apos = 0;
	uint32_t vpos = 0;
	int vpack_size = 0;
	int apack_size = 0;

	init_ptsl(abufl);
	init_ptsl(vbufl);

	if (mult < 0 || mult >1000){
		fprintf(stderr,"Multipler too large\n");
		exit(1);
	}
	init_remux(&rem, fin, fout, mult);
	rem.pack_size = pack_size;
	data_size = pack_size - MAX_H_SIZE;
	fprintf(stderr,"pack_size: %d header_size: %d data size: %d\n",
		pack_size, MAX_H_SIZE, data_size);
	refill_buffy(&rem);
	fprintf(stderr,"Package size: %d\n",pack_size);
	
	if ( get_video_info(&rem) < 0 ){
		fprintf(stderr,"ERROR: Can't find valid video stream\n");
		exit(1);
	}

	i = 0;
	while(! rem.time_off && i < rem.vframen) {
		if( (rem.time_off = rem.vframe_list[i].time)) break;
		i++;
	}

	if ( get_audio_info(&rem) < 0 ){
		fprintf(stderr,"ERROR: Can't find valid audio stream\n");
		exit(1);
	}
	
	rem.vpts = rem.vpts_list[0].PTS;
	rem.vdts = rem.vpts;
	rem.vpts_off = rem.vpts;
	fprintf(stderr,"Video start PTS = %fs \n",rem.vpts_off/90000.);
	rem.apts = rem.apts_list[0].PTS;
	rem.apts_off = rem.apts;
	ptsdiff = rem.vpts - rem.apts;
	if (ptsdiff > 0) rem.vpts_off -= ptsdiff;
	else rem.apts_off -= -ptsdiff;
	fprintf(stderr,"Audio start PTS = %fs\n",rem.apts_off/90000.);
	fprintf(stderr,"Difference Video - Audio = %fs\n",ptsdiff/90000.);
	fprintf(stderr,"Time offset = %ds\n",rem.time_off);

	rem.muxr = (rem.video_info.bit_rate + 
		    rem.audio_info.bit_rate)/400;
	fprintf(stderr,"MUXRATE: %.2f Mb/sec\n",rem.muxr/2500.);
	SCR_inc = 1800 * pack_size / rem.muxr;
	
	r = 0;
	while ( rem.vptsn < 2 && !r) r = refill_buffy(&rem);
	r = 0;
	while ( rem.aptsn < 2 && !r) r = refill_buffy(&rem);

	//rem.vpts_delay =  (uint32_t)(2*90000ULL* (uint64_t)pack_size/rem.muxr);
	rem.vpts_delay = rem.dts_delay;
	rem.apts_delay = rem.vpts_delay;

	vbuf_max = 29440;
	abuf_max = 4096;
	vbuf = 0;
	abuf = 0;
	pos = write_ps_header(buf,rem.SCR,rem.muxr, 1, 0, 0, 1, 1, 1, 
			      0xC0, 0, 32, 0xE0, 1, 230);
	pos += write_pes_header( PADDING_STREAM, pack_size-pos, 0, buf+pos,0);
	pos = rem.pack_size;
	write( fout, buf, pos);

	apos = rem.aread;
	vpos = rem.vread;
	print_info( &rem, 1 );

	while( ring_rest(&rem.aud_buffy) && ring_rest(&rem.vid_buffy) ){
		uint32_t next_apts;
		uint32_t next_vdts;
		int asize, vsize;

		r1 = 0;
		r2 = 0;
		while ( rem.aframen < 2 && !r1) 
			r1 = refill_buffy(&rem);
		while ( rem.vframen < 2 && !r2) 
			r2 = refill_buffy(&rem);
		if (r1 && r2) break;

		if ( !r1 && apos <= rem.aread)
			apos = rem.aframe_list[1].pos;
		if ( !r2 && vpos <= rem.vread)
			vpos = rem.vframe_list[1].pos;
		apack_size = apos - rem.aread; 
		vpack_size = vpos - rem.vread; 
		

		next_vdts = (uint32_t)((uint64_t)rem.vdts + rem.vpts_delay 
				  - rem.vpts_off) ;
		ok_video = ( rem.SCR < next_vdts);

		next_apts = (uint32_t)((uint64_t)rem.apts + rem.apts_delay 
				  - rem.apts_off) ;
		ok_audio = ( rem.SCR  < next_apts);

		asize = (apack_size > data_size ? data_size: apack_size);
		vsize = (vpack_size > data_size ? data_size: vpack_size);

		fprintf(stderr,"vframen: %d  aframen: %d  v_ok: %d  a_ok: %d  v_buf: %d  a_buf: %d vpacks: %d  apacks: %d\n",rem.vframen,rem.aframen, ok_video, ok_audio, (int)vbuf,(int)abuf,vsize, asize);
		

		if( vbuf+vsize  < vbuf_max && vsize && ok_audio ){
			fprintf(stderr,"1 ");
			pos = write_video_pes( &rem, buf, &vpack_size);
			write( fout, buf, pos);
			vbuf += vpack_size;
			vbufn = add_pts( vbufl, rem.vdts, vpack_size, 
					 0, vbufn, 0);
			packets++;
		} else if ( abuf+asize < abuf_max && asize &&
			    ok_video  ){
			fprintf(stderr,"2 ");
			pos = write_audio_pes( &rem, buf, &apack_size);
			write( fout, buf, pos);
			abuf += apack_size;
			abufn = add_pts( abufl, rem.apts, apack_size, 
					 0, abufn, 0);
			packets++;
		} else if ( abuf+asize < abuf_max && asize &&
			    !ok_audio){
			fprintf(stderr,"3 ");
			pos = write_audio_pes( &rem, buf, &apack_size);
			write( fout, buf, pos);
			abuf += apack_size;
			abufn = add_pts( abufl, rem.apts, apack_size, 
					 0, abufn, 0);
			packets++;
		} else if (vbuf+vsize  < vbuf_max && vsize &&
			   !ok_video){
			fprintf(stderr,"4 ");
			pos = write_video_pes( &rem, buf, &vpack_size);
			write( fout, buf, pos);
			vbuf += vpack_size;
			vbufn = add_pts( vbufl, rem.vdts, vpack_size, 
					 0, vbufn, 0);
			packets++;
		} else {
		fprintf(stderr,"5 ");
			pos = write_ps_header(buf,rem.SCR,rem.muxr, 1, 0, 0, 
					      1, 1, 1, 0, 0, 0, 0, 0, 0);

			pos += write_pes_header( PADDING_STREAM, pack_size-pos,
						 0, buf+pos, 0);
			write( fout, buf, pos);
		}


		//fprintf(stderr,"vbufn: %d  abufn: %d  ", vbufn,abufn);
		//fprintf(stderr,"vbuf: %5d  abuf: %4d\n", vbuf,abuf);

		if (rem.SCR > rem.vdts+rem.vpts_off -rem.vpts_delay) 
			rem.SCR = rem.vdts-rem.vpts_off;
		rem.SCR = (uint32_t)((uint64_t) rem.SCR + SCR_inc);

		if ( rem.apts_off + rem.SCR < rem.apts_delay ) pts_d = 0;
		else pts_d = (uint64_t) rem.SCR + rem.apts_off - rem.apts_delay;
		abuf -= del_ptss( abufl, (uint32_t) pts_d, &abufn);

		if ( rem.vpts_off + rem.SCR < rem.vpts_delay ) pts_d = 0;
		else pts_d = (uint64_t) rem.SCR + rem.vpts_off - rem.vpts_delay;
		vbuf -= del_ptss( vbufl, (uint32_t) pts_d, &vbufn);

		print_info( &rem, 1);
		//fprintf(stderr,"vbufn: %d  abufn: %d  ", vbufn,abufn);
		//fprintf(stderr,"vbuf: %5d  abuf: %4d\n\n", vbuf,abuf);


	}
	pos = write_ps_header(buf,rem.SCR,rem.muxr, 1, 0, 0, 1, 1, 1, 
			      0, 0, 0, 0, 0, 0);

	pos += write_pes_header( PADDING_STREAM, pack_size-pos-4, 0, 
				 buf+pos, 0);
	pos = rem.pack_size-4;
	write( fout, buf, pos);

	write( fout, mpeg_end, 4);
	fprintf(stderr,"\ndone\n");
}


typedef 
struct pes_buffer_s{
	ringbuffy   pes_buffy;
	uint8_t     type;
	PTS_List    pts_list[MAX_PTS];
	FRAME_List  frame_list[MAX_FRAME];
	int         pes_size;
	uint64_t    written;
	uint64_t    read;
} PESBuffer;


void init_PESBuffer(PESBuffer *pbuf, int pes_size, int buf_size, uint8_t type)
{
	init_framel( pbuf->frame_list);
	init_ptsl( pbuf->pts_list);
	ring_init( &pbuf->pes_buffy, buf_size);
	pbuf->pes_size = pes_size;
	pbuf->type = type; 
	pbuf->written = 0;
	pbuf->read = 0;
}
	

#define MAX_PBUF 4

typedef
struct remux_s{
	PESBuffer pbuf_list[MAX_PBUF];
	int num_pbuf;
} REMUX;


void init_REMUX(REMUX *rem)
{
	rem->num_pbuf = 0;
}



#define REPACK      2048 
#define ABUF_SIZE   REPACK*1024
#define VBUF_SIZE   REPACK*10240

void remux_main(uint8_t *buf, int count, void *pr)
{
	int i, b;
	int bufsize = 0;
	p2p *p = (p2p *) pr;
	PESBuffer *pbuf;
	REMUX *rem = (REMUX *) p->data;
	uint8_t type = buf[3];
	int *npbuf = &(rem->num_pbuf);

	switch ( type ){
	case PRIVATE_STREAM1:
		bufsize = ABUF_SIZE;
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		if (!bufsize) bufsize = VBUF_SIZE;
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:
		if (!bufsize) bufsize = ABUF_SIZE;
		b = -1;
		for ( i = 0; i < *npbuf; i++){
			if ( type == rem->pbuf_list[i].type ){
				b = i;
				break;
			}
		}
		if (b < 0){
			if ( *npbuf < MAX_PBUF ){
				init_PESBuffer(&rem->pbuf_list[*npbuf], 
					       p->repack+6, bufsize, type);
				b = *npbuf;
				(*npbuf)++;
			} else {
				fprintf(stderr,"Not enough PES buffers\n");
				exit(1);
			}
		}
		break;
	default:
		return;
	}
	
	pbuf = &(rem->pbuf_list[b]);
	if (ring_write(&(pbuf->pes_buffy),(char *)buf,count) != count){
		fprintf(stderr,"buffer overflow type 0x%2x\n",type);
		exit(1);
	} else {
		pbuf->written += count;
		if ((p->flag2 & PTS_DTS_FLAGS)){
			uint32_t PTS = trans_pts_dts(p->pts);
			add_pts(pbuf->pts_list, PTS, pbuf->written, 
				pbuf->written, 0, 0);
		}
		p->flag2 = 0;
	}

}

void output_mux(p2p *p) 
{
	int i, filling;
	PESBuffer *pbuf;
	ringbuffy   *pes_buffy;	
	REMUX *rem = (REMUX *) p->data;
	int repack = p->repack;
	int npbuf = rem->num_pbuf;

	for ( i = 0; i < npbuf; i++){
		pbuf = &(rem->pbuf_list[i]);
		pes_buffy = &pbuf->pes_buffy;
		filling = pes_buffy->size - ring_rest(pes_buffy);
		if (filling/(2 *repack)){
			pbuf->read += ring_read_file(pes_buffy, p->fd1, 
						     (filling/repack)*repack);
		}
	}
}



#define SIZE 32768

void remux2(int fdin, int fdout)
{
	p2p p;
	int count = 1;
	uint8_t buf[SIZE];
	uint64_t length = 0;
	uint64_t l = 0;
	int verb = 0;
	REMUX rem;
	
	init_p2p(&p, remux_main, REPACK);
	p.fd1 = fdout;
	p.data = (void *) &rem;
	

	if (fdin != STDIN_FILENO) verb = 1; 

	if (verb) {
		length = lseek(fdin, 0, SEEK_END);
		lseek(fdin,0,SEEK_SET);
	}

	while (count > 0){
		count = read(fdin,buf,SIZE);
		l += count;
		if (verb)
			fprintf(stderr,"Writing  %2.2f %%\r",
				100.*l/length);

		get_pes(buf,count,&p,pes_repack);
		output_mux(&p);
	}
		
}
