// POSIX headers
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>

// ANSI C headers
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef USING_V4L2
// HACK. Broken kernel headers < 2.6.25 fail compile in videodev2.h when
//       compiling with -std=c99.  We could remove this in the .pro file,
//       but that would disable it for all .c files.
#undef __STRICT_ANSI__
#ifdef USING_V4L1
#include <linux/videodev.h>
#endif // USING_V4L1
#include <linux/videodev2.h>
#endif // USING_V4L2

// vbitext headers
#include "vt.h"
#include "vbi.h"
#include "hamm.h"

#define FAC    (1<<16)         // factor for fix-point arithmetic

static unsigned char *rawbuf;          // one common buffer for raw vbi data.
#ifdef USING_V4L2
static int rawbuf_size;                // its current size
#endif // USING_V4L2

/***** bttv api *****/
#define BTTV_VBISIZE           _IOR('v' , BASE_VIDIOCPRIVATE+8, int)


static void
error(const char *str, ...)
{
    va_list ap;

    va_start(ap, str);
    vfprintf(stderr, str, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void
out_of_sync(struct vbi *vbi)
{
    int i;

    // discard all in progress pages
    for (i = 0; i < 8; ++i)
       vbi->rpage[i].page->flags &= ~PG_ACTIVE;
}


// send an event to all clients

static void
vbi_send(struct vbi *vbi, int type, int i1, int i2, int i3, void *p1)
{
    struct vt_event ev[1];
    struct vbi_client *cl, *cln;

    ev->resource = vbi;
    ev->type = type;
    ev->i1 = i1;
    ev->i2 = i2;
    ev->i3 = i3;
    ev->p1 = p1;

    for (cl = (void*)vbi->clients->first; (cln = (void*)cl->node->next); 
         (cl = cln))
       cl->handler(cl->data, ev);
}

static void
vbi_send_page(struct vbi *vbi, struct raw_page *rvtp, int page)
{
    struct vt_page *cvtp = 0;

    if (rvtp->page->flags & PG_ACTIVE)
    {
       if (rvtp->page->pgno % 256 != page)
       {
           rvtp->page->flags &= ~PG_ACTIVE;
           enhance(rvtp->enh, rvtp->page);
//         if (vbi->cache)
//             cvtp = vbi->cache->op->put(vbi->cache, rvtp->page);
           vbi_send(vbi, EV_PAGE, 0, 0, 0, cvtp ?: rvtp->page);
       }
    }
}

// fine tune pll
// this routines tries to adjust the sampling point of the decoder.
// it collects parity and hamming errors and moves the sampling point
// a 10th of a bitlength left or right.

#define PLL_SAMPLES    4       // number of err vals to collect
#define PLL_ERROR      4       // if this err val is crossed, readjust
//#define PLL_ADJUST   4       // max/min adjust (10th of bitlength)

static void
pll_add(struct vbi *vbi, int n, int err)
{
    if (vbi->pll_fixed)
       return;

    if (err > PLL_ERROR*2/3)   // limit burst errors
       err = PLL_ERROR*2/3;

    vbi->pll_err += err;
    vbi->pll_cnt += n;
    if (vbi->pll_cnt < PLL_SAMPLES)
       return;

    if (vbi->pll_err > PLL_ERROR)
    {
       if (vbi->pll_err > vbi->pll_lerr)
           vbi->pll_dir = -vbi->pll_dir;
       vbi->pll_lerr = vbi->pll_err;

       vbi->pll_adj += vbi->pll_dir;
       if (vbi->pll_adj < -PLL_ADJUST || vbi->pll_adj > PLL_ADJUST)
       {
           vbi->pll_adj = 0;
           vbi->pll_dir = -1;
           vbi->pll_lerr = 0;
       }

#ifdef DEBUG
           printf("pll_adj = %2d\n", vbi->pll_adj);
#endif
    }
    vbi->pll_cnt = 0;
    vbi->pll_err = 0;
}

void
vbi_pll_reset(struct vbi *vbi, int fine_tune)
{
    vbi->pll_fixed = fine_tune >= -PLL_ADJUST && fine_tune <= PLL_ADJUST;

    vbi->pll_err = 0;
    vbi->pll_lerr = 0;
    vbi->pll_cnt = 0;
    vbi->pll_dir = -1;
    vbi->pll_adj = 0;
    if (vbi->pll_fixed)
       vbi->pll_adj = fine_tune;
#ifdef DEBUG
       if (vbi->pll_fixed)
           printf("pll_reset (fixed@%d)\n", vbi->pll_adj);
       else
           printf("pll_reset (auto)\n");
#endif
}

// process one videotext packet

static int
vt_line(struct vbi *vbi, unsigned char *p)
{
    struct vt_page *cvtp;
    struct raw_page *rvtp;
    int hdr, mag, mag8, pkt, i;
    int err = 0;

    hdr = hamm16(p, &err);
    if (err & 0xf000)
       return -4;

    mag = hdr & 7;
    mag8 = mag?: 8;
    pkt = (hdr >> 3) & 0x1f;
    p += 2;

    rvtp = vbi->rpage + mag;
    cvtp = rvtp->page;

    switch (pkt)
    {
       case 0:
       {
           int b1, b2, b3, b4;

           b1 = hamm16(p, &err);       // page number
           b2 = hamm16(p+2, &err);     // subpage number + flags
           b3 = hamm16(p+4, &err);     // subpage number + flags
           b4 = hamm16(p+6, &err);     // language code + more flags

           if (vbi->ppage->page->flags & PG_MAGSERIAL)
               vbi_send_page(vbi, vbi->ppage, b1);
           vbi_send_page(vbi, rvtp, b1);

           if (err & 0xf000)
               return 4;

           cvtp->errors = (err >> 8) + chk_parity(p + 8, 32);;
           cvtp->pgno = mag8 * 256 + b1;
           cvtp->subno = (b2 + b3 * 256) & 0x3f7f;
           cvtp->lang = "\0\4\2\6\1\5\3\7"[b4 >> 5] + (latin1 ? 0 : 8);
           cvtp->flags = b4 & 0x1f;
           cvtp->flags |= b3 & 0xc0;
           cvtp->flags |= (b2 & 0x80) >> 2;
           cvtp->lines = 1;
           cvtp->flof = 0;
           vbi->ppage = rvtp;

           pll_add(vbi, 1, cvtp->errors);

           conv2latin(p + 8, 32, cvtp->lang);
           vbi_send(vbi, EV_HEADER, cvtp->pgno, cvtp->subno, cvtp->flags, p);

           if (b1 == 0xff)
               return 0;

           cvtp->flags |= PG_ACTIVE;
           init_enhance(rvtp->enh);
           memcpy(cvtp->data[0]+0, p, 40);
           memset(cvtp->data[0]+40, ' ', sizeof(cvtp->data)-40);
           return 0;
       }

       case 1 ... 24:
       {
           pll_add(vbi, 1, err = chk_parity(p, 40));

           if (~cvtp->flags & PG_ACTIVE)
               return 0;

           cvtp->errors += err;
           cvtp->lines |= 1 << pkt;
           conv2latin(p, 40, cvtp->lang);
           memcpy(cvtp->data[pkt], p, 40);
           return 0;
       }
       case 26:
       {
           int d, t[13];

           if (~cvtp->flags & PG_ACTIVE)
               return 0;


           d = hamm8(p, &err);
           if (err & 0xf000)
               return 4;

           for (i = 0; i < 13; ++i)
               t[i] = hamm24(p + 1 + 3*i, &err);
           if (err & 0xf000)
               return 4;

           //printf("enhance on %x/%x\n", cvtp->pgno, cvtp->subno);
           add_enhance(rvtp->enh, d, (unsigned int *)t);
           return 0;
       }
       case 27:
       {
           // FLOF data (FastText)
           int b1,b2,b3,x;

           if (~cvtp->flags & PG_ACTIVE)
               return 0; // -1 flushes all pages.  we may never resync again :(

           b1 = hamm8(p, &err);
           b2 = hamm8(p + 37, &err);
           if (err & 0xf000)
               return 4;
           if (b1 != 0 || !(b2 & 8))
               return 0;

           for (i = 0; i < 6; ++i)
           {
               err = 0;
               b1 = hamm16(p+1+6*i, &err);
               b2 = hamm16(p+3+6*i, &err);
               b3 = hamm16(p+5+6*i, &err);
               if (err & 0xf000)
                   return 1;
               x = (b2 >> 7) | ((b3 >> 5) & 0x06);
               cvtp->link[i].pgno = ((mag ^ x) ?: 8) * 256 + b1;
               cvtp->link[i].subno = (b2 + b3 * 256) & 0x3f7f;
           }
           cvtp->flof = 1;
           return 0;
       }
       case 30:
       {
           if (mag8 != 8)
               return 0;

           p[0] = hamm8(p, &err);              // designation code
           p[1] = hamm16(p+1, &err);           // initial page
           p[3] = hamm16(p+3, &err);           // initial subpage + mag
           p[5] = hamm16(p+5, &err);           // initial subpage + mag
           if (err & 0xf000)
               return 4;

           err += chk_parity(p+20, 20);
           conv2latin(p+20, 20, 0);

           vbi_send(vbi, EV_XPACKET, mag8, pkt, err, p);
           return 0;
       }
       default:
           // unused at the moment...
           //vbi_send(vbi, EV_XPACKET, mag8, pkt, err, p);
           return 0;
    }
    return 0;
}



// process one raw vbi line

static int
vbi_line(struct vbi *vbi, unsigned char *p)
{
    unsigned char data[43], min, max;
    int dt[256], hi[6], lo[6];
    int i, n, sync, thr;
    int bpb = vbi->bpb;

    /* remove DC. edge-detector */
    for (i = vbi->soc; i < vbi->eoc; ++i)
       dt[i] = p[i+bpb/FAC] - p[i];    // amplifies the edges best.

    /* set barrier */
    for (i = vbi->eoc; i < vbi->eoc+16; i += 2)
       dt[i] = 100, dt[i+1] = -100;

    /* find 6 rising and falling edges */
    for (i = vbi->soc, n = 0; n < 6; ++n)
    {
       while (dt[i] < 32)
           i++;
       hi[n] = i;
       while (dt[i] > -32)
           i++;
       lo[n] = i;
    }
    if (i >= vbi->eoc)
       return -1;      // not enough periods found

    i = hi[5] - hi[1]; // length of 4 periods (8 bits)
    if (i < vbi->bp8bl || i > vbi->bp8bh)
       return -1;      // bad frequency

    /* AGC and sync-reference */
    min = 255, max = 0, sync = 0;
    for (i = hi[4]; i < hi[5]; ++i)
       if (p[i] > max)
           max = p[i], sync = i;
    for (i = lo[4]; i < lo[5]; ++i)
       if (p[i] < min)
           min = p[i];
    thr = (min + max) / 2;

    p += sync;

    /* search start-byte 11100100 */
    for (i = 4*bpb + vbi->pll_adj*bpb/10; i < 16*bpb; i += bpb)
       if (p[i/FAC] > thr && p[(i+bpb)/FAC] > thr) // two ones is enough...
       {
           /* got it... */
           memset(data, 0, sizeof(data));

           for (n = 0; n < 43*8; ++n, i += bpb)
               if (p[i/FAC] > thr)
                   data[n/8] |= 1 << (n%8);

           if (data[0] != 0x27)        // really 11100100? (rev order!)
               return -1;

           if ((i = vt_line(vbi, data+1)))
           {
               if (i < 0)
                   pll_add(vbi, 2, -i);
               else
                   pll_add(vbi, 1, i);
           }
           return 0;
       }
    return -1;
}



// called when new vbi data is waiting

void
vbi_handler(struct vbi *vbi, int fd)
{
    int n, i;
    unsigned int seq;

    (void)fd;

    n = read(vbi->fd, rawbuf, vbi->bufsize);

    if (dl_empty(vbi->clients))
       return;

    if (n != vbi->bufsize)
       return;

    seq = *(unsigned int *)&rawbuf[n - 4];
    if (vbi->seq+1 != seq)
    {
       out_of_sync(vbi);
       if (seq < 3 && vbi->seq >= 3)
           vbi_reset(vbi);
    }
    vbi->seq = seq;

    if (seq > 1)       // the first may contain data from prev channel
    {
#if 1
       for (i = 0; i+vbi->bpl <= n; i += vbi->bpl)
           vbi_line(vbi, rawbuf + i);
#else
        /* work-around for old saa7134 driver versions (prior 0.2.6) */
       for (i = 16 * vbi->bpl; i + vbi->bpl <= n; i += vbi->bpl)
           vbi_line(vbi, rawbuf + i);

       for (i = 0; i + vbi->bpl <= 16 * vbi->bpl; i += vbi->bpl)
           vbi_line(vbi, rawbuf + i);
#endif
    }
}



int
vbi_add_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    if (!(cl = malloc(sizeof(*cl))))
       return -1;
    cl->handler = handler;
    cl->data = data;
    // cl is not leaking, the first struct element has the same address
    // as the struct
    dl_insert_last(vbi->clients, cl->node);
    return 0;
}



void
vbi_del_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    for (cl = (void*) vbi->clients->first; cl->node->next; cl = (void*) cl->node->next)
       if (cl->handler == handler && cl->data == data)
       {
           dl_remove(cl->node);
           break;
       }
    return;
}

