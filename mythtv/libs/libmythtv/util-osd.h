#ifndef UTIL_OSD_H
#define UTIL_OSD_H

#include "mythlogging.h"
#include "mythimage.h"
#include "mythframe.h"

#define ALIGN_C 2
#ifdef MMX
#define ALIGN_X_MMX 8
#else
#define ALIGN_X_MMX 2
#endif

void yuv888_to_yv12(VideoFrame *frame, MythImage *osd_image,
                    int left, int top, int right, int bottom);
void inline mmx_yuv888_to_yv12(VideoFrame *frame, MythImage *osd_image,
                               int left, int top, int right, int bottom);
void inline c_yuv888_to_yv12(VideoFrame *frame, MythImage *osd_image,
                             int left, int top, int right, int bottom);
void yuv888_to_i44(unsigned char *dest, MythImage *osd_image, QSize dst_size,
                   int left, int top, int right, int bottom, bool ifirst);
#endif

