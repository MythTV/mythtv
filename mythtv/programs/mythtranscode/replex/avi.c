/*
 * avi.c: AVI container functions for replex
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

#include <stdlib.h>
#include <string.h>

#include "mpg_common.h"
#include "avi.h"
#include "replex.h"
#include "pes.h"


#define DEBUG 1

#ifdef DEBUG
#include "mpg_common.h"
#endif
uint32_t getle32(uint8_t *buf)
{
	return (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
}

uint32_t getbe32(uint8_t *buf)
{
	return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
}

void printhead(uint8_t *buf)
{
	printf("%c%c%c%c ", buf[0], buf[1], buf[2], buf[3]);
}

uint32_t getsize(int fd)
{
	int len;
	uint8_t buf[4];

	len=read(fd, buf, 4);
	return getle32(buf);
}

uint32_t getsize_buf(uint8_t *buf)
{
	return getle32(buf);
}


int check_riff(avi_context *ac, uint8_t *buf, int len)
{
	uint32_t tag;
	int c = 0;

	if (len < 12) return -1; 
	tag = getle32(buf);
	if (tag != TAG_IT('R','I','F','F')) return -1;
	c+=4;
	
	ac->riff_end = getle32(buf+c);
	c+=4;

	tag = getle32(buf+c);
	if (tag != TAG_IT('A','V','I',' ') && 
	    tag != TAG_IT('A','V','I','X') ) return -1;

	return c+4;
}

static
int new_idx_frame( avi_context *ac, uint32_t pos, uint32_t len, 
		   uint32_t fl, uint32_t id)
{
	int num = ac->num_idx_frames;
	if (ac->num_idx_alloc < num+1){
		avi_index *idx;
		uint32_t newnum = num + 1024;
		
		if (ac->idx){
			idx = realloc(ac->idx, 
						 newnum*sizeof(avi_index));
		} else {
			idx = malloc(newnum*sizeof(avi_index));
		}
		if (!idx) return -1;
		ac->idx = idx;
		ac->num_idx_alloc = newnum;
	}
	ac->idx[num].off = pos;
	ac->idx[num].id = id;
	ac->idx[num].len = len;
	ac->idx[num].flags = fl;
	ac->num_idx_frames++;

	

	return 0;
}

void print_index(avi_context *ac, int num){
	char *cc;
	cc = (char *) &ac->idx[num].id;
	fprintf(stderr,"%d chunkid: %c%c%c%c ", 
		num,
		*cc,*(cc+1),*(cc+2),*(cc+3));
	fprintf(stderr,"  chunkoff: 0x%04x ",
		(int)ac->idx[num].off);
	fprintf(stderr,"  chunksize: 0x%04x ",
		ac->idx[num].len);
	fprintf(stderr,"  chunkflags: 0x%04x \n",
		ac->idx[num].flags);
}

int avi_read_index(avi_context *ac, int fd)
{
	uint32_t tag;
	uint32_t isize;
	uint32_t c;
	off_t start;
	uint8_t buf[16];
	char *cc;

	if (!(ac->avih_flags & AVI_HASINDEX)) return -2;
	fprintf(stderr,"READING INDEX\n");
	if ((start = lseek(fd, 0, SEEK_CUR)) < 0) return -3;
	if (lseek(fd, ac->movi_length+ac->movi_start+4, SEEK_SET) < 0) return -4;

	read(fd,buf,4);			
	tag = getle32(buf);

	if (tag != TAG_IT('i','d','x','1')){
		cc = (char *) &tag;
		fprintf(stderr,"  tag: %c%c%c%c\n ",*cc,
			*(cc+1),*(cc+2),*(cc+3));

		if (lseek(fd, start, SEEK_SET) < 0 ) return -5;
		return -1;
	}
	isize = getsize(fd);
	c = 0;
	
	while ( c < isize ){
		uint32_t chunkid;
		uint32_t chunkflags;
		uint32_t chunkoff;
		uint32_t chunksize;

		read(fd,buf,16);			
		chunkid = getle32(buf);
		chunkflags = getle32(buf+4);
		chunkoff = getle32(buf+8);
		chunksize = getle32(buf+12);
		
		new_idx_frame(ac, chunkoff, chunksize, chunkflags,chunkid);
		switch(chunkid){
		case TAG_IT('0','1','w','b'):
			ac->achunks++;
			if (!chunksize) ac->zero_achunks++;
			break;
		
		case TAG_IT('0','0','d','c'):
			ac->vchunks++;
			if (!chunksize) ac->zero_vchunks++;
			break;
		}

#ifdef DEBUG
/*
			print_index(ac,ac->num_idx_frames-1);
*/
#endif
		c+=16;
	}
