// MythTV
#include "libmythbase/mythcorecontext.h"
#include "DetectLetterbox.h"

#define LOC QString("DetectLetterbox: ")

static constexpr int8_t NUMBER_OF_DETECTION_LINES { 3 }; // How many lines are we looking at
static constexpr int8_t THRESHOLD                 { 5 }; // Y component has to not vary more than this in the bars
static constexpr int8_t HORIZONTAL_THRESHOLD      { 4 }; // How tolerant are we that the image has horizontal edges

DetectLetterbox::DetectLetterbox()
{
    int dbAdjustFill = gCoreContext->GetNumSetting("AdjustFill", 0);
    m_isDetectLetterbox = dbAdjustFill >= kAdjustFill_AutoDetect_DefaultOff;
    m_detectLetterboxDefaultMode =
            static_cast<AdjustFillMode>(std::max(static_cast<int>(kAdjustFill_Off),
                                        dbAdjustFill - kAdjustFill_AutoDetect_DefaultOff));
    m_detectLetterboxLimit = gCoreContext->GetNumSetting("DetectLeterboxLimit", 75);
}

/** \fn DetectLetterbox::Detect(VideoFrame*)
 *  \brief Detects if this frame is or is not letterboxed
 *
 *  If a change is detected detectLetterboxSwitchFrame and
 *  detectLetterboxDetectedMode are set.
 */
