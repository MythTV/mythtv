/*
 * multiplex.c: multiplex functions for replex
 *        
 *
 * Copyright (C) 2003 - 2006
 *                    Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
 *           (C) 2006 Reel Multimedia
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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "multiplex.h"


static int mplx_write(multiplex_t *mx, uint8_t *buffer,int length)
{
	int k=0;

	if ( mx->max_write && mx->total_written+ length >  
	     mx-> max_write && !mx->max_reached){
		mx->max_reached = 1;
		fprintf(stderr,"Maximum file size %dKB reached\n", mx->max_write/1024);
		return 0;
	}
	if ((k=write(mx->fd_out, buffer, length)) <= 0){
		mx->zero_write_count++;
	} else {
		mx->total_written += k;
	}
	return k;
}

static int buffers_filled(multiplex_t *mx)
{
	int vavail=0, aavail=0, i;

	vavail = ring_avail(mx->index_vrbuffer)/sizeof(index_unit);
	
//	for (i=0; i<mx->extcnt;i++){
//		aavail += ring_avail(&mx->index_extrbuffer[i])/
//				     sizeof(index_unit);
//	}
	for (i=0; i<mx->apidn;i++){
		aavail += ring_avail(&mx->index_arbuffer[i])/sizeof(index_unit);
 	}
 
	for (i=0; i<mx->ac3n;i++){
		aavail += ring_avail(&mx->index_ac3rbuffer[i])
			/sizeof(index_unit);
	}

	if (aavail+vavail) return ((aavail+vavail));
	return 0;
}

static int all_audio_ok(int *aok, int n)
{
       int ok=0,i;
 
       if (!n) return 0;
       for (i=0;  i < n ;i++){
               if (aok[i]) ok++;
       }
       if (ok == n) return 1;
       return 0;
}
static int rest_audio_ok(int j, int *aok, int n)
{
       int ok=0,i;
 
       if (!(n-1)) return 0;
       for (i=0;  i < n ;i++){
               if (i!=j && aok[i]) ok++;
       }
       if (ok == n) return 1;
       return 0;
}

/*
static int use_video(uint64_t vpts, extdata_t *ext, int *aok, int n)
{
	int i;
	for(i=0; i < n; i++)
		if(aok[i] && ptscmp(vpts,ext[i].pts) > 0)
			return 0;
	return 1;
}
*/
/*
static int which_ext(extdata_t *ext, int *aok, int n)
{
	int i;
	int started = 0;
	int pos = -1;
	uint64_t tmppts = 0;
	for(i=0; i < n; i++)
		if(aok[i]){
			if(! started){
				started=1;
				tmppts=ext[i].pts;
				pos = i;
			} else if(ptscmp(tmppts, ext[i].pts) > 0) {
				tmppts = ext[i].pts;
				pos = i;
			}
		}
	return pos;
}
*/
/*
static int peek_next_video_unit(multiplex_t *mx, index_unit *viu)
{
	if (!ring_avail(mx->index_vrbuffer) && mx->finish) return 0;

	while (ring_avail(mx->index_vrbuffer) < sizeof(index_unit))
		if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
			fprintf(stderr,"error in peek next video unit\n");
			return 0;
		}

	ring_peek(mx->index_vrbuffer, (uint8_t *)viu, sizeof(index_unit),0);
*/
static int get_next_video_unit(multiplex_t *mx, index_unit *viu)
{
       if (!ring_avail(mx->index_vrbuffer) && mx->finish) return 0;
 
       while (ring_avail(mx->index_vrbuffer) < sizeof(index_unit))
               if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
                       fprintf(stderr,"error in get next video unit\n");
                       return 0;
               }
 
       ring_read(mx->index_vrbuffer, (uint8_t *)viu, sizeof(index_unit));

#ifdef OUT_DEBUG
	fprintf(stderr,"video index start: %d  stop: %d  (%d)  rpos: %d\n", 
		viu->start, (viu->start+viu->length),
		viu->length, ring_rpos(mx->vrbuffer));
#endif

	return 1;
}

static int peek_next_video_unit(multiplex_t *mx, index_unit *viu)
{
	if (!ring_avail(mx->index_vrbuffer) && mx->finish) return 0;

	while (ring_avail(mx->index_vrbuffer) < sizeof(index_unit))
		if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
			fprintf(stderr,"error in peek next video unit\n");
			return 0;
		}

	ring_peek(mx->index_vrbuffer, (uint8_t *)viu, sizeof(index_unit),0);
#ifdef OUT_DEBUG
	fprintf(stderr,"video index start: %d  stop: %d  (%d)  rpos: %d\n", 
		viu->start, (viu->start+viu->length),
		viu->length, ring_rpos(mx->vrbuffer));
