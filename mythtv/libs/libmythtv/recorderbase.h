#ifndef RECORDERBASE_H_
#define RECORDERBASE_H_

#include <qstring.h>
#include <qmap.h>
#include <qsqldatabase.h>
#include <qwaitcondition.h>

#include <pthread.h>

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
    RecorderBase();
    virtual ~RecorderBase();

    void SetRingBuffer(RingBuffer *rbuf);
    void SetRecording(ProgramInfo *pginfo);

    float GetFrameRate(void) { return video_frame_rate; }
    void SetFrameRate(float rate) { video_frame_rate = rate; }

    virtual void SetOption(const QString &opt, const QString &value);
    virtual void SetOption(const QString &opt, int value);
    virtual void SetVideoFilters(QString &filters) = 0;

    virtual void SetOptionsFromProfile(RecordingProfile *profile, 
                                       const QString &videodev, 
                                       const QString &audiodev,
                                       const QString &vbidev, int ispip) = 0;

    virtual void Initialize(void) = 0;
    virtual void StartRecording(void) = 0;
    virtual void StopRecording(void) = 0;
    virtual void Reset(void) = 0;   

    virtual void Pause(bool clear = true) = 0;
    virtual void Unpause(void) = 0;
    virtual bool GetPause(void) = 0;
    virtual void WaitForPause(void) = 0;

    virtual bool IsRecording(void) = 0;
    virtual bool IsErrored(void) = 0;

    virtual long long GetFramesWritten(void) = 0;
    
    virtual bool Open(void) = 0;
    virtual int GetVideoFd(void) = 0;
    
    virtual long long GetKeyframePosition(long long desired) = 0;

    virtual void ChannelNameChanged(const QString& new_name);
    QString GetCurChannelName() const;

  protected:
    void SetIntOption(RecordingProfile *profile, const QString &name);

    RingBuffer *ringBuffer;
    bool weMadeBuffer;

    QString codec;
    QString audiodevice;
    QString videodevice;
    QString vbidevice;

    char vbimode;
    int ntsc;
    int ntsc_framerate;
    double video_frame_rate;

    ProgramInfo *curRecording;
    QString curChannelName;

    QWaitCondition pauseWait;
};

#endif
