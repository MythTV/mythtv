#ifndef IVTVDECODER_H_
#define IVTVDECODER_H_

#include <qstring.h>
#include <qmap.h>
#include <qsqldatabase.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"

extern "C" {
#include "frame.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class ProgramInfo;

class IvtvDecoder : public DecoderBase
{
  public:
    IvtvDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                    ProgramInfo *pginfo);
   ~IvtvDecoder();

    void Reset(void);

    static bool CanHandle(char testbuf[2048], const QString &filename);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    void GetFrame(int onlyvideo);

    bool DoRewind(long long desiredFrame);
    bool DoFastForward(long long desiredFrame);

    bool isLastFrameKey(void) { return false; }
    void WriteStoredData(RingBuffer *rb, bool storevid)
                           { (void)rb; (void)storevid; }
    void SetRawAudioState(bool state) { (void)state; }
    bool GetRawAudioState(void) { return false; }
    void SetRawVideoState(bool state) { (void)state; }
    bool GetRawVideoState(void) { return false; }

    long UpdateStoredFrameNum(long frame) { (void)frame; return 0; }

    void SetPositionMap(void);

    QString GetEncodingType(void) { return QString("MPEG-2"); }

    void InterruptDisplay(void);

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

  private:
    void MpegPreProcessPkt(unsigned char *buf, int len, long long startpos);
    void SeekReset(int skipframes = 0);

    RingBuffer *ringBuffer;

    int frame_decoded;

    long long framesPlayed;
    long long framesRead;

    bool hasFullPositionMap;
    QMap<long long, long long> positionMap;

    long long lastKey;

    int keyframedist;
    int prevgoppos;

    bool exitafterdecoded;

    bool ateof;
    bool gopset;

    QSqlDatabase *m_db;
    ProgramInfo *m_playbackinfo;

    double fps;
    bool validvpts;

    int firstgoppos;
    bool gotvideo;

    long long laststartpos;

    unsigned char prvpkt[3];

    static bool ntsc;

    bool ingetframe;
};

#endif
