/*****************************************************************************
 * = NAME
 * audiooutputca.cpp
 *
 * = DESCRIPTION
 * Core Audio glue for Mac OS X.
 * This plays MythTV audio through the default output device on OS X.
 *
 * = BUGS
 * If digital passthru is set but there is no digital hardware,
 * the code disables audio to prevent outputting packetised static.
 * Analog fallback is possible, but the base class Reconfigure() doesn't
 * do the right thing. I think NVP and AFD need to retry if OpenDevice
 * fails and passthru was set.
 *
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Jeremiah Morris, Andrew Kimpton, Nigel Pearson, Jean-Yves Avenard
 *****************************************************************************/

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

using namespace std;

#include "mythcorecontext.h"
#include "audiooutputca.h"
#include "config.h"
#include "SoundTouch.h"

#define LOC QString("CoreAudio: ")
#define LOC_WARN QString("CoreAudio, Warning: ")
#define LOC_ERR QString("CoreAudio, Error: ")

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

/** \class CoreAudioData
 *  \brief This holds Core Audio member variables and low-level audio IO methods
 * The name is now a misnomer, it should be CoreAudioPrivate, or CoreAudioMgr
 */
class CoreAudioData {
public:
    CoreAudioData(AudioOutputCA *parent);


    bool OpenDevice();
    bool OpenAnalog();
    bool OpenSPDIF ();
    void CloseSPDIF();

    pid_t GetHogPID();
    bool  SetHogPID(pid_t pid);
    bool  SetHogMode();
    void  ReleaseHogMode()   { SetHogPID(-1); }

    bool SetUnMixable();

    bool FindAC3Stream();
    void ResetAudioDevices();
    void ResetStream(AudioStreamID s);
    int *RatesList(AudioDeviceID d);
    bool *ChannelsList(AudioDeviceID d, bool passthru);

    // Helpers for iterating. Returns a malloc'd array
    AudioStreamID               *StreamsList(AudioDeviceID d);
    AudioStreamBasicDescription *FormatsList(AudioStreamID s);

    int  AudioStreamChangeFormat(AudioStreamID               s,
                                 AudioStreamBasicDescription format);

    void  Debug(QString msg)
    {   VERBOSE(VB_AUDIO,      "CoreAudioData::" + msg);   }

    void  Error(QString msg)
    {    VERBOSE(VB_IMPORTANT, "AudioOutputCA Error: " + msg);   }

    void  Warn (QString msg)
    {    VERBOSE(VB_IMPORTANT, "AudioOutputCA Warning: " + msg);   }



    AudioOutputCA  *mCA;    // We could subclass, but this ends up tidier

    // Analog output specific
    AudioUnit      mOutputUnit;

    // SPDIF mode specific
    bool           mDigitalInUse;   // Is the digital (SPDIF) output in use?
    AudioDeviceID  mDeviceID;
    AudioStreamID  mStreamID;       // StreamID that has a cac3 streamformat
    int            mStreamIndex;    // Index of mStreamID in an AudioBufferList
    AudioStreamBasicDescription
                   mFormatOrig,     // The original format the stream
                   mFormatNew;      // The format we changed the stream to
    bool           mRevertFormat;   // Do we need to revert the stream format?
    bool           mChangedMixing;  // Do we need to set the mixing mode back?
};

// These callbacks communicate with Core Audio.
static OSStatus RenderCallbackAnalog(void                       *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp       *inTimeStamp,
                                     UInt32                     inBusNumber,
                                     UInt32                     inNumberFrames,
                                     AudioBufferList            *ioData);
static OSStatus RenderCallbackSPDIF(AudioDeviceID        inDevice,
                                    const AudioTimeStamp *inNow,
                                    const void           *inInputData,
                                    const AudioTimeStamp *inInputTime,
                                    AudioBufferList      *outOutputData,
                                    const AudioTimeStamp *inOutputTime,
                                    void                 *threadGlobals);


/** \class AudioOutputCA
 *  \brief Implements Core Audio (Mac OS X Hardware Abstraction Layer) output.
 */

