#ifndef HTTPLIVESTREAM_H
#define HTTPLIVESTREAM_H

#include <QString>

#include "datacontracts/liveStreamInfoList.h"

#include "frame.h"

typedef enum {
    kHLSStatusUndefined    = -1,
    kHLSStatusQueued       = 0,
    kHLSStatusStarting     = 1,
    kHLSStatusRunning      = 2,
    kHLSStatusCompleted    = 3,
    kHLSStatusErrored      = 4,
    kHLSStatusStopping     = 5,
    kHLSStatusStopped      = 6
} HTTPLiveStreamStatus;


class MTV_PUBLIC HTTPLiveStream
{
 public:
    HTTPLiveStream(QString srcFile, uint16_t width = 640, uint16_t height = 480,
                   uint32_t bitrate = 800000, uint32_t abitrate = 64000,
                   uint16_t maxSegments = 0, uint16_t segmentSize = 10,
                   uint32_t aobitrate = 32000, int32_t srate = -1);
    HTTPLiveStream(int streamid);
   ~HTTPLiveStream();

    bool InitForWrite(void);
    bool LoadFromDB(void);

    int      GetStreamID(void) const { return m_streamid; }
    uint16_t GetWidth(void) const { return m_width; }
    uint16_t GetHeight(void) const { return m_height; }
    uint32_t GetBitrate(void) const { return m_bitrate; }
    uint32_t GetAudioBitrate(void) const { return m_audioBitrate; }
    uint32_t GetAudioOnlyBitrate(void) const { return m_audioOnlyBitrate; }
    uint16_t GetMaxSegments(void) const { return m_maxSegments; }
    QString  GetSourceFile(void) const { return m_sourceFile; }
    QString  GetHTMLPageName(void) const;
    QString  GetMetaPlaylistName(void) const;
    QString  GetPlaylistName(bool audioOnly = false) const;
    uint16_t GetSegmentSize(void) const { return m_segmentSize; }
    QString  GetFilename(uint16_t segmentNumber = 0, bool fileOnly = false,
                         bool audioOnly = false, bool encoded = false) const;
    QString  GetCurrentFilename(
        bool audioOnly = false, bool encoded = false) const;

    void SetOutputVars(void);

    HTTPLiveStreamStatus GetDBStatus(void) const;

    int      AddStream(void);
    bool     AddSegment(void);

    bool WriteHTML(void);
    bool WriteMetaPlaylist(void);
    bool WritePlaylist(bool audioOnly = false, bool writeEndTag = false);

    bool SaveSegmentInfo(void);

    bool UpdateSizeInfo(uint16_t width, uint16_t height,
                        uint16_t srcwidth, uint16_t srcheight);
    bool UpdateStatus(HTTPLiveStreamStatus status);
    bool UpdateStatusMessage(QString message);
    bool UpdatePercentComplete(int percent);

    QString StatusToString(HTTPLiveStreamStatus status);

    bool CheckStop(void);

           DTC::LiveStreamInfo     *StartStream(void);
    static DTC::LiveStreamInfo     *StopStream(int id);
    static bool                     RemoveStream(int id);

           DTC::LiveStreamInfo     *GetLiveStreamInfo(DTC::LiveStreamInfo *info = NULL);
    static DTC::LiveStreamInfoList *GetLiveStreamInfoList( const QString &FileName = "");

 protected:
    bool        m_writing;
    int         m_streamid;
    QString     m_sourceFile;
    QString     m_sourceHost;
    uint16_t    m_sourceWidth;
    uint16_t    m_sourceHeight;
    QString     m_outDir;
    QString     m_outBase;
    QString     m_outBaseEncoded;
    QString     m_outFile;
    QString     m_outFileEncoded;
    QString     m_audioOutFile;
    QString     m_audioOutFileEncoded;
    uint16_t    m_segmentSize;
    uint16_t    m_maxSegments;
    uint16_t    m_segmentCount;
    uint16_t    m_startSegment;
    uint16_t    m_curSegment;
    QString     m_httpPrefix;
    QString     m_httpPrefixRel;
    uint16_t    m_height;
    uint16_t    m_width;
    uint32_t    m_bitrate;
    uint32_t    m_audioBitrate;
    uint32_t    m_audioOnlyBitrate;
    int32_t     m_sampleRate;

    QDateTime   m_created;
    QDateTime   m_lastModified;
    uint16_t    m_percentComplete;
    QString     m_relativeURL;
    QString     m_fullURL;
    QString     m_statusMessage;

    HTTPLiveStreamStatus m_status;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

