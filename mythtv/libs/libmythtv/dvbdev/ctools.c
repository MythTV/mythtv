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

#include "ctools.h"

#define MAX_SEARCH 1024 * 1024


/*

      PES
  
*/

ssize_t save_read(int fd, void *buf, size_t count)
{
	ssize_t neof = 1;
	size_t re = 0;
	
	while(neof >= 0 && re < count){
		neof = read(fd, buf+re, count - re);
		if (neof > 0) re += neof;
		else break;
	}

	if (neof < 0 && re == 0) return neof;
	else return re;
}

void init_pes(pes_packet *p){
	p->stream_id = 0;
	p->llength[0] = 0;
	p->llength[1] = 0;
	p->length = 0;
	p->flags1 = 0x80;
	p->flags2 = 0;
	p->pes_hlength = 0;
	p->trick = 0;
	p->add_cpy = 0;
	p->priv_flags = 0;
	p->pack_field_length = 0;
	p->pack_header = (uint8_t *) NULL;
	p->pck_sqnc_cntr = 0;
	p->org_stuff_length = 0;
	p->pes_ext_lngth = 0;
	p->pes_ext = (uint8_t *) NULL;
	p->pes_pckt_data = (uint8_t *) NULL;
	p->padding = 0;
	p->mpeg = 2; // DEFAULT MPEG2
	p->mpeg1_pad = 0;
	p->mpeg1_headr = NULL;
	p->stuffing = 0;
}

void kill_pes(pes_packet *p){
	if (p->pack_header)
		free(p->pack_header);
	if (p->pes_ext)
		free(p->pes_ext);
	if (p->pes_pckt_data)
		free(p->pes_pckt_data);
	if (p->mpeg1_headr)
		free(p->mpeg1_headr);
	init_pes(p);
}

void setlength_pes(pes_packet *p){
 	short *ll;
	ll = (short *) p->llength;
	p->length = ntohs(*ll);
}

static void setl_pes(pes_packet *p){
	setlength_pes(p);
	if (p->length)
		p->pes_pckt_data = (uint8_t *)malloc(p->length);
}

void nlength_pes(pes_packet *p){
	if (p->length <= 0xFFFF){
		short *ll = (short *) p->llength;
		short l = p->length;
		*ll = htons(l);
	} else {
		p->llength[0] =0x00;
		p->llength[1] =0x00;
	}
}

static void nl_pes(pes_packet *p)
{
	nlength_pes(p);
	p->pes_pckt_data = (uint8_t *) malloc(p->length);
}

void pts2pts(uint8_t *av_pts, uint8_t *pts)
{
  
	av_pts[0] = ((pts[0] & 0x06) << 5) | 
		((pts[1] & 0xFC) >> 2); 
	av_pts[1] = ((pts[1] & 0x03) << 6) |
		((pts[2] & 0xFC) >> 2); 
	av_pts[2] = ((pts[2] & 0x02) << 6) |
			((pts[3] & 0xFE) >> 1);
	av_pts[3] = ((pts[3] & 0x01) << 7) |
		((pts[4] & 0xFE) >> 1);
	
}


int cwrite_pes(uint8_t *buf, pes_packet *p, long length){
	int count,i;
	uint8_t dummy;
	int more = 0;
	uint8_t headr[3] = { 0x00, 0x00 , 0x01};

	if (length <  p->length+p->pes_hlength){
		fprintf(stderr,"Wrong buffer size in cwrite_pes\n");
		exit(1);
	}


	memcpy(buf,headr,3);
	count = 3;
	buf[count] = p->stream_id;
	count++;

	switch ( p->stream_id ) {
				
	case PROG_STREAM_MAP:
	case PRIVATE_STREAM2:
	case PROG_STREAM_DIR:
	case ECM_STREAM     :
	case EMM_STREAM     :
	case PADDING_STREAM :
		buf[count] = p->llength[0];
		count++;
		buf[count] = p->llength[1];
		count++;
		memcpy(buf+count,p->pes_pckt_data,p->length);
		count += p->length;
		break;
	case DSM_CC_STREAM  :
	case ISO13522_STREAM:
	case PRIVATE_STREAM1:
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		buf[count] = p->llength[0];
		count++;
		buf[count] = p->llength[1];
		count++;
		more = 1;
		break;
	}	
	

	if ( more ) {
		if ( p->mpeg == 2 ){
			memcpy(buf+count,&p->flags1,1);
			count++;
			memcpy(buf+count,&p->flags2,1);
			count++;
			memcpy(buf+count,&p->pes_hlength,1);
			count++;
			
			if ((p->flags2 & PTS_DTS_FLAGS) == PTS_ONLY){
				memcpy(buf+count,p->pts,5);
				count += 5;
			} else 
				if ((p->flags2 & PTS_DTS_FLAGS) == PTS_DTS){
					memcpy(buf+count,p->pts,5);
					count += 5;
					memcpy(buf+count,p->dts,5);
					count += 5;
				}
			if (p->flags2 & ESCR_FLAG){
				memcpy(buf+count,p->escr,6);
				count += 6;
			}
			if (p->flags2 & ES_RATE_FLAG){
				memcpy(buf+count,p->es_rate,3);
				count += 3;
			}
			if (p->flags2 & DSM_TRICK_FLAG){
				memcpy(buf+count,&p->trick,1);
				count++;
			}
			if (p->flags2 & ADD_CPY_FLAG){
				memcpy(buf+count,&p->add_cpy,1);
				count++;
			}
			if (p->flags2 & PES_CRC_FLAG){
				memcpy(buf+count,p->prev_pes_crc,2);
				count += 2;
			}
			if (p->flags2 & PES_EXT_FLAG){
				memcpy(buf+count,&p->priv_flags,1);
				count++;

				if (p->priv_flags & PRIVATE_DATA){
					memcpy(buf+count,p->pes_priv_data,16);
					count += 16;
				}
				if (p->priv_flags & HEADER_FIELD){
					memcpy(buf+count,&p->pack_field_length,
					       1);
					count++;
					memcpy(buf+count,p->pack_header,
						     p->pack_field_length);
					count += p->pack_field_length;

				}
				
				if ( p->priv_flags & PACK_SEQ_CTR){
					memcpy(buf+count,&p->pck_sqnc_cntr,1);
					count++;
					memcpy(buf+count,&p->org_stuff_length,
					       1);
					count++;
				}
				
				if ( p->priv_flags & P_STD_BUFFER){
					memcpy(buf+count,p->p_std,2);
					count += 2;
				}
				if ( p->priv_flags & PES_EXT_FLAG2){
					memcpy(buf+count,&p->pes_ext_lngth,1);
					count++;
					memcpy(buf+count,p->pes_ext,
						     p->pes_ext_lngth);
					count += p->pes_ext_lngth;
				}
			}
			dummy = 0xFF;
			for (i=0;i<p->stuffing;i++) {
				memcpy(buf+count,&dummy,1);
				count++;
			}
		} else {
			if (p->mpeg1_pad){
				memcpy(buf+count,p->mpeg1_headr,p->mpeg1_pad);
				count += p->mpeg1_pad;
			}
			if ((p->flags2 & PTS_DTS_FLAGS) == PTS_ONLY){
				memcpy(buf+count,p->pts,5);
				count += 5;
			}
			else if ((p->flags2 & PTS_DTS_FLAGS) == 
				 PTS_DTS){
				memcpy(buf+count,p->pts,5);
					count += 5;
				memcpy(buf+count,p->dts,5);
					count += 5;
			}
		}			
		memcpy(buf+count,p->pes_pckt_data,p->length);
		count += p->length;
	}

	return count;

}

void write_pes(int fd, pes_packet *p){
	long length;
	uint8_t *buf;
	int l = p->length+p->pes_hlength;
	
	buf = (uint8_t *) malloc(l);
	length = cwrite_pes(buf,p,l);
	write(fd,buf,length);
	free(buf);
}

static unsigned int find_length(int f){
	uint64_t p = 0;
	uint64_t start = 0;
	uint64_t q = 0;
	int found = 0;
	uint8_t sync4[4];
	int neof = 1;

	start = lseek(f,0,SEEK_CUR);
	start -=2;
        lseek(f,start,SEEK_SET);
	while ( neof > 0 && !found ){
		p = lseek(f,0,SEEK_CUR);
		neof = save_read(f,&sync4,4);
		if (sync4[0] == 0x00 && sync4[1] == 0x00 && sync4[2] == 0x01) {
			switch ( sync4[3] ) {
				
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
			case PRIVATE_STREAM1:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
				found = 1;
				break;
			default:
				q = lseek(f,0,SEEK_CUR);
				break;
			}	
		} 
	}
	q = lseek(f,0,SEEK_CUR);
	lseek(f,start+2,SEEK_SET);
	if (found) return (unsigned int)(q-start)-4-2;
	else return (unsigned int)(q-start-2);
	
}


