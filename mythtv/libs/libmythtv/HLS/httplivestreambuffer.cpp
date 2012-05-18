/*****************************************************************************
 * httplivestreambuffer.cpp
 * MythTV
 *
 * Created by Jean-Yves Avenard on 6/05/12.
 * Copyright (c) 2012 Bubblestuff Pty Ltd. All rights reserved.
 *
 * Based on httplive.c by Jean-Paul Saman <jpsaman _AT_ videolan _DOT_ org>
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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtAlgorithms>
#include <QUrl>

#include <sys/time.h> // for gettimeofday

#include "mthread.h"
#include "httplivestreambuffer.h"
#include "mythdownloadmanager.h"
#include "mythlogging.h"

#ifdef USING_LIBCRYPTO
// encryption related stuff
#include <openssl/aes.h>
#define AES_BLOCK_SIZE 16       // HLS only support AES-128
#endif

#define LOC QString("HLSBuffer: ")

// Constants
#define PLAYBACK_MINBUFFER 2    // number of segments to prefetch before playback starts
#define PLAYBACK_READAHEAD 6    // number of segments download queue ahead of playback

enum
{
    RET_ERROR = -1,
    RET_OK    = 0,
};

/* utility methods */

static QString relative_URI(QString &surl, QString &spath)
{
    QUrl url  = QUrl(surl);
    QUrl path = QUrl(spath);

    if (!path.isRelative())
    {
        return spath;
    }
    return url.resolved(path).toString();
}

static uint64_t mdate(void)
{
    timeval  t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000ULL + t.tv_usec;
}

static bool downloadURL(QString url, QByteArray *buffer)
{
    MythDownloadManager *mdm = GetMythDownloadManager();
    return mdm->download(url, buffer);
}

/* segment container */

class HLSSegment
{
public:
    HLSSegment(const int mduration, const int id, QString &title,
               QString &uri, QString &current_key_path)
    {
        m_duration      = mduration; /* seconds */
        m_id            = id;
        m_bandwidth     = 0;
        m_url           = uri;
        m_played        = 0;
        m_title         = title;
#ifdef USING_LIBCRYPTO
        m_keyloaded     = false;
        m_psz_key_path  = current_key_path;
#endif
    }

    HLSSegment(const HLSSegment &rhs)
    {
        *this = rhs;
    }

    ~HLSSegment()
    {
    }

    HLSSegment &operator=(const HLSSegment &rhs)
    {
        if (this == &rhs)
            return *this;
        m_id            = rhs.m_id;
        m_duration      = rhs.m_duration;
        m_bandwidth     = rhs.m_bandwidth;
        m_url           = rhs.m_url;
        // keep the old data downloaded
        m_data          = m_data;
        m_played        = m_played;
        m_title         = rhs.m_title;
#ifdef USING_LIBCRYPTO
        m_psz_key_path  = rhs.m_psz_key_path;
        memcpy(&m_aeskey, &(rhs.m_aeskey), sizeof(m_aeskey));
        m_keyloaded     = rhs.m_keyloaded;
#endif
        return *this;
    }

    int Duration(void)
    {
        return m_duration;
    }

    int Id(void)
    {
        return m_id;
    }

    void Lock(void)
    {
        m_lock.lock();
    }

    void Unlock(void)
    {
        m_lock.unlock();
    }

    bool IsEmpty(void)
    {
        return m_data.isEmpty();
    }

    int32_t Size(void)
    {
        return m_data.size();
    }

    int Download(void)
    {
        // must own lock
        bool ret = downloadURL(m_url, &m_data);
        // didn't succeed, clear buffer
        if (!ret)
        {
            m_data.clear();
            return RET_ERROR;
        }
        return RET_OK;
    }

    QString Url(void)
    {
        return m_url;
    }

    int32_t SizePlayed(void)
    {
        return m_played;
    }

    uint32_t Read(uint8_t *buffer, int32_t length)
    {
        int32_t left = m_data.size() - m_played;
        if (length > left)
        {
            length = left;
        }
        if (buffer != NULL)
        {
            memcpy(buffer, m_data.constData() + m_played, length);
        }
        m_played += length;
        return length;
    }

    void Reset(void)
    {
        m_played = 0;
    }

    void Clear(void)
    {
        m_played = 0;
        m_data.clear();
    }

    QString Title(void)
    {
        return m_title;
    }
    void SetTitle(QString &x)
    {
        m_title = x;
    }
    /**
     * provides pointer to raw segment data
     */
    const char *Data(void)
    {
        return m_data.constData();
    }

#ifdef USING_LIBCRYPTO
    int DownloadKey(void)
    {
        // must own lock
        if (m_keyloaded)
            return RET_OK;
        QByteArray key;
        bool ret = downloadURL(m_psz_key_path, &key);
        if (ret != RET_OK || key.size() != AES_BLOCK_SIZE)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("The AES key loaded doesn't have the right size (%1)")
                .arg(key.size()));
            return RET_ERROR;
        }
        AES_set_decrypt_key((const unsigned char*)key.constData(), 128, &m_aeskey);
        m_keyloaded = true;
        return RET_OK;
    }

    int DecodeData(uint8_t *IV)
    {
        /* Decrypt data using AES-128 */
        int aeslen = m_data.size() & ~0xf;
        unsigned char iv[AES_BLOCK_SIZE];
        char *decrypted_data = new char[aeslen];
        if (IV == NULL)
        {
            /*
             * If the EXT-X-KEY tag does not have the IV attribute, implementations
             * MUST use the sequence number of the media file as the IV when
             * encrypting or decrypting that media file.  The big-endian binary
             * representation of the sequence number SHALL be placed in a 16-octet
             * buffer and padded (on the left) with zeros.
             */
            memset(iv, 0, AES_BLOCK_SIZE);
            iv[15] = m_id         & 0xff;
            iv[14] = (m_id >> 8)  & 0xff;
            iv[13] = (m_id >> 16) & 0xff;
            iv[12] = (m_id >> 24) & 0xff;
        }
        else
        {
            memcpy(iv, IV, sizeof(iv));
        }
        AES_cbc_encrypt((unsigned char*)m_data.constData(),
                        (unsigned char*)decrypted_data, aeslen,
                        &m_aeskey, iv, AES_DECRYPT);
        memcpy(decrypted_data + aeslen, m_data.constData() + aeslen,
               m_data.size() - aeslen);

        // remove the PKCS#7 padding from the buffer
        int pad = m_data[m_data.size()-1];
        if (pad <= 0 || pad > AES_BLOCK_SIZE)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("bad padding character (0x%1)").arg(pad, 0, 16, QLatin1Char('0')));
            return RET_ERROR;
        }
        aeslen = m_data.size() - pad;
        m_data = QByteArray(decrypted_data, aeslen);
        delete[] decrypted_data;

        return RET_OK;
    }

    bool HasKeyPath(void)
    {
        return !m_psz_key_path.isEmpty();
    }

    bool KeyLoaded(void)
    {
        return m_keyloaded;
    }

    QString KeyPath(void)
    {
        return m_psz_key_path;
    }

    void SetKeyPath(QString &path)
    {
        m_psz_key_path = path;
    }

    void CopyAESKey(HLSSegment &segment)
    {
        memcpy(&m_aeskey, &(segment.m_aeskey), sizeof(m_aeskey));
        m_keyloaded = segment.m_keyloaded;
    }
private:
    AES_KEY     m_aeskey;       // AES-128 key
    bool        m_keyloaded;
#endif

private:
    int         m_id;           // unique sequence number
    int         m_duration;     // segment duration (seconds)
    uint64_t    m_bandwidth;    // bandwidth usage of segments (bits per second)
    QString     m_title;        // human-readable informative title of the media segment

    QString     m_url;
    QString     m_psz_key_path; // URL key path
    QByteArray  m_data;         // raw data
    int32_t     m_played;       // bytes counter of data already read from segment
    QMutex      m_lock;
};

/* stream class */

class HLSStream
{
public:
    HLSStream(const int mid, const uint64_t bw, QString &uri)
    {
        m_id            = mid;
        m_bandwidth     = bw;
        m_targetduration= -1;   // not known yet
        m_size          = 0;
        m_startsequence = 0;    // default is 0
        m_version       = 1;    // default protocol version
        m_cache         = true;
        m_url           = uri;
#ifdef USING_LIBCRYPTO
        m_ivloaded      = false;
#endif
    }

