#ifndef HTTPLIVESTREAM_H
#define HTTPLIVESTREAM_H

#include <QString>

#include "libmythtv/mythframe.h"

enum HTTPLiveStreamStatus : std::int8_t {
    kHLSStatusUndefined    = -1,
    kHLSStatusQueued       = 0,
    kHLSStatusStarting     = 1,
    kHLSStatusRunning      = 2,
    kHLSStatusCompleted    = 3,
    kHLSStatusErrored      = 4,
    kHLSStatusStopping     = 5,
    kHLSStatusStopped      = 6
};


class MTV_PUBLIC HTTPLiveStream
{
 public:
    explicit HTTPLiveStream(QString srcFile, uint16_t width = 640, uint16_t height = 480,
                   uint32_t bitrate = 800000, uint32_t abitrate = 64000,
                   uint16_t maxSegments = 0, uint16_t segmentSize = 10,
                   uint32_t aobitrate = 32000, int32_t srate = -1);
    explicit HTTPLiveStream(int streamid);
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
    bool UpdateStatusMessage(const QString& message);
    bool UpdatePercentComplete(int percent);

    static QString StatusToString(HTTPLiveStreamStatus status);

    bool CheckStop(void);

 protected:
    bool        m_writing          {false};
    int         m_streamid         {-1};
    QString     m_sourceFile;
    QString     m_sourceHost;
    uint16_t    m_sourceWidth      {0};
    uint16_t    m_sourceHeight     {0};
    QString     m_outDir;
    QString     m_outBase;
    QString     m_outBaseEncoded;
    QString     m_outFile;
    QString     m_outFileEncoded;
    QString     m_audioOutFile;
    QString     m_audioOutFileEncoded;
    uint16_t    m_segmentSize      {10};
    uint16_t    m_maxSegments      {0};
    uint16_t    m_segmentCount     {0};
    uint16_t    m_startSegment     {0};
    uint16_t    m_curSegment       {0};
    QString     m_httpPrefix;
    QString     m_httpPrefixRel;
    uint16_t    m_height           {480};
    uint16_t    m_width            {640};
    uint32_t    m_bitrate          {800000};
    uint32_t    m_audioBitrate     { 64000};
    uint32_t    m_audioOnlyBitrate { 32000};
    int32_t     m_sampleRate       {-1};

    QDateTime   m_created;
    QDateTime   m_lastModified;
    uint16_t    m_percentComplete  {0};
    QString     m_relativeURL;
    QString     m_fullURL;
    QString     m_statusMessage;

    HTTPLiveStreamStatus m_status  {kHLSStatusUndefined};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

