// -*- Mode: c++ -*-
#ifndef RECORDERBASE_H_
#define RECORDERBASE_H_

#include <cstdint>

#include <QWaitCondition>
#include <QAtomicInt>
#include <QDateTime>
#include <QRunnable>
#include <QString>
#include <QMutex>
#include <QMap>

#include "libmythbase/mythtimer.h"
#include "libmythbase/programtypes.h" // for MarkTypes, frm_pos_map_t

#include "libmythtv/mythtvexp.h"
#include "libmythtv/mythavrational.h"
#include "libmythtv/recordingfile.h"
#include "libmythtv/recordingquality.h"
#include "libmythtv/scantype.h"

extern "C"
{
#include "libavcodec/avcodec.h" // for Video/Audio codec enums
}

class FireWireDBOptions;
class GeneralDBOptions;
class RecordingProfile;
class RecordingInfo;
class DVBDBOptions;
class RecorderBase;
class ChannelBase;
class MythMediaBuffer;
class TVRec;

/** \class RecorderBase
 *  \brief This is the abstract base class for supporting
 *         recorder hardware.
 *
 *  For a digital streams specialization, see the DTVRecorder.
 *  For a specialization for MPEG hardware encoded analog streams,
 *  see MpegRecorder.
 *  For a specialization for software encoding of frame grabber
 *  recorders, see NuppelVideoRecorder.
 *
 *  \sa TVRec
 */
class MTV_PUBLIC RecorderBase : public QRunnable
{
    friend class Transcode; // for access to SetIntOption(), SetStrOption()

  public:
    explicit RecorderBase(TVRec *rec);
    ~RecorderBase() override;

    /// \brief Sets the video frame rate.
    void SetFrameRate(double rate)
    {
        m_videoFrameRate = rate;
        m_ntscFrameRate = (29.96 <= rate && 29.98 >= rate);
        m_frameRate = MythAVRational(lround(rate * 100), 100);
    }

    /** \brief Changes the Recording from the one set initially with
     *         SetOptionsFromProfile().
     *
     *   This method is useful for LiveTV, when we do not want to
     *   pause the recorder for a SetOptionsFromProfile() call just
     *   because a new program is comming on.
     *
     *  \sa ChannelNameChanged(const QString&)
     */
    void SetRecording(const RecordingInfo *pginfo);

    /** \brief Tells recorder to use an externally created ringbuffer.
     *
     *   If this an external RingBuffer is set, it should be before any
     *   Initialize(), Open(), or StartRecorder() calls. Externally created
     *   RingBuffers are not deleted in the Recorder's destructor.
     */
    void SetRingBuffer(MythMediaBuffer *Buffer);

    /** \brief Set an specific option.
     *
     *   Base options include: codec, videodevice,
     *   tvformat&nbsp;(ntsc,ntsc-jp,pal-m),
     *   vbiformat&nbsp;("none","pal teletext","ntsc").
     */
    virtual void SetOption(const QString &name, const QString &value);

    /** \brief Set an specific integer option.
     *
     *   There are no integer options in RecorderBase.
     */
    virtual void SetOption(const QString &name, int value);

    /** \brief Set an specific boolean option.
     *
     *   This is a helper function to enforce type checking.
     */
    void SetBoolOption(const QString &name, bool value)
        { SetOption(name, static_cast<int>(value)); }

    /** \brief Tells recorder which filters to use.
     *
     *   These filters are used by frame grabber encoders to lower
     *   the bitrate while keeping quality good. They must execute
     *   quickly so that frames are not lost by the recorder.
     */
    virtual void SetVideoFilters(QString &filters) = 0;

    /** \brief Sets basic recorder options.
     *
     *   SetOptionsFromProfile is used to tell the recorder
     *   about the recording profile as well as the devices
     *   to use.
     */
    virtual void SetOptionsFromProfile(RecordingProfile *profile,
                                       const QString &videodev,
                                       const QString &audiodev,
                                       const QString &vbidev) = 0;

    /** \brief Sets next recording info, to be applied as soon as practical.
     *
     *   This should not lose any frames on the switchover, and should
     *   initialize the RingBuffer stream with headers as appropriate.
     *
     *   The switch does not have to happen immediately,
     *   but should happen soon. (i.e. it can wait for a key frame..)
     *
     *   This calls TVRec::RingBufferChanged() when the switch happens.
     */
    void SetNextRecording(const RecordingInfo *ri, MythMediaBuffer *Buffer);