    HLSStream(HLSStream &rhs, bool copy = true)
    {
        (*this) = rhs;
        if (!copy)
            return;
        // copy all the segments across
        QList<HLSSegment*>::iterator it = m_segments.begin();
        for (; it != m_segments.end(); ++it)
        {
            const HLSSegment *old = *it;
            HLSSegment *segment = new HLSSegment(*old);
            AppendSegment(segment);
        }
    }

    ~HLSStream()
    {
        QList<HLSSegment*>::iterator it = m_segments.begin();
        for (; it != m_segments.end(); ++it)
        {
            delete *it;
        }
    }

    HLSStream &operator=(const HLSStream &rhs)
    {
        if (this == &rhs)
            return *this;
        // do not copy segments
        m_id            = rhs.m_id;
        m_version       = rhs.m_version;
        m_startsequence = rhs.m_startsequence;
        m_targetduration= rhs.m_targetduration;
        m_bandwidth     = rhs.m_bandwidth;
        m_size          = rhs.m_size;
        m_url           = rhs.m_url;
        m_cache         = rhs.m_cache;
#ifdef USING_LIBCRYPTO
        m_keypath       = rhs.m_keypath;
        m_ivloaded      = rhs.m_ivloaded;
#endif
        return *this;
    }

    static bool IsGreater(const HLSStream *s1, const HLSStream *s2)
    {
        return s1->Bandwidth() > s2->Bandwidth();
    }

    bool operator<(HLSStream &b)
    {
        return this->Bandwidth() < b.Bandwidth();
    }

    bool operator>(HLSStream &b)
    {
        return this->Bandwidth() > b.Bandwidth();
    }

    /**
     * Return the estimated size of the stream in bytes
     * if a segment hasn't been downloaded, its size is estimated according
     * to the bandwidth of the stream
     */
    uint64_t Size(bool force = false)
    {
        if (m_size > 0 && !force)
            return m_size;
        QMutexLocker lock(&m_lock);

        int64_t size = 0;
        int count = NumSegments();
        for (int i = 0; i < count; i++)
        {
            HLSSegment *segment    = GetSegment(i);
            segment->Lock();
            if (segment->Size() > 0)
            {
                size += (int64_t)segment->Size();
            }
            else
            {
                size += segment->Duration() * Bandwidth() / 8;
            }
            segment->Unlock();
        }
        m_size = size;
        return m_size;
    }

    void Clear(void)
    {
        m_segments.clear();
    }

    int NumSegments(void)
    {
        return m_segments.size();
    }

    void AppendSegment(HLSSegment *segment)
    {
        // must own lock
        m_segments.append(segment);
    }

    HLSSegment *GetSegment(const int wanted)
    {
        int count = NumSegments();
        if (count <= 0)
            return NULL;
        if ((wanted < 0) || (wanted >= count))
            return NULL;
        return m_segments[wanted];
    }

    HLSSegment *FindSegment(const int id)
    {
        int count = NumSegments();
        if (count <= 0)
            return NULL;
        for (int n = 0; n < count; n++)
        {
            HLSSegment *segment = GetSegment(n);
            if (segment == NULL)
                break;
            if (segment->Id() == id)
                return segment;
        }
        return NULL;
    }

    /**
     * return the size in bytes of the wanted segment or -1 if segment doesn't
     * exist.
     * If the segment hasn't been downloaded yet, estimate its size according
     * to the stream bandwidth
     */
    int64_t GetSegmentLength(int wanted)
    {
        QMutexLocker lock(&m_lock);
        int length;
        HLSSegment *segment = GetSegment(wanted);
        if (segment == NULL)
        {
            return -1;
        }
        segment->Lock();
        if (segment->Size() > 0)
        {
            length = segment->Size();
        }
        else
        {
            length = segment->Duration() * Bandwidth() / 8;
        }
        segment->Unlock();
        return length;
    }

    void AddSegment(const int duration, QString &title, QString &uri)
    {
        QMutexLocker lock(&m_lock);
        QString psz_uri = relative_URI(m_url, uri);
        int id = NumSegments() + m_startsequence;
#ifndef USING_LIBCRYPTO
        QString m_keypath;
#endif
        HLSSegment *segment = new HLSSegment(duration, id, title, psz_uri,
                                             m_keypath);
        AppendSegment(segment);
    }

    void RemoveSegment(HLSSegment *segment, bool willdelete = true)
    {
        QMutexLocker lock(&m_lock);
        if (willdelete)
        {
            delete segment;
        }
        int count = NumSegments();
        if (count <= 0)
            return;
        for (int n = 0; n < count; n++)
        {
            HLSSegment *old = GetSegment(n);
            if (old == segment)
            {
                m_segments.removeAt(n);
                break;
            }
        }
        return;
    }

    void RemoveListSegments(QMap<HLSSegment*,bool> &table)
    {
        QMap<HLSSegment*,bool>::iterator it;
        for (it = table.begin(); it != table.end(); ++it)
        {
            bool todelete   = *it;
            HLSSegment *p  = it.key();
            RemoveSegment(p, todelete);
        }
    }

    int DownloadSegmentData(int segmentnum, uint64_t &bandwidth, int stream)
    {
        HLSSegment *segment = GetSegment(segmentnum);
        if (segment == NULL)
            return RET_ERROR;

        segment->Lock();
        if (!segment->IsEmpty())
        {
            /* Segment already downloaded */
            segment->Unlock();
            return RET_OK;
        }

        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("started download of segment %1/%2 using stream %3")
            .arg(segment->Id()).arg(NumSegments()+m_startsequence).arg(stream));

        /* sanity check - can we download this segment on time? */
        if ((bandwidth > 0) && (m_bandwidth > 0))
        {
            uint64_t size = (segment->Duration() * m_bandwidth); /* bits */
            int estimated = (int)(size / bandwidth);
            if (estimated > segment->Duration())
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("downloading of segment %1 will take %2s, "
                            "which is longer than its playback (%3s) at %4bit/s")
                    .arg(segment->Id())
                    .arg(estimated)
                    .arg(segment->Duration())
                    .arg(bandwidth));
            }
        }

        uint64_t start = mdate();
        if (segment->Download() != RET_OK)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("downloaded segment %1 from stream %2 failed")
                .arg(segment->Id()).arg(m_id));
            segment->Unlock();
            return RET_ERROR;
        }

        uint64_t downloadduration = mdate() - start;
        if (m_bandwidth == 0 && segment->Duration() > 0)
        {
            /* Try to estimate the bandwidth for this stream */
            m_bandwidth = (uint64_t)(((double)segment->Size() * 8) /
                                     ((double)segment->Duration()));
        }

#ifdef USING_LIBCRYPTO
        /* If the segment is encrypted, decode it */
        if (segment->HasKeyPath())
        {
            /* Do we have loaded the key ? */
            if (!segment->KeyLoaded())
            {
                if (ManageSegmentKeys() != RET_OK)
                {
                    LOG(VB_PLAYBACK, LOG_ERR, LOC +
                        "couldn't retrieve segment AES-128 key");
                    segment->Unlock();
                    return RET_OK;
                }
            }
            if (segment->DecodeData(m_ivloaded ? m_AESIV : NULL) != RET_OK)
            {
                segment->Unlock();
                return RET_ERROR;
            }
        }
#endif
        segment->Unlock();

        downloadduration = downloadduration < 1 ? 1 : downloadduration;
        bandwidth = segment->Size() * 8 * 1000000ULL / downloadduration; /* bits / s */
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("downloaded segment %1 took %2ms for %3 bytes: bandwidth:%4kiB/s")
            .arg(segment->Id())
            .arg(downloadduration / 1000)
            .arg(segment->Size())
            .arg(bandwidth / 8192.0));

        return RET_OK;
    }
    int Id(void) const
    {
        return m_id;
    }
    int Version(void)
    {
        return m_version;
    }
    void SetVersion(int x)
    {
        m_version = x;
    }
    int StartSequence(void)
    {
        return m_startsequence;
    }
    void SetStartSequence(int x)
    {
        m_startsequence = x;
    }
    int TargetDuration(void)
    {
        return m_targetduration;
    }
    void SetTargetDuration(int x)
    {
        m_targetduration = x;
    }
    uint64_t Bandwidth(void) const
    {
        return m_bandwidth;
    }
    bool Cache(void)
    {
        return m_cache;
    }
    void SetCache(bool x)
    {
        m_cache = x;
    }
    void Lock(void)
    {
        m_lock.lock();
    }
    void Unlock(void)
    {
        m_lock.unlock();
    }
    QString Url(void)
    {
        return m_url;
    }
    void UpdateWith(HLSStream &upd)
    {
        QMutexLocker lock(&m_lock);
        m_targetduration    = upd.m_targetduration < 0 ?
                                m_targetduration : upd.m_targetduration;
        m_cache             = upd.m_cache;
    }