void cread_pes(char *buf, pes_packet *p){
	
	uint8_t count, dummy, check;
	int i;
	uint64_t po = 0;
	int c=0;

	switch ( p->stream_id ) {
		
	case PROG_STREAM_MAP:
	case PRIVATE_STREAM2:
	case PROG_STREAM_DIR:
	case ECM_STREAM     :
	case EMM_STREAM     :
		memcpy(p->pes_pckt_data,buf+c,p->length);
		return;
		break;
	case PADDING_STREAM :
		p->padding = p->length;
		memcpy(p->pes_pckt_data,buf+c,p->length);
		return;
		break;			
	case DSM_CC_STREAM  :
	case ISO13522_STREAM:
	case PRIVATE_STREAM1:
	case AUDIO_STREAM_S ... AUDIO_STREAM_E:
	case VIDEO_STREAM_S ... VIDEO_STREAM_E:
		break;
	default:
		return;
		break;
	}	
	
	po = c;
	memcpy(&p->flags1,buf+c,1);
	c++;
	if ( (p->flags1 & 0xC0) == 0x80 ) p->mpeg = 2;
	else p->mpeg = 1;
	
	if ( p->mpeg == 2 ){
		memcpy(&p->flags2,buf+c,1);
		c++;
		memcpy(&p->pes_hlength,buf+c,1);
		c++;
		
		p->length -=p->pes_hlength+3;
		count = p->pes_hlength;
		
		if ((p->flags2 & PTS_DTS_FLAGS) == PTS_ONLY){
			memcpy(p->pts,buf+c,5);
			c += 5;
			count -=5;
		} else 
			if ((p->flags2 & PTS_DTS_FLAGS) == PTS_DTS){
				memcpy(p->pts,buf+c,5);
				c += 5;
				memcpy(p->dts,buf+c,5);
				c += 5;
				count -= 10;
			}
		
		if (p->flags2 & ESCR_FLAG){
			memcpy(p->escr,buf+c,6);
			c += 6;
			count -= 6;
		}
		
		if (p->flags2 & ES_RATE_FLAG){
			memcpy(p->es_rate,buf+c,3);
			c += 3;
			count -= 3;
		}

		if (p->flags2 & DSM_TRICK_FLAG){
			memcpy(&p->trick,buf+c,1);
			c += 1;
			count -= 1;
		}
		
		if (p->flags2 & ADD_CPY_FLAG){
			memcpy(&p->add_cpy,buf+c,1);
			c++;
			count -= 1;
		}
		
		if (p->flags2 & PES_CRC_FLAG){
			memcpy(p->prev_pes_crc,buf+c,2);
			c += 2;
			count -= 2;
		}			
		
		if (p->flags2 & PES_EXT_FLAG){
			memcpy(&p->priv_flags,buf+c,1);
			c++;
			count -= 1;
			
			if (p->priv_flags & PRIVATE_DATA){
				memcpy(p->pes_priv_data,buf+c,16);
				c += 16;
				count -= 16;
			}
			
			if (p->priv_flags & HEADER_FIELD){
				memcpy(&p->pack_field_length,buf+c,1);
				c++;
				p->pack_header = (uint8_t *)
					malloc(p->pack_field_length);
				memcpy(p->pack_header,buf+c,
				       p->pack_field_length);
				c += p->pack_field_length;
				count -= 1+p->pack_field_length;
			}
			
			if ( p->priv_flags & PACK_SEQ_CTR){
				memcpy(&p->pck_sqnc_cntr,buf+c,1);
				c++;
				memcpy(&p->org_stuff_length,buf+c,1);
				c++;
				count -= 2;
			}
			
			if ( p->priv_flags & P_STD_BUFFER){
				memcpy(p->p_std,buf+c,2);
				c += 2;
				count -= 2;
			}

			if ( p->priv_flags & PES_EXT_FLAG2){
				memcpy(&p->pes_ext_lngth,buf+c,1);
				c++;
				p->pes_ext = (uint8_t *)
					malloc(p->pes_ext_lngth);
				memcpy(p->pes_ext,buf+c,
				       p->pes_ext_lngth);
				c += p->pes_ext_lngth;
				count -= 1+p->pes_ext_lngth;
			}
		}
		p->stuffing = count;
		for(i = 0; i< count ;i++){ 
			memcpy(&dummy,buf+c,1);
			c++;
		}
	} else {
		p->mpeg1_pad = 1;
 		check = p->flags1;
		while (check == 0xFF){
			memcpy(&check,buf+c,1);
			c++;
			p->mpeg1_pad++;
		}
		
		if ( (check & 0xC0) == 0x40){
			memcpy(&check,buf+c,1);
			c++;
			p->mpeg1_pad++;
			memcpy(&check,buf+c,1);
			c++;
			p->mpeg1_pad++;
		}
		p->flags2 = 0;
		p->length -= p->mpeg1_pad;
		
		c = po;
		if ( (check & 0x30)){
			p->length ++;
			p->mpeg1_pad --;
			
			if (check == p->flags1){
				p->pes_hlength = 0;
			} else {
				p->mpeg1_headr = (uint8_t *)
					malloc(p->mpeg1_pad);
				p->pes_hlength = p->mpeg1_pad;
				memcpy(p->mpeg1_headr,buf+c,
				       p->mpeg1_pad);
				c += p->mpeg1_pad;
			}
			
			p->flags2 = (check & 0xF0) << 2;
			if ((p->flags2 & PTS_DTS_FLAGS) == PTS_ONLY){
				memcpy(p->pts,buf+c,5);
				c += 5;
				p->length -= 5;
				p->pes_hlength += 5;
			}
			else if ((p->flags2 & PTS_DTS_FLAGS) == 
				 PTS_DTS){
				memcpy(p->pts,buf+c,5);
				c += 5;
				memcpy(p->dts,buf+c,5);
				c += 5;
				p->length -= 10;
				p->pes_hlength += 10;
			}
		} else {
			p->mpeg1_headr = (uint8_t *) malloc(p->mpeg1_pad);
			p->pes_hlength = p->mpeg1_pad;
			memcpy(p->mpeg1_headr,buf+c,
			       p->mpeg1_pad);
			c += p->mpeg1_pad;
		}
	}
	memcpy(p->pes_pckt_data,buf+c,p->length);
}


int read_pes(int f, pes_packet *p){
	
	uint8_t sync4[4];
	int found=0;
	uint64_t po = 0;
	int neof = 1;
	uint8_t *buf;

	while (neof > 0 && !found) {
	        po = lseek(f,0,SEEK_CUR);
		if (po < 0) return -1;
		if ((neof = save_read(f,&sync4,4)) < 4) return -1;
		if (sync4[0] == 0x00 && sync4[1] == 0x00 && sync4[2] == 0x01) {
			p->stream_id = sync4[3];
			switch ( sync4[3] ) {
				
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
			case PRIVATE_STREAM1:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
				if((neof = save_read(f,p->llength,2)) < 2)
					return -1;
				setl_pes(p);
				if (!p->length){ 
					p->length = find_length(f);
					nl_pes(p);
				}
				found = 1;
				break;
				
			default:
				if (lseek(f,po+1,SEEK_SET) < po+1) return -1;
			break;
			}	
		} else if(lseek(f,po+1,SEEK_SET) < po+1) return -1;
	}

	if (!found || !p->length) return 0;
	
	if (p->length >0){
		buf = (uint8_t *) malloc(p->length);
		if((neof = save_read(f,buf,p->length))< p->length) return -1;
		cread_pes((char *)buf,p);
		free(buf);
	} else return 0;

	return neof;
}

/*

   Transport Stream

*/

void init_ts(ts_packet *p){
	p->pid[0] = 0;
	p->pid[1] = 0;
	p->flags = 0;
	p->count = 0;
	p->adapt_length = 0;
	p->adapt_flags = 0;
	p->splice_count = 0;
	p->priv_dat_len = 0;
	p->priv_dat = NULL;
	p->adapt_ext_len = 0;
	p->adapt_eflags = 0;
	p->rest = 0;
	p->stuffing = 0;
}