#ifdef USING_V4L2
static int
set_decode_parms(struct vbi *vbi, struct v4l2_vbi_format *p)
{
    double fs;         // sampling rate
    double bpb;                // bytes per bit
    int soc, eoc;      // start/end of clock run-in
    int bpl;           // bytes per line

    if (p->sample_format != V4L2_PIX_FMT_GREY)
    {
       fprintf(stderr, "got pix fmt %x\n", p->sample_format);
       error("v4l2: unsupported vbi data format");
       return -1;
    }

    // some constants from the standard:
    //   horizontal frequency                  fh = 15625Hz
    //   teletext bitrate                      ft = 444*fh = 6937500Hz
    //   teletext identification sequence      10101010 10101010 11100100
    //   13th bit of seq rel to falling hsync  12us -1us +0.4us
    // I search for the clock run-in (10101010 10101010) from 12us-1us-12.5/ft
    // (earliest first bit) to 12us+0.4us+3.5/ft (latest last bit)
    //   earlist first bit                     tf = 12us-1us-12.5/ft = 9.2us
    //   latest last bit                       tl = 12us+0.4us+3.5/ft = 12.9us
    //   total number of used bits             n = (2+1+2+40)*8 = 360

    bpl = p->samples_per_line;
    fs = p->sampling_rate;
    bpb = fs/6937500.0;
    soc = (int)(9.2e-6*fs) - (int)p->offset;
    eoc = (int)(12.9e-6*fs) - (int)p->offset;
    if (soc < 0)
       soc = 0;
    if (eoc > bpl - (int)(43*8*bpb))
       eoc = bpl - (int)(43*8*bpb);
    if (eoc - soc < (int)(16*bpb))
    {
       // line too short or offset too large or wrong sample_rate
       error("v4l2: broken vbi format specification");
       return -1;
    }
    if (eoc > 240)
    {
       // the vbi_line routine can hold max 240 values in its work buffer
       error("v4l2: unable to handle these sampling parameters");
       return -1;
    }

    vbi->bpb = bpb * FAC + 0.5;
    vbi->soc = soc;
    vbi->eoc = eoc;
    vbi->bp8bl = 0.97 * 8*bpb; // -3% tolerance
    vbi->bp8bh = 1.03 * 8*bpb; // +3% tolerance

    vbi->bpl = bpl;
    vbi->bufsize = bpl * (p->count[0] + p->count[1]);

    return 0;
}
#endif // USING_V4L2