#ifdef USING_LIBCRYPTO
    /**
     * Will download all required segment AES-128 keys
     * Will try to re-use already downloaded keys if possible
     */
    int ManageSegmentKeys()
    {
        HLSSegment   *seg       = NULL;
        HLSSegment   *prev_seg  = NULL;
        int          count      = NumSegments();

        for (int i = 0; i < count; i++)
        {
            prev_seg = seg;
            seg = GetSegment(i);
            if (seg == NULL )
                continue;
            if (!seg->HasKeyPath())
                continue;   /* No key to load ? continue */
            if (seg->KeyLoaded())
                continue;   /* The key is already loaded */

            /* if the key has not changed, and already available from previous segment,
             * try to copy it, and don't load the key */
            if (prev_seg && prev_seg->KeyLoaded() &&
                (seg->KeyPath() == prev_seg->KeyPath()))
            {
                seg->CopyAESKey(*prev_seg);
                continue;
            }
            if (seg->DownloadKey() != RET_OK)
                return RET_ERROR;
        }
        return RET_OK;
    }
    bool SetAESIV(QString &line)
    {
        /*
         * If the EXT-X-KEY tag has the IV attribute, implementations MUST use
         * the attribute value as the IV when encrypting or decrypting with that
         * key.  The value MUST be interpreted as a 128-bit hexadecimal number
         * and MUST be prefixed with 0x or 0X.
         */
        if (!line.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            return false;
        if (line.size() % 2)
        {
            // not even size, pad with front 0
            line.insert(2, QLatin1String("0"));
        }
        int padding = max(0, AES_BLOCK_SIZE - (line.size() - 2));
        QByteArray ba = QByteArray(padding, 0x0);
        ba.append(QByteArray::fromHex(QByteArray(line.toAscii().constData() + 2)));
        memcpy(m_AESIV, ba.constData(), ba.size());
        m_ivloaded = true;
        return true;
    }
    uint8_t *AESIV(void)
    {
        return m_AESIV;
    }
    void SetKeyPath(QString &x)
    {
        m_keypath = x;
    }
private:
    QString     m_keypath;              // URL path of the encrypted key
    bool        m_ivloaded;
    uint8_t     m_AESIV[AES_BLOCK_SIZE];// IV used when decypher the block
#endif

private:
    int         m_id;                   // program id
    int         m_version;              // protocol version should be 1
    int         m_startsequence;        // media starting sequence number
    int         m_targetduration;       // maximum duration per segment (s)
    uint64_t    m_bandwidth;            // bandwidth usage of segments (bits per second)
    uint64_t    m_size;                 // stream length is calculated by taking the sum
                                        // foreach segment of (segment->duration * hls->bandwidth/8)

    QList<HLSSegment*> m_segments;      // list of segments
    QString     m_url;                  // uri to m3u8
    QMutex      m_lock;
    bool        m_cache;                // allow caching
};

// Playback Stream Information
class HLSPlayback
{
public:
    HLSPlayback(void) : m_offset(0), m_stream(0), m_segment(0)
    {
    }
    /* offset is only used from main thread, no need for locking */
    uint64_t Offset(void)
    {
        return m_offset;
    }
    void SetOffset(uint64_t val)
    {
        m_offset = val;
    }
    void AddOffset(uint64_t val)
    {
        m_offset += val;
    }
    int Stream(void)
    {
        QMutexLocker lock(&m_lock);
        return m_stream;
    }
    void SetStream(int val)
    {
        QMutexLocker lock(&m_lock);
        m_stream = val;
    }
    int Segment(void)
    {
        QMutexLocker lock(&m_lock);
        return m_segment;
    }
    void SetSegment(int val)
    {
        QMutexLocker lock(&m_lock);
        m_segment = val;
    }
    int IncrSegment(void)
    {
        QMutexLocker lock(&m_lock);
        return ++m_segment;
    }

private:
    uint64_t        m_offset;   // current offset in media
    int             m_stream;   // current HLSStream
    int             m_segment;  // current segment for playback
    QMutex          m_lock;
};

// Stream Download Thread
class StreamWorker : public MThread
{
public:
    StreamWorker(HLSRingBuffer *parent, int startup, int buffer) : MThread("HLSStream"),
        m_parent(parent), m_interrupted(false), m_bandwidth(0), m_stream(0),
        m_segment(startup), m_seek(-1), m_buffer(buffer),
        m_sumbandwidth(0.0), m_countbandwidth(0)
    {
    }
    void Cancel(void)
    {
        m_interrupted = true;
        Wakeup();
        wait();
    }
    int CurrentStream(void)
    {
        QMutexLocker lock(&m_lock);
        return m_stream;
    }
    int Segment(void)
    {
        QMutexLocker lock(&m_lock);
        return m_segment;
    }
    void Seek(int val)
    {
        QMutexLocker lock(&m_lock);
        m_seek = val;
        Wakeup();
    }
    bool IsSeeking(void)
    {
        // must own lock
        return m_seek != -1;
    }
    bool SetFromSeek(void)
    {
        // must own lock
        if (m_seek < 0)
            return false;
        m_segment = m_seek;
        m_seek = -1;
        return true;
    }
    bool IsAtEnd(bool lock = false)
    {
        if (lock)
        {
            m_lock.lock();
        }
        int count = m_parent->NumSegments();
        bool ret = m_segment >= count;
        if (lock)
        {
            m_lock.unlock();
        }
        return ret;
    }
    int CurrentPlaybackBuffer(bool lock = true)
    {
        if (lock)
        {
            m_lock.lock();
        }
        int ret = m_segment - m_parent->m_playback->Segment();
        if (lock)
        {
            m_lock.unlock();
        }
        return ret;
    }
    void SetBuffer(int val)
    {
        QMutexLocker lock(&m_lock);
        m_buffer = val;
    }
    void AddSegmentToStream(int segnum, int stream)
    {
        QMutexLocker lock(&m_lock);
        m_segmap.insert(segnum, stream);
    }
    /**
     * return the stream used to download a particular segment
     * or -1 if it was never downloaded
     */
    int StreamForSegment(int segmentid, bool lock = true) const
    {
        if (lock)
        {
            m_lock.lock();
        }
        int ret;
        if (!m_segmap.contains(segmentid))
        {
            ret = -1; // we never downloaded that segment on any streams
        }
        else
        {
            ret = m_segmap[segmentid];
        }
        if (lock)
        {
            m_lock.unlock();
        }
        return ret;
    }

    void Wakeup(void)
    {
        // send a wake signal
        m_waitcond.wakeAll();
    }
    void WaitForSignal(unsigned long time = ULONG_MAX)
    {
        // must own lock
        m_waitcond.wait(&m_lock, time);
    }
    void Lock(void)
    {
        m_lock.lock();
    }
    void Unlock(void)
    {
        m_lock.unlock();
    }
    uint64_t Bandwidth(void)
    {
        return m_bandwidth;
    }
    double AverageNewBandwidth(int64_t bandwidth)
    {
        m_sumbandwidth += bandwidth;
        m_countbandwidth++;
        m_bandwidth = m_sumbandwidth / m_countbandwidth;
        return m_bandwidth;
    }

protected:
    void run(void)
    {
        int retries = 0;
        while (!m_interrupted)
        {
            /*
             * we can go into waiting if:
             * - not live and download is more than 3 segments ahead of playback
             * - we are at the end of the stream
             */
            Lock();
            int playsegment = m_parent->m_playback->Segment();
            if ((!m_parent->m_live && (playsegment < m_segment - m_buffer)) ||
                IsAtEnd())
            {
                /* wait until
                 * 1- got interrupted
                 * 2- we are less than 6 segments ahead of playback
                 * 3- got asked to seek to a particular segment */
                while (!m_interrupted && !IsSeeking() &&
                       (((m_segment - playsegment) > m_buffer) || IsAtEnd()))
                {
                    WaitForSignal();
                    // do we have new segments available added by PlaylistWork?
                    if (m_parent->m_live && !IsAtEnd())
                        break;
                    playsegment = m_parent->m_playback->Segment();
                }
                SetFromSeek();
            }
            Unlock();

            if (m_interrupted)
            {
                Wakeup();
                break;
            }
            // have we already downloaded the required segment?
            if (StreamForSegment(m_segment) < 0)
            {
                HLSStream *hls = m_parent->GetStream(m_stream);
                uint64_t bw = m_bandwidth;
                int err = hls->DownloadSegmentData(m_segment, bw, m_stream);
                bw = AverageNewBandwidth(bw);
                if (err != RET_OK)
                {
                    if (m_interrupted)
                        break;
                    retries++;
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("download failed, retry #%1").arg(retries));
                    // should test which error, in the mean time, just retry
                    if (!m_parent->m_meta)
                        continue; // no other stream to default to
                    // error, should try another stream
                    continue;
                }

                AddSegmentToStream(m_segment, m_stream);
                if (m_parent->m_meta && hls->Bandwidth() != bw)
                {
                    int newstream = m_parent->BandwidthAdaptation(hls->Id(), bw);

                    if (newstream >= 0 && newstream != m_stream)
                    {
                        LOG(VB_PLAYBACK, LOG_INFO, LOC +
                            QString("detected %1 bandwidth %2 stream changing "
                                    "from stream %3 to stream %4")
                            .arg(bw >= hls->Bandwidth() ? "faster" : "lower")
                            .arg(bw).arg(m_stream).arg(newstream));
                        m_stream = newstream;
                    }
                }
            }
            Lock();
            if (!SetFromSeek())
            {
                m_segment++;
            }
            Unlock();
            // Signal we're done
            Wakeup();
        }
    }

