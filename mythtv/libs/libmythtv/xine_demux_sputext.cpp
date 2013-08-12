/*
 * Copyright (C) 2000-2003 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * code based on old libsputext/xine_decoder.c
 *
 * code based on mplayer module:
 *
 * Subtitle reader with format autodetection
 *
 * Written by laaz
 * Some code cleanup & realloc() by A'rpi/ESP-team
 * dunnowhat sub format by szabi
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include "xine_demux_sputext.h"

#define LOG_MODULE "demux_sputext"
#define LOG_VERBOSE
/*
#define LOG
*/

#define ERR           (void *)-1
#define LINE_LEN      1000
#define LINE_LEN_QUOT "1000"

/*
 * Demuxer code start
 */

#define FORMAT_UNKNOWN   -1
#define FORMAT_MICRODVD   0
#define FORMAT_SUBRIP     1
#define FORMAT_SUBVIEWER  2
#define FORMAT_SAMI       3
#define FORMAT_VPLAYER    4
#define FORMAT_RT         5
#define FORMAT_SSA        6 /* Sub Station Alpha */
#define FORMAT_PJS        7
#define FORMAT_MPSUB      8
#define FORMAT_AQTITLE    9
#define FORMAT_JACOBSUB   10
#define FORMAT_SUBVIEWER2 11
#define FORMAT_SUBRIP09   12
#define FORMAT_MPL2       13 /*Mplayer sub 2 ?*/

static int eol(char p) {
  return (p=='\r' || p=='\n' || p=='\0');
}

static inline void trail_space(char *s) {
  int i;
  while (isspace(*s)) {
    char *copy = s;
    do {
      copy[0] = copy[1];
      copy++;
    } while(*copy);
  }
  i = strlen(s) - 1;
  while (i > 0 && isspace(s[i]))
    s[i--] = '\0';
}

/*
 * Reimplementation of fgets() using the input->read() method.
 */
static char *read_line_from_input(demux_sputext_t *demuxstr, char *line, off_t len) {
  off_t nread = 0;
  char *s;
  int linelen;

  // Since our RemoteFile code sleeps 200ms whenever we get back less data
  // than requested, but this code just keeps trying to read until it gets
  // an error back, we check for empty reads so that we can stop reading
  // when there is no more data to read
  if (demuxstr->emptyReads == 0 && (len - demuxstr->buflen) > 512) {
    nread = len - demuxstr->buflen;
    if (nread > demuxstr->rbuffer_len - demuxstr->rbuffer_cur)
        nread = demuxstr->rbuffer_len - demuxstr->rbuffer_cur;
    if (nread < 0) {
      printf("read failed.\n");
      return NULL;
    }
    memcpy(&demuxstr->buf[demuxstr->buflen],
           &demuxstr->rbuffer_text[demuxstr->rbuffer_cur],
           nread);
    demuxstr->rbuffer_cur += nread;
  }

  if (!nread)
    demuxstr->emptyReads++;

  demuxstr->buflen += nread;
  demuxstr->buf[demuxstr->buflen] = '\0';

  s = strchr(demuxstr->buf, '\n');

  if (line && (s || demuxstr->buflen)) {

    linelen = s ? (s - demuxstr->buf) + 1 : demuxstr->buflen;

    memcpy(line, demuxstr->buf, linelen);
    line[linelen] = '\0';

    memmove(demuxstr->buf, &demuxstr->buf[linelen], SUB_BUFSIZE - linelen);
    demuxstr->buflen -= linelen;

    return line;
  }

  return NULL;
}


static subtitle_t *sub_read_line_sami(demux_sputext_t *demuxstr, subtitle_t *current) {

  static char line[LINE_LEN + 1];
  static char *s = NULL;
  char text[LINE_LEN + 1], *p, *q;
  int state;

  p = NULL;
  current->lines = current->start = 0;
  current->end = -1;
  state = 0;

  /* read the first line */
  if (!s)
    if (!(s = read_line_from_input(demuxstr, line, LINE_LEN))) return 0;

  do {
    switch (state) {

    case 0: /* find "START=" */
      s = strstr (s, "Start=");
      if (s) {
        current->start = strtol (s + 6, &s, 0) / 10;
        state = 1; continue;
      }
      break;

    case 1: /* find "<P" */
      if ((s = strstr (s, "<P"))) { s += 2; state = 2; continue; }
      break;

    case 2: /* find ">" */
      if ((s = strchr (s, '>'))) { s++; state = 3; p = text; continue; }
      break;

    case 3: /* get all text until '<' appears */
      if (*s == '\0') { break; }
      else if (*s == '<') { state = 4; }
      else if (!strncasecmp (s, "&nbsp;", 6)) { *p++ = ' '; s += 6; }
      else if (*s == '\r') { s++; }
      else if (!strncasecmp (s, "<br>", 4) || *s == '\n') {
        *p = '\0'; p = text; trail_space (text);
        if (text[0] != '\0')
          current->text[current->lines++] = strdup (text);
        if (*s == '\n') s++; else s += 4;
      }
      else *p++ = *s++;
      continue;

    case 4: /* get current->end or skip <TAG> */
      q = strstr (s, "Start=");
      if (q) {
        current->end = strtol (q + 6, &q, 0) / 10 - 1;
        *p = '\0'; trail_space (text);
        if (text[0] != '\0')
          current->text[current->lines++] = strdup (text);
        if (current->lines > 0) { state = 99; break; }
        state = 0; continue;
      }
      s = strchr (s, '>');
      if (s) { s++; state = 3; continue; }
      break;
    }

    /* read next line */
    if (state != 99 && !(s = read_line_from_input (demuxstr, line, LINE_LEN)))
      return 0;

  } while (state != 99);

  return current;
}