#endif

	return 1;
}
	
static int get_next_audio_unit(multiplex_t *mx, index_unit *aiu, int i)
{
	if (!ring_avail(&mx->index_arbuffer[i]) && mx->finish) return 0;

	while(ring_avail(&mx->index_arbuffer[i]) < sizeof(index_unit))
		if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
			fprintf(stderr,"error in get next audio unit\n");
			return 0;
		}
	
	ring_read(&mx->index_arbuffer[i], (uint8_t *)aiu, sizeof(index_unit));

#ifdef OUT_DEBUG
	fprintf(stderr,"audio index start: %d  stop: %d  (%d)  rpos: %d\n", 
		aiu->start, (aiu->start+aiu->length),
		aiu->length, ring_rpos(&mx->arbuffer[i]));
#endif
	return 1;
}

static int get_next_ac3_unit(multiplex_t *mx, index_unit *aiu, int i)
{
	if (!ring_avail(&mx->index_ac3rbuffer[i]) && mx->finish) return 0;
	while(ring_avail(&mx->index_ac3rbuffer[i]) < sizeof(index_unit))
		if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
			fprintf(stderr,"error in get next ac3 unit\n");
			return 0;
		}
	
	ring_read(&mx->index_ac3rbuffer[i], (uint8_t *)aiu, sizeof(index_unit));
	return 1;
}

static void writeout_video(multiplex_t *mx)
{  
	uint8_t outbuf[3000];
	int written=0;
	uint8_t ptsdts=0;
	int length;
	int nlength=0;
	index_unit *viu = &mx->viu;

#ifdef OUT_DEBUG
	fprintf(stderr,"writing VIDEO pack");
#endif
	
	if (viu->frame_start && viu->seq_header && viu->gop && 
	    viu->frame == I_FRAME){
		if (!mx->startup && mx->navpack){
			write_nav_pack(mx->pack_size, mx->apidn, mx->ac3n, 
				       mx->SCR, mx->muxr, outbuf);
			mplx_write(mx, outbuf, mx->pack_size);
			ptsinc(&mx->SCR, mx->SCRinc);
		} else mx->startup = 0;
#ifdef OUT_DEBUG
		fprintf (stderr, " with sequence and gop header\n");
#endif
	}

	if (mx->finish != 2 && dummy_space(&mx->vdbuf) < mx->data_size){
		return;
	}

	length = viu->length;
	if  (length  < mx->data_size )
		dummy_add(&mx->vdbuf, uptsdiff(viu->dts+mx->video_delay,0)
			  , length);

	while (length  < mx->data_size ){
		index_unit nviu;
		if ( peek_next_video_unit(mx, &nviu)){
			if (!(nviu.seq_header && nviu.gop && 
			      nviu.frame == I_FRAME)){
				get_next_video_unit(mx, viu);
				length += viu->length; 
				if  (length  < mx->data_size )
					dummy_add(&mx->vdbuf, 
						  uptsdiff(viu->dts+
							   mx->video_delay,0)
						  , viu->length);
			} else break;
		} else break;
	}

	if (viu->frame_start){
		switch (mx->frame_timestamps){
		case TIME_ALWAYS:
			if (viu->frame == I_FRAME || viu->frame == P_FRAME)
				ptsdts = PTS_DTS;
			else 
				ptsdts = PTS_ONLY;
			break;

		case TIME_IFRAME:
			if (viu->frame == I_FRAME)
				ptsdts = PTS_DTS;
			break;
		}
		viu->frame_start=0;
		if (mx->VBR) {
			mx->extra_clock = ptsdiff(viu->dts + mx->video_delay, 
						  mx->SCR + 500*CLOCK_MS);
#ifdef OUT_DEBUG
			fprintf(stderr,"EXTRACLOCK2: %lli %lli %lli\n", viu->dts, mx->video_delay, mx->SCR);
			fprintf(stderr,"EXTRACLOCK2: %lli ", mx->extra_clock);
			printpts(mx->extra_clock);
			fprintf(stderr,"\n");
#endif

			if (mx->extra_clock < 0)
				mx->extra_clock = 0.0;
		}
	}


       nlength = length;

       written = write_video_pes( mx->pack_size, mx->apidn, mx->ac3n, 
                                  viu->pts+mx->video_delay, 
                                  viu->dts+mx->video_delay, 
                                  mx->SCR, mx->muxr, outbuf, &nlength, ptsdts,
                                  mx->vrbuffer);
  
      length -= nlength;
      dummy_add(&mx->vdbuf, uptsdiff( viu->dts+mx->video_delay,0)
                 , viu->length-length);
      viu->length = length;
      if (viu->gop){
              pts2time( viu->pts + mx->video_delay, outbuf, written);
      }
  
      mplx_write(mx, outbuf, written);
      
      if (viu->length == 0){
              get_next_video_unit(mx, viu);
      }
  
}
 
