#include <iostream>
//#include <stdlib.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputdx.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#define FRAMES_NUM 65536

/*****************************************************************************
 * DirectSound GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
#include <initguid.h>
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

/*****************************************************************************
 * Useful macros
 *****************************************************************************/
#ifndef WAVE_FORMAT_IEEE_FLOAT
#   define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#   define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define  WAVE_FORMAT_EXTENSIBLE   0xFFFE
#endif

#ifndef SPEAKER_FRONT_LEFT
#   define SPEAKER_FRONT_LEFT             0x1
#   define SPEAKER_FRONT_RIGHT            0x2
#   define SPEAKER_FRONT_CENTER           0x4
#   define SPEAKER_LOW_FREQUENCY          0x8
#   define SPEAKER_BACK_LEFT              0x10
#   define SPEAKER_BACK_RIGHT             0x20
#   define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#   define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#   define SPEAKER_BACK_CENTER            0x100
#   define SPEAKER_SIDE_LEFT              0x200
#   define SPEAKER_SIDE_RIGHT             0x400
#   define SPEAKER_TOP_CENTER             0x800
#   define SPEAKER_TOP_FRONT_LEFT         0x1000
#   define SPEAKER_TOP_FRONT_CENTER       0x2000
#   define SPEAKER_TOP_FRONT_RIGHT        0x4000
#   define SPEAKER_TOP_BACK_LEFT          0x8000
#   define SPEAKER_TOP_BACK_CENTER        0x10000
#   define SPEAKER_TOP_BACK_RIGHT         0x20000
#   define SPEAKER_RESERVED               0x80000000
#endif

#ifndef DSSPEAKER_HEADPHONE
#   define DSSPEAKER_HEADPHONE         0x00000001
#endif
#ifndef DSSPEAKER_MONO
#   define DSSPEAKER_MONO              0x00000002
#endif
#ifndef DSSPEAKER_QUAD
#   define DSSPEAKER_QUAD              0x00000003
#endif
#ifndef DSSPEAKER_STEREO
#   define DSSPEAKER_STEREO            0x00000004
#endif
#ifndef DSSPEAKER_SURROUND
#   define DSSPEAKER_SURROUND          0x00000005
#endif
#ifndef DSSPEAKER_5POINT1
#   define DSSPEAKER_5POINT1           0x00000006
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */
                                        /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );


AudioOutputDX::AudioOutputDX(QString audiodevice, int audio_bits, 
                             int audio_channels, int audio_samplerate,
                             AudioOutputSource source,
                             bool set_initial_vol, bool audio_passthru)
{
    this->audiodevice = audiodevice;
    
    dsound_dll = NULL;
    dsobject = NULL;
    dsbuffer = NULL;
    
    effdsp = 0;
    paused = false;
    
    InitDirectSound();
    
    Reconfigure(audio_bits, audio_channels, audio_samplerate, audio_passthru);
}

void AudioOutputDX::SetBlocking(bool blocking)
{
    // FIXME: kedl: not sure what else could be required here?
}

void AudioOutputDX::Reconfigure(int audio_bits, int audio_channels,
                                int audio_samplerate, int audio_passthru)
{
    if (dsbuffer)
        DestroyDSBuffer();
        
    CreateDSBuffer(audio_bits, audio_channels, audio_samplerate, false);
    
    awaiting_data = true;
    paused = true;
        
    effdsp = audio_samplerate;
    this->audio_bits = audio_bits;
    this->audio_channels = audio_channels;
    this->audio_passthru = audio_passthru;
}

AudioOutputDX::~AudioOutputDX()
{
    DestroyDSBuffer();

    /* release the DirectSound object */
    if (dsobject)
        IDirectSound_Release(dsobject);
    
    /* free DSOUND.DLL */
    if (dsound_dll)
       FreeLibrary(dsound_dll);
}

void AudioOutputDX::Reset(void)
{
    audbuf_timecode = 0;
    awaiting_data = true;
    write_cursor = 0;

    if (dsbuffer)    
    {
        IDirectSoundBuffer_Stop(dsbuffer);
        IDirectSoundBuffer_SetCurrentPosition(dsbuffer, 0);
    }

}

