// MythTV
#include "mythcorecontext.h"
#include "DetectLetterbox.h"

#define LOC QString("DetectLetterbox: ")

#define NUMBER_OF_DETECTION_LINES 3 // How many lines are we looking at
#define THRESHOLD 5                 // Y component has to not vary more than this in the bars
#define HORIZONTAL_THRESHOLD 4      // How tolerant are we that the image has horizontal edges

DetectLetterbox::DetectLetterbox(MythPlayer* const Player)
{
    int dbAdjustFill = gCoreContext->GetNumSetting("AdjustFill", 0);
    m_isDetectLetterbox = dbAdjustFill >= kAdjustFill_AutoDetect_DefaultOff;
    m_detectLetterboxDefaultMode =
            static_cast<AdjustFillMode>(max(static_cast<int>(kAdjustFill_Off),
                                            dbAdjustFill - kAdjustFill_AutoDetect_DefaultOff));
    m_detectLetterboxDetectedMode = Player->GetAdjustFill();
    m_detectLetterboxLimit = gCoreContext->GetNumSetting("DetectLeterboxLimit", 75);
    m_player = Player;
}

/** \fn DetectLetterbox::Detect(VideoFrame*)
 *  \brief Detects if this frame is or is not letterboxed
 *
 *  If a change is detected detectLetterboxSwitchFrame and
 *  detectLetterboxDetectedMode are set.
 */
