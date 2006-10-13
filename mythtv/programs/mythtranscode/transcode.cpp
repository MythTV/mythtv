#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <sys/time.h>
#include <ctime>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qmap.h>

#include <iostream>
using namespace std;

#include "osdtypes.h"
#include "transcode.h"
#include "audiooutput.h"
#include "recordingprofile.h"
#include "remoteutil.h"
#include "mythcontext.h"
#include "jobqueue.h"

extern "C" {
#include "../libavcodec/avcodec.h"
}

// This class is to act as a fake audio output device to store the data
// for reencoding.

class AudioReencodeBuffer : public AudioOutput
{
 public:
    AudioReencodeBuffer(int audio_bits, int audio_channels)
    {
        Reset();
        Reconfigure(audio_bits, audio_channels, 0, 0);
        bufsize = 512000;
        audiobuffer = new unsigned char[bufsize];
    }

   ~AudioReencodeBuffer()
    {
        delete [] audiobuffer;
    }

    // reconfigure sound out for new params
    virtual void Reconfigure(int audio_bits, int audio_channels,
                             int audio_samplerate, bool audio_passthru)
    {
        (void)audio_samplerate;
        (void)audio_passthru;
        bits = audio_bits;
        channels = audio_channels;
        bytes_per_sample = bits * channels / 8;
    }

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate)
    {
        eff_audiorate = (dsprate / 100);
    }

    virtual void SetBlocking(bool block) { (void)block; }
    virtual void Reset(void)
    {
        audiobuffer_len = 0;
    }

    // timecode is in milliseconds.
    virtual bool AddSamples(char *buffer, int samples, long long timecode)
    {
        int freebuf = bufsize - audiobuffer_len;

        if (samples * bytes_per_sample > freebuf)
        {
            bufsize += samples * bytes_per_sample - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        memcpy(audiobuffer + audiobuffer_len, buffer, 
               samples * bytes_per_sample);
        audiobuffer_len += samples * bytes_per_sample;
        // last_audiotime is at the end of the sample
        last_audiotime = timecode + samples * 1000 / eff_audiorate;

        return true;
    }

    virtual bool AddSamples(char *buffers[], int samples, long long timecode)
    {
        int audio_bytes = bits / 8;
        int freebuf = bufsize - audiobuffer_len;

        if (samples * bytes_per_sample > freebuf)
        {
            bufsize += samples * bytes_per_sample - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        for (int itemp = 0; itemp < samples*audio_bytes; itemp+=audio_bytes)
        {
            for(int chan = 0; chan < channels; chan++)
            {
                audiobuffer[audiobuffer_len++] = buffers[chan][itemp];
                if (bits == 16)
                    audiobuffer[audiobuffer_len++] = buffers[chan][itemp+1];
            }
        }

        // last_audiotime is at the end of the sample
        last_audiotime = timecode + samples * 1000 / eff_audiorate;

        return true;
    }

    virtual void SetTimecode(long long timecode)
    {
        last_audiotime = timecode;
    }
    virtual bool GetPause(void)
    {
        return false;
    }
    virtual void Pause(bool paused)
    {
        (void)paused;
    }
    virtual void Drain(void)
    {
        // Do nothing
        return;
    }

    virtual int GetAudiotime(void)
    {
        return last_audiotime;
    }

    virtual int GetVolumeChannel(int)
    { 
        // Do nothing
        return 100;
    }
    virtual void SetVolumeChannel(int, int) 
    {
        // Do nothing
    }
    virtual void SetVolumeAll(int)
    {
        // Do nothing
    }
    

    virtual int GetCurrentVolume(void)
    { 
        // Do nothing
        return 100;
    }
    virtual void SetCurrentVolume(int) 
    {
        // Do nothing
    }
    virtual void AdjustCurrentVolume(int) 
    {
        // Do nothing
    }
    virtual void SetMute(bool) 
    {
        // Do nothing
    }
    virtual void ToggleMute(void) 
    {
        // Do nothing
    }
    virtual kMuteState GetMute(void) 
    {
        // Do nothing
        return MUTE_OFF;
    }
    virtual kMuteState IterateMutedChannels(void) 
    {
        // Do nothing
        return MUTE_OFF;
    }

    //  These are pure virtual in AudioOutput, but we don't need them here
    virtual void bufferOutputData(bool){ return; }
    virtual int readOutputData(unsigned char*, int ){ return 0; }

    int bufsize;
    unsigned char *audiobuffer;
    int audiobuffer_len, channels, bits, bytes_per_sample, eff_audiorate;
    long long last_audiotime;
};

Transcode::Transcode(ProgramInfo *pginfo)
{
    m_proginfo = pginfo;
    nvr = NULL;
    nvp = NULL;
    inRingBuffer = NULL;
    outRingBuffer = NULL;
    fifow = NULL;
    kfa_table = NULL;
    showprogress = false;
}
Transcode::~Transcode()
{
    if (nvr)
        delete nvr;
    if (nvp)
        delete nvp;
    if (inRingBuffer)
        delete inRingBuffer;
    if (outRingBuffer)
        delete outRingBuffer;
    if (fifow)
        delete fifow;
    if (kfa_table)
    {
        while(! kfa_table->isEmpty())
        {
           delete kfa_table->last();
           kfa_table->removeLast();
        }
        delete kfa_table;
    }
}
void Transcode::ReencoderAddKFA(long curframe, long lastkey, long num_keyframes)
{
    long delta = curframe - lastkey;
    if (delta != 0 && delta != keyframedist)
    {
        struct kfatable_entry *kfate = new struct kfatable_entry;
        kfate->adjust = keyframedist - delta;
        kfate->keyframe_number = num_keyframes;
        kfa_table->append(kfate);
    }
}

bool Transcode::GetProfile(QString profileName, QString encodingType)
{
    if (profileName.lower() == "autodetect")
    {
        bool result = false;
        if (encodingType == "MPEG-2")
            result = profile.loadByGroup("MPEG2", "Transcoders");
        if (encodingType == "MPEG-4" || encodingType == "RTjpeg")
            result = profile.loadByGroup("RTjpeg/MPEG4",
                                         "Transcoders");
        if (! result)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Transcode: Couldn't find profile for : %1").
                    arg(encodingType));

            return false;
        }
    }
    else
    {
        bool isNum;
        int profileID;
        profileID = profileName.toInt(&isNum);
        // If a bad profile is specified, there will be trouble
        if (isNum && profileID > 0)
            profile.loadByID(profileID);
        else
        {
            VERBOSE(VB_IMPORTANT, QString("Couldn't find profile #: %1").
                    arg(profileName));
            return false;
        }
    }
    return true;
}

