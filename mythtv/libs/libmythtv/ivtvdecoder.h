#ifndef IVTVDECODER_H_
#define IVTVDECODER_H_

#include <qstring.h>
#include <qmap.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"

extern "C" {
#include "frame.h"
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

class ProgramInfo;
class MythSqlDatabase;

struct IvtvQueuedFrame
{
    int raw;
    long long actual;
    IvtvQueuedFrame(void) { raw = 0; actual = 0; };
    IvtvQueuedFrame(int r, long long a) { raw = r; actual = a; };
};
typedef QValueList<IvtvQueuedFrame> ivtv_frame_list_t;

class DeviceInfo
{
  public:
    DeviceInfo() : works(false), ntsc(true) {}
    bool works;
    bool ntsc;
};
typedef QMap<QString,DeviceInfo> DevInfoMap;

class IvtvDecoder : public DecoderBase
{
  public:
    IvtvDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo);
   ~IvtvDecoder();

    static bool CanHandle(char testbuf[2048], const QString &filename);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    bool GetFrame(int onlyvideo);

    bool DoFastForward(long long desiredFrame, bool doflush = true);

    bool isLastFrameKey(void) { return false; }
    void WriteStoredData(RingBuffer *rb, bool storevid, long timecodeOffset)
                           { (void)rb; (void)storevid; (void)timecodeOffset;}
    void SetRawAudioState(bool state) { (void)state; }
    bool GetRawAudioState(void) const { return false; }
    void SetRawVideoState(bool state) { (void)state; }
    bool GetRawVideoState(void) const { return false; }

    long UpdateStoredFrameNum(long frame) { (void)frame; return 0; }

    QString GetEncodingType(void) const { return QString("MPEG-2"); }

    void UpdateFramesPlayed(void);

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

  private:
    int MpegPreProcessPkt(unsigned char *buf, int len, 
                          long long startpos, long stopframe);
    void SeekReset(long long newkey = 0, uint skipframes = 0,
                   bool needFlush = false, bool discardFrames = false);
    bool ReadWrite(int onlyvideo, long stopframe = LONG_MAX);
    bool StepFrames(long long start, long long count);

    static bool GetDeviceWorks(QString dev);
    static bool GetDeviceNTSC(QString dev);
    static void SetDeviceInfo(QString dev, bool works, bool ntsc);
    static bool CheckDevice(void);

  private:
    bool              validvpts;
    bool              gotvideo;
    bool              gopset;
    bool              needPlay;
    unsigned int      mpeg_state;

    int               prevgoppos;
    int               firstgoppos;

    unsigned char    *vidbuf;
    int               vidread;
    int               vidwrite;
    int               vidfull;
    int               vidframes;
    int               frame_decoded;
    long long         videoPlayed;
    long long         lastStartFrame;
    long long         laststartpos;

    int               nexttoqueue;
    int               lastdequeued;
    ivtv_frame_list_t queuedlist;

  private:
    static DevInfoMap devInfo;
    static QMutex     devInfoLock;
    static const uint vidmax;
};

#endif