private:
    HLSRingBuffer  *m_parent;
    bool            m_interrupted;
    uint64_t        m_bandwidth;// measured average download bandwidth (bits per second)
    int             m_stream;   // current HLSStream
    int             m_segment;  // current segment for downloading
    int             m_seek;     // segment requested by seek (default -1)
    int             m_buffer;   // buffer kept between download and playback
    QMap<int,int>   m_segmap;   // segment with streamid used for download
    mutable QMutex  m_lock;
    QWaitCondition  m_waitcond;
    double          m_sumbandwidth;
    int             m_countbandwidth;
};

// Playlist Refresh Thread
class PlaylistWorker : public MThread
{
public:
    PlaylistWorker(HLSRingBuffer *parent, int64_t wait) : MThread("HLSStream"),
        m_parent(parent), m_interrupted(false), m_tries(0)
    {
        m_last   = mdate();
        m_wakeup = m_last + wait;
    }
    void Cancel()
    {
        m_interrupted = true;
        wait();
    }

protected:
    void run(void)
    {
        double wait = 0.5;

        while (!m_interrupted)
        {
            if (m_parent->m_streamworker == NULL)
            {
                // streamworker not running
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    "StreamWorker not running, aborting live playback");
                m_interrupted = true;
                break;
            }
            int64_t now = mdate();
            if (now >= m_wakeup)
            {
                /* reload the m3u8 */
                if (ReloadPlaylist() != RET_OK)
                {
                    /* No change in playlist, then backoff */
                    m_tries++;
                    if (m_tries == 1)       wait = 0.5;
                    else if (m_tries == 2)  wait = 1;
                    else if (m_tries >= 3)  wait = 2;

                    /* Can we afford to backoff? */
                    if (m_parent->m_streamworker->CurrentPlaybackBuffer() < 3)
                    {
                        if (m_tries == 0)
                            continue; // restart immediately if it's the first try
                        m_tries = 0;
                        wait = 0.5;
                    }
                }
                else
                {
                    // make streamworker process things
                    m_parent->m_streamworker->Wakeup();
                    m_tries = 0;
                    wait = 0.5;
                }

                HLSStream *hls = m_parent->CurrentStream();
                if (hls == NULL)
                {
                    // an irrevocable error has occured. exit
                    LOG(VB_PLAYBACK, LOG_ERR, LOC +
                        "unable to retrieve current stream, aborting live playback");
                    m_interrupted = true;
                    break;
                }

                /* determine next time to update playlist */
                m_last   = now;
                m_wakeup = now + ((int64_t)(hls->TargetDuration() * wait)
                                  * (int64_t)1000000);
            }
            usleep(1000000); // sleep 1s
        }
    }

private:
    /**
     * Reload playlist
     */
    int ReloadPlaylist(void)
    {
        StreamsList *streams = new StreamsList;

        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Reloading HLS live meta playlist");

        if (GetHTTPLiveMetaPlaylist(streams) != RET_OK)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "reloading playlist failed");
            m_parent->FreeStreamsList(streams);
            return RET_ERROR;
        }

        /* merge playlists */
        int count = streams->size();
        for (int n = 0; n < count; n++)
        {
            HLSStream *hls_new = m_parent->GetStream(n, streams);
            if (hls_new == NULL)
                continue;

            HLSStream *hls_old = m_parent->FindStream(hls_new);
            if (hls_old == NULL)
            {   /* new hls stream - append */
                m_parent->m_streams.append(hls_new);
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("new HLS stream appended (id=%1, bandwidth=%2)")
                    .arg(hls_new->Id()).arg(hls_new->Bandwidth()));
            }
            else if (UpdatePlaylist(hls_new, hls_old) != RET_OK)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("failed updating HLS stream (id=%1, bandwidth=%2)")
                    .arg(hls_new->Id()).arg(hls_new->Bandwidth()));
                m_parent->FreeStreamsList(streams);
                return RET_ERROR;
            }
        }
        delete streams;
        return RET_OK;
    }

    int UpdatePlaylist(HLSStream *hls_new, HLSStream *hls)
    {
        int count = hls_new->NumSegments();

        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("updated hls stream (program-id=%1, bandwidth=%2) has %3 segments")
            .arg(hls_new->Id()).arg(hls_new->Bandwidth()).arg(count));
        QMap<HLSSegment*,bool> table;

        for (int n = 0; n < count; n++)
        {
            HLSSegment *p = hls_new->GetSegment(n);
            if (p == NULL)
                return RET_ERROR;

            hls->Lock();
            HLSSegment *segment = hls->FindSegment(p->Id());
            if (segment)
            {
                segment->Lock();
                /* they should be the same */
                if ((p->Id() != segment->Id()) ||
                    (p->Duration() != segment->Duration()) ||
                    (p->Url() != segment->Url()))
                {
                    LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                        QString("existing segment found with different content - resetting"));
                    LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                        QString("-       id: new=%1, old=%2")
                        .arg(p->Id()).arg(segment->Id()));
                    LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                        QString("- duration: new=%1, old=%2")
                        .arg(p->Duration()).arg(segment->Duration()));
                    LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                        QString("-     file: new=%1 old=%2")
                        .arg(p->Url()).arg(segment->Url()));

                    /* Resetting content */
                    *segment = *p;
                }
                // mark segment to be removed from new stream, and deleted
                table.insert(p, true);
                segment->Unlock();
            }
            else
            {
                // check if it was considered a VOD playlist before
                // switch to live mode if changes are detected
                // Some provider use the #EXT-X-ENDLIST incorrectly
                if (!m_parent->m_live)
                {
                    m_parent->m_live     = true;
                    m_parent->m_falsevod = true;
                    LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                        "switching to live mode, HLS stream incorrectly tagged "
                        "stream as VOD");
                }
                int last = hls->NumSegments() - 1;
                HLSSegment *l = hls->GetSegment(last);
                if (l == NULL)
                {
                    hls->Unlock();
                    return RET_ERROR;
                }

                if ((l->Id() + 1) != p->Id())
                {
                    LOG(VB_PLAYBACK, LOG_ERR, LOC +
                        QString("gap in id numbers found: new=%1 expected %2")
                        .arg(p->Id()).arg(l->Id()+1));
                }
                hls->AppendSegment(p);
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("- segment %1 appended")
                    .arg(p->Id()));
                // segment was moved to another stream, so do not delete it
                table.insert(p, false);
            }
            hls->Unlock();
        }
        hls_new->RemoveListSegments(table);

        /* update meta information */
        hls->UpdateWith(*hls_new);
        return RET_OK;
    }

    int GetHTTPLiveMetaPlaylist(StreamsList *streams)
    {
        int err = RET_ERROR;

        /* Duplicate HLS stream META information */
        for (int i = 0; i < m_parent->m_streams.size(); i++)
        {
            HLSStream *src, *dst;
            src = m_parent->GetStream(i);
            if (src == NULL)
                return RET_ERROR;

            dst = new HLSStream(*src);
            streams->append(dst);

            /* Download playlist file from server */
            QByteArray buffer;
            if (!downloadURL(dst->Url(), &buffer))
            {
                return RET_ERROR;
            }
            /* Parse HLS m3u8 content. */
            err = m_parent->ParseM3U8(&buffer, streams);
        }
        return err;
    }

    // private variable members
    HLSRingBuffer * m_parent;
    bool            m_interrupted;
    int64_t         m_last;       /* playlist last loaded */
    int64_t         m_wakeup;     /* next reload time */
    int             m_tries;      /* times it was not changed */
};