void kill_ts(ts_packet *p){
	if (p->priv_dat)
		free(p->priv_dat);
	init_ts(p);
}



unsigned short pid_ts(ts_packet *p)
{
  return get_pid(p->pid);
}

int cwrite_ts(uint8_t *buf, ts_packet *p, long length){
	long count,i;
	uint8_t sync,dummy;

	sync = 0x47;
	memcpy(buf,&sync,1);
	count = 1;
	memcpy(buf+count,p->pid,2);
	count += 2;
	memcpy(buf+count,&p->flags,1);
	count++;

	 
	if (! (p->flags & ADAPT_FIELD) && (p->flags & PAYLOAD)){
		memcpy(buf+count,p->data,184);
		count += 184;
	} else {
		memcpy(buf+count,&p->adapt_length,1);
		count++;
		memcpy(buf+count,&p->adapt_flags,1);
		count++;

		if ( p->adapt_flags & PCR_FLAG ){
			memcpy(buf+count, p->pcr,6);
			count += 6;
		}
		if ( p->adapt_flags & OPCR_FLAG ){
			memcpy(buf+count, p->opcr,6);
			count += 6;
		}
		if ( p->adapt_flags & SPLICE_FLAG ){
			memcpy(buf+count, &p->splice_count,1);
			count++;
		}
		if( p->adapt_flags & TRANS_PRIV){
			memcpy(buf+count,&p->priv_dat_len,1);
			count++;
			memcpy(buf+count,p->priv_dat,p->priv_dat_len);
			count += p->priv_dat_len;
		}
			
		if( p->adapt_flags & ADAP_EXT_FLAG){
			memcpy(buf+count,&p->adapt_ext_len,1);
			count++;
			memcpy(buf+count,&p->adapt_eflags,1);
			count++;
		
			if( p->adapt_eflags & LTW_FLAG){
				memcpy(buf+count,p->ltw,2);
				count += 2;
			}
			if( p->adapt_eflags & PIECE_RATE){
				memcpy(buf+count,p->piece_rate,3);
				count += 3;
			}
			if( p->adapt_eflags & SEAM_SPLICE){
				memcpy(buf+count,p->dts,5);
				count += 5;
			}
		}
		dummy = 0xFF;
		for(i=0; i < p->stuffing ; i++){
			memcpy(buf+count,&dummy,1);
			count++;
		}
		if (p->flags & PAYLOAD){
			memcpy(buf+count,p->data,p->rest);
			count += p->rest;
		}
	} 


	return count;
}

void write_ts(int fd, ts_packet *p){
	long length;
	uint8_t buf[TS_SIZE];

	length = cwrite_ts(buf,p,TS_SIZE);
	write(fd,buf,length);
}

int read_ts (int f, ts_packet *p){
	uint8_t sync;
	int found=0;
	uint64_t po,q;
	int neof = 1;

	sync=0;
	while (neof > 0 && !found) {
		neof = save_read(f,&sync,1);
		if (sync == 0x47) 
			found = 1;
	}
	neof = save_read(f,p->pid,2);
	neof = save_read(f,&p->flags,1);
	p->count = p->flags & COUNT_MASK;
	 
	if (!(p->flags & ADAPT_FIELD) && (p->flags & PAYLOAD)){
		//no adapt. field only payload
		neof = save_read(f,p->data,184);
		p->rest = 184;
		return neof;
	} 

	if ( p->flags & ADAPT_FIELD ) {
		// adaption field
		neof = save_read(f,&p->adapt_length,1);
		po = lseek(f,0,SEEK_CUR);
		neof = save_read(f,&p->adapt_flags,1);

		if ( p->adapt_flags & PCR_FLAG )
			neof = save_read(f, p->pcr,6);

		if ( p->adapt_flags & OPCR_FLAG )
			neof = save_read(f, p->opcr,6);

		if ( p->adapt_flags & SPLICE_FLAG )
			neof = save_read(f, &p->splice_count,1);

		if( p->adapt_flags & TRANS_PRIV){
			neof = save_read(f,&p->priv_dat_len,1);
			p->priv_dat = (uint8_t *) malloc(p->priv_dat_len);
			neof = save_read(f,p->priv_dat,p->priv_dat_len);
		}
			
		if( p->adapt_flags & ADAP_EXT_FLAG){
			neof = save_read(f,&p->adapt_ext_len,1);
			neof = save_read(f,&p->adapt_eflags,1);
			if( p->adapt_eflags & LTW_FLAG)
				neof = save_read(f,p->ltw,2);
			
			if( p->adapt_eflags & PIECE_RATE)
				neof = save_read(f,p->piece_rate,3);
			
			if( p->adapt_eflags & SEAM_SPLICE)
				neof = save_read(f,p->dts,5);
		}
		q = lseek(f,0,SEEK_CUR);
		p->stuffing = p->adapt_length -(q-po);
		p->rest = 183-p->adapt_length;
		lseek(f,q+p->stuffing,SEEK_SET);
		if (p->flags & PAYLOAD) // payload
			neof = save_read(f,p->data,p->rest);
		else 
			lseek(f,q+p->rest,SEEK_SET);
	}
	return neof;
}

void cread_ts (char *buf, ts_packet *p, long length){
	uint8_t sync;
	int found=0;
	uint64_t po,q;
	long count=0;
	
	sync=0;
	while (count < length  && !found) {
		sync=buf[count];
		count++;
		if (sync == 0x47) 
			found = 1;
	}
	memcpy(p->pid,buf+count,2);
	count += 2;
	p->flags = buf[count];
	count++;
	p->count = p->flags & COUNT_MASK;
	 
	if (!(p->flags & ADAPT_FIELD) && (p->flags & PAYLOAD)){
		//no adapt. field only payload
		memcpy(p->data,buf+count,184);
		p->rest = 184;
		return;
	} 

	if ( p->flags & ADAPT_FIELD ) {
		// adaption field
		p->adapt_length = buf[count];
		count++;
		po = count;
		memcpy(&p->adapt_flags,buf+count,1);
		count++;

		if ( p->adapt_flags & PCR_FLAG ){
			memcpy( p->pcr,buf+count,6);
			count += 6;
		}
		if ( p->adapt_flags & OPCR_FLAG ){
			memcpy( p->opcr,buf+count,6);
			count += 6;
		}
		if ( p->adapt_flags & SPLICE_FLAG ){
			memcpy( &p->splice_count,buf+count,1);
			count++;
		}
		if( p->adapt_flags & TRANS_PRIV){
			memcpy(&p->priv_dat_len,buf+count,1);
			count++;
			p->priv_dat = (uint8_t *) malloc(p->priv_dat_len);
			memcpy(p->priv_dat,buf+count,p->priv_dat_len);
			count += p->priv_dat_len;
		}
			
		if( p->adapt_flags & ADAP_EXT_FLAG){
			memcpy(&p->adapt_ext_len,buf+count,1);
			count++;
			memcpy(&p->adapt_eflags,buf+count,1);
			count++;
			if( p->adapt_eflags & LTW_FLAG){
				memcpy(p->ltw,buf+count,2);
				count += 2;
			}
			if( p->adapt_eflags & PIECE_RATE){
				memcpy(p->piece_rate,buf+count,3);
				count += 3;
			}
			if( p->adapt_eflags & SEAM_SPLICE){
				memcpy(p->dts,buf+count,5);
				count += 5;
			}
		}
		q = count;
		p->stuffing = p->adapt_length -(q-po);
		p->rest = 183-p->adapt_length;
		count = q+p->stuffing;
		if (p->flags & PAYLOAD){ // payload
			memcpy(p->data,buf+count,p->rest);
			count += p->rest;
		} else 
			count = q+p->rest;
	}
}


/*

   Program Stream

*/


void init_ps(ps_packet *p)
{
	p->stuff_length=0xF8;
	p->data = NULL;
	p->sheader_length = 0;
	p->audio_bound = 0;
	p->video_bound = 0;
	p->npes = 0;
	p->mpeg = 2;
}

void kill_ps(ps_packet *p)
{
	if (p->data)
		free(p->data);
	init_ps(p);
}

void setlength_ps(ps_packet *p)
{
	short *ll;
	ll = (short *) p->sheader_llength;
	if (p->mpeg == 2)
		p->sheader_length = ntohs(*ll) - 6;
	else 
		p->sheader_length = ntohs(*ll);
}	

