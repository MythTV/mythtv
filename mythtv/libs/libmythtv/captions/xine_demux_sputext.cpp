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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <cctype>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "captions/xine_demux_sputext.h"

#include "mythlogging.h"

#define LOG_MODULE "demux_sputext"
#define LOG_VERBOSE
/*
#define LOG
*/

#define ERR           ((void *)-1)
#define LINE_LEN      1000
#define LINE_LEN_QUOT "1000"

/*
 * Demuxer code start
 */

static bool isEol(char p) {
  return (p=='\r' || p=='\n' || p=='\0');
}

static inline void trail_space(std::string& str)
{
    auto mark = str.find_last_of(" \t\r\n");
    if (mark != std::string::npos)
        str.erase(mark);
    mark = str.find_first_not_of(" \t\r\n");
    if (mark != std::string::npos)
        str.erase(0, mark);
}

/*
 *
 */
static char *read_line_from_input(demux_sputext_t *demuxstr, std::string& line) {

  line.reserve(LINE_LEN);
  if ((line.capacity() - demuxstr->buf.size()) > 512) {
    off_t nread = line.capacity() - demuxstr->buf.size();
    nread = std::min(nread, demuxstr->rbuffer_len - demuxstr->rbuffer_cur);
    if (nread < 0) {
      printf("read failed.\n");
      return nullptr;
    }
    if (nread > 0) {
      demuxstr->buf.append(&demuxstr->rbuffer_text[demuxstr->rbuffer_cur],
                           nread);
      demuxstr->rbuffer_cur += nread;
    }
  }

  size_t index = demuxstr->buf.find('\n');
  if (index != std::string::npos) {
    line.assign(demuxstr->buf, 0, index+1);
    demuxstr->buf.erase(0, index+1);
    return line.data();
  }
  if (!demuxstr->buf.empty()) {
    line = demuxstr->buf;
    demuxstr->buf.clear();
    return line.data();
  }

  return nullptr;
}


static subtitle_t *sub_read_line_sami(demux_sputext_t *demuxstr, subtitle_t *current) {

  static std::string s_line;
  static char *s_s = nullptr;
  std::string text;

  current->start = 0;
  current->end = -1;
  int state = 0;

  /* read the first line */
  if (!s_s)
    if (!(s_s = read_line_from_input(demuxstr, s_line))) return nullptr;

  do {
    switch (state) {

    case 0: /* find "START=" */
      s_s = strcasestr (s_s, "Start=");
      if (s_s) {
        current->start = strtol (s_s + 6, &s_s, 0) / 10;
        state = 1; continue;
      }
      break;

    case 1: /* find "<P" */
      if ((s_s = strcasestr (s_s, "<P"))) { s_s += 2; state = 2; continue; }
      break;

    case 2: /* find ">" */
      if ((s_s = strchr (s_s, '>'))) { s_s++; state = 3; text.clear(); continue; }
      break;

    case 3: /* get all text until '<' appears */
      if (*s_s == '\0') { break; }
      else if (strncasecmp (s_s, "&nbsp;", 6) == 0) { text += ' '; s_s += 6; }
      else if (*s_s == '\r') { s_s++; }
      else if (strncasecmp (s_s, "<br>", 4) == 0 || *s_s == '\n') {
        trail_space (text);
        if (!text.empty())
          current->text.push_back(text);
        text.clear();
        if (*s_s == '\n') s_s++; else s_s += 4;
      }
      else if (*s_s == '<') { state = 4; }
      else text += *s_s++;
      continue;

    case 4: /* get current->end or skip <TAG> */
      char *q = strcasestr (s_s, "start=");
      if (q) {
        current->end = strtol (q + 6, &q, 0) / 10 - 1;
        trail_space (text);
        if (!text.empty())
          current->text.push_back(text);
        if (!current->text.empty()) { state = 99; break; }
        state = 0; continue;
      }
      s_s = strchr (s_s, '>');
      if (s_s) { s_s++; state = 3; continue; }
      break;
    }

    /* read next line */
    if (state != 99 && !(s_s = read_line_from_input (demuxstr, s_line)))
      return nullptr;

  } while (state != 99);

  return current;
}



