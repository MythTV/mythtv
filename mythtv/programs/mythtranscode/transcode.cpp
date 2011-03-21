#include <fcntl.h>
#include <math.h>

#include <QStringList>
#include <QMap>
#include <QRegExp>

#include "mythconfig.h"

#include "transcode.h"
#include "audiooutput.h"
#include "spdifencoder.h"
#include "recordingprofile.h"
#include "mythcorecontext.h"
#include "jobqueue.h"
#include "exitcodes.h"

#include "NuppelVideoRecorder.h"
#include "mythplayer.h"
#include "programinfo.h"
#include "mythdbcon.h"


extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

using namespace std;

#define LOC QString("Transcode: ")
#define LOC_ERR QString("Transcode, Error: ")

// This class is to act as a fake audio output device to store the data
// for reencoding.

class AudioReencodeBuffer : public AudioOutput
{
 public:
    AudioReencodeBuffer(AudioFormat audio_format, int audio_channels,
                        bool passthru)
    {
        Reset();
        const AudioSettings settings(audio_format, audio_channels, 0, 0, false);
        Reconfigure(settings);
        bufsize = 512000;
        audiobuffer = new unsigned char[bufsize];

        ab_count = 0;
        memset(ab_len, 0, sizeof(ab_len));
        memset(ab_offset, 0, sizeof(ab_offset));
        memset(ab_time, 0, sizeof(ab_time));
        m_initpassthru = passthru;
        m_spdifenc = NULL;
    }

    ~AudioReencodeBuffer()
    {
        delete [] audiobuffer;
    }

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings)
    {
        ClearError();

        m_passthru      = settings.use_passthru;
        channels        = settings.channels;
        bytes_per_frame = channels *
            AudioOutputSettings::SampleSize(settings.format);
        m_samplerate    = settings.samplerate;

        if (m_passthru)
        {
            QString log = AudioOutputSettings::GetPassthroughParams(
                settings.codec,
                settings.codec_profile,
                m_samplerate, channels,
                true);
            VERBOSE(VB_AUDIO, "Setting " + log + " passthrough");

            bytes_per_frame = channels *
                AudioOutputSettings::SampleSize(FORMAT_S16);

            if (m_spdifenc)
            {
                delete m_spdifenc;
            }

            m_spdifenc = new SPDIFEncoder("spdif", settings.codec);
            if (m_spdifenc->Succeeded() && settings.codec == CODEC_ID_DTS)
            {
                switch(settings.codec_profile)
                {
                    case FF_PROFILE_DTS:
                    case FF_PROFILE_DTS_ES:
                    case FF_PROFILE_DTS_96_24:
                        m_spdifenc->SetMaxHDRate(0);
                        break;
                    case FF_PROFILE_DTS_HD_HRA:
                        m_spdifenc->SetMaxHDRate(192000);
                        break;
                    case FF_PROFILE_DTS_HD_MA:
                        m_spdifenc->SetMaxHDRate(768000);
                        break;
                }
            }
            if (!m_spdifenc->Succeeded())
            {
                delete m_spdifenc;
                m_spdifenc = NULL;
            }
        }
        eff_audiorate   = m_samplerate * 100;
    }

    // dsprate is in 100 * frames/second
    virtual void SetEffDsp(int dsprate)
    {
        eff_audiorate = (dsprate / 100);
    }

    virtual void Reset(void)
    {
        audiobuffer_len = 0;
        ab_count = 0;
    }

    virtual int64_t LengthLastData(void) { return m_length_last_data; }

    // timecode is in milliseconds.
    virtual bool AddFrames(void *buffer, int frames, int64_t timecode)
    {
        return AddData(buffer, frames * bytes_per_frame, timecode);
    }

    // timecode is in milliseconds.
    virtual bool AddData(void *buffer, int len, int64_t timecode)
    {
        int freebuf = bufsize - audiobuffer_len;
        int newlen;

        if (m_passthru && m_spdifenc)
        {
                /*
                 * mux into an IEC958 packet. The resulting data will be dumped.
                 * We do so to estimate timestamps
                 */
            m_spdifenc->WriteFrame((unsigned char *)buffer, len);
            newlen = m_spdifenc->GetProcessedSize();
            if (newlen > 0)
            {
                m_spdifenc->Reset();
            }
        }
        else
        {
            newlen = len;
        }
        m_length_last_data = (int64_t)
            ((double)(newlen * 1000) / (m_samplerate * bytes_per_frame));

        if (len > freebuf)
        {
            bufsize += len - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        ab_len[ab_count] = len;
        ab_offset[ab_count] = audiobuffer_len;

        memcpy(audiobuffer + audiobuffer_len, buffer,
               len);
        audiobuffer_len += len;

        // last_audiotime is at the end of the frame
        last_audiotime = timecode + (newlen / bytes_per_frame) * 1000 /
            eff_audiorate;

        ab_time[ab_count] = last_audiotime;
        ab_count++;

        return true;
    }

    virtual void SetTimecode(int64_t timecode)
    {
        last_audiotime = timecode;
    }
    virtual bool CanPassthrough(int, int) const
    {
        return false;
    }
    virtual bool IsPaused(void) const
    {
        return false;
    }
    virtual void Pause(bool paused)
    {
        (void)paused;
    }
    virtual void PauseUntilBuffered(void)
    {
        // Do nothing
    }
    virtual void Drain(void)
    {
        // Do nothing
    }

    virtual int64_t GetAudiotime(void)
    {
        return last_audiotime;
    }

    virtual int GetVolumeChannel(int) const
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
    virtual uint GetCurrentVolume(void) const
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
    virtual MuteState GetMuteState(void) const
    {
        // Do nothing
        return kMuteOff;
    }
    virtual MuteState IterateMutedChannels(void)
    {
        // Do nothing
        return kMuteOff;
    }
    virtual bool ToggleUpmix(void)
    {
        // Do nothing
        return false;
    }

    virtual void SetSWVolume(int new_volume, bool save)
    {
        // Do nothing
        return;
    }
    virtual int GetSWVolume(void)
    {
        // Do nothing
        return 100;
    }

    //  These are pure virtual in AudioOutput, but we don't need them here
    virtual void bufferOutputData(bool){ return; }
    virtual int readOutputData(unsigned char*, int ){ return 0; }

    /**
     * Test if we can output digital audio
     */
    virtual bool CanPassthrough(int, int, int) const { return m_initpassthru; }

    int bufsize;
    int ab_count;
    int ab_len[128];
    int ab_offset[128];
    long long ab_time[128];
    unsigned char *audiobuffer;
    int audiobuffer_len, channels, bits, bytes_per_frame, eff_audiorate;
    long long last_audiotime;
private:
    int                 m_samplerate;
    bool                m_passthru, m_initpassthru;
    // SPDIF Encoder for digital passthrough
    SPDIFEncoder       *m_spdifenc;
    int64_t             m_length_last_data;
};

