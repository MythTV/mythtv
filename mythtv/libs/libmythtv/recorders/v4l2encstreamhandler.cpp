// -*- Mode: c++ -*-

#include <iostream>

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef USING_MINGW
#include <poll.h>
//#include <sys/select.h>
#include <sys/ioctl.h>
#endif

// Qt headers
#include <QString>
#include <QFile>

// MythTV headers
#include "v4l2encstreamhandler.h"
#include "v4lchannel.h"
#include "dtvsignalmonitor.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cardutil.h"
#include "exitcodes.h"

const char* V4L2encStreamHandler::m_stream_types[] =
{
    "MPEG-2 PS", "MPEG-2 TS",     "MPEG-1 VCD",    "PES AV",
    "",          "PES V",          "",             "PES A",
    "",          "",              "DVD",           "VCD",
    "SVCD",      "DVD-Special 1", "DVD-Special 2", 0
};

const int V4L2encStreamHandler::m_audio_rateL1[] =
{
    32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0
};

const int V4L2encStreamHandler::m_audio_rateL2[] =
{
    32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0
};

const int V4L2encStreamHandler::m_audio_rateL3[] =
{
    32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};

#define LOC      QString("V4L2SH(%1): ").arg(_device)

QMap<QString,V4L2encStreamHandler*> V4L2encStreamHandler::m_handlers;
QMap<QString,uint>              V4L2encStreamHandler::m_handlers_refcnt;
QMutex                          V4L2encStreamHandler::m_handlers_lock;

V4L2encStreamHandler *V4L2encStreamHandler::Get(const QString &devname,
                                                int audioinput)
{
    QMutexLocker locker(&m_handlers_lock);

    QString devkey = devname;

    QMap<QString,V4L2encStreamHandler*>::iterator it = m_handlers.find(devkey);

    if (it == m_handlers.end())
    {
        V4L2encStreamHandler *newhandler = new V4L2encStreamHandler(devname,
                                                                    audioinput);

        m_handlers[devkey] = newhandler;
        m_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("V4L2SH: Creating new stream handler for %1")
            .arg(devname));
    }
    else
    {
        m_handlers_refcnt[devkey]++;
        uint rcount = m_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("V4L2SH: Using existing stream handler for %1")
            .arg(devkey) + QString(" (%1 in use)").arg(rcount));
    }

    return m_handlers[devkey];
}

void V4L2encStreamHandler::Return(V4L2encStreamHandler * & ref)
{
    QMutexLocker locker(&m_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = m_handlers_refcnt.find(devname);
    if (rit == m_handlers_refcnt.end())
        return;

    LOG(VB_RECORD, LOG_INFO, QString("V4L2SH: Return '%1' in use %2")
        .arg(devname).arg(*rit));

    if (*rit > 1)
    {
        ref = nullptr;
        --(*rit);
        return;
    }

    QMap<QString, V4L2encStreamHandler*>::iterator it =
        m_handlers.find(devname);
    if ((it != m_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("V4L2SH: Closing handler for %1")
            .arg(devname));
        delete *it;
        m_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("V4L2SH: Error: Couldn't find handler for %1")
            .arg(devname));
    }

    m_handlers_refcnt.erase(rit);
    ref = nullptr;
}

/*
  V4L2encStreamHandler
*/
bool V4L2encStreamHandler::Status(bool &failed, bool &failing)
{
    failed = !IsRunning();
    failing = m_failing;
    return !failed && !failing;
}