AudioOutputCA::AudioOutputCA(const AudioSettings &settings)
    : AudioOutputBase(settings),
      d(new CoreAudioData(this))
{
    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputCA::~AudioOutputCA()
{
    KillAudio();

    delete d;
}

// This was moved from the AudioOutputCA class, but I have kept it
// here in the file to minimise the size of the version control diffs
bool CoreAudioData::OpenAnalog()
{
    // Get default output device
    ComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL)
    {
        Error("FindNextComponent failed");
        return false;
    }

    OSStatus err = OpenAComponent(comp, &mOutputUnit);
    if (err)
    {
        Error(QString("OpenAComponent returned %1").arg((long)err));
        return false;
    }

    // Attach callback to default output
    AURenderCallbackStruct input;
    input.inputProc = RenderCallbackAnalog;
    input.inputProcRefCon = this;

    err = AudioUnitSetProperty(mOutputUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0, &input, sizeof(input));
    if (err)
    {
        Error(QString("AudioUnitSetProperty (callback) returned %1")
                      .arg((long)err));
        return false;
    }

    int formatFlags;
    switch (mCA->output_format)
    {
        case FORMAT_S16:
            formatFlags = kLinearPCMFormatFlagIsSignedInteger;
            break;
        case FORMAT_FLT:
            formatFlags = kLinearPCMFormatFlagIsFloat;
            break;
        default:
            formatFlags = kLinearPCMFormatFlagIsSignedInteger;
            break;
    }

    // Set up the audio output unit
    AudioStreamBasicDescription conv_in_desc;
    bzero(&conv_in_desc, sizeof(AudioStreamBasicDescription));
    conv_in_desc.mSampleRate       = mCA->samplerate;
    conv_in_desc.mFormatID         = kAudioFormatLinearPCM;
    conv_in_desc.mFormatFlags      = formatFlags;
#if HAVE_BIGENDIAN
    conv_in_desc.mFormatFlags     |= kLinearPCMFormatFlagIsBigEndian;
#endif
    conv_in_desc.mBytesPerPacket   = mCA->output_bytes_per_frame;
    // This seems inefficient, does it hurt if we increase this?
    conv_in_desc.mFramesPerPacket  = 1;
    conv_in_desc.mBytesPerFrame    = mCA->output_bytes_per_frame;
    conv_in_desc.mChannelsPerFrame = mCA->channels;
    conv_in_desc.mBitsPerChannel   =
        AudioOutputSettings::FormatToBits(mCA->output_format);

    err = AudioUnitSetProperty(mOutputUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0, &conv_in_desc,
                               sizeof(AudioStreamBasicDescription));
    if (err)
    {
        Error(QString("AudioUnitSetProperty returned %1").arg((long)err));
        return false;
    }

    // We're all set up - start the audio output unit
    ComponentResult res = AudioUnitInitialize(mOutputUnit);
    if (res)
    {
        Error(QString("AudioUnitInitialize returned %1").arg((long)res));
        return false;
    }

    err = AudioOutputUnitStart(mOutputUnit);
    if (err)
    {
        Error(QString("AudioOutputUnitStart returned %1").arg((long)err));
        return false;
    }

    return true;
}

AudioOutputSettings* AudioOutputCA::GetOutputSettings()
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    // Seek hardware sample rate available
    int rate;
    int *rates = d->RatesList(d->mDeviceID);
    int *p_rates = rates;

    while (*p_rates > 0 && (rate = settings->GetNextRate()))
    {
        if (*p_rates == rate)
        {
            settings->AddSupportedRate(*p_rates);
            p_rates++;
        }
    }
    free(rates);

    // Supported format: 16 bits audio or float
    settings->AddSupportedFormat(FORMAT_S16);
    settings->AddSupportedFormat(FORMAT_FLT);

    bool *channels = d->ChannelsList(d->mDeviceID, passthru);
    if (channels == NULL)
    {
        // Error retrieving list of supported channels, assume stereo only
        settings->AddSupportedChannels(2);
    }
    else
    {
        for (int i = CHANNELS_MIN; i <= CHANNELS_MAX; i++)
        {
            if (channels[i])
            {
                Debug(QString("Support %1 channels").arg(i));
                // In case 8 channels are supported but not 6, fake 6
                if (i == 8 && !channels[6])
                    settings->AddSupportedChannels(6);
                settings->AddSupportedChannels(i);
            }
        }
        free(channels);
    }

    settings->setPassthrough(0); // Maybe passthrough

    return settings;
}

bool AudioOutputCA::OpenDevice()
{
    bool deviceOpened = false;

    if (passthru || enc)
    {
        if (!d->FindAC3Stream())
        {
            Error("Configured for AC3 passthru but can't find an AC3"
                  " output stream. Disabling audio to prevent static");
            return false;
        }

        Debug("OpenDevice() Trying Digital.");
        deviceOpened = d->OpenSPDIF();
    }

    if (!deviceOpened)
    {
        Debug("OpenDevice() Trying Analog.");
        deviceOpened = d->OpenAnalog();
    }

    if (!deviceOpened)
    {
        Error("Couldn't open any audio device!");
        return false;
    }

    if (internal_vol && set_initial_vol)
    {
        QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";
        SetCurrentVolume(gCoreContext->GetNumSetting(controlLabel, 80));
    }

    return true;
}

