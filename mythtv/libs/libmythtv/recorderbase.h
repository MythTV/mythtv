// -*- Mode: c++ -*-
#ifndef RECORDERBASE_H_
#define RECORDERBASE_H_

#include <qmutex.h>
#include <qstring.h>
#include <qmap.h>
#include <qsqldatabase.h>
#include <qwaitcondition.h>

#include <pthread.h>

class TVRec;
class RingBuffer;
class ProgramInfo;
class RecordingProfile;

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
class RecorderBase
{
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
    void SetRecording(const ProgramInfo *pginfo);

    /** \brief Tells recorder to use an externally created ringbuffer.
     *
     *   If this an external RingBuffer is set, it should be before any
     *   Initialize(), Open(), or StartRecorder() calls. Externally created
     *   RingBuffers are not deleted in the Recorder's destructor.
     */
    void SetRingBuffer(RingBuffer *rbuf);

    /** \brief Set an specific option.
     *
     *   Base options include: codec, audiodevice, videodevice, vbidevice,
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
    virtual void SetNextRecording(const ProgramInfo*, RingBuffer*) = 0;

    /** \brief This is called between SetOptionsFromProfile() and
     *         StartRecording() to initialize any devices, etc.
     */
    virtual void Initialize(void) = 0;

    /** \brief StartRecording() starts the recording process, and does not
     *         exit until the recording is complete.
     *  \sa StopRecording()
     */
    virtual void StartRecording(void) = 0;

    /** \brief StopRecording() signals to the StartRecording() function that
     *         it should stop recording and exit cleanly.
     *
     *   This function should block until StartRecording() has finished up.
     */
    virtual void StopRecording(void) = 0;

    /** \brief Reset the recorder to the startup state.
     *
     *   This is used after Pause(bool), WaitForPause() and 
     *   after the RingBuffer's StopReads() method has been called.
     */
    virtual void Reset(void) = 0;   

    /// \brief Tells whether the StartRecorder() loop is running.
    virtual bool IsRecording(void) = 0;

    /// \brief Tells us whether an unrecoverable error has been encountered.
    virtual bool IsErrored(void) = 0;

    /** \brief Returns number of frames written to disk.
     *
     *   It is not always safe to seek up to this frame in a player
     *   because frames may not be written in display order.
     */
    virtual long long GetFramesWritten(void) = 0;
    
    /** \brief Open devices needed by recorder.
     *
     *   This is usually called by StartRecording().
     *  \return true if device was successfully opened.
     */
    virtual bool Open(void) = 0;
    
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
     *   This returns -1 if a keyframe position can not be found
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
    virtual long long GetKeyframePosition(long long desired) = 0;

    /** \brief Pause tells StartRecording() to pause, it should not block.
     *
     *   Once paused the recorder calls tvrec->RecorderPaused().
     *
     *  \param clear if true any generated timecodes should be reset.
     *  \sa Unpause(), WaitForPause()
     */
    virtual void Pause(bool clear = true)
        { (void) clear; request_pause = true; }

    /// \brief Unpause tells StartRecording() to unpause, it should not block.
    virtual void Unpause(void)
        { request_pause = false; unpauseWait.wakeAll(); }
    /// \brief Returns true iff recorder is paused.
    virtual bool IsPaused(void) const { return paused; }
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
    virtual void FinishRecording(void) = 0;
    virtual void StartNewFile(void) { }

    TVRec         *tvrec;
    RingBuffer    *ringBuffer;
    bool           weMadeBuffer;

    QString        codec;
    QString        audiodevice;
    QString        videodevice;
    QString        vbidevice;

    int            vbimode;
    bool           ntsc;
    bool           ntsc_framerate;
    double         video_frame_rate;

    ProgramInfo   *curRecording;

    // For handling pausing
    bool           request_pause;
    bool           paused;
    QWaitCondition pauseWait;
    QWaitCondition unpauseWait;

    // For RingBuffer switching
    QMutex         nextRingBufferLock;
    RingBuffer    *nextRingBuffer;
    ProgramInfo   *nextRecording;
};

#endif