bool AudioOutputDX::AddSamples(char *buffer, int frames, long long timecode)
{
    FillBuffer(frames, buffer);

    HRESULT dsresult;
    
    if (awaiting_data) 
    {
//        dsresult = IDirectSoundBuffer_Play(dsbuffer, 0, 0, DSBPLAY_LOOPING);
//        if (dsresult == DSERR_BUFFERLOST)
//        {
//            IDirectSoundBuffer_Restore(dsbuffer);
//            dsresult = IDirectSoundBuffer_Play(dsbuffer, 0, 0, DSBPLAY_LOOPING );
//        }
//        if (dsresult != DS_OK)
//        {
//            VERBOSE(VB_IMPORTANT, "cannot start playing buffer" );
//        }
        
        awaiting_data = false;
    }


    
//    VERBOSE(VB_IMPORTANT, "add_samples(a) " << frames << " " << timecode);

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((frames*1000.0) / effdsp);

    return true;
}


bool AudioOutputDX::AddSamples(char *buffers[], int frames, long long timecode)
{
//    VERBOSE(VB_IMPORTANT, "add_samples(b) " << frames << " " << timecode);

/*    int err;
    int waud=0;
    int audio_bytes = audio_bits / 8;
    int audbufsize = frames*audio_bytes*audio_channels;
    char *audiobuffer = (char *) calloc(1,audbufsize);

    if (audiobuffer==NULL)
    {
        fprintf(stderr, "couldn't get memory to write audio to artsd\n");
    return;
    }

    if (pcm_handle == NULL)
    {
        free(audiobuffer);
        return;
    }

    for (int itemp = 0; itemp < frames*audio_bytes; itemp+=audio_bytes)
    {
        for(int chan = 0; chan < audio_channels; chan++)
        {
            audiobuffer[waud++] = buffers[chan][itemp];
            if (audio_bits == 16)
                audiobuffer[waud++] = buffers[chan][itemp+1];
            if (waud >= audbufsize)
                waud -= audbufsize;
        }
    }

    err = arts_write(pcm_handle, audiobuffer, frames*4);
    free(audiobuffer);

    if (err < 0)
    {
        fprintf(stderr, "arts_write error: %s\n", arts_error_text(err));
        return;
    }

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode*/
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    //audbuf_timecode = timecode + (int)((frames*100000.0) / effdsp);

    return true;
}

void AudioOutputDX::SetTimecode(long long timecode)
{
    audbuf_timecode = timecode;
}

void AudioOutputDX::SetEffDsp(int dsprate)
{
    VERBOSE(VB_IMPORTANT, "setting dsprate = " << dsprate);
    
    HRESULT dsresult;

    dsresult = IDirectSoundBuffer_SetFrequency(dsbuffer, dsprate / 100);

    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, "cannot set frequency");
    }

    effdsp = dsprate / 100;
}

bool AudioOutputDX::GetPause(void)
{
    return paused;
}

void AudioOutputDX::Pause(bool pause)
{
    HRESULT dsresult;

    VERBOSE(VB_IMPORTANT, "pause: " << pause);

    if (pause == paused)
        return;
        
    if (paused)
    {
        VERBOSE(VB_IMPORTANT, "unpausing");
    
        dsresult = IDirectSoundBuffer_Play(dsbuffer, 0, 0, DSBPLAY_LOOPING);
        if (dsresult == DSERR_BUFFERLOST)
        {
            IDirectSoundBuffer_Restore(dsbuffer);
            dsresult = IDirectSoundBuffer_Play(dsbuffer, 0, 0, DSBPLAY_LOOPING );
        }
        if (dsresult != DS_OK)
        {
            VERBOSE(VB_IMPORTANT, "cannot start playing buffer" );
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "pausing");
    
        IDirectSoundBuffer_Stop(dsbuffer);
    }

    paused = pause;
}

int AudioOutputDX::GetAudiotime(void)
{
    DWORD play_pos;
    HRESULT dsresult;

    dsresult = IDirectSoundBuffer_GetCurrentPosition(dsbuffer, &play_pos, NULL);

    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, "cannot get current buffer positions");
        return -1;
    }

    int frames = (write_cursor - play_pos) / audio_bytes_per_sample;
    
    if (frames < 0)
        frames += buffer_size;

    return audbuf_timecode - (int)((frames*1000.0) / effdsp);
}