V4L2encStreamHandler::V4L2encStreamHandler(const QString & device,
                                           int audio_input) :
    StreamHandler(device),          m_failing(false),
    m_hasTuner(false),              m_hasPictureAttributes(false),
    m_bufferSize(1000 * TSPacket::kSize), m_desired_stream_type(-1),
    m_stream_type(-1),              m_aspect_ratio(-1),
    m_bitrate_mode(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR),
    m_bitrate(-1),                  m_max_bitrate(-1),
    m_audio_codec(-1),              m_audio_samplerate(-1),
    m_audio_bitrateL1(-1),          m_audio_bitrateL2(-1),
    m_audio_bitrateL3(-1),          m_audio_volume(-1),
    m_lang_mode(-1),
    m_low_bitrate_mode(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR),
    m_low_bitrate(-1),              m_low_peak_bitrate(-1),
    m_medium_bitrate_mode(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR),
    m_medium_bitrate(-1),           m_medium_peak_bitrate(-1),
    m_high_bitrate_mode(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR),
    m_high_bitrate(-1),             m_high_peak_bitrate(-1),
    m_fd(-1),                       m_audio_input(audio_input),
    m_width(-1),                    m_height(-1),
    m_has_lock(false),              m_signal_strength(-1),
    m_drb(nullptr),
    m_streaming_cnt(0),             m_pause_encoding_allowed(true)
{
    setObjectName("V4L2encSH");

    if (!Open())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("-- Failed to open %1: ")
            .arg(_device) + ENO);
        _error = true;
        return;
    }
    else
        LOG(VB_RECORD, LOG_INFO, LOC + QString("'%1' open").arg(_device));
}

V4L2encStreamHandler::~V4L2encStreamHandler(void)
{
    StopEncoding();
    Close();
}

void V4L2encStreamHandler::run(void)
{
    RunProlog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- begin");

    if (!IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Starting stream handler, but v4l2 is not open!");
        if (!Open())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("run() -- Failed to open %1: ")
                .arg(_device) + ENO);
            _error = true;
            return;
        }
    }

#if 0
    // VBI
    if (m_vbi_fd >= 0)
        m_vbi_thread = new VBIThread(this);
#endif

    bool      good_data = false;
    bool      gap = false;
    QDateTime gap_start;

    int len, remainder = 0;

    QByteArray buffer;
    char* pkt_buf = new char[PACKET_SIZE + 1];

    SetRunning(true, true, false);

    while (_running_desired && !_error)
    {
        // Get V4L2 data
        if (m_streaming_cnt.load() == 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "Waiting for stream start.");
            QMutexLocker locker(&_start_stop_lock);
            _running_state_changed.wait(&_start_stop_lock, 5000);
            continue;
        }

        // Check for errors

        if (!m_drb)
            break;

        len = m_drb->Read(reinterpret_cast<unsigned char *>(pkt_buf),
                          PACKET_SIZE);
        if (m_drb->IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "run() -- Device error detected");

            if (good_data)
            {
                if (gap)
                {
                    /* Already processing a gap, which means
                     * restarting the encoding didn't work! */
                    m_failing = true;
                }
                else
                    gap = true;
            }

            RestartEncoding();
        }
        else if (m_drb->IsEOF())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "run() -- Device EOF detected");
            _error = true;
        }
        else
        {
#if 0 // For this to work, the data needs to be propagated back up to
      // the 'recorder', but there could be multiple rcorders...

            // If we have seen good data, but now have a gap, note it
            if (good_data)
            {
                if (gap)
                {
                    QMutexLocker locker(&statisticsLock);
                    QDateTime gap_end(MythDate::current());

                    for (Irec = m_rec_gaps.begin();
                         Irec != m_rec_caps.end(); ++Irec)
                    {
                        (*Irec).push_back(RecordingGap
                                          (gap_start, gap_end));
                    }
                    LOG(VB_RECORD, LOG_DEBUG,
                        LOC + QString("Inserted gap %1 dur %2")
                        .arg(recordingGaps.back().toString())
                        .arg(gap_start.secsTo(gap_end)));
                    gap = false;
                }
                else
                    gap_start = MythDate::current();
            }
            else
                good_data = true;
#else
            good_data = true;
#endif
        }

        if (len < 0)
        {
            if (errno != EAGAIN)
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("run() -- error reading from: %1")
                    .arg(_device) + ENO);
            continue;
        }

        buffer.append(pkt_buf, len);
        len = buffer.size();

        if (len < static_cast<int>(TSPacket::kSize))
            continue;

        if (!_listener_lock.tryLock())
            continue;

        if (_stream_data_list.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("run() -- _stream_data_list is empty, %1 buffered")
                .arg(buffer.size()));
            _listener_lock.unlock();
            continue;
        }

        StreamDataList::const_iterator sit = _stream_data_list.begin();
        for (; sit != _stream_data_list.end(); ++sit)
            remainder = sit.key()->ProcessData
                        (reinterpret_cast<const uint8_t *>
                         (buffer.constData()), len);

        _listener_lock.unlock();

        if (remainder > 0 && (len > remainder)) // leftover bytes
            buffer.remove(0, len - remainder);
        else
            buffer.clear();
    }

    QString tmp(_error);
    LOG(VB_GENERAL, LOG_WARNING, LOC +
        QString("_running_desired(%1)  _error(%2)")
                .arg(_running_desired).arg(tmp));

    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- finishing up");
    StopEncoding();

    delete[] pkt_buf;

    SetRunning(false, true, false);
    RunEpilog();

    LOG(VB_RECORD, LOG_INFO, LOC + "run() -- end");
}