void AudioOutputCA::CloseDevice()
{
    if (d->mOutputUnit)
    {
        AudioOutputUnitStop(d->mOutputUnit);
        AudioUnitUninitialize(d->mOutputUnit);
        AudioUnitReset(d->mOutputUnit, kAudioUnitScope_Input, NULL);
        CloseComponent(d->mOutputUnit);
        d->mOutputUnit = NULL;
    }

    if (d->mDigitalInUse)
        d->CloseSPDIF();
}

template <class AudioDataType>
static inline void _ReorderSmpteToCA(AudioDataType *buf, uint frames)
{
    AudioDataType tmpLS, tmpRS, tmpRLs, tmpRRs, *buf2;
    for (uint i = 0; i < frames; i++)
    {
        buf = buf2 = buf + 4;
        tmpRLs = *buf++;
        tmpRRs = *buf++;
        tmpLS = *buf++;
        tmpRS = *buf++;
        
        *buf2++ = tmpLS;
        *buf2++ = tmpRS;
        *buf2++ = tmpRLs;
        *buf2++ = tmpRRs;
    }
}

static inline void ReorderSmpteToCA(void *buf, uint frames, AudioFormat format)
{
    switch(AudioOutputSettings::FormatToBits(format))
    {
        case  8: _ReorderSmpteToCA((uchar *)buf, frames); break;
        case 16: _ReorderSmpteToCA((short *)buf, frames); break;
        default: _ReorderSmpteToCA((int   *)buf, frames); break;
    }
}

/** Object-oriented part of callback */
bool AudioOutputCA::RenderAudio(unsigned char *aubuf,
                                int size, unsigned long long timestamp)
{
    if (pauseaudio || killaudio)
    {
        actually_paused = true;
        return false;
    }

    /* This callback is called when the sound system requests
       data.  We don't want to block here, because that would
       just cause dropouts anyway, so we always return whatever
       data is available.  If we haven't received enough, either
       because we've finished playing or we have a buffer
       underrun, we play silence to fill the unused space.  */

    int written_size = GetAudioData(aubuf, size, false);
    if (written_size && (size > written_size))
    {
        // play silence on buffer underrun
        bzero(aubuf + written_size, size - written_size);
    }

    //Audio received is in SMPTE channel order, reorder to CA unless passthru
    if (!passthru && channels == 8)
    {
        ReorderSmpteToCA(aubuf, size / output_bytes_per_frame, output_format);
    }

    /* update audiotime (bufferedBytes is read by GetBufferedOnSoundcard) */
    UInt64 nanos = AudioConvertHostTimeToNanos(
                        timestamp - AudioGetCurrentHostTime());
    bufferedBytes = (int)((nanos / 1000000000.0) *    // secs
                          (effdsp / 100.0) *          // frames/sec
                          output_bytes_per_frame);    // bytes/frame

    return (written_size > 0);
}

void AudioOutputCA::WriteAudio(unsigned char *aubuf, int size)
{
    (void)aubuf;
    (void)size;
    return;     // unneeded and unused in CA
}

int AudioOutputCA::GetBufferedOnSoundcard(void) const
{
    return bufferedBytes;
}

/** Reimplement the base class's version of GetAudiotime()
 *  so that we don't use gettimeofday or Qt mutexes.
 */
int64_t AudioOutputCA::GetAudiotime(void)
{
    int audbuf_timecode = GetBaseAudBufTimeCode();

    if (!audbuf_timecode)
        return 0;

    int totalbuffer = audioready() + GetBufferedOnSoundcard();

    return audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                  (output_bytes_per_frame *
                                   effdsp * stretchfactor));
}

/* This callback provides converted audio data to the default output device. */
OSStatus RenderCallbackAnalog(void                       *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp       *inTimeStamp,
                              UInt32                     inBusNumber,
                              UInt32                     inNumberFrames,
                              AudioBufferList            *ioData)
{
    (void)inBusNumber;
    (void)inNumberFrames;

    AudioOutputCA *inst = (static_cast<CoreAudioData *>(inRefCon))->mCA;

    if (!inst->RenderAudio((unsigned char *)(ioData->mBuffers[0].mData),
                           ioData->mBuffers[0].mDataByteSize,
                           inTimeStamp->mHostTime))
    {
        // play silence if RenderAudio returns false
        bzero(ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);
        *ioActionFlags = kAudioUnitRenderAction_OutputIsSilence;
    }
    return noErr;
}

