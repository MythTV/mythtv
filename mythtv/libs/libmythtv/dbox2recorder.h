/**
 *  DBOX2Recorder
 *  Copyright (c) 2005 by Levent Gündogdu
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DBOX2RECORDER_H_
#define DBOX2RECORDER_H_

#include "dtvrecorder.h"
#include <time.h>
#include "dbox2channel.h"
#include "sitypes.h"
#include "qhttp.h"
#include "mpeg/tspacket.h"

#define DBOX2_TIMEOUT 15
#define DBOX_MAX_PID_COUNT 32
#define PAT_TID   0x00
#define PMT_TID   0x02
#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a

typedef struct stream_meta_{
    int      socket;
    uint8_t* buffer;
    int      bufferIndex;
} stream_meta;


class DBox2Recorder : public DTVRecorder
{
    Q_OBJECT
    public:

        DBox2Recorder(DBox2Channel *channel, int cardid);
        ~DBox2Recorder() { TeardownAll(); }

	void StartRecording(void);
	bool Open(void); 
	void ProcessTSPacket(unsigned char *tspacket, int len);
	void SetOptionsFromProfile(RecordingProfile *profile,
				   const QString &videodev,
				   const QString &audiodev,
				   const QString &vbidev, int ispip);
	
	void SetOption(const QString &name, const QString &value);
	void SetOption(const QString &name, int value);
	
    signals:
        void RecorderAlive(bool);

    public slots:
        void httpRequestFinished ( int id, bool error );
	void ChannelChanged();
	void ChannelChanging();
        void deleteLater(void);
    
    private:
	// Methods
        void TeardownAll(void);
	void ChannelChanged(dbox2_channel_t& chan);
	void CreatePAT(uint8_t *ts_packet);
	int  getPMTSectionID(uint8_t* buffer, int pmtPID);
	void updatePMTSectionID(uint8_t* buffer, int pmtPID);
	int  processStream(stream_meta* stream);
	void initStream(stream_meta* meta);
	int  OpenStream();
	bool RequestStream();
	bool RequestInfo();
	int  findTSHeader(uint8_t* buffer, int len);
	void Close();
	// Members for creating/handling PAT and PMT
	int m_cardid;
	uint8_t *m_patPacket;
	int pat_cc;
	int pkts_until_pat;
	int m_pidPAT;
	int m_pids[DBOX_MAX_PID_COUNT];
	int m_pidCount;
	int m_pmtPID;
	int m_ac3PID;
	int m_sectionID;
	DBox2Channel *m_channel;
	// Connection relevant members
	int port;
	int httpPort;
	QString ip;
	bool isOpen;
	QHttp* http;
	int m_lastPIDRequestID;
	int m_lastInfoRequestID;
	time_t lastpacket;
	int bufferSize;
	stream_meta transportStream;
	TSPacket tpkt;
	int m_videoWidth;
	int m_videoHeight;
	QString m_videoFormat;
	bool _request_abort;
};

#endif
