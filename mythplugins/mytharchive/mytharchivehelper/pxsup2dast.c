/*
 #
 # pxsup2dast.c version YYYY-MM-DD (take from most recent change below).
 #
 # Project X sup to dvdauthor subtitle xml file.
 # too ät iki piste fi
 #
 # -------------------------------------------------
 #
 # This is currently wery picky what this expects of ProjectX .sup to contain.
 # Update: 2005/07/02 Not so picky anymore, but control sequence parsing is
 # not perfect (yet!?)
 #
 # Change 2010-08-29: Initial handling of 0x07 control code (from
 # Simon Liddicott). Currently just ignores contents but doesn't choke on
 # it anymore...
 #
 # Change 2009-08-09: Renamed getline() as getpixelline() (to avoid function
 # name collision. Fixed GPL version to 2 (only). Thanks Ville Skyttä.
 #
 # Change 2009-01-10: Added subtitle indexing change (fix) from patch
 # sent by Ian Stewart.
 #
 # This program is released under GNU GPL version 2. Check
 # http://www.fsf.org/licenses/licenses.html
 # to get your own copy of the GPL v2 license.
 #
 */

typedef enum { false = 0, true = 1 }	bool;
typedef unsigned char			bool8;


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <setjmp.h>
#include <zlib.h>

#if 1
/* this is all speed, not so much portability -- using C99 features... */
#include <stdint.h>

typedef int8_t  ei8;		typedef uint8_t  eu8;
typedef int16_t ei16;		typedef uint16_t eu16;
typedef int32_t ei32;		typedef uint32_t eu32;

typedef int_least8_t  li8;	typedef uint_least8_t  lu8;
typedef int_least16_t li16;	typedef uint_least16_t lu16;
typedef int_least32_t li32;	typedef uint_least32_t lu32;

typedef int_fast8_t  fi8;	typedef uint_fast8_t  fu8;
typedef int_fast16_t fi16;	typedef uint_fast16_t fu16;
typedef int_fast32_t fi32;	typedef uint_fast32_t fu32;
#endif


#if (__GNUC__ >= 3)
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))
#define GCCATTR_UNUSED	 __attribute ((unused))
#define GCCATTR_NORETURN __attribute ((noreturn))
#define GCCATTR_CONST	 __attribute ((const))
#else
#define GCCATTR_PRINTF(m, n)
#define GCCATTR_UNUSED
#define GCCATTR_NORETURN
#define GCCATTR_CONST
#endif

/* use this only to cast quoted strings in function calls */
#define CUS (const unsigned char *)

/* forward declarations */
typedef struct _Png4File Png4File;
typedef struct _BoundStr BoundStr;

enum { MiscError = 1, EOFIndicator, IndexError };

int sup2dast(const char *supfile, const char *ifofile, int delay_ms);

/****** Poor man's exception code ... (heavily inspired by cexcept). ******/

struct exc__state 
{
    struct exc__state * prev;
    jmp_buf env; 
};

struct 
{
    struct exc__state * last;
    char msgbuf[1024];
    int  buflen;
} EXC /* = { 0 }*/ ;

#define exc_try do { struct exc__state exc_s; int exc_type GCCATTR_UNUSED; \
    exc_s.prev = EXC.last; EXC.last = &exc_s; if ((exc_type = setjmp(exc_s.env)) == 0)

#define exc_ftry do { struct exc__state exc_s, *exc_p = EXC.last; \
    int exc_type GCCATTR_UNUSED; exc_s.prev = EXC.last; \
    EXC.last = &exc_s; if ((exc_type = setjmp(exc_s.env)) == 0)

#define exc_catch(t) else if (t == exc_type)

#define exc_end else __exc_throw(exc_type); EXC.last = exc_s.prev; } while (0)

#define exc_return for (EXC.last = exc_p;;) return

#define exc_fthrow for (EXC.last = exc_p;;) ex_throw

#define exc_catchall else
#define exc_endall EXC.last = exc_s.prev; } while (0)