int AudioOutputCA::GetVolumeChannel(int channel) const
{
    // FIXME: this only returns global volume
    (void)channel;
    Float32 volume;

    if (!AudioUnitGetParameter(d->mOutputUnit,
                               kHALOutputParam_Volume,
                               kAudioUnitScope_Global, 0, &volume))
        return (int)lroundf(volume * 100.0f);

    return 0;    // error case
}

void AudioOutputCA::SetVolumeChannel(int channel, int volume)
{
    // FIXME: this only sets global volume
    (void)channel;
     AudioUnitSetParameter(d->mOutputUnit, kHALOutputParam_Volume,
                           kAudioUnitScope_Global, 0, (volume * 0.01f), 0);
}

// IOProc style callback for SPDIF audio output
static OSStatus RenderCallbackSPDIF(AudioDeviceID        inDevice,
                                    const AudioTimeStamp *inNow,
                                    const void           *inInputData,
                                    const AudioTimeStamp *inInputTime,
                                    AudioBufferList      *outOutputData,
                                    const AudioTimeStamp *inOutputTime,
                                    void                 *inRefCon)
{
    CoreAudioData    *d = static_cast<CoreAudioData *>(inRefCon);
    AudioOutputCA    *a = d->mCA;
    int           index = d->mStreamIndex;

    (void)inDevice;     // unused
    (void)inNow;        // unused
    (void)inInputData;  // unused
    (void)inInputTime;  // unused

    if (!a->RenderAudio((unsigned char *)(outOutputData->mBuffers[index].mData),
                        outOutputData->mBuffers[index].mDataByteSize,
                        inOutputTime->mHostTime))
        // play silence if RenderAudio returns false
        bzero(outOutputData->mBuffers[index].mData,
              outOutputData->mBuffers[index].mDataByteSize);

    return noErr;
}


CoreAudioData::CoreAudioData(AudioOutputCA *parent) : mCA(parent)
{
    UInt32        paramSize;
    OSStatus      err;


    // Initialise private data
    mOutputUnit   = NULL;
    mDeviceID     = 0;
    mDigitalInUse = false;
    mRevertFormat = false;
    mStreamIndex  = -1;


    // Reset all the devices to a default 'non-hog' and mixable format.
    // If we don't do this we may be unable to find the Default Output device.
    // (e.g. if we crashed last time leaving it stuck in AC-3 mode)
    ResetAudioDevices();

    // Find the ID of the default Device
    paramSize = sizeof(mDeviceID);
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                   &paramSize, &mDeviceID);
    if (err == noErr)
        Debug(QString("CoreAudioData - default device ID = %1").arg(mDeviceID));
    else
    {
        Warn(QString("CoreAudioData - could not get default audio device: %1")
             .arg(err));
        mDeviceID = 0;
    }
}

pid_t CoreAudioData::GetHogPID()
{
    OSStatus  err;
    pid_t     PID;
    UInt32    PIDsize = sizeof(PID);

    err = AudioDeviceGetProperty(mDeviceID, 0, FALSE,
                                 kAudioDevicePropertyHogMode, &PIDsize, &PID);
    if (err != noErr)
    {
        // This is not a fatal error.
        // Some drivers simply don't support this property
        Debug(QString("GetHogPID - unable to check: %1").arg(err));
        return -1;
    }

    return PID;
}

bool CoreAudioData::SetHogPID(pid_t PID)
{
    OSStatus  err;


    err = AudioDeviceSetProperty(mDeviceID, 0, 0, FALSE,
                                 kAudioDevicePropertyHogMode,
                                 sizeof(PID), &PID);
    if (err != noErr)
    {
        Error(QString("SetHogPID(%1) failed: %2").arg(PID).arg(err));
        return false;
    }

    return true;
}

bool CoreAudioData::SetHogMode()
{
    pid_t  myPID  = getpid();
    pid_t  hogPID = GetHogPID();

    if (hogPID != -1 && hogPID != myPID)
    {
        Warn(QString("SetHogMode - Selected audio device is exclusively in use by another program (PID %1)").arg(hogPID));
        return false;
    }

    return SetHogPID(myPID);
}

/**
 * Set mixable to false if we are allowed to
 */
