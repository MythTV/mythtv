/*
 * Copyright 2003 Chris Morgan <cmorgan@alum.wpi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _H_JACK_OUT_H
#define _H_JACK_OUT_H

#include <jack/jack.h>

#ifndef __cplusplus
#define bool long
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define ERR_SUCCESS                           0
#define ERR_OPENING_JACK                      1
#define ERR_RATE_MISMATCH                     2
#define ERR_BYTES_PER_OUTPUT_FRAME_INVALID    3
#define ERR_BYTES_PER_INPUT_FRAME_INVALID     4
#define ERR_TOO_MANY_OUTPUT_CHANNELS          5
#define ERR_PORT_NAME_OUTPUT_CHANNEL_MISMATCH 6
#define ERR_PORT_NOT_FOUND                    7

enum status_enum { PLAYING, PAUSED, STOPPED, CLOSED, RESET };
enum pos_enum    { BYTES, MILLISECONDS };

#define PLAYED          1 /* played out of the speakers(estimated value but should be close */
#define WRITTEN_TO_JACK 2 /* amount written out to jack */
#define WRITTEN         3 /* amount written to the bio2jack device */

/**********************/
/* External functions */
void JACK_Init(void); /* call this before any other bio2jack calls */
int  JACK_Open(int *deviceID, unsigned int bits_per_sample, unsigned long *rate, int channels);
int  JACK_OpenEx(int *deviceID, unsigned int bits_per_channel,
                 unsigned long *rate,
                 unsigned int input_channels, unsigned int output_channels,
                 const char **jack_port_name, unsigned int jack_port_name_count,
                 unsigned long jack_port_flags);
int  JACK_Close(int deviceID); /* return 0 for success */
void JACK_Reset(int deviceID); /* free all buffered data and reset several values in the device */
long JACK_Write(int deviceID, unsigned char *data, unsigned long bytes); /* returns the number of bytes written */

/* state setting values */
/* set/get the written/played/buffered value based on a byte or millisecond input value */
long JACK_GetPosition(int deviceID, enum pos_enum position, int type);
void JACK_SetPosition(int deviceID, enum pos_enum position, long value);

long JACK_GetJackLatency(int deviceID); /* return the latency in milliseconds of jack */

int JACK_SetState(int deviceID, enum status_enum state); /* playing, paused, stopped */
enum status_enum JACK_GetState(int deviceID);

long JACK_GetMaxBufferedBytes(int deviceID);
void JACK_SetMaxBufferedBytes(int deviceID, long max_buffered_bytes); /* set the max number of bytes the jack driver should buffer */

/* bytes that jack requests during each callback */
unsigned long JACK_GetJackBufferedBytes(int deviceID);

/* Properties of the jack driver */

/* linear means 0 volume is silence, 100 is full volume */
/* dbAttenuation means 0 volume is 0dB attenuation */
/* Bio2jack defaults to linear */
enum JACK_VOLUME_TYPE { linear, dbAttenuation };
enum JACK_VOLUME_TYPE JACK_SetVolumeEffectType(int deviceID,
                                               enum JACK_VOLUME_TYPE type);

int  JACK_SetAllVolume(int deviceID, unsigned int volume); /* returns 0 on success */
int  JACK_SetVolumeForChannel(int deviceID, unsigned int channel, unsigned int volume);
void JACK_GetVolumeForChannel(int deviceID, unsigned int channel, unsigned int *volume);



long JACK_GetOutputBytesPerSecond(int deviceID); /* bytes_per_frame * sample_rate */
long JACK_GetInputBytesPerSecond(int deviceID);  /* bytes_per_frame * sample_rate */
long JACK_GetBytesStored(int deviceID);          /* bytes currently buffered in the device,
                                                  this is a real-time approximation */
long JACK_GetBytesFreeSpace(int deviceID);       /* bytes of free space in the buffers */
long JACK_GetBytesPerOutputFrame(int deviceID);
int  JACK_GetNumInputChannels(int deviceID);
int  JACK_SetNumInputChannels(int deviceID, int channels);
int  JACK_GetNumOutputChannels(int deviceID);
int  JACK_SetNumOutputChannels(int deviceID, int channels);
long JACK_GetSampleRate(int deviceID); /* samples per second */

#endif /* #ifndef JACK_OUT_H */