#define INSIZE 6000
int add_to_inbuf(uint8_t *inbuf,ringbuffer *arbuffer, int inbc, int off, int length)
{  
       int add;
       
       if (inbc + length > INSIZE) {
               fprintf(stderr,"buffer too small in write_out_audio %d %d\n",inbc,length);
               return 0;
       }
       add= ring_peek( arbuffer, inbuf+inbc, length, off);
       if (add < length) {
               fprintf(stderr,"error while peeking audio ring %d (%d)\n", add,length);
               return 0;
       }
       return add;
}
 
 
 static void clear_audio(multiplex_t *mx, int type, int n)
{
       index_unit *aiu;
       int g=0;
       ringbuffer *arbuffer;
 
       switch (type){
               
       case MPEG_AUDIO:
#ifdef OUT_DEBUG
               fprintf(stderr,"clear AUDIO%d pack\n",n);
#endif
               arbuffer = &mx->arbuffer[n];
               aiu = &mx->aiu[n];
               break;
 
       case AC3:
#ifdef OUT_DEBUG
               fprintf(stderr,"clear AC3%d pack\n",n);
#endif
               arbuffer = &mx->ac3rbuffer[n];
               aiu = &mx->ac3iu[n];
               break;
 
       default:
               return;
       }
       while (aiu->err == JUMP_ERR){
//            fprintf(stderr,"FOUND ONE\n");
               ring_skip(arbuffer, aiu->length);               
               if (type == MPEG_AUDIO)
                       g = get_next_audio_unit(mx, aiu, n);
               else
                       g = get_next_ac3_unit(mx, aiu, n);              
      }

}
static void my_memcpy(uint8_t *target, int offset, uint8_t *source, int length, int maxlength)
{
       uint8_t *targ = NULL;
 
       targ = target + offset;
       
       if ( offset+length > maxlength){
               fprintf(stderr,"WARNING: buffer overflow in my_memcopy \n");
//            fprintf(stderr, "source 0x%x  offset %d target 0x%x  length %d maxlength %d\n"
//                    , source, offset, target, length, maxlength);
       }
       memcpy(targ, source, length);
}
 