static void __exc_throw(int type) /* protoadd GCCATTR_NORETURN */ 
{
    struct exc__state * exc_s;
    exc_s = EXC.last;
    EXC.last = EXC.last->prev;
    longjmp(exc_s->env, type);
}

static void exc_throw(int type, const char * format, ...)
    /* protoadd GCCATTR_NORETURN */ 
{
    if (format != NULL) 
    {
        va_list ap;
        unsigned int len;
        int err = errno;

        va_start(ap, format);
        len = vsnprintf(EXC.msgbuf, sizeof EXC.msgbuf, format, ap);
        va_end(ap);

        if (len >= sizeof EXC.msgbuf) 
        {
            len = sizeof EXC.msgbuf - 1;
            EXC.msgbuf[len] = '\0'; 
        }
        else 
        {
            if (format[strlen(format) - 1] == ':') 
            {
                int l = snprintf(&EXC.msgbuf[len], sizeof EXC.msgbuf - len,
                      " %s.", strerror(err));
                if (l + len >= sizeof EXC.msgbuf) 
                {
                    len = sizeof EXC.msgbuf - 1;
                    EXC.msgbuf[len] = '\0'; 
                }
                else
                    len += l; 
            }
        }
        EXC.buflen = len; 
    }
    else 
    {
        EXC.msgbuf[0] = '\0';
        EXC.buflen = 0; 
    }

    __exc_throw(type); 
}

/****** end exception code block ******/


static eu8 * xxfread(FILE * stream, eu8 * ptr, size_t size) 
{
    size_t n = fread(ptr, size, 1, stream);
    if (n == 0) 
    {
        if (ferror(stream))
            exc_throw(MiscError, "fread failure:");
        exc_throw(EOFIndicator, NULL); }
    return ptr; 
}

static void xxfwrite(FILE * stream, const eu8 * ptr, size_t size) 
{
    size_t n = fwrite(ptr, size, 1, stream);
    if (n == 0) 
    {
        if (ferror(stream))
            exc_throw(MiscError, "fwrite failure:");
        exc_throw(MiscError, "fwrite failure:"); 
    }
}

#define xxfwriteCS(f, s) xxfwrite(f, CUS s, sizeof s - 1)

static void yuv2rgb(int y,   int cr,  int cb,
            eu8 * r, eu8 * g, eu8 * b)  
{
    int lr, lg, lb;

    /* from dvdauthor... */
    lr = (500 + 1164 * (y - 16) + 1596 * (cr - 128)              ) /1000;
    lg = (500 + 1164 * (y - 16) -  813 * (cr - 128) - 391 * (cb - 128)) / 1000;
    lb = (500 + 1164 * (y - 16)                    + 2018 * (cb - 128)) / 1000;

    *r = (lr < 0)? 0: (lr > 255)? 255: (eu8)lr;
    *g = (lg < 0)? 0: (lg > 255)? 255: (eu8)lg;
    *b = (lb < 0)? 0: (lb > 255)? 255: (eu8)lb;  
}

static void rgb2yuv(eu8 r,   eu8   g,  eu8 b,
                    eu8 * y, eu8 * cr, eu8 * cb)
{
    /* int ly, lcr, lcb; */

    /* from dvdauthor... */
    *y  = ( 257 * r + 504 * g +  98 * b +  16500) / 1000;
    *cr = ( 439 * r - 368 * g -  71 * b + 128500) / 1000;
    *cb = (-148 * r - 291 * g + 439 * b + 128500) / 1000; 
}

/* the code above matches nicely with http://www.fourcc.org/fccyvrgb.php

 *  RGB to YUV Conversion

 * Y  =      (0.257 * R) + (0.504 * G) + (0.098 * B) + 16
 * Cr = V =  (0.439 * R) - (0.368 * G) - (0.071 * B) + 128
 * Cb = U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128

 * YUV to RGB Conversion

 * B = 1.164(Y - 16)                  + 2.018(U - 128)
 * G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128)
 * R = 1.164(Y - 16) + 1.596(V - 128)

*/


static FILE * xfopen(const char * filename, const char * mode) 
{
    FILE * fh = fopen(filename, mode);
    if (fh == NULL)
        exc_throw(MiscError, "fopen(\"%s\", \"%s\") failure:", filename, mode);
    return fh; 
}

