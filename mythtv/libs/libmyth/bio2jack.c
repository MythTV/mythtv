/*
 * Copyright 2003-2004 Chris Morgan <cmorgan@alum.wpi.edu>
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

/* NOTE: All functions that take a jack_driver_t* do NOT lock the device, in order to get a */
/*       jack_driver_t* you must call getDriver() which will pthread_mutex_lock() */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
#include <jack/jack.h>
#include <pthread.h>
#include <sys/time.h>

#include "bio2jack.h"

//FIXME: this is only true if we actually have messages
//FIXME: probably want to send all state changing messages like:
// setting reset, stopped, paused etc through messages
// if we don't do this we run the risk of setting reset, then
// sending a message and having things execute not in the order that
// the user sent them

/* enable/disable TRACING through the JACK_Callback() function */
/* this can sometimes be too much information */
/* NOTE: need to enable the general tracing as well */
#define CALLBACK_TRACE          0

/* set to 1 for verbose output */
#define VERBOSE_OUTPUT          0

/* set to 1 to enable tracing */
#define TRACE_ENABLE            0

/* set to 1 to enable tracing of getDriver() and releaseDriver() */
#define TRACE_getReleaseDevice  0


#define OUTFILE stderr

#if TRACE_ENABLE
    #define TRACE(...) fprintf(OUTFILE, "%s:", __FUNCTION__),	\
         fprintf(OUTFILE, __VA_ARGS__),				\
         fflush(OUTFILE);
#else
    #define TRACE(...) do{}while(0)
#endif

    #define ERR(...) fprintf(OUTFILE, "ERR: %s:", __FUNCTION__),\
         fprintf(OUTFILE, __VA_ARGS__),				\
         fflush(OUTFILE);

#define min(a,b)   (((a) < (b)) ? (a) : (b))
#define max(a,b)   (((a) < (b)) ? (b) : (a))


/* Structure that holds a packet of wave output data from the client */
typedef struct wave_header_s
{
  unsigned char* pData;        /* pointer to the audio data */
  long  size;                  /* number of bytes pointed to by pData */

  struct wave_header_s *pNext; /* pointer to the next wave_header_s */
} wave_header_t;


enum cmd_enum { CMD_SET_POSITION };

/* message passing structure */
typedef struct message_s
{
  enum cmd_enum    command;
  long             data;
  
  struct message_s *pNext;     /* pointer to the next message */
} message_t;


#define MAX_OUTPUT_PORTS  10

typedef struct jack_driver_s
{
  int              deviceID;                      /* id of this device */
  long             sample_rate;                   /* samples(frames) per second */
  unsigned long    num_input_channels;            /* number of input channels(1 is mono, 2 stereo etc..) */
  unsigned long    num_output_channels;           /* number of output channels(1 is mono, 2 stereo etc..) */
  unsigned long    bits_per_channel;
  unsigned long    bytes_per_output_frame;        /* (num_output_channels * bits_per_channel) / 8 */
  unsigned long    bytes_per_input_frame;         /* (num_input_channels * bits_per_channel) / 8 */

  unsigned long    latencyMS;                     /* latency in ms between writing and actual audio output of the written data */

  long             clientBytesInJack;             /* number of INPUT bytes(from the client of bio2jack) we wrote to jack(not necessary the number of bytes we wrote to jack) */
  unsigned long    buffer_size;                   /* number of bytes in the buffer allocated for processing data in JACK_Callback */

  unsigned char*   sound_buffer;
  struct timeval   previousTime;                  /* time of last JACK_Callback() write to jack, allows for MS accurate bytes played  */

  unsigned long    written_client_bytes;          /* input bytes we wrote to jack, not necessarily actual bytes we wrote to jack due to channel and other conversion */
  unsigned long    played_client_bytes;           /* input bytes that jack has played */

  unsigned long    client_bytes;                  /* total bytes written by the client of bio2jack via JACK_Write() */

  jack_port_t*     output_port[MAX_OUTPUT_PORTS]; /* output ports */
  jack_client_t*   client;                        /* pointer to jack client */

  char             **jack_port_name;              /* user given strings for the port names, can be NULL */
  unsigned int     jack_port_name_count;          /* the number of port names given */
  unsigned long    jack_port_flags;               /* flags to be passed to jack when opening the output ports */

  wave_header_t*   pPlayPtr;                      /* pointer to the current wave header */
  long             playptr_offset;                /* offset to not yet written bytes in pPlayPtr */

  enum status_enum state;                         /* one of PLAYING, PAUSED, STOPPED, CLOSED, RESET etc*/

  unsigned int     volume[MAX_OUTPUT_PORTS];      /* percentage of sample value to preserve, 100 would be no attenuation */
  enum JACK_VOLUME_TYPE volumeEffectType;         /* linear or dbAttenuation, if dbAttenuation volume is the number of dBs of
                                                     attenuation to apply, 0 volume being no attenation, full volume */

  long             position_byte_offset;          /* an offset that we will apply to returned position queries to achieve */
                                                  /* the position that the user of the driver desires set */

  bool             in_use;                        /* true if this device is currently in use */

  message_t        *pMessages;                    /* linked list of messages we are sending to the callback process */

  pthread_mutex_t  mutex;

  /* variables used for trying to restart the connection to jack */
  bool             jackd_died;                    /* true if jackd has died and we should try to restart it */
  struct timeval   last_reconnect_attempt;
} jack_driver_t;


/* enable/disable code that allows us to close a device without actually closing the jack device */
/* this works around the issue where jack doesn't always close devices by the time the close function call returns */
#define JACK_CLOSE_HACK 1

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t              nframes_t;

/* allocate devices for output */
static int first_free_device = 0;
#define MAX_OUTDEVICES 10
static jack_driver_t outDev[MAX_OUTDEVICES];

/* default audio buffer size */
#define SECONDS .25
static long MAX_BUFFERED_BYTES = (long)(16 * 2 * (44100/(1/SECONDS))) / 8; /* 16 bits, 2 channels .25 seconds */

#if JACK_CLOSE_HACK
static void	JACK_CloseDevice(jack_driver_t* drv, bool close_client);
#else
static void	JACK_CloseDevice(jack_driver_t* drv);
#endif


/* Prototypes */
static int JACK_OpenDevice(jack_driver_t* drv);
static long JACK_GetBytesFreeSpaceFromDriver(jack_driver_t *drv);
static void JACK_ResetFromDriver(jack_driver_t *drv);
static long JACK_GetPositionFromDriver(jack_driver_t *drv, enum pos_enum position, int type);




/* Return the difference between two timeval structures in terms of milliseconds */
long TimeValDifference(struct timeval *start, struct timeval *end)
{
  double long ms; /* milliseconds value */

  ms = end->tv_sec - start->tv_sec; /* compute seconds difference */
  ms*=(double)1000; /* convert to milliseconds */
  
  ms+=(double)(end->tv_usec - start->tv_usec) / (double)1000; /* add on microseconds difference */

  return (long)ms;
}

/* get a device and lock the devices mutex */
/* */
/* also attempt to reconnect to jack since this function is called */
/* from most other bio2jack functions it provides a good point to attempt */
/* reconnection */
jack_driver_t *getDriver(int deviceID)
{
    jack_driver_t *drv = &outDev[deviceID];
#if TRACE_getReleaseDevice
    TRACE("deviceID == %d\n", deviceID);
#endif
    pthread_mutex_lock(&drv->mutex);

    /* should we try to restart the jack server? */
    if(drv->jackd_died && drv->client == 0)
    {
        struct timeval now;
        gettimeofday(&now, 0);
        
        /* wait 250ms before trying again */
        if(TimeValDifference(&drv->last_reconnect_attempt, &now) >= 250)
        {
            JACK_OpenDevice(drv);
            drv->last_reconnect_attempt = now;
        }
    }

    return drv;
}

