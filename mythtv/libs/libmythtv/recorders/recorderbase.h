// -*- Mode: c++ -*-
#ifndef RECORDERBASE_H_
#define RECORDERBASE_H_

#include <stdint.h>

#include <QWaitCondition>
#include <QAtomicInt>
#include <QDateTime>
#include <QRunnable>
#include <QString>
#include <QMutex>
#include <QMap>

#include "recordingquality.h"
#include "programtypes.h" // for MarkTypes, frm_pos_map_t
#include "mythtimer.h"
#include "mythtvexp.h"

class FireWireDBOptions;
class GeneralDBOptions;
class RecordingProfile;
class RecordingInfo;
class DVBDBOptions;
class RecorderBase;
class ChannelBase;
class RingBuffer;
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
    RecorderBase(TVRec *rec);
    virtual ~RecorderBase();

    /// \brief Sets the video frame rate.
    void SetFrameRate(double rate)
    {
        video_frame_rate = rate;
        ntsc_framerate = (29.96 <= rate && 29.98 >= rate);
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
    void SetRingBuffer(RingBuffer *rbuf);

    /** \brief Set an specific option.
     *
     *   Base options include: codec, videodevice,
     *   tvformat&nbsp;(ntsc,ntsc-jp,pal-m),
     *   vbiformat&nbsp;("none","pal teletext","ntsc").
     */
    virtual void SetOption(const QString &opt, const QString &value);

    /** \brief Set an specific integer option.
     *
     *   There are no integer options in RecorderBase.
     */
    virtual void SetOption(const QString &opt, int value);

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
    void SetNextRecording(const RecordingInfo*, RingBuffer*);

    /** \brief This is called between SetOptionsFromProfile() and
     *         run() to initialize any devices, etc.
     */
    virtual void Initialize(void) = 0;

    /** \brief run() starts the recording process, and does not
     *         exit until the recording is complete.
     *  \sa StopRecording()
     */
    virtual void run(void) = 0;

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
    int64_t GetKeyframePosition(uint64_t desired) const;
    bool GetKeyframePositions(
        int64_t start, int64_t end, frm_pos_map_t&) const;
    bool GetKeyframeDurations(
        int64_t start, int64_t end, frm_pos_map_t&) const;

    virtual void StopRecording(void);
    virtual bool IsRecording(void);
    virtual bool IsRecordingRequested(void);

    /// \brief Returns a report about the current recordings quality.
    virtual RecordingQuality *GetRecordingQuality(const RecordingInfo*) const;

    // pausing interface
    virtual void Pause(bool clear = true);
    virtual void Unpause(void);
    virtual bool IsPaused(bool holding_lock = false) const;
    virtual bool WaitForPause(int timeout = 1000);

    /** \brief Returns an approximation of the frame rate.
     *
     *  \bug This can be off by at least half, our non-frame grabber based
     *       recorders do not not try to approximate the frame rate. So
     *       a 720p recording at 60fps will report a frame-rate of 25fps.
     */
    double GetFrameRate(void) { return video_frame_rate; }

    /** \brief If requested, switch to new RingBuffer/ProgramInfo objects
     */
    virtual void CheckForRingBufferSwitch(void);

    /** \brief Save the seektable to the DB
     */
    void SavePositionMap(bool force = false);

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
        const RecordingProfile &profile,
        const GeneralDBOptions &genOpt,
        const DVBDBOptions     &dvbOpt);

  protected:
    /** \brief Convenience function used to set integer options from a profile.
     *  \sa SetOption(const QString&, int)
     */
    void SetIntOption(RecordingProfile *profile, const QString &name);
    /** \brief Convenience function used to set QString options from a profile.
     *  \sa SetOption(const QString&, const QString&)
     */
    void SetStrOption(RecordingProfile *profile, const QString &name);
    virtual bool PauseAndWait(int timeout = 100);

    virtual void ResetForNewFile(void) = 0;
    virtual void ClearStatistics(void);
    virtual void FinishRecording(void) = 0;
    virtual void StartNewFile(void) { }

    /** \brief Set seektable type
     */
    void SetPositionMapType(MarkTypes type) { positionMapType = type; }

    /** \brief Note a change in aspect ratio in the recordedmark table
     */
    void AspectChange(uint ratio, long long frame);

    /** \brief Note a change in video size in the recordedmark table
     */
    void ResolutionChange(uint width, uint height, long long frame);

    /** \brief Note a change in video frame rate in the recordedmark table
     */
    void FrameRateChange(uint framerate, long long frame);

    /** \brief Note the total duration in the recordedmark table
     */
    void SetDuration(uint64_t duration);

    /** \brief Note the total frames in the recordedmark table
     */
    void SetTotalFrames(uint64_t total_frames);

    TVRec         *tvrec;
    RingBuffer    *ringBuffer;
    bool           weMadeBuffer;

    QString        videocodec;
    QString        videodevice;

    bool           ntsc;
    bool           ntsc_framerate;
    double         video_frame_rate;

    uint           m_videoAspect; // AspectRatio

    uint           m_videoHeight;
    uint           m_videoWidth;
    double         m_frameRate;

    RecordingInfo *curRecording;

    // For handling pausing + stop recording
    mutable QMutex pauseLock; // also used for request_recording and recording
    bool           request_pause;
    bool           paused;
    QWaitCondition pauseWait;
    QWaitCondition unpauseWait;
    /// True if API call has requested a recording be [re]started
    bool           request_recording;
    /// True while recording is actually being performed
    bool           recording;
    QWaitCondition recordingWait;
    

    // For RingBuffer switching
    QMutex         nextRingBufferLock;
    RingBuffer    *nextRingBuffer;
    RecordingInfo *nextRecording;

    // Seektable  support
    MarkTypes      positionMapType;
    mutable QMutex positionMapLock;
    frm_pos_map_t  positionMap;
    frm_pos_map_t  positionMapDelta;
    frm_pos_map_t  durationMap;
    frm_pos_map_t  durationMapDelta;
    MythTimer      positionMapTimer;

    // Statistics
    // Note: Once we enter RecorderBase::run(), only that thread can
    // update these values safely. These values are read in that thread
    // without locking and updated with the lock held. Outside that
    // thread these values are only read, and only with the lock held.
    mutable QMutex statisticsLock;
    QAtomicInt     timeOfFirstDataIsSet; // doesn't need locking
    QDateTime      timeOfFirstData;
    QAtomicInt     timeOfLatestDataCount; // doesn't need locking
    QAtomicInt     timeOfLatestDataPacketInterval; // doesn't need locking
    QDateTime      timeOfLatestData;
    MythTimer      timeOfLatestDataTimer;
    RecordingGaps  recordingGaps;
    /// timeOfLatest update interval target in milliseconds.
    static const uint kTimeOfLatestDataIntervalTarget;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