static void writeout_audio(multiplex_t *mx, int type, int n)
{  
       uint8_t outbuf[3000];
       uint8_t inbuf[INSIZE];
       int inbc=0;
       int written=0;
       int length=0;
       dummy_buffer *dbuf;
       ringbuffer *airbuffer;
       ringbuffer *arbuffer;
       int nlength=0;
       uint64_t pts, dpts=0;
       uint64_t adelay;
       int aframesize;
       int newpts=0;
       int nframes=1;
       int ac3_off=0;
       int rest_data = 5;
       uint64_t *apts;
       index_unit *aiu;
       int g=0;
       int add, off=0;
       int fakelength = 0;
       int droplength = 0;
  
       switch (type){
  
       case MPEG_AUDIO:
#ifdef OUT_DEBUG
               fprintf(stderr,"writing AUDIO%d pack\n",n);
#endif
               airbuffer = &mx->index_arbuffer[n];
               arbuffer = &mx->arbuffer[n];
               dbuf = &mx->adbuf[n];
               adelay = mx->apts_off[n];
               aframesize = mx->aframes[n];    
               apts = &mx->apts[n];
               aiu = &mx->aiu[n];
               break;
  
       case AC3:
#ifdef OUT_DEBUG
               fprintf(stderr,"writing AC3%d pack\n",n);
#endif
               airbuffer = &mx->index_ac3rbuffer[n];
               arbuffer = &mx->ac3rbuffer[n];
               dbuf = &mx->ac3dbuf[n];
               adelay = mx->ac3pts_off[n];
               aframesize = mx->ac3frames[n];  
               rest_data = 1; // 4 bytes AC3 header
               apts = &mx->ac3pts[n];
               aiu = &mx->ac3iu[n];
               break;
 
       default:

/*
	if (mx->is_ts)
		written = write_video_ts(  viu->pts+mx->video_delay, 
					   viu->dts+mx->video_delay, 
					   mx->SCR, outbuf, &nlength,
					   ptsdts, mx->vrbuffer);
	else
		written = write_video_pes( mx->pack_size, mx->extcnt, 
					   viu->pts+mx->video_delay, 
					   viu->dts+mx->video_delay, 
					   mx->SCR, mx->muxr, outbuf, &nlength,
					   ptsdts, mx->vrbuffer);

	// something bad happened with the PES or TS write, bail
	if (written == -1)
		return;

	length -= nlength;
	dummy_add(&mx->vdbuf, uptsdiff( viu->dts+mx->video_delay,0)
		  , viu->length-length);
	viu->length = length;

	//estimate next pts based on bitrate of this stream and data written
	viu->dts = uptsdiff(viu->dts + ((nlength*viu->ptsrate)>>8), 0);

	write(mx->fd_out, outbuf, written);

#ifdef OUT_DEBUG
	fprintf(stderr,"VPTS");
	printpts(viu->pts);
	fprintf(stderr," DTS");
	printpts(viu->dts);
	printpts(mx->video_delay);
	fprintf(stderr,"\n");
#endif
	
	if (viu->length == 0){
		get_next_video_unit(mx, viu);
	}

}

static void writeout_ext(multiplex_t *mx, int n)
{  
	uint8_t outbuf[3000];
	int written=0;
	unsigned int length=0;
	int nlength=0;
	uint64_t pts, dpts=0;
	int newpts=0;
	int nframes=1;
	int ac3_off=0;
	int rest_data = 5;

	int type = mx->ext[n].type;
	ringbuffer *airbuffer = &mx->index_extrbuffer[n];
	dummy_buffer *dbuf = &mx->ext[n].dbuf;
	uint64_t adelay = mx->ext[n].pts_off;
	uint64_t *apts = &mx->ext[n].pts;
	index_unit *aiu = &mx->ext[n].iu;

	switch (type){

	case MPEG_AUDIO:
#ifdef OUT_DEBUG
		fprintf(stderr,"writing AUDIO%d pack\n",n);
#endif
		break;

	case AC3:
#ifdef OUT_DEBUG
		fprintf(stderr,"writing AC3%d pack\n",n);
#endif
		rest_data = 1; // 4 bytes AC3 header
		break;

	default:
*/
		return;
	}
	
	if (mx->finish != 2 && dummy_space(dbuf) < mx->data_size + rest_data){
		return;
	}

	pts = uptsdiff( aiu->pts + mx->audio_delay, adelay );
	*apts = pts;
	length = aiu->length;

	if (length <  aiu->framesize){
		newpts = 1;
		ac3_off = length;
		nframes = 0;
	} 

	switch (aiu->err){
		
	case NO_ERR:
		add = add_to_inbuf(inbuf, arbuffer, inbc, off, aiu->length);
		off += add;
		inbc += add;
		break;
	case PTS_ERR:
		break;
	case FRAME_ERR:
	case JUMP_ERR: 
		break;
	case DUMMY_ERR:
	  if (aiu->fillframe){
//		  fprintf(stderr,"1. memcopy 0x%x\n",aiu->fillframe);
			my_memcpy(inbuf, inbc
			       , aiu->fillframe + aframesize - length
				  , length, INSIZE);
			inbc += length;
			fakelength += length;
		} else fprintf(stderr,"no fillframe \n");
		
		break;
	}

	dummy_add(dbuf, pts, aiu->length);

#ifdef OUT_DEBUG
	fprintf(stderr,"start: %d  stop: %d (%d)  length %d ", 
		aiu->start, (aiu->start+aiu->length),
		aiu->length, length);
	printpts(*apts);
	printpts(aiu->pts);
	printpts(mx->audio_delay);
	printpts(adelay);
	printpts(pts);
	fprintf(stderr,"\n");
#endif
	while (length  < mx->data_size + rest_data){
		if (ring_read(airbuffer, (uint8_t *)aiu, sizeof(index_unit)) > 0){
			
			dpts = uptsdiff(aiu->pts +mx->audio_delay, adelay );
			
			if (newpts){
				pts = dpts;
				newpts=0;
			}
			
			
			switch(aiu->err)
			{
			case NO_ERR:
				length += aiu->length;
				add = add_to_inbuf(inbuf, arbuffer, inbc, off, aiu->length);
				inbc += add;
				off += add;
				nframes++;
				break;

			case PTS_ERR:
				break;
			case FRAME_ERR:
			case JUMP_ERR:
				droplength += aiu->length;
 				off += aiu->length;
				break;
			case DUMMY_ERR:
				length += aframesize;
				if (aiu->fillframe){
//					fprintf(stderr,"2. memcopy 0x%x\n",aiu->fillframe);
					my_memcpy(inbuf, inbc, aiu->fillframe, aframesize, INSIZE);
					inbc += aframesize;
					fakelength += aframesize;
					nframes++;
				} else fprintf(stderr,"no fillframe \n");
				
				break;
			} 

			if (length < mx->data_size + rest_data)
				dummy_add(dbuf, dpts, aiu->length);
			
			*apts = dpts;


#ifdef OUT_DEBUG
			fprintf(stderr,"start: %d  stop: %d (%d)  length %d ", 
				aiu->start, (aiu->start+aiu->length),
				aiu->length, length);
			printpts(*apts);
			printpts(aiu->pts);
			printpts(mx->audio_delay);
			printpts(adelay);
			fprintf(stderr,"\n");
#endif
		} else if (mx->finish){
			break;
		} else if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
			fprintf(stderr,"error in writeout audio\n");
			exit(1);
		}
	}

	nlength = length;