static void xfseek0(FILE * stream, long offset) 
{
    if (fseek(stream, offset, SEEK_SET) < 0)
        exc_throw(MiscError, "fseek(stream, %ld, SEEK_SET) failure:",offset); 
}

static void xmkdir(const char * path, int mode) 
{
    if (mkdir(path, mode) < 0)
        exc_throw(MiscError, "mkdir(%s, 0%o) failure:", path, mode);
}


static bool fexists(const char * filename) 
{
    struct stat st;

    if (stat(filename, &st) == 0 && S_ISREG(st.st_mode))
        return true;
    return false; 
}

static bool dexists(const char * filename)
{
    struct stat st;

    if (stat(filename, &st) == 0 && S_ISDIR(st.st_mode))
        return true;

    return false; 
}

static fu32 get_uint32_be(const eu8 * bytes)  
{
    return (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3]; 
}

static fu16 get_uint16_be(const eu8 * bytes)
{
    return (bytes[0] << 8) + bytes[1]; 
}

static fu32 get_uint32_le(const eu8 * bytes)  
{
    return (bytes[3] << 24) + (bytes[2] << 16) + (bytes[1] << 8) + bytes[0]; 
}

#if 0  /* protoline */
static fu32 get_uint16_le(const eu8 * bytes)  {
    return (bytes[1] << 8) + bytes[0]; }
#endif  /* protoline */


static void set_uint32_be(eu8 * ptr, eu32 value) 
{
    ptr[0] = value>>24; ptr[1] = value>>16; ptr[2] = value>>8; ptr[3] =value; 
}

#if 0 /* protoline */
static void set_uint16_be(eu8 * ptr, eu32 value) {
    ptr[0] = value>>8; ptr[1] = value; }

static void set_uint32_le(eu8 * ptr, eu32 value) {
    ptr[3] = value>>24; ptr[2] = value>>16; ptr[1] = value>>8; ptr[0] =value; }
#endif  /* protoline */

static void set_uint16_le(eu8 * ptr, eu16 value) 
{
    ptr[1] = value>>8; ptr[0] =value; 
}

static void xxfwrite_uint32_be(FILE * fh, eu32 value) 
{
    eu8 buf[4];
    set_uint32_be(buf, value);
    xxfwrite(fh, buf, 4); 
}

static void ifopalette(const char * filename,
             eu8 yuvpalette[16][3], eu8 rgbpalette[16][3])  
{
    eu8 buf[1024], r, g, b;
    fu32 offset, pgc;
    int i;
    FILE * fh;

    fh = xfopen(filename, "rb");
        if (memcmp(xxfread(fh, buf, 12), "DVDVIDEO-VTS", 12) != 0)
            exc_throw(MiscError,
                    "(IFO) file %s not of type DVDVIDEO-VTS.", filename);

    xfseek0(fh, 0xcc);
    offset = get_uint32_be(xxfread(fh, buf, 4));
    xfseek0(fh, offset * 0x800 + 12);
    pgc = offset * 0x800 + get_uint32_be(xxfread(fh, buf, 4));
    /* seek to palette */
    xfseek0(fh, pgc + 0xa4);
    xxfread(fh, buf, 16 * 4);
    fclose(fh);
    for (i = 0; i < 16; i++) 
    {
        eu8 * p = buf + i * 4 + 1;
        yuvpalette[i][0] =p[0]; yuvpalette[i][1] =p[1]; yuvpalette[i][2] =p[2];
        yuv2rgb(p[0], p[1], p[2], &r, &g, &b);
        rgbpalette[i][0] = r; rgbpalette[i][1] = g; rgbpalette[i][2] = b; 
    }
}


static void set2palettes(int value, eu8 * yuvpalpart, eu8 * rgbpalpart) 
{
    eu8 r, g, b, y, cr, cb;

    r = value >> 16; g = value >> 8; b = value;
    rgbpalpart[0] = r, rgbpalpart[1] = g,  rgbpalpart[2] = b;
    rgb2yuv(r, g, b, &y, &cr, &cb);
    yuvpalpart[0] = y, yuvpalpart[1] = cr, yuvpalpart[2] = cb; 
}


