#ifndef _HLS_Reader_h_
#define _HLS_Reader_h_

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
#include "mythdownloadmanager.h"
#else
#include "mythsingledownload.h"
#endif

#include "mythlogging.h"

#include "HLSSegment.h"
#include "HLSStream.h"
#include "HLSStreamWorker.h"
#include "HLSPlaylistWorker.h"


class HLSReader
{
    friend class HLSStreamWorker;
    friend class HLSPlaylistWorker;

  public:
    typedef QMap<QString, HLSRecStream* > StreamContainer;
    typedef QList<HLSRecSegment> SegmentContainer;

    HLSReader(void);
    ~HLSReader(void);

    bool Open(const QString & uri);
    void Close(void);
    int Read(uint8_t* buffer, int len);
    void Throttle(bool val);
    bool IsThrottled(void) const { return m_throttle; }
    bool IsOpen(const QString& url) const
    { return m_curstream && m_m3u8 == url; }

    bool LoadMetaPlaylists(MythSingleDownload& downloader);
    bool UpdateSegment(int64_t sequence_num, int duration,
		       const QString& title, const QString& uri);
    void HadError(void) { m_error = true; }

    QString StreamURL(void) const
    { return QString("%1").arg(m_curstream ? m_curstream->Url() : ""); }

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    static bool DownloadURL(const QString &url, QByteArray *buffer);
#endif
    static void CancelURL(const QString &url);
    static void CancelURL(const QStringList &urls);
    static QString RelativeURI(const QString surl, const QString spath);

  protected:
    void Cancel(void);
    bool LoadSegments(MythSingleDownload& downloader);
    uint PercentBuffered(void) const;
    uint TargetDuration(void) const
    { return (m_curstream ? m_curstream->TargetDuration() : 0); }

  private:
    static QString DecodedURI(const QString uri);

    bool IsValidPlaylist(QTextStream & text);

    static QString ParseAttributes(const QString& line, const char* attr);
    static bool ParseDecimalValue(const QString& line, int& target);
    static bool ParseDecimalValue(const QString& line, int64_t& target);
    static bool ParseVersion(const QString& line, int& version,
			     const QString& loc);
    static HLSRecStream* ParseStreamInformation(const QString& line,
						const QString& url,
						const QString& loc);
    static bool ParseTargetDuration(HLSRecStream* hls, const QString& line,
				    const QString& loc);
    static bool ParseSegmentInformation(const HLSRecStream* hls,
					const QString& line,
					uint& duration, QString& title,
					const QString& url);
    static bool ParseMediaSequence(int64_t & sequence_num, const QString& line,
				   const QString& loc);
    static bool ParseKey(HLSRecStream* hls, const QString& line, bool& aesmsg,
			 const QString& loc);
    static bool ParseProgramDateTime(HLSRecStream* hls, const QString& line,
				     const QString& loc);
    static bool ParseAllowCache(HLSRecStream* hls, const QString& line,
				const QString& loc);
    static bool ParseDiscontinuity(HLSRecStream* hls, const QString& line,
				   const QString& loc);
    static bool ParseEndList(HLSRecStream* hls, const QString& loc);

    bool ParseM3U8(const QByteArray & buffer, HLSRecStream* stream = NULL);
    void DecreaseBitrate(int progid);
    void IncreaseBitrate(int progid);

    // Downloading
    bool LoadSegments(HLSRecStream & hlsstream);
    int DownloadSegmentData(MythSingleDownload& downloader, HLSRecStream* hls,
			    HLSRecSegment& segment, int playlist_size);

  private:
    QString          m_m3u8;
    StreamContainer  m_streams;
    SegmentContainer m_segments;
    HLSRecStream    *m_curstream;

    bool m_error;
    bool m_cancel;
    bool m_throttle;
    bool m_aesmsg;   // only print one time that the media is encrypted

    HLSPlaylistWorker *m_playlistworker;
    HLSStreamWorker   *m_streamworker;

    int64_t    m_seq_begin;
    int64_t    m_seq_first;
    int64_t    m_seq_next;
    int64_t    m_seq_end;
    bool       m_bandwidthcheck;
    uint       m_prebuffer_cnt;
    QMutex     m_seq_lock;
    QMutex     m_stream_lock;
    QMutex     m_throttle_lock;
    QWaitCondition  m_throttle_cond;

    // Downloading
    int         m_slow_cnt;
    QByteArray  m_buffer;
    QMutex      m_buflock;
};

#endif