/*
       if (type == MPEG_AUDIO)
               written = write_audio_pes( mx->pack_size, mx->apidn, mx->ac3n
                                          , n, pts, mx->SCR, mx->muxr, 
                                          outbuf, &nlength, PTS_ONLY,
                                          arbuffer);
       else 
               written = write_ac3_pes( mx->pack_size, mx->apidn, mx->ac3n
                                        , n, pts, mx->SCR, mx->muxr, 
                                        outbuf, &nlength, PTS_ONLY,
                                        nframes, ac3_off,
                                        arbuffer, aiu->length);
*/
 
       if (type == MPEG_AUDIO)
               written = bwrite_audio_pes( mx->pack_size, mx->apidn, mx->ac3n
                                          , n, pts, mx->SCR, mx->muxr, 
                                          outbuf, &nlength, PTS_ONLY,
                                          inbuf, inbc);
       else 
               written = bwrite_ac3_pes( mx->pack_size, mx->apidn, mx->ac3n
                                        , n, pts, mx->SCR, mx->muxr, 
                                        outbuf, &nlength, PTS_ONLY,
                                        nframes, ac3_off,
                                        inbuf, inbc , aiu->length);
       
       if (aiu->err == DUMMY_ERR){
               fakelength -= length-nlength;
       }
       length -= nlength;
       mplx_write(mx, outbuf, written);
       if (nlength-fakelength+droplength){
               ring_skip(arbuffer, nlength-fakelength+droplength);
       }

       dummy_add(dbuf, dpts, aiu->length-length);
       aiu->length = length;
       aiu->start = ring_rpos(arbuffer);
 
       if (aiu->length == 0){
               if (type == MPEG_AUDIO)
                       g = get_next_audio_unit(mx, aiu, n);
               else
                       g = get_next_ac3_unit(mx, aiu, n);
       }
 
 
       *apts = uptsdiff(aiu->pts + mx->audio_delay, adelay);

/*
	switch (type) {
	case MPEG_AUDIO:
		if(mx->is_ts)
			written = write_audio_ts( mx->ext[n].strmnum, pts,
					outbuf, &nlength, newpts ? 0 : PTS_ONLY,
					&mx->extrbuffer[n]);
		else
			written = write_audio_pes( mx->pack_size, mx->extcnt,
					mx->ext[n].strmnum, pts, mx->SCR,
					mx->muxr, outbuf, &nlength, PTS_ONLY,
					&mx->extrbuffer[n]);
		break;
	case AC3:
		if(mx->is_ts)
			written = write_ac3_ts(mx->ext[n].strmnum, pts,
					outbuf, &nlength, newpts ? 0 : PTS_ONLY,
					mx->ext[n].frmperpkt, &mx->extrbuffer[n]);
		else
			written = write_ac3_pes( mx->pack_size, mx->extcnt,
					mx->ext[n].strmnum, pts, mx->SCR,
					mx->muxr, outbuf, &nlength, PTS_ONLY,
					nframes, ac3_off,
					&mx->extrbuffer[n]);
		break;
	}

	// something bad happened when writing TS or PES to the MPEG or AC3
	// audio stream
	if (written == -1)
		return;

	length -= nlength;
	write(mx->fd_out, outbuf, written);

	dummy_add(dbuf, dpts, aiu->length-length);
	aiu->length = length;
	aiu->start = ring_rpos(&mx->extrbuffer[n]);

	if (aiu->length == 0){
		get_next_ext_unit(mx, aiu, n);
	} else {
		//estimate next pts based on bitrate of stream and data written
		aiu->pts = uptsdiff(aiu->pts + ((nlength*aiu->ptsrate)>>8), 0);
	}
	*apts = uptsdiff(aiu->pts + mx->audio_delay, adelay);
*/
#ifdef OUT_DEBUG
	//if ((int64_t)*apts < 0) fprintf(stderr,"SCHEISS ");
	if ((int64_t)*apts < 0) fprintf(stderr,"MIST ");
	fprintf(stderr,"APTS");
	printpts(*apts);
	printpts(aiu->pts);
	printpts(mx->audio_delay);
	printpts(adelay);
	fprintf(stderr,"\n");
#endif
	int lc=0;
	while (aiu->err == JUMP_ERR && lc < 100){
		lc++;
		
		ring_skip(arbuffer, aiu->length);		
		if (type == MPEG_AUDIO)
			g = get_next_audio_unit(mx, aiu, n);
		else
			g = get_next_ac3_unit(mx, aiu, n);		
	}

	if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
		fprintf(stderr,"error in writeout audio\n");
		exit(1);
	}
}