    /** \brief This is called between SetOptionsFromProfile() and
     *         run() to initialize any devices, etc.
     */
    virtual void Initialize(void) = 0;

    /** \brief run() starts the recording process, and does not
     *         exit until the recording is complete.
     *  \sa StopRecording()
     */
    void run(void) override = 0; // QRunnable

    /** \brief Reset the recorder to the startup state.
     *
     *   This is used after Pause(bool), WaitForPause() and
     *   after the RingBuffer's StopReads() method has been called.
     */
    virtual void Reset(void) = 0;

    /// \brief Tells us whether an unrecoverable error has been encountered.
    virtual bool IsErrored(void) = 0;

    /** \brief Returns number of frames written to disk.
     *
     *   It is not always safe to seek up to this frame in a player
     *   because frames may not be written in display order.
     */
    virtual long long GetFramesWritten(void) = 0;

    /** \brief Returns file descriptor of recorder device.
     *
     *   This is used by channel when only one open file descriptor
     *   is allowed on a device node. This is the case with
     *   video4linux devices and similar devices in BSD.
     *   It is not needed by newer drivers, such as those
     *   used for DVB.
     */
    virtual int GetVideoFd(void) = 0;

    /** \brief Returns closest keyframe position before the desired frame.
     *
     *   This returns -1 if a keyframe position cannot be found
     *   for a frame. This could be true if the keyframe has not
     *   yet been seen by the recorder(unlikely), or if a keyframe
     *   map does not exist or is not up to date. The latter can
     *   happen because the video is an external video, because
     *   the database is corrupted, or because this is a live
     *   recording and it is being read by a remote frontend
     *   faster than the keyframes can be saved to the database.
     *
     *  \return Closest prior keyframe, or -1 if there is no prior
     *          known keyframe.
     */
    long long GetKeyframePosition(long long desired) const;
    bool GetKeyframePositions(
        long long start, long long end, frm_pos_map_t &map) const;
    bool GetKeyframeDurations(
        long long start, long long end, frm_pos_map_t &map) const;

    virtual void StopRecording(void);
    virtual bool IsRecording(void);
    virtual bool IsRecordingRequested(void);

    /// \brief Returns a report about the current recordings quality.
    virtual RecordingQuality *GetRecordingQuality(const RecordingInfo *ri) const;

    // pausing interface
    virtual void Pause(bool clear = true);
    virtual void Unpause(void);
    virtual bool IsPaused(bool holding_lock = false) const;
    virtual bool WaitForPause(std::chrono::milliseconds timeout = 1s);

    /** \brief Returns the latest frame rate.
     */
    double GetFrameRate(void) const { return m_frameRate.toDouble(); }

    /** \brief If requested, switch to new RingBuffer/ProgramInfo objects
     */
    virtual bool CheckForRingBufferSwitch(void);

    /** \brief Save the seektable to the DB
     */
    void SavePositionMap(bool force = false, bool finished = false);

    enum AspectRatio {
        ASPECT_UNKNOWN       = 0x00,
        ASPECT_1_1           = 0x01,
        ASPECT_4_3           = 0x02,
        ASPECT_16_9          = 0x03,
        ASPECT_2_21_1        = 0x04,
        ASPECT_CUSTOM        = 0x05,
    };

    static RecorderBase *CreateRecorder(
        TVRec                  *tvrec,
        ChannelBase            *channel,
        RecordingProfile       &profile,
        const GeneralDBOptions &genOpt);

  protected:
    /** \brief Convenience function used to set integer options from a profile.
     *  \sa SetOption(const QString&, int)
     */
    void SetIntOption(RecordingProfile *profile, const QString &name);
    /** \brief Convenience function used to set QString options from a profile.
     *  \sa SetOption(const QString&, const QString&)
     */
    void SetStrOption(RecordingProfile *profile, const QString &name);
    virtual bool PauseAndWait(std::chrono::milliseconds timeout = 100ms);

    virtual void ResetForNewFile(void) = 0;
    virtual void SetRecordingStatus(RecStatus::Type status,
                                    const QString& file, int line);
    virtual void ClearStatistics(void);
    virtual void FinishRecording(void);
    virtual void StartNewFile(void) { }