static int
setup_dev(struct vbi *vbi)
{
#ifdef USING_V4L2
    struct v4l2_format v4l2_format;
    struct v4l2_vbi_format *vbifmt = &v4l2_format.fmt.vbi;

    memset(&v4l2_format, 0, sizeof(v4l2_format));
    v4l2_format.type = V4L2_BUF_TYPE_VBI_CAPTURE;
    if (ioctl(vbi->fd, VIDIOC_G_FMT, &v4l2_format) == -1)
    {
#ifdef USING_V4L1
       // not a v4l2 device.  assume bttv and create a standard fmt-struct.
       int size;
       perror("ioctl VIDIOC_G_FMT");

       vbifmt->sample_format = V4L2_PIX_FMT_GREY;
       vbifmt->sampling_rate = 35468950;
       vbifmt->samples_per_line = 2048;
       vbifmt->offset = 244;
       if ((size = ioctl(vbi->fd, BTTV_VBISIZE, 0)) == -1)
       {
           // BSD or older bttv driver.
           vbifmt->count[0] = 16;
           vbifmt->count[1] = 16;
       }
       else if (size % 2048)
       {
           error("broken bttv driver (bad buffer size)");
           return -1;
       }
       else
       {
           size /= 2048;
           vbifmt->count[0] = size/2;
           vbifmt->count[1] = size - size/2;
       }
#else
       error("Video 4 Linux version 1 support is not enabled.");
       return -1;
#endif
    }

    if (set_decode_parms(vbi, vbifmt) == -1)
       return -1;

    if (vbi->bpl < 1 || vbi->bufsize < vbi->bpl || vbi->bufsize % vbi->bpl != 0)
    {
       error("strange size of vbi buffer (%d/%d)", vbi->bufsize, vbi->bpl);
       return -1;
    }

    // grow buffer if necessary
    if (rawbuf_size < vbi->bufsize)
    {
       if (rawbuf)
           free(rawbuf);
       if (!(rawbuf = malloc(rawbuf_size = vbi->bufsize)))
            error("malloc refused in setup_dev()\n");
    }

    return 0;
#else
    return -1;
#endif // USING_V4L2
}