static void writeout_padding (multiplex_t *mx)
{
	uint8_t outbuf[3000];
	//fprintf(stderr,"writing PADDING pack\n");

	write_padding_pes( mx->pack_size, mx->apidn, mx->ac3n, mx->SCR, 
			   mx->muxr, outbuf);
	mplx_write(mx, outbuf, mx->pack_size);
}

void check_times( multiplex_t *mx, int *video_ok, int *audio_ok, int *ac3_ok,
		  int *start)
{
	int i;
	
	memset(audio_ok, 0, N_AUDIO*sizeof(int));
	memset(ac3_ok, 0, N_AC3*sizeof(int));
	*video_ok = 0;
	
	if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
		fprintf(stderr,"error in get next video unit\n");
		return;
	}

	/* increase SCR to next packet */
	mx->oldSCR = mx->SCR;
	if (!*start){ 
		ptsinc(&mx->SCR, mx->SCRinc);
	} else *start = 0;
	
	if (mx->VBR) {
#ifdef OUT_DEBUG
		fprintf(stderr,"EXTRACLOCK: %lli ", mx->extra_clock);
		printpts(mx->extra_clock);
		fprintf(stderr,"\n");
#endif
		
		if (mx->extra_clock > 0.0) {
			int64_t d = mx->extra_clock/ mx->SCRinc - 1;
			if (d > 0)
				mx->extra_clock = d*mx->SCRinc;
			else
				mx->extra_clock = 0.0;
		}
		
		if (mx->extra_clock > 0.0) {
			int64_t temp_scr = mx->extra_clock;
			
			for (i=0; i<mx->apidn; i++){
				if (ptscmp(mx->SCR + temp_scr + 100*CLOCK_MS, 
					   mx->aiu[i].pts) > 0) {
					while (ptscmp(mx->SCR + temp_scr 
						      + 100*CLOCK_MS,
						      mx->aiu[i].pts) > 0) 
						temp_scr -= mx->SCRinc;
					temp_scr += mx->SCRinc;
				}
			}
			
			for (i=0; i<mx->ac3n; i++){
				if (ptscmp(mx->SCR + temp_scr + 100*CLOCK_MS, 
					   mx->ac3iu[i].pts) > 0) {
					while (ptscmp(mx->SCR + temp_scr
						      + 100*CLOCK_MS,
						      mx->ac3iu[i].pts) > 0) 
						temp_scr -= mx->SCRinc;
					temp_scr += mx->SCRinc;
				}
			}
			
			if (temp_scr > 0.0) {
				mx->SCR += temp_scr;
				mx->extra_clock -= temp_scr;
			} else
				mx->extra_clock = 0.0;
		}
	}


	/* clear decoder buffers up to SCR */
	dummy_delete(&mx->vdbuf, mx->SCR);    
	
	for (i=0;i <mx->apidn; i++){
		dummy_delete(&mx->adbuf[i], mx->SCR);
		clear_audio(mx, MPEG_AUDIO, i);
	}
	for (i=0;i <mx->ac3n; i++) {
		dummy_delete(&mx->ac3dbuf[i], mx->SCR);
		clear_audio(mx, AC3, i);
	}
	
	
	if (dummy_space(&mx->vdbuf) > mx->vsize && mx->viu.length > 0 &&
	    (ptscmp(mx->viu.dts + mx->video_delay, 1000*CLOCK_MS +mx->oldSCR)<0)
	    && ring_avail(mx->index_vrbuffer)){
		*video_ok = 1;
	}
	
	for (i = 0; i < mx->apidn; i++){
	        //fprintf(stdout, "0 check_times AC3 ptscmp: %d apidn: %d ac3n: %d \n",
		//		ptscmp(mx->ac3pts[i], 200*CLOCK_MS + mx->oldSCR), mx->apidn, mx->ac3n);
		if (dummy_space(&mx->adbuf[i]) > mx->asize && 
		    mx->aiu[i].length > 0 &&
		    ptscmp(mx->apts[i], 200*CLOCK_MS + mx->oldSCR) < 0
		    && ring_avail(&mx->index_arbuffer[i])){
			audio_ok[i] = 1;
		}
	}
	for (i = 0; i < mx->ac3n; i++){
	        //fprintf(stdout, "1 check_times AC3 ptscmp: %d \n", ptscmp(mx->ac3pts[i], 200*CLOCK_MS + mx->oldSCR));
		if (dummy_space(&mx->ac3dbuf[i]) > mx->asize && 
		    mx->ac3iu[i].length > 0 &&
		    ptscmp(mx->ac3pts[i], 200*CLOCK_MS + mx->oldSCR) < 0
		    && ring_avail(&mx->index_ac3rbuffer[i])){
			ac3_ok[i] = 1;
		}
		//else
	        //    fprintf(stdout, "2 check_times AC3 ptscmp: %d \n", ptscmp(mx->ac3pts[i], 200*CLOCK_MS + mx->oldSCR));
	}
}

