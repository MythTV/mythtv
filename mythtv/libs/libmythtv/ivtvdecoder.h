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

class IvtvDecoder : public DecoderBase
{
  public:
    IvtvDecoder(NuppelVideoPlayer *parent, MythSqlDatabase *db,
                    ProgramInfo *pginfo);
   ~IvtvDecoder();

    static bool CanHandle(char testbuf[2048], const QString &filename);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    void GetFrame(int onlyvideo);

    bool DoFastForward(long long desiredFrame);

    bool isLastFrameKey(void) { return false; }
    void WriteStoredData(RingBuffer *rb, bool storevid)
                           { (void)rb; (void)storevid; }
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
    void MpegPreProcessPkt(unsigned char *buf, int len, long long startpos);
    void SeekReset(long long newkey = 0, int skipframes = 0,
                   bool needFlush = false);
    bool ReadWrite(int onlyvideo, int delay);
    bool StepFrames(int start, int count);

    int frame_decoded;

    long long lastStartFrame;

    int prevgoppos;

    bool gopset;

    bool validvpts;

    int firstgoppos;
    bool gotvideo;

    long long laststartpos;

    unsigned char prvpkt[3];

    static bool ntsc;

    static const int vidmax = 131072; // must be a power of 2
    unsigned char vidbuf[vidmax];
    int vidread, vidwrite, vidfull;
};

#endif