bool DetectLetterbox::Detect(MythVideoFrame *Frame, float VideoAspect, AdjustFillMode& Current)
{
    if (!Frame || !m_isDetectLetterbox)
        return false;

    unsigned char *buf = Frame->m_buffer;
    if (!buf)
        return false;

    const FramePitches pitches = Frame->m_pitches;
    const FrameOffsets offsets = Frame->m_offsets;
    const int height = Frame->m_height;

    // If the black bars is larger than this limit we switch to Half or Full Mode
    //    const int fullLimit = static_cast<int>(((height - width * 9 / 16) / 2) * m_detectLetterboxLimit / 100);
    //    const int halfLimit = (static_cast<int>(((height - width * 9 / 14) / 2) * m_detectLetterboxLimit / 100);

    // If the black bars is larger than this limit we switch to Half or Full Mode
    const int fullLimit = static_cast<int>((height * (1 - VideoAspect * 9 / 16) / 2) * m_detectLetterboxLimit / 100);
    const int halfLimit = static_cast<int>((height * (1 - VideoAspect * 9 / 14) / 2) * m_detectLetterboxLimit / 100);

    // Lines to scan for black letterbox edge
    const std::array<int,3> xPos { Frame->m_width / 4, Frame->m_width / 2, Frame->m_width * 3 / 4} ;
    int topHits     = 0;
    int bottomHits  = 0;
    int minTop      = 0;
    int minBottom   = 0;
    int maxTop      = 0;
    int maxBottom   = 0;
    std::array<int,3> topHit    = { 0, 0, 0 };
    std::array<int,3> bottomHit = { 0, 0, 0 };

    switch (Frame->m_type)
    {
        case FMT_YV12:
        case FMT_YUV420P9:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P14:
        case FMT_YUV420P16:
        case FMT_NV12:
        case FMT_P010:
        case FMT_P016:
            if (!m_firstFrameChecked || m_frameType != Frame->m_type)
            {
                m_firstFrameChecked = Frame->m_frameNumber;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("'%1' frame format detected")
                    .arg(MythVideoFrame::FormatDescription(Frame->m_type)));
            }
            m_frameType = Frame->m_type;
            break;
        default:
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("'%1' frame format is not supported")
                    .arg(MythVideoFrame::FormatDescription(Frame->m_type)));
            m_isDetectLetterbox = false;
            return false;
    }

    if (Frame->m_frameNumber < 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Strange frame number %1").arg(Frame->m_frameNumber));
        return false;
    }

    if (VideoAspect > 1.5F)
    {
        if (m_detectLetterboxDetectedMode != m_detectLetterboxDefaultMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("The source is already in widescreen (aspect: %1)")
                .arg(static_cast<double>(VideoAspect)));
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = m_detectLetterboxDefaultMode;
            m_detectLetterboxSwitchFrame = Frame->m_frameNumber;
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("The source is already in widescreen (aspect: %1)")
            .arg(static_cast<double>(VideoAspect)));
        m_isDetectLetterbox = false;
        return false;
    }

    // Establish the level of light in the edge
    int averageY = 0;
    for (int pos : xPos)
    {
        averageY += buf[offsets[0] + (5 * pitches[0])            + pos];
        averageY += buf[offsets[0] + ((height - 6) * pitches[0]) + pos];
    }

    averageY /= NUMBER_OF_DETECTION_LINES * 2;
    if (averageY > 64) // Too bright to be a letterbox border
        averageY = 0;

    // Note - for 10/12 bit etc we only sample the most significant byte
    bool triplanar = MythVideoFrame::FormatIs420(m_frameType);
    int depth = MythVideoFrame::ColorDepth(m_frameType);
    int leftshift = depth > 8 ? 1 : 0;
    int rightshift = depth > 8 ? 0 : 1;

    // Scan the detection lines
    for (int y = 5; y < height / 4; y++) // skip first pixels incase of noise in the edge
    {
        for (int detectionLine = 0; detectionLine < NUMBER_OF_DETECTION_LINES; detectionLine++)
        {
            int Y = buf[offsets[0] + (y * pitches[0]) + (xPos[detectionLine] << leftshift)];
            int U = 0;
            int V = 0;
            if (triplanar)
            {
                U = buf[offsets[1] + ((y>>1) * pitches[1]) + (xPos[detectionLine] >> rightshift)];
                V = buf[offsets[2] + ((y>>1) * pitches[2]) + (xPos[detectionLine] >> rightshift)];
            }
            else
            {
                int offset = offsets[1] + ((y >> 1) * pitches[1]) + (xPos[detectionLine & ~0x1] << leftshift);
                U = buf[offset];
                V = buf[offset + (1 << leftshift)];
            }

            if ((!topHit[detectionLine]) &&
                ( Y > averageY + THRESHOLD || Y < averageY - THRESHOLD ||
                  U < 128 - 32 || U > 128 + 32 ||
                  V < 128 - 32 || V > 128 + 32 ))
            {
                topHit[detectionLine] = y;
                topHits++;
                if (!minTop)
                    minTop = y;
                maxTop = y;
            }

            Y = buf[offsets[0] + ((height-y-1) * pitches[0]) + (xPos[detectionLine] << leftshift)];
            if (triplanar)
            {
                U = buf[offsets[1] + (((height-y-1) >> 1) * pitches[1]) + (xPos[detectionLine] >> rightshift)];
                V = buf[offsets[2] + (((height-y-1) >> 1) * pitches[2]) + (xPos[detectionLine] >> rightshift)];
            }
            else
            {
                int offset = offsets[1] + (((height - y -1) >> 1) * pitches[1]) + (xPos[detectionLine & ~0x1] << leftshift);
                U = buf[offset];
                V = buf[offset + (1 << leftshift)];
            }

            if ((!bottomHit[detectionLine]) &&
                ( Y > averageY + THRESHOLD || Y < averageY - THRESHOLD ||
                  U < 128 - 32 || U > 128 + 32 ||
                  V < 128 - 32 || V > 128 + 32 ))
            {
                bottomHit[detectionLine] = y;
                bottomHits++;
                if (!minBottom)
                    minBottom = y;
                maxBottom = y;
            }
        }

        if (topHits == NUMBER_OF_DETECTION_LINES && bottomHits == NUMBER_OF_DETECTION_LINES)
            break;
    }

    if (topHits != NUMBER_OF_DETECTION_LINES)
        maxTop = height / 4;
    if (!minTop)
        minTop = height / 4;
    if (bottomHits != NUMBER_OF_DETECTION_LINES)
        maxBottom = height / 4;
    if (!minBottom)
        minBottom = height / 4;

    bool horizontal = (((minTop != 0) && (maxTop - minTop < HORIZONTAL_THRESHOLD)) &&
                       ((minBottom != 0) && (maxBottom - minBottom < HORIZONTAL_THRESHOLD)));

    if (m_detectLetterboxSwitchFrame > Frame->m_frameNumber) // user is reversing
    {
        m_detectLetterboxDetectedMode = Current;
        m_detectLetterboxSwitchFrame = -1;
        m_detectLetterboxPossibleHalfFrame = -1;
        m_detectLetterboxPossibleFullFrame = -1;
    }

    if (minTop < halfLimit || minBottom < halfLimit)
        m_detectLetterboxPossibleHalfFrame = -1;
    if (minTop < fullLimit || minBottom < fullLimit)
        m_detectLetterboxPossibleFullFrame = -1;

    if (m_detectLetterboxDetectedMode != kAdjustFill_Full)
    {
        if (m_detectLetterboxPossibleHalfFrame == -1 && minTop > halfLimit && minBottom > halfLimit)
            m_detectLetterboxPossibleHalfFrame = Frame->m_frameNumber;
    }
    else
    {
        if (m_detectLetterboxPossibleHalfFrame == -1 && minTop < fullLimit && minBottom < fullLimit)
            m_detectLetterboxPossibleHalfFrame = Frame->m_frameNumber;
    }

    if (m_detectLetterboxPossibleFullFrame == -1 && minTop > fullLimit && minBottom > fullLimit)
        m_detectLetterboxPossibleFullFrame = Frame->m_frameNumber;

    if (maxTop < halfLimit || maxBottom < halfLimit) // Not too restrictive when switching to off
    {
        // No Letterbox
        if (m_detectLetterboxDetectedMode != m_detectLetterboxDefaultMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Non Letterbox detected on line: %1 (limit: %2)")
                .arg(std::min(maxTop, maxBottom)).arg(halfLimit));
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = m_detectLetterboxDefaultMode;
            m_detectLetterboxSwitchFrame = Frame->m_frameNumber;
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
    }
    else if (horizontal && minTop > halfLimit && minBottom > halfLimit &&
             maxTop < fullLimit && maxBottom < fullLimit)
    {
        // Letterbox (with narrow bars)
        if (m_detectLetterboxDetectedMode != kAdjustFill_Half)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Narrow Letterbox detected on line: %1 (limit: %2) frame: %3")
                .arg(minTop).arg(halfLimit).arg(m_detectLetterboxPossibleHalfFrame));
            m_detectLetterboxConsecutiveCounter = 0;
            if (m_detectLetterboxDetectedMode == kAdjustFill_Full && m_detectLetterboxSwitchFrame != -1)
            {
                // Do not change switch frame if switch to Full mode has not been executed yet
            }
            else
            {
                m_detectLetterboxSwitchFrame = m_detectLetterboxPossibleHalfFrame;
            }
            m_detectLetterboxDetectedMode = kAdjustFill_Half;
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
    }
    else if (horizontal && minTop > fullLimit && minBottom > fullLimit)
    {
        // Letterbox
        m_detectLetterboxPossibleHalfFrame = -1;
        if (m_detectLetterboxDetectedMode != kAdjustFill_Full)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Detected Letterbox on line: %1 (limit: %2) frame: %3")
                .arg(minTop).arg(fullLimit).arg(m_detectLetterboxPossibleFullFrame));
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = kAdjustFill_Full;
            m_detectLetterboxSwitchFrame = m_detectLetterboxPossibleFullFrame;
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
    }
    else
    {
        if (m_detectLetterboxConsecutiveCounter <= 3)
            m_detectLetterboxConsecutiveCounter = 0;
    }

    if (m_detectLetterboxSwitchFrame == -1)
        return false;

    if (m_detectLetterboxSwitchFrame <= Frame->m_frameNumber && m_detectLetterboxConsecutiveCounter > 3)
    {
        if (Current != m_detectLetterboxDetectedMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Switched to '%1' on frame %2 (%3)")
                .arg(toString(m_detectLetterboxDetectedMode))
                .arg(Frame->m_frameNumber).arg(m_detectLetterboxSwitchFrame));
            Current = m_detectLetterboxDetectedMode;
            return true;
        }
        m_detectLetterboxSwitchFrame = -1;
    }
    else if (m_detectLetterboxSwitchFrame <= Frame->m_frameNumber)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Not Switched to '%1' on frame %2 "
                    "(%3) Not enough consecutive detections (%4)")
            .arg(toString(m_detectLetterboxDetectedMode))
            .arg(Frame->m_frameNumber).arg(m_detectLetterboxSwitchFrame)
            .arg(m_detectLetterboxConsecutiveCounter));
    }

    return false;
}

void DetectLetterbox::SetDetectLetterbox(bool Detect, AdjustFillMode Mode)
{
    m_isDetectLetterbox = Detect;
    m_detectLetterboxSwitchFrame = -1;
    m_detectLetterboxDetectedMode = Mode;
    m_firstFrameChecked = 0;
}

bool DetectLetterbox::GetDetectLetterbox() const
{
    return m_isDetectLetterbox;
}
