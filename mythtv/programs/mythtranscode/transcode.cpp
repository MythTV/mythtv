#include <fcntl.h>
#include <math.h>
#include <iostream>

#include <QStringList>
#include <QMap>
#include <QRegExp>
#include <QList>
#include <QWaitCondition>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>

#include "mythconfig.h"

#include "transcode.h"
#include "audiooutput.h"
#include "recordingprofile.h"
#include "mythcorecontext.h"
#include "jobqueue.h"
#include "exitcodes.h"
#include "mthreadpool.h"
#include "deletemap.h"

#include "NuppelVideoRecorder.h"
#include "mythplayer.h"
#include "programinfo.h"
#include "mythdbcon.h"
#include "avformatwriter.h"
#include "httplivestream.h"


extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

using namespace std;

#define LOC QString("Transcode: ")

class AudioBuffer
{
  public:
    AudioBuffer() : m_buffer(QByteArray()), m_time(-1) {};
    AudioBuffer(const AudioBuffer &old) : m_buffer(old.m_buffer),
        m_time(old.m_time) {};

    ~AudioBuffer() {};

    void appendData(unsigned char *buffer, int len, long long time)
    {
        m_buffer.append((const char *)buffer, len);
        m_time = time;
    }

    void setData(unsigned char *buffer, int len, long long time)
    {
        m_buffer.clear();
        appendData(buffer, len, time);
    }

    void clear(void)
    {
        m_buffer.clear();
        m_time = -1;
    }

    QByteArray m_buffer;
    long long m_time;
};

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
        m_initpassthru = passthru;
        m_audioFrameSize = 0;
    }

    ~AudioReencodeBuffer()
    {
        Reset();
    }

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings)
    {
        ClearError();

        m_passthru        = settings.use_passthru;
        m_channels        = settings.channels;
        m_bytes_per_frame = m_channels *
            AudioOutputSettings::SampleSize(settings.format);
        m_eff_audiorate   = settings.samplerate;
    }

    // dsprate is in 100 * frames/second
    virtual void SetEffDsp(int dsprate)
    {
        m_eff_audiorate = (dsprate / 100);
    }

    virtual void Reset(void)
    {
        QMutexLocker locker(&m_bufferMutex);
        for (QList<AudioBuffer *>::iterator it = m_bufferList.begin();
             it != m_bufferList.end(); it = m_bufferList.erase(it))
        {
            AudioBuffer *ab = *it;
            delete ab;
        }
    }

    // timecode is in milliseconds.
    virtual bool AddFrames(void *buffer, int frames, int64_t timecode)
    {
        return AddData(buffer, frames * m_bytes_per_frame, timecode, frames);
    }

    // timecode is in milliseconds.
    virtual bool AddData(void *buffer, int len, int64_t timecode, int frames)
    {
        if (len == 0)
            return true;

        QMutexLocker locker(&m_bufferMutex);
        AudioBuffer *ab;
        unsigned char *buf = (unsigned char *)buffer;
        int savedlen = 0;
        int firstlen = 0;

        if (m_audioFrameSize)
        {
            savedlen = m_saveBuffer.m_buffer.size();
            if (savedlen)
            {
                ab = new AudioBuffer(m_saveBuffer);
                m_saveBuffer.clear();
                firstlen = m_audioFrameSize - savedlen;
            }
            else
                ab = new AudioBuffer;

            int overage;

            if ((overage = (len + savedlen) % m_audioFrameSize))
            {
                long long newtime = timecode + ((len / m_bytes_per_frame) /
                                                (m_eff_audiorate / 1000));
                len -= overage;
                m_saveBuffer.setData(&buf[len], overage, newtime);
            }
        }
        else
        {
            ab = new AudioBuffer;
        }

        int bufflen = (m_audioFrameSize ? m_audioFrameSize : len);
        int index = 0;

        do
        {
            if (!len)
                break;

            int blocklen = (firstlen ? firstlen : bufflen);

            m_last_audiotime = timecode + ((blocklen / m_bytes_per_frame) /
                                           (m_eff_audiorate / 1000));
            ab->appendData(&buf[index], blocklen, m_last_audiotime);
            m_bufferList.append(ab);

            timecode = m_last_audiotime;
            index += blocklen;
            firstlen = 0;

            ab = new AudioBuffer;
        } while (index < len);

        // The last buffer is actually unused
        delete ab;

        return true;
    }

    AudioBuffer *GetData(void)
    {
        QMutexLocker locker(&m_bufferMutex);

        if (m_bufferList.isEmpty())
            return NULL;

        AudioBuffer *ab = m_bufferList.takeFirst();
        return ab;
    }

    int GetCount(long long time)
    {
        QMutexLocker locker(&m_bufferMutex);

        if (m_bufferList.isEmpty())
            return 0;

        int count = 0;
        for (QList<AudioBuffer *>::iterator it = m_bufferList.begin();
             it != m_bufferList.end(); ++it)
        {
            AudioBuffer *ab = *it;

            if (ab->m_time <= time)
                count++;
            else
                break;
        }
        return count;
    }

    long long GetSamples(long long time)
    {
        QMutexLocker locker(&m_bufferMutex);

        if (m_bufferList.isEmpty())
            return 0;

        long long samples = 0;
        for (QList<AudioBuffer *>::iterator it = m_bufferList.begin();
             it != m_bufferList.end(); ++it)
        {
            AudioBuffer *ab = *it;

            if (ab->m_time <= time)
                samples += ab->m_buffer.size();
            else
                break;
        }
        return samples / m_bytes_per_frame;
    }

    virtual void SetTimecode(int64_t timecode)
    {
        m_last_audiotime = timecode;
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
        return m_last_audiotime;
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

    virtual bool IsUpmixing(void)
    {
        // Do nothing
        return false;
    }

    virtual bool ToggleUpmix(void)
    {
        // Do nothing
        return false;
    }

    virtual bool CanUpmix(void)
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
    virtual bool CanPassthrough(int, int, int, int) const
        { return m_initpassthru; }

    int                 m_channels;
    int                 m_bits;
    int                 m_bytes_per_frame;
    int                 m_eff_audiorate;
    long long           m_last_audiotime;
    bool                m_passthru;
    int                 m_audioFrameSize;
  private:
    bool                m_initpassthru;
    QMutex              m_bufferMutex;
    QList<AudioBuffer *> m_bufferList;
    AudioBuffer          m_saveBuffer;
};