void write_out_packs( multiplex_t *mx, int video_ok, 
		      int *audio_ok, int *ac3_ok)
{
	int i;

	if (video_ok && /* !all_audio_ok(audio_ok, mx->apidn) &&  */
	    !all_audio_ok(ac3_ok, mx->ac3n)) {
		writeout_video(mx);  
	} else { // second case(s): audio ok, video in time
		int done=0;
		for ( i = 0; i < mx->ac3n; i++){
			if ( ac3_ok[i] && !rest_audio_ok(i,ac3_ok, mx->ac3n)
			     && !all_audio_ok(audio_ok, mx->apidn)){

				writeout_audio(mx, AC3, i);
				done = 1;
				break;
			}
		}

		for ( i = 0; i < mx->apidn && !done; i++){
			if ( audio_ok[i] && !rest_audio_ok(i, audio_ok, 
							   mx->apidn)){
				writeout_audio(mx, MPEG_AUDIO, i);
				done = 1;
				break;
			}
		}


		if (!done && !mx->VBR){
			writeout_padding(mx);
		}
	}
	
}

void finish_mpg(multiplex_t *mx)
{
	int start=0;
	int video_ok = 0;
	int audio_ok[N_AUDIO];
	int ac3_ok[N_AC3];
        int n,nn,old,i;
        uint8_t mpeg_end[4] = { 0x00, 0x00, 0x01, 0xB9 };
                                                                                
        memset(audio_ok, 0, N_AUDIO*sizeof(int));
        memset(ac3_ok, 0, N_AC3*sizeof(int));
        mx->finish = 1;
                                                                                
        old = 0;nn=0;
        while ((n=buffers_filled(mx)) && nn<20 ){
                if (n== old) nn++;
                else if (nn) nn--;
                old = n;
                check_times( mx, &video_ok, audio_ok, ac3_ok, &start);
                write_out_packs( mx, video_ok, audio_ok, ac3_ok);
        }

        old = 0;nn=0;
	while ((n=ring_avail(mx->index_vrbuffer)/sizeof(index_unit))
	       && nn<10){
		if (n== old) nn++;
		else if (nn) nn--;
		old = n;
		writeout_video(mx);  
	}
	
// flush the rest
        mx->finish = 2;
        old = 0;nn=0;
	for (i = 0; i < mx->apidn; i++){
		while ((n=ring_avail(&mx->index_arbuffer[i])/sizeof(index_unit))
		       && nn <10){
			if (n== old) nn++;
			else if (nn) nn--;
			old = n;
			writeout_audio(mx, MPEG_AUDIO, i);
		}
	}
	
        old = 0;nn=0;
	for (i = 0; i < mx->ac3n; i++){
		while ((n=ring_avail(&mx->index_ac3rbuffer[i])
			/sizeof(index_unit))
			&& nn<10){
			if (n== old) nn++;
			else if (nn) nn--;
			old = n;
			writeout_audio(mx, AC3, i);
		}
	}
                        
                          
	if (mx->otype == REPLEX_MPEG2)
		mplx_write(mx, mpeg_end,4);
}