Transcode::Transcode(ProgramInfo *pginfo) :
    m_proginfo(pginfo),
    keyframedist(30),
    nvr(NULL),
    player(NULL),
    player_ctx(NULL),
    inRingBuffer(NULL),
    outRingBuffer(NULL),
    fifow(NULL),
    kfa_table(NULL),
    showprogress(false),
    recorderOptions("")
{
}

Transcode::~Transcode()
{
    if (nvr)
        delete nvr;
    if (player_ctx)
    {
        player       = NULL;
        inRingBuffer = NULL;
        delete player_ctx;
    }
    if (outRingBuffer)
        delete outRingBuffer;
    if (fifow)
        delete fifow;
    if (kfa_table)
        delete kfa_table;
}
void Transcode::ReencoderAddKFA(long curframe, long lastkey, long num_keyframes)
{
    long delta = curframe - lastkey;
    if (delta != 0 && delta != keyframedist)
    {
        struct kfatable_entry kfate;
        kfate.adjust = keyframedist - delta;
        kfate.keyframe_number = num_keyframes;
        kfa_table->push_back(kfate);
    }
}

bool Transcode::GetProfile(QString profileName, QString encodingType,
                           int height, int frameRate)
{
    if (profileName.toLower() == "autodetect")
    {
        if (height == 1088)
            height = 1080;

        QString autoProfileName = QObject::tr("Autodetect from %1").arg(height);
        if (frameRate == 25 || frameRate == 30)
            autoProfileName += "i";
        if (frameRate == 50 || frameRate == 60)
            autoProfileName += "p";

        bool result = false;
        VERBOSE(VB_IMPORTANT,
                QString("Transcode: Looking for autodetect profile: %1").
                arg(autoProfileName));
        result = profile.loadByGroup(autoProfileName, "Transcoders");

        if (!result && encodingType == "MPEG-2")
        {
            result = profile.loadByGroup("MPEG2", "Transcoders");
            autoProfileName = "MPEG2";
        }
        if (!result && (encodingType == "MPEG-4" || encodingType == "RTjpeg"))
        {
            result = profile.loadByGroup("RTjpeg/MPEG4",
                                         "Transcoders");
            autoProfileName = "RTjpeg/MPEG4";
        }
        if (!result)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Transcode: Couldn't find profile for : %1").
                    arg(encodingType));

            return false;
        }

        VERBOSE(VB_IMPORTANT,
                QString("Transcode: Using autodetect profile: %1").
                arg(autoProfileName));
    }
    else
    {
        bool isNum;
        int profileID;
        profileID = profileName.toInt(&isNum);
        // If a bad profile is specified, there will be trouble
        if (isNum && profileID > 0)
            profile.loadByID(profileID);
        else if (!profile.loadByGroup(profileName, "Transcoders"))
        {
            VERBOSE(VB_IMPORTANT, QString("Couldn't find profile #: %1").
                    arg(profileName));
            return false;
        }
    }
    return true;
}