static void setl_ps(ps_packet *p)
{
	setlength_ps(p);
	p->data = (uint8_t *) malloc(p->sheader_length);
}

int mux_ps(ps_packet *p)
{
	uint32_t mux = 0;
	uint8_t *i = (uint8_t *)&mux;

	i[1] = p->mux_rate[0];
	i[2] = p->mux_rate[1];
	i[3] = p->mux_rate[2];
	mux = ntohl(mux);
	mux = (mux >>2);
	return mux;
}

int rate_ps(ps_packet *p)
{
	uint32_t rate=0;
	uint8_t *i= (uint8_t *) &rate;

	i[1] = p->rate_bound[0] & 0x7F;
	i[2] = p->rate_bound[1];
	i[3] = p->rate_bound[2];
	
	rate = ntohl(rate);
	rate = (rate >> 1);
	return rate;
}


uint32_t scr_base_ps(ps_packet *p) // only 32 bit!!
{
	uint32_t base = 0;
	uint8_t *buf = (uint8_t *)&base;
	
	buf[0] |= (long int)((p->scr[0] & 0x18) << 3);
	buf[0] |= (long int)((p->scr[0] & 0x03) << 4);
	buf[0] |= (long int)((p->scr[1] & 0xF0) >> 4);
		 
	buf[1] |= (long int)((p->scr[1] & 0x0F) << 4);
	buf[1] |= (long int)((p->scr[2] & 0xF0) >> 4);

	buf[2] |= (long int)((p->scr[2] & 0x08) << 4);
	buf[2] |= (long int)((p->scr[2] & 0x03) << 5);
	buf[2] |= (long int)((p->scr[3] & 0xF8) >> 3);

	buf[3] |= (long int)((p->scr[3] & 0x07) << 5);
	buf[3] |= (long int)((p->scr[4] & 0xF8) >> 3);

	base = ntohl(base);
	return base;
}

uint16_t scr_ext_ps(ps_packet *p)
{
	short ext = 0;

	ext = (short)(p->scr[5] >> 1);
	ext += (short) (p->scr[4] &  0x03) * 128;

	return ext;
}

int cwrite_ps(uint8_t *buf, ps_packet *p, long length)
{
	long count,i;
	uint8_t headr1[4] = {0x00, 0x00, 0x01, 0xBA };
	uint8_t headr2[4] = {0x00, 0x00, 0x01, 0xBB };
	uint8_t buffy = 0xFF;

	
	memcpy(buf,headr1,4);
	count = 4;
	if (p->mpeg == 2){
		memcpy(buf+count,p->scr,6);
		count += 6;
		memcpy(buf+count,p->mux_rate,3);
		count += 3;
		memcpy(buf+count,&p->stuff_length,1);
		count++;
		for(i=0; i< (p->stuff_length & 3); i++){
			memcpy(buf+count,&buffy,1);
			count++;
		}
	} else {
		memcpy(buf+count,p->scr,5);
		count += 5;
		memcpy(buf+count,p->mux_rate,3);
		count += 3;
	}
	if (p->sheader_length){
		memcpy(buf+count,headr2,4);
		count += 4;
		memcpy(buf+count,p->sheader_llength,2);
		count += 2;
		if ( p->mpeg == 2){
			memcpy(buf+count,p->rate_bound,3);
			count += 3;
			memcpy(buf+count,&p->audio_bound,1);
			count++;
			memcpy(buf+count,&p->video_bound,1);
			count++;
			memcpy(buf+count,&p->reserved,1);
			count++;
		}
		memcpy(buf+count,p->data,p->sheader_length);
		count += p->sheader_length;
	}

	return count;
}

void write_ps(int fd, ps_packet *p){
	long length;
	uint8_t buf[PS_MAX];

	length = cwrite_ps(buf,p,PS_MAX);
	write(fd,buf,length);
}

int read_ps (int f, ps_packet *p){
	uint8_t headr[4];
	pes_packet pes;
	int i,done;
	int found=0;
	uint64_t po = 0;
	uint64_t q = 0;
	long count = 0;
	int neof = 1;

	po = lseek(f,0,SEEK_CUR);
	while (neof > 0 && !found && count < MAX_SEARCH) {
		neof = save_read(f,&headr,4);
		if (headr[0] == 0x00 && headr[1] == 0x00 && headr[2] == 0x01){
			if ( headr[3] == 0xBA ) 
				found = 1;
			else 
				if ( headr[3] == 0xB9 ) break;
				else lseek(f,po+1,SEEK_SET);
		}
		count++;
	}
	
	if (found){
		neof = save_read(f,p->scr,6);
		if (p->scr[0] & 0x40)
			p->mpeg = 2;
		else
			p->mpeg = 1;

		if (p->mpeg == 2){
			neof = save_read(f,p->mux_rate,3);
			neof = save_read(f,&p->stuff_length,1);
			po = lseek(f,0,SEEK_CUR);
			lseek(f,po+(p->stuff_length & 3),SEEK_SET);
		} else {
			p->mux_rate[0] = p->scr[5]; //mpeg1 scr is only 5 bytes
			neof = save_read(f,p->mux_rate+1,2);
		}
			
		po = lseek(f,0,SEEK_CUR);
		neof = save_read(f,headr,4);
		if (headr[0] == 0x00 && headr[1] == 0x00 && 
		    headr[2] == 0x01 && headr[3] == 0xBB ) {
			neof = save_read(f,p->sheader_llength,2);
			setl_ps(p);
			if (p->mpeg == 2){
				neof = save_read(f,p->rate_bound,3);
				neof = save_read(f,&p->audio_bound,1);
				neof = save_read(f,&p->video_bound,1);
				neof = save_read(f,&p->reserved,1);
			}
			neof = save_read(f,p->data,p->sheader_length);
		} else {
			lseek(f,po,SEEK_SET);
			p->sheader_length = 0;
		}

		i = 0;
		done = 0;
		q = lseek(f,0,SEEK_CUR);
		do {
			po = lseek(f,0,SEEK_CUR);
			neof = save_read(f,headr,4);
			lseek(f,po,SEEK_SET);
			if ( headr[0] == 0x00 && headr[1] == 0x00 
			     && headr[2] == 0x01 && headr[3] != 0xBA){
				init_pes(&pes);
				neof = read_pes(f,&pes);
			        i++;
			} else done = 1;
			kill_pes(&pes);
		} while ( neof > 0 && !done);
		p->npes = i;
		lseek(f,q,SEEK_SET);
	} 
	return neof;
}

void cread_ps (char *buf, ps_packet *p, long length){
	uint8_t *headr;
	pes_packet pes;
	int i,done;
	int found=0;
	uint64_t po = 0;
	uint64_t q = 0;
	long count = 0;
	long c = 0;
	
	po = c;
	while ( count < length && !found && count < MAX_SEARCH) {
		headr = (uint8_t *)buf+c;
		c += 4;
		if (headr[0] == 0x00 && headr[1] == 0x00 && headr[2] == 0x01){
			if ( headr[3] == 0xBA ) 
				found = 1;
			else 
				if ( headr[3] == 0xB9 ) break;
				else c = po+1;
		}
		count++;
	}
	
	if (found){
		memcpy(p->scr,buf+c,6);
		c += 6;
		if (p->scr[0] & 0x40)
			p->mpeg = 2;
		else
			p->mpeg = 1;

		if (p->mpeg == 2){
			memcpy(p->mux_rate,buf+c,3);
			c += 3;
			memcpy(&p->stuff_length,buf+c,1);
			c++;
			po = c;
			c = po+(p->stuff_length & 3);
		} else {
			p->mux_rate[0] = p->scr[5]; //mpeg1 scr is only 5 bytes
			memcpy(p->mux_rate+1,buf+c,2);
			c += 2;
		}
			
		po = c;
		headr = (uint8_t *)buf+c;
		c += 4;
		if (headr[0] == 0x00 && headr[1] == 0x00 && 
		    headr[2] == 0x01 && headr[3] == 0xBB ) {
			memcpy(p->sheader_llength,buf+c,2);
			c += 2;
			setl_ps(p);
			if (p->mpeg == 2){
				memcpy(p->rate_bound,buf+c,3);
				c += 3;
				memcpy(&p->audio_bound,buf+c,1);
				c++;
				memcpy(&p->video_bound,buf+c,1);
				c++;
				memcpy(&p->reserved,buf+c,1);
				c++;
			}
			memcpy(p->data,buf+c,p->sheader_length);
			c += p->sheader_length;
		} else {
			c = po;
			p->sheader_length = 0;
		}

		i = 0;
		done = 0;
		q = c;
		do {
			headr = (uint8_t *)buf+c;
			if ( headr[0] == 0x00 && headr[1] == 0x00 
			     && headr[2] == 0x01 && headr[3] != 0xBA){
				init_pes(&pes);
				//	cread_pes(buf+c,&pes);
			        i++;
			} else done = 1;
			kill_pes(&pes);
		} while (c < length && !done);
		p->npes = i;
		c = q;
	} 
}