bool CoreAudioData::SetUnMixable()
{
    UInt32         b_mix = 0;
    Boolean        b_writeable = false;
    OSStatus       err;
    UInt32         paramSize;

    paramSize = sizeof(b_writeable);
    err = AudioDeviceGetPropertyInfo(mDeviceID, 0, FALSE,
                                     kAudioDevicePropertySupportsMixing,
                                     &paramSize, &b_writeable);

    if (err != noErr)
    {
        Warn(QString("SetUnMixable - Can't GetPropertyInfo?").arg(err));
        return false;
    }

    paramSize = sizeof(b_mix);
    err = AudioDeviceGetProperty(mDeviceID, 0, FALSE,
                                 kAudioDevicePropertySupportsMixing,
                                 &paramSize, &b_mix);
    if (err == noErr && b_writeable)
    {
        b_mix = 0;
        err = AudioDeviceSetProperty(mDeviceID, 0, 0, FALSE,
                                     kAudioDevicePropertySupportsMixing,
                                     paramSize, &b_mix);
        mChangedMixing = true;
    }

    if (err != noErr)
    {
        Warn(QString("SetUnMixable - failed to set mixmode: %1").arg(err));
        ReleaseHogMode();
        return false;
    }

    return true;
}

/**
 * Get a list of all the streams on this device
 */
AudioStreamID *CoreAudioData::StreamsList(AudioDeviceID d)
{
    OSStatus       err;
    UInt32         listSize;
    AudioStreamID  *list;


    err = AudioDeviceGetPropertyInfo(d, 0, FALSE,
                                     kAudioDevicePropertyStreams,
                                     &listSize, NULL);
    if (err != noErr)
    {
        Error(QString("StreamsList() - could not get list size: %1").arg(err));
        return NULL;
    }

    // Space for a terminating ID:
    listSize += sizeof(AudioStreamID);
    list      = (AudioStreamID *)malloc(listSize);

    if (list == NULL)
    {
        Error("StreamsList() - out of memory?");
        return NULL;
    }

    err = AudioDeviceGetProperty(d, 0, FALSE,
                                 kAudioDevicePropertyStreams,
                                 &listSize, list);
    if (err != noErr)
    {
        Error(QString("StreamsList() - could not get list: %1").arg(err));
        return NULL;
    }

    // Add a terminating ID:
    list[listSize/sizeof(AudioStreamID)] = kAudioHardwareBadStreamError;

    return list;
}

AudioStreamBasicDescription *CoreAudioData::FormatsList(AudioStreamID s)
{
    OSStatus                     err;
    AudioStreamBasicDescription  *list;
    UInt32                       listSize;
    AudioDevicePropertyID        p;


    // This is deprecated for kAudioStreamPropertyAvailablePhysicalFormats,
    // but compiling on 10.3 requires the older constant
    p = kAudioStreamPropertyPhysicalFormats;

    // Retrieve all the stream formats supported by this output stream
    err = AudioStreamGetPropertyInfo(s, 0, p, &listSize, NULL);
    if (err != noErr)
    {
        Warn(QString("FormatsList() couldn't get list size: %1").arg(err));
        return NULL;
    }

    // Space for a terminating ID:
    listSize += sizeof(AudioStreamBasicDescription);
    list      = (AudioStreamBasicDescription *)malloc(listSize);

    if (list == NULL)
    {
        Error("FormatsList() - out of memory?");
        return NULL;
    }

    err = AudioStreamGetProperty(s, 0, p, &listSize, list);
    if (err != noErr)
    {
        Warn(QString("FormatsList() couldn't get list: %1").arg(err));
        free(list);
        return NULL;
    }

    // Add a terminating ID:
    list[listSize/sizeof(AudioStreamID)].mFormatID = 0;

    return list;
}

static UInt32   sNumberCommonSampleRates = 15;
static Float64  sCommonSampleRates[] = {
    8000.0,   11025.0,  12000.0,
    16000.0,  22050.0,  24000.0,
    32000.0,  44100.0,  48000.0,
    64000.0,  88200.0,  96000.0,
    128000.0, 176400.0, 192000.0 };

static bool		IsRateCommon(Float64 inRate)
{
	bool theAnswer = false;
	for(UInt32 i = 0; !theAnswer && (i < sNumberCommonSampleRates); i++)
	{
		theAnswer = inRate == sCommonSampleRates[i];
	}
	return theAnswer;
}

