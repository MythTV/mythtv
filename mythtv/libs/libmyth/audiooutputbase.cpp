#include <qstring.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace std;
#include "audiooutputbase.h"

#include <iostream>
#include <qdatetime.h>
#include <sys/time.h>
#include <unistd.h>


AudioOutputBase::AudioOutputBase(QString audiodevice, int, 
                                 int, int,
                                 AudioOutputSource source, bool set_initial_vol)
{
    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);
    pthread_cond_init(&audio_bufsig, NULL);
    this->audiodevice = audiodevice;

    output_audio = 0;
    audio_bits = -1;
    audio_channels = -1;
    audio_samplerate = -1;    
    current_seconds = -1;
    source_bitrate = -1;
    audio_stretchfactor = 1.0;
    pSoundStretch = NULL;
    blocking = false;
    this->source = source;
    this->set_initial_vol = set_initial_vol;
    soundcard_buffer_size = 0;
    buffer_output_data_for_use = false; // used by AudioOutputNULL


    src_ctx = NULL;

    // You need to call the next line from your concrete class.
    // Virtuals cause problems in the base class constructors
    // Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputBase::~AudioOutputBase()
{
    // Make sure you call the next line in your concrete class to ensure everything is shutdown correctly.
    // Cant be called here due to use of virtual functions
    // KillAudio();
    
    pthread_mutex_destroy(&audio_buflock);
    pthread_mutex_destroy(&avsync_lock);
    pthread_cond_destroy(&audio_bufsig);
}

void AudioOutputBase::SetSourceBitrate(int rate)
{
    if (rate > 0)
        source_bitrate = rate;
}

void AudioOutputBase::SetStretchFactorLocked(float laudio_stretchfactor)
{
    effdspstretched = (int)((float)effdsp / laudio_stretchfactor);
    if (audio_stretchfactor != laudio_stretchfactor)
    {
        audio_stretchfactor = laudio_stretchfactor;
        if (pSoundStretch)
        {
            VERBOSE(VB_GENERAL, QString("Changing time stretch to %1")
                                        .arg(audio_stretchfactor));
            pSoundStretch->setTempo(audio_stretchfactor);
        }
        else if (audio_stretchfactor != 1.0)
        {
            VERBOSE(VB_GENERAL, QString("Using time stretch %1")
                                        .arg(audio_stretchfactor));
            pSoundStretch = new soundtouch::SoundTouch();
            pSoundStretch->setSampleRate(audio_samplerate);
            pSoundStretch->setChannels(audio_channels);

            pSoundStretch->setTempo(audio_stretchfactor);
            pSoundStretch->setSetting(SETTING_SEQUENCE_MS, 35);

            // dont need these with only tempo change
            //pSoundStretch->setPitch(1.0);
            //pSoundStretch->setRate(1.0);

            //pSoundStretch->setSetting(SETTING_USE_QUICKSEEK, true);
            //pSoundStretch->setSetting(SETTING_USE_AA_FILTER, false);
        }
    }
}

