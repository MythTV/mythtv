#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "multiplex.h"

static int buffers_filled(multiplex_t *mx)
{
	int vavail=0, aavail=0, i;

	vavail = ring_avail(mx->index_vrbuffer)/sizeof(index_unit);
	
	for (i=0; i<mx->extcnt;i++){
		aavail += ring_avail(&mx->index_extrbuffer[i])/
				     sizeof(index_unit);
	}

	if (aavail+vavail) return ((aavail+vavail));
	return 0;
}

static int all_ext_ok(int *aok, int n)
{
	int ok=0,i;

	if (!n) return 0;
	for (i=0;  i < n ;i++){
		if (aok[i]) ok ++;
	}
	if (ok == n) return 1;
	return 0;
}

static int rest_ext_ok(int j, int *aok, int n)
{
	int ok=0,i;

	if (!(n-j-1)) return 0;
	for (i=j+1;  i < n ;i++){
		if (aok[i]) ok ++;
	}
	if (ok == (n-j-1)) return 1;
	return 0;
}

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
	
static int get_next_ext_unit(multiplex_t *mx, index_unit *extiu, int i)
{
	if (!ring_avail(&mx->index_extrbuffer[i]) && mx->finish) return 0;

	while(ring_avail(&mx->index_extrbuffer[i]) < sizeof(index_unit))
		if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
			fprintf(stderr,"error in get next ext unit\n");
			return 0;
		}
	
	ring_read(&mx->index_extrbuffer[i], (uint8_t *)extiu,
		  sizeof(index_unit));

#ifdef OUT_DEBUG
	fprintf(stderr,"ext index start: %d  stop: %d  (%d)  rpos: %d\n", 
		extiu->start, (extiu->start+extiu->length),
		extiu->length, ring_rpos(&mx->extrbuffer[i]));
#endif
	return 1;
}

static uint8_t get_ptsdts(multiplex_t *mx, index_unit *viu)
{
	uint8_t ptsdts = 0;
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
	return ptsdts;
}

