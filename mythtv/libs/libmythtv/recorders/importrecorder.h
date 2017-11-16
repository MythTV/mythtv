// -*- Mode: c++ -*-

#ifndef _IMPORT_RECORDER_H_
#define _IMPORT_RECORDER_H_

#include <QMutex>

#include "dtvrecorder.h"
#include "tspacket.h"
#include "mpegstreamdata.h"
#include "DeviceReadBuffer.h"

struct AVFormatContext;
struct AVPacket;
class MythCommFlagPlayer;

/** \brief ImportRecorder imports files, creating a seek map and
 *         other stuff that MythTV likes to have for recording.
 *
 *  \note This currently only supports MPEG-TS files, but the
 *        plan is to support all files that ffmpeg does.
 */
class ImportRecorder : public DTVRecorder
{
  public:
    explicit ImportRecorder(TVRec*);
    ~ImportRecorder();

    // RecorderBase
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    void run(void);

    bool Open(void);
    void Close(void);

    void InitStreamData(void) {}

    virtual long long GetFramesWritten(void);
    virtual RecordingQuality *GetRecordingQuality(const RecordingInfo*) const {return NULL;}
    void UpdateRecSize();

  private:
    int             _import_fd;
    MythCommFlagPlayer *m_cfp;
    long long m_nfc;
};

#endif // _IMPORT_RECORDER_H_
