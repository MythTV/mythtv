// -*- Mode: c++ -*-

// MythTV headers
#include "DetectLetterbox.h"
#include "NuppelVideoPlayer.h"
#include "videoouttypes.h"
#include "mythcorecontext.h"

DetectLetterbox::DetectLetterbox(NuppelVideoPlayer* const nvp)
{
    int dbAdjustFill = gCoreContext->GetNumSetting("AdjustFill", 0);
    isDetectLetterbox = dbAdjustFill >= kAdjustFill_AutoDetect_DefaultOff;
    firstFrameChecked = 0;
    detectLetterboxDefaultMode = (AdjustFillMode) max((int) kAdjustFill_Off,
                                 dbAdjustFill - kAdjustFill_AutoDetect_DefaultOff);
    detectLetterboxSwitchFrame = -1;
    detectLetterboxPossibleHalfFrame = -1;
    detectLetterboxPossibleFullFrame = -1;
    detectLetterboxConsecutiveCounter = 0;
    detectLetterboxDetectedMode = nvp->GetAdjustFill();
    detectLetterboxLimit = gCoreContext->GetNumSetting("DetectLeterboxLimit", 75);
    nupple_video_player = nvp;
}

DetectLetterbox::~DetectLetterbox()
{
}

/** \fn DetectLetterbox::Detect(VideoFrame*)
 *  \brief Detects if this frame is or is not letterboxed
 *
 *  If a change is detected detectLetterboxSwitchFrame and
 *  detectLetterboxDetectedMode are set.
 */