typedef HRESULT (WINAPI *LPFNDSC) (LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

/*****************************************************************************
 * InitDirectSound: handle all the gory details of DirectSound initialisation
 *****************************************************************************/
int AudioOutputDX::InitDirectSound()
{
//    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
    LPFNDSC OurDirectSoundCreate;

    VERBOSE(VB_IMPORTANT, "initialising DirectSound");
   
    dsound_dll = LoadLibrary("DSOUND.DLL");
    if (dsound_dll == NULL )
    {
        VERBOSE(VB_IMPORTANT, "cannot open DSOUND.DLL" );
        goto error;
    }

    OurDirectSoundCreate = (LPFNDSC)
                                GetProcAddress(dsound_dll, "DirectSoundCreate");
    if (OurDirectSoundCreate == NULL)
    {
        VERBOSE(VB_IMPORTANT, "GetProcAddress FAILED" );
        goto error;
    }

    VERBOSE(VB_IMPORTANT, "create DS object");

    /* Create the direct sound object */
    if FAILED(OurDirectSoundCreate(NULL, &dsobject, NULL))
    {
        VERBOSE(VB_IMPORTANT, "cannot create a direct sound device" );
        goto error;
    }

    VERBOSE(VB_IMPORTANT, "setting cooperative level");

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
    if (IDirectSound_SetCooperativeLevel(dsobject,
                                         GetDesktopWindow(),
                                         DSSCL_EXCLUSIVE))
    {
        VERBOSE(VB_IMPORTANT, "cannot set direct sound cooperative level" );
    }

    VERBOSE(VB_IMPORTANT, "creating notificatoin events");

    /* Then create the notification events */
    for (int i = 0; i < 4; i++)
        notif[i].hEventNotify = CreateEvent(NULL, FALSE, FALSE, NULL);

    VERBOSE(VB_IMPORTANT, "initialised DirectSound");

    return 0;

 error:
    dsobject = NULL;
    if (dsound_dll)
    {
        FreeLibrary(dsound_dll);
        dsound_dll = NULL;
    }
    return -1;

}


/*****************************************************************************
 * CreateDSBuffer: Creates a direct sound buffer of the required format.
 *****************************************************************************
 * This function creates the buffer we'll use to play audio.
 * In DirectSound there are two kinds of buffers:
 * - the primary buffer: which is the actual buffer that the soundcard plays
 * - the secondary buffer(s): these buffers are the one actually used by
 *    applications and DirectSound takes care of mixing them into the primary.
 *
 * Once you create a secondary buffer, you cannot change its format anymore so
 * you have to release the current one and create another.
 *****************************************************************************/
int AudioOutputDX::CreateDSBuffer(int audio_bits, int audio_channels, int audio_samplerate, bool b_probe)
{
    WAVEFORMATEXTENSIBLE waveformat;
    DSBUFFERDESC         dsbdesc;
    unsigned int         i;

    /* First set the sound buffer format */
/*    waveformat.dwChannelMask = 0;
    for( i = 0; i < sizeof(pi_channels_in)/sizeof(uint32_t); i++ )
    {
        if ( i_channels & pi_channels_in[i] )
            waveformat.dwChannelMask |= pi_channels_out[i];
    }*/

/*    switch( i_format )
    {
    case VLC_FOURCC('s','p','d','i'):
        i_nb_channels = 2;
        / * To prevent channel re-ordering * /
        waveformat.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_FOURCC('f','l','3','2'):
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_FOURCC('s','1','6','l'):*/
        waveformat.Format.wBitsPerSample = audio_bits;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_PCM;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_PCM;
/*        break;
    }*/

    waveformat.Format.nChannels = audio_channels;
    waveformat.Format.nSamplesPerSec = audio_samplerate;
    waveformat.Format.nBlockAlign =
                     waveformat.Format.wBitsPerSample / 8 * audio_channels;
    waveformat.Format.nAvgBytesPerSec =
               waveformat.Format.nSamplesPerSec * waveformat.Format.nBlockAlign;

    audio_bytes_per_sample = waveformat.Format.wBitsPerSample / 8 * audio_channels;

    VERBOSE(VB_IMPORTANT, "New format: " << audio_bits << "bits, " << audio_channels << "ch, " << audio_samplerate << "Hz");

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if (audio_channels <= 2)
    {
        waveformat.Format.cbSize = 0;
    }
    else
    {
        waveformat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        waveformat.Format.cbSize =
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }


    /* Then fill in the direct sound descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_CTRLPOSITIONNOTIFY     /* We need notification */
                    | DSBCAPS_GLOBALFOCUS       /* Allows background playing */
                    | DSBCAPS_LOCHARDWARE;      /* Needed for 5.1 on emu101k */
    dsbdesc.dwBufferBytes = FRAMES_NUM * audio_bits/8 * audio_channels;   /* buffer size */
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&waveformat;

    if FAILED(IDirectSound_CreateSoundBuffer(dsobject, &dsbdesc, &dsbuffer, NULL))
    {
        /* Try without DSBCAPS_LOCHARDWARE */
        dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
        if FAILED(IDirectSound_CreateSoundBuffer(dsobject, &dsbdesc, &dsbuffer, NULL))
        {
            return -1;
        }
        if ( !b_probe ) VERBOSE(VB_IMPORTANT, "couldn't use hardware sound buffer" );
    }

    write_cursor = 0;
    buffer_size = FRAMES_NUM * audio_bits/8 * audio_channels;

    /* Stop here if we were just probing */
    if ( b_probe )
    {
        IDirectSoundBuffer_Release(dsbuffer);
        dsbuffer = NULL;
        return 0;
    }

    /* Now the secondary buffer is created, we need to setup its position
     * notification */
    for (i = 0; i < 4; i++)
    {
        notif[i].dwOffset = buffer_size / 4 * i;
    }

    /* Get the IDirectSoundNotify interface */
    if FAILED(IDirectSoundBuffer_QueryInterface(dsbuffer, IID_IDirectSoundNotify, (LPVOID*) &dsnotify))
    {
        VERBOSE(VB_IMPORTANT, "cannot get IDirectSoundNotify interface" );
        goto error;
    }

    if FAILED(IDirectSoundNotify_SetNotificationPositions(dsnotify, 4, notif))
    {
        VERBOSE(VB_IMPORTANT, "cannot set position notification" );
        goto error;
    }

//    p_aout->output.p_sys->i_channel_mask = waveformat.dwChannelMask;
//    CheckReordering( p_aout, i_nb_channels );

    VERBOSE(VB_IMPORTANT, "created DirectSound buffer");

    return 0;

 error:
    DestroyDSBuffer();
    return -1;
}


/*****************************************************************************
 * DestroyDSBuffer
 *****************************************************************************
 * This function destroys the secondary buffer.
 *****************************************************************************/
void AudioOutputDX::DestroyDSBuffer()
{
    if (dsnotify)
    {
        IDirectSoundNotify_Release(dsnotify);
        dsnotify = NULL;
    }

    VERBOSE(VB_IMPORTANT, "destroying DirectSound buffer");

    if (dsbuffer)
    {
        IDirectSoundBuffer_Stop(dsbuffer);
        IDirectSoundBuffer_Release(dsbuffer);
        dsbuffer = NULL;
    }
}

/*****************************************************************************
 * FillBuffer: Fill in one of the direct sound frame buffers.
 *****************************************************************************
 * Returns VLC_SUCCESS on success.
 *****************************************************************************/
int AudioOutputDX::FillBuffer(int frames, char *buffer)
{
    DWORD play_pos, write_pos, end_write;
    void *p_write_position, *p_wrap_around;
    DWORD l_bytes1, l_bytes2;
    HRESULT dsresult;

    if (!awaiting_data && !paused) 
    {
    
//        VERBOSE(VB_IMPORTANT, "checking buffer positions");
    
        dsresult = IDirectSoundBuffer_GetCurrentPosition(dsbuffer, &play_pos, &write_pos);

        if (dsresult != DS_OK)
        {
            VERBOSE(VB_IMPORTANT, "cannot get current buffer positions");
            return -1;
        }

        end_write = write_cursor + (frames + FRAMES_NUM/4) * audio_bytes_per_sample;

        while (!(((play_pos < write_pos) && (write_cursor >= write_pos || write_cursor < play_pos) &&
                                        (end_write >= write_pos || end_write < play_pos)) ||
            ((play_pos < write_pos) && (write_cursor >= write_pos && write_cursor < play_pos) &&
                                        (end_write >= write_pos && end_write < play_pos))))
        {
//            VERBOSE(VB_IMPORTANT, "cannot write audio data: " << write_cursor << " " << write_pos << " " << play_pos << " sleeping");

            HANDLE  notification_events[4];

            for(int i = 0; i < 4; i++)
                notification_events[i] = notif[i].hEventNotify;

                WaitForMultipleObjects(4, notification_events, 0, INFINITE );
                
//                VERBOSE(VB_IMPORTANT, "woken");
                
            dsresult = IDirectSoundBuffer_GetCurrentPosition(dsbuffer, &play_pos, &write_pos);

            if (dsresult != DS_OK)
            {
                VERBOSE(VB_IMPORTANT, "cannot get current buffer positions");
                return -1;
            }

            end_write = write_cursor + (frames + FRAMES_NUM/4) * audio_bytes_per_sample;
        }
    }
                                    

//    VERBOSE(VB_IMPORTANT, "Locking buffer");

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock(
                dsbuffer,                                       /* DS buffer */
                write_cursor,                                   /* Start offset */
                frames * audio_bytes_per_sample,          /* Number of bytes */
                &p_write_position,                  /* Address of lock start */
                &l_bytes1,       /* Count of bytes locked before wrap around */
                &p_wrap_around,            /* Buffer adress (if wrap around) */
                &l_bytes2,               /* Count of bytes after wrap around */
                0);                                                   /* Flags */
    if (dsresult == DSERR_BUFFERLOST)
    {
        IDirectSoundBuffer_Restore(dsbuffer);
        dsresult = IDirectSoundBuffer_Lock(
                               dsbuffer,
                               write_cursor,
                               frames * audio_bytes_per_sample,
                               &p_write_position,
                               &l_bytes1,
                               &p_wrap_around,
                               &l_bytes2,
                               0);
    }
    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, "cannot lock buffer");
        return -1;
    }

    if (buffer == NULL)
    {
        VERBOSE(VB_IMPORTANT, "Writing null bytes");
    
        memset(p_write_position, 0, l_bytes1);
        if (p_wrap_around)
            memset(p_wrap_around, 0, l_bytes2);
            
           write_cursor += l_bytes1 + l_bytes2;
           
           while (write_cursor >= buffer_size)
               write_cursor -= buffer_size;
    }
