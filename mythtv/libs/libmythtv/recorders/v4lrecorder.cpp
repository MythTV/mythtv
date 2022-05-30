// -*- Mode: c++ -*-

#ifdef USING_V4L2
#include <linux/videodev2.h>
#endif // USING_V4L2

#include <sys/ioctl.h>      // for ioctl
#include <sys/time.h>       // for gettimeofday
#include <unistd.h>         // for IO_NONBLOCK
#include <fcntl.h>          // for IO_NONBLOCK

#include "libmyth/mythcontext.h"
#include "libmythbase/mythlogging.h"

#include "captions/vbi608extractor.h"
#include "v4lrecorder.h"
#include "vbitext/vbi.h"
#include "tv_rec.h"

#define TVREC_CARDNUM \
        ((m_tvrec != nullptr) ? QString::number(m_tvrec->GetInputId()) : "NULL")

#define LOC QString("V4LRec[%1](%2): ") \
            .arg(TVREC_CARDNUM, m_videodevice)

V4LRecorder::~V4LRecorder()
{
    {
        QMutexLocker locker(&m_pauseLock);
        m_requestHelper = false;
        m_unpauseWait.wakeAll();
    }

    if (m_vbiThread)
    {
        m_vbiThread->wait();
        delete m_vbiThread;
        m_vbiThread = nullptr;
        CloseVBIDevice();
    }
}

void V4LRecorder::StopRecording(void)
{
    DTVRecorder::StopRecording();
    while (m_vbiThread && m_vbiThread->isRunning())
        m_vbiThread->wait();
}

bool V4LRecorder::IsHelperRequested(void) const
{
    QMutexLocker locker(&m_pauseLock);
    return m_requestHelper && m_requestRecording;
}

void V4LRecorder::SetOption(const QString &name, const QString &value)
{
    if (name == "audiodevice")
        m_audioDeviceName = value;
    else if (name == "vbidevice")
        m_vbiDeviceName = value;
    else if (name == "vbiformat")
        m_vbiMode = VBIMode::Parse(value);
    else
        DTVRecorder::SetOption(name, value);
}

static void vbi_event(void *data_in, struct vt_event *ev)
{
    auto *data = static_cast<struct VBIData *>(data_in);
    switch (ev->type)
    {
       case EV_PAGE:
       {
            auto *vtp = (struct vt_page *) ev->p1;
            if (vtp->flags & PG_SUBTITLE)
            {
#if 0
                LOG(VB_GENERAL, LOG_DEBUG, QString("subtitle page %1.%2")
                      .arg(vtp->pgno, 0, 16) .arg(vtp->subno, 0, 16));
#endif
                data->foundteletextpage = true;
                memcpy(&(data->teletextpage), vtp, sizeof(vt_page));
            }
       }
       break;

       case EV_HEADER:
       case EV_XPACKET:
           break;
    }
}

int V4LRecorder::OpenVBIDevice(void)
{
    int fd = -1;
    if (m_vbiFd >= 0)
        return m_vbiFd;

    struct VBIData *vbi_cb = nullptr;
    struct vbi     *pal_tt = nullptr;
    uint width = 0;
    uint start_line = 0;
    uint line_count = 0;

    QByteArray vbidev = m_vbiDeviceName.toLatin1();
    if (VBIMode::PAL_TT == m_vbiMode)
    {
        pal_tt = vbi_open(vbidev.constData(), nullptr, 99, -1);
        if (pal_tt)
        {
            fd = pal_tt->fd;
            vbi_cb = new VBIData;
            memset(vbi_cb, 0, sizeof(VBIData));
            vbi_cb->nvr = this;
            vbi_add_handler(pal_tt, vbi_event, vbi_cb);
        }
    }
    else if (VBIMode::NTSC_CC == m_vbiMode)
    {
        fd = open(vbidev.constData(), O_RDONLY/*|O_NONBLOCK*/);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid CC/Teletext mode");
        return -1;
    }

    if (fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Can't open vbi device: '%1'").arg(m_vbiDeviceName));
        return -1;
    }

    if (VBIMode::NTSC_CC == m_vbiMode)
    {
#ifdef USING_V4L2
        struct v4l2_format fmt {};
        fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
        if (0 != ioctl(fd, VIDIOC_G_FMT, &fmt))
        {
            LOG(VB_RECORD, LOG_ERR, "V4L2 VBI setup failed");
            close(fd);
            return -1;
        }
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("vbi_format  rate: %1"
                    "\n\t\t\t          offset: %2"
                    "\n\t\t\tsamples_per_line: %3"
                    "\n\t\t\t          starts: %4, %5"
                    "\n\t\t\t          counts: %6, %7"
                    "\n\t\t\t           flags: 0x%8")
                .arg(fmt.fmt.vbi.sampling_rate)
                .arg(fmt.fmt.vbi.offset)
                .arg(fmt.fmt.vbi.samples_per_line)
                .arg(fmt.fmt.vbi.start[0])
                .arg(fmt.fmt.vbi.start[1])
                .arg(fmt.fmt.vbi.count[0])
                .arg(fmt.fmt.vbi.count[1])
                .arg(fmt.fmt.vbi.flags,0,16));

        width      = fmt.fmt.vbi.samples_per_line;
        start_line = fmt.fmt.vbi.start[0];
        line_count = fmt.fmt.vbi.count[0];
        if (line_count != fmt.fmt.vbi.count[1])
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                    "VBI must have the same number of "
                    "odd and even fields for our decoder");
            close(fd);
            return -1;
        }
        if (start_line > 21 || start_line + line_count < 22)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "VBI does not include line 21");
            // TODO We could try to set the VBI format ourselves..
            close(fd);
            return -1;
        }