HLSRingBuffer::HLSRingBuffer(const QString &lfilename) :
    RingBuffer(kRingBuffer_HLS),
    m_playback(new HLSPlayback()),
    m_cache(false),         m_meta(false),          m_live(true),
    m_falsevod(false),      m_error(false),         m_aesmsg(false),
    m_streamworker(NULL),   m_playlistworker(NULL)
{
    startreadahead = false;
    OpenFile(lfilename);
}

HLSRingBuffer::~HLSRingBuffer()
{
    QWriteLocker lock(&rwlock);

    if (m_playlistworker)
    {
        m_playlistworker->Cancel();
        delete m_playlistworker;
    }
    // stream worker must be deleted after playlist worker
    if (m_streamworker)
    {
        m_streamworker->Cancel();
        delete m_streamworker;
    }
    FreeStreamsList(&m_streams);
    delete m_playback;
}

void HLSRingBuffer::FreeStreamsList(StreamsList *streams)
{
    /* Free hls streams */
    for (int i = 0; i < streams->size(); i++)
    {
        HLSStream *hls;
        hls = GetStream(i, streams);
        if (hls)
        {
            delete hls;
        }
    }
    if (streams != &m_streams)
    {
        delete streams;
    }
}

HLSStream *HLSRingBuffer::GetStreamForSegment(int segnum)
{
    int stream = m_streamworker->StreamForSegment(segnum);
    if (stream < 0)
    {
        return CurrentStream();
    }
    return GetStream(stream);
}

HLSStream *HLSRingBuffer::GetStream(const int wanted, const StreamsList *streams) const
{
    if (streams == NULL)
    {
        streams = &m_streams;
    }
    int count = streams->size();
    if (count <= 0)
        return NULL;
    if ((wanted < 0) || (wanted >= count))
        return NULL;
    return streams->at(wanted);
}

HLSStream *HLSRingBuffer::GetFirstStream(const StreamsList *streams)
{
    return GetStream(0, streams);
}

HLSStream *HLSRingBuffer::GetLastStream(const StreamsList *streams)
{
    if (streams == NULL)
    {
        streams = &m_streams;
    }
    int count = streams->size();
    if (count <= 0)
        return NULL;
    count--;
    return GetStream(count, streams);
}

HLSStream *HLSRingBuffer::FindStream(const HLSStream *hls_new,
                                      const StreamsList *streams)
{
    if (streams == NULL)
    {
        streams = &m_streams;
    }
    int count = streams->size();
    for (int n = 0; n < count; n++)
    {
        HLSStream *hls = GetStream(n, streams);
        if (hls)
        {
            /* compare */
            if ((hls->Id() == hls_new->Id()) &&
                ((hls->Bandwidth() == hls_new->Bandwidth()) ||
                 (hls_new->Bandwidth() == 0)))
            {
                return hls;
            }
        }
    }
    return NULL;
}

/**
 * return the stream we are currently streaming from
 */
HLSStream *HLSRingBuffer::CurrentStream(void)
{
    if (!m_streamworker)
    {
        return NULL;
    }
    return GetStream(m_streamworker->CurrentStream());
}

int HLSRingBuffer::BandwidthAdaptation(int progid, uint64_t &bandwidth)
{
    int candidate = -1;
    uint64_t bw = bandwidth;
    uint64_t bw_candidate = 0;

    int count = m_streams.size();
    for (int n = 0; n < count; n++)
    {
        /* Select best bandwidth match */
        HLSStream *hls = GetStream(n);
        if (hls == NULL)
            break;

        /* only consider streams with the same PROGRAM-ID */
        if (hls->Id() == progid)
        {
            if ((bw >= hls->Bandwidth()) &&
                (bw_candidate < hls->Bandwidth()))
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("candidate stream %1 bandwidth (bits/s) %2 >= %3")
                    .arg(n).arg(bw).arg(hls->Bandwidth()));
                bw_candidate = hls->Bandwidth();
                candidate = n; /* possible candidate */
            }
        }
    }
    bandwidth = bw_candidate;
    return candidate;
}

bool HLSRingBuffer::IsHTTPLiveStreaming(QByteArray *s)
{
    if (!s || s->size() < 7)
        return false;

    if (!s->startsWith((const char*)"#EXTM3U"))
        return false;

    QTextStream stream(s);
    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    while (1)
    {
        QString line = stream.readLine();
        if (line.isNull())
            break;
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("IsHTTPLiveStreaming: %1").arg(line));
        if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION"))  ||
            line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE"))  ||
            line.startsWith(QLatin1String("#EXT-X-KEY"))             ||
            line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE"))     ||
            line.startsWith(QLatin1String("#EXT-X-ENDLIST"))         ||
            line.startsWith(QLatin1String("#EXT-X-STREAM-INF"))      ||
            line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY"))   ||
            line.startsWith(QLatin1String("#EXT-X-VERSION")))
        {
                return true;
        }
    }
    return false;
}

bool HLSRingBuffer::TestForHTTPLiveStreaming(QString &filename)
{
    bool isHLS = false;
    avcodeclock->lock();
    av_register_all();
    avcodeclock->unlock();
    URLContext *context;
    // Do a peek on the URL to test the format
    int ret = ffurl_open(&context, filename.toAscii(),
                         AVIO_FLAG_READ, NULL, NULL);
    if (ret >= 0)
    {
        unsigned char buffer[1024];
        ret = ffurl_read(context, buffer, sizeof(buffer));
        if (ret > 0)
        {
            QByteArray ba((const char*)buffer, ret);
            isHLS = IsHTTPLiveStreaming(&ba);
        }
        ffurl_close(context);
    }
    else
    {
        // couldn't peek, rely on URL analysis
        QUrl url = filename;
        isHLS =
        url.path().endsWith(QLatin1String("m3u8"), Qt::CaseInsensitive) ||
        QString(url.encodedQuery()).contains(QLatin1String("m3u8"), Qt::CaseInsensitive);
    }
    return isHLS;
}

/* Parsing */
QString HLSRingBuffer::ParseAttributes(QString &line, const char *attr)
{
    int p = line.indexOf(QLatin1String(":"));
    if (p < 0)
        return QString();

    QStringList list = QStringList(line.mid(p+1).split(','));
    QStringList::iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        QString arg = (*it).trimmed();
        if (arg.startsWith(attr))
        {
            int pos = arg.indexOf(QLatin1String("="));
            if (pos < 0)
                continue;
            return arg.mid(pos+1);
        }
    }
    return QString();
}

int HLSRingBuffer::ParseSegmentInformation(HLSStream *hls, QString &line,
                                            int &duration, QString &title)
{
    /*
     * #EXTINF:<duration>,<title>
     *
     * "duration" is an integer that specifies the duration of the media
     * file in seconds.  Durations SHOULD be rounded to the nearest integer.
     * The remainder of the line following the comma is the title of the
     * media file, which is an optional human-readable informative title of
     * the media segment
     */
    int p = line.indexOf(QLatin1String(":"));
    if (p < 0)
        return RET_ERROR;

    QStringList list = QStringList(line.mid(p+1).split(','));

    /* read duration */
    if (list.isEmpty())
    {
        return RET_ERROR;
    }
    QString val = list[0];
    bool ok;

    if (hls->Version() < 3)
    {
        duration = val.toInt(&ok);
        if (!ok)
        {
            duration = -1;
            return RET_ERROR;
        }
    }
    else
    {
        double d = val.toDouble(&ok);
        if (!ok)
        {
            duration = -1;
            return RET_ERROR;
        }
        if ((d) - ((int)d) >= 0.5)
            duration = ((int)d) + 1;
        else
            duration = ((int)d);
    }

    if (list.size() >= 2)
    {
        title = list[1];
    }

    /* Ignore the rest of the line */
    return RET_OK;
}

int HLSRingBuffer::ParseTargetDuration(HLSStream *hls, QString &line)
{
    /*
     * #EXT-X-TARGETDURATION:<s>
     *
     * where s is an integer indicating the target duration in seconds.
     */
    int duration       = -1;
    QByteArray ba      = line.toUtf8();
    const char *p_read = ba.constData();
    int ret = sscanf(p_read, "#EXT-X-TARGETDURATION:%d", &duration);
    if (ret != 1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "expected #EXT-X-TARGETDURATION:<s>");
        return RET_ERROR;
    }
    hls->SetTargetDuration(duration); /* seconds */
    return RET_OK;
}