// Cutter object is used in performing clean cutting. The
// act of cutting is shared between the player and the
// transcode loop. The player performs the initial part
// of the cut by seeking, and the transcode loop handles
// the remaining part by discarding data.
class Cutter
{
  private:
    bool          active;
    frm_dir_map_t foreshortenedCutList;
    DeleteMap     tracker;
    int64_t       totalFrames;
    int64_t       videoFramesToCut;
    int64_t       audioFramesToCut;
    float         audioFramesPerVideoFrame;
    enum
    {
        MAXLEADIN  = 200,
        MINCUT     = 20
    };

  public:
    Cutter() : active(false), videoFramesToCut(0), audioFramesToCut(0),
        audioFramesPerVideoFrame(0.0) {};

    void SetCutList(frm_dir_map_t &deleteMap)
    {
        // Break each cut into two parts, the first for
        // the player and the second for the transcode loop.
        frm_dir_map_t           remainingCutList;
        frm_dir_map_t::Iterator it;
        int64_t                 start = 0;
        int64_t                 leadinLength;

        foreshortenedCutList.clear();

        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
        {
            switch(it.value())
            {
                case MARK_CUT_START:
                    foreshortenedCutList[it.key()] = MARK_CUT_START;
                    start = it.key();
                    break;

                case MARK_CUT_END:
                    leadinLength = min((int64_t)(it.key() - start),
                                       (int64_t)MAXLEADIN);
                    if (leadinLength >= MINCUT)
                    {
                        foreshortenedCutList[it.key() - leadinLength + 2] =
                            MARK_CUT_END;
                        remainingCutList[it.key() - leadinLength + 1] =
                            MARK_CUT_START;
                        remainingCutList[it.key()] = MARK_CUT_END;
                    }
                    else
                    {
                        // Cut too short to use new method.
                        foreshortenedCutList[it.key()] = MARK_CUT_END;
                    }
                    break;
            }
        }

        tracker.SetMap(remainingCutList);
    }

    frm_dir_map_t AdjustedCutList() const
    {
        return foreshortenedCutList;
    }

    void Activate(float v2a, int64_t total)
    {
        active = true;
        audioFramesPerVideoFrame = v2a;
        totalFrames = total;
        videoFramesToCut = 0;
        audioFramesToCut = 0;
        tracker.TrackerReset(0, totalFrames);
    }

    void NewFrame(int64_t currentFrame)
    {
        if (active)
        {
            if (videoFramesToCut == 0)
            {
                uint64_t jumpTo = 0;

                if (tracker.TrackerWantsToJump(currentFrame, totalFrames,
                                               jumpTo))
                {
                    // Reset the tracker and work out how much video and audio
                    // to drop
                    tracker.TrackerReset(jumpTo, totalFrames);
                    videoFramesToCut = jumpTo - currentFrame;
                    audioFramesToCut += (int64_t)(videoFramesToCut *
                                        audioFramesPerVideoFrame + 0.5);
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Clean cut: discarding frame from %1 to %2: "
                                "vid %3 aud %4")
                        .arg((long)currentFrame).arg((long)jumpTo)
                        .arg((long)videoFramesToCut)
                        .arg((long)audioFramesToCut));
                }
            }
        }
    }

    bool InhibitUseVideoFrame()
    {
        if (videoFramesToCut == 0)
        {
            return false;
        }
        else
        {
            // We are inside a cut. Inhibit use of this frame
            videoFramesToCut--;

            if(videoFramesToCut == 0)
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Clean cut: end of video cut; audio frames left "
                            "to cut %1") .arg((long)audioFramesToCut));

            return true;
        }
    }

    bool InhibitUseAudioFrames(int64_t frames, long *totalAudio)
    {
        if (audioFramesToCut == 0)
        {
            return false;
        }
        else if (abs(audioFramesToCut - frames) < audioFramesToCut)
        {
            // Drop the packet containing these frames if doing
            // so gets us closer to zero left to drop
            audioFramesToCut -= frames;
            if(audioFramesToCut == 0)
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Clean cut: end of audio cut; vidio frames left "
                            "to cut %1") .arg((long)videoFramesToCut));
            return true;
        }
        else
        {
            // Don't drop this packet even though we still have frames to cut,
            // because doing so would put us further out. Instead, inflate the 
            // callers record of how many audio frames have been output.
            *totalAudio += audioFramesToCut;
            audioFramesToCut = 0;
            LOG(VB_GENERAL, LOG_INFO,
                QString("Clean cut: end of audio cut; vidio frames left to "
                        "cut %1") .arg((long)videoFramesToCut));
            return false;
        }
    }

    bool InhibitDummyFrame()
    {
        if (audioFramesToCut > 0)
        {
            // If the cutter is in the process of dropping audio then
            // it is better to drop more audio rather than insert a dummy frame
            audioFramesToCut += (int64_t)(audioFramesPerVideoFrame + 0.5);
            return true;
        }
        else
        {
            return false;
        }
    }

    bool InhibitDropFrame()
    {
        if (audioFramesToCut > (int64_t)(audioFramesPerVideoFrame + 0.5))
        {
            // If the cutter is in the process of dropping audio and the
            // amount to drop is sufficient then we can drop less
            // audio rather than drop a frame
            audioFramesToCut -= (int64_t)(audioFramesPerVideoFrame + 0.5);
            return true;
        }
        else
        {
            return false;
        }
    }
};