bool V4L2encStreamHandler::Open(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "open() -- begin");

    if (IsOpen())
    {
        LOG(VB_RECORD, LOG_WARNING, LOC + "run() -- Already open.");
        return true;
    }
    Close();

    QMutexLocker lock(&m_stream_lock);
    m_v4l2.Open(_device, m_vbi_device);
    if (!m_v4l2.IsOpen())
    {
        m_error = QString("Open of '%1' failed: ").arg(_device) + ENO;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open() -- " + m_error);
        return false;
    }

    if (!m_v4l2.IsEncoder())
    {
        m_error = "V4L version 2 required";
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open() -- " + m_error);
        m_v4l2.Close();
        return false;
    }

    if (m_drb)
    {
        if (m_drb->IsRunning())
            m_drb->Stop();
        delete m_drb;
        m_drb = nullptr;
    }


    m_fd = open(_device.toLatin1().constData(), O_RDWR | O_NONBLOCK);

    m_drb = new DeviceReadBuffer(this);
    if (!m_drb)
    {
        m_error = "Failed to allocate DRB buffer";
        LOG(VB_GENERAL, LOG_ERR, LOC + "Configure() -- " + m_error);
        Close();
        return false;
    }

    m_drb->SetRequestPause(true);
    if (!m_drb->Setup(_device.toLatin1().constData(), m_fd))
    {
        m_error = "Failed to setup DRB buffer";
        LOG(VB_GENERAL, LOG_ERR, LOC + "Configure() -- " + m_error);
        Close();
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "open() -- done");
    return true;
}