HLSStream *HLSRingBuffer::ParseStreamInformation(QString &line, QString &uri)
{
    /*
     * #EXT-X-STREAM-INF:[attribute=value][,attribute=value]*
     *  <URI>
     */
    int id;
    uint64_t bw;
    QString attr;

    attr = ParseAttributes(line, "PROGRAM-ID");
    if (attr.isNull())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-STREAM-INF: expected PROGRAM-ID=<value>");
        return NULL;
    }
    id = attr.toInt();

    attr = ParseAttributes(line, "BANDWIDTH");
    if (attr.isNull())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-STREAM-INF: expected BANDWIDTH=<value>");
        return NULL;
    }
    bw = attr.toInt();

    if (bw == 0)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-STREAM-INF: bandwidth cannot be 0");
        return NULL;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("bandwidth adaptation detected (program-id=%1, bandwidth=%2")
        .arg(id).arg(bw));

    QString psz_uri = relative_URI(m_m3u8, uri);

    return new HLSStream(id, bw, psz_uri);
}

int HLSRingBuffer::ParseMediaSequence(HLSStream *hls, QString &line)
{
    /*
     * #EXT-X-MEDIA-SEQUENCE:<number>
     *
     * A Playlist file MUST NOT contain more than one EXT-X-MEDIA-SEQUENCE
     * tag.  If the Playlist file does not contain an EXT-X-MEDIA-SEQUENCE
     * tag then the sequence number of the first URI in the playlist SHALL
     * be considered to be 0.
    */
    int sequence;
    QByteArray ba = line.toUtf8();
    const char *p_read = ba.constData();

    int ret = sscanf(p_read, "#EXT-X-MEDIA-SEQUENCE:%d", &sequence);
    if (ret != 1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "expected #EXT-X-MEDIA-SEQUENCE:<s>");
        return RET_ERROR;
    }

    if (hls->StartSequence() > 0)
    {
        if (!m_live)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("EXT-X-MEDIA-SEQUENCE already present in playlist (new=%1, old=%2)")
                .arg(sequence).arg(hls->StartSequence()));
        }
    }
    hls->SetStartSequence(sequence);
    return RET_OK;
}


int HLSRingBuffer::ParseKey(HLSStream *hls, QString &line)
{
    /*
     * #EXT-X-KEY:METHOD=<method>[,URI="<URI>"][,IV=<IV>]
     *
     * The METHOD attribute specifies the encryption method.  Two encryption
     * methods are defined: NONE and AES-128.
     */
    int err = 0;
    QString attr = ParseAttributes(line, "METHOD");
    if (attr.isNull())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-KEY: expected METHOD=<value>");
        return RET_ERROR;
    }

    if (attr.startsWith(QLatin1String("NONE")))
    {
        QString uri = ParseAttributes(line, "URI");
        if (!uri.isNull())
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-KEY: URI not expected");
            err = RET_ERROR;
        }
        /* IV is only supported in version 2 and above */
        if (hls->Version() >= 2)
        {
            QString iv = ParseAttributes(line, "IV");
            if (!iv.isNull())
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-KEY: IV not expected");
                err = RET_ERROR;
            }
        }
    }
#ifdef USING_LIBCRYPTO
    else if (attr.startsWith(QLatin1String("AES-128")))
    {
        QString uri, iv;
        if (m_aesmsg == false)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "playback of AES-128 encrypted HTTP Live media detected.");
            m_aesmsg = true;
        }
        uri = ParseAttributes(line, "URI");
        if (uri.isNull())
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "#EXT-X-KEY: URI not found for encrypted HTTP Live media in AES-128");
            return RET_ERROR;
        }

        /* Url is between quotes, remove them */
        hls->SetKeyPath(uri.remove(QChar(QLatin1Char('"'))));

        iv = ParseAttributes(line, "IV");
        if (!hls->SetAESIV(iv))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "invalid IV");
            err = RET_ERROR;
        }
    }
#endif
    else
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "invalid encryption type, only NONE "
#ifdef USING_LIBCRYPTO
            "and AES-128 are supported"
#else
            "is supported."
#endif
            );
        err = RET_ERROR;
    }
    return err;
}

int HLSRingBuffer::ParseProgramDateTime(HLSStream *hls, QString &line)
{
    /*
     * #EXT-X-PROGRAM-DATE-TIME:<YYYY-MM-DDThh:mm:ssZ>
     */
    LOG(VB_PLAYBACK, LOG_WARNING, LOC +
        QString("tag not supported: #EXT-X-PROGRAM-DATE-TIME %1")
        .arg(line));
    return RET_OK;
}

int HLSRingBuffer::ParseAllowCache(HLSStream *hls, QString &line)
{
    /*
     * The EXT-X-ALLOW-CACHE tag indicates whether the client MAY or MUST
     * NOT cache downloaded media files for later replay.  It MAY occur
     * anywhere in the Playlist file; it MUST NOT occur more than once.  The
     * EXT-X-ALLOW-CACHE tag applies to all segments in the playlist.  Its
     * format is:
     *
     * #EXT-X-ALLOW-CACHE:<YES|NO>
     */
    int pos = line.indexOf(QLatin1String(":"));
    if (pos < 0)
        return RET_ERROR;
    QString answer = line.mid(pos+1, 3);
    if (answer.size() < 2)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "#EXT-X-ALLOW-CACHE, ignoring ...");
        return RET_ERROR;
    }
    hls->SetCache(!answer.startsWith(QLatin1String("NO")));
    return RET_OK;
}

int HLSRingBuffer::ParseVersion(QString &line, int &version)
{
    /*
     * The EXT-X-VERSION tag indicates the compatibility version of the
     * Playlist file.  The Playlist file, its associated media, and its
     * server MUST comply with all provisions of the most-recent version of
     * this document describing the protocol version indicated by the tag
     * value.
     *
     * Its format is:
     *
     * #EXT-X-VERSION:<n>
     */
    QByteArray ba = line.toUtf8();
    const char *p_read = ba.constData();

    int ret = sscanf(p_read, "#EXT-X-VERSION:%d", &version);
    if (ret != 1)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "#EXT-X-VERSION: no protocol version found, should be version 1.");
        return RET_ERROR;
    }

    if (version <= 0 || version > 3)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("#EXT-X-VERSION should be version 1, 2 or 3 iso %1")
            .arg(version));
        return RET_ERROR;
    }
    return RET_OK;
}

int HLSRingBuffer::ParseEndList(HLSStream *hls)
{
    /*
     * The EXT-X-ENDLIST tag indicates that no more media files will be
     * added to the Playlist file.  It MAY occur anywhere in the Playlist
     * file; it MUST NOT occur more than once.  Its format is:
     */
    m_live = m_falsevod;
    if (!m_live)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "video on demand (vod) mode");
    }
    return RET_OK;
}

int HLSRingBuffer::ParseDiscontinuity(HLSStream *hls, QString &line)
{
    /* Not handled, never seen so far */
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("#EXT-X-DISCONTINUITY %1").arg(line));
    return RET_OK;
}