static void argpalette(const char * arg,
               eu8 yuvpalette[16][3], eu8 rgbpalette[16][3])  
{
    unsigned int i;

    if (strlen(arg) != 20 || arg[6] != ',' || arg[13] != ',')
        exc_throw(MiscError, "Palette arg %s invalid.\n", arg);

    for (i = 0; i < 16; i++)
        set2palettes(i * 0x111111, yuvpalette[i], rgbpalette[i]);

    sscanf(arg, "%x", &i); /* "%x," ? */
    set2palettes(i, yuvpalette[1], rgbpalette[1]);
    sscanf(arg + 7, "%x", &i);
    set2palettes(i, yuvpalette[2], rgbpalette[2]);
    sscanf(arg + 14, "%x", &i);
    set2palettes(i, yuvpalette[3], rgbpalette[3]); 
}


/* typedef struct _Png4File Png4File; */
struct _Png4File 
{
    FILE * fh;
    int width;
    int hleft;
    int nibble;
    int bufpos;
    int chunklen;
    eu32 crc;
    eu32 adler;
    eu8 palettechunk[24];
    eu8 buffer[65536]; 
};

static void png4file_init(Png4File * self, eu8 palette[4][3])
{
    memcpy(self->palettechunk, "\0\0\0\x0c" "PLTE", 8);
    memcpy(self->palettechunk + 8, palette, 12);
    self->crc = 0 /*crc32(0, Z_NULL, 0)*/;
    self->crc = crc32(self->crc, self->palettechunk + 4, 16);
    set_uint32_be(self->palettechunk + 20, self->crc); 
}

static void png4file_open(Png4File * self,
                    const char * filename, int height, int width) 
{
    eu32 crc;
    self->fh = xfopen(filename, "wb");
    self->width = width;
    self->hleft = height;
    self->nibble = -1;

    xxfwrite(self->fh, CUS "\x89PNG\r\n\x1a\n" "\0\0\0\x0d", 12);

    memcpy(self->buffer, "IHDR", 4);
    set_uint32_be(self->buffer + 4, width);
    set_uint32_be(self->buffer + 8, height);
    memcpy(self->buffer + 12, "\004\003\0\0\0", 5);

    crc = crc32(0, self->buffer, 17);
    set_uint32_be(self->buffer + 17, crc);
    xxfwrite(self->fh, self->buffer, 21);

    xxfwrite(self->fh, self->palettechunk, sizeof self->palettechunk);

    /* XXX quick hack, first color transparent. */
    xxfwriteCS(self->fh, "\0\0\0\001" "tRNS" "\0" "\x40\xe6\xd8\x66");

    xxfwrite(self->fh, CUS "\0\0\0\0IDAT" "\x78\001", 10);
    self->buffer[0] = '\0';
    self->buffer[5] = '\0';
    self->bufpos = 6;
    self->chunklen = 2; /* 78 01, zlib header */
    self->crc = crc32(0, CUS "IDAT" "\x78\001", 6);
    self->adler = 1 /* adler32(0, Z_NULL, 0) */; 
}

static void png4file_flush(Png4File * self) 
{
    int l = self->bufpos - 5;
    self->chunklen += self->bufpos;
    set_uint16_le(self->buffer + 1, l);
    set_uint16_le(self->buffer + 3, l ^ 0xffff);
    xxfwrite(self->fh, self->buffer, self->bufpos);
    self->crc = crc32(self->crc, self->buffer, self->bufpos);
    self->adler = adler32(self->adler, self->buffer + 5, self->bufpos - 5);
    self->buffer[0] = '\0';
    self->bufpos = 5; 
}

static void png4file_addpixel(Png4File * self, eu8 pixel) 
{
    if (self->nibble < 0)
        self->nibble = (pixel << 4);
    else 
    {
        self->buffer[self->bufpos++] = self->nibble | pixel;
        self->nibble = -1;
        if (self->bufpos == sizeof self->buffer - 4)
            png4file_flush(self); 
    }
}

