// MythTV
#include "libmythbase/mythlogging.h"
#include "mythframe.h"
#include "mythavutil.h"
#include "mythplayerui.h"
#include "mythvideoout.h"
#include "mythvideoscantracker.h"

#define LOC QString("ScanTracker: ")

MythVideoScanTracker::MythVideoScanTracker(MythPlayerUI* Parent)
  : m_parentPlayer(Parent)
{
}

void MythVideoScanTracker::InitialiseScan(MythVideoOutput* VideoOutput)
{
    // Default to interlaced playback but set the tracker to progressive
    // Enable autodetection of interlaced/progressive from video stream
    // Previously we set to interlaced and the scan tracker to 2 but this
    // mis-'detected' a number of streams as interlaced when they are progressive.
    // This significantly reduces the number of errors and also ensures we do not
    // needlessly setup deinterlacers - which may consume significant resources.
    // We set to interlaced for those streams whose frame rate is initially detected
    // as e.g. 59.9 when it is actually 29.97 interlaced.
    m_scan        = kScan_Interlaced;
    m_scanLocked  = false;
    m_scanTracker = -2;
    if (VideoOutput)
        VideoOutput->SetDeinterlacing(true, m_parentPlayer->CanSupportDoubleRate());
}

void MythVideoScanTracker::UnlockScan()
{
    m_scanInitialized = false;
    m_scanLocked = false;
}

void MythVideoScanTracker::ResetTracker()
{
    m_scanLocked  = false;
    m_scanTracker = (m_scan == kScan_Interlaced) ? 2 : 0;
}

FrameScanType MythVideoScanTracker::NextScanOverride()
{
    int next = m_scanOverride + 1;
    if (next > kScan_Progressive)
        next = kScan_Detect;
    return static_cast<FrameScanType>(next);
}

void MythVideoScanTracker::SetScanOverride(FrameScanType Scan)
{
    if (m_scanOverride == Scan)
        return;

    m_scanOverride = Scan;
    if (m_scanOverride == kScan_Detect)
    {
        m_scanLocked = false;
        m_scanInitialized = false;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Reverting to auto detection of scan");
    }
}

FrameScanType MythVideoScanTracker::GetScanForDisplay(MythVideoFrame* Frame, bool& SecondField)
{
    if (!Frame)
        return kScan_Progressive;

    // Update details for debug OSD
    m_lastDeinterlacer   = Frame->m_deinterlaceInuse;
    m_lastDeinterlacer2x = Frame->m_deinterlaceInuse2x;
    // We use the underlying pix_fmt as it retains the distinction between hardware
    // and software frames for decode only decoders.
    m_lastFrameCodec = MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_pixFmt));

    // Decide on type and rate
    FrameScanType result = m_scan;
    if ((kScan_Detect == m_scan) || (kScan_Ignore == m_scan) || Frame->m_alreadyDeinterlaced)
    {
        result = kScan_Progressive;
    }
    else if (is_interlaced(m_scan))
    {
        result = kScan_Interlaced;
        Frame->m_interlacedReverse = (m_scan == kScan_Intr2ndField);
    }

    // Only display the second field if needed.
    // This is somewhat circular - we check for double rate support when enabling
    // deinterlacing, request that in the video buffers and the differing implementations
    // then set m_lastDeinterlacer2x when they are actually using double rate
    SecondField = is_interlaced(result) && m_lastDeinterlacer2x;
    return result;
}

FrameScanType MythVideoScanTracker::GetScanType() const
{
    return m_scan;
}

FrameScanType MythVideoScanTracker::GetScanTypeWithOverride() const
{
    if (m_scanOverride > kScan_Detect)
        return m_scanOverride;
    return m_scan;
}

void MythVideoScanTracker::CheckScanUpdate(MythVideoOutput* VideoOutput, std::chrono::microseconds FrameInterval)
{
    if (m_resetScan != kScan_Ignore)
        SetScanType(m_resetScan, VideoOutput, FrameInterval);
}

QString MythVideoScanTracker::GetDeinterlacerName()
{
    return MythVideoFrame::DeinterlacerName(m_lastDeinterlacer, m_lastDeinterlacer2x, m_lastFrameCodec);
}