/*
  conversion
*/

void init_trans(trans *p)
{
	int i;

	p->found = 0;
	p->pes = 0;
	p->is_full = 0;
	p->pes_start = 0;
	p->pes_started = 0;
	p->set = 0;

	for (i = 0; i < MASKL*MAXFILT ; i++){
		p->mask[i] = 0;
		p->filt[i] = 0;
	}
	for (i = 0; i < MAXFILT ; i++){
		p->sec[i].found = 0;
		p->sec[i].length = 0;
	}	
}

int set_trans_filt(trans *p, int filtn, uint16_t pid, uint8_t *mask, uint8_t *filt, int pes)
{
	int i;
	int off;

	if ( filtn > MAXFILT-1 || filtn<0 ) return -1;
	p->pid[filtn] = pid;
	if (pes) p->pes |= (tflags)(1 << filtn);
	else {
		off = MASKL*filtn;
		p->pes &= ~((tflags) (1 << filtn) );
		for (i = 0; i < MASKL ; i++){
			p->mask[off+i] = mask[i];
			p->filt[off+i] = filt[i];
		}
	}		
	p->set |= (tflags) (1 << filtn);
	return 0;
}

void clear_trans_filt(trans *p,int filtn)
{
	int i;

	p->set &= ~((tflags) (1 << filtn) );
	p->pes &= ~((tflags) (1 << filtn) );
	p->is_full &= ~((tflags) (1 << filtn) );
	p->pes_start &= ~((tflags) (1 << filtn) );
	p->pes_started &= ~((tflags) (1 << filtn) );

	for (i = MASKL*filtn; i < MASKL*(filtn+1) ; i++){
		p->mask[i] = 0;
		p->filt[i] = 0;
	}
	p->sec[filtn].found = 0;
	p->sec[filtn].length = 0;
}

int filt_is_set(trans *p, int filtn)
{
	if (p->set & ((tflags)(1 << filtn))) return 1;
	return 0;
}

int pes_is_set(trans *p, int filtn)
{
	if (p->pes & ((tflags)(1 << filtn))) return 1;
	return 0;
}

int pes_is_started(trans *p, int filtn)
{
	if (p->pes_started & ((tflags)(1 << filtn))) return 1;
	return 0;
}

int pes_is_start(trans *p, int filtn)
{
	if (p->pes_start & ((tflags)(1 << filtn))) return 1;
	return 0;
}

int filt_is_ready(trans *p,int filtn)
{
	if (p->is_full & ((tflags)(1 << filtn))) return 1;
	return 0;
}

void trans_filt(uint8_t *buf, int count, trans *p)
{
	int c=0;
	//fprintf(stderr,"trans_filt\n");
	

	while (c < count && p->found <1 ){
		if ( buf[c] == 0x47) p->found = 1;
		c++;
		p->packet[0] = 0x47;
	}
	if (c == count) return;
	
	while( c < count && p->found < 188 && p->found > 0 ){
		p->packet[p->found] = buf[c];
		c++;
		p->found++;
	}
	if (p->found == 188){
		p->found = 0;
		tfilter(p);
	}

	if (c < count) trans_filt(buf+c,count-c,p);
} 


void tfilter(trans *p)
{
	int l,c;
	int tpid;
	uint8_t flag,flags;
	uint8_t adapt_length = 0;
	uint8_t cpid[2];


	//	fprintf(stderr,"tfilter\n");

	cpid[0] = p->packet[1];
	cpid[1] = p->packet[2];
	tpid = get_pid(cpid);

	if ( p->packet[1]&0x80){
		fprintf(stderr,"Error in TS for PID: %d\n", 
			tpid);
	}

	flag = cpid[0];
	flags = p->packet[3];
	
	if ( flags & ADAPT_FIELD ) {
		// adaption field
		adapt_length = p->packet[4];
	}

	c = 5 + adapt_length - (int)(!(flags & ADAPT_FIELD));
	if (flags & PAYLOAD){
		for ( l = 0; l < MAXFILT ; l++){
			if ( filt_is_set(p,l) ) {
				if ( p->pid[l] == tpid) {
					if ( pes_is_set(p,l) ){
						if (cpid[0] & PAY_START){
							p->pes_started |= 
								(tflags) 
								(1 << l);
							p->pes_start |= 
								(tflags) 
								(1 << l);
						} else {
							p->pes_start &= ~ 
								((tflags) 
								(1 << l));
						}
						pes_filter(p,l,c);
					} else {
						sec_filter(p,l,c);
					}	
				}
			}
		}
	}
}	


void pes_filter(trans *p, int filtn, int off)
{
	int count,c;
	uint8_t *buf;

	if (filtn < 0 || filtn >= MAXFILT) return; 

	count = 188 - off;
	c = 188*filtn;
	buf = p->packet+off;
	if (pes_is_started(p,filtn)){
		p->is_full |= (tflags) (1 << filtn);
		memcpy(p->transbuf+c,buf,count);
		p->transcount[filtn] = count;
	}
}

section *get_filt_sec(trans *p, int filtn)
{
	section *sec;
	
	sec = &p->sec[filtn];
	p->is_full &= ~((tflags) (1 << filtn) );
	return sec;
}

int get_filt_buf(trans *p, int filtn,uint8_t **buf)
{
	*buf = p->transbuf+188*filtn;
	p->is_full &= ~((tflags) (1 << filtn) );
	return p->transcount[filtn];
}




void sec_filter(trans *p, int filtn, int off)
{
	int i,j;
	int error;
	int count,c;
	uint8_t *buf, *secbuf;
	section *sec;

	//	fprintf(stderr,"sec_filter\n");

	if (filtn < 0 || filtn >= MAXFILT) return; 

	count = 188 - off;
	c = 0;
	buf = p->packet+off;
	sec = &p->sec[filtn];
	secbuf = sec->payload;
	if(!filt_is_ready(p,filtn)){
		p->is_full &= ~((tflags) (1 << filtn) );
		sec->found = 0;
		sec->length = 0;
	}
		
	if ( !sec->found ){
		c = buf[c]+1;
		if (c >= count) return;
		sec->id = buf[c];
		secbuf[0] = buf[c];
		c++;
		sec->found++;
		sec->length = 0;
	}
	
	while ( c < count && sec->found < 3){
		secbuf[sec->found] = buf[c];
		c++;
		sec->found++;
	}
	if (c == count) return;
	
	if (!sec->length && sec->found == 3){
		sec->length |= ((secbuf[1] & 0x0F) << 8); 
		sec->length |= (secbuf[2] & 0xFF);
	}
	
	while ( c < count && sec->found < sec->length+3){
		secbuf[sec->found] = buf[c];
		c++;
		sec->found++;
	}

	if ( sec->length && sec->found == sec->length+3 ){
		error=0;
		for ( i = 0; i < MASKL; i++){
			if (i > 0 ) j=2+i;
			else j = 0;
			error += (sec->payload[j]&p->mask[MASKL*filtn+i])^
				(p->filt[MASKL*filtn+i]&
				 p->mask[MASKL*filtn+i]);
		}
		if (!error){
			p->is_full |= (tflags) (1 << filtn);
		}
		if (buf[0]+1 < c ) c=count;
	}
	
	if ( c < count ) sec_filter(p, filtn, off);

}

#define MULT 1024


