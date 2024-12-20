#ifndef HLS_READER_H
#define HLS_READER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTextStream>

/*
  Use MythSingleDownload.

  MythDownloadManager leaks memory and QNetworkAccessManager can only
  handle six simultaneous downloads.  Each HLS stream can be
  downloading the playlist and the segments at the same time, so the
  limit of six could impact performance if recording more than three
  HLS channels.
*/

#ifdef HLS_USE_MYTHDOWNLOADMANAGER
#include "libmythbase/mythdownloadmanager.h"
#else
#include "libmythbase/mythsingledownload.h"
#endif

#include "libmythbase/mythlogging.h"
#include "libmythtv/mythtvexp.h"

#include "HLSSegment.h"
#include "HLSStream.h"
#include "HLSStreamWorker.h"
#include "HLSPlaylistWorker.h"


class MTV_PUBLIC  HLSReader
{
    friend class HLSStreamWorker;
    friend class HLSPlaylistWorker;

  public:
    using StreamContainer = QMap<QString, HLSRecStream* >;
    using SegmentContainer = QVector<HLSRecSegment>;

    HLSReader(int inputId) { m_inputId = inputId; };
    ~HLSReader(void);

    bool Open(const QString & m3u, int bitrate_index = 0);
    void Close(bool quiet = false);
    qint64 Read(uint8_t* buffer, qint64 len);
    void Throttle(bool val);
    bool IsThrottled(void) const { return m_throttle; }
    bool IsOpen(const QString& url) const
    { return m_curstream && m_m3u8 == url; }
    bool FatalError(void) const { return m_fatal; }

    bool LoadMetaPlaylists(MythSingleDownload& downloader);
    void ResetStream(void)
      { QMutexLocker lock(&m_streamLock); m_curstream = nullptr; }
    void ResetSequence(void) { m_curSeq = -1; }


    QString StreamURL(void) const
    { return QString("%1").arg(m_curstream ? m_curstream->M3U8Url() : ""); }

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    static bool DownloadURL(const QString &url, QByteArray *buffer);
#endif
    static void CancelURL(const QString &url);
    static void CancelURL(const QStringList &urls);

    static bool IsValidPlaylist(QTextStream & text);

  protected:
    void Cancel(bool quiet = false);
    bool LoadSegments(MythSingleDownload& downloader);
    uint PercentBuffered(void) const;
    std::chrono::seconds TargetDuration(void) const
    { return (m_curstream ? m_curstream->TargetDuration() : 0s); }

    void AllowPlaylistSwitch(void) { m_bandwidthCheck = true; }

    void PlaylistGood(void);
    void PlaylistRetrying(void);
    int  PlaylistRetryCount(void) const;

  private:
    bool ParseM3U8(const QByteArray & buffer, HLSRecStream* stream = nullptr);
    void DecreaseBitrate(int progid);
    void IncreaseBitrate(int progid);

    // Downloading
    int DownloadSegmentData(MythSingleDownload& downloader, HLSRecStream* hls,
			    const HLSRecSegment& segment, int playlist_size);

    // Debug
    void EnableDebugging(void);

  private:
    QString            m_m3u8;
    QString            m_segmentBase;
    StreamContainer    m_streams;
    SegmentContainer   m_segments;
    HLSRecStream      *m_curstream      {nullptr};
    int64_t            m_curSeq         {-1};
    int                m_bitrateIndex   {0};

    bool               m_fatal          {false};
    bool               m_cancel         {false};
    bool               m_throttle       {true};
    // Only print one time that the media is encrypted
    bool               m_aesMsg         {false};

    HLSPlaylistWorker *m_playlistWorker {nullptr};
    HLSStreamWorker   *m_streamWorker   {nullptr};

    int                m_playlistSize   {0};
    bool               m_bandwidthCheck {false};
    uint               m_prebufferCnt   {10};

    QMutex             m_seqLock;

    mutable QMutex     m_streamLock;

    mutable QMutex     m_workerLock;

    QMutex             m_throttleLock;
    QWaitCondition     m_throttleCond;

    bool               m_debug          {false};
    int                m_debugCnt       {0};

    // Downloading
    int                m_slowCnt        {0};
    QByteArray         m_buffer;
    QMutex             m_bufLock;

    // Log message
    int                m_inputId        {0};
};

#endif // HLS_READER_H
