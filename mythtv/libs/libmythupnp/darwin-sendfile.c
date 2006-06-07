/*
 * Why doesn't Darwin/OS X include sendfile()?
 *
 * The guts of the file copying was stolen from dcc_pump_readwrite() from:
 * http://www.opensource.apple.com/darwinsource/Current/distcc-31.0.81/src/io.c
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "darwin-sendfile.h"

ssize_t sendfile64(int out_fd, int in_fd, __off64_t *offset, size_t size)
{
    ssize_t written = 0;

    if (*offset)
        if (lseek(in_fd, *offset, SEEK_SET) == -1)
            return -1;

    while (size > 0)
    {
        char    buf[60000], *p;
        ssize_t in_count, out_count, wanted;

        wanted = (size > sizeof buf) ? (sizeof buf) : size;
        in_count = read(in_fd, buf, (size_t) wanted);
         
        if (in_count == -1)
        {
            fprintf(stderr, "sendfile64(): failed to read %ld bytes: %s",
                    (long) wanted, strerror(errno));
            return -1;
        }
        else if (in_count == 0)
            break;  /* EOF - nothing left to read */

        size -= in_count;
        p = buf;

        while (in_count > 0)
        {
            out_count = write(out_fd, p, (size_t) in_count);
            if (out_count == -1 || out_count == 0)
            {
                fprintf(stderr, "sendfile64(): failed to write %ld bytes: %s",
                        (long) in_count, strerror(errno));
                return -1;
            }
            in_count -= out_count;
            p        += out_count;
            written  += out_count;
        }
    }

    *offset += written;
    return written;
}