void write_ps_headr( ps_packet *p, uint8_t *pts,int fd)
{
	long  muxr = 37500;
	uint8_t    audio_bound = 1;
	uint8_t    fixed = 0;
	uint8_t    CSPS = 0;
	uint8_t    audio_lock = 1;
	uint8_t    video_lock = 1;
	uint8_t    video_bound = 1;
	uint8_t    stream1 = 0XC0;
	uint8_t    buffer1_scale = 1;
	uint32_t   buffer1_size = 32;
	uint8_t    stream2 = 0xE0;
	uint8_t    buffer2_scale = 1;
	uint32_t   buffer2_size = 230;
                    
	init_ps(p);
	
	p->mpeg = 2;
// SCR = 0
	p->scr[0] = 0x44;
	p->scr[1] = 0x00;
	p->scr[2] = 0x04;
	p->scr[3] = 0x00;
	p->scr[4] = 0x04;
	p->scr[5] = 0x01;
	
// SCR = PTS
	p->scr[0] = 0x44 | ((pts[0] >> 3)&0x18) | ((pts[0] >> 4)&0x03);
	p->scr[1] = 0x00 | ((pts[0] << 4)&0xF0) | ((pts[1] >> 4)&0x0F);
	p->scr[2] = 0x04 | ((pts[1] << 4)&0xF0) | ((pts[2] >> 4)&0x08)
		| ((pts[2] >> 5)&0x03);
	p->scr[3] = 0x00 | ((pts[2] << 3)&0xF8) | ((pts[3] >> 5)&0x07);
	p->scr[4] = 0x04 | ((pts[3] << 3)&0xF8);
	p->scr[5] = 0x01;
	
	p->mux_rate[0] = (uint8_t)(muxr >> 14);
	p->mux_rate[1] = (uint8_t)(0xff & (muxr >> 6));
	p->mux_rate[2] = (uint8_t)(0x03 | ((muxr & 0x3f) << 2));

	p->stuff_length = 0xF8;
	
	p->sheader_llength[0] = 0x00;
	p->sheader_llength[1] = 0x0c;

	setl_ps(p);
	
	p->rate_bound[0] = (uint8_t)(0x80 | (muxr >>15));
	p->rate_bound[1] = (uint8_t)(0xff & (muxr >> 7));
	p->rate_bound[2] = (uint8_t)(0x01 | ((muxr & 0x7f)<<1));
	
	
	p->audio_bound = (uint8_t)((audio_bound << 2)|(fixed << 1)|CSPS);
	p->video_bound = (uint8_t)((audio_lock << 7)|
			      (video_lock << 6)|0x20|video_bound);
	p->reserved = (uint8_t)(0xFF);
	
	p->data[0] =  stream2;
	p->data[1] =  (uint8_t) (0xc0 | (buffer2_scale << 5) | 
			    (buffer2_size >> 8));
	p->data[2] =  (uint8_t) (buffer2_size & 0xff);
	p->data[3] =  stream1;
	p->data[4] =  (uint8_t) (0xc0 | (buffer1_scale << 5) | 
			    (buffer1_size >> 8));
	p->data[5] =  (uint8_t) (buffer1_size & 0xff);
	
	write_ps(fd, p);
	kill_ps(p);
}



void twrite(uint8_t const *buf)
{
	int l = TS_SIZE;
	int c = 0;
	int w;


	while (l){
		w = write(STDOUT_FILENO,buf+c,l);
		if (w>=0){
			l-=w;
			c+=w;
		}
	}
}

void init_p2t(p2t_t *p, void (*fkt)(uint8_t const *buf))
{
	memset(p->pes,0,TS_SIZE);
	p->counter = 0;
	p->pos = 0;
	p->frags = 0;
	if (fkt) p->t_out = fkt;
	else p->t_out = twrite;
}

void clear_p2t(p2t_t *p)
{
	memset(p->pes,0,TS_SIZE);
	p->counter = 0;
	p->pos = 0;
	p->frags = 0;
}


long int find_pes_header(uint8_t const *buf, long int length, int *frags)
{
	int c = 0;
	int found = 0;

	*frags = 0;

	while (c < length-3 && !found) {
		if (buf[c] == 0x00 && buf[c+1] == 0x00 && 
		    buf[c+2] == 0x01) {
			switch ( buf[c+3] ) {
			case 0xBA:
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
			case PRIVATE_STREAM1:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
				found = 1;
				break;
				
			default:
				c++;
				break;
			}	
		} else c++;
	}
	if (c == length-3 && !found){
		if (buf[length-1] == 0x00) *frags = 1;
		if (buf[length-2] == 0x00 &&
		    buf[length-1] == 0x00) *frags = 2;
		if (buf[length-3] == 0x00 &&
		    buf[length-2] == 0x00 &&
		    buf[length-1] == 0x01) *frags = 3;
		return -1;
	}

	return c;
}

void pes_to_ts( uint8_t const *buf, long int length, uint16_t pid, p2t_t *p)
{
	int c,c2,l,add;
	int check,rest;

	c = 0;
	c2 = 0;
	if (p->frags){
		check = 0;
		switch(p->frags){
		case 1:
			if ( buf[c] == 0x00 && buf[c+1] == 0x01 ){
				check = 1;
				c += 2;
			}
			break;
		case 2:
			if ( buf[c] == 0x01 ){
				check = 1;
				c++;
			}
			break;
		case 3:
			check = 1;
		}
		if(check){
			switch ( buf[c] ) {
				
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
			case PRIVATE_STREAM1:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
				p->pes[0] = 0x00;
				p->pes[1] = 0x00;
				p->pes[2] = 0x01;
				p->pes[3] = buf[c];
				p->pos=4;
				memcpy(p->pes+p->pos,buf+c,TS_SIZE-4-p->pos);
				c += TS_SIZE-4-p->pos;
				p_to_t(p->pes,TS_SIZE-4,pid,&p->counter,
				       p->t_out);
				clear_p2t(p);
				break;
				
			default:
				c=0;
				break;
			}
		}
		p->frags = 0;
	}
		
	if (p->pos){
		c2 = find_pes_header(buf+c,length-c,&p->frags);
		if (c2 >= 0 && c2 < TS_SIZE-4-p->pos){
			l = c2+c;
		} else l = TS_SIZE-4-p->pos;
		memcpy(p->pes+p->pos,buf,l);
		c += l;
		p->pos += l;
		p_to_t(p->pes,p->pos,pid,&p->counter,
		       p->t_out);
		clear_p2t(p);
	}
			
	add = 0;
	while (c < length){
		c2 = find_pes_header(buf+c+add,length-c-add,&p->frags);
		if (c2 >= 0) {
			c2 += c+add;
			if (c2 > c){
				p_to_t(buf+c,c2-c,pid,&p->counter,
				       p->t_out);
				c = c2;
				clear_p2t(p);
				add = 0;
			} else add = 1;
		} else {
			l = length-c;
			rest = l % (TS_SIZE-4);
			l -= rest;
			p_to_t(buf+c,l,pid,&p->counter,
			       p->t_out);
			memcpy(p->pes,buf+c+l,rest);
			p->pos = rest;
			c = length;
		}
	}
}



void p_to_t( uint8_t const *buf, long int length, uint16_t pid, uint8_t *counter, 
	    void (*ts_write)(uint8_t const *))
{
  
	int l, pes_start;
	uint8_t obuf[TS_SIZE];
	long int c = 0;
	pes_start = 0;
	if ( length > 3 && 
	     buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01 )
		switch (buf[3]){
			case PROG_STREAM_MAP:
			case PRIVATE_STREAM2:
			case PROG_STREAM_DIR:
			case ECM_STREAM     :
			case EMM_STREAM     :
			case PADDING_STREAM :
			case DSM_CC_STREAM  :
			case ISO13522_STREAM:
			case PRIVATE_STREAM1:
			case AUDIO_STREAM_S ... AUDIO_STREAM_E:
			case VIDEO_STREAM_S ... VIDEO_STREAM_E:
				pes_start = 1;
				break;
				
			default:
				break;
		}			

	while ( c < length ){
		memset(obuf,0,TS_SIZE);
		if (length - c >= TS_SIZE-4){
			l = write_ts_header(pid, counter, pes_start
					     , obuf, TS_SIZE-4);
			memcpy(obuf+l, buf+c, TS_SIZE-l);
			c += TS_SIZE-l;
		} else { 
			l = write_ts_header(pid, counter, pes_start
					     , obuf, length-c);
			memcpy(obuf+l, buf+c, TS_SIZE-l);
			c = length;
		}
		ts_write(obuf);
		pes_start = 0;
	}
}