bool V4L2encStreamHandler::Configure(void)
{
    if (m_streaming_cnt.load() > 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Configure() -- Already configured.");
        return true;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "Configure() -- begin");

    if (m_width > 0 && m_height > 0 &&
        !m_v4l2.SetResolution(m_width, m_height))
    {
        m_v4l2.Close();
        LOG(VB_RECORD, LOG_ERR, LOC + "Configure() -- failed");
        return false;
    }

    if (m_v4l2.HasTuner())
        SetLanguageMode();    // we don't care if this fails...

    if (m_v4l2.HasAudioSupport())
        m_v4l2.SetVolume(m_audio_volume);  // we don't care if this fails...

    if (m_desired_stream_type >= 0)
        m_v4l2.SetStreamType(m_desired_stream_type);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Options for %1")
        .arg(m_v4l2.StreamTypeDesc(GetStreamType())));

    if (m_aspect_ratio >= 0)
        m_v4l2.SetVideoAspect(m_aspect_ratio);

    if (m_bitrate_mode < 0 && m_high_bitrate_mode >= 0)
        m_bitrate_mode = m_high_bitrate_mode;
    if (m_max_bitrate < 0 && m_high_peak_bitrate > 0)
        m_max_bitrate = m_high_peak_bitrate;
    if (m_bitrate < 0 && m_high_bitrate > 0)
        m_bitrate = m_high_bitrate;

    m_max_bitrate = std::max(m_max_bitrate, m_bitrate);

    if (m_bitrate_mode >= 0)
        m_v4l2.SetVideoBitrateMode(m_bitrate_mode);
    if (m_bitrate > 0)
        m_v4l2.SetVideoBitrate(m_bitrate * 1000);
    if (m_max_bitrate > 0)
        m_v4l2.SetVideoBitratePeak(m_max_bitrate * 1000);

    if (m_audio_input >= 0)
        m_v4l2.SetAudioInput(m_audio_input);
    else
        LOG(VB_CHANNEL, LOG_WARNING, "Audio input not set.");

    if (m_audio_codec >= 0)
        m_v4l2.SetAudioCodec(m_audio_codec);
    if (m_audio_samplerate >= 0)
        m_v4l2.SetAudioSamplingRate(m_audio_samplerate);
    if (m_audio_bitrateL2 >= 0)
        m_v4l2.SetAudioBitrateL2(m_audio_bitrateL2);

    ConfigureVBI();

    LOG(VB_RECORD, LOG_INFO, LOC + "Configure() -- done");
    return false;
}

void V4L2encStreamHandler::Close(void)
{
#if 0
// VBI
    if (m_vbi_thread != NULL)
    {
        m_vbi_thread->wait();
        delete m_vbi_thread;
        m_vbi_thread = NULL;

        CloseVBIDevice();
    }
#endif

    if (m_drb)
    {
        m_drb->Stop();
        delete m_drb;
        m_drb = nullptr;
    }

    m_v4l2.Close();

    if (m_fd >= 0)
    {
        close(m_fd);
        m_fd = -1;
        LOG(VB_RECORD, LOG_INFO, LOC + "Closed.");
    }

    m_streaming_cnt.fetchAndStoreAcquire(0);
}

bool V4L2encStreamHandler::StartEncoding(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "StartEncoding() -- begin");
    int old_cnt;

    QMutexLocker lock(&m_stream_lock);

    if (!IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "V4L2 recorder not initialized.");
        return false;
    }

    if ((old_cnt = m_streaming_cnt.load()) == 0)
    {
        // Start encoding

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Streaming mode %1.  Not using it.")
            .arg(m_v4l2.HasStreaming() ? "available" : "not available"));

        if (!m_v4l2.UserAdjustableResolution())
        {
            for (int idx = 0; idx < 10; ++idx)
            {
                if (SetBitrateForResolution())
                    break;
            }
        }

        if (m_pause_encoding_allowed)
            m_pause_encoding_allowed = m_v4l2.ResumeEncoding();
        if (!m_pause_encoding_allowed)
            m_v4l2.StartEncoding();

        // (at least) with the 3.10 kernel, the V4L2_ENC_CMD_START does
        // not reliably start the data flow from a HD-PVR.  A read() seems
        // to work, though.

        int idx = 1;
        for ( ; idx < 50; ++idx)
        {
            uint8_t dummy;
            int len = read(m_fd, &dummy, 0);
            if (len >= 0)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("StartEncoding read %1 bytes").arg(len));
                break;
            }
            if (idx == 20)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "StartEncoding: read failing, re-opening device: " + ENO);
                close(m_fd);
                usleep(2000);
                m_fd = open(_device.toLatin1().constData(), O_RDWR | O_NONBLOCK);
                if (m_fd < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "StartEncoding: Can't open video device." + ENO);
                    m_error = "Failed to start recording";
                    return false;
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("StartEncoding: read failed, retry in %1 msec:")
                    .arg(100 * idx) + ENO);
                usleep(100 * idx);
            }
        }
        if (idx == 50)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "StartEncoding: read from video device failed." + ENO);
            m_error = "Failed to start recording";
            close(m_fd);
            m_fd = -1;
            return false;
        }
        if (idx > 0)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("%1 read attempts required to start encoding").arg(idx));
        }

        if (m_drb)
        {
            m_drb->SetRequestPause(false);
            m_drb->Start();
        }
    }
    else
        LOG(VB_RECORD, LOG_INFO, LOC + "Already encoding");

    QMutexLocker listen_lock(&_listener_lock);

    m_streaming_cnt.ref();

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StartEncoding() -- %1->%2 listeners")
        .arg(old_cnt).arg(m_streaming_cnt.load()));

    LOG(VB_RECORD, LOG_INFO, LOC + "StartEncoding() -- end");

    return true;
}