int HLSRingBuffer::ParseM3U8(const QByteArray *buffer, StreamsList *streams)
{
    /**
     * dThe http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8
     * document defines the following new tags: EXT-X-TARGETDURATION,
     * EXT-X-MEDIA-SEQUENCE, EXT-X-KEY, EXT-X-PROGRAM-DATE-TIME, EXT-X-
     * ALLOW-CACHE, EXT-X-STREAM-INF, EXT-X-ENDLIST, EXT-X-DISCONTINUITY,
     * and EXT-X-VERSION.
     */
    if (streams == NULL)
    {
        streams = &m_streams;
    }
    QTextStream stream(*buffer); stream.setCodec("UTF-8");

    QString line = stream.readLine();
    if (line.isNull())
        return RET_ERROR;

    if (!line.startsWith(QLatin1String("#EXTM3U")))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "missing #EXTM3U tag .. aborting");
        return RET_ERROR;
    }

    /* What is the version ? */
    int version = 1;
    int p = buffer->indexOf("#EXT-X-VERSION:");
    if (p >= 0)
    {
        stream.seek(p);
        QString psz_version = stream.readLine();
        if (psz_version.isNull())
            return RET_ERROR;
        int ret = ParseVersion(psz_version, version);
        if (ret != RET_OK)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "#EXT-X-VERSION: no protocol version found, assuming version 1.");
            version = 1;
        }
    }

    /* Is it a live stream ? */
    m_live = buffer->indexOf("#EXT-X-ENDLIST") < 0 ? true : m_falsevod;

    /* Is it a meta index file ? */
    bool meta = buffer->indexOf("#EXT-X-STREAM-INF") < 0 ? false : true;

    int err = RET_OK;

    if (meta)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Meta playlist");

        /* M3U8 Meta Index file */
        stream.seek(0); // rewind
        while (true)
        {
            line = stream.readLine();
            if (line.isNull())
                break;

            if (line.startsWith(QLatin1String("#EXT-X-STREAM-INF")))
            {
                m_meta = true;
                QString uri = stream.readLine();
                if (uri.isNull())
                {
                    err = RET_ERROR;
                    break;
                }
                if (uri.startsWith(QLatin1String("#")))
                {
                    LOG(VB_GENERAL, LOG_INFO, LOC +
                        QString("Skipping invalid stream-inf: %1")
                        .arg(uri));
                }
                else
                {
                    HLSStream *hls = ParseStreamInformation(line, uri);
                    if (hls)
                    {
                        streams->append(hls);
                        /* Download playlist file from server */
                        QByteArray buf;
                        bool ret = downloadURL(hls->Url(), &buf);
                        if (!ret)
                        {
                            err = RET_ERROR;
                            break;
                        }
                        /* Parse HLS m3u8 content. */
                        err = ParseM3U8(&buf, streams);
                        if (err != RET_OK)
                            break;
                        hls->SetVersion(version);
                    }
                }
            }
        }
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("%1 Playlist HLS protocol version: %2")
            .arg(m_live ? "Live": "VOD").arg(version));

        HLSStream *hls = NULL;
        if (m_meta)
            hls = GetLastStream(streams);
        else
        {
            /* No Meta playlist used */
            hls = new HLSStream(0, 0, m_m3u8);
            streams->append(hls);
            /* Get TARGET-DURATION first */
            p = buffer->indexOf("#EXT-X-TARGETDURATION:");
            if (p >= 0)
            {
                stream.seek(p);
                QString psz_duration = stream.readLine();
                if (psz_duration.isNull())
                    return RET_ERROR;
                err = ParseTargetDuration(hls, psz_duration);
            }
            /* Store version */
            hls->SetVersion(version);
        }

        // rewind
        stream.seek(0);
        /* */
        int segment_duration = -1;
        QString title;
        do
        {
            /* Next line */
            line = stream.readLine();
            if (line.isNull())
                break;
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("ParseM3U8: %1")
                .arg(line));

            if (line.startsWith(QLatin1String("#EXTINF")))
                err = ParseSegmentInformation(hls, line, segment_duration, title);
            else if (line.startsWith(QLatin1String("#EXT-X-TARGETDURATION")))
                err = ParseTargetDuration(hls, line);
            else if (line.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE")))
                err = ParseMediaSequence(hls, line);
            else if (line.startsWith(QLatin1String("#EXT-X-KEY")))
                err = ParseKey(hls, line);
            else if (line.startsWith(QLatin1String("#EXT-X-PROGRAM-DATE-TIME")))
                err = ParseProgramDateTime(hls, line);
            else if (line.startsWith(QLatin1String("#EXT-X-ALLOW-CACHE")))
                err = ParseAllowCache(hls, line);
            else if (line.startsWith(QLatin1String("#EXT-X-DISCONTINUITY")))
                err = ParseDiscontinuity(hls, line);
            else if (line.startsWith(QLatin1String("#EXT-X-VERSION")))
            {
                int version;
                err = ParseVersion(line, version);
                hls->SetVersion(version);
            }
            else if (line.startsWith(QLatin1String("#EXT-X-ENDLIST")))
                err = ParseEndList(hls);
            else if (!line.startsWith(QLatin1String("#")) && !line.isEmpty())
            {
                hls->AddSegment(segment_duration, title, line);
                segment_duration = -1; /* reset duration */
                title = "";
            }
        }
        while (err == RET_OK);
    }
    return err;
}

// stream content functions
/**
 * Preferetch the first x segments of the stream
 */
int HLSRingBuffer::Prefetch(int count)
{
    int retries = 0;
    int64_t starttime = mdate();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Starting Prefetch for %2 segments")
        .arg(count));
    m_streamworker->Lock();
    m_streamworker->Wakeup();
    while (!m_error && (retries < 20) &&
           (m_streamworker->CurrentPlaybackBuffer(false) < count) &&
           !m_streamworker->IsAtEnd())
    {
        m_streamworker->WaitForSignal();
        retries++;
    }
    m_streamworker->Unlock();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Finished Prefetch (%1s)")
        .arg((mdate() - starttime) / 1000000.0));
    // we waited more than 10s abort
    if (retries >= 10)
        return RET_ERROR;
    return RET_OK;
}

void HLSRingBuffer::SanityCheck(HLSSegment *segment)
{
    /* sanity check */
    if ((m_streamworker->CurrentPlaybackBuffer() == 0) &&
        (!m_streamworker->IsAtEnd(true) || m_live))
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "playback will stall");
    }
    else if ((m_streamworker->CurrentPlaybackBuffer() < PLAYBACK_MINBUFFER) &&
             (!m_streamworker->IsAtEnd(true) || m_live))
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "playback in danger of stalling");
    }
    else if (m_live && m_streamworker->IsAtEnd(true) &&
             (m_streamworker->CurrentPlaybackBuffer() < PLAYBACK_MINBUFFER))
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "playback will exit soon, starving for data");
    }
}

/**
 * Retrieve segment [segnum] from any available stream
 * Return NULL if segment couldn't be retrieved after 5s
 */
HLSSegment *HLSRingBuffer::GetSegment(int segnum, int timeout)
{
    HLSSegment *segment = NULL;
    int retries = 0;
    int stream = m_streamworker->StreamForSegment(segnum);
    if (stream < 0)
    {
        // we haven't downloaded that segment, request it
        // we should never be into this condition for normal playback
        m_streamworker->Seek(segnum);
        m_streamworker->Lock();
        /* Wait for download to be finished */
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            LOC + QString("waiting to get segment %1")
            .arg(segnum));
        while (!m_error && (stream < 0) && (retries < 10))
        {
            m_streamworker->WaitForSignal(timeout);
            stream = m_streamworker->StreamForSegment(segnum, false);
            retries++;
        }
        m_streamworker->Unlock();
        if (stream < 0)
            return NULL;
    }
    HLSStream *hls = GetStream(stream);
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("GetSegment[%1] stream[%2] (bitrate:%3)")
        .arg(segnum).arg(stream).arg(hls->Bandwidth()));
    hls->Lock();
    segment = hls->GetSegment(segnum);
    m_cache = hls->Cache();
    hls->Unlock();
    SanityCheck(segment);
    return segment;
}

int HLSRingBuffer::NumSegments(void) const
{
    HLSStream *hls = GetStream(0);
    if (hls == NULL)
        return 0;
    hls->Lock();
    int count = hls->NumSegments();
    hls->Unlock();
    return count;
}

int HLSRingBuffer::ChooseSegment(int stream)
{
    /* Choose a segment to start which is no closer than
     * 3 times the target duration from the end of the playlist.
     */
    int wanted          = 0;
    int segid           = 0;
    int wanted_duration = 0;
    int count           = NumSegments();
    int i = count - 1;

    HLSStream *hls = GetStream(stream);
    while(i >= 0)
    {
        HLSSegment *segment = hls->GetSegment(i);

        if (segment->Duration() > hls->TargetDuration())
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("EXTINF:%1 duration is larger than EXT-X-TARGETDURATION:%2")
                .arg(segment->Duration()).arg(hls->TargetDuration()));
        }

        wanted_duration += segment->Duration();
        if (wanted_duration >= 3 * hls->TargetDuration())
        {
            /* Start point found */
            wanted   = i;
            segid    = segment->Id();
            break;
        }
        i-- ;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Choose segment %1/%2 [%3]")
        .arg(wanted).arg(count).arg(segid));
    return wanted;
}