/**
 * \brief Extract the next token from a string.
 *
 * \param source The character string to scan.
 * \param dest A newly allocated string containing the text from the
 *             source string up to the next newline, carriage return,
 *             or vertical bar.
 *
 * \returns one of 1) a pointer to a newly allocated string, 2)
 * nullptr ig the end of te string was reached, or "(char*)-1" on
 * error.
 */
static char *sub_readtext(char *source, std::string& dest) {
  if (source == nullptr)
      return nullptr;

  int len=0;
  char *p=source;

  while ( !isEol(*p) && *p!= '|' ) {
    p++,len++;
  }

  dest.assign(source, len);

  while (*p=='\r' || *p=='\n' || *p=='|')
    p++;

  if (*p)  return p;  /* not-last text field */
  return (char*)nullptr;   /* last text field     */
}

static subtitle_t *sub_read_line_microdvd(demux_sputext_t *demuxstr, subtitle_t *current) {

  std::string line;  line.reserve(LINE_LEN + 1);
  std::string line2; line2.reserve(LINE_LEN + 1);

  current->end=-1;
  do {
    if (!read_line_from_input (demuxstr, line)) return nullptr;
  } while ((sscanf (line.c_str(), "{%" SCNd64 "}{}%" LINE_LEN_QUOT "[^\r\n]", &(current->start), line2.data()) !=2) &&
           (sscanf (line.c_str(), "{%" SCNd64 "}{%" SCNd64 "}%" LINE_LEN_QUOT "[^\r\n]", &(current->start), &(current->end),line2.data()) !=3)
          );

  char *next=line2.data();
  std::string out {};
  while ((next = sub_readtext (next, out))) {
    if (next==ERR) return (subtitle_t *)ERR;
    current->text.push_back(out);
  }
  current->text.push_back(out);

  return current;
}