#define SetProfileOption(profile, name) { \
    int value = profile.byName(name)->getValue().toInt(); \
    nvr->SetOption(name, value); \
}

void TranscodeWriteText(void *ptr, unsigned char *buf, int len, int timecode, int pagenr)
{
  NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)ptr;
  nvr->WriteText(buf, len, timecode, pagenr);
}

int Transcode::TranscodeFile(char *inputname, char *outputname,
                              QString profileName,
                              bool honorCutList, bool framecontrol,
                              int jobID, QString fifodir)
{ 
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime statustime = curtime;
    int audioframesize;
    int audioFrame = 0;

    if (jobID >= 0)
        JobQueue::ChangeJobComment(jobID, "0% " + QObject::tr("Completed"));

    nvp = new NuppelVideoPlayer("transcoder", m_proginfo);
    nvp->SetNullVideo();

    if (showprogress)
    {
        statustime = statustime.addSecs(5);
    }
    // Input setup
    nvr = new NuppelVideoRecorder(NULL, NULL);
    inRingBuffer = new RingBuffer(inputname, false, false);
    nvp->SetRingBuffer(inRingBuffer);

    AudioOutput *audioOutput = new AudioReencodeBuffer(0, 0);
    AudioReencodeBuffer *arb = ((AudioReencodeBuffer*)audioOutput);
    nvp->SetAudioOutput(audioOutput);
    nvp->SetTranscoding(true);

    if (nvp->OpenFile(false) < 0)
    {
        VERBOSE(VB_IMPORTANT, "Transcoding aborted, error opening file.");
        return REENCODE_ERROR;
    }

    long long total_frame_count = nvp->GetTotalFrameCount();
    long long new_frame_count = total_frame_count;
    if (honorCutList && m_proginfo)
    {
        VERBOSE(VB_GENERAL, "Honoring the cutlist while transcoding");

        QMap<long long, int> delMap;
        QMap<long long, int>::Iterator it;
        QString cutStr = "";
        long long lastStart = 0;

        m_proginfo->GetCutList(delMap);

        for (it = delMap.begin(); it != delMap.end(); ++it)
        {
            if (it.data())
            {
                if (cutStr != "")
                    cutStr += ",";
                cutStr += QString("%1-").arg((long)it.key());
                lastStart = it.key();
            }
            else
            {
                cutStr += QString("%1").arg((long)it.key());
                new_frame_count -= (it.key() - lastStart);
            }
        }
        if (cutStr == "")
            cutStr = "Is Empty";
        VERBOSE(VB_GENERAL, QString("Cutlist        : %1").arg(cutStr));
        VERBOSE(VB_GENERAL, QString("Original Length: %1 frames")
                                    .arg((long)total_frame_count));
        VERBOSE(VB_GENERAL, QString("New Length     : %1 frames")
                                    .arg((long)new_frame_count));

        if ((m_proginfo->IsEditing()) ||
            (JobQueue::IsJobRunning(JOB_COMMFLAG, m_proginfo)))
        {
            VERBOSE(VB_IMPORTANT, "Transcoding aborted, cutlist changed");
            return REENCODE_CUTLIST_CHANGE;
        }
        m_proginfo->SetMarkupFlag(MARK_UPDATED_CUT, false);
        curtime = curtime.addSecs(60);
    }

    nvp->ReinitAudio();
    QString encodingType = nvp->GetEncodingType();
    bool copyvideo = false, copyaudio = false;

    QString vidsetting = NULL, audsetting = NULL, vidfilters = NULL;

    int video_width = nvp->GetVideoWidth();
    int video_height = nvp->GetVideoHeight();
     
    if (video_height == 1088) {
       VERBOSE(VB_IMPORTANT, "Found video height of 1088.  This is unusual and "
               "more than likely the video is actually 1080 so mythtranscode "
               "will treat it as such.");
    }

    float video_aspect = nvp->GetVideoAspect();
    float video_frame_rate = nvp->GetFrameRate();
    int newWidth = video_width;
    int newHeight = video_height;

    kfa_table = new QPtrList<struct kfatable_entry>;

    if (fifodir == NULL)
    {
        if (!GetProfile(profileName, encodingType)) {
            VERBOSE(VB_IMPORTANT, "Transcoding aborted, no profile found.");
            return REENCODE_ERROR;
        }
        vidsetting = profile.byName("videocodec")->getValue();
        audsetting = profile.byName("audiocodec")->getValue();
        vidfilters = profile.byName("transcodefilters")->getValue();

        if (encodingType == "MPEG-2" &&
            profile.byName("transcodelossless")->getValue().toInt())
        {
            VERBOSE(VB_IMPORTANT, "Switching to MPEG-2 transcoder.");
            return REENCODE_MPEG2TRANS;
        }

        // Recorder setup
        if (profile.byName("transcodelossless")->getValue().toInt())
        {
            vidsetting = encodingType;
            audsetting = "MP3";
        }
        else if (profile.byName("transcoderesize")->getValue().toInt())
        {
            int actualHeight = (video_height == 1088 ? 1080 : video_height);

            nvp->SetVideoFilters(vidfilters);
            newWidth = profile.byName("width")->getValue().toInt();
            newHeight = profile.byName("height")->getValue().toInt();

            // If height or width are 0, then we need to calculate them
            if (newHeight == 0 && newWidth > 0)
                newHeight = (int)(1.0 * newWidth * actualHeight / video_width);
            else if (newWidth == 0 && newHeight > 0)
                newWidth = (int)(1.0 * newHeight * video_width / actualHeight);
            else if (newWidth == 0 && newHeight == 0)
            {
                newHeight = 480;
                newWidth = (int)(1.0 * 480 * video_width / actualHeight);
                if (newWidth > 640)
                {
                    newWidth = 640;
                    newHeight = (int)(1.0 * 640 * actualHeight / video_width);
                }
            }

            if (encodingType.left(4).lower() == "mpeg")
            {
                // make sure dimensions are valid for MPEG codecs
                newHeight = (newHeight + 15) & ~0xF;
                newWidth  = (newWidth  + 15) & ~0xF;
            }

            VERBOSE(VB_IMPORTANT, QString("Resizing from %1x%2 to %3x%4")
                    .arg(video_width).arg(video_height)
                    .arg(newWidth).arg(newHeight));
        }
        else  // lossy and no resize
            nvp->SetVideoFilters(vidfilters);

        // this is ripped from tv_rec SetupRecording. It'd be nice to merge
        nvr->SetOption("inpixfmt", FMT_YV12);

        nvr->SetOption("width", newWidth);
        nvr->SetOption("height", newHeight);

        nvr->SetOption("tvformat", gContext->GetSetting("TVFormat"));
        nvr->SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

        if (vidsetting == "MPEG-4")
        {
            nvr->SetOption("codec", "mpeg4");

            SetProfileOption(profile, "mpeg4bitrate");
            SetProfileOption(profile, "mpeg4scalebitrate");
            SetProfileOption(profile, "mpeg4maxquality");
            SetProfileOption(profile, "mpeg4minquality");
            SetProfileOption(profile, "mpeg4qualdiff");
            SetProfileOption(profile, "mpeg4optionvhq");
            SetProfileOption(profile, "mpeg4option4mv");
            nvr->SetupAVCodec();
        }
        else if (vidsetting == "RTjpeg")
        {
            nvr->SetOption("codec", "rtjpeg");
            SetProfileOption(profile, "rtjpegquality");
            SetProfileOption(profile, "rtjpegchromafilter");
            SetProfileOption(profile, "rtjpeglumafilter");
            nvr->SetupRTjpeg();
        }
        else if (vidsetting == "")
        {
            VERBOSE(VB_IMPORTANT, "No video information found!");
            VERBOSE(VB_IMPORTANT, "Please ensure that recording profiles "
                                  "for the transcoder are set");
            return REENCODE_ERROR;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("Unknown video codec: %1").arg(vidsetting));
            return REENCODE_ERROR;
        }

        nvr->SetOption("samplerate", arb->eff_audiorate);
        if (audsetting == "MP3")
        {
            nvr->SetOption("audiocompression", 1);
            SetProfileOption(profile, "mp3quality");
            copyaudio = true;
        }
        else if (audsetting == "Uncompressed")
        {
            nvr->SetOption("audiocompression", 0);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("Unknown audio codec: %1").arg(audsetting));
        }

        nvr->AudioInit(true);

        nvr->SetFrameRate(video_frame_rate);
        nvr->SetVideoAspect(video_aspect);
        nvr->SetTranscoding(true);

        outRingBuffer = new RingBuffer(outputname, true, false);
        nvr->SetRingBuffer(outRingBuffer);
        nvr->WriteHeader();
        nvr->StreamAllocate();
    }

    if (vidsetting == encodingType && !framecontrol &&
        fifodir == NULL && honorCutList &&
        video_width == newWidth && video_height == newHeight)
    {
        copyvideo = true;
        VERBOSE(VB_GENERAL, "Reencoding video in 'raw' mode");
    }

    keyframedist = 30;
    nvp->InitForTranscode(copyaudio, copyvideo);
    if (nvp->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "Unable to initialize NuppelVideoPlayer for Transcode");
        delete nvp;
        return REENCODE_ERROR;
    }

    int vidSize = 0;

    // 1920x1080 video is actually 1920x1088 because of the 16x16 blocks so
    // we have to fudge the output size here.  nuvexport knows how to handle
    // this and as of right now it is the only app that uses the fifo ability.
    if (video_height == 1080 && video_width == 1920)
        vidSize = (1088 * 1920) * 3 / 2;
    else
        vidSize = (video_height * video_width) * 3 / 2;

    VideoFrame frame;
    frame.codec = FMT_YV12;
    frame.width = newWidth;
    frame.height = newHeight;
    frame.size = newWidth * newHeight * 3 / 2;

    if (fifodir != NULL)
    {
        QString audfifo = fifodir + QString("/audout");
        QString vidfifo = fifodir + QString("/vidout");
        int audio_size = arb->eff_audiorate * arb->bytes_per_sample;
        // framecontrol is true if we want to enforce fifo sync.
        if (framecontrol)
            VERBOSE(VB_GENERAL, "Enforcing sync on fifos");
        fifow = new FIFOWriter::FIFOWriter(2, framecontrol);

        if (!fifow->FIFOInit(0, QString("video"), vidfifo, vidSize, 50) ||
            !fifow->FIFOInit(1, QString("audio"), audfifo, audio_size, 25))
        {
            VERBOSE(VB_IMPORTANT, "Error initializing fifo writer.  Aborting");
            unlink(outputname);
            return REENCODE_ERROR;
        }
        VERBOSE(VB_GENERAL, QString("Video %1x%2@%3fps Audio rate: %4")
                                   .arg(video_width).arg(video_height)
                                   .arg(video_frame_rate)
                                   .arg(arb->eff_audiorate));
        VERBOSE(VB_GENERAL, "Created fifos. Waiting for connection.");
    }

    bool forceKeyFrames = (fifow == NULL) ? framecontrol : false;

    QMap<long long, int>::Iterator dm_iter = NULL;
    bool writekeyframe = true;
   
    int num_keyframes = 0;

    int did_ff = 0; 

    long curFrameNum = 0;
    frame.frameNumber = 1;
    long lastKeyFrame = 0;
    long totalAudio = 0;
    int dropvideo = 0;
    long long lasttimecode = 0;
    long long timecodeOffset = 0;

    float rateTimeConv = arb->eff_audiorate * arb->bytes_per_sample / 1000.0;
    float vidFrameTime = 1000.0 / video_frame_rate;
    int wait_recover = 0;
    VideoOutput *videoOutput = nvp->getVideoOutput();
    bool is_key = 0;
    bool first_loop = true;
    unsigned char *newFrame = new unsigned char[newWidth * newHeight * 3 / 2];

    frame.buf = newFrame;
    AVPicture imageIn, imageOut;
    ImgReSampleContext *scontext;

    if (fifow)
        VERBOSE(VB_GENERAL, "Dumping Video and Audio data to fifos");
    else if (copyaudio)
        VERBOSE(VB_GENERAL, "Copying Audio while transcoding Video");
    else
        VERBOSE(VB_GENERAL, "Transcoding Video and Audio");

    QTime flagTime;
    flagTime.start();

    while (nvp->TranscodeGetNextFrame(dm_iter, &did_ff, &is_key, honorCutList))
    {
        if (first_loop)
        {
            copyaudio = nvp->GetRawAudioState();
            first_loop = false;
        }
        VideoFrame *lastDecode = videoOutput->GetLastDecodedFrame();

        frame.timecode = lastDecode->timecode;

        if (frame.timecode < lasttimecode)
            frame.timecode = (long long)(lasttimecode + vidFrameTime);

        if (fifow)
        {
            frame.buf = lastDecode->buf;
            totalAudio += arb->audiobuffer_len;
            int audbufTime = (int)(totalAudio / rateTimeConv);
            int auddelta = arb->last_audiotime - audbufTime;
            int vidTime = (int)(curFrameNum * vidFrameTime + 0.5);
            int viddelta = frame.timecode - vidTime;
            int delta = viddelta - auddelta;
            if (abs(delta) < 500 && abs(delta) >= vidFrameTime)
            {
               QString msg = QString("Audio is %1ms %2 video at # %3")
                          .arg(abs(delta))
                          .arg(((delta > 0) ? "ahead of" : "behind"))
                          .arg((int)curFrameNum);
                VERBOSE(VB_GENERAL, msg);
                dropvideo = (delta > 0) ? 1 : -1;
                wait_recover = 0;
            }
            else if (delta >= 500 && delta < 10000)
            {
                if (wait_recover == 0)
                {
                    dropvideo = 5;
                    wait_recover = 6;
                }
                else if (wait_recover == 1)
                {
                    // Video is badly lagging.  Try to catch up.
                    int count = 0;
                    while (delta > vidFrameTime)
                    {
                        fifow->FIFOWrite(0, frame.buf, vidSize);
                        count++;
                        delta -= (int)vidFrameTime;
                    }
                    QString msg = QString("Added %1 blank video frames")
                                  .arg(count);
                    curFrameNum += count;
                    dropvideo = 0;
                    wait_recover = 0;
                }
                else
                    wait_recover--;
            }
            else
            {
                dropvideo = 0;
                wait_recover = 0;
            }

            // int buflen = (int)(arb->audiobuffer_len / rateTimeConv);
            // cout << curFrameNum << ": video time: " << frame.timecode
            //      << " audio time: " << arb->last_audiotime << " buf: "
            //      << buflen << " exp: "
            //      << audbufTime << " delta: " << delta << endl;
            if (arb->audiobuffer_len)
                fifow->FIFOWrite(1, arb->audiobuffer, arb->audiobuffer_len);
            if (dropvideo < 0)
            {
                dropvideo++;
                curFrameNum--;
            }
            else
            {
                fifow->FIFOWrite(0, frame.buf, vidSize);
                if (dropvideo)
                {
                    fifow->FIFOWrite(0, frame.buf, vidSize);
                    curFrameNum++;
                    dropvideo--;
                }
            }
            videoOutput->DoneDisplayingFrame();
            audioOutput->Reset();
            nvp->FlushTxtBuffers();
            lasttimecode = frame.timecode;
        }
        else if (copyaudio)
        {
            // Encoding from NuppelVideo to NuppelVideo with MP3 audio
            // So let's not decode/reencode audio
            if (!nvp->GetRawAudioState()) 
            {
                // The Raw state changed during decode.  This is not good
                unlink(outputname);
                delete newFrame;
                VERBOSE(VB_IMPORTANT, "Transcoding aborted, NuppelVideoPlayer "
                                      "is not in raw audio mode.");
                return REENCODE_ERROR;
            }

            if (forceKeyFrames) 
                writekeyframe = true;
            else
            {
                writekeyframe = is_key;
                if (writekeyframe)
                {
                    // Currently, we don't create new sync frames,
                    // (though we do create new 'I' frames), so we mark
                    // the key-frames before deciding whether we need a
                    // new 'I' frame.

                    //need to correct the frame# and timecode here
                    // Question:  Is it necessary to change the timecodes?
                    long sync_offset = nvp->UpdateStoredFrameNum(curFrameNum);
                    nvr->UpdateSeekTable(num_keyframes, false, sync_offset);
                    ReencoderAddKFA(curFrameNum, lastKeyFrame, num_keyframes);
                    num_keyframes++;
                    lastKeyFrame = curFrameNum;

                    if (did_ff)
                        did_ff = 0;
                }
            }

            if (did_ff == 1)
            {
                timecodeOffset +=
                    (frame.timecode - lasttimecode - (int)vidFrameTime);
            }
            lasttimecode = frame.timecode;
            frame.timecode -= timecodeOffset;

            if (! nvp->WriteStoredData(outRingBuffer, (did_ff == 0),
                                       timecodeOffset))
            {
                if (video_aspect != nvp->GetVideoAspect())
                {
                    video_aspect = nvp->GetVideoAspect();
                    nvr->SetNewVideoParams(video_aspect);
                }

                if (video_width != nvp->GetVideoWidth() ||
                    video_height != nvp->GetVideoHeight())
                {
                    video_width = nvp->GetVideoWidth();
                    video_height = nvp->GetVideoHeight();

                    VERBOSE(VB_IMPORTANT, QString("Resizing from %1x%2 to %3x%4")
                        .arg(video_width).arg(video_height)
                        .arg(newWidth).arg(newHeight));

                }

                if (did_ff == 1)
                {
                  // Create a new 'I' frame if we just processed a cut.
                  did_ff = 2;
                  writekeyframe = true;
                }

                if ((video_width == newWidth) && (video_height == newHeight))
                {
                    frame.buf = lastDecode->buf;
                }
                else
                {
                    avpicture_fill(&imageIn, lastDecode->buf, PIX_FMT_YUV420P,
                                   video_width, video_height);
                    avpicture_fill(&imageOut, frame.buf, PIX_FMT_YUV420P,
                                   newWidth, newHeight);
                    if (video_height != 1088) {
                        scontext = img_resample_init(newWidth, newHeight,
                                                     video_width, video_height);
                    } else {
                        scontext = img_resample_full_init(newWidth, newHeight,
                                                     video_width, video_height,
                                                     0,8,0,0,0,0,0,0);
                    }
                    img_resample(scontext, &imageOut, &imageIn);
                    img_resample_close(scontext);
                }

                nvr->WriteVideo(&frame, true, writekeyframe);
            }
            audioOutput->Reset();
            nvp->FlushTxtBuffers();
        } 
        else 
        {
            if (did_ff == 1)
            {
                did_ff = 2;
                timecodeOffset +=
                    (frame.timecode - lasttimecode - (int)vidFrameTime);
            }

            if (video_aspect != nvp->GetVideoAspect())
            {
                video_aspect = nvp->GetVideoAspect();
                nvr->SetNewVideoParams(video_aspect);
            }

            if (video_width != nvp->GetVideoWidth() || 
                video_height != nvp->GetVideoHeight())
            {
                video_width = nvp->GetVideoWidth();
                video_height = nvp->GetVideoHeight(); 
                VERBOSE(VB_IMPORTANT, QString("Resizing from %1x%2 to %3x%4")
                        .arg(video_width).arg(video_height)
                        .arg(newWidth).arg(newHeight));
            }

            if ((video_width == newWidth) && (video_height == newHeight))
            {
                frame.buf = lastDecode->buf;
            }
            else
            {
                avpicture_fill(&imageIn, lastDecode->buf, PIX_FMT_YUV420P,
                               video_width, video_height);
                avpicture_fill(&imageOut, frame.buf, PIX_FMT_YUV420P,
                               newWidth, newHeight);
                if (video_height != 1088) {
                    scontext = img_resample_init(newWidth, newHeight,
                                                 video_width, video_height);
                }
                else
                {
                    scontext = img_resample_full_init(newWidth, newHeight,
                                                      video_width, video_height,
                                                      0,8,0,0,0,0,0,0);
                }
 
                img_resample(scontext, &imageOut, &imageIn);
                img_resample_close(scontext);
            }

            // audio is fully decoded, so we need to reencode it
            audioframesize = arb->audiobuffer_len;
            if (audioframesize > 0)
            {
                nvr->SetOption("audioframesize", audioframesize);
                int starttime = audioOutput->GetAudiotime();
                nvr->WriteAudio(arb->audiobuffer, audioFrame++,
                                starttime - timecodeOffset);
                if (nvr->IsErrored()) {
                    VERBOSE(VB_IMPORTANT, "Transcode: Encountered "
                            "irrecoverable error in NVR::WriteAudio");
                    delete newFrame;
                    return REENCODE_ERROR;
                }
                arb->audiobuffer_len = 0;
            }
            nvp->TranscodeWriteText(&TranscodeWriteText, (void *)(nvr));

            lasttimecode = frame.timecode;
            frame.timecode -= timecodeOffset;

            if (forceKeyFrames)
                nvr->WriteVideo(&frame, true, true);
            else
                nvr->WriteVideo(&frame);
        }
        if (showprogress && QDateTime::currentDateTime() > statustime)
        {
            VERBOSE(VB_IMPORTANT, QString("Processed: %1 of %2 frames(%3 seconds)").
                    arg((long)curFrameNum).arg((long)total_frame_count).
                    arg((long)(curFrameNum / video_frame_rate)));
            statustime = QDateTime::currentDateTime();
            statustime = statustime.addSecs(5);
        }
        if (QDateTime::currentDateTime() > curtime) 
        {
            if (honorCutList && m_proginfo &&
                m_proginfo->CheckMarkupFlag(MARK_UPDATED_CUT))
            {
                unlink(outputname);
                delete newFrame;
                VERBOSE(VB_IMPORTANT, "Transcoding aborted, cutlist updated");
                return REENCODE_CUTLIST_CHANGE;
            }

            if ((jobID >= 0) || (print_verbose_messages & VB_IMPORTANT))
            {
                if (JobQueue::GetJobCmd(jobID) == JOB_STOP)
                {
                    unlink(outputname);
                    delete newFrame;
                    VERBOSE(VB_IMPORTANT, "Transcoding STOPped by JobQueue");
                    return REENCODE_STOPPED;
                }

                float flagFPS = 0.0;
                float elapsed = flagTime.elapsed() / 1000.0;
                if (elapsed)
                    flagFPS = curFrameNum / elapsed;

                int percentage = curFrameNum * 100 / new_frame_count;

                if (jobID >= 0)
                    JobQueue::ChangeJobComment(jobID,
                              QObject::tr("%1% Completed @ %2 fps.")
                                          .arg(percentage).arg(flagFPS));
                else
                    VERBOSE(VB_IMPORTANT, QString(
                            "mythtranscode: %1% Completed @ %2 fps.")
                            .arg(percentage).arg(flagFPS));

            }
            curtime = QDateTime::currentDateTime();
            curtime = curtime.addSecs(20);
        }

        curFrameNum++;
        frame.frameNumber = 1 + (curFrameNum << 1);
    }

    if (! fifow)
    {
        if (m_proginfo)
        {
            m_proginfo->ClearPositionMap(MARK_KEYFRAME);
            m_proginfo->ClearPositionMap(MARK_GOP_START);
            m_proginfo->ClearPositionMap(MARK_GOP_BYFRAME);
        }

        nvr->WriteSeekTable();
        if (!kfa_table->isEmpty())
            nvr->WriteKeyFrameAdjustTable(kfa_table);
    } else {
        fifow->FIFODrain();
    }
    delete newFrame;
    return REENCODE_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