static void png4file_endrow(Png4File * self) 
{
    if (self->nibble >= 0) 
    {
        self->buffer[self->bufpos++] = self->nibble;
        self->nibble = -1; 
    }

    self->hleft--;
    if (self->hleft)
        self->buffer[self->bufpos++] = '\0'; 
}

static void png4file_close(Png4File * self) 
{
    eu8 adlerbuf[4];
    self->buffer[0] = 0x01;
    if (self->bufpos)
        png4file_flush(self);
    else 
    {
        self->bufpos = 5;
        png4file_flush(self); 
    }

    set_uint32_be(adlerbuf, self->adler);
    xxfwrite(self->fh, adlerbuf, 4);
    self->crc = crc32(self->crc, adlerbuf, 4);
    xxfwrite_uint32_be(self->fh, self->crc);
    xxfwriteCS(self->fh, "\0\0\0\0" "IEND" "\xae\x42\x60\x82");
    xfseek0(self->fh, 70);
    xxfwrite_uint32_be(self->fh, self->chunklen + 4 /* adler*/);
    fclose(self->fh);
    self->fh = NULL; 
}

static eu8 getnibble(eu8 ** data, int * nibble) 
{
    if (*nibble >= 0) 
    {
        eu8 rv = *nibble & 0x0f;
        *nibble = -1;
        return rv; 
    }
    /* else */
    *nibble = *(*data)++;
    return *nibble >> 4; 
}


static void getpixelline(eu8 ** data, int width, Png4File * picfile)
{
    int nibble = -1;
    int col = 0;
    int number, cindex;
    /* Originally from gtkspu - this from the python implementation of this */

    while (1) 
    {
        int bits = getnibble(data, &nibble);
        if ((bits & 0xc) != 0) 
        {
            /* have 4-bit code */
            number = (bits & 0xc) >> 2;
            cindex = bits & 0x3; }
        else 
        {
            bits = (bits << 4) | getnibble(data, &nibble);
            if ((bits & 0xf0) != 0) 
            {
                /* have 8-bit code */
                number = (bits & 0x3c) >> 2;
                cindex = bits & 0x3; 
            }
            else 
            {
                bits = (bits << 4) | getnibble(data, &nibble);
                if ((bits & 0xfc0) != 0) 
                {
                    /* have 12-bit code */
                    number = (bits & 0xfc) >> 2;
                    cindex = bits & 0x3; 
                }
                else 
                {
                    /* have 16-bit code */
                    bits = (bits << 4) | getnibble(data, &nibble);
                    number = (bits & 0x3fc) >> 2;
                    cindex = bits & 0x3;

                    if (number == 0)
                        number = width; 
                }
            }
        }

        /* printf("%d %d %d %d\n", number, col, width, cindex); */
        for (; number > 0 && col < width; number--, col++)
            png4file_addpixel(picfile, cindex);

        if (col == width)
        {
            png4file_endrow(picfile);
            return; 
        }
    }
}

static void makebitmap(eu8 * data, int w, int h, int top, int bot,
                       char * filename, eu8 palette[4][3])
{
    eu8 * top_ibuf = data + top;
    eu8 * bot_ibuf = data + bot;
    int i;
    Png4File picfile;

    png4file_init(&picfile, palette); /* not bottleneck even re-doing this every time */
    png4file_open(&picfile, filename, h, w);
    for (i = 0; i < h / 2; i++) 
    {
        getpixelline(&top_ibuf, w, &picfile);
        getpixelline(&bot_ibuf, w, &picfile);
    }

    png4file_close(&picfile); 
}


static char * pts2ts(fu32 pts, char * rvbuf, bool is_png_filename) 
{
    int h = pts / (3600 * 90000);
    int m = pts / (60 * 90000) % 60;
    int s = pts / 90000 % 60;
    int hs = (pts + 450) / 900 % 100;

    if (is_png_filename)
        sprintf(rvbuf, "%02d+%02d+%02d.%02d.png", h, m, s, hs);
    else
        sprintf(rvbuf, "%02d:%02d:%02d.%02d", h, m, s, hs);

    return rvbuf; 
}