bool HLSRingBuffer::OpenFile(const QString &lfilename, uint retry_ms)
{
    QWriteLocker lock(&rwlock);

    safefilename = lfilename;
    filename = lfilename;

    QByteArray buffer;
    if (!downloadURL(filename, &buffer))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("Couldn't open URL %1").arg(filename));
        return false;   // can't download file
    }
    if (!IsHTTPLiveStreaming(&buffer))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("%1 isn't a HTTP Live Streaming URL").arg(filename));
        return false;
    }
    // let's go
    m_m3u8 = filename;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HTTP Live Streaming (%1)")
        .arg(m_m3u8));

    /* Parse HLS m3u8 content. */
    if (ParseM3U8(&buffer, &m_streams) != RET_OK || m_streams.isEmpty())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("An error occurred reading M3U8 playlist (%1)").arg(filename));
        m_error = true;
        return false;
    }

    /* HLS standard doesn't provide any guaranty about streams
     being sorted by bandwidth, so we sort them, higher bandwidth being first */
    qSort(m_streams.begin(), m_streams.end(), HLSStream::IsGreater);

    int startup = m_live ? ChooseSegment(0) : 0;
    if (m_live && startup < 0)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            "less data than 3 times 'target duration' available for "
            "live playback, playback may stall");
        startup = 0;
    }
    m_playback->SetSegment(startup);

    m_streamworker = new StreamWorker(this, startup, PLAYBACK_READAHEAD);
    m_streamworker->start();

    /* Initialize HLS live stream thread */
    //if (m_live)
    {
        HLSStream *hls  = GetStream(0);
        int64_t wakeup   =
            (int64_t)hls->TargetDuration() * PLAYBACK_MINBUFFER * 1000000ULL;
        m_playlistworker = new PlaylistWorker(this, wakeup);
        m_playlistworker->start();
    }

    if (Prefetch(PLAYBACK_MINBUFFER) != RET_OK)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "fetching first segment failed or didn't complete within 10s.");
        m_error = true;
        return false;
    }
    // set current stream position
    HLSStream *hls = CurrentStream();
    if (hls != NULL)
    {
        int64_t offset = CalculateOffset(startup);
        if (offset > 0)
        {
            m_playback->SetOffset(offset);
        }
    }
    return true;
}

bool HLSRingBuffer::SaveToDisk(QString filename)
{
    // download it all
    FILE *fp = fopen(filename.toAscii().constData(), "w");
    if (fp == NULL)
        return false;
    int count = NumSegments();
    for (int i = 0; i < count; i++)
    {
        HLSSegment *segment = GetSegment(i);
        if (segment == NULL)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                QString("downloading %1 failed").arg(i));
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("download of %1 succeeded")
                .arg(i));
            fwrite(segment->Data(), segment->Size(), 1, fp);
        }
    }
    fclose(fp);
    return true;
}

int64_t HLSRingBuffer::SizeMedia(void) const
{
    if (m_error)
        return -1;
    int64_t size = 0;
    int count = NumSegments();
    for (int i = 0; i < count; i++)
    {
        int stream = m_streamworker->StreamForSegment(i);
        if (stream < 0)
        {
            // segment not downloaded will estimate from stream bandwidth
            stream = m_streamworker->CurrentStream();
        }
        HLSStream  *hls        = GetStream(stream);
        HLSSegment *segment    = hls->GetSegment(i);
        hls->Lock();
        segment->Lock();
        if (segment->Size() > 0)
        {
            size += (int64_t)segment->Size();
        }
        else
        {
            size += segment->Duration() * hls->Bandwidth() / 8;
        }
        segment->Unlock();
        hls->Unlock();
    }

    return size;
}

int HLSRingBuffer::safe_read(void *data, uint sz)
{
    if (m_error)
        return -1;

    int used = 0;
    int i_read = sz;

    do
    {
        int segnum          = m_playback->Segment();
        int stream          = m_streamworker->StreamForSegment(segnum);
        HLSStream *hls     = GetStream(stream);
        if (hls == NULL)
            break;
        HLSSegment *segment= hls->GetSegment(segnum);
        if (segment == NULL)
            break;

        segment->Lock();
        if (segment->SizePlayed() == segment->Size())
        {
            if (!m_cache || m_live)
            {
                segment->Clear();
            }
            else
            {
                segment->Reset();
            }

            m_playback->IncrSegment();
            segment->Unlock();

            /* signal download thread we're about to use a new segment */
            m_streamworker->Wakeup();
            continue;
        }

        if (segment->SizePlayed() == 0)
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("started reading segment %1 from stream %2 (%3 buffered)")
                .arg(segment->Id()).arg(stream)
                .arg(m_streamworker->CurrentPlaybackBuffer()));

        int32_t len = segment->Read((uint8_t*)data + used, i_read);
        used    += len;
        i_read  -= len;
        segment->Unlock();
    }
    while (i_read > 0);

    if (m_live && (m_streamworker->CurrentPlaybackBuffer() < 1))
    {
        // danger of getting to the end... pause until we have some more
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("pausing until we get sufficient data buffered"));
        m_streamworker->Wakeup();
        m_streamworker->Lock();
        int retries = 0;
        while (!m_error &&
               (m_streamworker->CurrentPlaybackBuffer(false) < 1))
        {
            m_streamworker->WaitForSignal(1000);
            retries++;
        }
        m_streamworker->Unlock();
    }
    m_playback->AddOffset(used);
    return used;
}

long long HLSRingBuffer::GetRealFileSize(void) const
{
    QReadLocker lock(&rwlock);
    return SizeMedia();
}

int64_t HLSRingBuffer::CalculateOffset(int count, int64_t *max, int *segnum)
{
    int64_t length = 0LL;
    int64_t prev_length = 0LL;
    int n;
    for (n = 0; n < count; n++)
    {
        HLSStream *hls = GetStreamForSegment(n);
        if (hls == NULL)
        {
            return -1;
        }
        int64_t duration = hls->GetSegmentLength(n);
        if (duration < 0)
        {
            return -1;
        }
        length += duration;

        if (max && *max < length)
        {
            if (count - n >= 3 && segnum)
            {
                *segnum = n;
                // seek within segment
                //  segment->Read(NULL, stop - prev_length);
                break;
            }
            /* Do not search in last 3 segments */
            return -1;
        }
        prev_length = length;
    }
    // if we've reached the end of the stream
    if (max && n == count)
        *max = length;
    return length;
}

long long HLSRingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    if (m_error)
        return -1;

    if (!IsSeekingAllowed())
        return -1;

    QWriteLocker lock(&poslock);

    int64_t where;
    switch (whence)
    {
        case SEEK_CUR:
            // return current location, nothing to do
            if (pos == 0)
                return m_playback->Offset();
            where = m_playback->Offset() + pos;
            break;
        case SEEK_END:
            where = SizeMedia() - pos;
            break;
        case SEEK_SET:
        default:
            where = pos;
            break;
    }
    int64_t length = 0;

    int count  = NumSegments();
    int segnum = m_playback->Segment();

    /* restore current segment to start position */
    HLSSegment *segment = GetSegment(segnum);
    if (segnum == count || segment == NULL)
    {
        ateof = true;
        return SizeMedia();
    }
    segment->Lock();
    segment->Reset();
    segment->Unlock();

    length = CalculateOffset(count, &where, &segnum);
    if (length < 0)
        return -1;
    if (where == length)
    {
        // we have reached the end of the stream rewind to the last 2 segments
        segnum = count - PLAYBACK_MINBUFFER;
        for (int i = count - PLAYBACK_MINBUFFER; i < count; i++)
        {
            HLSStream *hls = GetStreamForSegment(i);
            if (hls == NULL)
            {
                return -1;
            }
            int64_t last = hls->GetSegmentLength(i);
            if (last < 0)
            {
                return -1;
            }
            where -= last;
        }
    }
    m_playback->SetSegment(segnum);
    m_streamworker->Seek(segnum);
    m_streamworker->Lock();

    /* Wait for download to be finished and to buffer 3 segment */
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("seek to segment %1").arg(segnum));
    int retries = 0;
    while (!m_error && (m_streamworker->IsSeeking() ||
           ((m_streamworker->CurrentPlaybackBuffer(false) < 3) &&
            !m_streamworker->IsAtEnd())))
    {
        m_streamworker->WaitForSignal(); //1000);
        retries++;
    }
    m_streamworker->Unlock();
    m_playback->SetOffset(where);

    return m_playback->Offset();
}

long long HLSRingBuffer::GetReadPosition(void) const
{
    if (m_error < 0)
        return 0;
    return m_playback->Offset();
}

bool HLSRingBuffer::IsSeekingAllowed(void)
{
    if (m_error)
        return false;

    if (m_live)
        return false;
    return true;
}

bool HLSRingBuffer::IsOpen(void) const
{
    return m_streams.size() > 0 && !m_error;
}