struct vbi *
vbi_open(const char *vbi_name, struct cache *ca, int fine_tune, int big_buf)
{
    static int inited = 0;
    struct vbi *vbi = 0;

    (void)ca;

    if (! inited)
       lang_init();
    inited = 1;

    if (!(vbi = malloc(sizeof(*vbi))))
    {
       error("out of memory");
       goto fail1;
    }

    if ((vbi->fd = open(vbi_name, O_RDONLY)) == -1)
    {
       error("cannot open vbi device");
       goto fail2;
    }

    if (big_buf != -1)
       error("-oldbttv/-newbttv is obsolete.  option ignored.");

    if (setup_dev(vbi) == -1)
       goto fail3;

//    vbi->cache = ca;

    dl_init(vbi->clients);
    vbi->seq = 0;
    out_of_sync(vbi);
    vbi->ppage = vbi->rpage;

    vbi_pll_reset(vbi, fine_tune);
// ECA removed    fdset_add_fd(fds, vbi->fd, vbi_handler, vbi);
    return vbi;

fail3:
    close(vbi->fd);
fail2:
    free(vbi);
fail1:
    return 0;
}



void
vbi_close(struct vbi *vbi)
{
//    fdset_del_fd(fds, vbi->fd);
//    if (vbi->cache)
//     vbi->cache->op->close(vbi->cache);
    close(vbi->fd);
    free(vbi);
}


struct vt_page *
vbi_query_page(struct vbi *vbi, int pgno, int subno)
{
    struct vt_page *vtp = 0;

    (void)pgno;
    (void)subno;

//    if (vbi->cache)
//     vtp = vbi->cache->op->get(vbi->cache, pgno, subno);
    if (vtp == 0)
    {
       // EV_PAGE will come later...
       return 0;
    }

    vbi_send(vbi, EV_PAGE, 1, 0, 0, vtp);
    return vtp;
}

void
vbi_reset(struct vbi *vbi)
{
//    if (vbi->cache)
//     vbi->cache->op->reset(vbi->cache);
    vbi_send(vbi, EV_RESET, 0, 0, 0, 0);
}