bool V4L2encStreamHandler::StopEncoding(void)
{
    int old_cnt = m_streaming_cnt.load();

    if (old_cnt == 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "StopEncoding: already stopped.");
        return true;
    }

    QMutexLocker lock(&m_stream_lock);

    if (m_streaming_cnt.deref())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("StopEncoding() -- delayed, still have %1 listeners")
            .arg(m_streaming_cnt.load()));
        return true;
    }

    if (!IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "StopEncoding() -- V4L2enc recorder not started.");
        return false;
    }

    // Stop encoding
    if (m_drb)
        m_drb->SetRequestPause(true);

    if (m_pause_encoding_allowed)
        m_pause_encoding_allowed = m_v4l2.PauseEncoding();
    if (!m_pause_encoding_allowed)
        m_v4l2.StopEncoding();

    // allow last bits of data through..
    if (m_drb && m_drb->IsRunning())
        usleep(20 * 1000);
#if 0
    // close the fd so streamoff/streamon work in V4LChannel    Close();
    close(m_fd);
#endif

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StopEncoding() -- %1->%2 listeners")
        .arg(old_cnt).arg(m_streaming_cnt.load()));

    return true;
}

void V4L2encStreamHandler::RestartEncoding(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "RestartEncoding()");

    StopEncoding();
    StartEncoding();
}

#if 0
void V4L2encStreamHandler::PriorityEvent(int fd)
{
    // TODO report on buffer overruns, etc.
}
#endif

/// Set audio language mode
bool V4L2encStreamHandler::SetLanguageMode(void)
{
    if (m_lang_mode < 0)
        return true;

    if (V4L2_TUNER_MODE_LANG1_LANG2      == m_lang_mode &&
        V4L2_MPEG_AUDIO_ENCODING_LAYER_1 == m_audio_layer)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "SetLanguageMode() -- Dual audio mode incompatible "
            "with Layer I audio. Falling back to Main Language");
        m_lang_mode = V4L2_TUNER_MODE_LANG1;
    }

    return m_v4l2.SetLanguageMode(m_lang_mode);
}

static int find_index(const int *audio_rate, int value)
{
    for (uint i = 0; audio_rate[i] != 0; ++i)
    {
        if (audio_rate[i] == value)
            return i;
    }

    return -1;
}