/* *********** */

struct _BoundStr 
{
    eu8 * p;
    int l; 
};

static void boundstr_init(BoundStr * bs, eu8 * p, int l) 
{
    bs->p = p;
    bs->l = l; 
}

static eu8 * boundstr_read(BoundStr * bs, int l) 
{
    eu8 * rp;
    if (l > bs->l)
        exc_throw(IndexError, "XXX IndexError %p.", bs);
    rp = bs->p;
    bs->p += l;
    bs->l -= l;
    return rp; 
}

/* *********** */


static void pxsubtitle(const char * supfile, FILE * ofh, eu8 palette[16][3],
                       bool createpics, int delay_ms,
                       char * fnbuf, char * fnbuf_fp) 
{
    char  junk[32];
    char  sptsstr[32];
    eu8   data[65536];
    eu8 * ctrl;
    time_t volatile pt = 0;
    bool volatile last = false;
    /*char  transparent[8]; */
    FILE * sfh = xfopen(supfile, "rb");
    if (memcmp(xxfread(sfh, data, 2), "SP", 2) != 0)
    {
        exc_throw(MiscError, "Syncword missing. XXX bailing out.");
    }

    /*sprintf(transparent,
      "%02x%02x%02x", palette[0][0], palette[0][1], palette[0][2]); */

    exc_try 
    {
        eu32 volatile lastendpts = 0;

        while (1)
        {
            /* X.java reads 5 bytes of pts, SubPicture.java writes 4. With
             * 4 bytes 47721 seconds (13.25 hours) can be handled.
             * Later, bytes 1,2,3,4 (instead of 0,1,2,3) could be used.
             * 256/90000 = 1/351 sec -- way better than required resolution. 
             */
            eu32 pts = get_uint32_le(xxfread(sfh, data, 8));
            eu16 size = get_uint16_be(xxfread(sfh, data, 2));
            eu16 pack = get_uint16_be(xxfread(sfh, data, 2));
            eu32 endpts;
            xxfread(sfh, data, pack - 4);
            ctrl = data + pack - 4;
            xxfread(sfh, ctrl, size - pack);

            exc_try 
            {
                if (memcmp(xxfread(sfh, (unsigned char *)junk, 2),
                           "SP", 2) != 0)
                exc_throw(MiscError, "Syncword missing. XXX bailing out."); 
            }
            exc_catch (EOFIndicator)
            last = true;
            exc_end;
            {
                time_t ct;
                BoundStr bs;
                int  prev;
                int x1 = -1, x2 = -1, y1 = -1, y2 = -1;
                int top_field = -1, bot_field = -1;
                int end = 0;
                int colcon_length;
                eu8 this_palette[4][3];
                boundstr_init(&bs, ctrl, size - pack);

                prev = 0;
                while (1) 
                {
                    int date = get_uint16_be(boundstr_read(&bs, 2));
                    int next = get_uint16_be(boundstr_read(&bs, 2));

                    while (1) 
                    {
                        eu8 * p;
                        eu8 cmd = boundstr_read(&bs, 1)[0];
                        int xpalette, i, n;
                        switch (cmd) 
                        {
                            case 0x00:      /* force display: */
                                continue;
                            case 0x01:      /* start date (read above) */
                                continue;
                            case 0x02:      /* stop date (read above) */
                                end = date;
                                continue;
                            case 0x03:                /* palette */
                                xpalette = get_uint16_be(boundstr_read(&bs, 2));
                                for (n = 0; n < 4; n++) {
                                    i = (xpalette >> (n * 4) & 0x0f);
                                    this_palette[n][0] = palette[i][0];
                                    this_palette[n][1] = palette[i][1];
                                    this_palette[n][2] = palette[i][2];
                                }
                                continue;
                            case 0x04:      /* alpha channel */
                                /*alpha =*/ boundstr_read(&bs, 2);
                                continue;
                            case 0x05:      /* coordinates */
                                p = boundstr_read(&bs, 6);
                                x1 = (p[0] << 4) + (p[1] >> 4);
                                x2 = ((p[1] & 0xf) << 8) + p[2];
                                y1 = (p[3] << 4) + (p[4] >> 4);
                                y2 = ((p[4] & 0xf) << 8) + p[5];
                                continue;
                            case 0x06:		/* rle offsets */
                                top_field = get_uint16_be(boundstr_read(&bs, 2));
                                bot_field = get_uint16_be(boundstr_read(&bs, 2));
                                continue;
                            case 0x07:                /* */
                                colcon_length = get_uint16_be(boundstr_read(&bs, 2)-2);
                                boundstr_read(&bs, colcon_length);
                                continue;
                        }
                        if (cmd == 0xff)	/* end command */
                            break;
                        exc_throw(MiscError, "%d: Unknown control sequence", cmd);
                    }

                    if (prev == next)
                        break;
                    prev = next;
                }

                /* check for overlapping subtitles */
                eu32 tmppts = pts;
                if (delay_ms)
                    tmppts += delay_ms * 90;

                /* hack to fix projectx bug adding wrong end times around a cut point*/
                if (end > 500)
                    end = 500;

                endpts = tmppts + end * 1000; /* ProjectX ! (other: 900, 1024) */

                if (tmppts <= lastendpts)
                {
                    if (lastendpts < endpts)
                    {
                        pts = lastendpts + 2 * (1000 / 30) * 90;/*??? */
                        tmppts = pts;
                        if (delay_ms)
                            tmppts += delay_ms * 90;
                        printf("Fixed overlapping subtitle!\n");
                    }
                    else
                    {
                        printf("Dropping overlapping subtitle\n");
                        continue;
                    }
                }

                lastendpts = endpts;

                pts2ts(pts, fnbuf_fp, true);
                pts2ts(tmppts, sptsstr + 1, false);

                if (pt != time(&ct)) 
                {
                    pt = ct;
                    sptsstr[0] = '\r';
                    size_t len = write(1, sptsstr, strlen(sptsstr));
                    if (len != strlen(sptsstr))
                        printf("ERROR: write failed");
                }

                fprintf(ofh, "  <spu start=\"%s\" end=\"%s\" image=\"%s\""
                        " xoffset=\"%d\" yoffset=\"%d\" />\n",
                        sptsstr + 1, pts2ts(endpts, junk, false),
                        fnbuf, x1, y1);

                if (createpics)
                    makebitmap(data, x2 - x1 + 1, y2 - y1 + 1, top_field - 4,
                               bot_field - 4, fnbuf, this_palette);
                if (last)
                    exc_throw(EOFIndicator, NULL); 
            }
        }
    }
    exc_catch (EOFIndicator) 
    {
        size_t len = write(1, sptsstr, strlen(sptsstr));
        if (len != strlen(sptsstr))
            printf("ERROR: write failed");
        return;
    }
    exc_end;
}