int write_ps_header(uint8_t *buf, 
		    uint32_t   SCR, 
		    long  muxr,
		    uint8_t    audio_bound,
		    uint8_t    fixed,
		    uint8_t    CSPS,
		    uint8_t    audio_lock,
		    uint8_t    video_lock,
		    uint8_t    video_bound,
		    uint8_t    stream1,
		    uint8_t    buffer1_scale,
		    uint32_t   buffer1_size,
		    uint8_t    stream2,
		    uint8_t    buffer2_scale,
		    uint32_t   buffer2_size)                    
{
	ps_packet p;
	uint8_t *pts;
	long lpts;
	init_ps(&p);
	
	lpts = htonl(SCR);
	pts = (uint8_t *) &lpts;

	
	p.mpeg = 2;
// SCR = 0
	p.scr[0] = 0x44;
	p.scr[1] = 0x00;
	p.scr[2] = 0x04;
	p.scr[3] = 0x00;
	p.scr[4] = 0x04;
	p.scr[5] = 0x01;
	
// SCR = PTS
	p.scr[0] = 0x44 | ((pts[0] >> 3)&0x18) | ((pts[0] >> 4)&0x03);
	p.scr[1] = 0x00 | ((pts[0] << 4)&0xF0) | ((pts[1] >> 4)&0x0F);
	p.scr[2] = 0x04 | ((pts[1] << 4)&0xF0) | ((pts[2] >> 4)&0x08)
		| ((pts[2] >> 5)&0x03);
	p.scr[3] = 0x00 | ((pts[2] << 3)&0xF8) | ((pts[3] >> 5)&0x07);
	p.scr[4] = 0x04 | ((pts[3] << 3)&0xF8);
	p.scr[5] = 0x01;
	
	p.mux_rate[0] = (uint8_t)(muxr >> 14);
	p.mux_rate[1] = (uint8_t)(0xff & (muxr >> 6));
	p.mux_rate[2] = (uint8_t)(0x03 | ((muxr & 0x3f) << 2));

	p.stuff_length = 0xF8;
	
	if (stream1 && stream2){
		p.sheader_llength[0] = 0x00;
		p.sheader_llength[1] = 0x0c;

		setl_ps(&p);
		
		p.rate_bound[0] = (uint8_t)(0x80 | (muxr >>15));
		p.rate_bound[1] = (uint8_t)(0xff & (muxr >> 7));
		p.rate_bound[2] = (uint8_t)(0x01 | ((muxr & 0x7f)<<1));

	
		p.audio_bound = (uint8_t)((audio_bound << 2)|(fixed << 1)|CSPS);
		p.video_bound = (uint8_t)((audio_lock << 7)|
				     (video_lock << 6)|0x20|video_bound);
		p.reserved = (uint8_t)(0xFF >> 1);
		
		p.data[0] =  stream2;
		p.data[1] =  (uint8_t) (0xc0 | (buffer2_scale << 5) | 
				   (buffer2_size >> 8));
		p.data[2] =  (uint8_t) (buffer2_size & 0xff);
		p.data[3] =  stream1;
		p.data[4] =  (uint8_t) (0xc0 | (buffer1_scale << 5) | 
				   (buffer1_size >> 8));
		p.data[5] =  (uint8_t) (buffer1_size & 0xff);
		
		cwrite_ps(buf, &p, PS_HEADER_L2);
		kill_ps(&p);
		return PS_HEADER_L2;
	} else {
		cwrite_ps(buf, &p, PS_HEADER_L1);
		kill_ps(&p);
		return PS_HEADER_L1;
	}
}



#define MAX_BASE 80
#define MAX_PATH 256
#define MAX_EXT 10

int break_up_filename(char *name, char *base_name, char *path, char *ext)
{
	int l,i,sstop,sstart;

	l = strlen(name);
	sstop = l;
	sstart = -1;
	for( i= l-1; i >= 0; i--){
		if (sstop == l && name[i] == '.') sstop = i;
		if (sstart<0 && name[i] == '/') sstart = i+1;
	}
	if (sstart < 0) sstart = 0;
	if (sstop-sstart < MAX_BASE){
		strncpy(base_name, name+sstart, sstop-sstart);
		base_name[sstop-sstart]=0;
		if(sstart > 0){
			if( l - sstop + sstart < MAX_PATH){
				strncpy(path, name, sstart);
				path[sstart] = 0;
			} else {
				fprintf(stderr,"PATH too long\n");
				return -1;
			}

		} else {
			strcpy(path, "./");
		}

		if(sstop < l){
			if( l - sstop -1 < MAX_EXT){
				strncpy(ext, name+sstop+1, l-sstop-1);
				ext[l-sstop-1]=0;
			} else {
				fprintf(stderr,"Extension too long\n");
				return -1;
			}

		} else {
			strcpy(ext, "");
		}

	} else {
		fprintf(stderr,"Name too long\n");
		return -1;
	}
/*
        printf("%d %d\n",sstart, sstop);
        printf("%s %d\n",name, strlen(name));
        printf("%s %d\n",base_name, strlen(base_name));
        printf("%s %d\n",path,strlen(path));
        printf("%s %d\n",ext,strlen(ext));
*/  
	return 0;
}


int seek_mpg_start(uint8_t *buf, int size)
{
	int found = 0;
	int c=0;
	int seq = 0;
	int mpeg = 0;
	int mark = 0;

	while ( !seq ){
		while (found != 4){
			switch (found) {
			case 0:
				if ( buf[c] == 0x00 ) found++;
				c++;
				break;
			case 1:
				if ( buf[c] == 0x00 ) found++;
				else found = 0;
				c++;
				break;			   
			case 2:
				if ( buf[c] == 0x01 ) found++;
				else found = 0;
				if ( buf[c] == 0x00 ) found = 2;
				c++;
				break;
			
			case 3:
				if ( (buf[c] & 0xe0) == 0xe0 ) found++;
				else found = 0;
				c++;
				break;
			}
			if (c >= size) return -1;
		}
		
		if (found == 4){
			mark = c-4;
			c+=2;
			if (c >= size) return -1;

			if ( (buf[c] & 0xC0) == 0x80 ){
				mpeg = 2;
				c += 2;
				if (c >= size) return -1;
				c += buf[c]+1;
				if (c >= size) return -1;
			} else {
				mpeg = 1;
				while( buf[c] == 0xFF ) {
					c++;
					if (c >= size) return -1;
				}
				if ( (buf[c] & 0xC0) == 0x40) c+=2;
				if (c >= size) return -1;
				if ( (buf[c] & 0x30) ){
					if ( (buf[c] & 0x30) == 0x20) c+=5;
					else c+=10;
				} else c++;
				if (c >= size) return -1;
			}

			if ( buf[c] == 0x00 && 
			     buf[c+1] == 0x00 && 
			     buf[c+2] == 0x01 && 
			     buf[c+3] == 0xB3 ) 
				seq = 1;
		}
		found = 0;
	}

	return size-mark;
}


void write_mpg(int fstart, uint64_t length, int fdin, int fdout)
{
//	uint8_t mpeg_end[4] = { 0x00, 0x00, 0x01, 0xB9 };
	uint8_t *buf;
	uint64_t l=0;
	uint64_t count = 0;
	struct stat sb;
	int buf_size;

	fstat (fdout, &sb);
	buf_size =  sb.st_blksize;

	buf = (char *) alloca (buf_size + sizeof (int));
	
	lseek(fdin, fstart, SEEK_SET);

	while ( count < length && (l = read(fdin,buf,buf_size)) >= 0){
		if (l > 0) count+=l;
		write(fdout,buf,l);
		printf("written %02.2f%%\r",(100.*count)/length);
	}
	printf("\n");

	//write( fdout, mpeg_end, 4);
}