#ifdef DEBUG
	fprintf(stderr,"Found %d video (%d were empty) and %d audio (%d were empty) chunks\n", (int)ac->vchunks, (int)ac->zero_vchunks, (int)ac->achunks, (int)ac->zero_achunks);

#endif	
	lseek(fd, start, SEEK_SET);

	return 0;
}


int read_avi_header( avi_context *ac, int fd)
{
	uint8_t buf[256];
	uint32_t tag;
	uint32_t size = 0;
	int c = 0;
	int skip=0;
	int n;
#ifdef DEBUG
	char *cc;
#endif
 
	while ((c=read(fd, buf, 4))==4) {
		skip=0;
		tag = getle32(buf);

#ifdef DEBUG
		cc = (char *) &tag;
		fprintf(stderr,"tag: %c%c%c%c ",*cc,*(cc+1),*(cc+2),*(cc+3));
#endif
		switch(tag){
		case TAG_IT('L','I','S','T'):
			size = getsize(fd);
			break;

				
		case TAG_IT('m','o','v','i'): 
			ac->done=1;
			ac->movi_start = lseek(fd, 0, SEEK_CUR);
			ac->movi_length = size-8;
#ifdef DEBUG
			fprintf(stderr,"  size: %d",size);
			fprintf(stderr," header done\n");
#endif
			return 0;
			break;

		case TAG_IT('h','d','r','l'): 
			break;
			

		case TAG_IT('s','t','r','l'): 
			break;

		case TAG_IT('J','U','N','K'):
		case TAG_IT('s','t','r','f'):
		case TAG_IT('s','t','r','d'):
		case TAG_IT('s','t','r','n'):
			size = getsize(fd);
			skip=1;
			break;
		case TAG_IT('a','v','i','h'):
			size = getsize(fd);
			c=0;
			read(fd,buf,size);			
			ac->msec_per_frame = getle32(buf+c);
			c+=12;
			ac->avih_flags  = getle32(buf+c);
			c+=4;
			ac->total_frames = getle32(buf+c);
			c+=4;
			ac->init_frames = getle32(buf+c);
			c+=4;
			ac->nstreams = getle32(buf+c);
			c+=8;
			ac->width = getle32(buf+c);
			c+=4;
			ac->height = getle32(buf+c);
			c+=4;


#ifdef DEBUG
			fprintf(stderr,"  size: %d\n",size);
			fprintf(stderr,"    microsecs per frame %d\n",
				ac->msec_per_frame);
			if (ac->avih_flags & AVI_HASINDEX)
				fprintf(stderr,"    AVI has index\n");
			if (ac->avih_flags & AVI_USEINDEX)
				fprintf(stderr,"    AVI must use index\n");
			if (ac->avih_flags & AVI_INTERLEAVED)
				fprintf(stderr,"    AVI is interleaved\n");
			if(ac->total_frames)
				fprintf(stderr,"    total frames: %d\n",
					ac->total_frames);

			fprintf(stderr,"    number of streams: %d\n",
				ac->nstreams);
			fprintf(stderr,"    size: %dx%d\n",
				ac->width, ac->height);
#endif
			break;

		case TAG_IT('s','t','r','h'):
			size = getsize(fd);
#ifdef DEBUG
			fprintf(stderr,"  size: %d\n",size);
#endif

			c=0;
			read(fd,buf,size);
			tag = getle32(buf);
			c+=16;
#ifdef DEBUG
			cc = (char *) &tag;
			fprintf(stderr,"    tag: %c%c%c%c ",*cc,
				*(cc+1),*(cc+2),*(cc+3));
#endif
			switch ( tag ){
			case TAG_IT('v','i','d','s'):
				ac->vhandler = getle32(buf+4);
#ifdef DEBUG
				if (ac->vhandler){
					cc = (char *) &ac->vhandler;
					fprintf(stderr,"     video handler: %c%c%c%c "
						,*cc,*(cc+1),*(cc+2),*(cc+3));
				}
#endif
				ac->vi.initial_frames = getle32(buf+c);
				c+=4;
				ac->vi.dw_scale = getle32(buf+c); 
				c+=4;
				ac->vi.dw_rate = getle32(buf+c); 
				c+=4;
				ac->vi.dw_start = getle32(buf+c); 
				c+=4;
				if (ac->vi.dw_scale)
					ac->vi.fps = (ac->vi.dw_rate*1000)/
						ac->vi.dw_scale;

#ifdef DEBUG
				fprintf(stderr,"\n");
#endif 
				fprintf(stderr,"AVI video info:  ");
				fprintf(stderr,"dw_scale %d  dw_rate %d ", 
					ac->vi.dw_scale, ac->vi.dw_rate);
				fprintf(stderr,"fps %0.3f  ini_frames %d  dw_start %d\n", 
					ac->vi.fps/1000.0, 
					ac->vi.initial_frames,
					ac->vi.dw_start);
				break;	
			case TAG_IT('a','u','d','s'):
				ac->ahandler = getle32(buf+4);
#ifdef DEBUG
				if (ac->vhandler){
					cc = (char *) &ac->ahandler;
					fprintf(stderr,"     audio handler: %c%c%c%c "
						,*cc,*(cc+1),*(cc+2),*(cc+3));
				}
#endif

				if (ac->ntracks == MAX_TRACK) break;
				n = ac->ntracks;
				ac->ai[n].initial_frames = getle32(buf+c);
				c+=4;
				ac->ai[n].dw_scale = getle32(buf+c); 
				c+=4;
				ac->ai[n].dw_rate = getle32(buf+c); 
				c+=4;
				ac->ai[n].dw_start = getle32(buf+c); 
				c+=16;
				ac->ai[n].dw_ssize = getle32(buf+c); 
				if (ac->ai[n].dw_scale)
					ac->ai[n].fps = 
						(ac->ai[n].dw_rate*1000)/
						ac->ai[n].dw_scale;
#ifdef DEBUG
				fprintf(stderr,"\n");
#endif 
				fprintf(stderr,"AVI audio%d info:  ",n);
				fprintf(stderr,"dw_scale %d  dw_rate %d ", 
					ac->ai[n].dw_scale, ac->ai[n].dw_rate);
				fprintf(stderr,"ini_frames %d  dw_start %d",
					ac->ai[n].initial_frames,
					ac->ai[n].dw_start);
				fprintf(stderr,"  fps %0.3f  sam_size %d\n",
					ac->ai[n].fps/1000.,
					ac->ai[n].dw_ssize);
				
				ac->ntracks++;
				break;	
			}
			break;
			
		case TAG_IT('I','N','F','O'):
			size -=4;
			skip =1;
#ifdef DEBUG
			fprintf(stderr,"  size: %d",size);
#endif
			break;

		}
#ifdef DEBUG
		fprintf(stderr,"\n");
#endif

		if (skip){
			lseek(fd, size, SEEK_CUR);
			size = 0;
		}

	}
	
	return -1;
}