#if 0
static void usage(const char * pn) /* protoadd GCCATTR_NORETURN */
{
    exc_throw(MiscError, "\n"
              "Usage: %s [--delay ms] <supfile> <ifofile>|<palette>" "\n"
              "\n"
              "\tExamples:" "\n"
              "\n"
              "\t ProjectX decoded recording.sup and recording.sup.IFO" "\n"
              "\n"
              "\t\t$ pxsup2dast recording.sup*" "\n"
              "\n"
              "\t Having test.sup and map.ifo" "\n"
              "\n"
              "\t\t$ pxsup2dast test.sup map.ifo" "\n"
              "\n"
              "\t No .IFO, so giving 3 colors (rgb components in hex)" "\n"
              "\n"
              "\t\t$ pxsup2dast titles.sup ff0000,00ff00,0000ff" "\n"
              "\n"
              "\t Trying to fix sync in recording" "\n"
              "\n"
              "\t\t$ pxsup2dast --delay 750 recording.sup*" "\n"
              , pn);
}
#endif

static bool samepalette(char * filename, eu8 palette[16][3])
{
    FILE * fh;
    int i;
    unsigned int r,g,b;

    if (!fexists(filename))
        return false;

    fh = xfopen(filename, "rb");
    for (i = 0; i < 16; i++)
    {
        if (fscanf(fh, "%02x%02x%02x\n", &r, &g, &b) != 3 ||
               r != palette[i][0] || g != palette[i][1] || b != palette[i][2]) 
        {
            fclose(fh);
            return false; 
        }
    }
    fclose(fh);
    return true; 
}

