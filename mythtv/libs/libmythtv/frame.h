/*
 *  frame.h
 */

#ifndef _FRAME_H
#define _FRAME_H 

typedef enum Codec_
{
    CODEC_RGB = 0,
    CODEC_YUV
} Codec;

typedef struct Frame_
{
    Codec codec;
    int height;
    int width;
    int bpp;
    int frameNumber;
    unsigned char *buf;
    int len;
    int timecode;
    int is_field;
} Frame;

#endif // #ifndef _FRAME_H