static subtitle_t *sub_read_line_subviewer(demux_sputext_t *demuxstr, subtitle_t *current) {

  std::string line;
  int a1=0,a2=0,a3=0,a4=0,b1=0,b2=0,b3=0,b4=0; // NOLINT(readability-isolate-declaration)

  while (true) {
    if (!read_line_from_input(demuxstr, line)) return nullptr;
    if (sscanf (line.c_str(), "%d:%d:%d.%d,%d:%d:%d.%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8) {
      if (sscanf (line.c_str(), "%d:%d:%d,%d,%d:%d:%d,%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8)
        continue;
    }
    current->start = a1*360000+a2*6000+a3*100+a4;
    current->end   = b1*360000+b2*6000+b3*100+b4;

    if (!read_line_from_input(demuxstr, line))
      return nullptr;

    char *p=line.data();
    while (true) {
      char *q=nullptr;
      int len = 0;
      for (q=p,len=0; *p && *p!='\r' && *p!='\n' && *p!='|' &&
               (strncasecmp(p,"[br]",4) != 0); p++,len++);
      current->text.emplace_back(q, len);
      if (!*p || *p=='\r' || *p=='\n') break;
      if (*p=='[') while (*p++!=']');
      if (*p=='|') p++;
    }
    break;
  }
  return current;
}

static subtitle_t *sub_read_line_subrip(demux_sputext_t *demuxstr,subtitle_t *current) {
  std::string line;
  int a1=0,a2=0,a3=0,a4=0,b1=0,b2=0,b3=0,b4=0; // NOLINT(readability-isolate-declaration)
  int i = 0;

  do {
    if(!read_line_from_input(demuxstr,line))
      return nullptr;
    i = sscanf(line.c_str(),"%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4);
  } while(i < 8);
  current->start = a1*360000+a2*6000+a3*100+a4/10;
  current->end   = b1*360000+b2*6000+b3*100+b4/10;
  bool end_sub = false;
  do {
    char *p = nullptr; /* pointer to the curently read char */
    std::string temp_line; /* subtitle line that will be transfered to current->text[i] */
    temp_line.reserve(SUB_BUFSIZE);
    if(!read_line_from_input(demuxstr,line))
      return (!current->text.empty()) ? current : nullptr;
    for (p=line.data(); *p!='\0' && !end_sub; p++) {
      bool eol = false;
      switch(*p) {
        case '\\':
          if(*(p+1)=='N' || *(p+1)=='n') {
            eol = true;
            p++;
          } else
            temp_line += *p;
          break;
        case '{':
          // The different code for these if/else clauses is ifdef'd out.
          // NOLINTNEXTLINE(bugprone-branch-clone)
          if(strncmp(p,"{\\i1}",5) == 0) {
#if 0 /* italic not implemented in renderer, ignore them for now */
            temp_line.append("<i>");
#endif
            p+=4;
          }
          else if(strncmp(p,"{\\i0}",5) == 0) {
#if 0 /* italic not implemented in renderer, ignore them for now */
            temp_line.append("</i>");
#endif
            p+=4;
          }
          else
            temp_line += *p;
          break;
        case '\r': /* just ignore '\r's */
          break;
        case '\n':
          eol = true;
          break;
        default:
          temp_line += *p;
          break;
      }
      if (eol) {
        if (!temp_line.empty())
        {
          current->text.push_back(temp_line);
          temp_line.clear();
        } else {
          end_sub = true;
        }
      }
    }
  } while (!end_sub);
  return current;
}

static subtitle_t *sub_read_line_vplayer(demux_sputext_t *demuxstr,subtitle_t *current) {
  std::string line;
  int a1=0,a2=0,a3=0,b1=0,b2=0,b3=0; // NOLINT(readability-isolate-declaration)

  while (current->text.empty()) {
    if( demuxstr->next_line.empty() ) {
      /* if the buffer is empty.... */
      if( !read_line_from_input(demuxstr, line) ) return nullptr;
    } else {
      /* ... get the current line from buffer. */
      line = demuxstr->next_line;
      demuxstr->next_line.clear();
    }
    /* Initialize buffer with next line */
    if( ! read_line_from_input( demuxstr, demuxstr->next_line) ) {
      demuxstr->next_line.clear();
      return nullptr;
    }
    if( (sscanf( line.c_str(),                "%d:%d:%d:", &a1, &a2, &a3) < 3) ||
        (sscanf( demuxstr->next_line.c_str(), "%d:%d:%d:", &b1, &b2, &b3) < 3) )
      continue;
    current->start = a1*360000+a2*6000+a3*100;
    current->end   = b1*360000+b2*6000+b3*100;
    if ((current->end - current->start) > LINE_LEN)
      current->end = current->start + LINE_LEN; /* not too long though.  */
    /* teraz czas na wkopiowanie stringu */
    char *p=line.data();
    /* finds the body of the subtitle_t */
    for (int i=0; i<3; i++){
      char *p2=strchr( p, ':');
      if( p2 == nullptr ) break;
      p=p2+1;
    }

    char *next=p;
    std::string out {};
    while( (next = sub_readtext( next, out )) ) {
      if (next==ERR)
        return (subtitle_t *)ERR;
      current->text.push_back(out);
    }
    current->text.push_back(out);
  }
  return current;
}

static subtitle_t *sub_read_line_rt(demux_sputext_t *demuxstr,subtitle_t *current) {
  /*
   * TODO: This format uses quite rich (sub/super)set of xhtml
   * I couldn't check it since DTD is not included.
   * WARNING: full XML parses can be required for proper parsing
   */
  std::string line;
  int a1=0,a2=0,a3=0,a4=0,b1=0,b2=0,b3=0,b4=0; // NOLINT(readability-isolate-declaration)
  int plen = 0;

  while (current->text.empty()) {
    if (!read_line_from_input(demuxstr, line)) return nullptr;
    /*
     * TODO: it seems that format of time is not easily determined, it may be 1:12, 1:12.0 or 0:1:12.0
     * to describe the same moment in time. Maybe there are even more formats in use.
     */
    if (sscanf (line.c_str(), R"(<Time Begin="%d:%d:%d.%d" End="%d:%d:%d.%d")",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4) < 8)

      plen=a1=a2=a3=a4=b1=b2=b3=b4=0;
    if (
        (sscanf (line.c_str(), R"(<%*[tT]ime %*[bB]egin="%d:%d" %*[Ee]nd="%d:%d"%*[^<]<clear/>%n)",&a2,&a3,&b2,&b3,&plen) < 4) &&
        (sscanf (line.c_str(), R"(<%*[tT]ime %*[bB]egin="%d:%d" %*[Ee]nd="%d:%d.%d"%*[^<]<clear/>%n)",&a2,&a3,&b2,&b3,&b4,&plen) < 5) &&
        /*      (sscanf (line, "<%*[tT]ime %*[bB]egin=\"%d:%d.%d\" %*[Ee]nd=\"%d:%d\"%*[^<]<clear/>%n",&a2,&a3,&a4,&b2,&b3,&plen) < 5) && */
        (sscanf (line.c_str(), R"(<%*[tT]ime %*[bB]egin="%d:%d.%d" %*[Ee]nd="%d:%d.%d"%*[^<]<clear/>%n)",&a2,&a3,&a4,&b2,&b3,&b4,&plen) < 6) &&
        (sscanf (line.c_str(), R"(<%*[tT]ime %*[bB]egin="%d:%d:%d.%d" %*[Ee]nd="%d:%d:%d.%d"%*[^<]<clear/>%n)",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4,&plen) < 8)
        )
      continue;
    current->start = a1*360000+a2*6000+a3*100+a4/10;
    current->end   = b1*360000+b2*6000+b3*100+b4/10;
    /* TODO: I don't know what kind of convention is here for marking multiline subs, maybe <br/> like in xml? */
    size_t index = line.find("<clear/>");
    char *next = (index != std::string::npos) ? &line[index+8] : nullptr;
    std::string out {};
    while ((next = sub_readtext (next, out))) {
      if (next==ERR)
          return (subtitle_t *)ERR;
      current->text.push_back(out);
    }
    current->text.push_back(out);
  }
  return current;
}

static subtitle_t *sub_read_line_ssa(demux_sputext_t *demuxstr,subtitle_t *current) {
  int comma = 0;
  static int s_maxComma = 32; /* let's use 32 for the case that the */
  /*  amount of commas increase with newer SSA versions */

  int hour1   = 0;
  int min1    = 0;
  int sec1    = 0;
  int hunsec1 = 0;
  int hour2   = 0;
  int min2    = 0;
  int sec2    = 0;
  int hunsec2 = 0;
  int nothing = 0;
  std::string line;
  std::string line3; line3.resize(LINE_LEN);
  char *tmp = nullptr;

  do {
    if (!read_line_from_input(demuxstr, line)) return nullptr;
  } while (sscanf (line.data(), "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,"
                   "%" LINE_LEN_QUOT "[^\n\r]", &nothing,
                   &hour1, &min1, &sec1, &hunsec1,
                   &hour2, &min2, &sec2, &hunsec2,
                   line3.data()) < 9
           &&
           sscanf (line.data(), "Dialogue: %d,%d:%d:%d.%d,%d:%d:%d.%d,"
                   "%" LINE_LEN_QUOT "[^\n\r]", &nothing,
                   &hour1, &min1, &sec1, &hunsec1,
                   &hour2, &min2, &sec2, &hunsec2,
                   line3.data()) < 9       );

  size_t index = line3.find(',');
  if (index == std::string::npos)
    return nullptr;
  char *line2 = &line3[index];

  for (comma = 4; comma < s_maxComma; comma ++)
    {
      tmp = line2;
      if(!(tmp=strchr(++tmp, ','))) break;
      if(*(++tmp) == ' ') break;
      /* a space after a comma means we're already in a sentence */
      line2 = tmp;
    }

  if(comma < s_maxComma)s_maxComma = comma;
  /* eliminate the trailing comma */
  if(*line2 == ',') line2++;

  current->start = 360000*hour1 + 6000*min1 + 100*sec1 + hunsec1;
  current->end   = 360000*hour2 + 6000*min2 + 100*sec2 + hunsec2;

  while (((tmp=strstr(line2, "\\n")) != nullptr) || ((tmp=strstr(line2, "\\N")) != nullptr) ){
    current->text.emplace_back(line2, tmp-line2);
    line2=tmp+2;
  }

  current->text.emplace_back(line2);

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
  std::string line;

  if (!read_line_from_input(demuxstr, line))
    return nullptr;
  size_t mark = line.find_first_not_of(" \t\r\n");
  if (mark != std::string::npos)
      line.erase(0, mark);
  if (line.empty())
    return nullptr;
  if (sscanf (line.data(), "%" SCNd64 ",%" SCNd64 ",", &(current->start),
              &(current->end)) <2)
    return (subtitle_t *)ERR;
  /* the files I have are in tenths of second */
  current->start *= 10;
  current->end *= 10;

  /* copy the string to the text buffer */
  auto start = line.find('\"');
  if (start == std::string::npos)
    return (subtitle_t *)ERR;
  auto end = line.find('\"', start + 1);
  if (end == std::string::npos)
    return (subtitle_t *)ERR;
  current->text.push_back(line.substr(start+1, end));

  return current;
}

static subtitle_t *sub_read_line_mpsub (demux_sputext_t *demuxstr, subtitle_t *current) {
  std::string line;
  float a = NAN;
  float b = NAN;

  do {
    if (!read_line_from_input(demuxstr, line))
      return nullptr;
  } while (sscanf (line.c_str(), "%f %f", &a, &b) !=2);

  demuxstr->mpsub_position += (a*100.0F);
  current->start = (int) demuxstr->mpsub_position;
  demuxstr->mpsub_position += (b*100.0F);
  current->end = (int) demuxstr->mpsub_position;

  while (true) {
    if (!read_line_from_input(demuxstr, line))
      return (!current->text.empty()) ? current : nullptr;

    size_t mark = line.find_first_not_of(" \t\r\n");
    if (mark != std::string::npos)
        line.erase(0, mark);

    if (isEol(line[0]) && !current->text.empty())
      return current;

    if (isEol(line[0]))
      return nullptr;

    char *q = nullptr;
    for (q=line.data(); !isEol(*q); q++);
    *q='\0';
    line.resize(strlen(line.c_str()));
    if (!line.empty()) {
      current->text.push_back(line);
      /* printf(">%s<\n",line.data()); */
    } else {
      if (!current->text.empty())
        return current;
      return nullptr;
    }
  }

  return nullptr;
}

static subtitle_t *sub_read_line_aqt (demux_sputext_t *demuxstr, subtitle_t *current) {
  std::string line;

  while (true) {
    /* try to locate next subtitle_t */
    if (!read_line_from_input(demuxstr, line))
      return nullptr;
    if (!(sscanf (line.c_str(), "-->> %" SCNd64, &(current->start)) <1))
      break;
  }

  while (read_line_from_input(demuxstr, line))
  {
      std::string out {};
      sub_readtext(line.data(),out);
      if (out.empty())
          break;
      current->text.push_back(out);
      current->end = -1;
  }
  return (!current->text.empty())? current : nullptr;
}

static subtitle_t *sub_read_line_jacobsub(demux_sputext_t *demuxstr, subtitle_t *current) {
    std::string line1;
    std::string line2;
    std::string directive; directive.resize(LINE_LEN);
    char *p = nullptr;
    char *q = nullptr;
    unsigned a1=0, a2=0, a3=0, a4=0, b1=0, b2=0, b3=0, b4=0; // NOLINT(readability-isolate-declaration)
    unsigned comment = 0;
    static uint32_t s_jacoTimeRes = 30;
    static uint32_t s_jacoShift = 0;

    while (current->text.empty()) {
        if (!read_line_from_input(demuxstr, line1)) {
            return nullptr;
        }
        // Support continuation lines
        if (line1.size() >= 2) {
            while ((line1[line1.size()-2] == '\\') && (line1[line1.size()-1] == '\n')) {
                line1.resize(line1.size()-2);
                if (!read_line_from_input(demuxstr, line2))
                    return nullptr;
                size_t index = line2.find_first_not_of(" \t\r\n");
                if (index != std::string::npos)
                    line2.erase(0, index);
                line1 += line2;
            }
        }
        line2.resize(0);
        line2.resize(LINE_LEN);
        if (sscanf
            (line1.c_str(), "%u:%u:%u.%u %u:%u:%u.%u %" LINE_LEN_QUOT "[^\n\r]", &a1, &a2, &a3, &a4,
             &b1, &b2, &b3, &b4, line2.data()) < 9) {
            if (sscanf(line1.data(), "@%u @%u %" LINE_LEN_QUOT "[^\n\r]", &a4, &b4, line2.data()) < 3) {
                if (line1[0] == '#') {
                    int hours = 0;
                    int minutes = 0;
                    int seconds = 0;
                    int delta = 0;
                    uint32_t units = s_jacoShift;
                    switch (toupper(line1[1])) {
                    case 'S':
                        if (isalpha(line1[2])) {
                            delta = 6;
                        } else {
                            delta = 2;
                        }
                        if (sscanf(&line1[delta], "%d", &hours)) {
                            int inverter = 1;
                            if (hours < 0) {
                                hours *= -1;
                                inverter = -1;
                            }
                            if (sscanf(&line1[delta], "%*d:%d", &minutes)) {
                                if (sscanf
                                    (&line1[delta], "%*d:%*d:%d",
                                     &seconds)) {
                                    sscanf(&line1[delta], "%*d:%*d:%*d.%u",
                                           &units);
                                } else {
                                    hours = 0;
                                    sscanf(&line1[delta], "%d:%d.%u",
                                           &minutes, &seconds, &units);
                                    minutes *= inverter;
                                }
                            } else {
                                hours = minutes = 0;
                                sscanf(&line1[delta], "%d.%u", &seconds,
                                       &units);
                                seconds *= inverter;
                            }
                            s_jacoShift =
                                ((hours * 3600 + minutes * 60 +
                                  seconds) * s_jacoTimeRes +
                                 units) * inverter;
                        }
                        break;
                    case 'T':
                        if (isalpha(line1[2])) {
                            delta = 8;
                        } else {
                            delta = 2;
                        }
                        sscanf(&line1[delta], "%u", &s_jacoTimeRes);
                        break;
                    }
                }
                continue;
            }
            current->start =
                (unsigned long) ((a4 + s_jacoShift) * 100.0 /
                                 s_jacoTimeRes);
            current->end =
                (unsigned long) ((b4 + s_jacoShift) * 100.0 /
                                 s_jacoTimeRes);
        } else {
            current->start =
                (unsigned
                 long) (((a1 * 3600 + a2 * 60 + a3) * s_jacoTimeRes + a4 +
                         s_jacoShift) * 100.0 / s_jacoTimeRes);
            current->end =
                (unsigned
                 long) (((b1 * 3600 + b2 * 60 + b3) * s_jacoTimeRes + b4 +
                         s_jacoShift) * 100.0 / s_jacoTimeRes);
        }
        p = line2.data();
        while ((*p == ' ') || (*p == '\t')) {
            ++p;
        }
        if (isalpha(*p)||*p == '[') {
            if (sscanf(p, "%" LINE_LEN_QUOT "s %" LINE_LEN_QUOT "[^\n\r]", directive.data(), line1.data()) < 2)
                return (subtitle_t *)ERR;
            directive.resize(strlen(directive.c_str()));
            std::transform(directive.begin(), directive.end(), directive.begin(),
                           [](unsigned char c){ return std::toupper(c);});
            if (   (directive.find("RDB") != std::string::npos)
                || (directive.find("RDC") != std::string::npos)
                || (directive.find("RLB") != std::string::npos)
                || (directive.find("RLG") != std::string::npos)) {
                continue;
            }
            /* no alignment */
#if 0
            if (directive.find("JL") != std::string::npos) {
                current->alignment = SUB_ALIGNMENT_HLEFT;
            } else if (directive.find("JR") != std::string::npos) {
                current->alignment = SUB_ALIGNMENT_HRIGHT;
            } else {
                current->alignment = SUB_ALIGNMENT_HCENTER;
            }
#endif
            line2 = line1;
            p = line2.data();
        }
        for (q = line1.data(); (!isEol(*p)); ++p) {
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
                    q = line1.data();
                    current->text.push_back(line1);
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
                } else if (isEol(*(p + 1))) {
                    std::string tmpstr {};
                    if (!read_line_from_input(demuxstr, tmpstr))
                        return nullptr;
                    trail_space(tmpstr);
                    // The std::string addition can reallocate...
                    size_t offset = p - line2.data();
                    line2 += tmpstr;
                    p = line2.data() + offset;
                    break;
                }
                // Checked xine-lib-1.2.8. No fix there. Seems like it
                // should be a break.
                break;
            default:
                if (!comment) {
                    *q = *p;
                    ++q;
                }
            }
        }
        *q = '\0';
        current->text.push_back(line1);
    }
    return current;
}

static subtitle_t *sub_read_line_subviewer2(demux_sputext_t *demuxstr, subtitle_t *current) {
    std::string line;
    int a1=0,a2=0,a3=0,a4=0; // NOLINT(readability-isolate-declaration)

    while (current->text.empty()) {
        if (!read_line_from_input(demuxstr, line)) return nullptr;
        if (line[0]!='{')
            continue;
        if (sscanf (line.data(), "{T %d:%d:%d:%d",&a1,&a2,&a3,&a4) < 4)
            continue;
        current->start = a1*360000+a2*6000+a3*100+a4/10;
        for (;;) {
            if (!read_line_from_input(demuxstr, line)) break;
            if (line[0]=='}') break;
            size_t len = line.find_first_of("\n\r");
            if (len == 0)
                break;
            current->text.push_back(line.substr(0, len));
        }
    }
    return current;
}

static subtitle_t *sub_read_line_subrip09 (demux_sputext_t *demuxstr, subtitle_t *current) {
  std::string line;
  int h = 0;
  int m = 0;
  int s = 0;

  do {
      if (!read_line_from_input (demuxstr, line)) return nullptr;
  } while (sscanf (line.data(), "[%d:%d:%d]", &h, &m, &s) != 3);

  if (!read_line_from_input (demuxstr, line)) return nullptr;

  current->start = 360000 * h + 6000 * m + 100 * s;
  current->end = -1;

  char *next=line.data();
  std::string out {};
  while ((next = sub_readtext (next, out))) {
    if (next==ERR) return (subtitle_t *)ERR;
    current->text.push_back(out);
  }
  current->text.push_back(out);

  return current;
}

/* Code from subreader.c of MPlayer
** Sylvain "Skarsnik" Colinet <scolinet@gmail.com>
*/

static subtitle_t *sub_read_line_mpl2(demux_sputext_t *demuxstr, subtitle_t *current) {
  std::string line;
  std::string line2; line2.resize(LINE_LEN);

  do {
     if (!read_line_from_input (demuxstr, line)) return nullptr;
  } while ((sscanf (line.data(),
                      "[%" SCNd64 "][%" SCNd64 "]%" LINE_LEN_QUOT "[^\r\n]",
                      &(current->start), &(current->end), line2.data()) < 3));
  current->start *= 10;
  current->end *= 10;

  char *p=line2.data();
  char *next=p;
  std::string out {};
  while ((next = sub_readtext (next, out))) {
      if (next == ERR) {return (subtitle_t *)ERR;}
      current->text.push_back(out);
    }
  current->text.push_back(out);

  return current;
}


static int sub_autodetect (demux_sputext_t *demuxstr) {

  std::string line;
  int  i = 0;
  int  j = 0;
  char p = 0;

  while (j < 100) {
    j++;
    if (!read_line_from_input(demuxstr, line))
      return FORMAT_UNKNOWN;

    std::transform(line.begin(), line.end(), line.begin(),
                   [](unsigned char c){ return std::tolower(c);});

    if ((sscanf (line.data(), "{%d}{}", &i)==1) ||
        (sscanf (line.data(), "{%d}{%d}", &i, &i)==2)) {
      demuxstr->uses_time=0;
      return FORMAT_MICRODVD;
    }

    if (sscanf (line.data(), "%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d", &i, &i, &i, &i, &i, &i, &i, &i)==8) {
      demuxstr->uses_time=1;
      return FORMAT_SUBRIP;
    }

    if (sscanf (line.data(), "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8){
      demuxstr->uses_time=1;
      return FORMAT_SUBVIEWER;
    }

    if (sscanf (line.data(), "%d:%d:%d,%d,%d:%d:%d,%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8){
      demuxstr->uses_time=1;
      return FORMAT_SUBVIEWER;
    }

    if (line.find("<sami>") != std::string::npos) {
      demuxstr->uses_time=1;
      return FORMAT_SAMI;
    }
    // Sscanf stops looking at the format string once it populates the
    // last argument, so it never validates the colon after the
    // seconds.  Add a final "the rest of the line" argument to get
    // that validation, so that JACO subtitles can be distinguished
    // from this format.
    std::string line2; line2.resize(LINE_LEN);
    if (sscanf (line.data(), "%d:%d:%d:%" LINE_LEN_QUOT "[^\n\r]",
                &i, &i, &i, line2.data() )==4) {
      demuxstr->uses_time=1;
      return FORMAT_VPLAYER;
    }
    /*
     * A RealText format is a markup language, starts with <window> tag,
     * options (behaviour modifiers) are possible.
     */
    if (line.find("<window") != std::string::npos) {
      demuxstr->uses_time=1;
      return FORMAT_RT;
    }
    if ((line.find("dialogue: marked") != std::string::npos)  ||
        (line.find("dialogue: ") != std::string::npos)) {
      demuxstr->uses_time=1;
      return FORMAT_SSA;
    }
    if (sscanf (line.data(), "%d,%d,\"%c", &i, &i, (char *) &i) == 3) {
      demuxstr->uses_time=0;
      return FORMAT_PJS;
    }
    if (sscanf (line.data(), "format=%d", &i) == 1) {
      demuxstr->uses_time=0;
      return FORMAT_MPSUB;
    }
    if (sscanf (line.data(), "format=tim%c", &p)==1 && p=='e') {
      demuxstr->uses_time=1;
      return FORMAT_MPSUB;
    }
    if (line.find("-->>") != std::string::npos) {
      demuxstr->uses_time=0;
      return FORMAT_AQTITLE;
    }
    if (sscanf(line.data(), "@%d @%d", &i, &i) == 2 ||
        sscanf(line.data(), "%d:%d:%d.%d %d:%d:%d.%d", &i, &i, &i, &i, &i, &i, &i, &i) == 8) {
      demuxstr->uses_time = 1;
      return FORMAT_JACOBSUB;
    }
    if (sscanf(line.data(), "{t %d:%d:%d:%d",&i, &i, &i, &i) == 4) {
      demuxstr->uses_time = 1;
      return FORMAT_SUBVIEWER2;
    }
    if (sscanf(line.data(), "[%d:%d:%d]", &i, &i, &i) == 3) {
      demuxstr->uses_time = 1;
      return FORMAT_SUBRIP09;
    }

    if (sscanf (line.data(), "[%d][%d]", &i, &i) == 2) {
      demuxstr->uses_time = 1;
      return FORMAT_MPL2;
    }
  }
  return FORMAT_UNKNOWN;  /* too many bad lines */
}

// These functions all return either 1) nullptr, 2) (subtitle_t*)ERR,
// or 3) a pointer to the dest parameter.
using read_func_ptr = subtitle_t* (*)(demux_sputext_t *demuxstr,subtitle_t *dest);
const std::array<read_func_ptr, 14> read_func
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
    sub_read_line_mpl2
};

bool sub_read_file (demux_sputext_t *demuxstr) {

  /* Rewind (sub_autodetect() needs to read input from the beginning) */
  demuxstr->rbuffer_cur = 0;
  demuxstr->buf.clear();
  demuxstr->buf.reserve(SUB_BUFSIZE);

  demuxstr->format=sub_autodetect (demuxstr);
  if (demuxstr->format==FORMAT_UNKNOWN) {
    return false;
  }

  /*printf("Detected subtitle file format: %d\n", demuxstr->format);*/

  /* Rewind */
  demuxstr->rbuffer_cur = 0;
  demuxstr->buf.clear();

  demuxstr->num=0;
  int timeout = MAX_TIMEOUT;

  if (demuxstr->uses_time) timeout *= 100;
  else timeout *= 10;

  while(true) {
    subtitle_t dummy {};
    subtitle_t *sub = read_func[demuxstr->format] (demuxstr, &dummy);
    if (!sub) {
      break;   /* EOF */
    }

    if (sub==ERR)
      ++demuxstr->errs;
    else {
      demuxstr->subtitles.push_back(*sub);
      if (demuxstr->num > 0 && demuxstr->subtitles[demuxstr->num-1].end == -1) {
        /* end time not defined in the subtitle */
        if (timeout > sub->start - demuxstr->subtitles[demuxstr->num-1].start) {
          demuxstr->subtitles[demuxstr->num-1].end = sub->start;
        } else {
          demuxstr->subtitles[demuxstr->num-1].end = demuxstr->subtitles[demuxstr->num-1].start + timeout;
        }
      }
      ++demuxstr->num; /* Error vs. Valid */
    }
  }
  /* timeout of last subtitle */
  if (demuxstr->num > 0 && demuxstr->subtitles[demuxstr->num-1].end == -1)
  {
    demuxstr->subtitles[demuxstr->num-1].end = demuxstr->subtitles[demuxstr->num-1].start + timeout;
  }

#if DEBUG_XINE_DEMUX_SPUTEXT
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

  return true;
}