int *CoreAudioData::RatesList(AudioDeviceID d)
{
    OSStatus                    err;
    AudioValueRange             *list;
    int                         *finallist;
    UInt32                      listSize;
    UInt32                      nbitems = 0;

    // retrieve size of rate list
    err = AudioDeviceGetPropertyInfo(d, 0, 0,
                                     kAudioDevicePropertyAvailableNominalSampleRates,
                                     &listSize, NULL);
    if (err != noErr)
    {
        Warn(QString("RatesList() couldn't get data rate list size: %1").arg(err));
        return NULL;
    }

    list      = (AudioValueRange *)malloc(listSize);
    if (list == NULL)
    {
        Error("RatesList() - out of memory?");
        return NULL;
    }

	err = AudioDeviceGetProperty(d, 0, 0,
                                 kAudioDevicePropertyAvailableNominalSampleRates,
                                 &listSize, list);
    if (err != noErr)
    {
        Warn(QString("RatesList() couldn't get list: %1").arg(err));
        free(list);
        return NULL;
    }

    finallist = (int *)malloc(((listSize / sizeof(AudioValueRange)) + 1)
                              * sizeof(int));
    if (finallist == NULL)
    {
        Error("RatesList() - out of memory?");
        return NULL;
    }

    // iterate through the ranges and add the minimum, maximum, and common rates in between
    UInt32 theFirstIndex = 0, theLastIndex = 0;
    for(UInt32 i = 0; i < listSize / sizeof(AudioValueRange); i++)
    {
        theFirstIndex = theLastIndex;
        // find the index of the first common rate greater than or equal to the minimum
        while((theFirstIndex < sNumberCommonSampleRates) &&  (sCommonSampleRates[theFirstIndex] < list[i].mMinimum))
            theFirstIndex++;

        if (theFirstIndex >= sNumberCommonSampleRates)
            break;

        theLastIndex = theFirstIndex;
        // find the index of the first common rate greater than or equal to the maximum
        while((theLastIndex < sNumberCommonSampleRates) && (sCommonSampleRates[theLastIndex] < list[i].mMaximum))
        {
            finallist[nbitems++] = sCommonSampleRates[theLastIndex];
            theLastIndex++;
        }
        if (IsRateCommon(list[i].mMinimum))
            finallist[nbitems++] = list[i].mMinimum;
        else if (IsRateCommon(list[i].mMaximum))
            finallist[nbitems++] = list[i].mMaximum;
    }
    free(list);

    // Add a terminating ID
    finallist[nbitems] = -1;

    return finallist;
}

bool *CoreAudioData::ChannelsList(AudioDeviceID d, bool passthru)
{
    AudioStreamID               *streams;
    AudioStreamBasicDescription *formats;
    bool                        founddigital = false;
    bool                        *list;

    if ((list = (bool *)malloc((CHANNELS_MAX+1) * sizeof(bool))) == NULL)
        return NULL;

    bzero(list, (CHANNELS_MAX+1) * sizeof(bool));

    streams = StreamsList(mDeviceID);
    if (!streams)
    {
        free(list);
        return NULL;
    }

    if (passthru)
    {
        for (int i = 0; streams[i] != kAudioHardwareBadStreamError; i++)
        {
            formats = FormatsList(streams[i]);
            if (!formats)
                continue;

            // Find a stream with a cac3 stream
            for (int j = 0; formats[j].mFormatID != 0; j++)
            {
                if (formats[j].mFormatID == 'IAC3' ||
                    formats[j].mFormatID == kAudioFormat60958AC3)
                {
                    list[formats[j].mChannelsPerFrame] = true;
                    founddigital = true;
                }
            }
            free(formats);
        }        
    }

    if (!founddigital)
    {
        for (int i = 0; streams[i] != kAudioHardwareBadStreamError; i++)
        {
            formats = FormatsList(streams[i]);
            if (!formats)
                continue;
            for (int j = 0; formats[j].mFormatID != 0; j++)
                if (formats[j].mChannelsPerFrame <= CHANNELS_MAX)
                    list[formats[j].mChannelsPerFrame] = true;
            free(formats);
        }
    }
    return list;
}