#define MAX_BUF_SIZE 0xffff
int get_avi_from_index(pes_in_t *p, int fd, avi_context *ac, 
		       void (*func)(pes_in_t *p), int insize)
{
	struct replex *rx= (struct replex *) p->priv;
	avi_index *idx = ac->idx;
	int cidx = ac->current_idx;
	uint8_t buf[MAX_BUF_SIZE];
	uint32_t cid;
	int c=0;
	off_t pos=0;
	int per = 0;
	static int lastper=0;

	if (cidx > ac->num_idx_frames) return -2;

	switch(idx[cidx].id){
	case TAG_IT('0','1','w','b'):
		p->type = 1;
		p->rbuf = &rx->arbuffer[0];
		break;
		
	case TAG_IT('0','0','d','c'):
		p->type = 0xE0;
		p->rbuf = &rx->vrbuffer;
		break;

	default:
		fprintf(stderr,"strange chunk :\n");
		show_buf((uint8_t *) &idx[cidx].id,4);
		fprintf(stderr,"offset: 0x%04x  length: 0x%04x\n", 
			(int)idx[cidx].off, (int)idx[cidx].len);
		ac->current_idx++;
		p->found=0;
		return 0;
		break;
	}

	memset(buf, 0, MAX_BUF_SIZE);
	pos=lseek (fd, idx[cidx].off+ac->movi_start-4, SEEK_SET);
	read(fd,buf,idx[cidx].len);
	cid = getle32(buf);
	c+=4;
	p->plength = getsize_buf(buf+c);
//	show_buf(buf,16);
	if (idx[cidx].len > insize) return 0;

	if (idx[cidx].len > MAX_BUF_SIZE){
		fprintf(stderr,"Buffer too small in get_avi_from_index\n");
		exit(1);
	}
	if (!idx[cidx].len){
		func(p);
		ac->current_idx++;
		p->found=0;
		return 0;
	}
	if (cid != idx[cidx].id){
		char *cc;
		cc = (char *)&idx[cidx].id;
		fprintf(stderr,"wrong chunk id: %c%c%c%c != %c%c%c%c\n", 
			buf[0],buf[1],buf[2],buf[3]
			,*cc,*(cc+1),*(cc+2),*(cc+3));
		
		print_index(ac,cidx);
		exit(1);
	}
	if (p->plength != idx[cidx].len){
		fprintf(stderr,"wrong chunk size: %d != %d\n", 
			(int)p->plength, idx[cidx].len);
		exit(1);
	}
	c+=4;
	p->done = 1;
	p->ini_pos = ring_wpos(p->rbuf);
	
	per = (int)(100*(pos-ac->movi_start)/ac->movi_length);
	if (per>lastper) fprintf(stderr,"read %3d%%\r", per);
	lastper = per;

	if (ring_write(p->rbuf, buf+c, p->plength)<0){
		fprintf(stderr,	"ring buffer overflow %d 0x%02x\n"
			,p->rbuf->size,p->type);
		exit(1);
	}
	
	func(p);
	init_pes_in(p, 0, NULL, p->withbuf);
	
	ac->current_idx++;

	return 0;
}