/* release a device's mutex */
void releaseDriver(jack_driver_t *drv)
{
#if TRACE_getReleaseDevice
    TRACE("deviceID == %d\n", drv->deviceID);
#endif
    pthread_mutex_unlock(&drv->mutex);
}


/* Return a string corresponding to the input state */
char* DEBUGSTATE(enum status_enum state)
{
  if(state == PLAYING)
    return "PLAYING";
  else if(state == PAUSED)
    return "PAUSED";
  else if(state == STOPPED)
    return "STOPPED";
  else if(state == CLOSED)
    return "CLOSED";
  else if(state == RESET)
    return "RESET";
  else
    return "unknown state";
}


#define SAMPLE_MAX_16BIT  32767.0f

/* floating point volume routine */
/* volume should be a value between 0.0 and 1.0 */
static void float_volume_effect(sample_t *buf, unsigned long nsamples, float volume)
{
  if(volume < 0)   volume = 0;
  if(volume > 1.0) volume = 1.0;

  while (nsamples--)
  {
    *buf = (*buf) * volume;
    buf++;
  }    
}

/* convert from any number of source channels to any number of destination channels */
static void sample_move_d16_d16(short *dst, short *src,
				unsigned long nsamples, int nDstChannels, int nSrcChannels)
{
  int nSrcCount, nDstCount;

  TRACE("nsamples == %ld, nDstChannels == %d, nSrcChannels == %d\n", nsamples, nDstChannels, nSrcChannels);

  if(!nSrcChannels && !nDstChannels)
  {
    ERR("nSrcChannels of %d, nDstChannels of %d, can't have zero channels\n", nSrcChannels, nDstChannels);
    return; /* in the off chance we have zero channels somewhere */
  }

  while(nsamples--)
  {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;

    /* loop until all of our destination channels are filled */
    while(nDstCount)
    {
      nSrcCount--;
      nDstCount--;

      *dst = *src; /* copy the data over */
      dst++;
      src++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if(!nSrcCount && nDstCount)
      {
          src-=nSrcChannels;
          nSrcCount = nSrcChannels;
      }
    }
    
    /* advance the the position */
    src+=nSrcCount;
    dst+=nDstCount;
  }
}

/* convert from 16 bit to floating point */
/* channels to a buffer that will hold a single channel stream */
/* src_skip is in terms of 16bit samples */
static void sample_move_d16_s16 (sample_t *dst, short *src,
                        unsigned long nsamples, unsigned long src_skip)
{
  /* ALERT: signed sign-extension portability !!! */
  while (nsamples--)
  {
    *dst = (*src) / SAMPLE_MAX_16BIT;
    dst++;
    src += src_skip;
  }
}

/* fill dst buffer with nsamples worth of silence */
void sample_silence_dS (sample_t *dst, unsigned long nsamples)
{
  /* ALERT: signed sign-extension portability !!! */
  while (nsamples--)
  {
    *dst = 0;
    dst++;
  }
}

/******************************************************************
 *    JACK_callback
 *
 * everytime the jack server wants something from us it calls this
 * function, so we either deliver it some sound to play or deliver it nothing
 * to play
 */