bool CoreAudioData::OpenSPDIF()
{
    OSStatus       err;
    AudioStreamID  *streams;
    UInt32         paramSize = 0;

    // Hog the device
    if (!SetHogMode())
        return false;

    // Set mixable to false if we are allowed to
    if (!SetUnMixable())
        return false;

    streams = StreamsList(mDeviceID);
    if (!streams)
        return false;

    for (int i = 0; mStreamIndex < 0 &&
                    streams[i] != kAudioHardwareBadStreamError; ++i)
    {
        AudioStreamBasicDescription *formats = FormatsList(streams[i]);
        if (!formats)
            continue;

        // Find a stream with a cac3 stream
        for (int j = 0; formats[j].mFormatID != 0; j++)
        {
            if (formats[j].mFormatID == 'IAC3' ||
                formats[j].mFormatID == kAudioFormat60958AC3)
            {
                Debug("OpenSPDIF - found digital format");
                mDigitalInUse = true;
                break;
            }
        }

        if (!mDigitalInUse)
        {
            // No AC3 format streams - try and fallback to analog
            ReleaseHogMode();
            return false;
        }

        if (mDigitalInUse)
        {
            // if this stream supports a digital (cac3) format, then go set it.
            int requestedFmt = -1;
            int currentFmt   = -1;
            int backupFmt    = -1;

            mStreamID = streams[i];
            mStreamIndex = i;

            if (mRevertFormat == false)
            {
                // Retrieve the original format of this stream first
                // if not done so already
                paramSize = sizeof(mFormatOrig);
                err = AudioStreamGetProperty(mStreamID, 0,
                                             kAudioStreamPropertyPhysicalFormat,
                                             &paramSize, &mFormatOrig);
                if (err != noErr)
                {
                    Warn(QString("OpenSPDIF - could not retrieve the original streamformat: %1").arg(err));
                    continue;
                }
                mRevertFormat = true;
            }

            for (int j = 0; formats[j].mFormatID != 0; j++)
            {
                if (formats[j].mFormatID == 'IAC3' ||
                    formats[j].mFormatID == kAudioFormat60958AC3)
                {
                    if (formats[j].mSampleRate == mCA->samplerate)
                    {
                        requestedFmt = j;
                        break;
                    }
                    else if (formats[j].mSampleRate == mFormatOrig.mSampleRate)
                        currentFmt = j;
                    else if (backupFmt < 0 || formats[j].mSampleRate >
                                              formats[backupFmt].mSampleRate)
                        backupFmt = j;
                }
            }

            // We prefer to output at the samplerate of the original audio
            if (requestedFmt >= 0)
                mFormatNew = formats[requestedFmt];

            // If not possible, try to use the current samplerate of the device
            else if (currentFmt >= 0)
                mFormatNew = formats[currentFmt];

            // And if we have to, any digital format
            // will be just fine (highest rate possible)
            else
                mFormatNew = formats[backupFmt];
        }
        free(formats);
    }
    free(streams);

    if (!AudioStreamChangeFormat(mStreamID, mFormatNew))
    {
        ReleaseHogMode();
        return false;
    }

    // Add IOProc callback
    err = AudioDeviceAddIOProc(mDeviceID,
                               (AudioDeviceIOProc)RenderCallbackSPDIF,
                               (void *)this);
    if (err != noErr)
    {
        Warn(QString("OpenSPDIF - AudioDeviceAddIOProc failed: %1").arg(err));
        ReleaseHogMode();
        return false;
    }

    // Start device
    err = AudioDeviceStart(mDeviceID, (AudioDeviceIOProc)RenderCallbackSPDIF);
    if (err != noErr)
    {
        Warn(QString("OpenSPDIF - AudioDeviceStart failed: %1").arg(err));

        err = AudioDeviceRemoveIOProc(mDeviceID,
                                      (AudioDeviceIOProc)RenderCallbackSPDIF);
        if (err != noErr)
            Warn(QString("OpenSPDIF - AudioDeviceRemoveIOProc failed: %1")
                 .arg(err));
        ReleaseHogMode();
        return false;
    }

    return true;
}

void CoreAudioData::CloseSPDIF()
{
    OSStatus  err;
    UInt32    paramSize;

    // Stop device
    err = AudioDeviceStop(mDeviceID, (AudioDeviceIOProc)RenderCallbackSPDIF);
    if (err != noErr)
        Debug(QString("CloseSPDIF - AudioDeviceStop failed: %1").arg(err));

    // Remove IOProc callback
    err = AudioDeviceRemoveIOProc(mDeviceID,
                                  (AudioDeviceIOProc)RenderCallbackSPDIF);
    if (err != noErr)
        Debug(QString("CloseSPDIF - AudioDeviceRemoveIOProc failed: %1")
              .arg(err));

    if (mRevertFormat)
        AudioStreamChangeFormat(mStreamID, mFormatOrig);

    if (mChangedMixing && mFormatOrig.mFormatID != kAudioFormat60958AC3)
    {
        int      b_mix;
        Boolean  b_writeable;

        // Revert mixable to true if we are allowed to
        err = AudioDeviceGetPropertyInfo(mDeviceID, 0, FALSE,
                                         kAudioDevicePropertySupportsMixing,
                                         &paramSize, &b_writeable);

        err = AudioDeviceGetProperty(mDeviceID, 0, FALSE,
                                     kAudioDevicePropertySupportsMixing,
                                     &paramSize, &b_mix);

        if (!err && b_writeable)
        {
            Debug(QString("CloseSPDIF - mixable is: %1").arg(b_mix));
            b_mix = 1;
            err = AudioDeviceSetProperty(mDeviceID, 0, 0, FALSE,
                                         kAudioDevicePropertySupportsMixing,
                                         paramSize, &b_mix);
        }

        if (err != noErr)
            Debug(QString("CloseSPDIF - failed to set mixmode: %d").arg(err));
    }

    if (GetHogPID() == getpid())
        ReleaseHogMode();
}

