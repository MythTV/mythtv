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

class IvtvDecoder : public DecoderBase
{
  public:
    IvtvDecoder(NuppelVideoPlayer *parent, MythSqlDatabase *db,
                    ProgramInfo *pginfo);
   ~IvtvDecoder();

    static bool CanHandle(char testbuf[2048], const QString &filename);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    bool GetFrame(int onlyvideo);

    bool DoFastForward(long long desiredFrame, bool doflush = true);

    bool isLastFrameKey(void) { return false; }
    void WriteStoredData(RingBuffer *rb, bool storevid, long timecodeOffset)
                           { (void)rb; (void)storevid; (long)timecodeOffset;}
    void SetRawAudioState(bool state) { (void)state; }
    bool GetRawAudioState(void) { return false; }
    void SetRawVideoState(bool state) { (void)state; }
    bool GetRawVideoState(void) { return false; }

    long UpdateStoredFrameNum(long frame) { (void)frame; return 0; }

    QString GetEncodingType(void) { return QString("MPEG-2"); }

    void UpdateFramesPlayed(void);

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

  private:
    int MpegPreProcessPkt(unsigned char *buf, int start, int len, long long startpos);
    void SeekReset(long long newkey = 0, int skipframes = 0,
                   bool needFlush = false);
    bool ReadWrite(int onlyvideo);
    bool StepFrames(int start, int count);

    int frame_decoded;

    long long lastStartFrame;

    int prevgoppos;

    bool gopset;

    bool validvpts;

    int firstgoppos;
    bool gotvideo;

    long long laststartpos;

    static bool ntsc;

    static const int vidmax = 65536;
    unsigned char vidbuf[vidmax];
    int vidread, vidwrite, vidfull;
    int vidscan, vidpoll, vid2write;
    int vidanyframes, videndofframe;
    long long startpos;
    unsigned mpeg_state;
    long long framesScanned;
    bool needPlay;
    long long videoPlayed;

    int nexttoqueue;
    int lastdequeued;
    QValueList<IvtvQueuedFrame> queuedlist;
};

#endif