void AudioOutputBase::SetStretchFactor(float laudio_stretchfactor)
{
    pthread_mutex_lock(&audio_buflock);
    SetStretchFactorLocked(laudio_stretchfactor);
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputBase::Reconfigure(int laudio_bits, int laudio_channels, 
                                 int laudio_samplerate)
{
    if (laudio_bits == audio_bits && laudio_channels == audio_channels &&
        laudio_samplerate == audio_samplerate && !need_resampler)
        return;

    KillAudio();
    
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    lastaudiolen = 0;
    waud = raud = 0;
    audio_actually_paused = false;
    
    audio_channels = laudio_channels;
    audio_bits = laudio_bits;
    audio_samplerate = laudio_samplerate;
    if (audio_bits != 8 && audio_bits != 16)
    {
        Error("AudioOutput only supports 8 or 16bit audio.");
        return;
    }
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    need_resampler = false;
    killaudio = false;
    pauseaudio = false;
    was_paused = true;
    internal_vol = gContext->GetNumSetting("MythControlsVolume", 0);
    
    numlowbuffer = 0;

    VERBOSE(VB_GENERAL, QString("Opening audio device '%1'.")
            .arg(audiodevice));
    
    // Actually do the device specific open call
    if (!OpenDevice()) {
        pthread_mutex_unlock(&avsync_lock);
        pthread_mutex_unlock(&audio_buflock);
        VERBOSE(VB_AUDIO, "Aborting reconfigure");
        return;
    }

    SyncVolume();
    
    VERBOSE(VB_AUDIO, QString("Audio fragment size: %1")
            .arg(fragment_size));

    if (audio_buffer_unused < 0)
        audio_buffer_unused = 0;

    if (!gContext->GetNumSetting("AggressiveSoundcardBuffer", 0))
        audio_buffer_unused = 0;

    audbuf_timecode = 0;
    audiotime = 0;
    effdsp = audio_samplerate * 100;
    gettimeofday(&audiotime_updated, NULL);
    current_seconds = -1;
    source_bitrate = -1;

    // Check if we need the resampler
    if (audio_samplerate != laudio_samplerate)
    {
        int error;
        VERBOSE(VB_GENERAL, QString("Using resampler. From: %1 to %2")
                               .arg(laudio_samplerate).arg(audio_samplerate));
        src_ctx = src_new (SRC_SINC_BEST_QUALITY, audio_channels, &error);
        if (error)
        {
            Error(QString("Error creating resampler, the error was: %1")
                  .arg(src_strerror(error)) );
            return;
        }
        src_data.src_ratio = (double) audio_samplerate / laudio_samplerate;
        src_data.data_in = src_in;
        src_data.data_out = src_out;
        src_data.output_frames = 16384*6;
        need_resampler = true;
    }

    VERBOSE(VB_AUDIO, QString("Audio Stretch Factor: %1").arg(audio_stretchfactor));

    SetStretchFactorLocked(audio_stretchfactor);
    if (pSoundStretch)
    {
        pSoundStretch->setSampleRate(audio_samplerate);
        pSoundStretch->setChannels(audio_channels);
    }

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    StartOutputThread();
    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
    VERBOSE(VB_AUDIO, "Ending reconfigure");
}

void AudioOutputBase::StartOutputThread(void)
{
    pthread_create(&output_audio, NULL, kickoffOutputAudioLoop, this);    
}


void AudioOutputBase::StopOutputThread(void)
{
    if (output_audio)
    {
        pthread_join(output_audio, NULL);
        output_audio = 0;
    }
}

void AudioOutputBase::KillAudio()
{
    killAudioLock.lock();

    VERBOSE(VB_AUDIO, "Killing AudioOutputDSP");
    killaudio = true;
    StopOutputThread();

    // Close resampler?
    if (src_ctx)
        src_delete(src_ctx);
    need_resampler = false;

    // close sound stretcher
    if (pSoundStretch)
    {
        delete pSoundStretch;
        pSoundStretch = NULL;
    }

    CloseDevice();

    killAudioLock.unlock();
}


bool AudioOutputBase::GetPause(void)
{
    return audio_actually_paused;
}

void AudioOutputBase::Pause(bool paused)
{
    pauseaudio = paused;
    audio_actually_paused = false;
}

void AudioOutputBase::Reset()
{
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    raud = waud = 0;
    audbuf_timecode = 0;
    audiotime = 0;
    current_seconds = -1;
    was_paused = !pauseaudio;

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputBase::SetTimecode(long long timecode)
{
    pthread_mutex_lock(&audio_buflock);
    audbuf_timecode = timecode;
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputBase::SetEffDsp(int dsprate)
{
    VERBOSE(VB_AUDIO, QString("SetEffDsp: %1").arg(dsprate));
    effdsp = dsprate;
    effdspstretched = (int)((float)effdsp / audio_stretchfactor);
}

void AudioOutputBase::SetBlocking(bool blocking)
{
    this->blocking = blocking;
}

int AudioOutputBase::audiolen(bool use_lock)
{
    /* Thread safe, returns the number of valid bytes in the audio buffer */
    int ret;
    
    if (use_lock) 
        pthread_mutex_lock(&audio_buflock);

    if (waud >= raud)
        ret = waud - raud;
    else
        ret = AUDBUFSIZE - (raud - waud);

    if (use_lock)
        pthread_mutex_unlock(&audio_buflock);

    return ret;
}

int AudioOutputBase::audiofree(bool use_lock)
{
    return AUDBUFSIZE - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is AUDBUFSIZE - 1. */
}

int AudioOutputBase::GetAudiotime(void)
{
    /* Returns the current timecode of audio leaving the soundcard, based
       on the 'audiotime' computed earlier, and the delay since it was computed.

       This is a little roundabout...

       The reason is that computing 'audiotime' requires acquiring the audio 
       lock, which the video thread should not do. So, we call 'SetAudioTime()'
       from the audio thread, and then call this from the video thread. */
    int ret;
    struct timeval now;

    if (audiotime == 0)
        return 0;

    pthread_mutex_lock(&avsync_lock);

    gettimeofday(&now, NULL);

    ret = (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;
    ret = (int)(ret * audio_stretchfactor);

    ret += audiotime;

    pthread_mutex_unlock(&avsync_lock);
    return ret;
}

void AudioOutputBase::SetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return;

    int soundcard_buffer = 0;
    int totalbuffer;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is samples/sec, multiplied by 100.
       Bytes per sample is assumed to be 4.

       'audiotimecode' is the timecode of the audio that has just been 
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer.

       'ms/byte' is given by '25000/effdsp'...
     */

    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);
 
    soundcard_buffer = getBufferedOnSoundcard(); // bytes
    totalbuffer = audiolen(false) + soundcard_buffer;
 
    // include algorithmic latencies
    if (pSoundStretch)
    {
        // add the effect of unprocessed samples in time stretch algo
        totalbuffer += (int)((pSoundStretch->numUnprocessedSamples() *
                              audio_bytes_per_sample) / audio_stretchfactor);
    }
               
    audiotime = audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                   (audio_bytes_per_sample * effdspstretched));
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

bool AudioOutputBase::AddSamples(char *buffers[], int samples, 
                                 long long timecode)
{
    // NOTE: This function is not threadsafe
    int afree = audiofree(true);
    int len = samples * audio_bytes_per_sample;

    // Check we have enough space to write the data
    if (need_resampler && src_ctx)
        len = (int)ceilf(float(len) * src_data.src_ratio);

    if ((len > afree) && !blocking)
        return false; // would overflow

    // resample input if necessary
    if (need_resampler && src_ctx) 
    {
        // Convert to floats
        // TODO: Implicit assumption dealing with 16 bit input only.
        short **buf_ptr = (short**)buffers;
        for (int sample = 0; sample < samples; sample++) 
        {
            for (int channel = 0; channel < audio_channels; channel++) 
            {
                src_in[sample] = buf_ptr[channel][sample] / (1.0 * 0x8000);
            }
        }

        src_data.input_frames = samples;
        src_data.end_of_input = 0;
        int error = src_process(src_ctx, &src_data);
        if (error)
            VERBOSE(VB_IMPORTANT, QString("Error occured while resampling "
                    "audio: %1").arg(src_strerror(error)));

        src_float_to_short_array(src_data.data_out, (short int*)tmp_buff,
                                 src_data.output_frames_gen*audio_channels);

        _AddSamples(tmp_buff, true, src_data.output_frames_gen, timecode);
    } 
    else 
    {
        // Call our function to do the work
        _AddSamples(buffers, false, samples, timecode);
    }

    return true;
}

bool AudioOutputBase::AddSamples(char *buffer, int samples, long long timecode)
{
    // NOTE: This function is not threadsafe

    int afree = audiofree(true);
    int len = samples * audio_bytes_per_sample;

    // Check we have enough space to write the data
    if (need_resampler && src_ctx)
        len = (int)ceilf(float(len) * src_data.src_ratio);
    if ((len > afree) && !blocking)
        return false; // would overflow

    // resample input if necessary
    if (need_resampler && src_ctx) 
    {
        // Convert to floats
        short *buf_ptr = (short*)buffer;
        for (int sample = 0; sample < samples * audio_channels; sample++) 
        {       
            src_in[sample] = (float)buf_ptr[sample] / (1.0 * 0x8000);
        }
        
        src_data.input_frames = samples;
        src_data.end_of_input = 0;
        int error = src_process(src_ctx, &src_data);
        if (error)
            VERBOSE(VB_IMPORTANT, QString("Error occured while resampling "
                    "audio: %1").arg(src_strerror(error)));
        src_float_to_short_array(src_data.data_out, (short int*)tmp_buff, 
                                 src_data.output_frames_gen*audio_channels);

        _AddSamples(tmp_buff, true, src_data.output_frames_gen, timecode);
    } 
    else 
    {
        // Call our function to do the work
        _AddSamples(buffer, true, samples, timecode);
    }

    return true;
}

int AudioOutputBase::WaitForFreeSpace(int samples)
{
    int len = samples * audio_bytes_per_sample;
    int afree = audiofree(false);

    while (len > afree)
    {
        if (blocking)
        {
            VERBOSE(VB_AUDIO, "Waiting for free space");
            // wait for more space
            pthread_cond_wait(&audio_bufsig, &audio_buflock);
            afree = audiofree(false);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
            samples = afree / audio_bytes_per_sample;
            len = samples * audio_bytes_per_sample;
            if (src_ctx) 
            {
                int error = src_reset(src_ctx);
                if (error)
                    VERBOSE(VB_IMPORTANT, QString("Error occured while "
                            "resetting resampler: %1")
                            .arg(src_strerror(error)));
            }
        }
    }
    return len;
}

void AudioOutputBase::_AddSamples(void *buffer, bool interleaved, int samples, 
                                  long long timecode)
{
    pthread_mutex_lock(&audio_buflock);

    int len; // = samples * audio_bytes_per_sample;
    int audio_bytes = audio_bits / 8;
    int org_waud = waud;
    
    int afree = audiofree(false);

    VERBOSE(VB_AUDIO, QString("_AddSamples bytes=%1, used=%2, free=%3, timecode=%4")
            .arg(samples * audio_bytes_per_sample)
            .arg(AUDBUFSIZE-afree).arg(afree).arg((long)timecode));
    
    len = WaitForFreeSpace(samples);

    if (interleaved) 
    {
        char *mybuf = (char*)buffer;
        int bdiff = AUDBUFSIZE - org_waud;
        if (bdiff < len)
        {
            memcpy(audiobuffer + org_waud, mybuf, bdiff);
            memcpy(audiobuffer, mybuf + bdiff, len - bdiff);
        }
        else
            memcpy(audiobuffer + org_waud, mybuf, len);
 
        org_waud = (org_waud + len) % AUDBUFSIZE;
    } 
    else 
    {
        char **mybuf = (char**)buffer;
        for (int itemp = 0; itemp < samples * audio_bytes; itemp += audio_bytes)
        {
            for (int chan = 0; chan < audio_channels; chan++)
            {
                audiobuffer[org_waud++] = mybuf[chan][itemp];
                if (audio_bits == 16)
                    audiobuffer[org_waud++] = mybuf[chan][itemp+1];

                if (org_waud >= AUDBUFSIZE)
                    org_waud -= AUDBUFSIZE;
            }
        }
    }

    if (pSoundStretch)
    {
        // does not change the timecode, only the number of samples
        // back to orig pos
        org_waud = waud;
        int bdiff = AUDBUFSIZE - org_waud;
        int nSamplesToEnd = bdiff/audio_bytes_per_sample;
        if (bdiff < len)
        {
            pSoundStretch->putSamples((soundtouch::SAMPLETYPE*)(audiobuffer + 
                                      org_waud), nSamplesToEnd);
            pSoundStretch->putSamples((soundtouch::SAMPLETYPE*)audiobuffer,
                                      (len - bdiff) / audio_bytes_per_sample);
        }
        else
        {
            pSoundStretch->putSamples((soundtouch::SAMPLETYPE*)(audiobuffer + 
                                      org_waud), len / audio_bytes_per_sample);
        }

        int newLen = 0;
        int nSamples;
        len = WaitForFreeSpace(pSoundStretch->numSamples() * 
                               audio_bytes_per_sample);
        do 
        {
            int samplesToGet = len/audio_bytes_per_sample;
            if (samplesToGet > nSamplesToEnd)
            {
                samplesToGet = nSamplesToEnd;    
            }

            nSamples = pSoundStretch->receiveSamples((soundtouch::SAMPLETYPE*)
                                      (audiobuffer + org_waud), samplesToGet);
            if (nSamples == nSamplesToEnd)
            {
                org_waud = 0;
                nSamplesToEnd = AUDBUFSIZE/audio_bytes_per_sample;
            }
            else
            {
                org_waud += nSamples * audio_bytes_per_sample;
                nSamplesToEnd -= nSamples;
            }

            newLen += nSamples * audio_bytes_per_sample;
            len -= nSamples * audio_bytes_per_sample;
        } while (nSamples > 0);
    }

    waud = org_waud;
    lastaudiolen = audiolen(false);

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    // even with timestretch, timecode is still calculated from original
    // sample count
    audbuf_timecode = timecode + (int)((samples * 100000.0) / effdsp);
    if (interleaved)
        dispatchVisual((unsigned char *)buffer, len, timecode, audio_channels, audio_bits);

    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputBase::Status()
{
    long ct = GetAudiotime();

    if (ct < 0)
        ct = 0;

    if (source_bitrate == -1)
    {
        source_bitrate = audio_samplerate * audio_channels * audio_bits;
    }

    if (ct / 1000 != current_seconds) 
    {
        current_seconds = ct / 1000;
        OutputEvent e(current_seconds, ct,
                      source_bitrate, audio_samplerate, audio_bits, 
                      audio_channels);
        dispatch(e);
    }
}

void AudioOutputBase::OutputAudioLoop(void)
{
    int space_on_soundcard, last_space_on_soundcard;
    unsigned char zeros[fragment_size];
    unsigned char fragment[fragment_size];
 
    bzero(zeros, fragment_size);
    last_space_on_soundcard = 0;

    while (!killaudio)
    {
        if (pauseaudio)
        {
            if (!audio_actually_paused)
            {
                VERBOSE(VB_AUDIO, "OutputAudioLoop: audio paused");
                OutputEvent e(OutputEvent::Paused);
                dispatch(e);
                was_paused = true;
            }

            audio_actually_paused = true;
            
            audiotime = 0; // mark 'audiotime' as invalid.

            space_on_soundcard = getSpaceOnSoundcard();

            if (space_on_soundcard != last_space_on_soundcard) {
                VERBOSE(VB_AUDIO, QString("%1 bytes free on soundcard")
                        .arg(space_on_soundcard));
                last_space_on_soundcard = space_on_soundcard;
            }

            // only send zeros if card doesn't already have at least one
            // fragment of zeros -dag
            if (fragment_size >= soundcard_buffer_size - space_on_soundcard) 
            {
                if (fragment_size <= space_on_soundcard) {
                    WriteAudio(zeros, fragment_size);
                } else {
                    // this should never happen now -dag
                    VERBOSE(VB_AUDIO,
                            QString("waiting for space on soundcard "
                                    "to write zeros: have %1 need %2")
                            .arg(space_on_soundcard).arg(fragment_size));
                    usleep(5000);
                }
            }

            usleep(2000);
            continue;
        }
        else
        {
            if (was_paused) 
            {
                VERBOSE(VB_AUDIO, "OutputAudioLoop: Play Event");
                OutputEvent e(OutputEvent::Playing);
                dispatch(e);
                was_paused = false;
            }
        }

        space_on_soundcard = getSpaceOnSoundcard();
        
        // if nothing has gone out the soundcard yet no sense calling
        // this (think very fast loops here when soundcard doesn't have
        // space to take another fragment) -dag
        if (space_on_soundcard != last_space_on_soundcard)
            SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
        
        // wait for the buffer to fill with enough to play
        if (fragment_size > audiolen(true))
        {
            if (audiolen(true) > 0)  // only log if we're sending some audio
                VERBOSE(VB_AUDIO,
                        QString("audio waiting for buffer to fill: "
                                "have %1 want %2")
                        .arg(audiolen(true)).arg(fragment_size));

            VERBOSE(VB_AUDIO, "Broadcasting free space avail");
            pthread_mutex_lock(&audio_buflock);
            pthread_cond_broadcast(&audio_bufsig);
            pthread_mutex_unlock(&audio_buflock);

            usleep(2000);
            continue;
        }
        
        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock
        if (fragment_size > space_on_soundcard)
        {
            if (space_on_soundcard != last_space_on_soundcard) {
                VERBOSE(VB_AUDIO,
                        QString("audio waiting for space on soundcard: "
                                "have %1 need %2")
                        .arg(space_on_soundcard).arg(fragment_size));
                last_space_on_soundcard = space_on_soundcard;
            }

            numlowbuffer++;
            if (numlowbuffer > 5 && audio_buffer_unused)
            {
                VERBOSE(VB_IMPORTANT, "dropping back audio_buffer_unused");
                audio_buffer_unused /= 2;
            }

            usleep(5000);
            continue;
        }
        else
            numlowbuffer = 0;

        Status();

        if (GetAudioData(fragment, fragment_size, true))
            WriteAudio(fragment, fragment_size);
    }

    VERBOSE(VB_AUDIO, "OutputAudioLoop: Stop Event");
    OutputEvent e(OutputEvent::Stopped);
    dispatch(e);
}

int AudioOutputBase::GetAudioData(unsigned char *buffer, int buf_size, bool full_buffer)
{
    pthread_mutex_lock(&audio_buflock); // begin critical section
    
    // re-check audiolen() in case things changed.
    // for example, ClearAfterSeek() might have run
    int avail_size = audiolen(false);
    int fragment_size = buf_size;
    int written_size = 0;
    if (!full_buffer && (buf_size > avail_size))
    {
        // when full_buffer is false, return any available data
        fragment_size = avail_size;
    }
    
    if (avail_size && (fragment_size <= avail_size))
    {
        int bdiff = AUDBUFSIZE - raud;
        if (fragment_size > bdiff)
        {
            // always want to write whole fragments
            memcpy(buffer, audiobuffer + raud, bdiff);
            memcpy(buffer + bdiff, audiobuffer, fragment_size - bdiff);
        }
        else
        {
            memcpy(buffer, audiobuffer + raud, fragment_size);
        }

        /* update raud */
        raud = (raud + fragment_size) % AUDBUFSIZE;
        VERBOSE(VB_AUDIO, "Broadcasting free space avail");
        pthread_cond_broadcast(&audio_bufsig);

        written_size = fragment_size;
    }
    pthread_mutex_unlock(&audio_buflock); // end critical section
    
    // Mute individual channels through mono->stereo duplication
    kMuteState mute_state = GetMute();
    if (written_size &&
        audio_channels > 1 &&
        (mute_state == MUTE_LEFT || mute_state == MUTE_RIGHT))
    {
        int offset_src = 0;
        int offset_dst = 0;
        
        if (mute_state == MUTE_LEFT)
            offset_src = audio_bits / 8;    // copy channel 1 to channel 0
        else if (mute_state == MUTE_RIGHT)
            offset_dst = audio_bits / 8;    // copy channel 0 to channel 1
            
        for (int i = 0; i < written_size; i += audio_bytes_per_sample)
        {
            buffer[i + offset_dst] = buffer[i + offset_src];
            if (audio_bits == 16)
                buffer[i + offset_dst + 1] = buffer[i + offset_src + 1];
        }
    }
    
    return written_size;
}

// Wait for all data to finish playing
void AudioOutputBase::Drain()
{
    while (audiolen(true) > fragment_size)
    {
        usleep(1000);
    }
}

void *AudioOutputBase::kickoffOutputAudioLoop(void *player)
{
    VERBOSE(VB_AUDIO, QString("kickoffOutputAudioLoop: pid = %1")
                              .arg(getpid()));
    ((AudioOutputBase *)player)->OutputAudioLoop();
    VERBOSE(VB_AUDIO, "kickoffOutputAudioLoop exiting");
    return NULL;
}

int AudioOutputBase::readOutputData(unsigned char*, int)
{
    VERBOSE(VB_IMPORTANT, "base AudioOutputBase should not be "
                          "getting asked to readOutputData()");
    return 0;
}

