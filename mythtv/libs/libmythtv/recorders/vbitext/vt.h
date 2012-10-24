#ifndef VT_H
#define VT_H

#define VT_WIDTH       40
#define VT_HEIGHT      25
#define BAD_CHAR       0xb8    // substitute for chars with bad parity

struct vt_event
{
    int type;
    void *resource;    /* struct xio_win *, struct vbi *, ... */
    int i1, i2, i3, i4;
    void *p1;
};

#define EV_CLOSE       1
#define EV_KEY         2       // i1:KEY_xxx  i2:shift-flag
#define EV_MOUSE       3       // i1:button  i2:shift-flag i3:x  i4:y
#define EV_SELECTION   4       // i1:len  p1:data
#define EV_PAGE                5       // p1:vt_page   i1:query-flag
#define EV_HEADER      6       // i1:pgno  i2:subno  i3:flags  p1:data
#define EV_XPACKET     7       // i1:mag  i2:pkt  i3:errors  p1:data
#define EV_RESET       8       // ./.
#define EV_TIMER       9       // ./.

#define KEY_F(i)       (1000+i)
#define KEY_LEFT       2001
#define KEY_RIGHT      2002
#define KEY_UP         2003
#define KEY_DOWN       2004
#define KEY_PUP                2005
#define KEY_PDOWN      2006
#define KEY_DEL                2007
#define KEY_INS                2008

struct vt_page
{
    int pgno, subno;   // the wanted page number
    int lang;          // language code
    int flags;         // misc flags (see PG_xxx below)
    int errors;                // number of single bit errors in page
    unsigned int lines;                // 1 bit for each line received
    unsigned char data[VT_HEIGHT][VT_WIDTH];   // page contents
    int flof;          // page has FastText links
    struct {
       int pgno;
       int subno;
    } link[6];         // FastText links (FLOF)
};

#define PG_SUPPHEADER  0x01    // C7  row 0 is not to be displayed
#define PG_UPDATE      0x02    // C8  row 1-28 has modified (editors flag)
#define PG_OUTOFSEQ    0x04    // C9  page out of numerical order
#define PG_NODISPLAY   0x08    // C10 rows 1-24 is not to be displayed
#define PG_MAGSERIAL   0x10    // C11 serial trans. (any pkt0 terminates page)
#define PG_ERASE       0x20    // C4  clear previously stored lines
#define PG_NEWSFLASH   0x40    // C5  box it and insert into normal video pict.
#define PG_SUBTITLE    0x80    // C6  box it and insert into normal video pict.
// my flags
#define PG_ACTIVE      0x100   // currently fetching this page

#define ANY_SUB                0x3f7f  // universal subpage number

#endif

