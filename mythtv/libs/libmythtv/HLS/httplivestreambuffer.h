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

#include "mythcorecontext.h"
#include "ringbuffer.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

class MythDownloadManager;
class HLSStream;
class HLSSegment;
class StreamWorker;
class PlaylistWorker;
class HLSPlayback;

typedef QList<HLSStream*> StreamsList;

class HLSRingBuffer : public RingBuffer
{
public:
    HLSRingBuffer(const QString &lfilename);
    HLSRingBuffer(const QString &lfilename, bool open);
    virtual ~HLSRingBuffer();

    virtual bool IsOpen(void) const;
    virtual long long GetReadPosition(void) const;
    virtual bool OpenFile(const QString &lfilename,
                          uint retry_ms = kDefaultOpenTimeout);
    virtual long long Seek(long long pos, int whence, bool has_lock);
    virtual long long GetRealFileSize(void) const;
    virtual bool IsStreamed(void)           { return false;   }
    virtual bool IsSeekingAllowed(void)     { return !m_error; }
    virtual bool IsBookmarkAllowed(void)    { return true; }
    static bool IsHTTPLiveStreaming(QByteArray *s);
    static bool TestForHTTPLiveStreaming(const QString filename);
    bool SaveToDisk(const QString filename, int segstart = 0, int segend = -1);
    int NumStreams(void) const;
    int Read(void *data, uint i_read) { return safe_read(data, i_read); }
    void Interrupt(void);
    void Continue(void);

protected:
    virtual int safe_read(void *data, uint i_read);

private:
    void FreeStreamsList(QList<HLSStream*> *streams) const;
    HLSStream *GetStreamForSegment(int segid) const;
    HLSStream *GetStream(const int wanted, const StreamsList *streams = NULL) const;
    HLSStream *GetFirstStream(const StreamsList *streams = NULL) const;
    HLSStream *GetLastStream(const StreamsList *streams = NULL) const;
    HLSStream *FindStream(const HLSStream *hls_new, const StreamsList *streams = NULL) const;
    HLSStream *GetCurrentStream(void) const;
    QString ParseAttributes(const QString line, const char *attr) const;
    int ParseDecimalValue(const QString line, int &target) const;
    int ParseSegmentInformation(const HLSStream *hls, const QString line,
                                int &duration, QString &title) const;
    int ParseTargetDuration(HLSStream *hls, const QString line) const;
    HLSStream *ParseStreamInformation(const QString line, const QString uri) const;
    int ParseMediaSequence(HLSStream *hls, const QString line) const;
    int ParseKey(HLSStream *hls, const QString line);
    int ParseProgramDateTime(HLSStream *hls, const QString line) const;
    int ParseAllowCache(HLSStream *hls, const QString line) const;
    int ParseVersion(const QString line, int &version) const;
    int ParseEndList(HLSStream *hls) const;
    int ParseDiscontinuity(HLSStream *hls, const QString line) const;
    int ParseM3U8(const QByteArray *buffer, StreamsList *streams = NULL);
    int Prefetch(int count);
    void SanityCheck(const HLSStream *hls, const HLSSegment *segment) const;
    HLSSegment *GetSegment(int segnum, int timeout = 1000);
    int NumSegments(void) const;
    int ChooseSegment(int stream) const;
    int64_t SizeMedia(void) const;
    void WaitUntilBuffered(void);
    void SanitizeStreams(StreamsList *streams = NULL);

    // private member variables
    QString             m_m3u8;     // M3U8 url
    QByteArray          m_peeked;

    HLSPlayback        *m_playback;

    /* state */
    StreamsList         m_streams;  // bandwidth adaptation
    mutable QMutex      m_lock;     // protect general class members
    bool                m_meta;     // meta playlist
    bool                m_error;    // parsing error
    bool                m_aesmsg;   // only print one time that the media is encrypted
    int                 m_startup;  // starting segment (where seek start)
    /**
     * assumed bitrate of playback
     * used for the purpose of calculating length and seek position.
     * the value itself is irrelevant, as it's only used as a common reference
     */
    int64_t             m_bitrate;
    /**
     * FFmpeg seek to the end of the stream in order to determine the length
     * of the video. Set to boolean to true after we detected a seek to the end
     * this will prevent waiting for new data in safe_read
     */
    bool                m_seektoend;

    friend class StreamWorker;
    StreamWorker       *m_streamworker;
    friend class PlaylistWorker;
    PlaylistWorker     *m_playlistworker;
    FILE               *m_fd;
    bool                m_interrupted;
    bool                m_killed;
};

#endif