int CoreAudioData::AudioStreamChangeFormat(AudioStreamID               s,
                                           AudioStreamBasicDescription format)
{
    Debug(QString("AudioStreamChangeFormat(%1)").arg(s));

    OSStatus err = AudioStreamSetProperty(s, 0, 0,
                                          kAudioStreamPropertyPhysicalFormat,
                                          sizeof(format), &format);
    if (err != noErr)
    {
        Error(QString("AudioStreamChangeFormat couldn't set stream format: %1")
              .arg(err));
        return false;
    }

    return true;
}

bool CoreAudioData::FindAC3Stream()
{
    bool           foundAC3Stream = false;
    AudioStreamID  *streams;


    // Get a list of all the streams on this device
    streams = StreamsList(mDeviceID);
    if (!streams)
        return false;

    for (int i = 0; !foundAC3Stream &&
                    streams[i] != kAudioHardwareBadStreamError; ++i)
    {
        AudioStreamBasicDescription *formats = FormatsList(streams[i]);
        if (!formats)
            continue;

        // Find a stream with a cac3 stream
        for (int j = 0; formats[j].mFormatID != 0; j++)
            if (formats[j].mFormatID == 'IAC3' ||
                formats[j].mFormatID == kAudioFormat60958AC3)
            {
                Debug("FindAC3Stream - found digital format");
                foundAC3Stream = true;
                break;
            }

        free(formats);
    }
    free(streams);

    return foundAC3Stream;
}

/**
 * Reset any devices with an AC3 stream back to a Linear PCM
 * so that they can become a default output device
 */
void CoreAudioData::ResetAudioDevices()
{
    AudioDeviceID  *devices;
    int            numDevices;
    UInt32         size;


    AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
    devices    = (AudioDeviceID*)malloc(size);
    if (!devices)
    {
        Error("ResetAudioDevices - out of memory?");
        return;
    }
    numDevices = size / sizeof(AudioDeviceID);
    AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, devices);

    for (int i = 0; i < numDevices; i++)
    {
        AudioStreamID  *streams;

        streams = StreamsList(devices[i]);
        for (int j = 0; streams[j] != kAudioHardwareBadStreamError; j++)
            ResetStream(streams[j]);

        free(streams);
    }
    free(devices);
}

void CoreAudioData::ResetStream(AudioStreamID s)
{
    AudioStreamBasicDescription  currentFormat;
    OSStatus                     err;
    UInt32                       paramSize;


    // Find the streams current physical format
    paramSize = sizeof(currentFormat);
    AudioStreamGetProperty(s, 0, kAudioStreamPropertyPhysicalFormat,
                           &paramSize, &currentFormat);

    // If it's currently AC-3/SPDIF then reset it to some mixable format
    if (currentFormat.mFormatID == 'IAC3' ||
        currentFormat.mFormatID == kAudioFormat60958AC3)
    {
        AudioStreamBasicDescription *formats    = FormatsList(s);
        bool                        streamReset = false;


        if (!formats)
            return;

        for (int i = 0; !streamReset && formats[i].mFormatID != 0; i++)
            if (formats[i].mFormatID == kAudioFormatLinearPCM)
            {
                err = AudioStreamSetProperty(s, NULL, 0, kAudioStreamPropertyPhysicalFormat, sizeof(formats[i]), &(formats[i]));
                if (err != noErr)
                {
                    Warn(QString("ResetStream - could not set physical format: %1").arg(err));
                    continue;
                }
                else
                {
                    streamReset = true;
                    sleep(1);   // For the change to take effect
                }
            }

        free(formats);
    }
}