void DetectLetterbox::Detect(VideoFrame *Frame)
{
    if (!Frame || !GetDetectLetterbox())
        return;

    if (!m_player->GetVideoOutput())
        return;

    unsigned char *buf = Frame->buf;
    int *pitches = Frame->pitches;
    int *offsets = Frame->offsets;
    const int height = Frame->height;

    // If the black bars is larger than this limit we switch to Half or Full Mode
    //    const int fullLimit = static_cast<int>(((height - width * 9 / 16) / 2) * m_detectLetterboxLimit / 100);
    //    const int halfLimit = (static_cast<int>(((height - width * 9 / 14) / 2) * m_detectLetterboxLimit / 100);

    // If the black bars is larger than this limit we switch to Half or Full Mode
    const int fullLimit = static_cast<int>((height * (1 - m_player->GetVideoAspect() * 9 / 16) / 2) * m_detectLetterboxLimit / 100);
    const int halfLimit = static_cast<int>((height * (1 - m_player->GetVideoAspect() * 9 / 14) / 2) * m_detectLetterboxLimit / 100);

    // Lines to scan for black letterbox edge
    const int xPos[] = { Frame->width / 4, Frame->width / 2, Frame->width * 3 / 4} ;
    int topHits     = 0;
    int bottomHits  = 0;
    int minTop      = 0;
    int minBottom   = 0;
    int maxTop      = 0;
    int maxBottom   = 0;
    int topHit[]    = { 0, 0, 0 };
    int bottomHit[] = { 0, 0, 0 };

    switch (Frame->codec)
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
            if (!m_firstFrameChecked || m_frameType != Frame->codec)
            {
                m_firstFrameChecked = Frame->frameNumber;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("'%1' frame format detected")
                    .arg(format_description(Frame->codec)));
            }
            m_frameType = Frame->codec;
            break;
        default:
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("'%1' frame format is not supported")
                    .arg(format_description(Frame->codec)));
            m_isDetectLetterbox = false;
            return;
    }

    if (Frame->frameNumber < 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Strange frame number %1")
                 .arg(Frame->frameNumber));
        return;
    }

    if (m_player->GetVideoAspect() > 1.5F)
    {
        if (m_detectLetterboxDetectedMode != m_detectLetterboxDefaultMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("The source is already in widescreen (aspect: %1)")
                    .arg(static_cast<double>(m_player->GetVideoAspect())));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = m_detectLetterboxDefaultMode;
            m_detectLetterboxSwitchFrame = Frame->frameNumber;
            m_detectLetterboxLock.unlock();
        }
        else
        {
            m_detectLetterboxConsecutiveCounter++;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("The source is already in widescreen (aspect: %1)")
                .arg(static_cast<double>(m_player->GetVideoAspect())));
        m_isDetectLetterbox = false;
        return;
    }

    // Establish the level of light in the edge
    int averageY = 0;
    for (int detectionLine = 0; detectionLine < NUMBER_OF_DETECTION_LINES; detectionLine++)
    {
        averageY += buf[offsets[0] + 5 * pitches[0]            + xPos[detectionLine]];
        averageY += buf[offsets[0] + (height - 6) * pitches[0] + xPos[detectionLine]];
    }

    averageY /= NUMBER_OF_DETECTION_LINES * 2;
    if (averageY > 64) // Too bright to be a letterbox border
        averageY = 0;

    // Note - for 10/12 bit etc we only sample the most significant byte
    bool triplanar = format_is_420(m_frameType);
    int depth = ColorDepth(m_frameType);
    int leftshift = depth > 8 ? 1 : 0;
    int rightshift = depth > 8 ? 0 : 1;

    // Scan the detection lines
    for (int y = 5; y < height / 4; y++) // skip first pixels incase of noise in the edge
    {
        for (int detectionLine = 0; detectionLine < NUMBER_OF_DETECTION_LINES; detectionLine++)
        {
            int Y = buf[offsets[0] + y * pitches[0] + (xPos[detectionLine] << leftshift)];
            int U = 0;
            int V = 0;
            if (triplanar)
            {
                U = buf[offsets[1] + (y>>1) * pitches[1] + (xPos[detectionLine] >> rightshift)];
                V = buf[offsets[2] + (y>>1) * pitches[2] + (xPos[detectionLine] >> rightshift)];
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

            Y = buf[offsets[0] + (height-y-1) * pitches[0] + (xPos[detectionLine] << leftshift)];
            if (triplanar)
            {
                U = buf[offsets[1] + ((height-y-1) >> 1) * pitches[1] + (xPos[detectionLine] >> rightshift)];
                V = buf[offsets[2] + ((height-y-1) >> 1) * pitches[2] + (xPos[detectionLine] >> rightshift)];
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

    bool horizontal = ((minTop && maxTop - minTop < HORIZONTAL_THRESHOLD) &&
                       (minBottom && maxBottom - minBottom < HORIZONTAL_THRESHOLD));

    if (m_detectLetterboxSwitchFrame > Frame->frameNumber) // user is reversing
    {
        m_detectLetterboxLock.lock();
        m_detectLetterboxDetectedMode = m_player->GetAdjustFill();
        m_detectLetterboxSwitchFrame = -1;
        m_detectLetterboxPossibleHalfFrame = -1;
        m_detectLetterboxPossibleFullFrame = -1;
        m_detectLetterboxLock.unlock();
    }

    if (minTop < halfLimit || minBottom < halfLimit)
        m_detectLetterboxPossibleHalfFrame = -1;
    if (minTop < fullLimit || minBottom < fullLimit)
        m_detectLetterboxPossibleFullFrame = -1;

    if (m_detectLetterboxDetectedMode != kAdjustFill_Full)
    {
        if (m_detectLetterboxPossibleHalfFrame == -1 &&
            minTop > halfLimit && minBottom > halfLimit)
        {
            m_detectLetterboxPossibleHalfFrame = Frame->frameNumber;
        }
    }
    else
    {
        if (m_detectLetterboxPossibleHalfFrame == -1 &&
            minTop < fullLimit && minBottom < fullLimit)
        {
            m_detectLetterboxPossibleHalfFrame = Frame->frameNumber;
        }
    }
    if (m_detectLetterboxPossibleFullFrame == -1 &&
        minTop > fullLimit && minBottom > fullLimit)
    {
        m_detectLetterboxPossibleFullFrame = Frame->frameNumber;
    }

    if (maxTop < halfLimit || maxBottom < halfLimit) // Not too restrictive when switching to off
    {
        // No Letterbox
        if (m_detectLetterboxDetectedMode != m_detectLetterboxDefaultMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Non Letterbox detected on line: %1 (limit: %2)")
                    .arg(min(maxTop, maxBottom)).arg(halfLimit));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = m_detectLetterboxDefaultMode;
            m_detectLetterboxSwitchFrame = Frame->frameNumber;
            m_detectLetterboxLock.unlock();
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
                    .arg(minTop).arg(halfLimit)
                    .arg(m_detectLetterboxPossibleHalfFrame));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            if (m_detectLetterboxDetectedMode == kAdjustFill_Full &&
                m_detectLetterboxSwitchFrame != -1)
            {
                // Do not change switch frame if switch to Full mode has not been executed yet
            }
            else
            {
                m_detectLetterboxSwitchFrame = m_detectLetterboxPossibleHalfFrame;
            }
            m_detectLetterboxDetectedMode = kAdjustFill_Half;
            m_detectLetterboxLock.unlock();
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
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Detected Letterbox on line: %1 (limit: %2) frame: %3").arg(minTop)
                    .arg(fullLimit).arg(m_detectLetterboxPossibleFullFrame));
            m_detectLetterboxLock.lock();
            m_detectLetterboxConsecutiveCounter = 0;
            m_detectLetterboxDetectedMode = kAdjustFill_Full;
            m_detectLetterboxSwitchFrame = m_detectLetterboxPossibleFullFrame;
            m_detectLetterboxLock.unlock();
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
}

/** \fn DetectLetterbox::SwitchTo(VideoFrame*)
 *  \brief Switch to the mode detected by DetectLetterbox
 *
 *  Switch fill mode if a switch was detected for this frame.
 */
void DetectLetterbox::SwitchTo(VideoFrame *Frame)
{
    if (!GetDetectLetterbox())
        return;

    if (m_detectLetterboxSwitchFrame == -1)
        return;

    m_detectLetterboxLock.lock();
    if (m_detectLetterboxSwitchFrame <= Frame->frameNumber &&
        m_detectLetterboxConsecutiveCounter > 3)
    {
        if (m_player->GetAdjustFill() != m_detectLetterboxDetectedMode)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Switched to '%1' on frame %2 (%3)")
                    .arg(toString(m_detectLetterboxDetectedMode))
                    .arg(Frame->frameNumber)
                    .arg(m_detectLetterboxSwitchFrame));
            m_player->GetVideoOutput()->ToggleAdjustFill(m_detectLetterboxDetectedMode);
            m_player->ReinitOSD();
        }
        m_detectLetterboxSwitchFrame = -1;
    }
    else if (m_detectLetterboxSwitchFrame <= Frame->frameNumber)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Not Switched to '%1' on frame %2 "
                    "(%3) Not enough consecutive detections (%4)")
                .arg(toString(m_detectLetterboxDetectedMode))
                .arg(Frame->frameNumber).arg(m_detectLetterboxSwitchFrame)
                .arg(m_detectLetterboxConsecutiveCounter));
    }

    m_detectLetterboxLock.unlock();
}

void DetectLetterbox::SetDetectLetterbox(bool Detect)
{
    m_isDetectLetterbox = Detect;
    m_detectLetterboxSwitchFrame = -1;
    m_detectLetterboxDetectedMode = m_player->GetAdjustFill();
    m_firstFrameChecked = 0;
}

bool DetectLetterbox::GetDetectLetterbox() const
{
    return m_isDetectLetterbox;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