typedef struct transcodeFrameInfo
{
    VideoFrame *frame;
    int         didFF;
    bool        isKey;
} TranscodeFrameInfo;

class TranscodeFrameQueue : public QRunnable
{
  public:
    TranscodeFrameQueue(MythPlayer *player, VideoOutput *videoout,
        bool cutlist, int size = 5)
      : m_player(player),         m_videoOutput(videoout),
        m_honorCutlist(cutlist),
        m_eof(false),             m_maxFrames(size),
        m_runThread(true),        m_isRunning(false)
    {

    }

    ~TranscodeFrameQueue()
    {
        m_runThread = false;
        m_frameWaitCond.wakeAll();

        while (m_isRunning)
            usleep(50000);
    }

    void stop(void)
    {
        m_runThread = false;
        m_frameWaitCond.wakeAll();

        while (m_isRunning)
            usleep(50000);
    }

    void run()
    {
        frm_dir_map_t::iterator dm_iter;

        m_isRunning = true;
        while (m_runThread)
        {
            if (m_frameList.size() < m_maxFrames && !m_eof)
            {
                TranscodeFrameInfo tfInfo;
                tfInfo.frame = NULL;
                tfInfo.didFF = 0;
                tfInfo.isKey = false;

                if (m_player->TranscodeGetNextFrame(dm_iter, tfInfo.didFF,
                    tfInfo.isKey, m_honorCutlist))
                {
                    tfInfo.frame = m_videoOutput->GetLastDecodedFrame();

                    QMutexLocker locker(&m_queueLock);
                    m_frameList.append(tfInfo);
                }
                else
                {
                    m_eof = true;
                }

                m_frameWaitCond.wakeAll();
            }
            else
            {
                m_frameWaitLock.lock();
                m_frameWaitCond.wait(&m_frameWaitLock);
                m_frameWaitLock.unlock();
            }
        }
        m_isRunning = false;
    }

    VideoFrame *GetFrame(int &didFF, bool &isKey)
    {
        m_queueLock.lock();

        if (m_frameList.isEmpty())
        {
            m_queueLock.unlock();

            if (m_eof)
                return NULL;

            m_frameWaitLock.lock();
            m_frameWaitCond.wait(&m_frameWaitLock);
            m_frameWaitLock.unlock();

            if (m_frameList.isEmpty())
                return NULL;

            m_queueLock.lock();
        }

        TranscodeFrameInfo tfInfo = m_frameList.takeFirst();
        m_queueLock.unlock();
        m_frameWaitCond.wakeAll();

        didFF = tfInfo.didFF;
        isKey = tfInfo.isKey;

        return tfInfo.frame;
    }

  private:
    MythPlayer               *m_player;
    VideoOutput              *m_videoOutput;
    bool                      m_honorCutlist;
    bool                      m_eof;
    int                       m_maxFrames;
    bool                      m_runThread;
    bool                      m_isRunning;
    QMutex                    m_queueLock;
    QList<TranscodeFrameInfo> m_frameList;
    QWaitCondition            m_frameWaitCond;
    QMutex                    m_frameWaitLock;
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
    recorderOptions(""),
    avfMode(false),
    hlsMode(false),                 hlsStreamID(-1),
    hlsDisableAudioOnly(false),
    hlsMaxSegments(0),
    cmdContainer("mpegts"),         cmdAudioCodec("libmp3lame"),
    cmdVideoCodec("libx264"),
    cmdWidth(480),                  cmdHeight(0),
    cmdBitrate(800000),             cmdAudioBitrate(64000)
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
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Transcode: Looking for autodetect profile: %1")
                .arg(autoProfileName));
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
            LOG(VB_GENERAL, LOG_ERR,
                QString("Transcode: Couldn't find profile for : %1")
                    .arg(encodingType));

            return false;
        }

        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Transcode: Using autodetect profile: %1")
                .arg(autoProfileName));
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
            LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find profile #: %1")
                    .arg(profileName));
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

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("get_str_option(...%1): Option not in profile.").arg(name));

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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("get_int_option(...%1): Option is not an int.").arg(name));
    }

    return ret_int;
}

static void TranscodeWriteText(void *ptr, unsigned char *buf, int len,
                               int timecode, int pagenr)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)ptr;
    nvr->WriteText(buf, len, timecode, pagenr);
}