static int JACK_callback (nframes_t nframes, void *arg)
{
  sample_t* out_buffer[MAX_OUTPUT_PORTS];
  jack_driver_t* drv = (jack_driver_t*)arg;
  unsigned int i;

  gettimeofday(&drv->previousTime, 0); /* record the current time */

#if CALLBACK_TRACE
  TRACE("nframes %ld, sizeof(sample_t) == %d\n", (long)nframes, sizeof(sample_t));
#endif

  if(!drv->client)
    ERR("client is closed, this is weird...\n");

  /* retrieve the buffers for the output ports */
  for(i = 0; i < drv->num_output_channels; i++)
    out_buffer[i] = (sample_t *) jack_port_get_buffer(drv->output_port[i], nframes);

#if 0
  /* process one message */
  if(drv->pMessages)
  {
    message_t *msg = drv->pMessages;

    /* process a set position command */
    if(msg->command == CMD_SET_POSITION)
    {
      /* set the position byte offset based on the client_bytes and the desired */
      /* offset */
      drv->position_byte_offset = msg->data - drv->client_bytes;

      /* make all positions the same location */
      //FIXME: add the two other counters to this as well
      drv->written_jack_bytes = drv->played_bytes = drv->client_bytes;

      drv->clientBytesInJack = 0; /* no bytes left in jack else we will get bad byte counts */

      if(drv->pPlayPtr != 0)
      {
          TRACE("ERROR, setting position but pPlayPtr != 0\n");
          TRACE("state == %s\n", DEBUGSTATE(drv->state));
      }
      
      TRACE("deviceID(%d), setting position to byte offset of %ld\n", drv->deviceID, msg->data);
    }

#if 0
    if(msg->command == WRITTEN)
    {
      drv->client_bytes = msg->data;
      TRACE("deviceID(%d), setting client_bytes to %d\n", drv->deviceID, drv->client_bytes);
    } else if(msg->command == WRITTEN_TO_JACK)
    {
      drv->written_jack_bytes = msg->data;
      TRACE("deviceID(%d), setting written_jack_bytes to %d\n", drv->deviceID, drv->written_jack_bytes);
    }
    else if(msg->command == PLAYED) /* type is PLAYED */
    {
      drv->played_bytes = msg->data;
      TRACE("deviceID(%d), setting played_bytes to %d\n", drv->deviceID, drv->played_bytes);
    } else
    {
      ERR("unknown type for drv->setType\n");
    }
#endif
    drv->pMessages = msg->pNext;    /* take this message off of the queue */
    free(msg);                 /* free up its memory */
  }  
#endif

  /* handle playing state */
  if(drv->state == PLAYING)
  {
    unsigned long jackFramesAvailable = nframes; /* frames we have left to write to jack */
    unsigned long inputFramesAvailable;          /* frames we have available this loop */
    unsigned long numFramesToWrite;              /* num frames we are writing this loop */

    long written, read;
    unsigned char* buffer;

    written = read = 0;

#if CALLBACK_TRACE
    TRACE("playing... jackFramesAvailable = %ld\n", jackFramesAvailable);
#endif

#if JACK_CLOSE_HACK
    if(drv->in_use == FALSE)
    {
      /* output silence if nothing is being outputted */
      for(i = 0; i < drv->num_output_channels; i++)
          sample_silence_dS(out_buffer[i], nframes);

      return 0;
    }
#endif

    /* see if our buffer is large enough for the data we are writing */
    /* ie. Buffer_size < (bytes we already wrote + bytes we are going to write in this loop) */
    /* Note: sound_buffer is always filled with 16-bit data */
    /* so frame * 2 bytes(16 bits) * X output channels */
    if(drv->buffer_size < (jackFramesAvailable * sizeof(short) * drv->num_output_channels))
    {
      ERR("our buffer must have changed size\n");
      ERR("allocated %ld bytes, need %ld bytes\n", drv->buffer_size,
	  jackFramesAvailable * sizeof(short) * drv->num_output_channels);
      return 0;
    }

    /* while we have jackBytesLeft and a wave header to be played */
    while(jackFramesAvailable && drv->pPlayPtr)
    {
      /* (bytes of data) / (2 bytes(16 bits) * X input channels) == frames */
      if(drv->num_input_channels != 0)
          inputFramesAvailable = (drv->pPlayPtr->size - drv->playptr_offset) / (sizeof(short) * drv->num_input_channels);
      else
          inputFramesAvailable = 0;

#if CALLBACK_TRACE
      TRACE("inputFramesAvailable == %ld, jackFramesAvailable == %ld\n", inputFramesAvailable, jackFramesAvailable);
#endif

      buffer = drv->pPlayPtr->pData + drv->playptr_offset;

      /* convert to actual frames based on the format */
      if(drv->bits_per_channel == 8)
        inputFramesAvailable<<=1; /* multiply by 2 to get actual frames after conversion from 8 to 16 bits */

      numFramesToWrite = min(jackFramesAvailable, inputFramesAvailable); /* write as many bytes as we have space remaining, or as much as we have data to write */

#if CALLBACK_TRACE
      TRACE("inputFramesAvailable after conversion %ld\n", inputFramesAvailable);
      TRACE("nframes == %d, jackFramesAvailable == %ld,\n\tdrv->num_input_channels == %ld, drv->num_output_channels == %ld\n",
	    nframes, jackFramesAvailable, drv->num_input_channels, drv->num_output_channels);
#endif

      /* convert from 8 bit to 16 bit and mono to stereo if necessary */
      /* otherwise just memcpy to the output buffer */
      //      if(drv->bits_per_channel == 8)
      //      {
      //        sample_move_d8_d16 ((short*)drv->sound_buffer + ((jackBytesAvailableThisCallback - jackBytesLeft) / sizeof(short)),
      //                  buffer, jackBytesToWrite, drv->num_channels);
      //      } else if(drv->num_channels == 1)
      
      /* 16 bit input samples */
      /* if we have a mismatch of channels we need to lose or duplicate data */
      if(drv->num_input_channels != drv->num_output_channels)
      {
        sample_move_d16_d16((short*)drv->sound_buffer + ((nframes - jackFramesAvailable) * drv->bits_per_channel * drv->num_output_channels) / (sizeof(short) * 8),
			    (short*)buffer, numFramesToWrite, drv->num_output_channels, drv->num_input_channels);
      } else /* just copy the memory over */
      {
        memcpy(drv->sound_buffer + ((nframes - jackFramesAvailable) * drv->bits_per_channel * drv->num_output_channels) / 8,
	       buffer, (numFramesToWrite * drv->bits_per_channel * drv->num_input_channels) / 8);
      }

      /* advance to the next wave header if possible, or advance pointer */
      /* inside of the current header if we haven't completed it */
      if(numFramesToWrite == inputFramesAvailable)
      {
        wave_header_t* pOldHeader;
	
#if CALLBACK_TRACE
	TRACE("numFramesToWrite == inputFramesAvailable, advancing to next header\n");
#endif

        free(drv->pPlayPtr->pData); /* free the data we've played */
        drv->playptr_offset = 0;
        pOldHeader = drv->pPlayPtr;
        drv->pPlayPtr = drv->pPlayPtr->pNext;
        free(pOldHeader); /* free the wave header structure that we just finished playing */
      }
      else
      {
          /* else advance by the bytes we took in to write */
          drv->playptr_offset+=((numFramesToWrite * drv->bits_per_channel * drv->num_input_channels) / 8);
      }

      /* add on what we wrote */
      written+=((numFramesToWrite * drv->bits_per_channel * drv->num_output_channels) / 8);
      read+=((numFramesToWrite * drv->bits_per_channel * drv->num_input_channels) / 8);

      jackFramesAvailable-=numFramesToWrite; /* take away what was written */

#if CALLBACK_TRACE
      TRACE("jackFramesAvailable == %ld\n", jackFramesAvailable);
#endif
    } /* while(jackFramesAvailable && drv->pPlayPtr) */


    drv->written_client_bytes+=read;
    drv->played_client_bytes+=drv->clientBytesInJack; /* move forward by the previous bytes we wrote since those must have finished by now */
    drv->clientBytesInJack = read; /* record the input bytes we wrote to jack */

    /* Now that we have finished filling the buffer either until it is full or until */
    /* we have run out of application sound data to process, output */
    /* the audio to the jack server */

    /* convert from stereo 16 bit to single channel 32 bit float */
    /* for each output channel */
    /* NOTE: we skip over the number shorts(16 bits) we have output channels as the channel data */
    /* is encoded like chan1,chan2,chan3,chan1,chan2,chan3... */
    for(i = 0; i < drv->num_output_channels; i++)
    {
      sample_move_d16_s16(out_buffer[i], (short*)drv->sound_buffer + i, 
			(nframes - jackFramesAvailable), drv->num_output_channels);

      /* apply volume to the floating value */
      if(drv->volumeEffectType == dbAttenuation)
      {
          /* assume the volume setting is dB of attenuation, a volume of 0 */
          /* is 0dB attenuation */
          float volume = powf(10.0, -((float)drv->volume[i]) / 20.0);
          float_volume_effect(out_buffer[i], (nframes - jackFramesAvailable),
                              volume);
      } else
      {
          float_volume_effect(out_buffer[i], (nframes - jackFramesAvailable),
                              ((float)drv->volume[i] / 100.0));
      }
    }


    /* see if we still have jackBytesLeft here, if we do that means that we
    ran out of wave data to play and had a buffer underrun, fill in
    the rest of the space with zero bytes so at least there is silence */
    if(jackFramesAvailable)
    {
#if CALLBACK_TRACE
      TRACE("buffer underrun of %ld frames\n", jackFramesAvailable);
#endif
      for(i = 0 ; i < drv->num_output_channels; i++)
	sample_silence_dS(out_buffer[i] + (nframes - jackFramesAvailable), jackFramesAvailable);
    }
  }
  else if(drv->state == PAUSED ||
          drv->state == STOPPED ||
          drv->state == CLOSED ||
          drv->state == RESET)
  {
#if CALLBACK_TRACE
      TRACE("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif

      /* output silence if nothing is being outputted */
      for(i = 0; i < drv->num_output_channels; i++)
          sample_silence_dS(out_buffer[i], nframes);

      /* if we were told to reset then zero out some variables */
      /* and transition to STOPPED */
      if(drv->state == RESET)
      {
          wave_header_t *wh = drv->pPlayPtr;
          drv->written_client_bytes    = 0;
          drv->played_client_bytes     = 0;  /* number of the clients bytes that jack has played */

          drv->client_bytes            = 0;  /* bytes that the client wrote to use */
          drv->clientBytesInJack       = 0;  /* number of input bytes in jack(not necessary the number of bytes written to jack) */

          drv->pPlayPtr                = 0;
          drv->playptr_offset          = 0;
          drv->position_byte_offset    = 0;

          /* free up all of the buffers of audio that are queued */
          /* NOTE: this needs to be done inside of the callback because */
          /*  the callback could be using any of these buffers */
          wh = drv->pPlayPtr;
          while(wh)
          {
              wh = wh->pNext;
              free(drv->pPlayPtr->pData); /* free up the app data */
              free(drv->pPlayPtr); /* free the structure itself */
              drv->pPlayPtr = wh;
          }

          drv->state = STOPPED; /* transition to STOPPED */
      }
  }

#if CALLBACK_TRACE
  TRACE("done\n");
#endif

  return 0;
}


/******************************************************************
 *             JACK_bufsize
 *
 *             Called whenever the jack server changes the the max number
 *             of frames passed to JACK_callback
 */
static int JACK_bufsize (nframes_t nframes, void *arg)
{
  jack_driver_t* drv = (jack_driver_t*)arg;
  unsigned long buffer_required;
  TRACE("the maximum buffer size is now %lu frames\n", (long)nframes);

  /* make sure the callback routine has adequate memory for the nframes it will get */
  /* ie. Buffer_size < (bytes we already wrote + bytes we are going to write in this loop) */
  /* frames * 2 bytes in 16 bits * X channels of output */
  buffer_required = nframes * sizeof(short) * drv->num_output_channels;
  if(drv->buffer_size < buffer_required)
  {
    TRACE("expanding buffer from drv->buffer_size == %ld, to %ld\n",
          drv->buffer_size, buffer_required);
    drv->buffer_size = buffer_required;
    drv->sound_buffer = (unsigned char*)realloc(drv->sound_buffer, drv->buffer_size);

    /* if we don't have a buffer then error out */
    if(!drv->sound_buffer)
    {
      ERR("error allocating sound_buffer memory\n");
      return 0;
    }
  }

  TRACE("called\n");
  return 0;
}

/******************************************************************
 *		JACK_srate
 */
int JACK_srate (nframes_t nframes, void *arg)
{
    TRACE("the sample rate is now %lu/sec\n", (long)nframes);
    return 0;
}


/******************************************************************
 *		JACK_shutdown
 *
 * if this is called then jack shut down... handle this appropriately */
void JACK_shutdown(void* arg)
{
  jack_driver_t* drv = (jack_driver_t*)arg;

  drv->client = 0; /* reset client */
  drv->jackd_died = TRUE;

  TRACE("jack shutdown, setting client to 0 and jackd_died to true\n");
  TRACE("trying to reconnect right now\n");

  /* lets see if we can't reestablish the connection */
  if(JACK_OpenDevice(drv) != ERR_SUCCESS)
  {
    ERR("unable to reconnect with jack\n");
  }
}


/******************************************************************
 *		JACK_Error
 *
 * Callback for jack errors
 */
static void JACK_Error(const char *desc)
{
  ERR("%s\n", desc);
}


#if 0
/******************************************************************
 *		JACK_SendMessage
 *
 * put a message on the message queue of a driver */
static bool JACK_SendMessage(jack_driver_t* this, enum cmd_enum command, long data)
{
  message_t *newMessage;
  message_t **m;

  newMessage = (message_t*)malloc(sizeof(message_t));
  if(!newMessage)
  {
    ERR("error allocating new message\n");
    return FALSE;
  }

  newMessage->command = command;
  newMessage->data = data;
  newMessage->pNext = 0; /* setup the next pointer to point to null */

  /* now setup the last pointer in the existing array to point to this header */
  /* we use a pointer to a pointer here just to make this code more elegant */
  /* and in case pMessages is null it makes that condition clean */
  for(m = &(drv->pMessages); *m; m = &((*m)->pNext));
  *m = newMessage; /* point it to this new message */

  return TRUE;
}
#endif


/******************************************************************
 *		JACK_OpenDevice
 *
 *  RETURNS: ERR_SUCCESS upon success
 */
static int JACK_OpenDevice(jack_driver_t* drv)
{
  const char** ports;
  unsigned int i;
  char client_name[64];
  int failed = 0;

  TRACE("creating jack client and setting up callbacks\n");

#if JACK_CLOSE_HACK
  /* see if this device is already open */
  if(drv->client)
  {
    /* if this device is already in use then it is bad for us to be in here */
    if(drv->in_use)
      return ERR_OPENING_JACK;

    TRACE("using existing client\n");
    drv->in_use = TRUE;
    return ERR_SUCCESS;
  }
#endif	

  /* zero out the buffer pointer and the size of the buffer */
  drv->sound_buffer = 0;
  drv->buffer_size = 0;
  drv->playptr_offset = 0;

  /* set up an error handler */
  jack_set_error_function(JACK_Error);

  /* try to become a client of the JACK server */
  snprintf(client_name, sizeof(client_name), "bio2jack_%d_%d", 0, getpid());
  TRACE("client name '%s'\n", client_name);
  if ((drv->client = jack_client_new(client_name)) == 0)
  {
    /* try once more */
    TRACE("trying once more to jack_client_new");
    if ((drv->client = jack_client_new(client_name)) == 0)
    {
      ERR("jack server not running?\n");
      return ERR_OPENING_JACK;
    }
  }

  TRACE("setting up jack callbacks\n");

  /* JACK server to call `JACK_callback()' whenever
     there is work to be done. */
  jack_set_process_callback(drv->client, JACK_callback, drv);

  /* setup a buffer size callback */
  jack_set_buffer_size_callback(drv->client, JACK_bufsize, drv);

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(drv->client, JACK_srate, drv);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(drv->client, JACK_shutdown, drv);

  /* display the current sample rate. once the client is activated
     (see below), you should rely on your own sample rate
     callback (see above) for this value. */
  drv->sample_rate = jack_get_sample_rate(drv->client);
  TRACE("engine sample rate: %lu\n", drv->sample_rate);

  /* create the output ports */
  TRACE("creating output ports\n");
  for(i = 0; i < drv->num_output_channels; i++)
  {
    char portname[32];
    sprintf(portname, "out_%d", i);
    TRACE("port %d is named '%s'\n", i, portname);
    /* NOTE: Yes, this is supposed to be JackPortIsOutput since this is an output */
    /* port FROM bio2jack */
    drv->output_port[i] = jack_port_register(drv->client, portname,
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsOutput,
                                              0);
  }

  /* set the initial buffer size */
  JACK_bufsize(jack_get_buffer_size(drv->client), drv);

#if JACK_CLOSE_HACK
  drv->in_use = TRUE;
#endif

  /* tell the JACK server that we are ready to roll */
  TRACE("calling jack_activate()\n");
  if(jack_activate(drv->client))
  {
    ERR( "cannot activate client\n");
    return ERR_OPENING_JACK;
  }

  /* determine how we are to acquire port names */
  if((drv->jack_port_name_count == 0) || (drv->jack_port_name_count == 1))
  {
    if(drv->jack_port_name_count == 0)
    {
      TRACE("jack_get_ports() passing in NULL/NULL\n");
      ports = jack_get_ports(drv->client, NULL, NULL, drv->jack_port_flags);
    }
    else
    {
      TRACE("jack_get_ports() passing in port of '%s'\n", drv->jack_port_name[0]);
      ports = jack_get_ports(drv->client, drv->jack_port_name[0], NULL, drv->jack_port_flags);
    }

    /* display a trace of the output ports we found */
    if(ports) {
      for(i = 0; ports[i]; i++)
	TRACE("ports[%d] = '%s'\n", i, ports[i]);
    } else {
      i = 0; // No ports found
    }

    /* see if we have enough ports */
    if(i < drv->num_output_channels)
    {
      TRACE("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", drv->jack_port_flags);
      JACK_CloseDevice(drv, TRUE);
      return ERR_PORT_NOT_FOUND;
    }

    /* connect the ports. Note: you can't do this before
       the client is activated (this may change in the future). */
    for(i = 0; i < drv->num_output_channels; i++)
    {
      TRACE("jack_connect() to port %d('%p')\n", i, drv->output_port[i]);
      if(jack_connect(drv->client, jack_port_name(drv->output_port[i]), ports[i]))
      {
          ERR("cannot connect to output port %d('%s')\n", i, ports[i]);
          failed = 1;
      }
    } 

    free(ports); /* free the returned array of ports */
  } else
  {
    for(i = 0; i < drv->jack_port_name_count; i++)
    {
      TRACE("jack_get_ports() portname %d of '%s\n", i, drv->jack_port_name[i]);
      ports = jack_get_ports(drv->client, drv->jack_port_name[i], NULL, drv->jack_port_flags);

      if(!ports)
      {
          ERR("jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", drv->jack_port_flags);
          return ERR_PORT_NOT_FOUND;
      }
      TRACE("ports[%d] = '%s'\n", 0, ports[0]);      /* display a trace of the output port we found */

      /* connect the port */
      TRACE("jack_connect() to port %d('%p')\n", i, drv->output_port[i]);
      if(jack_connect(drv->client, jack_port_name(drv->output_port[i]), ports[0]))
      {
          ERR("cannot connect to output port %d('%s')\n", 0, ports[0]);
          failed = 1;
      }
      free(ports); /* free the returned array of ports */
    }
  }

  /* if something failed we need to shut the client down and return 0 */
  if(failed)
  {
    TRACE("failed, closing and returning error\n");
    JACK_CloseDevice(drv, TRUE);
    return ERR_OPENING_JACK;
  }

  TRACE("success\n");

  drv->jackd_died = FALSE; /* clear out this flag so we don't keep attempting to restart things */

  return ERR_SUCCESS; /* return success */
}


/******************************************************************
 *		JACK_CloseDevice
 *
 *	Close the connection to the server cleanly.
 *  If close_client is TRUE we close the client for this device instead of
 *    just marking the device as in_use(JACK_CLOSE_HACK only)
 */
#if JACK_CLOSE_HACK
static void JACK_CloseDevice(jack_driver_t* drv, bool close_client)
#else
static void JACK_CloseDevice(jack_driver_t* drv)
#endif
{
  unsigned int i;

#if JACK_CLOSE_HACK
  if(close_client)
  {
#endif
    TRACE("closing the jack client thread\n");
    if(drv->client)
    {
      //FIXME: deactivate or no, jack_client_close() is blocking...
      //jack_deactivate(drv->client); /* supposed to help the jack_client_close() to succeed */
      TRACE("after jack_deactivate()\n");
      jack_client_close(drv->client);
    }

    JACK_ResetFromDriver(drv);
    drv->client       = 0; /* reset client */
    free(drv->sound_buffer); /* free buffer memory */
    drv->sound_buffer = 0;
    drv->buffer_size  = 0; /* zero out size of the buffer */

    /* free up the port strings */
    TRACE("freeing up port strings\n");
    if(drv->jack_port_name_count > 1)
    {
      for(i = 0; i < drv->jack_port_name_count; i++)
          free(drv->jack_port_name[i]);
      free(drv->jack_port_name);
    }
#if JACK_CLOSE_HACK
  } else
  {
    TRACE("setting in_use to FALSE\n");
    drv->in_use = FALSE;

    if(!drv->client)
    {
      TRACE("critical error, closing a device that has no client\n");
    }
  }
#endif
}





/**************************************/
/* External interface functions below */
/**************************************/

/* Clear out any buffered data, stop playing, zero out some variables */
static void JACK_ResetFromDriver(jack_driver_t *drv)
{
  TRACE("resetting drv->deviceID(%d)\n", drv->deviceID);

  /* NOTE: we use the RESET state so we don't need to worry about clearing out */
  /* variables that the callback modifies while the callback is running */
  /* we set the state to RESET and the callback clears the variables out for us */
  drv->state = RESET; /* tell the callback that we are to reset, the callback will transition this to STOPPED */
}

/* Clear out any buffered data, stop playing, zero out some variables */
void JACK_Reset(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  TRACE("resetting deviceID(%d)\n", deviceID);
  JACK_ResetFromDriver(drv);
  releaseDriver(drv);
}


/*
 * open the audio device for writing to
 *
 * deviceID is set to the opened device
 * if client is non-zero and in_use is FALSE then just set in_use to TRUE
 *
 * return value is zero upon success, non-zero upon failure 
 *
 * if ERR_RATE_MISMATCH (*rate) will be updated with the jack servers rate
 */
int JACK_Open(int* deviceID, unsigned int bits_per_channel, unsigned long *rate, int channels)
{ 
  /* we call through to JACK_OpenEx() */
  return JACK_OpenEx(deviceID, bits_per_channel, rate, channels, channels,
  		     NULL, 0, JackPortIsPhysical);
}

/*
 * see JACK_Open() for comments
 * NOTE: jack_port_name has three ways of being used:
 *       - NULL - finds all ports with the given flags
 *       - A single regex string used to retreve all port names
 *       - A series of port names, one for each output channel
 *
 * we set *deviceID
 */
int JACK_OpenEx(int* deviceID, unsigned int bits_per_channel,
                unsigned long *rate, 
                unsigned int input_channels, unsigned int output_channels,
                const char** jack_port_name, 
                unsigned int jack_port_name_count,
                unsigned long jack_port_flags)
{
  jack_driver_t *drv = getDriver(first_free_device);
  unsigned int i;
  int retval;

  TRACE("bits_per_channel=%d rate=%ld, input_channels=%d, output_channels=%d\n",
        bits_per_channel, *rate, input_channels, output_channels);

  if(output_channels > MAX_OUTPUT_PORTS)
  {
    ERR("output_channels == %d, MAX_OUTPUT_PORTS == %d\n", output_channels, MAX_OUTPUT_PORTS);
    releaseDriver(drv);
    return ERR_TOO_MANY_OUTPUT_CHANNELS;
  }

  jack_port_flags|=JackPortIsInput; /* port must be input(ie we can put data into it), so mask this in */

  /* check that we have the correct number of port names */
  if((jack_port_name_count > 1) && (jack_port_name_count != output_channels))
  {
    ERR("specified individual port names but not enough, gave %d names, need %d\n",
	jack_port_name_count, output_channels);
    releaseDriver(drv);
    return ERR_PORT_NAME_OUTPUT_CHANNEL_MISMATCH;
  } else
  {
    /* copy this data into the device information */
    drv->jack_port_flags = jack_port_flags;
    drv->jack_port_name_count = jack_port_name_count;

    if(drv->jack_port_name_count != 0)
    {
      drv->jack_port_name = (char**)malloc(sizeof(char*) * drv->jack_port_name_count);
      for(i = 0; i < drv->jack_port_name_count; i++)
      {
          drv->jack_port_name[i] = strdup(jack_port_name[i]);
          TRACE("jack_port_name[%d] == '%s'\n", i, jack_port_name[i]);
      }
    }
    else
    {
      drv->jack_port_name = NULL;
      TRACE("jack_port_name = NULL\n");
    }
  }

  /* initialize some variables */
  drv->in_use = FALSE;

  JACK_ResetFromDriver(drv); /* flushes all queued buffers, sets status to STOPPED and resets some variables */

  /* drv->sample_rate is set by JACK_OpenDevice() */
  drv->bits_per_channel       = bits_per_channel;
  drv->num_input_channels     = input_channels;
  drv->num_output_channels    = output_channels;
  drv->bytes_per_input_frame  = (drv->bits_per_channel*drv->num_input_channels)/8;
  drv->bytes_per_output_frame = (drv->bits_per_channel*drv->num_output_channels)/8;

  TRACE("bytes_per_output_frame == %ld\n", drv->bytes_per_output_frame);
  TRACE("bytes_per_input_frame  == %ld\n", drv->bytes_per_input_frame);

  /* make sure bytes_per_frame is valid and non-zero */
  if(!drv->bytes_per_output_frame)
  {
    ERR("bytes_per_output_frame is zero\n");
    releaseDriver(drv);
    return ERR_BYTES_PER_OUTPUT_FRAME_INVALID;
  }

  /* go and open up the device */
  retval = JACK_OpenDevice(drv);
  if(retval != ERR_SUCCESS)
  {
    TRACE("error opening jack device\n");
    releaseDriver(drv);
    return retval;
  } else
  {
    TRACE("succeeded opening jack device\n");
  }

  /* make sure the sample rate of the jack server matches that of the client */
  if((long)(*rate) != drv->sample_rate)
  {
    TRACE("rate of %ld doesn't match jack sample rate of %ld, returning error\n",
	  *rate, drv->sample_rate);
    *rate = drv->sample_rate;
    JACK_CloseDevice(drv, TRUE);
    releaseDriver(drv);
    return ERR_RATE_MISMATCH;
  }

  first_free_device++; /* record that we opened this device */

  TRACE("sizeof(sample_t) == %d\n", sizeof(sample_t));

  drv->latencyMS = 10;

  TRACE("drv->latencyMS == %ldms\n", drv->latencyMS);

  *deviceID = drv->deviceID; /* set the deviceID for the caller */
  releaseDriver(drv);
  return ERR_SUCCESS; /* success */
}

/* Close the jack device */
//FIXME: add error handling in here at some point...
/* NOTE: return 0 for success, non-zero for failure */
int JACK_Close(int deviceID)
{
  jack_driver_t* drv = getDriver(deviceID);

  TRACE("deviceID(%d)\n", deviceID);

#if JACK_CLOSE_HACK
  JACK_CloseDevice(drv, TRUE);
#else
  JACK_CloseDevice(drv);
#endif

  JACK_ResetFromDriver(drv); /* reset this device to a normal starting state */

  first_free_device--; /* decrement device count */

  releaseDriver(drv);
  return 0;
}

/* If we haven't already taken in the max allowed data then create a wave header */
/* to package the audio data and attach the wave header to the end of the */
/* linked list of wave headers */
/* These wave headers will be peeled off as they are played by the callback routine */
/* Return value is the number of bytes written */
/* NOTE: this function takes the length of data to be written bytes */
long JACK_Write(int deviceID, unsigned char *data, unsigned long bytes)
{
  jack_driver_t *drv = getDriver(deviceID);
  wave_header_t *newWaveHeader;
  wave_header_t **wh;
  struct timeval now;
  long bytes_free;

  TRACE("deviceID(%d), bytes == %ld\n", deviceID, bytes);

  gettimeofday(&now, 0);
  TRACE("Starting Time = %ld.%ld\n", now.tv_sec, now.tv_usec);

  /* check and see that we have enough space for this audio */
  bytes_free = JACK_GetBytesFreeSpaceFromDriver(drv);
  TRACE("bytes free == %ld\n", bytes_free);
  if(bytes > bytes_free)
  {
    bytes = 0; /* act as if we were told to write 0 bytes */
  }

  /* handle the case where the user calls this routine with 0 bytes */
  if(bytes == 0)
  {
      releaseDriver(drv);
      return 0; /* indicate that we couldn't write any bytes */
  }

  newWaveHeader = (wave_header_t*)malloc(sizeof(wave_header_t));   /* create a wave header for this data */
  if(!newWaveHeader)
  {
    ERR("error allocating memory for newWaveHeader\n");
  }

  newWaveHeader->pData = (unsigned char*)malloc(sizeof(unsigned char) * bytes); /* allocate memory for the data */
  memcpy(newWaveHeader->pData, data, sizeof(unsigned char) * bytes);   /* copy in the data */
  newWaveHeader->size = bytes;   /* update the size */
  newWaveHeader->pNext = 0;   /* setup the next pointer to point to null */

  /* now setup the last pointer in the existing array to point to this header */
  /* we use a pointer to a pointer here just to make this code more elegant */
  /* and in case pQueuePtr is null it makes that condition clean */
  for(wh = &(drv->pPlayPtr); *wh; wh = &((*wh)->pNext));
  *wh = newWaveHeader; /* point it to this new header */

  drv->client_bytes += bytes; /* update client_bytes */

  if (!drv->pPlayPtr) /* if we have no header being played then use this one */
  {
    drv->pPlayPtr = newWaveHeader;
    drv->playptr_offset = 0;
  }

  /* if we are currently STOPPED we should start playing now... */
  if (drv->state == STOPPED)
  {
    TRACE("currently STOPPED, transitioning to PLAYING\n");
    drv->state = PLAYING;
  }

  gettimeofday(&now, 0);
  TRACE("Ending Time = %ld.%ld\n", now.tv_sec, now.tv_usec);

  TRACE("returning bytes written of %ld\n", bytes);

  releaseDriver(drv);
  return bytes; /* return the number of bytes we wrote out */
}

#if 0
/* the client is exiting, close the audio device and the corresponding client */
static void JACK_exit(int deviceID)
{
  jack_driver_t* this = getDriver(deviceID);

  TRACE("deviceID(%d)\n", deviceID);

  JACK_CloseDevice(drv, TRUE); /* close the device, FORCE the client to close */
  releaseDriver(drv);
}
#endif


/* return ERR_SUCCESS for success */
static int JACK_SetVolumeForChannelFromDriver(jack_driver_t *drv,
                                            unsigned int channel,
                                            unsigned int volume)
{
  /* ensure that we have the channel we are setting volume for */
  if(channel > (drv->num_output_channels - 1))
  {
    return 1;
  }

  if(volume > 100) volume = 100; /* check for values in excess of max */

  drv->volume[channel] = volume;
  return ERR_SUCCESS;
}

/* return ERR_SUCCESS for success */
int JACK_SetVolumeForChannel(int deviceID, unsigned int channel, unsigned int volume)
{
  jack_driver_t *drv = getDriver(deviceID);
  int retval = JACK_SetVolumeForChannelFromDriver(drv, channel, volume);
  releaseDriver(drv);
  return retval;
}

/* Set the volume */
/* return 0 for success */
/* NOTE: we check for invalid volume values */
int JACK_SetAllVolume(int deviceID, unsigned int volume)
{
  jack_driver_t *drv = getDriver(deviceID);
  unsigned int i;

  TRACE("deviceID(%d), setting volume of %d\n", deviceID, volume);

  for(i = 0; i < drv->num_output_channels; i++)
  {
    if(JACK_SetVolumeForChannelFromDriver(drv, i, volume) != ERR_SUCCESS)
    {
      releaseDriver(drv);
      return 1;
    }
  }

  releaseDriver(drv);
  return ERR_SUCCESS;
}

/* Return the current volume in the inputted pointers */
/* NOTE: we check for null pointers being passed in just in case */
void JACK_GetVolumeForChannel(int deviceID, unsigned int channel, unsigned int *volume)
{
  jack_driver_t *drv = getDriver(deviceID);

  if(volume) *volume = drv->volume[channel];

#if VERBOSE_OUTPUT
  if(volume)
  {
    TRACE("deviceID(%d), returning volume of %d for channel %d\n", deviceID, *volume, channel);
  } else
  {
    TRACE("volume is null, can't dereference it\n");
  }
#endif
  
  releaseDriver(drv);
}


/* linear means 0 volume is silence, 100 is full volume */
/* dbAttenuation means 0 volume is 0dB attenuation */
/* Bio2jack defaults to linear */
enum JACK_VOLUME_TYPE JACK_SetVolumeEffectType(int deviceID, enum JACK_VOLUME_TYPE type)
{
    enum JACK_VOLUME_TYPE retval;
    jack_driver_t *drv = getDriver(deviceID);

    TRACE("setting type of '%s'\n",
          (type == dbAttenuation ? "dbAttenuation" : "linear"));

    retval = drv->volumeEffectType;
    drv->volumeEffectType = type;

    releaseDriver(drv);
    return retval;
}


/* Controls the state of the playback(playing, paused, ...) */
int JACK_SetState(int deviceID, enum status_enum state)
{
  jack_driver_t *drv = getDriver(deviceID);

  switch (state) {
  case PAUSED:
    drv->state = PAUSED;
    break;
  case PLAYING:
    drv->state = PLAYING;
    break;
  case STOPPED:
    drv->state = STOPPED;
    break;
  default:
    TRACE("unknown state of %d\n", state);
  }

  TRACE("%s\n", DEBUGSTATE(drv->state));

  releaseDriver(drv);
  return 0;
}

/* Retrieve the current state of the device */
enum status_enum JACK_GetState(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  enum status_enum return_val;

  return_val = drv->state;
  releaseDriver(drv);

  TRACE("deviceID(%d), returning current state of %s\n", deviceID, DEBUGSTATE(return_val));
  return return_val;
}

/* Retrieve the number of bytes per second we are outputting */
long JACK_GetOutputBytesPerSecond(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long return_val;

  return_val =  drv->bytes_per_output_frame * drv->sample_rate;
  releaseDriver(drv);

#if VERBOSE_OUTPUT
  TRACE("deviceID(%d), return_val = %ld\n", deviceID, return_val);
#endif

  return return_val;
}

/* Retrieve the number of input bytes per second we are outputting */
static long JACK_GetInputBytesPerSecondFromDriver(jack_driver_t *drv)
{
  long return_val;

  return_val =  drv->bytes_per_input_frame * drv->sample_rate;
#if VERBOSE_OUTPUT
  TRACE("drv->deviceID(%d), return_val = %ld\n", drv->deviceID, return_val);
#endif

  return return_val;
}

/* Retrieve the number of input bytes per second we are outputting */
long JACK_GetInputBytesPerSecond(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long return_val = JACK_GetInputBytesPerSecondFromDriver(drv);
  releaseDriver(drv);

#if VERBOSE_OUTPUT
  TRACE("deviceID(%d), return_val = %ld\n", deviceID, return_val);
#endif

  return return_val;
}


/* Return the number of bytes we have buffered thus far for output */
/* NOTE: convert from output bytes to input bytes in here */
static long JACK_GetBytesStoredFromDriver(jack_driver_t *drv)
{
  struct timeval now;
  long elapsedMS;
  long return_val;

  /* get the number of bytes free space */
  /* TODO: its gross to subtract away the position byte offset here, consider */
  /*       other cleaner ways of doing this */
  return_val = (drv->client_bytes -
                  (JACK_GetPositionFromDriver(drv, BYTES, PLAYED)
                   - drv->position_byte_offset));

  if(return_val < 0) return_val = 0;

  TRACE("drv->deviceID(%d), return_val = %ld\n", drv->deviceID, return_val);

  return return_val;
}

/* An approximation of how many bytes we have to send out to jack */
/* that is computed as if we were sending jack a continuous stream of */
/* bytes rather than chunks during discrete callbacks.  */
/* Return the number of bytes we have buffered thus far for output */
/* NOTE: convert from output bytes to input bytes in here */
long JACK_GetBytesStored(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long retval = JACK_GetBytesStoredFromDriver(drv);
  releaseDriver(drv);
  TRACE("deviceID(%d), retval = %ld\n", deviceID, retval);
  return retval;
}

static long JACK_GetBytesFreeSpaceFromDriver(jack_driver_t *drv)
{
    return MAX_BUFFERED_BYTES - (drv->client_bytes - drv->played_client_bytes);
}

/* Return the number of bytes we can write to the device */
long JACK_GetBytesFreeSpace(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long return_val;

  return_val = JACK_GetBytesFreeSpaceFromDriver(drv);
  releaseDriver(drv);

  if(return_val < 0) return_val = 0;
  TRACE("deviceID(%d), JACK_GetFreeSpacesFromDriver(deviceID) == %ld\n", deviceID, return_val);

  return return_val;
}

/* Get the current position of the driver, either in bytes or */
/* in milliseconds */
/* NOTE: this is position relative to input bytes, output bytes may differ greatly due to input vs. output channel count */
static long JACK_GetPositionFromDriver(jack_driver_t *drv, enum pos_enum position, int type)
{
  long return_val = 0;  
  struct timeval now;
  long elapsedMS;
  double sec2msFactor = 1000;

  char *type_str = "UNKNOWN type";

  /* if we are reset we should return a position of 0 */
  if(drv->state == RESET)
  {
    TRACE("we are currently RESET, returning 0\n");
    return 0;
  }

  if(type == WRITTEN)
  {
    type_str = "WRITTEN";
    return_val = drv->client_bytes;
  }
  else if(type == WRITTEN_TO_JACK)
  {
    type_str = "WRITTEN_TO_JACK";
    return_val = drv->written_client_bytes;
  }
  else if(type == PLAYED)  /* account for the elapsed time for the played_bytes */
  {
    type_str = "PLAYED";
    return_val = drv->played_client_bytes;
    gettimeofday(&now, 0);

    elapsedMS = TimeValDifference(&drv->previousTime, &now); /* find the elapsed milliseconds since last JACK_Callback() */

    TRACE("elapsedMS since last callback is '%ld'\n", elapsedMS);
   
    /* account for the bytes played since the last JACK_Callback() */
    /* NOTE: [Xms * (Bytes/Sec)] * (1 sec/1,000ms) */
    /* NOTE: don't do any compensation if no data has been sent to jack since the last callback */
    /* as this would result a bogus computed result */
    if(drv->clientBytesInJack != 0)
    {
      return_val+=(long)((double)elapsedMS * ((double)JACK_GetInputBytesPerSecondFromDriver(drv) / sec2msFactor));
    } else
    {
      TRACE("clientBytesInJack == 0\n");
    }
  }

  /* add on the offset */
  return_val+=drv->position_byte_offset;

  /* convert byte position to milliseconds value if necessary */
  if(position == MILLISECONDS)
  {
      if(JACK_GetInputBytesPerSecondFromDriver(drv) != 0)
          return_val = (long)(((double)return_val / (double)JACK_GetInputBytesPerSecondFromDriver(drv)) * (double)sec2msFactor);
      else
          return_val = 0;
  }

  TRACE("drv->deviceID(%d), type(%s), return_val = %ld\n", drv->deviceID, type_str, return_val);

  return return_val;
}

/* Get the current position of the driver, either in bytes or */
/* in milliseconds */
/* NOTE: this is position relative to input bytes, output bytes may differ greatly due to input vs. output channel count */
long JACK_GetPosition(int deviceID, enum pos_enum position, int type)
{
  jack_driver_t *drv = getDriver(deviceID);
  long retval = JACK_GetPositionFromDriver(drv, position, type);
  releaseDriver(drv);
  TRACE("retval == %ld\n", retval);
  return retval;
}


// Set position always applies to written bytes
// NOTE: we must apply this instantly because if we pass this as a message
//   to the callback we risk the user sending us audio data in the mean time
//   and there is no need to send this as a message, we don't modify any
//   internal variables
void JACK_SetPositionFromDriver(jack_driver_t *drv, enum pos_enum position, long value)
{
  double sec2msFactor = 1000;
#if TRACE_ENABLE
  long input_value = value;
#endif

  /* convert the incoming value from milliseconds into bytes */
  if(position == MILLISECONDS)
      value = (long)(((double)value*(double)JACK_GetInputBytesPerSecondFromDriver(drv)) / sec2msFactor);

  /* ensure that if the user asks for the position */
  /* they will at this instant get the correct position */
  drv->position_byte_offset = value - drv->client_bytes;

  TRACE("deviceID(%d) input_value of %ld, new value of %ld, setting position_byte_offset to %ld\n",
	drv->deviceID, input_value, value, drv->position_byte_offset);
}

// Set position always applies to written bytes
// NOTE: we must apply this instantly because if we pass this as a message
//   to the callback we risk the user sending us audio data in the mean time
//   and there is no need to send this as a message, we don't modify any
//   internal variables
void JACK_SetPosition(int deviceID, enum pos_enum position, long value)
{
  jack_driver_t *drv = getDriver(deviceID);
  JACK_SetPositionFromDriver(drv, position, value);
  releaseDriver(drv);

  TRACE("deviceID(%d) value of %ld\n",
	drv->deviceID,	value);
}

/* Return the number of bytes per frame, or (output_channels * bits_per_channel) / 8 */
long JACK_GetBytesPerOutputFrame(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long return_val = drv->bytes_per_output_frame;
  releaseDriver(drv);
  TRACE("deviceID(%d), return_val = %ld\n", deviceID, return_val);
  return return_val;
}

/* Return the number of bytes we buffer max */
long JACK_GetMaxBufferedBytes(int deviceID)
{
  long return_val;

  return_val = MAX_BUFFERED_BYTES;
  TRACE("getting MAX_BUFFERED_BYTES of %ld\n", return_val);

  return return_val;
}

/* Set the max number of bytes the jack driver should buffer */
void JACK_SetMaxBufferedBytes(int deviceID, long max_buffered_bytes)
{
  TRACE("setting MAX_BUFFERED_BYTES to %ld, from %ld\n", max_buffered_bytes, MAX_BUFFERED_BYTES);
  MAX_BUFFERED_BYTES = max_buffered_bytes;
}

/* Get the number of output channels */
int JACK_GetNumOutputChannels(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  int return_val = drv->num_output_channels;
  releaseDriver(drv);
  TRACE("getting num_output_channels of %d\n", return_val);
  return return_val;
}
 
/* Get the number of input channels */
int JACK_GetNumInputChannels(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  int return_val = drv->num_input_channels;
  releaseDriver(drv);
  TRACE("getting num_input_channels of %d\n", return_val);
  return return_val;
}
 
int JACK_SetNumOutputChannels(int deviceID, int channels)
{
  //FIXME: todo, should we resize buffers here by checking the size in JACK_callback() the next
  // time around??
  return 1;
}

/* RETURNS:  previous number of input channels */
int JACK_SetNumInputChannels(int deviceID, int channels)
{
  jack_driver_t *drv = getDriver(deviceID);
  int return_val = drv->num_input_channels;
#if TRACE_ENABLE
  int bpif = drv->bytes_per_input_frame;
#endif

  long positionMS = JACK_GetPositionFromDriver(drv, MILLISECONDS, PLAYED);

  drv->num_input_channels = channels;
  drv->bytes_per_input_frame = (drv->bits_per_channel*drv->num_input_channels)/8;

  JACK_SetPositionFromDriver(drv, MILLISECONDS, positionMS);

  releaseDriver(drv);

  TRACE("changing num_input_channels from '%d' to '%d'\n", return_val, channels);
  TRACE("bytes_per_input_frame changed from '%d' to '%ld'\n", bpif, drv->bytes_per_input_frame);

  return return_val;
}
 
/* Get the number of samples per second, the sample rate */
long JACK_GetSampleRate(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  int return_val = drv->sample_rate;
  releaseDriver(drv);
  TRACE("getting sample_rate of %d\n", return_val);
  return return_val;
}

/* Initialize the jack porting libarary to a clean state */
void JACK_Init(void)
{
  jack_driver_t *drv;
  int x, y;

  TRACE("\n");

  for(x = 0; x < MAX_OUTDEVICES; x++)
  {
    drv = &outDev[x];

    JACK_Reset(x);

    drv->deviceID               = x; /* makes it easy to convert a pointer back into a deviceID */
    drv->client                 = 0;
    drv->in_use                 = FALSE;
    for(y = 0; y < MAX_OUTPUT_PORTS; y++) /* make all volume 25% as a default */
      drv->volume[y] = 25;
    drv->volumeEffectType       = linear;
    drv->state                  = CLOSED;
    drv->bytes_per_output_frame = 0;
    drv->bytes_per_input_frame  = 0;
    drv->sample_rate            = 0;
    drv->pMessages              = 0; /* no messages */
    drv->position_byte_offset   = 0; /* no offset applied now */
    gettimeofday(&drv->previousTime, 0); /* record the current time */

    drv->jackd_died             = FALSE;
    gettimeofday(&drv->last_reconnect_attempt, 0);

    pthread_mutex_init(&drv->mutex, NULL);
  }

  TRACE("finished\n");
}

/* Get the latency, in ms, of jack */
long JACK_GetJackLatency(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long return_val;

  return_val = jack_port_get_total_latency(drv->client, drv->output_port[0]);
  TRACE("got latency of %ldms\n", return_val);

  releaseDriver(drv);
  return return_val;
}

unsigned long JACK_GetJackBufferedBytes(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long return_val;

  return_val = drv->buffer_size;

  releaseDriver(drv);
  return return_val;
}


/* unit testing code */
#if 0
bool test_sample_move_d16_d16(void)
{
  /* test sample_move_d16_d16 */
  short buffer[4];
  short buffer2[8];
  buffer[0] = 1;
  buffer[1] = 10;
  buffer[2] = 20;
  buffer[3] = 40;
  sample_move_d16_d16(buffer2, buffer, 2, 4, 2);
  
  if(buffer[0] != buffer2[0])
    TRACE("error, buffer[0] != buffer2[0]\n");
  if(buffer[1] != buffer2[1])
    TRACE("error, buffer[1] != buffer2[1]\n");
  if(buffer[0] != buffer2[2])
    TRACE("error, buffer[0] != buffer2[2]\n");
  if(buffer[1] != buffer2[3])
    TRACE("error, buffer[1] != buffer2[3]\n");

  if(buffer[2] != buffer2[4])
    TRACE("error, buffer[2] != buffer2[4]\n");
  if(buffer[3] != buffer2[5])
    TRACE("error, buffer[3] != buffer2[5]\n");
  if(buffer[2] != buffer2[6])
    TRACE("error, buffer[2] != buffer2[6]\n");
  if(buffer[3] != buffer2[7])
    TRACE("error, buffer[3] != buffer2[7]\n");

  return TRUE;
}
#endif
