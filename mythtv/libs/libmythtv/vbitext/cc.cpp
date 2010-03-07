/* cc.c -- closed caption decoder
 * Robert Kulagowski (rkulagowski@thrupoint.net) 2003-01-02
 * further adopted for MythTV by Erik Arendse
 * essentially, a stripped copy of ntsc-cc.c from xawtv project
 * Mike Baker (mbm@linux.com)
 * (based on code by timecop@japan.co.jp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cstdlib>
#include <cstdarg>
#include <unistd.h>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <fcntl.h>
#include <sys/types.h>
#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "cc.h"

static int parityok(int n)      /* check parity for 2 bytes packed in n */
{
    int mask = 0;
    int j, k;
    for (k = 1, j = 0; j < 7; j++)
    {
        if (n & (1 << j))
            k++;
    }
    if ((k & 1) == ((n >> 7) & 1))
        mask |= 0x00FF;
    for (k = 1, j = 8; j < 15; j++)
    {
        if (n & (1 << j))
            k++;
    }
    if ((k & 1) == ((n >> 15) & 1))
        mask |= 0xFF00;
    return mask;
}


static int decodebit(unsigned char *data, int threshold, int scale1)
{
    int i, sum = 0;
    for (i = 0; i < scale1; i++)
        sum += data[i];
    return (sum > threshold * scale1);
}


static int decode(unsigned char *vbiline, int scale0, int scale1)
{
    int max[7], min[7], val[7], i, clk, tmp, sample, packedbits = 0;

    for (clk = 0; clk < 7; clk++)
        max[clk] = min[clk] = val[clk] = -1;
    clk = tmp = 0;
    i = 30;

    while (i < 600 && clk < 7)
    {   /* find and lock all 7 clocks */
        sample = vbiline[i];
        if (max[clk] < 0)
        {       /* find maximum value before drop */
            if (sample > 85 && sample > val[clk])
                (val[clk] = sample, tmp = i);   /* mark new maximum found */
            else if (val[clk] - sample > 30)    /* far enough */
                (max[clk] = tmp, i = tmp + 10);
        }
        else
        {       /* find minimum value after drop */
            if (sample < 85 && sample < val[clk])
                (val[clk] = sample, tmp = i);   /* mark new minimum found */
            else if (sample - val[clk] > 30)    /* searched far enough */
                (min[clk++] = tmp, i = tmp + 10);
        }
        i++;
    }

    i = min[6] = min[5] - max[5] + max[6];

    if (clk != 7 || vbiline[max[3]] - vbiline[min[5]] < 45)     /* failure to locate clock lead-in */
        return -1;

    /* calculate threshold */
    for (i = 0, sample = 0; i < 7; i++)
        sample = (sample + vbiline[min[i]] + vbiline[max[i]]) / 3;

    for (i = min[6]; vbiline[i] < sample; i++);

    tmp = i + scale0;
    for (i = 0; i < 16; i++)
        if (decodebit(&vbiline[tmp + i * scale0], sample, scale1))
            packedbits |= 1 << i;
    return packedbits & parityok(packedbits);
}

#if 0
static int webtv_check(char *buf, int len)
{
    unsigned long sum;
    unsigned long nwords;
    unsigned short csum = 0;
    char temp[9];
    int nbytes = 0;

    while (buf[0] != '<' && len > 6)    //search for the start
    {
        buf++;
        len--;
    }

    if (len == 6)       //failure to find start
        return 0;

    while (nbytes + 6 <= len)
    {
        //look for end of object checksum, it's enclosed in []'s and there shouldn't be any [' after
        if (buf[nbytes] == '[' && buf[nbytes + 5] == ']' && buf[nbytes + 6] != '[')
            break;
        else
            nbytes++;
    }
    if (nbytes + 6 > len)       //failure to find end
        return 0;

    nwords = nbytes >> 1;
    sum = 0;

    //add up all two byte words
    while (nwords-- > 0)
    {
        sum += *buf++ << 8;
        sum += *buf++;
    }
    if (nbytes & 1)
    {
        sum += *buf << 8;
    }
    csum = (unsigned short) (sum >> 16);
    while (csum != 0)
    {
        sum = csum + (sum & 0xffff);
        csum = (unsigned short) (sum >> 16);
    }
    sprintf(temp, "%04X\n", (int) ~sum & 0xffff);
    buf++;
    if (!strncmp(buf, temp, 4))
    {
        buf[5] = 0;
        printf("\33[35mWEBTV: %s\33[0m\n", buf - nbytes - 1);
        fflush(stdout);
    }
    return 0;
}
#endif

#if 0
// don't use this, blocking IO prevents shuttown of the NVP
struct cc *cc_open(const char *vbi_name)
{
    struct cc *cc;

    if (!(cc = new struct cc))
    {
        printf("out of memory\n");
        return 0;
    }

    if ((cc->fd = open(vbi_name, O_RDONLY)) == -1)
    {
        printf("cannot open vbi device\n");
        free(cc);
        return 0;
    }

    cc->code1 = -1;
    cc->code2 = -1;

    return cc;
}
#endif

void cc_close(struct cc *cc)
{
    close(cc->fd);
    delete cc;
}

// returns:
// 0 = no new data yet
// 1 = new channel 1 data present in outtext
// 2 = new channel 2 data present in outtext
// -1 = invalid cc data, reset all outtext
// -2 = readerror
#if 0
// don't use this, blocking IO prevents shuttown of the NVP
void cc_handler(struct cc *cc)
{
    if (read(cc->fd, cc->buffer, CC_VBIBUFSIZE) != CC_VBIBUFSIZE)
    {
        printf("Can't read vbi data\n");
        cc->code1 = -2;
        cc->code2 = -2;
    }
    else
    {
        cc->code1 = decode((unsigned char *)(cc->buffer + (2048 * 11)));
        cc->code2 = decode((unsigned char *)(cc->buffer + (2048 * 27)));
    }
}
#endif

void cc_decode(struct cc *cc)
{
    int spl    = cc->samples_per_line;
    int sl     = cc->start_line;
    int l21_f1 = spl * (21 - sl);
    int l21_f2 = spl * (cc->line_count + 21 - sl);
    cc->code1 = decode((unsigned char *)(cc->buffer + l21_f1), cc->scale0, cc->scale1);
    cc->code2 = decode((unsigned char *)(cc->buffer + l21_f2), cc->scale0, cc->scale1);
}