void DetectLetterbox::Detect(VideoFrame *frame)
{
    unsigned char *buf = frame->buf;
    int *pitches = frame->pitches;
    int *offsets = frame->offsets;
    const int width = frame->width;
    const int height = frame->height;
    const long long frameNumber = frame->frameNumber;
    const int NUMBER_OF_DETECTION_LINES = 3; // How many lines are we looking at
    const int THRESHOLD = 5; // Y component has to not vary more than this in the bars
    const int HORIZONTAL_THRESHOLD = 4; // How tolerant are we that the image has horizontal edges

    // If the black bars is larger than this limit we switch to Half or Full Mode
    //    const int fullLimit = (int) (((height - width * 9 / 16) / 2) * detectLetterboxLimit / 100);
    //    const int halfLimit = (int) (((height - width * 9 / 14) / 2) * detectLetterboxLimit / 100);
    // If the black bars is larger than this limit we switch to Half or Full Mode
    const int fullLimit = (int) ((height * (1 - nupple_video_player->GetVideoAspect() * 9 / 16) / 2) * detectLetterboxLimit / 100);
    const int halfLimit = (int) ((height * (1 - nupple_video_player->GetVideoAspect() * 9 / 14) / 2) * detectLetterboxLimit / 100);

    const int xPos[] = {width / 4, width / 2, width * 3 / 4};    // Lines to scan for black letterbox edge
    int topHits = 0, bottomHits = 0, minTop = 0, minBottom = 0, maxTop = 0, maxBottom = 0;
    int topHit[] = {0, 0, 0}, bottomHit[] = {0, 0, 0};

    if (!GetDetectLetterbox())
        return;

    if (!nupple_video_player->getVideoOutput())
        return;

    switch (frame->codec) {
        case FMT_YV12:
            if (!firstFrameChecked)
            {
                firstFrameChecked = frameNumber;
                VERBOSE(VB_PLAYBACK,
                        QString("Detect Letterbox: YV12 frame format detected"));
            }
            break;
        default:
            VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: The source is not "
                    "a supported frame format (was %1)").arg(frame->codec));
            isDetectLetterbox = false;
            return;
    }

    if (frameNumber < 0)
    {
        VERBOSE(VB_PLAYBACK,
                QString("Detect Letterbox: Strange frame number %1")
                .arg(frameNumber));
        return;
    }

    if (nupple_video_player->GetVideoAspect() > 1.5)
    {
        if (detectLetterboxDetectedMode != detectLetterboxDefaultMode)
        {
            VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: The source is "
                    "already in widescreen (aspect: %1)")
                    .arg(nupple_video_player->GetVideoAspect()));
            detectLetterboxLock.lock();
            detectLetterboxConsecutiveCounter = 0;
            detectLetterboxDetectedMode = detectLetterboxDefaultMode;
            detectLetterboxSwitchFrame = frameNumber;
            detectLetterboxLock.unlock();
        }
        else
        {
            detectLetterboxConsecutiveCounter++;
        }
        VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: The source is already "
                "in widescreen (aspect: %1)")
                .arg(nupple_video_player->GetVideoAspect()));
        isDetectLetterbox = false;
        return;
    }

    // Establish the level of light in the edge
    int averageY = 0;
    for (int detectionLine = 0;
         detectionLine < NUMBER_OF_DETECTION_LINES;
         detectionLine++)
    {
        averageY += buf[offsets[0] + 5 * pitches[0]            + xPos[detectionLine]];
        averageY += buf[offsets[0] + (height - 6) * pitches[0] + xPos[detectionLine]];
    }
    averageY /= NUMBER_OF_DETECTION_LINES * 2;
    if (averageY > 64) // To bright to be a letterbox border
        averageY = 0;

    // Scan the detection lines
    for (int y = 5; y < height / 4; y++) // skip first pixels incase of noise in the edge
    {
        for (int detectionLine = 0;
             detectionLine < NUMBER_OF_DETECTION_LINES;
             detectionLine++)
        {
            int Y = buf[offsets[0] +  y     * pitches[0] +  xPos[detectionLine]];
            int U = buf[offsets[1] + (y>>1) * pitches[1] + (xPos[detectionLine]>>1)];
            int V = buf[offsets[2] + (y>>1) * pitches[2] + (xPos[detectionLine]>>1)];
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

            Y = buf[offsets[0] + (height-y-1     ) * pitches[0] + xPos[detectionLine]];
            U = buf[offsets[1] + ((height-y-1) >> 1) * pitches[1] + (xPos[detectionLine]>>1)];
            V = buf[offsets[2] + ((height-y-1) >> 1) * pitches[2] + (xPos[detectionLine]>>1)];
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

        if (topHits == NUMBER_OF_DETECTION_LINES &&
            bottomHits == NUMBER_OF_DETECTION_LINES)
        {
            break;
        }
    }
    if (topHits != NUMBER_OF_DETECTION_LINES) maxTop = height / 4;
    if (!minTop) minTop = height / 4;
    if (bottomHits != NUMBER_OF_DETECTION_LINES) maxBottom = height / 4;
    if (!minBottom) minBottom = height / 4;

    bool horizontal = ((minTop && maxTop - minTop < HORIZONTAL_THRESHOLD) &&
                       (minBottom && maxBottom - minBottom < HORIZONTAL_THRESHOLD));

    if (detectLetterboxSwitchFrame > frameNumber) // user is reversing
    {
        detectLetterboxLock.lock();
        detectLetterboxDetectedMode = nupple_video_player->GetAdjustFill();
        detectLetterboxSwitchFrame = -1;
        detectLetterboxPossibleHalfFrame = -1;
        detectLetterboxPossibleFullFrame = -1;
        detectLetterboxLock.unlock();
    }

    if (minTop < halfLimit || minBottom < halfLimit)
        detectLetterboxPossibleHalfFrame = -1;
    if (minTop < fullLimit || minBottom < fullLimit)
        detectLetterboxPossibleFullFrame = -1;

    if (detectLetterboxDetectedMode != kAdjustFill_Full)
    {
        if (detectLetterboxPossibleHalfFrame == -1 &&
            minTop > halfLimit && minBottom > halfLimit) {
            detectLetterboxPossibleHalfFrame = frameNumber;
        }
    }
    else
    {
        if (detectLetterboxPossibleHalfFrame == -1 &&
            minTop < fullLimit && minBottom < fullLimit) {
            detectLetterboxPossibleHalfFrame = frameNumber;
        }
    }
    if (detectLetterboxPossibleFullFrame == -1 &&
        minTop > fullLimit && minBottom > fullLimit)
        detectLetterboxPossibleFullFrame = frameNumber;

    if ( maxTop < halfLimit || maxBottom < halfLimit) // Not to restrictive when switching to off
    {
        // No Letterbox
        if (detectLetterboxDetectedMode != detectLetterboxDefaultMode)
        {
            VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: Non Letterbox "
                    "detected on line: %1 (limit: %2)")
                    .arg(min(maxTop, maxBottom)).arg(halfLimit));
            detectLetterboxLock.lock();
            detectLetterboxConsecutiveCounter = 0;
            detectLetterboxDetectedMode = detectLetterboxDefaultMode;
            detectLetterboxSwitchFrame = frameNumber;
            detectLetterboxLock.unlock();
        }
        else
        {
            detectLetterboxConsecutiveCounter++;
        }
    }
    else if (horizontal && minTop > halfLimit && minBottom > halfLimit &&
             maxTop < fullLimit && maxBottom < fullLimit)
    {
        // Letterbox (with narrow bars)
        if (detectLetterboxDetectedMode != kAdjustFill_Half)
        {
            VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: Narrow Letterbox "
                    "detected on line: %1 (limit: %2) frame: %3")
                    .arg(minTop).arg(halfLimit)
                    .arg(detectLetterboxPossibleHalfFrame));
            detectLetterboxLock.lock();
            detectLetterboxConsecutiveCounter = 0;
            if (detectLetterboxDetectedMode == kAdjustFill_Full &&
                detectLetterboxSwitchFrame != -1) {
                // Do not change switch frame if switch to Full mode has not been executed yet
            }
            else
                detectLetterboxSwitchFrame = detectLetterboxPossibleHalfFrame;
            detectLetterboxDetectedMode = kAdjustFill_Half;
            detectLetterboxLock.unlock();
        }
        else
        {
            detectLetterboxConsecutiveCounter++;
        }
    }
    else if (horizontal && minTop > fullLimit && minBottom > fullLimit)
    {
        // Letterbox
        detectLetterboxPossibleHalfFrame = -1;
        if (detectLetterboxDetectedMode != kAdjustFill_Full)
        {
            VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: Detected Letterbox "
                    "on line: %1 (limit: %2) frame: %2").arg(minTop)
                    .arg(fullLimit).arg(detectLetterboxPossibleFullFrame));
            detectLetterboxLock.lock();
            detectLetterboxConsecutiveCounter = 0;
            detectLetterboxDetectedMode = kAdjustFill_Full;
            detectLetterboxSwitchFrame = detectLetterboxPossibleFullFrame;
            detectLetterboxLock.unlock();
        }
        else
        {
            detectLetterboxConsecutiveCounter++;
        }
    }
    else
    {
        if (detectLetterboxConsecutiveCounter <= 3)
            detectLetterboxConsecutiveCounter = 0;
    }
}