#define CHECKBUF (1024*1024)
#define ONE_GIG  (1024UL*1024UL*1024UL)
void split_mpg(char *name, uint64_t size)
{
	char base_name[MAX_BASE];
	char path[MAX_PATH];
	char ext[MAX_EXT];
	char new_name[256];
	uint8_t buf[CHECKBUF];
	int fdin;
	int fdout;
	uint64_t length = 0;
	uint64_t last;
	int i;
	int mark, csize;
	struct stat sb;

	if (break_up_filename(name,base_name,path,ext) < 0) exit(1);


	if ( (fdin = open(name, O_RDONLY|O_LARGEFILE)) < 0){
		fprintf(stderr,"Can't open %s\n",name);
		exit(1);
	}

	fstat (fdin, &sb);

	length = sb.st_size;
	if ( length < ONE_GIG )
		printf("Filelength = %2.2f MB\n", length/1024./1024.);
	else
		printf("Filelength = %2.2f GB\n", length/1024./1024./1024.);

	if ( length < size ) length = size;
	
	printf("Splitting %s into Files with size <= %2.2f MB\n",name,
	       size/1024./1024.);
	
	csize = CHECKBUF;
	read(fdin, buf, csize);
	if ( (mark = seek_mpg_start(buf,csize)) < 0){
		fprintf(stderr,"Couldn't find sequence header\n");
		exit(1);
	}

	last = csize-mark;

	for ( i = 0 ; i < length/size; i++){
		csize = CHECKBUF;
		
		if (csize > length-last) csize = length-last;
		lseek(fdin, last+size-csize, SEEK_SET);
		read(fdin, buf, csize);
		if ( (mark = seek_mpg_start(buf,csize)) < 0){
			fprintf(stderr,"Couldn't find sequence header\n");
			exit(1);
		}

		sprintf(new_name,"%s-%03d.%s",base_name,i,ext);
		printf("writing %s\n",new_name);

		if ( (fdout = open(new_name,O_WRONLY|O_CREAT|O_TRUNC
				   |O_LARGEFILE,
				   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|
				   S_IROTH|S_IWOTH)) < 0){
			fprintf(stderr,"Can't open %s\n",new_name);
			exit(1);
		}
		write_mpg(last, size-mark, fdin, fdout);
		last = last + size - mark;
	}
	sprintf(new_name,"%s-%03d.%s",base_name,i,ext);
	printf("writing %s\n",new_name);

	if ( (fdout = open(new_name,O_WRONLY|O_CREAT|O_TRUNC
			   |O_LARGEFILE,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|
			   S_IROTH|S_IWOTH)) < 0){
		fprintf(stderr,"Can't open %s\n",new_name);
		exit(1);
	}
	write_mpg(last, length-last, fdin, fdout);
}




void cut_mpg(char *name, uint64_t size)
{
	char base_name[MAX_BASE];
	char path[MAX_PATH];
	char ext[MAX_EXT];
	char new_name[256];
	uint8_t buf[CHECKBUF];
	int fdin;
	int fdout;
	uint64_t length = 0;
	uint64_t last;
	int mark, csize;
	struct stat sb;

	if (break_up_filename(name,base_name,path,ext) < 0) exit(1);


	if ( (fdin = open(name, O_RDONLY|O_LARGEFILE)) < 0){
		fprintf(stderr,"Can't open %s\n",name);
		exit(1);
	}

	fstat (fdin, &sb);

	length = sb.st_size;
	if ( length < ONE_GIG )
		printf("Filelength = %2.2f MB\n", length/1024./1024.);
	else
		printf("Filelength = %2.2f GB\n", length/1024./1024./1024.);

	if ( length < size ) length = size;
	
	printf("Splitting %s into 2 Files with length %.2f MB and %.2f MB\n",
	       name, size/1024./1024., (length-size)/1024./1024.);
	
	csize = CHECKBUF;
	read(fdin, buf, csize);
	if ( (mark = seek_mpg_start(buf,csize)) < 0){
		fprintf(stderr,"Couldn't find sequence header\n");
		exit(1);
	}

	last = csize-mark;

	if (csize > length-last) csize = length-last;
	lseek(fdin, last+size-csize, SEEK_SET);
	read(fdin, buf, csize);
	if ( (mark = seek_mpg_start(buf,csize)) < 0){
		fprintf(stderr,"Couldn't find sequence header\n");
		exit(1);
	}

	sprintf(new_name,"%s-1.%s",base_name,ext);
	printf("writing %s\n",new_name);

	if ( (fdout = open(new_name,O_WRONLY|O_CREAT|O_TRUNC
			   |O_LARGEFILE,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|
			   S_IROTH|S_IWOTH)) < 0){
		fprintf(stderr,"Can't open %s\n",new_name);
		exit(1);
	}
	write_mpg(last, size-mark, fdin, fdout);
	last = last + size - mark;

	sprintf(new_name,"%s-2.%s",base_name,ext);
	printf("writing %s\n",new_name);

	if ( (fdout = open(new_name,O_WRONLY|O_CREAT|O_TRUNC
			   |O_LARGEFILE,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|
			   S_IROTH|S_IWOTH)) < 0){
		fprintf(stderr,"Can't open %s\n",new_name);
		exit(1);
	}
	write_mpg(last, length-last, fdin, fdout);
}




void write_all (int fd, uint8_t *data, int length)
{
        int r;

        while (length) {
                if ((r = write(fd, data, length)) > 0) {
			data += r;
			length -= r;
                }
        }
}



void read_all (int fd, uint8_t *data, int length)
{
        int c = 0;

        while(1) {
                if( read(fd, data+c, 1) == 1) {
                        c++;
                        if(data[c-1] == '\n') {
                                data[c] = 0;
                                break;
                        }
                }
                else {
			fprintf (stderr, "Error reading socket\n");
			exit(1);
                }
        }
}



char *url2host (uint8_t *url, char **name, uint32_t *ip, uint32_t *port)
{
	uint8_t *murl;
	struct hostent *hoste;
	struct in_addr haddr;
	int found_ip = 1;
	
	if (!(strncmp(url, "http://", 7)))
		url += 7;

	*name = strdup(url);
	if (!(*name)) {
		*name = NULL;
		return (NULL);
	}

	murl = url;
	while (*murl && *murl != ':' && *murl != '/') {
		if ((*murl < '0' || *murl > '9') && *murl != '.')
			found_ip = 0;
		murl++;
	}

	(*name)[murl - url] = 0;
	if (found_ip) {
		if ((*ip = inet_addr(*name)) == INADDR_NONE)
			return (NULL);
	} else {
		if (!(hoste = gethostbyname(*name)))
			return (NULL);
		memcpy (&haddr, hoste->h_addr, sizeof(haddr));
		*ip = haddr.s_addr;
	}

	if (!*murl || *murl == '/') {
		*port = 80;
		return (murl);
	}
	*port = atoi(++murl);

	while (*murl && *murl != '/')
		murl++;
	return (murl);
}

#define ACCEPT "Accept: video/mpeg, video/x-mpegurl, */*\r\n"

int http_open (char *url)
{
	char purl[1024], *host, req[1024], *sptr;
	uint32_t ip;
	uint32_t port;
	int sock;
	int reloc, relocnum = 0;
	struct sockaddr_in server;
	int mfd;

	strncpy (purl, url, 1023);
	purl[1023] = '\0';

	do {
		host = NULL;
		strcpy (req, "GET ");
		if (!(sptr = url2host(purl, &host, &ip, &port))) {
			fprintf (stderr, "Unknown host\n");
			exit (1);
		}
		strcat (req, sptr);
		sprintf (req + strlen(req),
			 " HTTP/1.0\r\nUser-Agent: %s/%s\r\n",
			 "whatever", "you want");
		if (host) {
			sprintf(req + strlen(req),
				"Host: %s:%u\r\n", host, port);
			free (host);
		}

		strcat (req, ACCEPT);
		strcat (req, "\r\n");
		
		server.sin_port = htons(port);
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = ip;

		if ((sock = socket(PF_INET, SOCK_STREAM, 6)) < 0) {
			perror ("socket");
			exit (1);
		}

		if (connect(sock, (struct sockaddr *)&server, 
			    sizeof(server))) {
			perror ("connect");
			exit (1);
		}
		
		write_all (sock, req, strlen(req));
		if (!(mfd = fileno(fdopen(sock, "rb")))) {
			perror ("open");
			exit (1);
		}
		reloc = 0;
		purl[0] = '\0';
		read_all (mfd, req, 1023);
		if ((sptr = strchr(req, ' '))) {
			switch (sptr[1]) {
				case '2':
					break;
				case '3':
					reloc = 1;
				default:
					fprintf (stderr, "HTTP req failed:%s",
						sptr+1); 
					exit (1);
			}
		}
		do {
			read_all (mfd,req, 1023);
			if (!strncmp(req, "Location:", 9))
				strncpy (purl, req+10, 1023);
		} while (req[0] != '\r' && req[0] != '\n');
	} while (reloc && purl[0] && relocnum++ < 3);
	if (reloc) {
		fprintf (stderr, "Too many HTTP relocations.\n");
		exit (1);
	}

	return sock;
}

extern int errno;
const char * strerrno (void)
{
	return strerror(errno);
}