#endif // USING_V4L2
    }

    if (VBIMode::PAL_TT == m_vbiMode)
    {
        m_palVbiCb = vbi_cb;
        m_palVbiTt = pal_tt;
    }
    else if (VBIMode::NTSC_CC == m_vbiMode)
    {
        m_ntscVbiWidth     = width;
        m_ntscVbiStartLine = start_line;
        m_ntscVbiLineCount = line_count;
        m_vbi608 = new VBI608Extractor();
    }

    m_vbiFd = fd;

    return fd;
}

void V4LRecorder::CloseVBIDevice(void)
{
    if (m_vbiFd < 0)
        return;

    if (m_palVbiTt)
    {
        vbi_del_handler(m_palVbiTt, vbi_event, m_palVbiCb);
        vbi_close(m_palVbiTt);
        delete m_palVbiCb;
        m_palVbiCb = nullptr;
    }
    else
    {
        delete m_vbi608; m_vbi608 = nullptr;
        close(m_vbiFd);
    }

    m_vbiFd = -1;
}

void V4LRecorder::RunVBIDevice(void)
{
    if (m_vbiFd < 0)
        return;

    unsigned char *buf = nullptr;
    unsigned char *ptr = nullptr;
    unsigned char *ptr_end = nullptr;
    if (m_ntscVbiWidth)
    {
        uint sz   = m_ntscVbiWidth * m_ntscVbiLineCount * 2;
        buf = ptr = new unsigned char[sz];
        ptr_end   = buf + sz;
    }

    while (IsHelperRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsHelperRequested() || IsErrored())
            break;

        struct timeval tv {0, 5000};
        fd_set rdset;

        FD_ZERO(&rdset); // NOLINT(readability-isolate-declaration)
        FD_SET(m_vbiFd, &rdset);

        int nr = select(m_vbiFd + 1, &rdset, nullptr, nullptr, &tv);
        if (nr < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "vbi select failed" + ENO);

        if (nr <= 0)
        {
            if (nr==0)
                LOG(VB_GENERAL, LOG_DEBUG, LOC + "vbi select timed out");
            continue; // either failed or timed out..
        }
        if (VBIMode::PAL_TT == m_vbiMode)
        {
            m_palVbiCb->foundteletextpage = false;
            vbi_handler(m_palVbiTt, m_palVbiTt->fd);
            if (m_palVbiCb->foundteletextpage)
            {
                // decode VBI as teletext subtitles
                FormatTT(m_palVbiCb);
            }
        }
        else if (VBIMode::NTSC_CC == m_vbiMode)
        {
            int ret = read(m_vbiFd, ptr, ptr_end - ptr);
            ptr = (ret > 0) ? ptr + ret : ptr;
            if ((ptr_end - ptr) == 0)
            {
                unsigned char *line21_field1 =
                    buf + ((21 - m_ntscVbiStartLine) * static_cast<size_t>(m_ntscVbiWidth));
                unsigned char *line21_field2 =
                    buf + ((m_ntscVbiLineCount + 21 - m_ntscVbiStartLine)
                           * static_cast<size_t>(m_ntscVbiWidth));
                bool cc1 = m_vbi608->ExtractCC12(line21_field1, m_ntscVbiWidth);
                bool cc2 = m_vbi608->ExtractCC34(line21_field2, m_ntscVbiWidth);
                if (cc1 || cc2)
                {
                    int code1 = m_vbi608->GetCode1();
                    int code2 = m_vbi608->GetCode2();
                    code1 = (0xFFFF==code1) ? -1 : code1;
                    code2 = (0xFFFF==code2) ? -1 : code2;
                    FormatCC(code1, code2);
                }
                ptr = buf;
            }
            else if (ret < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Reading VBI data" + ENO);
            }
        }
    }

    delete [] buf;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
