/*****************************************************************************
 * httplivestreambuffer.cpp
 * MythTV
 *
 * Created by Jean-Yves Avenard on 6/05/12.
 * Copyright (c) 2012 Bubblestuff Pty Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MythXCode_hlsbuffer_h
#define MythXCode_hlsbuffer_h

#include "libmythbase/mythcorecontext.h"
#include "libmythtv/io/mythmediabuffer.h"

class MythDownloadManager;
class HLSStream;
class HLSSegment;
class StreamWorker;
class PlaylistWorker;
class HLSPlayback;

using StreamsList = QList<HLSStream*>;

class HLSRingBuffer : public MythMediaBuffer
{
public:
    explicit HLSRingBuffer(const QString &lfilename);
    HLSRingBuffer(const QString &lfilename, bool open);
    ~HLSRingBuffer() override;

    bool IsOpen(void) const override; // RingBuffer
    long long GetReadPosition(void) const override; // RingBuffer
    bool OpenFile(const QString &lfilename,
                  std::chrono::milliseconds retry_ms = kDefaultOpenTimeout) override; // RingBuffer
    bool IsStreamed(void) override          { return false;   }  // RingBuffer
    bool IsSeekingAllowed(void) override    { return !m_error; } // RingBuffer
    bool IsBookmarkAllowed(void) override   { return true; }     // RingBuffer
    static bool IsHTTPLiveStreaming(QByteArray *s);
    static bool TestForHTTPLiveStreaming(const QString &filename);
    bool SaveToDisk(const QString &filename, int segstart = 0, int segend = -1);
    int NumStreams(void) const;
    void Interrupt(void);
    void Continue(void);
    int DurationForBytes(uint size);

protected:
    int SafeRead(void *data, uint sz) override; // RingBuffer
    long long GetRealFileSizeInternal(void) const override; // RingBuffer
    long long SeekInternal(long long pos, int whence) override; // RingBuffer

private:
    void FreeStreamsList(QList<HLSStream*> *streams) const;
    HLSStream *GetStreamForSegment(int segnum) const;
    HLSStream *GetStream(int wanted, const StreamsList *streams = nullptr) const;
    HLSStream *GetFirstStream(const StreamsList *streams = nullptr) const;
    HLSStream *GetLastStream(const StreamsList *streams = nullptr) const;
    HLSStream *FindStream(const HLSStream *hls_new, const StreamsList *streams = nullptr) const;
    HLSStream *GetCurrentStream(void) const;
    static QString ParseAttributes(const QString &line, const char *attr);
    static int ParseDecimalValue(const QString &line, int &target);
    static int ParseSegmentInformation(const HLSStream *hls, const QString &line,
                                       int &duration, QString &title);
    static int ParseTargetDuration(HLSStream *hls, const QString &line);
    HLSStream *ParseStreamInformation(const QString &line, const QString &uri) const;
    static int ParseMediaSequence(HLSStream *hls, const QString &line);
    int ParseKey(HLSStream *hls, const QString &line);
    static int ParseProgramDateTime(HLSStream *hls, const QString &line);
    static int ParseAllowCache(HLSStream *hls, const QString &line);
    static int ParseVersion(const QString &line, int &version);
    static int ParseEndList(HLSStream *hls);
    static int ParseDiscontinuity(HLSStream *hls, const QString &line);
    int ParseM3U8(const QByteArray *buffer, StreamsList *streams = nullptr);
    int Prefetch(int count);
    void SanityCheck(const HLSStream *hls) const;
    HLSSegment *GetSegment(int segnum, std::chrono::milliseconds timeout = 1s);
    int NumSegments(void) const;
    int ChooseSegment(int stream) const;
    int64_t SizeMedia(void) const;
    void WaitUntilBuffered(void);
    void SanitizeStreams(StreamsList *streams = nullptr);

    // private member variables
    QString             m_m3u8;     // M3U8 url
    QByteArray          m_peeked;

    HLSPlayback        *m_playback {nullptr};

    /* state */
    StreamsList         m_streams;  // bandwidth adaptation
    mutable QMutex      m_lock;     // protect general class members
    bool                m_meta    {false}; // meta playlist
    bool                m_error   {false}; // parsing error
#ifdef USING_LIBCRYPTO
    bool                m_aesmsg  {false}; // only print one time that the media is encrypted
#endif
    int                 m_startup {0};  // starting segment (where seek start)
    /**
     * assumed bitrate of playback
     * used for the purpose of calculating length and seek position.
     * the value itself is irrelevant, as it's only used as a common reference
     */
    int64_t             m_bitrate {0};
    /**
     * FFmpeg seek to the end of the stream in order to determine the length
     * of the video. Set to boolean to true after we detected a seek to the end
     * this will prevent waiting for new data in safe_read
     */
    bool                m_seektoend      {false};

    friend class StreamWorker;
    StreamWorker       *m_streamworker   {nullptr};
    friend class PlaylistWorker;
    PlaylistWorker     *m_playlistworker {nullptr};
    FILE               *m_fd             {nullptr};
    bool                m_interrupted    {false};
    bool                m_killed         {false};
};

#endif