int sup2dast(const char *supfile, const char *ifofile ,int delay_ms)
{
    exc_try 
    {
        int i;
        eu8 yuvpalette[16][3], rgbpalette[16][3];
        char fnbuf[1024];
        char * p;

        bool createpics;
        FILE * fh;

        memset(yuvpalette, 0, sizeof(yuvpalette));
        memset(rgbpalette, 0, sizeof(rgbpalette));

        if (write(1, "\n", 1) != 1)
            printf("ERROR: write failed");

        if (sizeof (char) != 1 || sizeof (int) < 2) /* very unlikely */
            exc_throw(MiscError, "Incompatible variable sizes.");

        if ((p = strrchr(supfile, '.')) != NULL)
            i = p - supfile;
        else
            i = strlen(supfile);
        if (i > 950)
            exc_throw(MiscError, "Can "
                  "not manage filenames longer than 950 characters.");
        memcpy(fnbuf, supfile, i);
        p = fnbuf + i;
        strcpy(p, ".d/");
        p += strlen(p);

        if (!dexists(fnbuf))
            xmkdir(fnbuf, 0755);

        if (fexists(ifofile))
            ifopalette(ifofile, yuvpalette, rgbpalette);
        else
            argpalette(ifofile, yuvpalette, rgbpalette);

        strcpy(p, "palette.ycrcb");
        if (samepalette(fnbuf, yuvpalette)) 
        {
            createpics = false;
            printf("Found palette.yuv having our palette information...\n");
            printf("...Skipping image files, Writing spumux.tmp.\n"); 
        }
        else 
        {
            createpics = true;
            printf("Writing image files and spumux.tmp.\n"); 
        }

        strcpy(p, "spumux.tmp");
        fh = xfopen(fnbuf, "wb");

        xxfwriteCS(fh, "<subpictures>\n <stream>\n");
        pxsubtitle(supfile, fh, rgbpalette, createpics, delay_ms, fnbuf, p);

        if (write(1, "\n", 1) != 1)
            printf("ERROR: write failed");

        xxfwriteCS(fh, " </stream>\n</subpictures>\n");
        fclose(fh);

        if (createpics) 
        {
            FILE * yuvfh, *rgbfh;
            printf("Writing palette.ycrcb and palette.rgb.\n");
            strcpy(p, "palette.ycrcb");
            yuvfh = xfopen(fnbuf, "wb");
            strcpy(p, "palette.rgb");
            rgbfh = xfopen(fnbuf, "wb");
            for (i = 0; i < 16; i++) 
            {
                fprintf(yuvfh, "%02x%02x%02x\n",
                yuvpalette[i][0], yuvpalette[i][1], yuvpalette[i][2]);
                fprintf(rgbfh, "%02x%02x%02x\n",
                rgbpalette[i][0], rgbpalette[i][1], rgbpalette[i][2]);
            }
            fclose(yuvfh);
            fclose(rgbfh); 
        }

        {
            char buf[1024];
            printf("Renaming spumux.tmp to spumux.xml.\n");
            strcpy(p, "spumux.tmp");
            i = strlen(fnbuf);
            memcpy(buf, fnbuf, i);
            strcpy(&buf[i - 3], "xml");
            unlink(buf);
            rename(fnbuf, buf);
        }
        p[0] = '\0';
        printf("Output files reside in %s\n", fnbuf);
        printf("All done.\n"); 
    }

    exc_catchall 
    {
        if (write(2, EXC.msgbuf, EXC.buflen) != EXC.buflen)
            printf("ERROR: write failed");

        if (write(2, "\n", 1) != 1)
            printf("ERROR: write failed");

        return 1;
    }
    exc_endall;

    return 0; 
}