void MythVideoScanTracker::SetScanType(FrameScanType Scan, MythVideoOutput* VideoOutput,
                                       std::chrono::microseconds FrameInterval)
{
    if (!is_current_thread(m_mainThread))
    {
        m_resetScan = Scan;
        return;
    }

    if (!VideoOutput)
        return;

    m_resetScan = kScan_Ignore;

    if (m_scanInitialized && (m_scan == Scan) && (m_lastFrameInterval == FrameInterval))
        return;

    m_scanLocked = (Scan != kScan_Detect);
    m_scanInitialized = true;
    m_lastFrameInterval = FrameInterval;

    if (is_interlaced(Scan))
    {
        float currentspeed = m_parentPlayer->GetPlaySpeed();
        bool normal = (currentspeed > 0.99F) && (currentspeed < 1.01F) && m_parentPlayer->AtNormalSpeed();
        VideoOutput->SetDeinterlacing(true, m_parentPlayer->CanSupportDoubleRate() && normal);
    }
    else if (kScan_Progressive == Scan)
    {
        VideoOutput->SetDeinterlacing(false, false);
    }

    m_scan = Scan;
}

/*! \brief Check whether deinterlacing should be enabled
 *
 * If the user has triggered an override, this will always be used (until 'detect'
 * is requested to turn it off again).
 *
 * For H264 material, the decoder will signal when the current frame is on a new
 * GOP boundary and if the frame's interlaced flag does not match the current
 * scan type, the scan type is unlocked. This works well for all test clips
 * with mixed progressive/interlaced sequences.
 *
 * For all other material, we lock the scan type to interlaced when interlaced
 * frames are seen - and do not unlock if we see progressive frames. This is
 * primarily targetted at MPEG2 material where there is a lot of content where
 * the scan type changes frequently - and for no obvious reason. This will result
 * in 'false positives' in some cases but there is no clear approach that works
 * for all cases. The previous behaviour is preserved (i.e. lock to interlaced
 * if interlaced frames are seen) which results in less erratic playback (as the
 * deinterlacers are not continually switched on and off) and correctly deinterlaces
 * material that is not otherwise flagged correctly.
*/
void MythVideoScanTracker::AutoDeint(MythVideoFrame* Frame, MythVideoOutput* VideoOutput,
                                     std::chrono::microseconds FrameInterval, bool AllowLock)
{
    if (!Frame)
        return;

    if ((m_scanOverride > kScan_Detect) && (m_scan != m_scanOverride))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Locking scan override to '%1'")
            .arg(ScanTypeToUserString(m_scanOverride, true)));
        SetScanType(m_scanOverride, VideoOutput, FrameInterval);
    }

    // This is currently only signalled for H264 content
    if (Frame->m_newGOP)
    {
        if (m_scanOverride < kScan_Interlaced &&
            ((Frame->m_interlaced && !is_interlaced(m_scan)) ||
            (!Frame->m_interlaced && is_interlaced(m_scan))))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Unlocking frame scan");
            m_scanLocked = false;
        }
    }

    if (m_scanLocked)
        return;

    if (Frame->m_interlaced)
    {
        if (m_scanTracker < 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Interlaced frame seen after %1 progressive frames")
                    .arg(abs(m_scanTracker)));
            m_scanTracker = 2;
            if (AllowLock)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Locking scan to Interlaced.");
                SetScanType(kScan_Interlaced, VideoOutput, FrameInterval);
                return;
            }
        }
        m_scanTracker++;
    }
    else
    {
        if (m_scanTracker > 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Progressive frame seen after %1 interlaced frames")
                    .arg(m_scanTracker));
            m_scanTracker = 0;
        }
        m_scanTracker--;
    }

    int min_count = !AllowLock ? 0 : 2;
    if (abs(m_scanTracker) <= min_count)
        return;

    SetScanType((m_scanTracker > min_count) ? kScan_Interlaced : kScan_Progressive,
                VideoOutput, FrameInterval);
    m_scanLocked  = false;
}

FrameScanType MythVideoScanTracker::DetectInterlace(FrameScanType NewScan, float Rate, int VideoHeight)
{
    FrameScanType scan = m_scan;
    QString dbg = QString("DetectInterlace(") + ScanTypeToString(NewScan) +
        QString(", ") + ScanTypeToString(scan) + QString(", ") +
        QString("%1").arg(static_cast<double>(Rate)) + QString(", ") +
        QString("%1").arg(VideoHeight) + QString(") ->");

    if (kScan_Ignore != NewScan || kScan_Detect == scan)
    {
        // The scanning mode should be decoded from the stream - otherwise guess
        // default to interlaced
        scan = kScan_Interlaced;
        // 720P, outside interlaced frame rates or too large for interlaced
        if ((720 == VideoHeight) || (Rate < 25) || (Rate > 30) || (VideoHeight > 1080))
            scan = kScan_Progressive;
        if (kScan_Detect != NewScan)
            scan = NewScan;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + dbg + ScanTypeToString(scan));
    return scan;
}