bool V4L2encStreamHandler::SetOption(const QString &opt, int value)
{
    if (m_streaming_cnt.load() > 0)
        return true;

    if (opt == "width")
        m_width = value;
    else if (opt == "height")
        m_height = value;
    else if (opt == "mpeg2bitratemode")
        m_bitrate_mode = value ? V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                         V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
    else if (opt == "mpeg2bitrate")
        m_bitrate = value;
    else if (opt == "mpeg2maxbitrate")
        m_max_bitrate = value;
    else if (opt == "samplerate")
    {
        switch (value)
        {
            case 32000:
              m_audio_samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000;
              break;
            case 44100:
              m_audio_samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100;
              break;
            case 48000:
            default:
              m_audio_samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
        }
    }
    else if (opt == "mpeg2audbitratel1")
    {
        int index = find_index(m_audio_rateL1, value);
        if (index >= 0)
            m_audio_bitrateL1 = index;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Audiorate(L1): " +
                QString("%1 is invalid").arg(value));
            return true;
        }
    }
    else if (opt == "mpeg2audbitratel2")
    {
        int index = find_index(m_audio_rateL2, value);
        if (index >= 0)
            m_audio_bitrateL2 = index;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Audiorate(L2): " +
                    QString("%1 is invalid").arg(value));
            return true;
        }
    }
    else if (opt == "mpeg2audbitratel3")
    {
        int index = find_index(m_audio_rateL3, value);
        if (index >= 0)
            m_audio_bitrateL3 = index;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Audiorate(L2): " +
                    QString("%1 is invalid").arg(value));
            return true;
        }
    }
    else if (opt == "mpeg2audvolume")
        m_audio_volume = value;
    else if (opt == "low_mpegbitratemode")
        m_low_bitrate_mode = value;
    else if (opt == "medium_mpegbitratemode")
        m_medium_bitrate_mode = value;
    else if (opt == "high_mpegbitratemode")
        m_high_bitrate_mode = value;
    else if (opt.endsWith("avgbitrate"))
    {
        if (opt.startsWith("low"))
            m_low_bitrate = value;
        else if (opt.startsWith("medium"))
            m_medium_bitrate = value;
        else if (opt.startsWith("high"))
            m_high_bitrate = value;
        else
            return false;
    }
    else if (opt.endsWith("peakbitrate"))
    {
        if (opt.startsWith("low"))
            m_low_peak_bitrate = value;
        else if (opt.startsWith("medium"))
            m_medium_peak_bitrate = value;
        else if (opt.startsWith("high"))
            m_high_peak_bitrate = value;
        else
            return false;
    }
    else
        return false;

    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetOption('%1', %2) -- success")
        .arg(opt).arg(value));
    return true;
}

int V4L2encStreamHandler::GetStreamType(void)
{
    if (m_stream_type == -1)
        m_stream_type = m_v4l2.GetStreamType();
    return m_stream_type;
}

bool V4L2encStreamHandler::SetOption(const QString &opt, const QString &value)
{
    if (m_streaming_cnt.load() > 0)
        return true;

    if (opt == "vbidevice")
        m_vbi_device = value;
    else if (opt == "mpeg2streamtype")
    {
        for (uint i = 0; i < sizeof(m_stream_types) / sizeof(char*); ++i)
        {
            if (QString(m_stream_types[i]) == value)
            {
                m_desired_stream_type = i;
                return true;
            }
        }
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("MPEG2 stream type %1 is invalid ").arg(value));
        return false;
    }
    else if (opt == "mpeg2language")
    {
        bool ok = false;
        int  lang_mode = value.toInt(&ok); // on failure language will be 0
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "MPEG2 language (stereo) flag " +
                    QString("'%1' is invalid").arg(value));
            return true;
        }
        switch (lang_mode)
        {
            case 1:
              m_lang_mode = V4L2_TUNER_MODE_LANG2;
              break;
            case 2:
              m_lang_mode = V4L2_TUNER_MODE_LANG1_LANG2;
              break;
            case 0:
            default:
              m_lang_mode = V4L2_TUNER_MODE_LANG1;
              break;
        }
    }
    else if (opt == "mpeg2aspectratio")
    {
        if (value == "Square")
            m_aspect_ratio = V4L2_MPEG_VIDEO_ASPECT_1x1;
        else if (value == "4:3")
            m_aspect_ratio = V4L2_MPEG_VIDEO_ASPECT_4x3;
        else if (value == "16:9")
            m_aspect_ratio = V4L2_MPEG_VIDEO_ASPECT_16x9;
        else if (value == "2.21:1")
            m_aspect_ratio = V4L2_MPEG_VIDEO_ASPECT_221x100;
        else
            m_aspect_ratio = V4L2_MPEG_VIDEO_ASPECT_16x9;
    }
    else if (opt == "mpeg2audtype")
    {
        if (value == "Layer I")
            m_audio_layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_1; // plus one?
        else if (value == "Layer II")
            m_audio_layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
        else if (value == "Layer III")
            m_audio_layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_3;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "MPEG2 audio layer: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "audiocodec")
    {
        if (value.startsWith("V4L2:"))
        {
            // Find the value in the options returns by the driver
            m_audio_codec = m_v4l2.GetOptionValue
                            (DriverOption::AUDIO_ENCODING, value.mid(5));
        }
    }
    else
        return false;

    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetOption('%1', '%2') -- success")
        .arg(opt).arg(value));
    return true;
}