int Transcode::TranscodeFile(const QString &inputname,
                             const QString &outputname,
                             const QString &profileName,
                             bool honorCutList, bool framecontrol,
                             int jobID, QString fifodir,
                             bool fifo_info, bool cleanCut,
                             frm_dir_map_t &deleteMap,
                             int AudioTrackNo,
                             bool passthru)
{
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime statustime = curtime;
    int audioFrame = 0;
    Cutter *cutter = NULL;
    AVFormatWriter *avfw = NULL;
    AVFormatWriter *avfw2 = NULL;
    HTTPLiveStream *hls = NULL;
    int hlsSegmentSize = 0;
    int hlsSegmentFrames = 0;

    if (jobID >= 0)
        JobQueue::ChangeJobComment(jobID, "0% " + QObject::tr("Completed"));

    if (hlsMode)
    {
        avfMode = true;

        if (hlsStreamID != -1)
        {
            hls = new HTTPLiveStream(hlsStreamID);
            hls->UpdateStatus(kHLSStatusStarting);
            cmdWidth = hls->GetWidth();
            cmdHeight = hls->GetHeight();
            cmdBitrate = hls->GetBitrate();
            cmdAudioBitrate = hls->GetAudioBitrate();
        }
    }

    if (!avfMode)
    {
        nvr = new NuppelVideoRecorder(NULL, NULL);

        if (!nvr)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Transcoding aborted, error creating NuppelVideoRecorder.");
            return REENCODE_ERROR;
        }
    }

    // Input setup
    if (hls && (hlsStreamID != -1))
        inRingBuffer = RingBuffer::Create(hls->GetSourceFile(), false, false);
    else
        inRingBuffer = RingBuffer::Create(inputname, false, false);

    player = new MythPlayer(kVideoIsNull);

    player_ctx = new PlayerContext(kTranscoderInUseID);
    player_ctx->SetPlayingInfo(m_proginfo);
    player_ctx->SetRingBuffer(inRingBuffer);
    player_ctx->SetPlayer(player);
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

    if (player->OpenFile() < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Transcoding aborted, error opening file.");
        if (player_ctx)
            delete player_ctx;
        return REENCODE_ERROR;
    }

    if (AudioTrackNo > -1)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Set audiotrack number to %1").arg(AudioTrackNo));
        player->GetDecoder()->SetTrack(kTrackTypeAudio, AudioTrackNo);
    }

    long long total_frame_count = player->GetTotalFrameCount();
    long long new_frame_count = total_frame_count;
    if (honorCutList && m_proginfo)
    {
        LOG(VB_GENERAL, LOG_INFO, "Honoring the cutlist while transcoding");

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
        LOG(VB_GENERAL, LOG_INFO, QString("Cutlist        : %1").arg(cutStr));
        LOG(VB_GENERAL, LOG_INFO, QString("Original Length: %1 frames")
                .arg((long)total_frame_count));
        LOG(VB_GENERAL, LOG_INFO, QString("New Length     : %1 frames")
                .arg((long)new_frame_count));

        if ((m_proginfo->QueryIsEditing()) ||
            (JobQueue::IsJobRunning(JOB_COMMFLAG, *m_proginfo)))
        {
            LOG(VB_GENERAL, LOG_INFO, "Transcoding aborted, cutlist changed");
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
       LOG(VB_GENERAL, LOG_NOTICE,
           "Found video height of 1088.  This is unusual and "
           "more than likely the video is actually 1080 so mythtranscode "
           "will treat it as such.");
    }

    DecoderBase* dec = player->GetDecoder();
    float video_aspect = dec ? dec->GetVideoAspect() : 4.0f / 3.0f;
    float video_frame_rate = player->GetFrameRate();
    int newWidth = video_width;
    int newHeight = video_height;
    bool halfFramerate = false;
    bool skippedLastFrame = false;

    kfa_table = new vector<struct kfatable_entry>;

    if (avfMode)
    {
        newWidth = cmdWidth;
        newHeight = cmdHeight;

        // TODO: is this necessary?  It got commented out, but may still be
        // needed.
        // int actualHeight = (video_height == 1088 ? 1080 : video_height);

        // If height or width are 0, then we need to calculate them
        if (newHeight == 0 && newWidth > 0)
            newHeight = (int)(1.0 * newWidth / video_aspect);
        else if (newWidth == 0 && newHeight > 0)
            newWidth = (int)(1.0 * newHeight * video_aspect);
        else if (newWidth == 0 && newHeight == 0)
        {
            newHeight = 480;
            newWidth = (int)(1.0 * 480 * video_aspect);
            if (newWidth > 640)
            {
                newWidth = 640;
                newHeight = (int)(1.0 * 640 / video_aspect);
            }
        }

        // make sure dimensions are valid for MPEG codecs
        newHeight = (newHeight + 15) & ~0xF;
        newWidth  = (newWidth  + 15) & ~0xF;

        avfw = new AVFormatWriter();
        if (!avfw)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Transcoding aborted, error creating AVFormatWriter.");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        avfw->SetVideoBitrate(cmdBitrate);
        avfw->SetHeight(newHeight);
        avfw->SetWidth(newWidth);
        avfw->SetAspect(video_aspect);
        avfw->SetAudioBitrate(cmdAudioBitrate);
        avfw->SetAudioChannels(arb->m_channels);
        avfw->SetAudioBits(16);
        avfw->SetAudioSampleRate(arb->m_eff_audiorate);
        avfw->SetAudioSampleBytes(2);

        if (hlsMode)
        {
            int segmentSize = 10;
            int audioOnlyBitrate = 0;

            if (!hlsDisableAudioOnly)
            {
                audioOnlyBitrate = 48000;

                avfw2 = new AVFormatWriter();
                if (!avfw2)
                {
                    LOG(VB_GENERAL, LOG_ERR, "Transcoding aborted, error "
                        "creating low-bitrate AVFormatWriter.");
                    if (player_ctx)
                        delete player_ctx;
                    return REENCODE_ERROR;
                }

                avfw2->SetContainer("mpegts");
                avfw2->SetAudioCodec("libmp3lame");
                avfw2->SetAudioBitrate(audioOnlyBitrate);
                avfw2->SetAudioChannels(arb->m_channels);
                avfw2->SetAudioBits(16);
                avfw2->SetAudioSampleRate(arb->m_eff_audiorate);
                avfw2->SetAudioSampleBytes(2);
            }

            avfw->SetContainer("mpegts");
            avfw->SetVideoCodec("libx264");
            avfw->SetAudioCodec("libmp3lame");
            //avfw->SetAudioCodec("libfaac");

            if (hlsStreamID == -1)
                hls = new HTTPLiveStream(inputname, newWidth, newHeight,
                                         cmdBitrate,
                                         cmdAudioBitrate, hlsMaxSegments,
                                         segmentSize, audioOnlyBitrate);

            hls->UpdateStatus(kHLSStatusStarting);
            hls->UpdateSizeInfo(newWidth, newHeight, video_width, video_height);

            if ((hlsStreamID != -1) &&
                (!hls->InitForWrite()))
            {
                LOG(VB_GENERAL, LOG_ERR, "hls->InitForWrite() failed");
                if (player_ctx)
                    delete player_ctx;
                return REENCODE_ERROR;
            }

            if (video_frame_rate > 30)
            {
                halfFramerate = true;
                avfw->SetFramerate(video_frame_rate/2);

                if (avfw2)
                    avfw2->SetFramerate(video_frame_rate/2);

                hlsSegmentSize = (int)(segmentSize * video_frame_rate / 2);
            }
            else
            {
                avfw->SetFramerate(video_frame_rate);

                if (avfw2)
                    avfw2->SetFramerate(video_frame_rate);

                hlsSegmentSize = (int)(segmentSize * video_frame_rate);
            }

            avfw->SetKeyFrameDist(90);
            if (avfw2)
                avfw2->SetKeyFrameDist(90);

            hls->AddSegment();
            avfw->SetFilename(hls->GetCurrentFilename());
            if (avfw2)
                avfw2->SetFilename(hls->GetCurrentFilename(true));
        }
        else
        {
            avfw->SetContainer(cmdContainer);
            avfw->SetVideoCodec(cmdVideoCodec);
            avfw->SetAudioCodec(cmdAudioCodec);
            avfw->SetFilename(outputname);
            avfw->SetFramerate(video_frame_rate);
        }

        avfw->SetThreadCount(
            gCoreContext->GetNumSetting("HTTPLiveStreamThreads", 2));

        if (avfw2)
            avfw2->SetThreadCount(1);

        if (!avfw->Init())
        {
            LOG(VB_GENERAL, LOG_ERR, "avfw->Init() failed");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        if (!avfw->OpenFile())
        {
            LOG(VB_GENERAL, LOG_ERR, "avfw->OpenFile() failed");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        if (avfw2 && !avfw2->Init())
        {
            LOG(VB_GENERAL, LOG_ERR, "avfw2->Init() failed");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        if (avfw2 && !avfw2->OpenFile())
        {
            LOG(VB_GENERAL, LOG_ERR, "avfw2->OpenFile() failed");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        arb->m_audioFrameSize = avfw->GetAudioFrameSize() * arb->m_channels * 2;

        player->SetVideoFilters(
            gCoreContext->GetSetting("HTTPLiveStreamFilters"));
    }
    else if (fifodir.isEmpty())
    {
        if (!GetProfile(profileName, encodingType, video_height,
                        (int)round(video_frame_rate))) {
            LOG(VB_GENERAL, LOG_ERR, "Transcoding aborted, no profile found.");
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
            LOG(VB_GENERAL, LOG_NOTICE, "Switching to MPEG-2 transcoder.");
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

            LOG(VB_GENERAL, LOG_INFO, QString("Resizing from %1x%2 to %3x%4")
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
            LOG(VB_GENERAL, LOG_ERR, "No video information found!");
            LOG(VB_GENERAL, LOG_ERR, "Please ensure that recording profiles "
                                     "for the transcoder are set");
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unknown video codec: %1").arg(vidsetting));
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }

        nvr->SetOption("samplerate", arb->m_eff_audiorate);
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
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unknown audio codec: %1").arg(audsetting));
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

                LOG(VB_GENERAL, LOG_NOTICE,
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

    if (vidsetting == encodingType && !framecontrol && !avfMode &&
        fifodir.isEmpty() && honorCutList &&
        video_width == newWidth && video_height == newHeight)
    {
        copyvideo = true;
        LOG(VB_GENERAL, LOG_INFO, "Reencoding video in 'raw' mode");
    }

    if (honorCutList && deleteMap.size() > 0)
    {
        if (cleanCut)
        {
            // Have the player seek only part of the way
            // through a cut, and then use the cutter to
            // discard the rest
            cutter = new Cutter();
            cutter->SetCutList(deleteMap);

            player->SetCutList(cutter->AdjustedCutList());
        }
        else
        {
            // Have the player apply the cut list
            player->SetCutList(deleteMap);
        }
    }

    player->InitForTranscode(copyaudio, copyvideo);
    if (player->IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Unable to initialize MythPlayer for Transcode");
        if (player_ctx)
            delete player_ctx;
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

    if (!fifodir.isEmpty())
    {
        AudioPlayer *aplayer = player->GetAudio();
        const char  *audio_codec_name;

        switch(aplayer->GetCodec())
        {
            case CODEC_ID_AC3:
                audio_codec_name = "ac3";
                break;
            case CODEC_ID_EAC3:
                audio_codec_name = "eac3";
                break;
            case CODEC_ID_DTS:
                audio_codec_name = "dts";
                break;
            case CODEC_ID_TRUEHD:
                audio_codec_name = "truehd";
                break;
            case CODEC_ID_MP3:
                audio_codec_name = "mp3";
                break;
            case CODEC_ID_MP2:
                audio_codec_name = "mp2";
                break;
            default:
                audio_codec_name = "unknown";
        }

        if (!arb->m_passthru)
            audio_codec_name = "raw";

        // If cutlist is used then get info on first uncut frame
        if (honorCutList && fifo_info)
        {
            bool is_key;
            int did_ff;
            frm_dir_map_t::iterator dm_iter;
            player->TranscodeGetNextFrame(dm_iter, did_ff, is_key, true);

            QSize buf_size = player->GetVideoBufferSize();
            video_width = buf_size.width();
            video_height = buf_size.height();
            video_aspect = player->GetVideoAspect();
            video_frame_rate = player->GetFrameRate();
        }

        // Display details of the format of the fifo data.
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoVideoWidth %1").arg(video_width));
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoVideoHeight %1").arg(video_height));
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoVideoAspectRatio %1").arg(video_aspect));
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoVideoFrameRate %1").arg(video_frame_rate));
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoAudioFormat %1").arg(audio_codec_name));
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoAudioChannels %1").arg(arb->m_channels));
        LOG(VB_GENERAL, LOG_INFO,
            QString("FifoAudioSampleRate %1").arg(arb->m_eff_audiorate));

        if(fifo_info)
        {
            // Request was for just the format of fifo data, not for
            // the actual transcode, so stop here.
            unlink(outputname.toLocal8Bit().constData());
            if (player_ctx)
                delete player_ctx;
            return REENCODE_OK;
        }

        QString audfifo = fifodir + QString("/audout");
        QString vidfifo = fifodir + QString("/vidout");
        int audio_size = arb->m_eff_audiorate * arb->m_bytes_per_frame;
        // framecontrol is true if we want to enforce fifo sync.
        if (framecontrol)
            LOG(VB_GENERAL, LOG_INFO, "Enforcing sync on fifos");
        fifow = new FIFOWriter(2, framecontrol);

        if (!fifow->FIFOInit(0, QString("video"), vidfifo, vidSize, 50) ||
            !fifow->FIFOInit(1, QString("audio"), audfifo, audio_size, 25))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Error initializing fifo writer.  Aborting");
            unlink(outputname.toLocal8Bit().constData());
            if (player_ctx)
                delete player_ctx;
            return REENCODE_ERROR;
        }
        LOG(VB_GENERAL, LOG_INFO,
            QString("Video %1x%2@%3fps Audio rate: %4")
                .arg(video_width).arg(video_height)
                .arg(video_frame_rate)
                .arg(arb->m_eff_audiorate));
        LOG(VB_GENERAL, LOG_INFO, "Created fifos. Waiting for connection.");
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

    float rateTimeConv = arb->m_eff_audiorate / 1000.0f;
    float vidFrameTime = 1000.0f / video_frame_rate;
    int wait_recover = 0;
    VideoOutput *videoOutput = player->GetVideoOutput();
    bool is_key = 0;
    bool first_loop = true;
    unsigned char *newFrame = new unsigned char[frame.size];
    frame.buf = newFrame;
    AVPicture imageIn, imageOut;
    struct SwsContext  *scontext = NULL;

    if (fifow)
        LOG(VB_GENERAL, LOG_INFO, "Dumping Video and Audio data to fifos");
    else if (copyaudio)
        LOG(VB_GENERAL, LOG_INFO, "Copying Audio while transcoding Video");
    else if (hlsMode)
        LOG(VB_GENERAL, LOG_INFO, "Transcoding for HTTP Live Streaming");
    else if (avfMode)
        LOG(VB_GENERAL, LOG_INFO, "Transcoding to libavformat container");
    else
        LOG(VB_GENERAL, LOG_INFO, "Transcoding Video and Audio");

    TranscodeFrameQueue *frameQueue =
        new TranscodeFrameQueue(player, videoOutput, honorCutList);
    MThreadPool::globalInstance()->start(frameQueue, "TranscodeFrameQueue");

    QTime flagTime;
    flagTime.start();

    if (cutter)
        cutter->Activate(vidFrameTime * rateTimeConv, total_frame_count);

    bool stopSignalled = false;
    VideoFrame *lastDecode = NULL;

    if (hls)
        hls->UpdateStatus(kHLSStatusRunning);

    while ((!stopSignalled) &&
           (lastDecode = frameQueue->GetFrame(did_ff, is_key)))
    {
        if (first_loop)
        {
            copyaudio = player->GetRawAudioState();
            first_loop = false;
        }

        float new_aspect = lastDecode->aspect;

        if (cutter)
            cutter->NewFrame(lastDecode->frameNumber);

        frame.timecode = lastDecode->timecode;

        if (frame.timecode < lasttimecode)
            frame.timecode = (long long)(lasttimecode + vidFrameTime);

        if (fifow)
        {
            frame.buf = lastDecode->buf;
            totalAudio += arb->GetSamples(frame.timecode);
            int audbufTime = (int)(totalAudio / rateTimeConv);
            int auddelta = frame.timecode - audbufTime;
            int vidTime = (int)(curFrameNum * vidFrameTime + 0.5);
            int viddelta = frame.timecode - vidTime;
            int delta = viddelta - auddelta;
            if (abs(delta) < 500 && abs(delta) >= vidFrameTime)
            {
               QString msg = QString("Audio is %1ms %2 video at # %3: "
                                     "auddelta=%4, viddelta=%5")
                   .arg(abs(delta))
                   .arg(((delta > 0) ? "ahead of" : "behind"))
                   .arg((int)curFrameNum)
                   .arg(auddelta)
                   .arg(viddelta);
                LOG(VB_GENERAL, LOG_INFO, msg);
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
                        if (!cutter || !cutter->InhibitDummyFrame())
                            fifow->FIFOWrite(0, frame.buf, vidSize);

                        count++;
                        delta -= (int)vidFrameTime;
                    }
                    QString msg = QString("Added %1 blank video frames")
                                  .arg(count);
                    LOG(VB_GENERAL, LOG_INFO, msg);
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

#if 0
            int buflen = (int)(arb->audiobuffer_len / rateTimeConv);
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("%1: video time: %2 audio time: %3 "
                        "buf: %4 exp: %5 delta: %6")
                    .arg(curFrameNum) .arg(frame.timecode)
                    .arg(arb->last_audiotime) .arg(buflen) .arg(audbufTime)
                    .arg(delta));
#endif
            while (arb->GetCount(frame.timecode))
            {
                AudioBuffer *ab = arb->GetData();

                if (!cutter ||
                    !cutter->InhibitUseAudioFrames(1, &totalAudio))
                    fifow->FIFOWrite(1, ab->m_buffer.data(),
                                     ab->m_buffer.size());

                delete ab;
            }

            if (dropvideo < 0)
            {
                if (cutter && cutter->InhibitDropFrame())
                    fifow->FIFOWrite(0, frame.buf, vidSize);

                LOG(VB_GENERAL, LOG_INFO, "Dropping video frame");
                dropvideo++;
                curFrameNum--;
            }
            else
            {
                if (!cutter || !cutter->InhibitUseVideoFrame())
                    fifow->FIFOWrite(0, frame.buf, vidSize);

                if (dropvideo)
                {
                    if (!cutter || !cutter->InhibitDummyFrame())
                        fifow->FIFOWrite(0, frame.buf, vidSize);

                    curFrameNum++;
                    dropvideo--;
                }
            }
            videoOutput->DoneDisplayingFrame(lastDecode);
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
                LOG(VB_GENERAL, LOG_ERR, "Transcoding aborted, MythPlayer "
                                         "is not in raw audio mode.");

                unlink(outputname.toLocal8Bit().constData());
                delete [] newFrame;
                if (player_ctx)
                    delete player_ctx;
                if (frameQueue)
                    frameQueue->stop();
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
                    long sync_offset;
                    sync_offset = player->UpdateStoredFrameNum(curFrameNum);
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
                if (video_aspect != new_aspect)
                {
                    video_aspect = new_aspect;
                    nvr->SetNewVideoParams(video_aspect);
                }

                QSize buf_size = player->GetVideoBufferSize();

                if (video_width != buf_size.width() ||
                    video_height != buf_size.height())
                {
                    video_width = buf_size.width();
                    video_height = buf_size.height();

                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Resizing from %1x%2 to %3x%4")
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

            if (video_aspect != new_aspect)
            {
                video_aspect = new_aspect;
                if (nvr)
                    nvr->SetNewVideoParams(video_aspect);
            }


            QSize buf_size = player->GetVideoBufferSize();

            if (video_width != buf_size.width() ||
                video_height != buf_size.height())
            {
                video_width = buf_size.width();
                video_height = buf_size.height();

                LOG(VB_GENERAL, LOG_INFO,
                    QString("Resizing from %1x%2 to %3x%4")
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
            if (arb->GetCount(frame.timecode))
            {
                int loop = 0;
                int bytesConsumed = 0;
                int buffersConsumed = 0;
                int count = arb->GetCount(frame.timecode);
                for (loop = 0; loop < count; loop++)
                {
                    AudioBuffer *ab = arb->GetData();
                    unsigned char *buf = (unsigned char *)ab->m_buffer.data();
                    if (avfMode)
                    {
                        if (did_ff != 1)
                        {
                            avfw->WriteAudioFrame(buf, audioFrame,
                                                  ab->m_time - timecodeOffset);

                            if (avfw2)
                            {
                                if ((avfw2->GetTimecodeOffset() == -1) &&
                                    (avfw->GetTimecodeOffset() != -1))
                                {
                                    avfw2->SetTimecodeOffset(
                                        avfw->GetTimecodeOffset());
                                }

                                avfw2->WriteAudioFrame(buf, audioFrame,
                                                   ab->m_time - timecodeOffset);
                            }

                            ++audioFrame;
                        }
                    }
                    else
                    {
                        nvr->SetOption("audioframesize", ab->m_buffer.size());
                        nvr->WriteAudio(buf, audioFrame++,
                                        ab->m_time - timecodeOffset);
                        if (nvr->IsErrored())
                        {
                            LOG(VB_GENERAL, LOG_ERR,
                                "Transcode: Encountered irrecoverable error in "
                                "NVR::WriteAudio");

                            delete [] newFrame;
                            if (player_ctx)
                                delete player_ctx;
                            if (frameQueue)
                                frameQueue->stop();
                            delete ab;
                            return REENCODE_ERROR;
                        }
                    }

                    ++buffersConsumed;
                    bytesConsumed += ab->m_buffer.size();

                    delete ab;
                }
            }

            if (!avfMode)
                player->GetCC608Reader()->
                    TranscodeWriteText(&TranscodeWriteText, (void *)(nvr));
            lasttimecode = frame.timecode;
            frame.timecode -= timecodeOffset;

            if (avfMode)
            {
                if (halfFramerate && !skippedLastFrame)
                {
                    skippedLastFrame = true;
                }
                else
                {
                    skippedLastFrame = false;

                    if ((hls) &&
                        (avfw->GetFramesWritten()) &&
                        (hlsSegmentFrames > hlsSegmentSize) &&
                        (avfw->NextFrameIsKeyFrame()))
                    {
                        hls->AddSegment();
                        avfw->ReOpen(hls->GetCurrentFilename());

                        if (avfw2)
                            avfw2->ReOpen(hls->GetCurrentFilename(true));

                        hlsSegmentFrames = 0;
                    }

                    avfw->WriteVideoFrame(&frame);
                    ++hlsSegmentFrames;
                }
            }
            else
            {
                if (forceKeyFrames)
                    nvr->WriteVideo(&frame, true, true);
                else
                    nvr->WriteVideo(&frame);
            }
        }
        if (QDateTime::currentDateTime() > statustime)
        {
            if (showprogress)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Processed: %1 of %2 frames(%3 seconds)").
                        arg((long)curFrameNum).arg((long)total_frame_count).
                        arg((long)(curFrameNum / video_frame_rate)));
            }

            if (hls && hls->CheckStop())
            {
                hls->UpdateStatus(kHLSStatusStopping);
                stopSignalled = true;
            }

            statustime = QDateTime::currentDateTime();
            statustime = statustime.addSecs(5);
        }
        if (QDateTime::currentDateTime() > curtime)
        {
            if (honorCutList && m_proginfo && !hls &&
                m_proginfo->QueryMarkupFlag(MARK_UPDATED_CUT))
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Transcoding aborted, cutlist updated");

                unlink(outputname.toLocal8Bit().constData());
                delete [] newFrame;
                if (player_ctx)
                    delete player_ctx;
                if (frameQueue)
                    frameQueue->stop();
                return REENCODE_CUTLIST_CHANGE;
            }

            if ((jobID >= 0) || (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_INFO)))
            {
                if (JobQueue::GetJobCmd(jobID) == JOB_STOP)
                {
                    LOG(VB_GENERAL, LOG_NOTICE,
                        "Transcoding STOPped by JobQueue");

                    unlink(outputname.toLocal8Bit().constData());
                    delete [] newFrame;
                    if (player_ctx)
                        delete player_ctx;
                    if (frameQueue)
                        frameQueue->stop();
                    return REENCODE_STOPPED;
                }

                float flagFPS = 0.0;
                float elapsed = flagTime.elapsed() / 1000.0;
                if (elapsed)
                    flagFPS = curFrameNum / elapsed;

                int percentage = curFrameNum * 100 / total_frame_count;

                if (hls)
                    hls->UpdatePercentComplete(percentage);

                if (jobID >= 0)
                    JobQueue::ChangeJobComment(jobID,
                              QObject::tr("%1% Completed @ %2 fps.")
                                          .arg(percentage).arg(flagFPS));
                else
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("mythtranscode: %1% Completed @ %2 fps.")
                            .arg(percentage).arg(flagFPS));

            }
            curtime = QDateTime::currentDateTime();
            curtime = curtime.addSecs(20);
        }

        curFrameNum++;
        frame.frameNumber = 1 + (curFrameNum << 1);

        player->DiscardVideoFrame(lastDecode);
    }

    sws_freeContext(scontext);

    if (!fifow)
    {
        if (avfw)
            avfw->CloseFile();

        if (avfw2)
            avfw2->CloseFile();

        if (!hls && m_proginfo)
        {
            m_proginfo->ClearPositionMap(MARK_KEYFRAME);
            m_proginfo->ClearPositionMap(MARK_GOP_START);
            m_proginfo->ClearPositionMap(MARK_GOP_BYFRAME);
        }

        if (nvr)
        {
            nvr->WriteSeekTable();
            if (!kfa_table->empty())
                nvr->WriteKeyFrameAdjustTable(*kfa_table);
        }
    } else {
        fifow->FIFODrain();
    }

    if (cutter)
        delete cutter;

    if (avfw)
        delete avfw;

    if (avfw2)
        delete avfw2;

    if (hls)
    {
        if (!stopSignalled)
        {
            hls->UpdateStatus(kHLSStatusCompleted);
            hls->UpdateStatusMessage("Transcoding Completed");
            hls->UpdatePercentComplete(100);
        }
        else
        {
            hls->UpdateStatus(kHLSStatusStopped);
            hls->UpdateStatusMessage("Transcoding Stopped");
        }
        delete hls;
    }

    if (frameQueue)
        frameQueue->stop();

    delete [] newFrame;
    if (player_ctx)
        delete player_ctx;
    return REENCODE_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