void init_multiplex( multiplex_t *mx, sequence_t *seq_head, audio_frame_t *aframe,
		     audio_frame_t *ac3frame, int apidn, int ac3n, 
		     uint64_t video_delay, uint64_t audio_delay, int fd,
		     int (*fill_buffers)(void *p, int f),
		     ringbuffer *vrbuffer, ringbuffer *index_vrbuffer,	
		     ringbuffer *arbuffer, ringbuffer *index_arbuffer,
		     ringbuffer *ac3rbuffer, ringbuffer *index_ac3rbuffer,
		     int otype)
{
	int i;
	uint32_t data_rate;

	mx->fill_buffers = fill_buffers;
	mx->video_delay = video_delay;
	mx->audio_delay = audio_delay;
	mx->fd_out = fd;
	mx->otype = otype;
	mx->total_written = 0;
	mx->zero_write_count = 0;
	mx->max_write = 0;
	mx->max_reached = 0;

	switch(mx->otype){

	case REPLEX_DVD:
		mx->video_delay += 180*CLOCK_MS;
		mx->audio_delay += 180*CLOCK_MS;
		mx->pack_size = 2048;
		mx->audio_buffer_size = 4*1024;
		mx->video_buffer_size = 232*1024;
		mx->mux_rate = 1260000;
		mx->navpack = 1;
		mx->frame_timestamps = TIME_IFRAME;
		mx->VBR = 1;
		mx->reset_clocks = 0;
		mx->write_end_codes = 0;
		mx->set_broken_link = 0;
		//		mx->max_write = 1024*1024*1024; // 1GB max VOB length
		break;


	case REPLEX_MPEG2:
		mx->video_delay += 180*CLOCK_MS;
		mx->audio_delay += 180*CLOCK_MS;
		mx->pack_size = 2048;
		mx->audio_buffer_size = 4*1024;
		mx->video_buffer_size = 224*1024;
		mx->mux_rate = 0;
		mx->navpack = 0;
		mx->frame_timestamps = TIME_ALWAYS;
		mx->VBR = 1;
		mx->reset_clocks = 1;
		mx->write_end_codes = 1;
		mx->set_broken_link = 1;
		break;

	case REPLEX_HDTV:
		mx->video_delay += 180*CLOCK_MS;
		mx->audio_delay += 180*CLOCK_MS;
		mx->pack_size = 2048;
		mx->audio_buffer_size = 4*1024;
		mx->video_buffer_size = 4*224*1024;
		mx->mux_rate = 0;
		mx->navpack = 0;
		mx->frame_timestamps = TIME_ALWAYS;
		mx->VBR = 1;
		mx->reset_clocks = 1;
		mx->write_end_codes = 1;
		mx->set_broken_link = 1;
		break;
	}

	mx->apidn = apidn;
	mx->ac3n = ac3n;


	mx->vrbuffer = vrbuffer;
	mx->index_vrbuffer = index_vrbuffer;
	mx->arbuffer = arbuffer;
	mx->index_arbuffer = index_arbuffer;
	mx->ac3rbuffer = ac3rbuffer;
	mx->index_ac3rbuffer = index_ac3rbuffer;

	dummy_init(&mx->vdbuf, mx->video_buffer_size);
	for (i=0; i<mx->apidn;i++){
		mx->apts_off[i] = 0;
		dummy_init(&mx->adbuf[i],mx->audio_buffer_size);
	}
	for (i=0; i<mx->ac3n;i++){
		mx->ac3pts_off[i] = 0;
		dummy_init(&mx->ac3dbuf[i], mx->audio_buffer_size);
	}

	mx->data_size = mx->pack_size - PES_H_MIN -10; 
	mx->vsize = mx->data_size;
	mx->asize = mx->data_size+5; // one less DTS
	
	data_rate = seq_head->bit_rate *400;
	for ( i = 0; i < mx->apidn; i++)
		data_rate += aframe[i].bit_rate;
	for ( i = 0; i < mx->ac3n; i++)
		data_rate += ac3frame[i].bit_rate;

	
	mx->muxr = ((uint64_t)data_rate / 8 * mx->pack_size) / mx->data_size;
//        mx->muxr = (data_rate / 8 * mx->pack_size) / mx->data_size; 
                                     // muxrate of payload in Byte/s

	if (mx->mux_rate) {
		if ( mx->mux_rate < mx->muxr)
                        fprintf(stderr, "data rate may be to high for required mux rate\n");
                mx->muxr = mx->mux_rate;
        }
	fprintf(stderr, "Mux rate: %.2f Mbit/s\n", mx->muxr*8.0/1000000.);
	
	mx->SCRinc = 27000000ULL/((uint64_t)mx->muxr / 
				     (uint64_t) mx->pack_size);
	
}

void setup_multiplex(multiplex_t *mx)
{
	int packlen;
	int i;

 	get_next_video_unit(mx, &mx->viu);
	for (i=0; i < mx->apidn; i++){
		get_next_audio_unit(mx, &mx->aiu[i], i);
		mx->apts[i] = uptsdiff(mx->aiu[i].pts +mx->audio_delay, 
				       mx->apts_off[i]); 
	}
	for (i=0; i < mx->ac3n; i++){
		get_next_ac3_unit(mx, &mx->ac3iu[i], i);
		mx->ac3pts[i] = uptsdiff(mx->ac3iu[i].pts +mx->audio_delay, 
					 mx->ac3pts_off[i]); 
	}

	packlen = mx->pack_size;

	mx->SCR = 0;

	// write first VOBU header
	if (mx->navpack){
		uint8_t outbuf[2048];
		write_nav_pack(mx->pack_size, mx->apidn, mx->ac3n, 
			       mx->SCR, mx->muxr, outbuf);
		mplx_write(mx, outbuf, mx->pack_size);
		ptsinc(&mx->SCR, mx->SCRinc);
		mx->startup = 1;
	} else mx->startup = 0;
}