bool V4L2encStreamHandler::HasLock(void)
{
    if (m_streaming_cnt.load() > 0)
        return true;

    return true;
}

int V4L2encStreamHandler::GetSignalStrength(void)
{
    if (m_streaming_cnt.load() > 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("GetSignalStrength() -- "
                                               "returning cached value (%1)")
            .arg(m_signal_strength));
        return m_signal_strength;
    }

    m_signal_strength = m_v4l2.GetSignalStrength();
    return m_signal_strength;
}

void V4L2encStreamHandler::SetBitrate(int bitrate, int maxbitrate,
                                      int bitratemode, const QString & reason)
{
    if (maxbitrate == bitrate)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("SetBitrate() -- %1 bitrate %2 kbps CBR")
            .arg(reason).arg(bitrate));
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("SetBitrate() -- %1 bitrate %2/%3 kbps VBR")
            .arg(reason).arg(bitrate).arg(maxbitrate));
    }

    // Set VBR or CBR mode
    if (bitratemode >= 0)
        m_v4l2.SetVideoBitrateMode(bitratemode);

    // Set average bitrate
    if (bitrate > 0)
        m_v4l2.SetVideoBitrate(bitrate * 1000);

    // Set max bitrate
    if (maxbitrate > 0)
        m_v4l2.SetVideoBitratePeak(maxbitrate * 1000);
}

bool V4L2encStreamHandler::SetBitrateForResolution(void)
{
    m_width = m_height = -1;

    int width, height, pix;
    int idx = 0;
    for ( ; idx < 10; ++idx)
    {
        if (m_v4l2.GetResolution(width, height))
            break;
        if (idx == 5)
        {
            m_v4l2.Close();
            m_v4l2.Open(_device, m_vbi_device);
        }
        usleep(100);
    }
    if (idx == 5)
        return false;

    m_width  = width;
    m_height = height;
    pix      = width * height;

    int old_mode = m_bitrate_mode;
    int old_max  = m_max_bitrate;
    int old_avg  = m_bitrate;

    if (m_low_bitrate > 0 && pix <= 768*1080)
    {
        m_bitrate_mode = m_low_bitrate_mode;
        m_max_bitrate = m_low_peak_bitrate;
        m_bitrate    = m_low_bitrate;
    }
    else if (m_high_bitrate > 0 && pix >= 1920*1080)
    {
        m_bitrate_mode = m_high_bitrate_mode;
        m_max_bitrate = m_high_peak_bitrate;
        m_bitrate    = m_high_bitrate;
    }
    else if (m_medium_bitrate > 0)
    {
        m_bitrate_mode = m_medium_bitrate_mode;
        m_max_bitrate = m_medium_peak_bitrate;
        m_bitrate    = m_medium_bitrate;
    }
    m_max_bitrate = std::max(m_max_bitrate, m_bitrate);

    if ((old_max != m_max_bitrate) || (old_avg != m_bitrate) ||
        old_mode != m_bitrate_mode)
    {
        if (old_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Old bitrate %1 CBR").arg(old_avg));
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO,LOC +
                QString("Old bitrate %1/%2 VBR").arg(old_avg).arg(old_max));
        }

        SetBitrate(m_bitrate, m_max_bitrate, m_bitrate_mode, "New");
    }

    return true;
}

bool V4L2encStreamHandler::ConfigureVBI(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "ConfigureVBI() -- begin");


    LOG(VB_RECORD, LOG_INFO, LOC + "ConfigureVBI() -- end");

    return false;
}