static char *sub_readtext(char *source, char **dest) {
  int len=0;
  char *p=source;

  while ( !eol(*p) && *p!= '|' ) {
    p++,len++;
  }

  if (!dest)
    return (char*)ERR;

  *dest= (char *)malloc (len+1);
  if (!(*dest))
    return (char*)ERR;

  strncpy(*dest, source, len);
  (*dest)[len]=0;

  while (*p=='\r' || *p=='\n' || *p=='|')
    p++;

  if (*p)  return p;  /* not-last text field */
  else return (char*)NULL;   /* last text field     */
}

static subtitle_t *sub_read_line_microdvd(demux_sputext_t *demuxstr, subtitle_t *current) {

  char line[LINE_LEN + 1];
  char line2[LINE_LEN + 1];
  char *p, *next;
  int i;

  memset (current, 0, sizeof(subtitle_t));

  current->end=-1;
  do {
    if (!read_line_from_input (demuxstr, line, LINE_LEN)) return NULL;
  } while ((sscanf (line, "{%ld}{}%" LINE_LEN_QUOT "[^\r\n]", &(current->start), line2) !=2) &&
           (sscanf (line, "{%ld}{%ld}%" LINE_LEN_QUOT "[^\r\n]", &(current->start), &(current->end),line2) !=3)
          );

  p=line2;

  next=p, i=0;
  while ((next =sub_readtext (next, &(current->text[i])))) {
    if (current->text[i]==ERR) return (subtitle_t *)ERR;
    i++;
    if (i>=SUB_MAX_TEXT) {
      printf ("Too many lines in a subtitle\n");
      current->lines=i;
      return current;
    }
  }
  current->lines= ++i;

  return current;
}