/*    else if ( p_aout->output.p_sys->b_chan_reorder )
    {
        /* Do the channel reordering here * /

        if ( p_aout->output.output.i_format ==  VLC_FOURCC('s','1','6','l') )
          InterleaveS16( (int16_t *)p_buffer->p_buffer,
                         (int16_t *)p_write_position,
                         p_aout->output.p_sys->pi_chan_table,
                         aout_FormatNbChannels( &p_aout->output.output ) );
        else
          InterleaveFloat32( (float *)p_buffer->p_buffer,
                             (float *)p_write_position,
                             p_aout->output.p_sys->pi_chan_table,
                             aout_FormatNbChannels( &p_aout->output.output ) );

        aout_BufferFree( p_buffer );
    }*/
    else    
    {
//        VERBOSE(VB_IMPORTANT, "filling buffer");
    
        memcpy(p_write_position, buffer, l_bytes1);
//        VERBOSE(VB_IMPORTANT, "buf_fill: " << l_bytes1 << " bytes, " << p_write_position << "-" << ((int)p_write_position + l_bytes1));
        if (p_wrap_around) {
            memcpy(p_wrap_around, buffer + l_bytes1, l_bytes2);
//        VERBOSE(VB_IMPORTANT, "buf_fill2: " << l_bytes2 << " bytes, " << p_wrap_around << "-" << ((int)p_wrap_around + l_bytes2));
        }
            
           write_cursor += l_bytes1 + l_bytes2;
           
           while (write_cursor >= buffer_size)
               write_cursor -= buffer_size;
    }

//    VERBOSE(VB_IMPORTANT, "unlocking buffer");

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock(dsbuffer, p_write_position, l_bytes1,
                              p_wrap_around, l_bytes2 );


//    VERBOSE(VB_IMPORTANT, "finished fillbuffer");

    return 0;
}

// Wait for all data to finish playing
void AudioOutputDX::Drain()
{
    // TODO: Wait until all data has been played...

}

int AudioOutputDX::GetVolumeChannel(int channel)
{
    // Do nothing
    return 100;
}
void AudioOutputDX::SetVolumeChannel(int channel, int volume)
{
    // Do nothing
}