    /** \brief Set seektable type
     */
    void SetPositionMapType(MarkTypes type) { m_positionMapType = type; }

    /** \brief Note a change in aspect ratio in the recordedmark table
     */
    void AspectChange(uint aspect, long long frame);

    /** \brief Note a change in video size in the recordedmark table
     */
    void ResolutionChange(uint width, uint height, long long frame);

    /** \brief Note a change in video frame rate in the recordedmark table
     */
    void FrameRateChange(uint framerate, uint64_t frame);

    /** \brief Note a change in video scan type in the recordedmark table
     */
    void VideoScanChange(SCAN_t scan, uint64_t frame);

    /** \brief Note a change in video codec
     */
    void VideoCodecChange(AVCodecID vCodec);

    /** \brief Note a change in audio codec
     */
    void AudioCodecChange(AVCodecID aCodec);

    /** \brief Note the total duration in the recordedmark table
     */
    void SetDuration(std::chrono::milliseconds duration);

    /** \brief Note the total frames in the recordedmark table
     */
    void SetTotalFrames(uint64_t total_frames);

    void TryWriteProgStartMark(const frm_pos_map_t &durationDeltaCopy);

    TVRec         *m_tvrec                {nullptr};
    MythMediaBuffer *m_ringBuffer         {nullptr};
    bool           m_weMadeBuffer         {true};

    AVContainer    m_containerFormat      {formatUnknown};
    AVCodecID      m_primaryVideoCodec    {AV_CODEC_ID_NONE};
    AVCodecID      m_primaryAudioCodec    {AV_CODEC_ID_NONE};
    QString        m_videocodec           {"rtjpeg"};
    QString        m_videodevice;

    bool           m_ntsc                 {true};
    bool           m_ntscFrameRate        {true};
    double         m_videoFrameRate       {29.97};

    uint           m_videoAspect          {0}; // AspectRatio (1 = 4:3, 2 = 16:9

    uint           m_videoHeight          {0};
    uint           m_videoWidth           {0};
    MythAVRational m_frameRate            {0};

    RecordingInfo *m_curRecording         {nullptr};

    // For handling pausing + stop recording
    mutable QMutex m_pauseLock; // also used for request_recording and recording
    bool           m_requestPause         {false};
    bool           m_paused               {false};
    QWaitCondition m_pauseWait;
    QWaitCondition m_unpauseWait;
    /// True if API call has requested a recording be [re]started
    bool           m_requestRecording     {false};
    /// True while recording is actually being performed
    bool           m_recording            {false};
    QWaitCondition m_recordingWait;


    // For RingBuffer switching
    QMutex         m_nextRingBufferLock;
    MythMediaBuffer    *m_nextRingBuffer       {nullptr};
    RecordingInfo *m_nextRecording        {nullptr};
    MythTimer      m_ringBufferCheckTimer;

    // Seektable  support
    MarkTypes      m_positionMapType      {MARK_GOP_BYFRAME};
    mutable QMutex m_positionMapLock;
    frm_pos_map_t  m_positionMap;
    frm_pos_map_t  m_positionMapDelta;
    frm_pos_map_t  m_durationMap;
    frm_pos_map_t  m_durationMapDelta;
    MythTimer      m_positionMapTimer;

    // ProgStart mark support
    qint64         m_estimatedProgStartMS {0};
    long long      m_lastSavedKeyframe    {0};
    long long      m_lastSavedDuration    {0};

    // Statistics
    // Note: Once we enter RecorderBase::run(), only that thread can
    // update these values safely. These values are read in that thread
    // without locking and updated with the lock held. Outside that
    // thread these values are only read, and only with the lock held.
    mutable QMutex m_statisticsLock;
    QAtomicInt     m_timeOfFirstDataIsSet; // doesn't need locking
    QDateTime      m_timeOfFirstData;
    QAtomicInt     m_timeOfLatestDataCount; // doesn't need locking
    QAtomicInt     m_timeOfLatestDataPacketInterval; // doesn't need locking
    QDateTime      m_timeOfLatestData;
    MythTimer      m_timeOfLatestDataTimer;
    RecordingGaps  m_recordingGaps;
    /// timeOfLatest update interval target in milliseconds.
    static constexpr std::chrono::milliseconds kTimeOfLatestDataIntervalTarget { 5s };
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