/** \fn DetectLetterbox::SwitchTo(VideoFrame*)
 *  \brief Switch to the mode detected by DetectLetterbox
 *
 *  Switch fill mode if a switch was detected for this frame.
 */
void DetectLetterbox::SwitchTo(VideoFrame *frame)
{
    if (!GetDetectLetterbox())
        return;

    if (detectLetterboxSwitchFrame != -1)
    {
        detectLetterboxLock.lock();
        if (detectLetterboxSwitchFrame <= frame->frameNumber &&
            detectLetterboxConsecutiveCounter > 3)
        {
            if (nupple_video_player->GetAdjustFill() != detectLetterboxDetectedMode)
            {
                VERBOSE(VB_PLAYBACK, QString("Detect Letterbox: Switched to %1 "
                        "on frame %2 (%3)").arg(detectLetterboxDetectedMode)
                        .arg(frame->frameNumber).arg(detectLetterboxSwitchFrame));
                nupple_video_player->getVideoOutput()->ToggleAdjustFill(detectLetterboxDetectedMode);
                nupple_video_player->ReinitOSD();
            }
            detectLetterboxSwitchFrame = -1;
        }
        else if (detectLetterboxSwitchFrame <= frame->frameNumber)
            VERBOSE(VB_PLAYBACK,
                    QString("Detect Letterbox: Not Switched to %1 on frame %2 "
                            "(%3) Not enough consecutive detections (%4)")
                    .arg(detectLetterboxDetectedMode)
                    .arg(frame->frameNumber).arg(detectLetterboxSwitchFrame)
                    .arg(detectLetterboxConsecutiveCounter));

        detectLetterboxLock.unlock();
    }
}

void DetectLetterbox::SetDetectLetterbox(bool detect)
{
    isDetectLetterbox = detect;
    detectLetterboxSwitchFrame = -1;
    detectLetterboxDetectedMode = nupple_video_player->GetAdjustFill();
    firstFrameChecked = 0;
}

bool DetectLetterbox::GetDetectLetterbox()
{
    return isDetectLetterbox;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