static void writeout_video(multiplex_t *mx)
{  
	uint8_t outbuf[3000];
	int written=0;
	uint8_t ptsdts=0;
	unsigned int length;
	int nlength=0;
        int frame_len=0;
	index_unit *viu = &mx->viu;

#ifdef OUT_DEBUG
	fprintf(stderr,"writing VIDEO pack\n");
#endif

	if(viu->frame_start) {
		ptsdts = get_ptsdts(mx, viu);
                frame_len = viu->length;
	}

	if (viu->frame_start && viu->seq_header && viu->gop && 
	    viu->frame == I_FRAME){
		if (!mx->startup && mx->navpack){
			write_nav_pack(mx->pack_size, mx->extcnt, 
				       mx->SCR, mx->muxr, outbuf);
			write(mx->fd_out, outbuf, mx->pack_size);
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
	while (length  < mx->data_size){
		index_unit nviu;
                int old_start = viu->frame_start;
                int old_frame = viu->frame;
                uint64_t old_pts = viu->pts;
                uint64_t old_dts = viu->dts;
		dummy_add(&mx->vdbuf, uptsdiff(viu->dts+mx->video_delay,0)
			  , viu->length);
		if ( peek_next_video_unit(mx, &nviu)){
			if (!(nviu.seq_header && nviu.gop && 
			      nviu.frame == I_FRAME)){
				get_next_video_unit(mx, viu);
                                frame_len = viu->length;
				length += viu->length; 
				if(old_start) {
					viu->pts = old_pts;
					viu->dts = old_dts;
					viu->frame = old_frame;
				} else {
					ptsdts = get_ptsdts(mx, viu);
				}
			} else break;
		} else break;
	}

	if (viu->frame_start){
		viu->frame_start=0;
	if (viu->gop){
                uint8_t gop[8];
                frame_len=length-frame_len;
                ring_peek(mx->vrbuffer, gop, 8, frame_len);
		pts2time( viu->pts + mx->video_delay, gop, 8);
                ring_poke(mx->vrbuffer, gop, 8, frame_len);
                viu->gop=0;
	}
		if (mx->VBR) {
			mx->extra_clock = ptsdiff(viu->dts + mx->video_delay, 
						  mx->SCR + 500*CLOCK_MS);
#ifdef OUT_DEBUG1
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
	written = write_video_pes( mx->pack_size, mx->extcnt, 
				   viu->pts+mx->video_delay, 
				   viu->dts+mx->video_delay, 
				   mx->SCR, mx->muxr, outbuf, &nlength, ptsdts,
				   mx->vrbuffer);

	length -= nlength;
	dummy_add(&mx->vdbuf, uptsdiff( viu->dts+mx->video_delay,0)
		  , viu->length-length);
	viu->length = length;

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

static void writeout_ext(multiplex_t *mx, int type, int n)
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

	ringbuffer *airbuffer = &mx->index_extrbuffer[n];
	dummy_buffer *dbuf = &mx->extdbuf[n];
	uint64_t adelay = mx->extpts_off[n];
	uint64_t *apts = &mx->extpts[n];
	index_unit *aiu = &mx->extiu[n];

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
		return;
	}
	
	if (mx->finish != 2 && dummy_space(dbuf) < mx->data_size + rest_data){
		return;
	}

	pts = uptsdiff( aiu->pts + mx->audio_delay, adelay );
	*apts = pts;
	length = aiu->length;
	if (length < aiu->framesize){
		newpts = 1;
		ac3_off = length;
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

			length+= aiu->length;
			if (length < mx->data_size + rest_data)
				dummy_add(dbuf, dpts, aiu->length);
			
			*apts = dpts;
			nframes++;
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
			fprintf(stderr,"error in writeout ext\n");
			exit(1);
		}
	}

	nlength = length;

	switch (type) {
	case MPEG_AUDIO:
		written = write_audio_pes( mx->pack_size, mx->extcnt,
					   mx->exttypcnt[n], pts, mx->SCR,
					   mx->muxr, outbuf, &nlength, PTS_ONLY,
					   &mx->extrbuffer[n]);
		break;
	case AC3:
		written = write_ac3_pes( mx->pack_size, mx->extcnt,
					 mx->exttypcnt[n], pts, mx->SCR,
					 mx->muxr, outbuf, &nlength, PTS_ONLY,
					 nframes, ac3_off,
					 &mx->extrbuffer[n]);
		break;
	}

	length -= nlength;
	write(mx->fd_out, outbuf, written);

	dummy_add(dbuf, dpts, aiu->length-length);
	aiu->length = length;
	aiu->start = ring_rpos(&mx->extrbuffer[n]);

	if (aiu->length == 0){
		get_next_ext_unit(mx, aiu, n);
	}
	*apts = uptsdiff(aiu->pts + mx->audio_delay, adelay);
#ifdef OUT_DEBUG
	if ((int64_t)*apts < 0) fprintf(stderr,"SCHEISS ");
	fprintf(stderr,"APTS");
	printpts(*apts);
	printpts(aiu->pts);
	printpts(mx->audio_delay);
	printpts(adelay);
	fprintf(stderr,"\n");
#endif

	if (mx->fill_buffers(mx->priv, mx->finish)< 0) {
		fprintf(stderr,"error in writeout ext\n");
		exit(1);
	}
}

static void writeout_padding (multiplex_t *mx)
{
	uint8_t outbuf[3000];
	//fprintf(stderr,"writing PADDING pack\n");

	write_padding_pes( mx->pack_size, mx->extcnt, mx->SCR, 
			   mx->muxr, outbuf);
	write(mx->fd_out, outbuf, mx->pack_size);
}

void check_times( multiplex_t *mx, int *video_ok, int *ext_ok, int *start)
{
	int i;
	int set_ok = 0;

	memset(ext_ok, 0, N_AUDIO*sizeof(int));
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
#ifdef OUT_DEBUG1
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
			
			for (i=0; i<mx->extcnt; i++){
				if (ptscmp(mx->SCR + temp_scr + 100*CLOCK_MS, 
					   mx->extiu[i].pts) > 0) {
					while (ptscmp(mx->SCR + temp_scr 
						      + 100*CLOCK_MS,
						      mx->extiu[i].pts) > 0) 
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
	
	for (i=0;i <mx->extcnt; i++)
		dummy_delete(&mx->extdbuf[i], mx->SCR);
	
	if (dummy_space(&mx->vdbuf) > mx->vsize && mx->viu.length > 0 &&
	    (ptscmp(mx->viu.dts + mx->video_delay, 1000*CLOCK_MS +mx->oldSCR)<0)
	    && ring_avail(mx->index_vrbuffer)){
		*video_ok = 1;
                set_ok = 1;
	}
	
	for (i = 0; i < mx->extcnt; i++){
		if (dummy_space(&mx->extdbuf[i]) > mx->extsize && 
		    mx->extiu[i].length > 0 &&
		    ptscmp(mx->extpts[i], 200*CLOCK_MS + mx->oldSCR) < 0
		    && ring_avail(&mx->index_extrbuffer[i])){
			ext_ok[i] = 1;
                        set_ok = 1;
		}
	}
#ifdef OUT_DEBUG
	if (set_ok) {
		fprintf(stderr, "SCR");
		printpts(mx->oldSCR);
		fprintf(stderr, "VDTS");
		printpts(mx->viu.dts);
		fprintf(stderr, " (%d)", *video_ok);
		fprintf(stderr, " EXT");
		for (i = 0; i < mx->extcnt; i++){
			fprintf(stderr, "%d:", mx->exttype[i]);
			printpts(mx->extpts[i]);
			fprintf(stderr, " (%d)", ext_ok[i]);
		}
		fprintf(stderr, "\n");
	}
#endif
}

void write_out_packs( multiplex_t *mx, int video_ok, int *ext_ok)
{
	int i;

	if (video_ok && !all_ext_ok(ext_ok, mx->extcnt)) {
		writeout_video(mx);  
	} else { // second case(s): audio ok, video in time
		int done=0;
		for ( i = 0; i < mx->extcnt; i++){
			if ( ext_ok[i] && !rest_ext_ok(i, ext_ok, mx->extcnt)) {
				writeout_ext(mx, mx->exttype[i], i);
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
	int ext_ok[N_AUDIO];
        int n,nn,old,i;
        uint8_t mpeg_end[4] = { 0x00, 0x00, 0x01, 0xB9 };
                                                                                
        memset(ext_ok, 0, N_AUDIO*sizeof(int));
        mx->finish = 1;
                                                                                
        old = 0;nn=0;
        while ((n=buffers_filled(mx)) && nn<1000 ){
                if (n== old) nn++;
                else  nn=0;
                old = n;
                check_times( mx, &video_ok, ext_ok, &start);
                write_out_packs( mx, video_ok, ext_ok);
        }

        old = 0;nn=0;
	while ((n=ring_avail(mx->index_vrbuffer)/sizeof(index_unit))
	       && nn<1000){
		if (n== old) nn++;
		else nn= 0;
		old = n;
		writeout_video(mx);  
	}
	
// flush the rest
        mx->finish = 2;
        old = 0;nn=0;
	for (i = 0; i < mx->extcnt; i++){
		while ((n=ring_avail(&mx->index_extrbuffer[i])/
				     sizeof(index_unit)) && nn <100){
			if (n== old) nn++;
			else nn = 0;
			old = n;
			writeout_ext(mx, mx->exttype[i], i);
		}
	}
	
	if (mx->otype == REPLEX_MPEG2)
		write(mx->fd_out, mpeg_end,4);

	dummy_destroy(&mx->vdbuf);
	for (i=0; i<mx->extcnt;i++)
		dummy_destroy(&mx->extdbuf[i]);
}


void init_multiplex( multiplex_t *mx, sequence_t *seq_head,
                     audio_frame_t *extframe, int *exttype, int *exttypcnt,
		     uint64_t video_delay, uint64_t audio_delay, int fd,
		     int (*fill_buffers)(void *p, int f),
		     ringbuffer *vrbuffer, ringbuffer *index_vrbuffer,	
		     ringbuffer *extrbuffer, ringbuffer *index_extrbuffer,
		     int otype)
{
	int i;
	uint32_t data_rate;

	mx->fill_buffers = fill_buffers;
	mx->video_delay = video_delay;
	mx->audio_delay = audio_delay;
	mx->fd_out = fd;
	mx->otype = otype;

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

	mx->extcnt = 0;
	for (mx->extcnt = 0; mx->extcnt < N_AUDIO && exttype[mx->extcnt];
			mx->extcnt++)
		;
	memcpy(mx->exttype, exttype, mx->extcnt);
	memcpy(mx->exttypcnt, exttypcnt, mx->extcnt);

	mx->vrbuffer = vrbuffer;
	mx->index_vrbuffer = index_vrbuffer;
	mx->extrbuffer = extrbuffer;
	mx->index_extrbuffer = index_extrbuffer;

	dummy_init(&mx->vdbuf, mx->video_buffer_size);
	for (i=0; i<mx->extcnt;i++){
		mx->extpts_off[i] = 0;
		switch (mx->exttype[i]) {
		case MPEG_AUDIO:
		case AC3:
		default:
			dummy_init(&mx->extdbuf[i],mx->audio_buffer_size);
		}
	}

	mx->data_size = mx->pack_size - PES_H_MIN - PS_HEADER_L1 - 10; 
	mx->vsize = mx->data_size;
	mx->extsize = mx->data_size+5; // one less DTS
	
	data_rate = seq_head->bit_rate *400;
	for ( i = 0; i < mx->extcnt; i++)
		data_rate += extframe[i].bit_rate;

	
	mx->muxr = ((uint64_t)data_rate / 8 * mx->pack_size) / mx->data_size; 
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
	int i;

 	get_next_video_unit(mx, &mx->viu);
	for (i=0; i < mx->extcnt; i++)
        {
		get_next_ext_unit(mx, &mx->extiu[i], i);
		if (mx->exttype[i] == MPEG_AUDIO || mx->exttype[i] == AC3)
			mx->extpts[i] = uptsdiff(
					mx->extiu[i].pts + mx->audio_delay, 
					mx->extpts_off[i]);
		else
			mx->extpts[i] = uptsdiff(
					mx->extiu[i].pts, mx->extpts_off[i]);
	}

	mx->SCR = 0;

	// write first VOBU header
	if (mx->navpack){
		uint8_t outbuf[2048];
		write_nav_pack(mx->pack_size, mx->extcnt, 
			       mx->SCR, mx->muxr, outbuf);
		write(mx->fd_out, outbuf, mx->pack_size);
		ptsinc(&mx->SCR, mx->SCRinc);
		mx->startup = 1;
	} else mx->startup = 0;
}