void get_avi(pes_in_t *p, uint8_t *buf, int count, void (*func)(pes_in_t *p))
{
	int l;
	int c=0;
	struct replex *rx= (struct replex *) p->priv;


//	show_buf(buf,16);
	while (c < count && p->found < 8
	       &&  !p->done){
		switch ( p->found ){
		case 0:
			if (buf[c] == '0') p->found++;
			else p->found = 0;
			c++;
			break;
		case 1:
			if (buf[c] == '0'|| buf[c] == '1'){
				p->found++;
				p->which = buf[c] - '0';
			} else if (buf[c] == '0'){
				p->found = 1;
			} else p->found = 0;
			c++;
			break;
		case 2:
			switch(buf[c]){
			case 'w':
			case 'd':
				p->found++;
				p->type=buf[c];
				break;
			default:
				p->found = 0;
				break;
			}
			c++;
			break;

		case 3:
			switch(buf[c]){
			case 'b':
				if (p->type == 'w'){
					p->found++;
					p->type = 1;
				} else p->found = 0;
				break;
			case 'c':
				if (p->type == 'd'){
					p->found++;
					p->type = 0xE0;
				} else p->found = 0;
				break;
			default:
				p->found = 0;
				break;
			}
			switch(p->type){

			case 1:
				p->rbuf = &rx->arbuffer[0];
				break;
			
			case 0xE0:
				p->rbuf = &rx->vrbuffer;
				break;
			}
			c++;
			break;

		case 4:
			p->plen[0] = buf[c];
			c++;
			p->found++;
			break;

		case 5:
			p->plen[1] = buf[c];
			c++;
			p->found++;
			break;

		case 6:
			p->plen[2] = buf[c];
			c++;
			p->found++;
			break;

		case 7:
			p->plen[3] = buf[c];
			c++;
			p->found++;
			p->plength = getsize_buf(p->plen);
			if (!p->plength){
				func(p);
				p->found=0;
				break;
			}
			p->done = 1;
			p->ini_pos = ring_wpos(p->rbuf); 
			/*
			if (p->type == 1) fprintf(stderr,"audio 0x%x 0x%x\n",
						  p->plength,ALIGN(p->plength));
			if (p->type == 1) fprintf(stderr,"video 0x%x 0x%x\n",
						  p->plength,ALIGN(p->plength));
			*/
			break;

		default:

			break;
		}
	}
	if (p->done || p->found > 8){
		while (c < count && p->found < p->plength+8){
			l = count -c;
			if (l+p->found > p->plength+8)
				l = p->plength+8-p->found;
			if (ring_write(p->rbuf, buf+c, l)<0){
				fprintf(stderr,	"ring buffer overflow %d\n"
					,p->rbuf->size);
				exit(1);
			}
			p->found += l;
			c += l;
		}			
		if(p->found == p->plength+8){
			func(p);
		}
	}

	if (p->plength && p->found == p->plength+8) {
		int a = 0;//ALIGN(p->plength);
		init_pes_in(p, 0, NULL, p->withbuf);
		if (c+a < count)
			get_avi(p, buf+c+a, count-c-a, func);
	}
}