static QString get_str_option(RecordingProfile &profile, const QString &name)
{
    const Setting *setting = profile.byName(name);
    if (setting)
        return setting->getValue();

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                "get_str_option(...%1): Option not in profile.").arg(name));

    return QString::null;
}

static int get_int_option(RecordingProfile &profile, const QString &name)
{
    QString ret_str = get_str_option(profile, name);
    if (ret_str.isEmpty())
        return 0;

    bool ok = false;
    int ret_int = ret_str.toInt(&ok);

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                    "get_int_option(...%1): Option is not an int.").arg(name));
    }

    return ret_int;
}

static void TranscodeWriteText(void *ptr, unsigned char *buf, int len, int timecode, int pagenr)
{
  NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)ptr;
  nvr->WriteText(buf, len, timecode, pagenr);
}

int Transcode::TranscodeFile(
    const QString &inputname,
    const QString &outputname,
    const QString &profileName,
    bool honorCutList, bool framecontrol,
    int jobID, QString fifodir,
    frm_dir_map_t &deleteMap,
    int AudioTrackNo,
    bool passthru)
{
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime statustime = curtime;
    int audioframesize;
    int audioFrame = 0;

    if (jobID >= 0)
        JobQueue::ChangeJobComment(jobID, "0% " + QObject::tr("Completed"));

    nvr = new NuppelVideoRecorder(NULL, NULL);

    // Input setup
    inRingBuffer = RingBuffer::Create(inputname, false, false);
    player = new MythPlayer();

    player_ctx = new PlayerContext(kTranscoderInUseID);
    player_ctx->SetPlayingInfo(m_proginfo);
    player_ctx->SetRingBuffer(inRingBuffer);
    player_ctx->SetPlayer(player);
    player->SetNullVideo();
    player->SetPlayerInfo(NULL, NULL, true, player_ctx);

    if (showprogress)
    {
        statustime = statustime.addSecs(5);
    }

    AudioOutput *audioOutput = new AudioReencodeBuffer(FORMAT_NONE, 0,
                                                       passthru);
    AudioReencodeBuffer *arb = ((AudioReencodeBuffer*)audioOutput);
    player->GetAudio()->SetAudioOutput(audioOutput);
    player->SetTranscoding(true);

    if (player->OpenFile(false) < 0)
    {
        VERBOSE(VB_IMPORTANT, "Transcoding aborted, error opening file.");
        if (player_ctx)
            delete player_ctx;
        return REENCODE_ERROR;
    }

    long long total_frame_count = player->GetTotalFrameCount();
    long long new_frame_count = total_frame_count;
    if (honorCutList && m_proginfo)
    {
        VERBOSE(VB_GENERAL, "Honoring the cutlist while transcoding");

        frm_dir_map_t::const_iterator it;
        QString cutStr;
        long long lastStart = 0;

        if (deleteMap.size() == 0)
            m_proginfo->QueryCutList(deleteMap);

        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
        {
            if (*it)
            {
                if (!cutStr.isEmpty())
                    cutStr += ",";
                cutStr += QString("%1-").arg((long)it.key());
                lastStart = it.key();
            }
            else
            {
                if (cutStr.isEmpty())
                    cutStr += "0-";
                cutStr += QString("%1").arg((long)it.key());
                new_frame_count -= (it.key() - lastStart);
            }
        }
        if (cutStr.isEmpty())
            cutStr = "Is Empty";
        else if (cutStr.endsWith('-') && (total_frame_count > lastStart))
        {
            new_frame_count -= (total_frame_count - lastStart);
            cutStr += QString("%1").arg(total_frame_count);
        }
        VERBOSE(VB_GENERAL, QString("Cutlist        : %1").arg(cutStr));
        VERBOSE(VB_GENERAL, QString("Original Length: %1 frames")
                                    .arg((long)total_frame_count));
        VERBOSE(VB_GENERAL, QString("New Length     : %1 frames")
                                    .arg((long)new_frame_count));

        if ((m_proginfo->QueryIsEditing()) ||
            (JobQueue::IsJobRunning(JOB_COMMFLAG, *m_proginfo)))
        {
            VERBOSE(VB_IMPORTANT, "Transcoding aborted, cutlist changed");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_CUTLIST_CHANGE;
        }
        m_proginfo->ClearMarkupFlag(MARK_UPDATED_CUT);
        curtime = curtime.addSecs(60);
    }

    player->GetAudio()->ReinitAudio();
    QString encodingType = player->GetEncodingType();
    bool copyvideo = false, copyaudio = false;

    QString vidsetting = NULL, audsetting = NULL, vidfilters = NULL;

    QSize buf_size = player->GetVideoBufferSize();
    int video_width = buf_size.width();
    int video_height = buf_size.height();

    if (video_height == 1088) {
       VERBOSE(VB_IMPORTANT, "Found video height of 1088.  This is unusual and "
               "more than likely the video is actually 1080 so mythtranscode "
               "will treat it as such.");
    }

    float video_aspect = player->GetVideoAspect();
    float video_frame_rate = player->GetFrameRate();
    int newWidth = video_width;
    int newHeight = video_height;

    kfa_table = new vector<struct kfatable_entry>;

    if (fifodir.isEmpty())
    {
        if (!GetProfile(profileName, encodingType, video_height,
                        (int)round(video_frame_rate))) {
            VERBOSE(VB_IMPORTANT, "Transcoding aborted, no profile found.");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        // For overriding settings on the command line
        QMap<QString, QString> recorderOptionsMap;
        if (!recorderOptions.isEmpty())
        {
            QStringList options = recorderOptions
                .split(",", QString::SkipEmptyParts);
            int loop = 0;
            while (loop < options.size())
            {
                QStringList tokens = options[loop].split("=");
                recorderOptionsMap[tokens[0]] = tokens[1];

                loop++;
            }
        }

        vidsetting = get_str_option(profile, "videocodec");
        audsetting = get_str_option(profile, "audiocodec");
        vidfilters = get_str_option(profile, "transcodefilters");

        if (encodingType == "MPEG-2" &&
            get_int_option(profile, "transcodelossless"))
        {
            VERBOSE(VB_IMPORTANT, "Switching to MPEG-2 transcoder.");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_MPEG2TRANS;
        }

        // Recorder setup
        if (get_int_option(profile, "transcodelossless"))
        {
            vidsetting = encodingType;
            audsetting = "MP3";
        }
        else if (get_int_option(profile, "transcoderesize"))
        {
            int actualHeight = (video_height == 1088 ? 1080 : video_height);

            player->SetVideoFilters(vidfilters);
            newWidth = get_int_option(profile, "width");
            newHeight = get_int_option(profile, "height");

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

            if (encodingType.left(4).toLower() == "mpeg")
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
            player->SetVideoFilters(vidfilters);

        // this is ripped from tv_rec SetupRecording. It'd be nice to merge
        nvr->SetOption("inpixfmt", FMT_YV12);

        nvr->SetOption("width", newWidth);
        nvr->SetOption("height", newHeight);

        nvr->SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
        nvr->SetOption("vbiformat", gCoreContext->GetSetting("VbiFormat"));

        nvr->SetFrameRate(video_frame_rate);
        nvr->SetVideoAspect(video_aspect);
        nvr->SetTranscoding(true);

        if ((vidsetting == "MPEG-4") ||
            (recorderOptionsMap["videocodec"] == "mpeg4"))
        {
            nvr->SetOption("videocodec", "mpeg4");

            nvr->SetIntOption(&profile, "mpeg4bitrate");
            nvr->SetIntOption(&profile, "scalebitrate");
            nvr->SetIntOption(&profile, "mpeg4maxquality");
            nvr->SetIntOption(&profile, "mpeg4minquality");
            nvr->SetIntOption(&profile, "mpeg4qualdiff");
            nvr->SetIntOption(&profile, "mpeg4optionvhq");
            nvr->SetIntOption(&profile, "mpeg4option4mv");
#ifdef USING_FFMPEG_THREADS
            nvr->SetIntOption(profile, "encodingthreadcount");
#endif
        }
        else if ((vidsetting == "MPEG-2") ||
                 (recorderOptionsMap["videocodec"] == "mpeg2video"))
        {
            nvr->SetOption("videocodec", "mpeg2video");

            nvr->SetIntOption(&profile, "mpeg2bitrate");
            nvr->SetIntOption(&profile, "scalebitrate");
#ifdef USING_FFMPEG_THREADS
            nvr->SetIntOption(&profile, "encodingthreadcount");
#endif
        }
        else if ((vidsetting == "RTjpeg") ||
                 (recorderOptionsMap["videocodec"] == "rtjpeg"))
        {
            nvr->SetOption("videocodec", "rtjpeg");
            nvr->SetIntOption(&profile, "rtjpegquality");
            nvr->SetIntOption(&profile, "rtjpegchromafilter");
            nvr->SetIntOption(&profile, "rtjpeglumafilter");
        }
        else if (vidsetting.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "No video information found!");
            VERBOSE(VB_IMPORTANT, "Please ensure that recording profiles "
                                  "for the transcoder are set");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("Unknown video codec: %1").arg(vidsetting));
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        nvr->SetOption("samplerate", arb->eff_audiorate);
        if (audsetting == "MP3")
        {
            nvr->SetOption("audiocompression", 1);
            nvr->SetIntOption(&profile, "mp3quality");
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

        // For overriding settings on the command line
        if (recorderOptionsMap.size() > 0)
        {
            QMap<QString, QString>::Iterator it;
            QString key, value;
            for (it = recorderOptionsMap.begin();
                 it != recorderOptionsMap.end(); ++it)
            {
                key   = it.key();
                value = *it;

                VERBOSE(VB_IMPORTANT,
                        QString("Forcing Recorder option '%1' to '%2'")
                        .arg(key).arg(value));

                if (value.contains(QRegExp("[^0-9]")))
                    nvr->SetOption(key, value);
                else
                    nvr->SetOption(key, value.toInt());

                if (key == "width")
                    newWidth  = (value.toInt() + 15) & ~0xF;
                else if (key == "height")
                    newHeight = (value.toInt() + 15) & ~0xF;
                else if (key == "videocodec")
                {
                    if (value == "mpeg4")
                        vidsetting = "MPEG-4";
                    else if (value == "mpeg2video")
                        vidsetting = "MPEG-2";
                    else if (value == "rtjpeg")
                        vidsetting = "RTjpeg";
                }
            }
        }

        if ((vidsetting == "MPEG-4") ||
            (vidsetting == "MPEG-2"))
            nvr->SetupAVCodecVideo();
        else if (vidsetting == "RTjpeg")
            nvr->SetupRTjpeg();

        outRingBuffer = RingBuffer::Create(outputname, true, false);
        nvr->SetRingBuffer(outRingBuffer);
        nvr->WriteHeader();
        nvr->StreamAllocate();
    }

    if (vidsetting == encodingType && !framecontrol &&
        fifodir.isEmpty() && honorCutList &&
        video_width == newWidth && video_height == newHeight)
    {
        copyvideo = true;
        VERBOSE(VB_GENERAL, "Reencoding video in 'raw' mode");
    }

    if (deleteMap.size() > 0)
        player->SetCutList(deleteMap);

    keyframedist = 30;
    player->InitForTranscode(copyaudio, copyvideo);
    if (player->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "Unable to initialize MythPlayer "
                "for Transcode");
        if (player_ctx)
            delete player_ctx;
        return REENCODE_ERROR;
    }

    if (AudioTrackNo > -1)
    {
        VERBOSE(VB_GENERAL, QString("Set audiotrack number to %1").arg(AudioTrackNo));
        player->GetDecoder()->SetTrack(kTrackTypeAudio, AudioTrackNo);
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

    if (!fifodir.isEmpty())
    {
        QString audfifo = fifodir + QString("/audout");
        QString vidfifo = fifodir + QString("/vidout");
        int audio_size = arb->eff_audiorate * arb->bytes_per_frame;
        // framecontrol is true if we want to enforce fifo sync.
        if (framecontrol)
            VERBOSE(VB_GENERAL, "Enforcing sync on fifos");
        fifow = new FIFOWriter(2, framecontrol);

        if (!fifow->FIFOInit(0, QString("video"), vidfifo, vidSize, 50) ||
            !fifow->FIFOInit(1, QString("audio"), audfifo, audio_size, 25))
        {
            VERBOSE(VB_IMPORTANT, "Error initializing fifo writer.  Aborting");
            unlink(outputname.toLocal8Bit().constData());
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }
        VERBOSE(VB_GENERAL, QString("Video %1x%2@%3fps Audio rate: %4")
                                   .arg(video_width).arg(video_height)
                                   .arg(video_frame_rate)
                                   .arg(arb->eff_audiorate));
        VERBOSE(VB_GENERAL, "Created fifos. Waiting for connection.");
    }

    bool forceKeyFrames = (fifow == NULL) ? framecontrol : false;

    frm_dir_map_t::iterator dm_iter;
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

    float rateTimeConv = arb->eff_audiorate * arb->bytes_per_frame / 1000.0;
    float vidFrameTime = 1000.0 / video_frame_rate;
    int wait_recover = 0;
    VideoOutput *videoOutput = player->getVideoOutput();
    bool is_key = 0;
    bool first_loop = true;
    unsigned char *newFrame = new unsigned char[frame.size];

    frame.buf = newFrame;
    AVPicture imageIn, imageOut;
    struct SwsContext  *scontext = NULL;

    if (fifow)
        VERBOSE(VB_GENERAL, "Dumping Video and Audio data to fifos");
    else if (copyaudio)
        VERBOSE(VB_GENERAL, "Copying Audio while transcoding Video");
    else
        VERBOSE(VB_GENERAL, "Transcoding Video and Audio");

    QTime flagTime;
    flagTime.start();

    while (player->TranscodeGetNextFrame(dm_iter, did_ff, is_key, honorCutList))
    {
        if (first_loop)
        {
            copyaudio = player->GetRawAudioState();
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
            videoOutput->DoneDisplayingFrame(lastDecode);
            audioOutput->Reset();
            player->GetCC608Reader()->FlushTxtBuffers();
            lasttimecode = frame.timecode;
        }
        else if (copyaudio)
        {
            // Encoding from NuppelVideo to NuppelVideo with MP3 audio
            // So let's not decode/reencode audio
            if (!player->GetRawAudioState())
            {
                // The Raw state changed during decode.  This is not good
                VERBOSE(VB_IMPORTANT, "Transcoding aborted, MythPlayer "
                        "is not in raw audio mode.");

                unlink(outputname.toLocal8Bit().constData());
                delete [] newFrame;
                if (player_ctx)
                    delete player_ctx;
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
                    long sync_offset = player->UpdateStoredFrameNum(curFrameNum);
                    nvr->UpdateSeekTable(num_keyframes, sync_offset);
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

            if (! player->WriteStoredData(outRingBuffer, (did_ff == 0),
                                       timecodeOffset))
            {
                if (video_aspect != player->GetVideoAspect())
                {
                    video_aspect = player->GetVideoAspect();
                    nvr->SetNewVideoParams(video_aspect);
                }

                QSize buf_size = player->GetVideoBufferSize();

                if (video_width != buf_size.width() ||
                    video_height != buf_size.height())
                {
                    video_width = buf_size.width();
                    video_height = buf_size.height();

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
                    frame.buf = newFrame;
                    avpicture_fill(&imageIn, lastDecode->buf, PIX_FMT_YUV420P,
                                   video_width, video_height);
                    avpicture_fill(&imageOut, frame.buf, PIX_FMT_YUV420P,
                                   newWidth, newHeight);

                    int bottomBand = (video_height == 1088) ? 8 : 0;
                    scontext = sws_getCachedContext(scontext, video_width,
                                   video_height, PIX_FMT_YUV420P, newWidth,
                                   newHeight, PIX_FMT_YUV420P,
                                   SWS_FAST_BILINEAR, NULL, NULL, NULL);

                    sws_scale(scontext, imageIn.data, imageIn.linesize, 0,
                              video_height - bottomBand,
                              imageOut.data, imageOut.linesize);
                }

                nvr->WriteVideo(&frame, true, writekeyframe);
            }
            audioOutput->Reset();
            player->GetCC608Reader()->FlushTxtBuffers();
        }
        else
        {
            if (did_ff == 1)
            {
                did_ff = 2;
                timecodeOffset +=
                    (frame.timecode - lasttimecode - (int)vidFrameTime);
            }

            if (video_aspect != player->GetVideoAspect())
            {
                video_aspect = player->GetVideoAspect();
                nvr->SetNewVideoParams(video_aspect);
            }


            QSize buf_size = player->GetVideoBufferSize();

            if (video_width != buf_size.width() ||
                video_height != buf_size.height())
            {
                video_width = buf_size.width();
                video_height = buf_size.height();

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
                frame.buf = newFrame;
                avpicture_fill(&imageIn, lastDecode->buf, PIX_FMT_YUV420P,
                               video_width, video_height);
                avpicture_fill(&imageOut, frame.buf, PIX_FMT_YUV420P,
                               newWidth, newHeight);

                int bottomBand = (video_height == 1088) ? 8 : 0;
                scontext = sws_getCachedContext(scontext, video_width,
                               video_height, PIX_FMT_YUV420P, newWidth,
                               newHeight, PIX_FMT_YUV420P,
                               SWS_FAST_BILINEAR, NULL, NULL, NULL);

                sws_scale(scontext, imageIn.data, imageIn.linesize, 0,
                          video_height - bottomBand,
                          imageOut.data, imageOut.linesize);
            }

            // audio is fully decoded, so we need to reencode it
            audioframesize = arb->audiobuffer_len;
            if (arb->ab_count)
            {
                for (int loop = 0; loop < arb->ab_count; loop++)
                {
                    nvr->SetOption("audioframesize", arb->ab_len[loop]);
                    nvr->WriteAudio(arb->audiobuffer + arb->ab_offset[loop],
                                    audioFrame++,
                                    arb->ab_time[loop] - timecodeOffset);
                    if (nvr->IsErrored())
                    {
                        VERBOSE(VB_IMPORTANT, "Transcode: Encountered "
                                "irrecoverable error in NVR::WriteAudio");

                        delete [] newFrame;
                        if (player_ctx)
                            delete player_ctx;
                        return REENCODE_ERROR;
                    }
                }
                arb->ab_count = 0;
                arb->audiobuffer_len = 0;
            }

            player->GetCC608Reader()->TranscodeWriteText(&TranscodeWriteText,
                                                   (void *)(nvr));

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
                m_proginfo->QueryMarkupFlag(MARK_UPDATED_CUT))
            {
                VERBOSE(VB_IMPORTANT, "Transcoding aborted, cutlist updated");

                unlink(outputname.toLocal8Bit().constData());
                delete [] newFrame;
                if (player_ctx)
                    delete player_ctx;
                return REENCODE_CUTLIST_CHANGE;
            }

            if ((jobID >= 0) || (VERBOSE_LEVEL_CHECK(VB_IMPORTANT)))
            {
                if (JobQueue::GetJobCmd(jobID) == JOB_STOP)
                {
                    VERBOSE(VB_IMPORTANT, "Transcoding STOPped by JobQueue");

                    unlink(outputname.toLocal8Bit().constData());
                    delete [] newFrame;
                    if (player_ctx)
                        delete player_ctx;
                    return REENCODE_STOPPED;
                }

                float flagFPS = 0.0;
                float elapsed = flagTime.elapsed() / 1000.0;
                if (elapsed)
                    flagFPS = curFrameNum / elapsed;

                int percentage = curFrameNum * 100 / total_frame_count;

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

    sws_freeContext(scontext);

    if (! fifow)
    {
        if (m_proginfo)
        {
            m_proginfo->ClearPositionMap(MARK_KEYFRAME);
            m_proginfo->ClearPositionMap(MARK_GOP_START);
            m_proginfo->ClearPositionMap(MARK_GOP_BYFRAME);
        }

        nvr->WriteSeekTable();
        if (!kfa_table->empty())
            nvr->WriteKeyFrameAdjustTable(*kfa_table);
    } else {
        fifow->FIFODrain();
    }

    delete [] newFrame;
    if (player_ctx)
        delete player_ctx;
    return REENCODE_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

