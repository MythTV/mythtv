/*
 *  dvb-mpegtools for the Siemens Fujitsu DVB PCI card
 *
 * Copyright (C) 2000, 2001 Marcus Metzler 
 *            for convergence integrated media GmbH
 * Copyright (C) 2002  Marcus Metzler 
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

 * The author can be reached at marcus@convergence.de, 

 * the project's page is at http://linuxtv.org/dvb/
 */


#include "transform.h"
#include <stdlib.h>
#include <string.h>
        
static unsigned int bitrates[3][16] =
{{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

static uint32_t freq[4] = {441, 480, 320, 0};

uint64_t trans_pts_dts(uint8_t *pts)
{
    uint64_t wts;

    wts = ((uint64_t)((pts[0] & 0x0E) << 5) |
           ((pts[1] & 0xFC) >> 2)) << 24;
    wts |= (((pts[1] & 0x03) << 6) |
        ((pts[2] & 0xFC) >> 2)) << 16;
    wts |= (((pts[2] & 0x02) << 6) |
        ((pts[3] & 0xFE) >> 1)) << 8;
    wts |= (((pts[3] & 0x01) << 7) |
        ((pts[4] & 0xFE) >> 1));
    return wts;
}

void get_pespts(uint8_t *av_pts,uint8_t *pts)
{
    pts[0] = 0x21 |
        ((av_pts[0] & 0xC0) >>5);
    pts[1] = ((av_pts[0] & 0x3F) << 2) |
        ((av_pts[1] & 0xC0) >> 6);
    pts[2] = 0x01 | ((av_pts[1] & 0x3F) << 2) |
        ((av_pts[2] & 0x80) >> 6);
    pts[3] = ((av_pts[2] & 0x7F) << 1) |
        ((av_pts[3] & 0x80) >> 7);
    pts[4] = 0x01 | ((av_pts[3] & 0x7F) << 1);
}

uint16_t get_pid(uint8_t *pid)
{
    uint16_t pp = 0;

    pp = (pid[0] & PID_MASK_HI)<<8;
    pp |= pid[1];

    return pp;
}


void reset_ipack(ipack *p)
{
    p->found = 0;
    p->cid = 0;
    p->plength = 0;
    p->flag1 = 0;
    p->flag2 = 0;
    p->hlength = 0;
    p->mpeg = 0;
    p->check = 0;
    p->which = 0;
    p->done = 0;
    p->count = 0;
    p->size = p->size_orig;
}

void init_ipack(ipack *p, int size,
                void (*func)(uint8_t *buf,  int size, void *priv))
{
    if ( !(p->buf = malloc(size)) ){
        fprintf(stderr,"Couldn't allocate memory for ipack\n");
        exit(1);
    }
    p->size_orig = size;
    p->func = func;
    reset_ipack(p);
    p->has_ai = 0;
    p->has_vi = 0;
    p->start = 0;
    p->replaceid =0;
    p->priv_type = PRIV_NONE;
}

void free_ipack(ipack * p)
{
    if (p->buf) free(p->buf);
}

int get_vinfo(uint8_t *mbuf, int count, VideoInfo *vi, int pr)
{
    uint8_t *headr;
    int found = 0;
    int sw;
    int form = -1;
    int c = 0;

    while (found < 4 && c+4 < count){
        uint8_t *b;

        b = mbuf+c;
        if ( b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x01
             && b[3] == 0xb3) found = 4;
        else {
            c++;
        }
    }

    if (! found) return -1;
    c += 4;
    if (c+12 >= count) return -1;
    headr = mbuf+c;

    vi->horizontal_size    = ((headr[1] &0xF0) >> 4) | (headr[0] << 4);
    vi->vertical_size    = ((headr[1] &0x0F) << 8) | (headr[2]);
    
    sw = (int)((headr[3]&0xF0) >> 4) ;

    switch( sw ){
    case 1:
        if (pr)
            fprintf(stderr,"Videostream: ASPECT: 1:1");
        vi->aspect_ratio = 100;        
        break;
    case 2:
        if (pr)
            fprintf(stderr,"Videostream: ASPECT: 4:3");
        vi->aspect_ratio = 133;        
        break;
    case 3:
        if (pr)
            fprintf(stderr,"Videostream: ASPECT: 16:9");
        vi->aspect_ratio = 177;        
        break;
    case 4:
        if (pr)
            fprintf(stderr,"Videostream: ASPECT: 2.21:1");
        vi->aspect_ratio = 221;        
        break;

        case 5 ... 15:
        if (pr)
            fprintf(stderr,"Videostream: ASPECT: reserved");
        vi->aspect_ratio = 0;        
        break;

    default:
        vi->aspect_ratio = 0;        
        return -1;
    }

    if (pr)
        fprintf(stderr,"  Size = %dx%d",vi->horizontal_size,
            vi->vertical_size);

    sw = (int)(headr[3]&0x0F);

    switch ( sw ) {
    case 1:
        if (pr)
            fprintf(stderr,"  FRate: 23.976 fps");
        vi->framerate = 24000/1001.;
        form = -1;
        break;
    case 2:
        if (pr)
            fprintf(stderr,"  FRate: 24 fps");
        vi->framerate = 24;
        form = -1;
        break;
    case 3:
        if (pr)
            fprintf(stderr,"  FRate: 25 fps");
        vi->framerate = 25;
        form = VIDEO_MODE_PAL;
        break;
    case 4:
        if (pr)
            fprintf(stderr,"  FRate: 29.97 fps");
        vi->framerate = 30000/1001.;
        form = VIDEO_MODE_NTSC;
        break;
    case 5:
        if (pr)
            fprintf(stderr,"  FRate: 30 fps");
        vi->framerate = 30;
        form = VIDEO_MODE_NTSC;
        break;
    case 6:
        if (pr)
            fprintf(stderr,"  FRate: 50 fps");
        vi->framerate = 50;
        form = VIDEO_MODE_PAL;
        break;
    case 7:
        if (pr)
            fprintf(stderr,"  FRate: 60 fps");
        vi->framerate = 60;
        form = VIDEO_MODE_NTSC;
        break;
    }

    vi->bit_rate = 400*(((headr[4] << 10) & 0x0003FC00UL) 
                | ((headr[5] << 2) & 0x000003FCUL) | 
                (((headr[6] & 0xC0) >> 6) & 0x00000003UL));
    
    if (pr){
        fprintf(stderr,"  BRate: %.2f Mbit/s",(vi->bit_rate)/1000000.);
        fprintf(stderr,"\n");
    }
    vi->video_format = form;

    vi->off = c-4;
    return c-4;
}

extern unsigned int bitrates[3][16];
extern uint32_t freq[4];

int get_ainfo(uint8_t *mbuf, int count, AudioInfo *ai, int pr)
{
    uint8_t *headr;
    int found = 0;
    int c = 0;
    int fr =0;
    
    while (!found && c < count){
        uint8_t *b = mbuf+c;

        if ( b[0] == 0xff && (b[1] & 0xf8) == 0xf8)
            found = 1;
        else {
            c++;
        }
    }    

    if (!found) return -1;

    if (c+3 >= count) return -1;
        headr = mbuf+c;

    ai->layer = (headr[1] & 0x06) >> 1;

    if (pr)
        fprintf(stderr,"Audiostream: Layer: %d", 4-ai->layer);


    ai->bit_rate = bitrates[(3-ai->layer)][(headr[2] >> 4 )]*1000;

    if (pr){
        if (ai->bit_rate == 0)
            fprintf (stderr,"  Bit rate: free");
        else if (ai->bit_rate == 0xf)
            fprintf (stderr,"  BRate: reserved");
        else
            fprintf (stderr,"  BRate: %d kb/s", ai->bit_rate/1000);
    }

    fr = (headr[2] & 0x0c ) >> 2;
    ai->frequency = freq[fr]*100;
    
    if (pr){
        if (ai->frequency == 3)
            fprintf (stderr, "  Freq: reserved\n");
        else
            fprintf (stderr,"  Freq: %2.1f kHz\n", 
                 ai->frequency/1000.);
    }
    ai->off = c;
    return c;
}

unsigned int ac3_bitrates[32] =
    {32,40,48,56,64,80,96,112,128,160,192,224,256,320,384,448,512,576,640,
     0,0,0,0,0,0,0,0,0,0,0,0,0};

uint32_t ac3_freq[4] = {480, 441, 320, 0};
uint32_t ac3_frames[3][32] =
    {{64,80,96,112,128,160,192,224,256,320,384,448,512,640,768,896,1024,
      1152,1280,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {69,87,104,121,139,174,208,243,278,348,417,487,557,696,835,975,1114,
      1253,1393,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {96,120,144,168,192,240,288,336,384,480,576,672,768,960,1152,1344,
      1536,1728,1920,0,0,0,0,0,0,0,0,0,0,0,0,0}}; 

#if 0
static uint8_t ac3_channels[8] = {2,1,2,3,3,4,4,5};
static char ac3_audio_coding[8][8] =
{ "1+1","1/0","2/0","3/0","2/1","3/1","2/2","3/2" };
#endif

int get_ac3info(uint8_t *mbuf, int count, AudioInfo *ai, int pr)
{
    uint8_t *headr;
    int found = 0;
    int c = 0;
    uint8_t frame;
    int fr = 0;

    while ( !found  && c < count){
        uint8_t *b = mbuf+c;
        if ( b[0] == 0x0b &&  b[1] == 0x77 )
            found = 1;
        else {
            c++;
        }
    }    


    if (!found){
        return -1;
    }
    ai->off = c;

    if (c+5 >= count) return -1;

    ai->layer = 0;  // 0 for AC3
        headr = mbuf+c+2;

    frame = (headr[2]&0x3f);
    ai->bit_rate = ac3_bitrates[frame>>1]*1000;

    if (pr) fprintf (stderr,"Audiostream: (AC3)  BRate: %d kb/s", ai->bit_rate/
1000);

    fr = (headr[2] & 0xc0 ) >> 6;
    ai->frequency = ac3_freq[fr]*100;
    if (pr) fprintf (stderr,"  Freq: %d Hz ", ai->frequency);

    ai->framesize = ac3_frames[fr][frame >> 1];
    if ((frame & 1) &&  (fr == 1)) ai->framesize++;
    ai->framesize = ai->framesize << 1;

// TODO: I cannot figure out how to get the channels properly
//        if (pr) fprintf (stderr, " Channels: %d (%s)\n",ac3_channels[headr[4] >> 5],
//                                 ac3_audio_coding[headr[4] >> 5]);

    if (pr) fprintf (stderr, "\n");

    return c;
}

void send_ipack(ipack *p)
{
    int streamid=0;
    int off;
    int ac3_off = 0;
    AudioInfo ai;
    int nframes= 0;
    int f=0;

    if (p->count < 10) return;
    p->buf[3] = p->cid;
    p->buf[4] = (uint8_t)(((p->count-6) & 0xFF00) >> 8);
    p->buf[5] = (uint8_t)((p->count-6) & 0x00FF);

    
    if (p->cid == PRIVATE_STREAM1){

        off = 9+p->buf[8];
        streamid = p->buf[off];
        switch (streamid & 0xF8){

        case 0x80:
            ai.off = 0;
            ac3_off = ((p->buf[off+2] << 8)| p->buf[off+3]);
            if (ac3_off < p->count)
                f=get_ac3info(p->buf+off+3+ac3_off, 
                          p->count-ac3_off, &ai,0);
            if ( !f ){
                nframes = (p->count-off-3-ac3_off)/ 
                    ai.framesize + 1;
                p->buf[off+1] = nframes;
                p->buf[off+2] = (ac3_off >> 8)& 0xFF;
                p->buf[off+3] = (ac3_off)& 0xFF;
                
                ac3_off +=  nframes * ai.framesize - p->count;
            }
            break;

        case 0x20:
        default:
            break;
        }
    } else     if(p->replaceid)
        p->buf[3] = p->replaceid;

    p->func(p->buf, p->count, p->data);    

    switch ( p->mpeg ){
    case 2:        
        
        p->buf[6] = 0x80;
        p->buf[7] = 0x00;
        p->buf[8] = 0x00;
        p->count = 9;

        if (p->cid == PRIVATE_STREAM1){
            switch (streamid & 0xF8){
            case 0x80:
                p->count += 4;
                p->buf[8] = p->hlength;
                p->buf[9] = streamid;
                p->buf[10] = 0;
                p->buf[11] = (ac3_off >> 8)& 0xFF;
                p->buf[12] = (ac3_off)& 0xFF;
                break;

            case 0x20:
                p->count += 2;
                p->buf[9] = 0x20;
                p->buf[10] = 0;
                break;
            }
        }
        break;

    case 1:
        p->buf[6] = 0x0F;
        p->count = 7;
        break;
    }

}


static void write_ipack(ipack *p, uint8_t *data, int count)
{
    uint8_t headr[3] = { 0x00, 0x00, 0x01} ;

    if (p->count < 6){
        p->count = 0;
        memcpy(p->buf+p->count, headr, 3);
        p->count += 6;
    }

    if (p->cid == PRIVATE_STREAM1 && p->count == p->hlength+9){
        switch (p->priv_type){
        case PRIV_DVD_AC3:
        case PRIV_DVB_SUB:
            fprintf(stderr,"juhu 0x%2x\n",data[0]);
            break;

        case PRIV_TS_AC3: /* keep this as default */
        default:
        {
            /*
             * add in a dummy 4 byte audio header
             * to match mpeg dvd standard. The values
             * will be filled in later (in send_ipack)
             * when it has a full packet to search
             */
            p->buf[p->count] = 0x80;
            p->buf[p->count+1] = 0;
            p->buf[p->count+2] = 0;
            p->buf[p->count+3] = 0;
            p->count+=4;
        }
        break;
        }
    }

    if (p->count + count < p->size){
        memcpy(p->buf+p->count, data, count); 
        p->count += count;
    } else {
        int rest = p->size - p->count;
        if (rest < 0) rest = 0;
        memcpy(p->buf+p->count, data, rest);
        p->count += rest;
//        fprintf(stderr,"count: %d \n",p->count);
        send_ipack(p);
        if (rest > 0 && count - rest > 0)
            write_ipack(p, data+rest, count-rest);
    }
}

void instant_repack (uint8_t *buf, int count, ipack *p)
{

    int l;
    unsigned short *pl;
    int c=0;

    while (c < count && (p->mpeg == 0 ||
           (p->mpeg == 1 && p->found < 7) ||
           (p->mpeg == 2 && p->found < 9))
           &&  (p->found < 5 || !p->done)){
        switch ( p->found ){
        case 0:
        case 1:
            if (buf[c] == 0x00) p->found++;
            else p->found = 0;
            c++;
            break;
        case 2:
            if (buf[c] == 0x01) p->found++;
            else if (buf[c] == 0){
                p->found = 2;
            } else p->found = 0;
            c++;
            break;
        case 3:
            p->cid = 0;
            switch (buf[c]){
            case PROG_STREAM_MAP:
            case PRIVATE_STREAM2:
            case PROG_STREAM_DIR:
            case ECM_STREAM     :
            case EMM_STREAM     :
            case PADDING_STREAM :
            case DSM_CC_STREAM  :
            case ISO13522_STREAM:
                p->done = 1;
            case PRIVATE_STREAM1:
            case VIDEO_STREAM_S ... VIDEO_STREAM_E:
            case AUDIO_STREAM_S ... AUDIO_STREAM_E:
                p->found++;
                p->cid = buf[c];
                c++;
                break;
            default:
                p->found = 0;
                break;
            }
            break;
            

        case 4:
            if (count-c > 1){
                pl = (unsigned short *) (buf+c);
                p->plength =  ntohs(*pl);
                p->plen[0] = buf[c];
                c++;
                p->plen[1] = buf[c];
                c++;
                p->found+=2;
            } else {
                p->plen[0] = buf[c];
                p->found++;
                return;
            }
            break;
        case 5:
            p->plen[1] = buf[c];
            c++;
            pl = (unsigned short *) p->plen;
            p->plength = ntohs(*pl);
            p->found++;
            break;


        case 6:
            if (!p->done){
                p->flag1 = buf[c];
                c++;
                p->found++;
                if ( (p->flag1 & 0xC0) == 0x80 ) p->mpeg = 2;
                else {
                    p->hlength = 0;
                    p->which = 0;
                    p->mpeg = 1;
                    p->flag2 = 0;
                }
            }
            break;

        case 7:
            if ( !p->done && p->mpeg == 2){
                p->flag2 = buf[c];
                c++;
                p->found++;
            }    
            break;

        case 8:
            if ( !p->done && p->mpeg == 2){
                p->hlength = buf[c];
                c++;
                p->found++;
            }
            break;
            
        default:

            break;
        }
    }


    if (c == count) return;

    if (!p->plength) p->plength = MMAX_PLENGTH-6;


    if ( p->done || ((p->mpeg == 2 && p->found >= 9) )){
        switch (p->cid){
            
        case AUDIO_STREAM_S ... AUDIO_STREAM_E:            
        case VIDEO_STREAM_S ... VIDEO_STREAM_E:
        case PRIVATE_STREAM1:
            
            if (p->mpeg == 2 && p->found == 9){
                write_ipack(p, &p->flag1, 1);
                write_ipack(p, &p->flag2, 1);
                write_ipack(p, &p->hlength, 1);
            }

            if (p->mpeg == 2 && p->found < 9+p->hlength){
                while (c < count && p->found < 9+p->hlength){
                    write_ipack(p, buf+c, 1);
                    c++;
                    p->found++;
                }
                if (c == count) return;
            }
            
            while (c < count && (long)p->found < (long)p->plength+6){
                l = count -c;
                if (l+(long)p->found > (long)p->plength+6)
                    l = p->plength+6-p->found;
                write_ipack(p, buf+c, l);
                p->found += l;
                c += l;
            }    
        
            break;
        }


        if ( p->done ){
            if( (long)p->found + count - c < (long)p->plength+6){
                p->found += count-c;
                c = count;
            } else {
                c += p->plength+6 - p->found;
                p->found = p->plength+6;
            }
        }

        if (p->plength && (long)p->found == (long)p->plength+6) {
            send_ipack(p);
            reset_ipack(p);
            if (c < count)
                instant_repack(buf+c, count-c, p);
        }
    }
    return;
}