static subtitle_t *sub_read_line_subviewer(demux_sputext_t *demuxstr, subtitle_t *current) {

  char line[LINE_LEN + 1];
  int a1,a2,a3,a4,b1,b2,b3,b4;
  char *p=NULL, *q=NULL;
  int len;

  memset (current, 0, sizeof(subtitle_t));

  while (1) {
    if (!read_line_from_input(demuxstr, line, LINE_LEN)) return NULL;
    if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8) {
      if (sscanf (line, "%d:%d:%d,%d,%d:%d:%d,%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8)
        continue;
    }
    current->start = a1*360000+a2*6000+a3*100+a4;
    current->end   = b1*360000+b2*6000+b3*100+b4;

    if (!read_line_from_input(demuxstr, line, LINE_LEN))
      return NULL;

    p=q=line;
    for (current->lines=1; current->lines <= SUB_MAX_TEXT; current->lines++) {
      for (q=p,len=0; *p && *p!='\r' && *p!='\n' && *p!='|' && strncasecmp(p,"[br]",4); p++,len++);
      current->text[current->lines-1]=(char *)malloc (len+1);
      if (!current->text[current->lines-1]) return (subtitle_t *)ERR;
      strncpy (current->text[current->lines-1], q, len);
      current->text[current->lines-1][len]='\0';
      if (!*p || *p=='\r' || *p=='\n') break;
      if (*p=='[') while (*p++!=']');
      if (*p=='|') p++;
    }
    if (current->lines > SUB_MAX_TEXT) current->lines = SUB_MAX_TEXT;
    break;
  }
  return current;
}

static subtitle_t *sub_read_line_subrip(demux_sputext_t *demuxstr,subtitle_t *current) {
  char line[LINE_LEN + 1];
  int a1,a2,a3,a4,b1,b2,b3,b4;
  int i,end_sub;

  memset(current,0,sizeof(subtitle_t));
  do {
    if(!read_line_from_input(demuxstr,line,LINE_LEN))
      return NULL;
    i = sscanf(line,"%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4);
  } while(i < 8);
  current->start = a1*360000+a2*6000+a3*100+a4/10;
  current->end   = b1*360000+b2*6000+b3*100+b4/10;
  i=0;
  end_sub=0;
  do {
    char *p; /* pointer to the curently read char */
    char temp_line[SUB_BUFSIZE]; /* subtitle line that will be transfered to current->text[i] */
    int temp_index; /* ... and its index wich 'points' to the first EMPTY place -> last read char is at temp_index-1 if temp_index>0 */
    temp_line[SUB_BUFSIZE-1]='\0'; /* just in case... */
    if(!read_line_from_input(demuxstr,line,LINE_LEN)) {
      if(i)
        break; /* if something was read, transmit it */
      else
        return NULL; /* if not, repport EOF */
    }
    for(temp_index=0,p=line;*p!='\0' && !end_sub && temp_index<SUB_BUFSIZE && i<SUB_MAX_TEXT;p++) {
      switch(*p) {
        case '\\':
          if(*(p+1)=='N' || *(p+1)=='n') {
            temp_line[temp_index++]='\0'; /* end of curent line */
            p++;
          } else
            temp_line[temp_index++]=*p;
          break;
        case '{':
#if 0 /* italic not implemented in renderer, ignore them for now */
          if(!strncmp(p,"{\\i1}",5) && temp_index+3<SUB_BUFSIZE) {
            temp_line[temp_index++]='<';
            temp_line[temp_index++]='i';
            temp_line[temp_index++]='>';
#else
          if(!strncmp(p,"{\\i1}",5)) {
#endif
            p+=4;
          }
#if 0 /* italic not implemented in renderer, ignore them for now */
          else if(!strncmp(p,"{\\i0}",5) && temp_index+4<SUB_BUFSIZE) {
            temp_line[temp_index++]='<';
            temp_line[temp_index++]='/';
            temp_line[temp_index++]='i';
            temp_line[temp_index++]='>';
#else
          else if(!strncmp(p,"{\\i0}",5)) {
#endif
            p+=4;
          }
          else
            temp_line[temp_index++]=*p;
          break;
        case '\r': /* just ignore '\r's */
          break;
        case '\n':
          temp_line[temp_index++]='\0';
          break;
        default:
          temp_line[temp_index++]=*p;
          break;
      }
      if(temp_index>0) {
        if(temp_index==SUB_BUFSIZE)
          printf("Too many characters in a subtitle line\n");
        if(temp_line[temp_index-1]=='\0' || temp_index==SUB_BUFSIZE) {
          if(temp_index>1) { /* more than 1 char (including '\0') -> that is a valid one */
            current->text[i]=(char *)malloc(temp_index);
            if(!current->text[i])
              return (subtitle_t *)ERR;
            strncpy(current->text[i],temp_line,temp_index); /* temp_index<=SUB_BUFSIZE is always true here */
            i++;
            temp_index=0;
          } else
            end_sub=1;
        }
      }
    }
  } while(i<SUB_MAX_TEXT && !end_sub);
  if(i>=SUB_MAX_TEXT)
    printf("Too many lines in a subtitle\n");
  current->lines=i;
  return current;
}

static subtitle_t *sub_read_line_vplayer(demux_sputext_t *demuxstr,subtitle_t *current) {
  char line[LINE_LEN + 1];
  int a1,a2,a3,b1,b2,b3;
  char *p=NULL, *next, *p2;
  int i;

  memset (current, 0, sizeof(subtitle_t));

  while (!current->text[0]) {
    if( demuxstr->next_line[0] == '\0' ) { /* if the buffer is empty.... */
      if( !read_line_from_input(demuxstr, line, LINE_LEN) ) return NULL;
    } else {
      /* ... get the current line from buffer. */
      strncpy( line, demuxstr->next_line, LINE_LEN);
      line[LINE_LEN] = '\0'; /* I'm scared. This makes me feel better. */
      demuxstr->next_line[0] = '\0'; /* mark the buffer as empty. */
    }
    /* Initialize buffer with next line */
    if( ! read_line_from_input( demuxstr, demuxstr->next_line, LINE_LEN) ) {
      demuxstr->next_line[0] = '\0';
      return NULL;
    }
    if( (sscanf( line,            "%d:%d:%d:", &a1, &a2, &a3) < 3) ||
        (sscanf( demuxstr->next_line, "%d:%d:%d:", &b1, &b2, &b3) < 3) )
      continue;
    current->start = a1*360000+a2*6000+a3*100;
    current->end   = b1*360000+b2*6000+b3*100;
    if ((current->end - current->start) > LINE_LEN)
      current->end = current->start + LINE_LEN; /* not too long though.  */
    /* teraz czas na wkopiowanie stringu */
    p=line;
    /* finds the body of the subtitle_t */
    for (i=0; i<3; i++){
      p2=strchr( p, ':');
      if( p2 == NULL ) break;
      p=p2+1;
    }

    next=p;
    i=0;
    while( (next = sub_readtext( next, &(current->text[i]))) ) {
      if (current->text[i]==ERR)
        return (subtitle_t *)ERR;
      i++;
      if (i>=SUB_MAX_TEXT) {
        printf("Too many lines in a subtitle\n");
        current->lines=i;
        return current;
      }
    }
    current->lines=++i;
  }
  return current;
}

static subtitle_t *sub_read_line_rt(demux_sputext_t *demuxstr,subtitle_t *current) {
  /*
   * TODO: This format uses quite rich (sub/super)set of xhtml
   * I couldn't check it since DTD is not included.
   * WARNING: full XML parses can be required for proper parsing
   */
  char line[LINE_LEN + 1];
  int a1,a2,a3,a4,b1,b2,b3,b4;
  char *p=NULL,*next=NULL;
  int i,len,plen;

  memset (current, 0, sizeof(subtitle_t));

  while (!current->text[0]) {
    if (!read_line_from_input(demuxstr, line, LINE_LEN)) return NULL;
    /*
     * TODO: it seems that format of time is not easily determined, it may be 1:12, 1:12.0 or 0:1:12.0
     * to describe the same moment in time. Maybe there are even more formats in use.
     */
    if ((len=sscanf (line, "<Time Begin=\"%d:%d:%d.%d\" End=\"%d:%d:%d.%d\"",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)

      plen=a1=a2=a3=a4=b1=b2=b3=b4=0;
    if (
        ((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&plen)) < 4) &&
        ((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&b2,&b3,&b4,&plen)) < 5) &&
        /*      ((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&plen)) < 5) && */
        ((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d.%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&b4,&plen)) < 6) &&
        ((len=sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d:%d.%d\" %*[Ee]nd=\"%d:%d:%d.%d\"%*[^<]<clear/>%n",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4,&plen)) < 8)
        )
      continue;
    current->start = a1*360000+a2*6000+a3*100+a4/10;
    current->end   = b1*360000+b2*6000+b3*100+b4/10;
    p=line;     p+=plen;i=0;
    /* TODO: I don't know what kind of convention is here for marking multiline subs, maybe <br/> like in xml? */
    next = strstr(line,"<clear/>")+8;i=0;
    while ((next =sub_readtext (next, &(current->text[i])))) {
      if (current->text[i]==ERR)
          return (subtitle_t *)ERR;
      i++;
      if (i>=SUB_MAX_TEXT) {
        printf("Too many lines in a subtitle\n");
        current->lines=i;
        return current;
      }
    }
    current->lines=i+1;
  }
  return current;
}

static subtitle_t *sub_read_line_ssa(demux_sputext_t *demuxstr,subtitle_t *current) {
  int comma;
  static int max_comma = 32; /* let's use 32 for the case that the */
  /*  amount of commas increase with newer SSA versions */

  int hour1, min1, sec1, hunsec1, hour2, min2, sec2, hunsec2, nothing;
  int num;
  char line[LINE_LEN + 1], line3[LINE_LEN + 1], *line2;
  char *tmp;

  do {
    if (!read_line_from_input(demuxstr, line, LINE_LEN)) return NULL;
  } while (sscanf (line, "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,"
                   "%[^\n\r]", &nothing,
                   &hour1, &min1, &sec1, &hunsec1,
                   &hour2, &min2, &sec2, &hunsec2,
                   line3) < 9
           &&
           sscanf (line, "Dialogue: %d,%d:%d:%d.%d,%d:%d:%d.%d,"
                   "%[^\n\r]", &nothing,
                   &hour1, &min1, &sec1, &hunsec1,
                   &hour2, &min2, &sec2, &hunsec2,
                   line3) < 9       );

  line2=strchr(line3, ',');
  if (!line2)
    return NULL;

  for (comma = 4; comma < max_comma; comma ++)
    {
      tmp = line2;
      if(!(tmp=strchr(++tmp, ','))) break;
      if(*(++tmp) == ' ') break;
      /* a space after a comma means we're already in a sentence */
      line2 = tmp;
    }

  if(comma < max_comma)max_comma = comma;
  /* eliminate the trailing comma */
  if(*line2 == ',') line2++;

  current->lines=0;num=0;
  current->start = 360000*hour1 + 6000*min1 + 100*sec1 + hunsec1;
  current->end   = 360000*hour2 + 6000*min2 + 100*sec2 + hunsec2;

  while (((tmp=strstr(line2, "\\n")) != NULL) || ((tmp=strstr(line2, "\\N")) != NULL) ){
    current->text[num]=(char *)malloc(tmp-line2+1);
    strncpy (current->text[num], line2, tmp-line2);
    current->text[num][tmp-line2]='\0';
    line2=tmp+2;
    num++;
    current->lines++;
    if (current->lines >=  SUB_MAX_TEXT) return current;
  }

  current->text[num]=strdup(line2);
  current->lines++;

  return current;
}

/* Sylvain "Skarsnik" Colinet <scolinet@gmail.com>
 * From MPlayer subreader.c :
 *
 * PJS subtitles reader.
 * That's the "Phoenix Japanimation Society" format.
 * I found some of them in http://www.scriptsclub.org/ (used for anime).
 * The time is in tenths of second.
 *
 * by set, based on code by szabi (dunnowhat sub format ;-)
 */

static subtitle_t *sub_read_line_pjs (demux_sputext_t *demuxstr, subtitle_t *current) {
  char line[LINE_LEN + 1];
  char text[LINE_LEN + 1];
  char *s, *d;

  memset (current, 0, sizeof(subtitle_t));

  if (!read_line_from_input(demuxstr, line, LINE_LEN))
    return NULL;
  for (s = line; *s && isspace(*s); s++);
  if (*s == 0)
    return NULL;
  if (sscanf (line, "%ld,%ld,", &(current->start),
              &(current->end)) <2)
    return (subtitle_t *)ERR;
  /* the files I have are in tenths of second */
  current->start *= 10;
  current->end *= 10;

  /* walk to the beggining of the string */
  for (; *s; s++) if (*s==',') break;
  if (*s) {
      for (s++; *s; s++) if (*s==',') break;
      if (*s) s++;
  }
  if (*s!='"') {
       return (subtitle_t *)ERR;
  }
  /* copy the string to the text buffer */
  for (s++, d=text; *s && *s!='"'; s++, d++)
      *d=*s;
  *d=0;
  current->text[0] = strdup(text);
  current->lines = 1;

  return current;
}

static subtitle_t *sub_read_line_mpsub (demux_sputext_t *demuxstr, subtitle_t *current) {
  char line[LINE_LEN + 1];
  float a,b;
  int num=0;
  char *p, *q;

  do {
    if (!read_line_from_input(demuxstr, line, LINE_LEN))
      return NULL;
  } while (sscanf (line, "%f %f", &a, &b) !=2);

  demuxstr->mpsub_position += (a*100.0);
  current->start = (int) demuxstr->mpsub_position;
  demuxstr->mpsub_position += (b*100.0);
  current->end = (int) demuxstr->mpsub_position;

  while (num < SUB_MAX_TEXT) {
    if (!read_line_from_input(demuxstr, line, LINE_LEN))
      return NULL;

    p=line;
    while (isspace(*p))
      p++;

    if (eol(*p) && num > 0)
      return current;

    if (eol(*p))
      return NULL;

    for (q=p; !eol(*q); q++);
    *q='\0';
    if (strlen(p)) {
      current->text[num]=strdup(p);
      printf(">%s<\n",p);
      current->lines = ++num;
    } else {
      if (num)
        return current;
      else
        return NULL;
    }
  }

  return NULL;
}

static subtitle_t *sub_read_line_aqt (demux_sputext_t *demuxstr, subtitle_t *current) {
  char line[LINE_LEN + 1];

  memset (current, 0, sizeof(subtitle_t));

  while (1) {
    /* try to locate next subtitle_t */
    if (!read_line_from_input(demuxstr, line, LINE_LEN))
      return NULL;
    if (!(sscanf (line, "-->> %ld", &(current->start)) <1))
      break;
  }

  if (!read_line_from_input(demuxstr, line, LINE_LEN))
    return NULL;

  sub_readtext((char *) &line,&current->text[0]);
  current->lines = 1;
  current->end = -1;

  if (!read_line_from_input(demuxstr, line, LINE_LEN))
    return current;;

  sub_readtext((char *) &line,&current->text[1]);
  current->lines = 2;

  if ((current->text[0][0]==0) && (current->text[1][0]==0)) {
    return NULL;
  }

  return current;
}

static subtitle_t *sub_read_line_jacobsub(demux_sputext_t *demuxstr, subtitle_t *current) {
    char line1[LINE_LEN], line2[LINE_LEN], directive[LINE_LEN], *p, *q;
    unsigned a1, a2, a3, a4, b1, b2, b3, b4, comment = 0;
    static unsigned jacoTimeres = 30;
    static int jacoShift = 0;

    memset(current, 0, sizeof(subtitle_t));
    memset(line1, 0, LINE_LEN);
    memset(line2, 0, LINE_LEN);
    memset(directive, 0, LINE_LEN);
    while (!current->text[0]) {
        if (!read_line_from_input(demuxstr, line1, LINE_LEN)) {
            return NULL;
        }
        if (sscanf
            (line1, "%u:%u:%u.%u %u:%u:%u.%u %" LINE_LEN_QUOT "[^\n\r]", &a1, &a2, &a3, &a4,
             &b1, &b2, &b3, &b4, line2) < 9) {
            if (sscanf(line1, "@%u @%u %" LINE_LEN_QUOT "[^\n\r]", &a4, &b4, line2) < 3) {
                if (line1[0] == '#') {
                    int hours = 0, minutes = 0, seconds, delta, inverter =
                        1;
                    unsigned units = jacoShift;
                    switch (toupper(line1[1])) {
                    case 'S':
                        if (isalpha(line1[2])) {
                            delta = 6;
                        } else {
                            delta = 2;
                        }
                        if (sscanf(&line1[delta], "%d", &hours)) {
                            if (hours < 0) {
                                hours *= -1;
                                inverter = -1;
                            }
                            if (sscanf(&line1[delta], "%*d:%d", &minutes)) {
                                if (sscanf
                                    (&line1[delta], "%*d:%*d:%d",
                                     &seconds)) {
                                    sscanf(&line1[delta], "%*d:%*d:%*d.%d",
                                           &units);
                                } else {
                                    hours = 0;
                                    sscanf(&line1[delta], "%d:%d.%d",
                                           &minutes, &seconds, &units);
                                    minutes *= inverter;
                                }
                            } else {
                                hours = minutes = 0;
                                sscanf(&line1[delta], "%d.%d", &seconds,
                                       &units);
                                seconds *= inverter;
                            }
                            jacoShift =
                                ((hours * 3600 + minutes * 60 +
                                  seconds) * jacoTimeres +
                                 units) * inverter;
                        }
                        break;
                    case 'T':
                        if (isalpha(line1[2])) {
                            delta = 8;
                        } else {
                            delta = 2;
                        }
                        sscanf(&line1[delta], "%u", &jacoTimeres);
                        break;
                    }
                }
                continue;
            } else {
                current->start =
                    (unsigned long) ((a4 + jacoShift) * 100.0 /
                                     jacoTimeres);
                current->end =
                    (unsigned long) ((b4 + jacoShift) * 100.0 /
                                     jacoTimeres);
            }
        } else {
            current->start =
                (unsigned
                 long) (((a1 * 3600 + a2 * 60 + a3) * jacoTimeres + a4 +
                         jacoShift) * 100.0 / jacoTimeres);
            current->end =
                (unsigned
                 long) (((b1 * 3600 + b2 * 60 + b3) * jacoTimeres + b4 +
                         jacoShift) * 100.0 / jacoTimeres);
        }
        current->lines = 0;
        p = line2;
        while ((*p == ' ') || (*p == '\t')) {
            ++p;
        }
        if (isalpha(*p)||*p == '[') {
            int cont, jLength;

            if (sscanf(p, "%s %" LINE_LEN_QUOT "[^\n\r]", directive, line1) < 2)
                return (subtitle_t *)ERR;
            jLength = strlen(directive);
            for (cont = 0; cont < jLength; ++cont) {
                if (isalpha(*(directive + cont)))
                    *(directive + cont) = toupper(*(directive + cont));
            }
            if ((strstr(directive, "RDB") != NULL)
                || (strstr(directive, "RDC") != NULL)
                || (strstr(directive, "RLB") != NULL)
                || (strstr(directive, "RLG") != NULL)) {
                continue;
            }
            /* no alignment */
#if 0
            if (strstr(directive, "JL") != NULL) {
                current->alignment = SUB_ALIGNMENT_HLEFT;
            } else if (strstr(directive, "JR") != NULL) {
                current->alignment = SUB_ALIGNMENT_HRIGHT;
            } else {
                current->alignment = SUB_ALIGNMENT_HCENTER;
            }
#endif
            strcpy(line2, line1);
            p = line2;
        }
        for (q = line1; (!eol(*p)) && (current->lines < SUB_MAX_TEXT); ++p) {
            switch (*p) {
            case '{':
                comment++;
                break;
            case '}':
                if (comment) {
                    --comment;
                    /* the next line to get rid of a blank after the comment */
                    if ((*(p + 1)) == ' ')
                        p++;
                }
                break;
            case '~':
                if (!comment) {
                    *q = ' ';
                    ++q;
                }
                break;
            case ' ':
            case '\t':
                if ((*(p + 1) == ' ') || (*(p + 1) == '\t'))
                    break;
                if (!comment) {
                    *q = ' ';
                    ++q;
                }
                break;
            case '\\':
                if (*(p + 1) == 'n') {
                    *q = '\0';
                    q = line1;
                    current->text[current->lines++] = strdup(line1);
                    ++p;
                    break;
                }
                if ((toupper(*(p + 1)) == 'C')
                    || (toupper(*(p + 1)) == 'F')) {
                    ++p,++p;
                    break;
                }
                if ((*(p + 1) == 'B') || (*(p + 1) == 'b') ||
                    /* actually this means "insert current date here" */
                    (*(p + 1) == 'D') ||
                    (*(p + 1) == 'I') || (*(p + 1) == 'i') ||
                    (*(p + 1) == 'N') ||
                    /* actually this means "insert current time here" */
                    (*(p + 1) == 'T') ||
                    (*(p + 1) == 'U') || (*(p + 1) == 'u')) {
                    ++p;
                    break;
                }
                if ((*(p + 1) == '\\') ||
                    (*(p + 1) == '~') || (*(p + 1) == '{')) {
                    ++p;
                } else if (eol(*(p + 1))) {
                    if (!read_line_from_input(demuxstr, directive, LINE_LEN))
                        return NULL;
                    trail_space(directive);
                    strncat(line2, directive,
                            ((LINE_LEN > 511) ? LINE_LEN-1 : 511)
                            - strlen(line2));
                    break;
                }
            default:
                if (!comment) {
                    *q = *p;
                    ++q;
                }
            }
        }
        *q = '\0';
        if (current->lines < SUB_MAX_TEXT)
            current->text[current->lines] = strdup(line1);
        else
            printf ("Too many lines in a subtitle\n");
    }
    current->lines++;
    return current;
}

static subtitle_t *sub_read_line_subviewer2(demux_sputext_t *demuxstr, subtitle_t *current) {
    char line[LINE_LEN+1];
    int a1,a2,a3,a4;
    char *p=NULL;
    int i,len;

    while (!current->text[0]) {
        if (!read_line_from_input(demuxstr, line, LINE_LEN)) return NULL;
        if (line[0]!='{')
            continue;
        if ((len=sscanf (line, "{T %d:%d:%d:%d",&a1,&a2,&a3,&a4)) < 4)
            continue;
        current->start = a1*360000+a2*6000+a3*100+a4/10;
        for (i=0; i<SUB_MAX_TEXT;) {
            if (!read_line_from_input(demuxstr, line, LINE_LEN)) break;
            if (line[0]=='}') break;
            len=0;
            for (p=line; *p!='\n' && *p!='\r' && *p; ++p,++len);
            if (len) {
                current->text[i]=(char *)malloc (len+1);
                if (!current->text[i]) return (subtitle_t *)ERR;
                strncpy (current->text[i], line, len); current->text[i][len]='\0';
                ++i;
            } else {
                break;
            }
        }
        current->lines=i;
    }
    return current;
}

static subtitle_t *sub_read_line_subrip09 (demux_sputext_t *demuxstr, subtitle_t *current) {
  char line[LINE_LEN + 1];
  char *next;
  int h, m, s;
  int i;

  memset (current, 0, sizeof(subtitle_t));

  do {
    if (!read_line_from_input (demuxstr, line, LINE_LEN)) return NULL;
  } while (sscanf (line, "[%d:%d:%d]", &h, &m, &s) != 3);

  if (!read_line_from_input (demuxstr, line, LINE_LEN)) return NULL;

  current->start = 360000 * h + 6000 * m + 100 * s;
  current->end = -1;

  next=line;
  i=0;
  while ((next = sub_readtext (next, &(current->text[i])))) {
    if (current->text[i]==ERR) return (subtitle_t *)ERR;
    i++;
    if (i>=SUB_MAX_TEXT) {
      printf("Too many lines in a subtitle\n");
      current->lines=i;
      return current;
    }
  }
  current->lines= ++i;

  return current;
}

/* Code from subreader.c of MPlayer
** Sylvain "Skarsnik" Colinet <scolinet@gmail.com>
*/

static subtitle_t *sub_read_line_mpl2(demux_sputext_t *demuxstr, subtitle_t *current) {
  char line[LINE_LEN+1];
  char line2[LINE_LEN+1];
  char *p, *next;
  int i;

  memset (current, 0, sizeof(subtitle_t));
  do {
     if (!read_line_from_input (demuxstr, line, LINE_LEN)) return NULL;
  } while ((sscanf (line,
                      "[%ld][%ld]%[^\r\n]",
                      &(current->start), &(current->end), line2) < 3));
  current->start *= 10;
  current->end *= 10;
  p=line2;

  next=p, i=0;
  while ((next = sub_readtext (next, &(current->text[i])))) {
      if (current->text[i] == ERR) {return (subtitle_t *)ERR;}
      i++;
      if (i >= SUB_MAX_TEXT) {
        printf("Too many lines in a subtitle\n");
        current->lines = i;
        return current;
      }
    }
  current->lines= ++i;

  return current;
}


static int sub_autodetect (demux_sputext_t *demuxstr) {

  char line[LINE_LEN + 1];
  int  i, j=0;
  char p;

  while (j < 100) {
    j++;
    if (!read_line_from_input(demuxstr, line, LINE_LEN))
      return FORMAT_UNKNOWN;

    if ((sscanf (line, "{%d}{}", &i)==1) ||
        (sscanf (line, "{%d}{%d}", &i, &i)==2)) {
      demuxstr->uses_time=0;
      return FORMAT_MICRODVD;
    }

    if (sscanf (line, "%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d", &i, &i, &i, &i, &i, &i, &i, &i)==8) {
      demuxstr->uses_time=1;
      return FORMAT_SUBRIP;
    }

    if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8){
      demuxstr->uses_time=1;
      return FORMAT_SUBVIEWER;
    }

    if (sscanf (line, "%d:%d:%d,%d,%d:%d:%d,%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8){
      demuxstr->uses_time=1;
      return FORMAT_SUBVIEWER;
    }

    if (strstr (line, "<SAMI>")) {
      demuxstr->uses_time=1;
      return FORMAT_SAMI;
    }
    if (sscanf (line, "%d:%d:%d:",     &i, &i, &i )==3) {
      demuxstr->uses_time=1;
      return FORMAT_VPLAYER;
    }
    /*
     * A RealText format is a markup language, starts with <window> tag,
     * options (behaviour modifiers) are possible.
     */
    if ( !strcasecmp(line, "<window") ) {
      demuxstr->uses_time=1;
      return FORMAT_RT;
    }
    if ((!memcmp(line, "Dialogue: Marked", 16)) || (!memcmp(line, "Dialogue: ", 10))) {
      demuxstr->uses_time=1;
      return FORMAT_SSA;
    }
    if (sscanf (line, "%d,%d,\"%c", &i, &i, (char *) &i) == 3) {
      demuxstr->uses_time=0;
      return FORMAT_PJS;
    }
    if (sscanf (line, "FORMAT=%d", &i) == 1) {
      demuxstr->uses_time=0;
      return FORMAT_MPSUB;
    }
    if (sscanf (line, "FORMAT=TIM%c", &p)==1 && p=='E') {
      demuxstr->uses_time=1;
      return FORMAT_MPSUB;
    }
    if (strstr (line, "-->>")) {
      demuxstr->uses_time=0;
      return FORMAT_AQTITLE;
    }
    if (sscanf(line, "@%d @%d", &i, &i) == 2 ||
        sscanf(line, "%d:%d:%d.%d %d:%d:%d.%d", &i, &i, &i, &i, &i, &i, &i, &i) == 8) {
      demuxstr->uses_time = 1;
      return FORMAT_JACOBSUB;
    }
    if (sscanf(line, "{T %d:%d:%d:%d",&i, &i, &i, &i) == 4) {
      demuxstr->uses_time = 1;
      return FORMAT_SUBVIEWER2;
    }
    if (sscanf(line, "[%d:%d:%d]", &i, &i, &i) == 3) {
      demuxstr->uses_time = 1;
      return FORMAT_SUBRIP09;
    }

    if (sscanf (line, "[%d][%d]", &i, &i) == 2) {
      demuxstr->uses_time = 1;
      return FORMAT_MPL2;
    }
  }
  return FORMAT_UNKNOWN;  /* too many bad lines */
}

subtitle_t *sub_read_file (demux_sputext_t *demuxstr) {

  int n_max;
  int timeout;
  subtitle_t *first;
  subtitle_t * (*func[])(demux_sputext_t *demuxstr,subtitle_t *dest)=
  {
    sub_read_line_microdvd,
    sub_read_line_subrip,
    sub_read_line_subviewer,
    sub_read_line_sami,
    sub_read_line_vplayer,
    sub_read_line_rt,
    sub_read_line_ssa,
    sub_read_line_pjs,
    sub_read_line_mpsub,
    sub_read_line_aqt,
    sub_read_line_jacobsub,
    sub_read_line_subviewer2,
    sub_read_line_subrip09,
    sub_read_line_mpl2,
  };

  /* Rewind (sub_autodetect() needs to read input from the beginning) */
  demuxstr->rbuffer_cur = 0;
  demuxstr->buflen = 0;
  demuxstr->emptyReads = 0;

  demuxstr->format=sub_autodetect (demuxstr);
  if (demuxstr->format==FORMAT_UNKNOWN) {
    return NULL;
  }

  /*printf("Detected subtitle file format: %d\n", demuxstr->format);*/

  /* Rewind */
  demuxstr->rbuffer_cur = 0;
  demuxstr->buflen = 0;
  demuxstr->emptyReads = 0;

  demuxstr->num=0;n_max=32;
  first = (subtitle_t *) malloc(n_max*sizeof(subtitle_t));
  if(!first) return NULL;
  timeout = MAX_TIMEOUT;

  if (demuxstr->uses_time) timeout *= 100;
  else timeout *= 10;

  while(1) {
    subtitle_t *sub;

    if(demuxstr->num>=n_max){
      n_max+=16;
      first=(subtitle_t *)realloc(first,n_max*sizeof(subtitle_t));
    }

    sub = func[demuxstr->format] (demuxstr, &first[demuxstr->num]);

    if (!sub) {
      break;   /* EOF */
    } else {
      demuxstr->emptyReads = 0;
    }

    if (sub==ERR)
      ++demuxstr->errs;
    else {
      if (demuxstr->num > 0 && first[demuxstr->num-1].end == -1) {
        /* end time not defined in the subtitle */
        if (timeout > 0) {
          /* timeout */
          if (timeout > sub->start - first[demuxstr->num-1].start) {
            first[demuxstr->num-1].end = sub->start;
          } else
            first[demuxstr->num-1].end = first[demuxstr->num-1].start + timeout;
        } else {
          /* no timeout */
          first[demuxstr->num-1].end = sub->start;
        }
      }
      ++demuxstr->num; /* Error vs. Valid */
    }
  }
  /* timeout of last subtitle */
  if (demuxstr->num > 0 && first[demuxstr->num-1].end == -1)
    if (timeout > 0) {
      first[demuxstr->num-1].end = first[demuxstr->num-1].start + timeout;
    }

#ifdef DEBUG_XINE_DEMUX_SPUTEXT
  {
    char buffer[1024];

    sprintf(buffer, "Read %i subtitles", demuxstr->num);

    if(demuxstr->errs)
      sprintf(buffer + strlen(buffer), ", %i bad line(s).\n", demuxstr->errs);
    else
      strcat(buffer, "\n");

    printf("%s", buffer);
  }
#endif

  return first;
}

