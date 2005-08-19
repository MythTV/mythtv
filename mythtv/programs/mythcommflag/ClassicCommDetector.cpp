#include <cmath>
#include <unistd.h>

#include "ClassicCommDetector.h"
#include "libmythtv/NuppelVideoPlayer.h"

#include "qstring.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/programinfo.h"

// This is used as a bitmask.
enum SkipTypes {
    COMM_DETECT_OFF         = 0x00,
    COMM_DETECT_BLANKS      = 0x01,
    COMM_DETECT_SCENE       = 0x02,
    COMM_DETECT_BLANK_SCENE = 0x03,
    COMM_DETECT_LOGO        = 0x04,
    COMM_DETECT_ALL         = 0xFF
};

enum frameMaskValues {
    COMM_FRAME_BLANK         = 0x0001,
    COMM_FRAME_SCENE_CHANGE  = 0x0002,
    COMM_FRAME_LOGO_PRESENT  = 0x0004,
    COMM_FRAME_ASPECT_CHANGE = 0x0008,
    COMM_FRAME_RATING_SYMBOL = 0x0010
};

enum frameAspects {
    COMM_ASPECT_NORMAL = 0,
    COMM_ASPECT_WIDE
};

enum frameFormats {
    COMM_FORMAT_NORMAL = 0,
    COMM_FORMAT_LETTERBOX,
    COMM_FORMAT_PILLARBOX,
    COMM_FORMAT_MAX
};

ClassicCommDetector::ClassicCommDetector(int commDetectMethod_in,
                                         bool showProgress_in,
                                         bool fullSpeed_in,
                                         NuppelVideoPlayer* nvp_in,
                                         const QDateTime& recordingStartedAt_in,
                                         const QDateTime& recordingStopsAt_in) :
        commDetectMethod(commDetectMethod_in),
        showProgress(showProgress_in),
        fullSpeed(fullSpeed_in),
        nvp(nvp_in),
        recordingStartedAt(recordingStartedAt_in),
        recordingStopsAt(recordingStopsAt_in)
{

    edgeMask = NULL;
    logoFrame = NULL;
    logoMask = NULL;
    logoCheckMask = NULL;
    logoMaxValues = NULL;
    logoMinValues = NULL;
    tmpBuf = NULL;

    stillRecording = recordingStopsAt > QDateTime::currentDateTime();
    
    commDetectBorder =
        gContext->GetNumSetting("CommDetectBorder", 20);
    commDetectBlankFrameMaxDiff =
        gContext->GetNumSetting("CommDetectBlankFrameMaxDiff", 25);
    commDetectDarkBrightness =
        gContext->GetNumSetting("CommDetectDarkBrightness", 80);
    commDetectDimBrightness =
        gContext->GetNumSetting("CommDetectDimBrightness", 120);
    commDetectBoxBrightness =
        gContext->GetNumSetting("CommDetectBoxBrightness", 30);
    commDetectDimAverage =
        gContext->GetNumSetting("CommDetectDimAverage", 35);
    commDetectMaxCommBreakLength =
        gContext->GetNumSetting("CommDetectMaxCommBreakLength", 395);
    commDetectMinCommBreakLength =
        gContext->GetNumSetting("CommDetectMinCommBreakLength", 60);
    commDetectMinShowLength =
        gContext->GetNumSetting("CommDetectMinShowLength", 65);
    commDetectMaxCommLength =
        gContext->GetNumSetting("CommDetectMaxCommLength", 125);
    commDetectLogoSamplesNeeded =
        gContext->GetNumSetting("CommDetectLogoSamplesNeeded", 240);
    commDetectLogoSampleSpacing =
        gContext->GetNumSetting("CommDetectLogoSampleSpacing", 2);
    commDetectLogoSecondsNeeded = commDetectLogoSamplesNeeded *
                                  commDetectLogoSampleSpacing;
    commDetectLogoGoodEdgeThreshold =
        gContext->GetSetting("CommDetectLogoGoodEdgeThreshold", "0.75")
        .toDouble();
    commDetectLogoBadEdgeThreshold =
        gContext->GetSetting("CommDetectLogoBadEdgeThreshold", "0.85")
        .toDouble();
    skipAllBlanks = !!gContext->GetNumSetting("CommSkipAllBlanks", 1);
    commDetectBlankCanHaveLogo =
        !!gContext->GetNumSetting("CommDetectBlankCanHaveLogo", 1);
}

void ClassicCommDetector::Init()
{
    width = nvp->GetVideoWidth();
    height = nvp->GetVideoHeight();
    fps = nvp->GetFrameRate();

#ifdef SHOW_DEBUG_WIN
    comm_debug_init(width, height);
#endif

    currentAspect = COMM_ASPECT_WIDE;

    curFrameNumber = -1;

    if (getenv("DEBUGCOMMFLAG"))
        verboseDebugging = true;
    else
        verboseDebugging = false;

    VERBOSE(VB_COMMFLAG, "Commercial Detection initialized: width = " <<
            width << ", height = " << height << ", fps = " <<
            nvp->GetFrameRate() << ", method = " << commDetectMethod);

    if ((width * height) > 1000000)
    {
        horizSpacing = 10;
        vertSpacing = 10;
    }
    else if ((width * height) > 800000)
    {
        horizSpacing = 8;
        vertSpacing = 8;
    }
    else if ((width * height) > 400000)
    {
        horizSpacing = 6;
        vertSpacing = 6;
    }
    else if ((width * height) > 300000)
    {
        horizSpacing = 6;
        vertSpacing = 4;
    }
    else
    {
        horizSpacing = 4;
        vertSpacing = 4;
    }

    VERBOSE(VB_COMMFLAG,
            QString("Using Sample Spacing of %1 horizontal & %2 vertical "
                    "pixels.").arg(horizSpacing).arg(vertSpacing));

    framesProcessed = 0;
    totalMinBrightness = 0;
    blankFrameCount = 0;

    aggressiveDetection = true;
    currentAspect = COMM_ASPECT_WIDE;
    decoderFoundAspectChanges = false;

    lastSentCommBreakMap.clear();
    commBreakMapUpdateRequested = false;
    sendCommBreakMapUpdates = false;

    // Check if close to 4:3
    if(fabs(((width*1.0)/height) - 1.333333) < 0.1)
        currentAspect = COMM_ASPECT_NORMAL;

    lastFrameWasSceneChange = false;

    memset(lastHistogram, 0, sizeof(lastHistogram));
    memset(histogram, 0, sizeof(histogram));
    lastHistogram[0] = -1;
    histogram[0] = -1;

    frameIsBlank = false;
    sceneHasChanged = false;
    stationLogoPresent = false;

    framePtr = NULL;

    if (edgeMask)
        delete [] edgeMask;
    edgeMask = new EdgeMaskEntry[width * height];

    if (logoFrame)
        delete [] logoFrame;
    logoFrame = new unsigned char[width * height];

    if (logoMask)
        delete [] logoMask;
    logoMask = new unsigned char[width * height];

    if (logoCheckMask)
        delete [] logoCheckMask;
    logoCheckMask = new unsigned char[width * height];

    if (logoMaxValues)
        delete [] logoMaxValues;
    logoMaxValues = new unsigned char[width * height];

    if (logoMinValues)
        delete [] logoMinValues;
    logoMinValues = new unsigned char[width * height];

    if (tmpBuf)
        delete [] tmpBuf;
    tmpBuf = new unsigned char[width * height];

    logoFrameCount = 0;
    logoInfoAvailable = false;

    ClearAllMaps();

    if (verboseDebugging)
    {
        VERBOSE(VB_COMMFLAG, "       Fr #      Min Max Avg Scn F A Mask");
        VERBOSE(VB_COMMFLAG, "       ------    --- --- --- --- - - ----");
    }
}

ClassicCommDetector::~ClassicCommDetector()
{
    if (edgeMask)
        delete [] edgeMask;
    if (logoFrame)
        delete [] logoFrame;
    if (logoMask)
        delete [] logoMask;
    if (logoCheckMask)
        delete [] logoCheckMask;
    if (logoMaxValues)
        delete [] logoMaxValues;
    if (logoMinValues)
        delete [] logoMinValues;
    if (tmpBuf)
        delete [] tmpBuf;
}

bool ClassicCommDetector::go()
{
    nvp->SetNullVideo();
    
    int requiredHeadStart = 30;
    if (commDetectMethod & COMM_DETECT_LOGO)
    {
        requiredHeadStart += commDetectLogoSecondsNeeded;
    }

    emit statusUpdate("Building Detection Buffer");

    int a = 0;
    while (a < requiredHeadStart && stillRecording)
    {
        emit breathe();
        if (m_bStop)
            return false;
        sleep(2);
        a = recordingStartedAt.secsTo(QDateTime::currentDateTime());
    }

    if (nvp->OpenFile() < 0)
        return false;

    Init();

    aggressiveDetection = gContext->GetNumSetting("AggressiveCommDetect", 1);

    if (!nvp->InitVideo())
    {
        VERBOSE(VB_IMPORTANT,
                "NVP: Unable to initialize video for FlagCommercials.");
        return false;
    }

    if ((commDetectMethod & COMM_DETECT_LOGO) &&
        ((nvp->GetLength() == 0) ||
         (nvp->GetLength() > commDetectLogoSecondsNeeded)))
    {
        emit statusUpdate("Searching for Logo");

        if (showProgress)
        {
            cerr << "Finding Logo";
            cerr.flush();
        }

        SearchForLogo();

        if (showProgress)
        {
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b            "
                    "\b\b\b\b\b\b\b\b\b\b\b\b";
            cerr.flush();
        }
    }

    QTime flagTime;
    flagTime.start();
    
    long usecPerFrame = (long)(1.0 / nvp->GetFrameRate() * 1000000);

    long long myTotalFrames;
    if (recordingStopsAt < QDateTime::currentDateTime() )
        myTotalFrames = nvp->GetTotalFrameCount();
    else
        myTotalFrames = (long long)(nvp->GetFrameRate() *
                        (recordingStartedAt.secsTo(recordingStopsAt)));

    if (showProgress)
    {
        if (myTotalFrames)
            cerr << "  0%/      ";
        else
            cerr << "     0/      ";
        cerr.flush();
    }

 
    float flagFPS;
    long long  currentFrameNumber;
    float aspect = nvp->GetVideoAspect();
    float newAspect = aspect;

    SetVideoParams(aspect);

    emit breathe();

    while (!nvp->GetEof())
    {
        struct timeval startTime;
        if (stillRecording)
            gettimeofday(&startTime, NULL);

        VideoFrame* currentFrame = nvp->GetRawVideoFrame();
        currentFrameNumber = currentFrame->frameNumber;

        //Lucas: maybe we should make the nuppelvideoplayer send out a signal
        //when the aspect ratio changes.
        //In order to not change too many things at a time, I"m using basic
        //polling for now.
        newAspect = nvp->GetVideoAspect();
        if (newAspect != aspect)
        {
            SetVideoParams(aspect);
            aspect = newAspect;
        }

        if (((currentFrameNumber % 500) == 0) ||
            (((currentFrameNumber % 100) == 0) &&
             (stillRecording)))
        {
            emit breathe();
            if (m_bStop)
                return false;
        }

        if ((sendCommBreakMapUpdates) &&
            ((commBreakMapUpdateRequested) ||
             ((currentFrameNumber % 500) == 0)))
        {
            QMap<long long,int> commBreakMap;
            QMap<long long, int>::Iterator it;
            QMap<long long, int>::Iterator lastIt;
            bool mapsAreIdentical = false;

            getCommercialBreakList(commBreakMap);

            if ((commBreakMap.size() == 0) &&
                (lastSentCommBreakMap.size() == 0))
            {
                mapsAreIdentical = true;
            }
            else if (commBreakMap.size() == lastSentCommBreakMap.size())
            {
                // assume true for now and set false if we find a difference
                mapsAreIdentical = true;
                for (it = commBreakMap.begin();
                     it != commBreakMap.end() && mapsAreIdentical; ++it)
                {
                    lastIt = lastSentCommBreakMap.find(it.key());
                    if ((lastIt == lastSentCommBreakMap.end()) ||
                        (lastIt.data() != it.data()))
                        mapsAreIdentical = false;
                }
            }

            if (commBreakMapUpdateRequested || !mapsAreIdentical)
            {
                emit gotNewCommercialBreakList();
                lastSentCommBreakMap = commBreakMap;
            }

            if (commBreakMapUpdateRequested)
                commBreakMapUpdateRequested = false;
        }

        while (m_bPaused)
        {
            emit breathe();
            sleep(1);
        }

        // sleep a little so we don't use all cpu even if we're niced
        if (!fullSpeed && !stillRecording)
            usleep(10000);

        if (((currentFrameNumber % 500) == 0) ||
            ((showProgress || stillRecording) &&
             ((currentFrameNumber % 100) == 0)))
        {
            float elapsed = flagTime.elapsed() / 1000.0;

            if (elapsed)
                flagFPS = currentFrameNumber / elapsed;
            else
                flagFPS = 0.0;

            int percentage;
            if (myTotalFrames)
                percentage = currentFrameNumber * 100 / myTotalFrames;
            else
                percentage = 0;

            if (percentage > 100)
                percentage = 100;

            if (showProgress)
            {
                if (myTotalFrames)
                {
                    cerr << "\b\b\b\b\b\b\b\b\b\b\b"
                         << QString::number(percentage).rightJustify(3, ' ')
                         << "%/"
                         << QString::number((int)flagFPS).rightJustify(3, ' ')
                         << "fps";
                }
                else
                {
                    cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b"
                         << QString::number(currentFrameNumber)
                                            .rightJustify(6, ' ')
                         << "/"
                         << QString::number((int)flagFPS).rightJustify(3, ' ')
                         << "fps";
                }
                cerr.flush();
            }

            if (myTotalFrames)
                emit statusUpdate(QObject::tr("%1% Completed @ %2 fps.")
                                  .arg(percentage).arg(flagFPS));
            else
                emit statusUpdate(QObject::tr("%1 Frames Completed @ %2 fps.")
                                  .arg((long)currentFrameNumber).arg(flagFPS));
        }

        ProcessFrame(currentFrame, currentFrameNumber);

        if (stillRecording)
        {
            usecPerFrame = (long)(1.0 / nvp->GetFrameRate() * 1000000);

            struct timeval endTime;
            gettimeofday(&endTime, NULL);

            long long usecSleep =
                      usecPerFrame -
                      (((endTime.tv_sec - startTime.tv_sec) * 1000000) +
                       (endTime.tv_usec - startTime.tv_usec));

            const int alwaysStayNSecondsBehind = 120;
            
            int secondsRecorded =
                recordingStartedAt.secsTo(QDateTime::currentDateTime());
            int secondsFlagged = (int)(framesProcessed / fps);
            
            if ((secondsRecorded - secondsFlagged) > alwaysStayNSecondsBehind)
                usecSleep = (long)(usecSleep * 0.33);
            else
                usecSleep = (long)(usecPerFrame * 1.5);
            
            if (usecSleep > 0)
                usleep(usecSleep);
        }
    }

    if (showProgress)
    {
        float elapsed = flagTime.elapsed() / 1000.0;

        if (elapsed)
            flagFPS = currentFrameNumber / elapsed;
        else
            flagFPS = 0.0;

        if (myTotalFrames)
            cerr << "\b\b\b\b\b\b      \b\b\b\b\b\b";
        else
            cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b             "
                    "\b\b\b\b\b\b\b\b\b\b\b\b\b";
        cerr.flush();
    }

    return true;
}

void ClassicCommDetector::getCommercialBreakList(QMap<long long, int> &marks)
{

    VERBOSE(VB_COMMFLAG, "CommDetect::GetCommBreakMap()");

    marks.clear();

    CleanupFrameInfo();

    switch (commDetectMethod)
    {
            case COMM_DETECT_OFF:         return;

            case COMM_DETECT_BLANKS:      BuildBlankFrameCommList();
                                          marks = blankCommBreakMap;
                                          break;

            case COMM_DETECT_SCENE:       BuildSceneChangeCommList();
                                          marks = sceneCommBreakMap;
                                          break;

            case COMM_DETECT_BLANK_SCENE: BuildBlankFrameCommList();
                                          BuildSceneChangeCommList();
                                          BuildMasterCommList();
                                          marks = commBreakMap;
                                          break;

            case COMM_DETECT_LOGO:        BuildLogoCommList();
                                          marks = logoCommBreakMap;
                                          break;

            case COMM_DETECT_ALL:         BuildAllMethodsCommList();
                                          marks = commBreakMap;
                                          break;
    }

    VERBOSE(VB_COMMFLAG, "Final Commercial Break Map" );
}

void ClassicCommDetector::recordingFinished(long long totalFileSize)
{
    (void)totalFileSize;

    stillRecording = false;
}

void ClassicCommDetector::requestCommBreakMapUpdate(void)
{
    commBreakMapUpdateRequested = true;
    sendCommBreakMapUpdates = true;
}

void ClassicCommDetector::SetVideoParams(float aspect)
{
    int newAspect = COMM_ASPECT_WIDE;

    VERBOSE(VB_COMMFLAG, QString("CommDetect::SetVideoParams called with "
                                 "aspect = %1").arg(aspect));
    // Default to Widescreen but use the same check as VideoOutput::MoveResize()
    // to determine if is normal 4:3 aspect
    if(fabs(aspect - 1.333333) < 0.1)
        newAspect = COMM_ASPECT_NORMAL;

    if (newAspect != currentAspect)
    {
        VERBOSE(VB_COMMFLAG,
                QString("Aspect Ratio changed from %1 to %2 at frame %3")
                .arg(currentAspect).arg(newAspect)
                .arg((long)curFrameNumber));

        if (frameInfo.contains(curFrameNumber))
        {
            // pretend that this frame is blank so that we can create test
            // blocks on real aspect ratio change boundaries.
            frameInfo[curFrameNumber].flagMask |= COMM_FRAME_BLANK;
            frameInfo[curFrameNumber].flagMask |= COMM_FRAME_ASPECT_CHANGE;
            decoderFoundAspectChanges = true;
        }
        else if (curFrameNumber != -1)
        {
            VERBOSE(VB_COMMFLAG, QString("Unable to keep track of Aspect ratio "
                                         "change because frameInfo for frame "
                                         "number %1 does not exist.")
                    .arg((long)curFrameNumber));
        }
        currentAspect = newAspect;
    }
}

void ClassicCommDetector::ProcessFrame(VideoFrame *frame,
                                       long long frame_number)
{
    int max = 0;
    int min = 255;
    int avg = 0;
    unsigned char pixel;
    int blankPixelsChecked = 0;
    long long totBrightness = 0;
    unsigned char rowMax[height];
    unsigned char colMax[width];
    int topDarkRow = commDetectBorder;
    int bottomDarkRow = height - commDetectBorder - 1;
    int leftDarkCol = commDetectBorder;
    int rightDarkCol = width - commDetectBorder - 1;
    int flagMask = 0;
    int *curHistogram = histogram;
    int sceneChangePercent = -1;
    FrameInfoEntry fInfo;

    if (!frame || frame_number == -1 || frame->codec != FMT_YV12)
    {
        VERBOSE(VB_COMMFLAG, "CommDetect: Invalid video frame or codec, "
                "unable to process frame.");
        return;
    }

    if (!width || !height)
    {
        VERBOSE(VB_COMMFLAG, "CommDetect: Width or Height is 0, "
                "unable to process frame.");
        return;
    }

    curFrameNumber = frame_number;
    framePtr = frame->buf;

    fInfo.minBrightness = -1;
    fInfo.maxBrightness = -1;
    fInfo.avgBrightness = -1;
    fInfo.sceneChangePercent = -1;
    fInfo.aspect = currentAspect;
    fInfo.format = COMM_FORMAT_NORMAL;
    fInfo.flagMask = 0;

    frameInfo[curFrameNumber] = fInfo;

    if (commDetectMethod & COMM_DETECT_BLANKS)
    {
        memset(&rowMax, 0, sizeof(rowMax));
        memset(&colMax, 0, sizeof(colMax));

        frameIsBlank = false;
    }

    if (commDetectMethod & COMM_DETECT_SCENE)
    {
        lastFrameWasSceneChange = sceneHasChanged;
        sceneHasChanged = false;

        if (lastHistogram[0] == -1)
            curHistogram = lastHistogram;
        else
            memcpy(lastHistogram, histogram, sizeof(histogram));

        memset(curHistogram, 0, sizeof(histogram));
    }

    stationLogoPresent = false;

    for(int y = commDetectBorder; y < (height - commDetectBorder);
            y += vertSpacing)
    {
        for(int x = commDetectBorder; x < (width - commDetectBorder);
                x += horizSpacing)
        {
            pixel = framePtr[y * width + x];

            if ((commDetectMethod & COMM_DETECT_BLANKS) &&
                ((!commDetectBlankCanHaveLogo) ||
                 (!logoInfoAvailable) ||
                 ((logoInfoAvailable) &&
                  ((y < logoMinY) || (y > logoMaxY) ||
                   (x < logoMinX) || (x > logoMaxX)))))
            {
                blankPixelsChecked++;
                totBrightness += pixel;

                if (pixel < min)
                    min = pixel;

                if (pixel > max)
                    max = pixel;

                if (pixel > rowMax[y])
                    rowMax[y] = pixel;

                if (pixel > colMax[x])
                    colMax[x] = pixel;
            }

            if (commDetectMethod & COMM_DETECT_SCENE)
                curHistogram[pixel]++;
        }
    }

    if (commDetectMethod & COMM_DETECT_BLANKS)
    {
        for(int y = commDetectBorder; y < (height - commDetectBorder);
                y += vertSpacing)
        {
            if (rowMax[y] > commDetectBoxBrightness)
                break;
            else
                topDarkRow = y;
        }

        for(int y = commDetectBorder; y < (height - commDetectBorder);
                y += vertSpacing)
            if (rowMax[y] >= commDetectBoxBrightness)
                bottomDarkRow = y;

        for(int x = commDetectBorder; x < (width - commDetectBorder);
                x += horizSpacing)
        {
            if (colMax[x] > commDetectBoxBrightness)
                break;
            else
                leftDarkCol = x;
        }

        for(int x = commDetectBorder; x < (width - commDetectBorder);
                x += horizSpacing)
            if (colMax[x] >= commDetectBoxBrightness)
                rightDarkCol = x;

        if ((topDarkRow > commDetectBorder) &&
            (topDarkRow < (height * .20)) &&
            (bottomDarkRow < (height - commDetectBorder)) &&
            (bottomDarkRow > (height * .80)))
        {
            frameInfo[curFrameNumber].format = COMM_FORMAT_LETTERBOX;
        }
        else if ((leftDarkCol > commDetectBorder) &&
                 (leftDarkCol < (width * .20)) &&
                 (rightDarkCol < (width - commDetectBorder)) &&
                 (rightDarkCol > (width * .80)))
        {
            frameInfo[curFrameNumber].format = COMM_FORMAT_PILLARBOX;
        }
        else
        {
            frameInfo[curFrameNumber].format = COMM_FORMAT_NORMAL;
        }

        avg = totBrightness / blankPixelsChecked;

        frameInfo[curFrameNumber].minBrightness = min;
        frameInfo[curFrameNumber].maxBrightness = max;
        frameInfo[curFrameNumber].avgBrightness = avg;

        totalMinBrightness += min;
        commDetectDimAverage = min + 10;

        // Is the frame really dark
        if (((max - min) <= commDetectBlankFrameMaxDiff) &&
            (max < commDetectDimBrightness))
            frameIsBlank = true;

        // Are we non-strict and the frame is blank
        if ((!aggressiveDetection) &&
            ((max - min) <= commDetectBlankFrameMaxDiff))
            frameIsBlank = true;

        // Are we non-strict and the frame is dark
        //                   OR the frame is dim and has a low avg brightness
        if ((!aggressiveDetection) &&
            ((max < commDetectDarkBrightness) ||
             ((max < commDetectDimBrightness) && (avg < commDetectDimAverage))))
            frameIsBlank = true;
    }

    if (commDetectMethod & COMM_DETECT_SCENE)
    {
        if (lastFrameWasSceneChange)
        {
            memcpy(lastHistogram, histogram, sizeof(histogram));
            frameInfo[curFrameNumber].sceneChangePercent = 0;
        }
        else if (curHistogram != lastHistogram)
        {
            long similar = 0;

            for(int i = 0; i < 256; i++)
            {
                if (histogram[i] < lastHistogram[i])
                    similar += histogram[i];
                else
                    similar += lastHistogram[i];
            }

            sceneChangePercent = (int)(100.0 * similar /
                                       ((width - (commDetectBorder * 2)) *
                                        (height - (commDetectBorder * 2)) /
                                        (vertSpacing * horizSpacing)));

            frameInfo[curFrameNumber].sceneChangePercent = sceneChangePercent;

            if (sceneChangePercent < 85)
            {
                memcpy(lastHistogram, histogram, sizeof(histogram));
                sceneHasChanged = true;
            }
        }
    }

    if ((logoInfoAvailable) && (commDetectMethod & COMM_DETECT_LOGO))
    {
        stationLogoPresent = CheckEdgeLogo();
    }

#if 0
    if ((commDetectMethod == COMM_DETECT_ALL) &&
        (CheckRatingSymbol()))
    {
        flagMask |= COMM_FRAME_RATING_SYMBOL;
    }
#endif

    if (frameIsBlank)
    {
        blankFrameMap[curFrameNumber] = MARK_BLANK_FRAME;
        flagMask |= COMM_FRAME_BLANK;
        blankFrameCount++;
    }

    if (sceneHasChanged)
    {
        sceneMap[curFrameNumber] = MARK_SCENE_CHANGE;
        flagMask |= COMM_FRAME_SCENE_CHANGE;
    }

    if (stationLogoPresent)
        flagMask |= COMM_FRAME_LOGO_PRESENT;

    frameInfo[curFrameNumber].flagMask = flagMask;

    if (verboseDebugging)
        VERBOSE(VB_COMMFLAG,
                QString().sprintf("Frame: %6ld -> %3d %3d %3d %3d %1d %1d %04x",
                                  (long)curFrameNumber,
                                  frameInfo[curFrameNumber].minBrightness,
                                  frameInfo[curFrameNumber].maxBrightness,
                                  frameInfo[curFrameNumber].avgBrightness,
                                  frameInfo[curFrameNumber].sceneChangePercent,
                                  frameInfo[curFrameNumber].format,
                                  frameInfo[curFrameNumber].aspect,
                                  frameInfo[curFrameNumber].flagMask ));

#ifdef SHOW_DEBUG_WIN
    comm_debug_show(frame->buf);
    getchar();
#endif

    framesProcessed++;
}

void ClassicCommDetector::ClearAllMaps(void)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::ClearAllMaps()");

    frameInfo.clear();
    blankFrameMap.clear();
    blankCommMap.clear();
    blankCommBreakMap.clear();
    sceneMap.clear();
    sceneCommBreakMap.clear();
    commBreakMap.clear();
}

void ClassicCommDetector::GetBlankCommMap(QMap<long long, int> &comms)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetBlankCommMap()");

    if (blankCommMap.isEmpty())
        BuildBlankFrameCommList();

    comms = blankCommMap;
}

void ClassicCommDetector::GetBlankCommBreakMap(QMap<long long, int> &comms)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetBlankCommBreakMap()");

    if (blankCommBreakMap.isEmpty())
        BuildBlankFrameCommList();

    comms = blankCommBreakMap;
}

void ClassicCommDetector::GetSceneChangeMap(QMap<long long, int> &scenes,
        long long start_frame)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetSceneChangeMap()");

    QMap<long long, int>::Iterator it;

    if (start_frame == -1)
        scenes.clear();

    for (it = sceneMap.begin(); it != sceneMap.end(); ++it)
        if ((start_frame == -1) || (it.key() >= start_frame))
            scenes[it.key()] = it.data();
}

void ClassicCommDetector::BuildMasterCommList(void)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::BuildMasterCommList()");

    if (blankCommBreakMap.size())
    {
        QMap<long long, int>::Iterator it;

        for(it = blankCommBreakMap.begin(); it != blankCommBreakMap.end(); ++it)
            commBreakMap[it.key()] = it.data();
    }

    if ((blankCommBreakMap.size() > 1) &&
        (sceneCommBreakMap.size() > 1))
    {
        // see if beginning of the recording looks like a commercial
        QMap<long long, int>::Iterator it_a;
        QMap<long long, int>::Iterator it_b;

        it_a = blankCommBreakMap.begin();
        it_b = sceneCommBreakMap.begin();

        if ((it_b.key() < 2) &&
            (it_a.key() > 2))
        {
            commBreakMap.erase(it_a.key());
            commBreakMap[0] = MARK_COMM_START;
        }


        // see if ending of recording looks like a commercial
        QMap<long long, int>::Iterator it;
        long long max_blank = 0;
        long long max_scene = 0;

        it = blankCommBreakMap.begin();
        for(unsigned int i = 0; i < blankCommBreakMap.size(); i++)
            if ((it.data() == MARK_COMM_END) &&
                (it.key() > max_blank))
                max_blank = it.key();

        it = sceneCommBreakMap.begin();
        for(unsigned int i = 0; i < sceneCommBreakMap.size(); i++)
            if ((it.data() == MARK_COMM_END) &&
                (it.key() > max_scene))
                max_scene = it.key();

        if ((max_blank < (framesProcessed - 2)) &&
            (max_scene > (framesProcessed - 2)))
        {
            commBreakMap.erase(max_blank);
            commBreakMap[framesProcessed] = MARK_COMM_END;
        }
    }

    if ((blankCommBreakMap.size() > 3) &&
        (sceneCommBreakMap.size() > 1))
    {
        QMap<long long, int>::Iterator it_a;
        QMap<long long, int>::Iterator it_b;
        long long b_start, b_end;
        long long s_start, s_end;

        b_start = b_end = -1;
        s_start = s_end = -1;

        it_a = blankCommBreakMap.begin();
        it_a++;
        it_b = it_a;
        it_b++;
        while(it_b != blankCommBreakMap.end())
        {
            long long fdiff = it_b.key() - it_a.key();
            bool allTrue = false;

            if (fdiff < (62 * fps))
            {
                long long f = it_a.key() + 1;

                allTrue = true;

                while ((f <= framesProcessed) && (f < it_b.key()) && (allTrue))
                    allTrue = FrameIsInBreakMap(f++, sceneCommBreakMap);
            }

            if (allTrue)
            {
                commBreakMap.erase(it_a.key());
                commBreakMap.erase(it_b.key());
            }

            it_a++; it_a++;
            it_b++;
            if (it_b != blankCommBreakMap.end())
                it_b++;
        }
    }
}

void ClassicCommDetector::UpdateFrameBlock(FrameBlock *fbp,
                                           FrameInfoEntry finfo,
                                           int format, int aspect)
{
    int value = 0;

    value = finfo.flagMask;

    if (value & COMM_FRAME_LOGO_PRESENT)
        fbp->logoCount++;

    if (value & COMM_FRAME_RATING_SYMBOL)
        fbp->ratingCount++;

    if (value & COMM_FRAME_SCENE_CHANGE)
        fbp->scCount++;

    if (finfo.format == format)
        fbp->formatMatch++;

    if (finfo.aspect == aspect)
        fbp->aspectMatch++;
}


void ClassicCommDetector::BuildAllMethodsCommList(void)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::BuildAllMethodsCommList()");

    FrameBlock *fblock;
    FrameBlock *fbp;
    int value = 0;
    int curBlock = 0;
    int maxBlock = 0;
    int lastScore = 0;
    int thisScore = 0;
    int nextScore = 0;
    long curFrame = 0;
    long breakStart = 0;
    long lastStart = 0;
    long lastEnd = 0;
    long firstLogoFrame = -1;
    bool nextFrameIsBlank = false;
    bool lastFrameWasBlank = false;
    long formatFrames = 0;
    int format = COMM_FORMAT_NORMAL;
    long aspectFrames = 0;
    int aspect = COMM_ASPECT_NORMAL;
    QString msg;
    long long formatCounts[COMM_FORMAT_MAX];
    QMap<long long, int> tmpCommMap;
    QMap<long long, int>::Iterator it;

    commBreakMap.clear();

    fblock = new FrameBlock[blankFrameCount + 2];

    curBlock = 0;
    curFrame = 1;

    fbp = &fblock[curBlock];
    fbp->start = 0;
    fbp->bfCount = 0;
    fbp->logoCount = 0;
    fbp->ratingCount = 0;
    fbp->scCount = 0;
    fbp->scRate = 0.0;
    fbp->formatMatch = 0;
    fbp->aspectMatch = 0;
    fbp->score = 0;

    lastFrameWasBlank = true;

    if (decoderFoundAspectChanges)
    {
        for(long long i = 0; i < framesProcessed; i++ )
        {
            if (frameInfo[i].aspect == COMM_ASPECT_NORMAL)
                aspectFrames++;
        }

        if (aspectFrames < (framesProcessed / 2))
        {
            aspect = COMM_ASPECT_WIDE;
            aspectFrames = framesProcessed - aspectFrames;
        }
    }
    else
    {
        for (int i=0;i<COMM_FORMAT_MAX;i++) formatCounts[i]=0;

        for(long long i = 0; i < framesProcessed; i++ )
            formatCounts[frameInfo[i].aspect]++;

        for(int i = 0; i < COMM_FORMAT_MAX; i++)
        {
            if (formatCounts[i] > formatFrames)
            {
                format = i;
                formatFrames = formatCounts[i];
            }
        }
    }

    while (curFrame <= framesProcessed)
    {
        value = frameInfo[curFrame].flagMask;

        if (((curFrame + 1) <= framesProcessed) &&
            (frameInfo[curFrame + 1].flagMask & COMM_FRAME_BLANK))
            nextFrameIsBlank = true;
        else
            nextFrameIsBlank = false;

        if (value & COMM_FRAME_BLANK)
        {
            fbp->bfCount++;

            if (!nextFrameIsBlank || !lastFrameWasBlank)
            {
                UpdateFrameBlock(fbp, frameInfo[curFrame], format, aspect);

                fbp->end = curFrame;
                fbp->frames = fbp->end - fbp->start + 1;
                fbp->length = fbp->frames / fps;

                if ((fbp->scCount) && (fbp->length > 1.05))
                    fbp->scRate = fbp->scCount / fbp->length;

                curBlock++;

                fbp = &fblock[curBlock];
                fbp->bfCount = 1;
                fbp->logoCount = 0;
                fbp->ratingCount = 0;
                fbp->scCount = 0;
                fbp->scRate = 0.0;
                fbp->score = 0;
                fbp->formatMatch = 0;
                fbp->aspectMatch = 0;
                fbp->start = curFrame;
            }

            lastFrameWasBlank = true;
        }
        else
        {
            lastFrameWasBlank = false;
        }

        UpdateFrameBlock(fbp, frameInfo[curFrame], format, aspect);

        if ((value & COMM_FRAME_LOGO_PRESENT) &&
            (firstLogoFrame == -1))
            firstLogoFrame = curFrame;

        curFrame++;
    }

    fbp->end = curFrame;
    fbp->frames = fbp->end - fbp->start + 1;
    fbp->length = fbp->frames / fps;

    if ((fbp->scCount) && (fbp->length > 1.05))
        fbp->scRate = fbp->scCount / fbp->length;

    maxBlock = curBlock;
    curBlock = 0;
    lastScore = 0;

    VERBOSE(VB_COMMFLAG, "Initial Block pass");
    VERBOSE(VB_COMMFLAG, "Block StTime StFrm  EndFrm Frames Secs    "
            "Bf  Lg Cnt RT Cnt SC Cnt SC Rt FmtMch AspMch Score");
    VERBOSE(VB_COMMFLAG, "----- ------ ------ ------ ------ ------- "
            "--- ------ ------ ------ ----- ------ ------ -----");
    while (curBlock <= maxBlock)
    {
        fbp = &fblock[curBlock];

        msg.sprintf("%5d %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d "
                    "%5.2f %6d %6d %5d",
                    curBlock, (int)(fbp->start / fps) / 60,
                    (int)((fbp->start / fps )) % 60,
                    fbp->start, fbp->end, fbp->frames, fbp->length,
                    fbp->bfCount, fbp->logoCount, fbp->ratingCount,
                    fbp->scCount, fbp->scRate, fbp->formatMatch,
                    fbp->aspectMatch, fbp->score);
        VERBOSE(VB_COMMFLAG, msg);

        if (fbp->frames > fps)
        {
            if (verboseDebugging)
                VERBOSE(VB_COMMFLAG, QString("      FRAMES > %1").arg(fps));

            if (fbp->length > commDetectMaxCommLength)
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      length > max comm length, +20");
                fbp->score += 20;
            }

            if (fbp->length > commDetectMaxCommBreakLength)
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      length > max comm break length,"
                            " +20");
                fbp->score += 20;
            }

            if ((fbp->length > 4) &&
                (fbp->logoCount > (fbp->frames * 0.60)) &&
                (fbp->bfCount < (fbp->frames * 0.10)))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      length > 4 && logoCount > "
                            "frames * 0.60 && bfCount < frames "
                            "* .10");
                if (fbp->length > commDetectMaxCommBreakLength)
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      length > "
                                "max comm break length, +20");
                    fbp->score += 20;
                }
                else
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      length <= "
                                "max comm break length, +10");
                    fbp->score += 10;
                }
            }

            if ((logoInfoAvailable) &&
                (fbp->logoCount < (fbp->frames * 0.50)))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      logoInfoAvailable && logoCount"
                            " < frames * .50, -10");
                fbp->score -= 10;
            }

            if (fbp->ratingCount > (fbp->frames * 0.05))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      rating symbol present > 5% "
                            "of time, +20");
                fbp->score += 20;
            }

            if ((fbp->scRate > 1.0) &&
                (fbp->logoCount < (fbp->frames * .90)))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      scRate > 1.0, -10");
                fbp->score -= 10;

                if (fbp->scRate > 2.0)
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      scRate > 2.0, -10");
                    fbp->score -= 10;
                }
            }

            if ((abs((int)(fbp->frames - (15 * fps))) < 5 ) ||
                (abs((int)(fbp->frames - (30 * fps))) < 6 ) ||
                (abs((int)(fbp->frames - (60 * fps))) < 8 ))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      block appears to be standard "
                            "comm length, -10");
                fbp->score -= 10;
            }
        }
        else
        {
            if (verboseDebugging)
                VERBOSE(VB_COMMFLAG, QString("      FRAMES <= %1").arg(fps));

            if ((logoInfoAvailable) &&
                (fbp->start >= firstLogoFrame) &&
                (fbp->logoCount == 0))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      logoInfoAvailable && logoCount"
                            " == 0, -10");
                fbp->score -= 10;
            }

            if (fbp->ratingCount > (fbp->frames * 0.25))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      rating symbol present > 25% "
                            "of time, +10");
                fbp->score += 10;
            }
        }

        if (decoderFoundAspectChanges)
        {
            if (fbp->aspectMatch > (fbp->frames * .90))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      > 90% of frames match show "
                            "aspect, +10");
                fbp->score += 10;

                if (fbp->aspectMatch > (fbp->frames * .95))
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      > 95% of frames match show "
                                "aspect, +10");
                    fbp->score += 10;
                }
            }
            else if (fbp->aspectMatch < (fbp->frames * .10))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      < 10% of frames match show "
                            "aspect, -10");
                fbp->score -= 10;

                if (fbp->aspectMatch < (fbp->frames * .05))
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      < 5% of frames match show "
                                "aspect, -10");
                    fbp->score -= 10;
                }
            }
        }
        else if (format != COMM_FORMAT_NORMAL)
        {
            if (fbp->formatMatch > (fbp->frames * .90))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      > 90% of frames match show "
                            "letter/pillar-box format, +10");
                fbp->score += 10;

                if (fbp->formatMatch > (fbp->frames * .95))
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      > 95% of frames match show "
                                "letter/pillar-box format, +10");
                    fbp->score += 10;
                }
            }
            else if (fbp->formatMatch < (fbp->frames * .10))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      < 10% of frames match show "
                            "letter/pillar-box format, -10");
                fbp->score -= 10;

                if (fbp->formatMatch < (fbp->frames * .05))
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      < 5% of frames match show "
                                "letter/pillar-box format, -10");
                    fbp->score -= 10;
                }
            }
        }

        msg.sprintf("  NOW %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d "
                    "%5.2f %6d %6d %5d",
                    (int)(fbp->start / fps) / 60,
                    (int)((fbp->start / fps )) % 60,
                    fbp->start, fbp->end, fbp->frames, fbp->length,
                    fbp->bfCount, fbp->logoCount, fbp->ratingCount,
                    fbp->scCount, fbp->scRate, fbp->formatMatch,
                    fbp->aspectMatch, fbp->score);
        VERBOSE(VB_COMMFLAG, msg);

        lastScore = fbp->score;
        curBlock++;
    }

    curBlock = 0;
    lastScore = 0;

    VERBOSE(VB_COMMFLAG, "============================================");
    VERBOSE(VB_COMMFLAG, "Second Block pass");
    VERBOSE(VB_COMMFLAG, "Block StTime StFrm  EndFrm Frames Secs    "
            "Bf  Lg Cnt RT Cnt SC Cnt SC Rt FmtMch AspMch Score");
    VERBOSE(VB_COMMFLAG, "----- ------ ------ ------ ------ ------- "
            "--- ------ ------ ------ ----- ------ ------ -----");
    while (curBlock <= maxBlock)
    {
        fbp = &fblock[curBlock];

        msg.sprintf("%5d %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d "
                    "%5.2f %6d %6d %5d",
                    curBlock, (int)(fbp->start / fps) / 60,
                    (int)((fbp->start / fps )) % 60,
                    fbp->start, fbp->end, fbp->frames, fbp->length,
                    fbp->bfCount, fbp->logoCount, fbp->ratingCount,
                    fbp->scCount, fbp->scRate, fbp->formatMatch,
                    fbp->aspectMatch, fbp->score);
        VERBOSE(VB_COMMFLAG, msg);

        if ((curBlock > 0) && (curBlock < maxBlock))
        {
            nextScore = fblock[curBlock + 1].score;

            if ((lastScore < 0) && (nextScore < 0) && (fbp->length < 35))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      lastScore < 0 && nextScore < 0 "
                            "&& length < 35, setting -10");
                fbp->score -= 10;
            }

            if ((fbp->bfCount > (fbp->frames * 0.95)) &&
                (fbp->frames < (2*fps)) &&
                (lastScore < 0 && nextScore < 0))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      blanks > frames * 0.95 && "
                            "frames < 2*fps && lastScore < 0 && "
                            "nextScore < 0, setting -10");
                fbp->score -= 10;
            }

            if ((fbp->frames < (120*fps)) &&
                (lastScore < 0) &&
                (fbp->score > 0) &&
                (fbp->score < 20) &&
                (nextScore < 0))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      frames < 120 * fps && (-20 < "
                            "lastScore < 0) && thisScore > 0 && "
                            "nextScore < 0, setting score = -10");
                fbp->score = -10;
            }

            if ((fbp->frames < (30*fps)) &&
                (lastScore > 0) &&
                (fbp->score < 0) &&
                (fbp->score > -20) &&
                (nextScore > 0))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      frames < 30 * fps && (0 < "
                            "lastScore < 20) && thisScore < 0 && "
                            "nextScore > 0, setting score = 10");
                fbp->score = 10;
            }
        }

        if ((fbp->score == 0) && (lastScore > 30))
        {
            int offset = 1;
            while(((curBlock + offset) <= maxBlock) &&
                    (fblock[curBlock + offset].frames < (2 * fps)) &&
                    (fblock[curBlock + offset].score == 0))
                offset++;

            if ((curBlock + offset) <= maxBlock)
            {
                offset--;
                if (fblock[curBlock + offset + 1].score > 0)
                {
                    for (; offset >= 0; offset--)
                    {
                        fblock[curBlock + offset].score += 10;
                        if (verboseDebugging)
                            VERBOSE(VB_COMMFLAG, QString("      Setting block "
                                                         "%1 score +10")
                                    .arg(curBlock+offset));
                    }
                }
                else if (fblock[curBlock + offset + 1].score < 0)
                {
                    for (; offset >= 0; offset--)
                    {
                        fblock[curBlock + offset].score -= 10;
                        if (verboseDebugging)
                            VERBOSE(VB_COMMFLAG, QString("      Setting block "
                                                         "%1 score -10")
                                    .arg(curBlock+offset));
                    }
                }
            }
        }

        msg.sprintf("  NOW %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d "
                    "%5.2f %6d %6d %5d",
                    (int)(fbp->start / fps) / 60,
                    (int)((fbp->start / fps )) % 60,
                    fbp->start, fbp->end, fbp->frames, fbp->length,
                    fbp->bfCount, fbp->logoCount, fbp->ratingCount,
                    fbp->scCount, fbp->scRate, fbp->formatMatch,
                    fbp->aspectMatch, fbp->score);
        VERBOSE(VB_COMMFLAG, msg);

        lastScore = fbp->score;
        curBlock++;
    }

    VERBOSE(VB_COMMFLAG, "============================================");
    VERBOSE(VB_COMMFLAG, "FINAL Block stats");
    VERBOSE(VB_COMMFLAG, "Block StTime StFrm  EndFrm Frames Secs    "
            "Bf  Lg Cnt RT Cnt SC Cnt SC Rt FmtMch AspMch Score");
    VERBOSE(VB_COMMFLAG, "----- ------ ------ ------ ------ ------- "
            "--- ------ ------ ------ ----- ------ ------ -----");
    curBlock = 0;
    lastScore = 0;
    breakStart = -1;
    while (curBlock <= maxBlock)
    {
        fbp = &fblock[curBlock];
        thisScore = fbp->score;

        if ((breakStart >= 0) &&
            ((fbp->end - breakStart) > (commDetectMaxCommBreakLength * fps)))
        {
            if (((fbp->start - breakStart) >
                (commDetectMinCommBreakLength * fps)) ||
                (breakStart == 0))
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG,
                            QString("Closing commercial block at start of "
                                    "frame block %1 with length %2, frame "
                                    "block length of %3 frames would put comm "
                                    "block length over max of %4 seconds.")
                            .arg(curBlock).arg(fbp->start - breakStart)
                            .arg(fbp->frames)
                            .arg(commDetectMaxCommBreakLength));

                commBreakMap[breakStart] = MARK_COMM_START;
                commBreakMap[fbp->start] = MARK_COMM_END;
                lastStart = breakStart;
                lastEnd = fbp->start;
                breakStart = -1;
            }
            else
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG,
                            QString("Ignoring what appears to be commercial"
                                    " block at frame %1 with length %2, "
                                    "length of %3 frames would put comm "
                                    "block length under min of %4 seconds.")
                            .arg((long)breakStart)
                            .arg(fbp->start - breakStart)
                            .arg(fbp->frames)
                            .arg(commDetectMinCommBreakLength));
                breakStart = -1;
            }
        }
        if (thisScore == 0)
        {
            thisScore = lastScore;
        }
        else if (thisScore < 0)
        {
            if ((lastScore > 0) || (curBlock == 0))
            {
                if ((fbp->start - lastEnd) < (commDetectMinShowLength * fps))
                {
                    commBreakMap.erase(lastStart);
                    commBreakMap.erase(lastEnd);
                    breakStart = lastStart;

                    if (verboseDebugging)
                    {
                        if (breakStart)
                            VERBOSE(VB_COMMFLAG,
                                    QString("ReOpening commercial block at "
                                            "frame %1 because show less than "
                                            "%2 seconds")
                                    .arg(breakStart)
                                    .arg(commDetectMinShowLength));
                        else
                            VERBOSE(VB_COMMFLAG,
                                    QString("Opening initial commercial block "
                                            "at start of recording, block 0."));
                    }
                }
                else
                {
                    breakStart = fbp->start;

                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG,
                                QString("Starting new commercial block at "
                                        "frame %1 from start of frame block %2")
                                .arg(fbp->start).arg(curBlock));
                }
            }
            else if (curBlock == maxBlock)
            {
                if ((fbp->end - breakStart) >
                    (commDetectMinCommBreakLength * fps))
                {
                    if (fbp->end <= (framesProcessed - (int)(2 * fps) - 2))
                    {
                        if (verboseDebugging)
                            VERBOSE(VB_COMMFLAG,
                                    QString("Closing final commercial block at "
                                            "frame %1").arg(fbp->end));

                        commBreakMap[breakStart] = MARK_COMM_START;
                        commBreakMap[fbp->end] = MARK_COMM_END;
                        lastStart = breakStart;
                        lastEnd = fbp->end;
                        breakStart = -1;
                    }
                }
                else
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG,
                                QString("Ignoring what appears to be commercial"
                                        " block at frame %1 with length %2, "
                                        "length of %3 frames would put comm "
                                        "block length under min of %4 seconds.")
                                .arg((long)breakStart)
                                .arg(fbp->start - breakStart)
                                .arg(fbp->frames)
                                .arg(commDetectMinCommBreakLength));
                    breakStart = -1;
                }
            }
        }
        else if ((thisScore > 0) &&
                 (lastScore < 0) &&
                 (breakStart != -1))
        {
            if (((fbp->start - breakStart) >
                (commDetectMinCommBreakLength * fps)) ||
                (breakStart == 0))
            {
                commBreakMap[breakStart] = MARK_COMM_START;
                commBreakMap[fbp->start] = MARK_COMM_END;
                lastStart = breakStart;
                lastEnd = fbp->start;

                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG,
                            QString("Closing commercial block at frame %1")
                            .arg(fbp->start));
            }
            else
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG,
                            QString("Ignoring what appears to be commercial "
                                    "block at frame %1 with length %2, "
                                    "length of %3 frames would put comm block "
                                    "length under min of %4 seconds.")
                            .arg((long)breakStart)
                            .arg(fbp->start - breakStart)
                            .arg(fbp->frames)
                            .arg(commDetectMinCommBreakLength));
            }
            breakStart = -1;
        }

        msg.sprintf("%5d %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d "
                    "%5.2f %6d %6d %5d",
                    curBlock, (int)(fbp->start / fps) / 60,
                    (int)((fbp->start / fps )) % 60,
                    fbp->start, fbp->end, fbp->frames, fbp->length,
                    fbp->bfCount, fbp->logoCount, fbp->ratingCount,
                    fbp->scCount, fbp->scRate, fbp->formatMatch,
                    fbp->aspectMatch, thisScore);
        VERBOSE(VB_COMMFLAG, msg);

        lastScore = thisScore;
        curBlock++;
    }

    if ((breakStart != -1) &&
        (breakStart <= (framesProcessed - (int)(2 * fps) - 2)))
    {
        if (verboseDebugging)
            VERBOSE(VB_COMMFLAG,
                    QString("Closing final commercial block started at "
                            "block %1 and going to end of program. length "
                            "is %2 frames")
                    .arg(curBlock)
                    .arg((long)(framesProcessed - breakStart - 1)));

        commBreakMap[breakStart] = MARK_COMM_START;
        commBreakMap[framesProcessed - (int)(2 * fps) - 2] = MARK_COMM_END;
    }

    // include/exclude blanks from comm breaks
    tmpCommMap = commBreakMap;
    commBreakMap.clear();

    if (verboseDebugging)
        VERBOSE(VB_COMMFLAG, "Adjusting start/end marks according to blanks.");
    for (it = tmpCommMap.begin(); it != tmpCommMap.end(); ++it)
    {
        if (it.data() == MARK_COMM_START)
        {
            lastStart = it.key();
            if (skipAllBlanks)
            {
                while ((lastStart > 0) &&
                        (frameInfo[lastStart - 1].flagMask & COMM_FRAME_BLANK))
                    lastStart--;
            }
            else
            {
                while ((lastStart < (framesProcessed - (2 * fps))) &&
                        (frameInfo[lastStart + 1].flagMask & COMM_FRAME_BLANK))
                    lastStart++;
            }

            if (verboseDebugging)
                VERBOSE(VB_COMMFLAG, QString("Start Mark: %1 -> %2")
                        .arg((long)it.key())
                        .arg((long)lastStart));

            commBreakMap[lastStart] = MARK_COMM_START;
        }
        else
        {
            lastEnd = it.key();
            if (skipAllBlanks)
            {
                while ((lastEnd < (framesProcessed - (2 * fps))) &&
                        (frameInfo[lastEnd + 1].flagMask & COMM_FRAME_BLANK))
                    lastEnd++;
            }
            else
            {
                while ((lastEnd > 0) &&
                        (frameInfo[lastEnd - 1].flagMask & COMM_FRAME_BLANK))
                    lastEnd--;
            }

            if (verboseDebugging)
                VERBOSE(VB_COMMFLAG, QString("End Mark  : %1 -> %2")
                        .arg((long)it.key())
                        .arg((long)lastEnd));

            commBreakMap[lastEnd] = MARK_COMM_END;
        }
    }

    delete [] fblock;
}


void ClassicCommDetector::BuildBlankFrameCommList(void)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::BuildBlankFrameCommList()");

    long long bframes[blankFrameMap.count()*2];
    long long c_start[blankFrameMap.count()];
    long long c_end[blankFrameMap.count()];
    int frames = 0;
    int commercials = 0;
    int i, x;
    QMap<long long, int>::Iterator it;

    blankCommMap.clear();

    for (it = blankFrameMap.begin(); it != blankFrameMap.end(); ++it)
        bframes[frames++] = it.key();

    if (frames == 0)
        return;

    // detect individual commercials from blank frames
    // commercial end is set to frame right before ending blank frame to
    //    account for instances with only a single blank frame between comms.
    for(i = 0; i < frames; i++ )
    {
        for(x=i+1; x < frames; x++ )
        {
            // check for various length spots since some channels don't
            // have blanks inbetween commercials just at the beginning and
            // end of breaks
            int gap_length = bframes[x] - bframes[i];
            if (((aggressiveDetection) &&
                ((abs((int)(gap_length - (5 * fps))) < 5 ) ||
                 (abs((int)(gap_length - (10 * fps))) < 7 ) ||
                 (abs((int)(gap_length - (15 * fps))) < 10 ) ||
                 (abs((int)(gap_length - (20 * fps))) < 11 ) ||
                 (abs((int)(gap_length - (30 * fps))) < 12 ) ||
                 (abs((int)(gap_length - (40 * fps))) < 1 ) ||
                 (abs((int)(gap_length - (45 * fps))) < 1 ) ||
                 (abs((int)(gap_length - (60 * fps))) < 15 ) ||
                 (abs((int)(gap_length - (90 * fps))) < 10 ) ||
                 (abs((int)(gap_length - (120 * fps))) < 10 ))) ||
                ((!aggressiveDetection) &&
                 ((abs((int)(gap_length - (5 * fps))) < 11 ) ||
                  (abs((int)(gap_length - (10 * fps))) < 13 ) ||
                  (abs((int)(gap_length - (15 * fps))) < 16 ) ||
                  (abs((int)(gap_length - (20 * fps))) < 17 ) ||
                  (abs((int)(gap_length - (30 * fps))) < 18 ) ||
                  (abs((int)(gap_length - (40 * fps))) < 3 ) ||
                  (abs((int)(gap_length - (45 * fps))) < 3 ) ||
                  (abs((int)(gap_length - (60 * fps))) < 20 ) ||
                  (abs((int)(gap_length - (90 * fps))) < 20 ) ||
                  (abs((int)(gap_length - (120 * fps))) < 20 ))))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x] - 1;
                commercials++;
                i = x-1;
                x = frames;
            }

            if ((!aggressiveDetection) &&
                ((abs((int)(gap_length - (30 * fps))) < (int)(fps * 0.85)) ||
                 (abs((int)(gap_length - (60 * fps))) < (int)(fps * 0.95)) ||
                 (abs((int)(gap_length - (90 * fps))) < (int)(fps * 1.05)) ||
                 (abs((int)(gap_length - (120 * fps))) < (int)(fps * 1.15))) &&
                ((x + 2) < frames) &&
                ((i + 2) < frames) &&
                ((bframes[i] + 1) == bframes[i+1]) &&
                ((bframes[x] + 1) == bframes[x+1]))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x];
                commercials++;
                i = x;
                x = frames;
            }
        }
    }

    i = 0;

    // don't allow single commercial at head
    // of show unless followed by another
    if ((commercials > 1) &&
        (c_end[0] < (33 * fps)) &&
        (c_start[1] > (c_end[0] + 40 * fps)))
        i = 1;

    // eliminate any blank frames at end of commercials
    bool first_comm = true;
    for(; i < (commercials-1); i++)
    {
        long long r = c_start[i];

        if ((r < (30 * fps)) &&
            (first_comm))
            r = 1;

        blankCommMap[r] = MARK_COMM_START;

        r = c_end[i];
        if ( i < (commercials-1))
        {
            for(x = 0; x < (frames-1); x++)
                if (bframes[x] == r)
                    break;
            while((x < (frames-1)) &&
                    ((bframes[x] + 1 ) == bframes[x+1]) &&
                    (bframes[x+1] < c_start[i+1]))
            {
                r++;
                x++;
            }

            if (skipAllBlanks)
                while((blankFrameMap.contains(r+1)) &&
                        (c_start[i+1] != (r+1)))
                    r++;
        }
        else
        {
            if (skipAllBlanks)
                while(blankFrameMap.contains(r+1))
                    r++;
        }

        blankCommMap[r] = MARK_COMM_END;
        first_comm = false;
    }

    blankCommMap[c_start[i]] = MARK_COMM_START;
    blankCommMap[c_end[i]] = MARK_COMM_END;

    VERBOSE(VB_COMMFLAG, "Blank-Frame Commercial Map" );
    for(it = blankCommMap.begin(); it != blankCommMap.end(); ++it)
        VERBOSE(VB_COMMFLAG, QString("    %1:%2")
                .arg((long int)it.key()).arg(it.data()));

    MergeBlankCommList();

    VERBOSE(VB_COMMFLAG, "Merged Blank-Frame Commercial Break Map" );
    for(it = blankCommBreakMap.begin(); it != blankCommBreakMap.end(); ++it)
        VERBOSE(VB_COMMFLAG, QString("    %1:%2")
                .arg((long int)it.key()).arg(it.data()));
}


void ClassicCommDetector::BuildSceneChangeCommList(void)
{
    int section_start = -1;
    int seconds = (int)(framesProcessed / fps);
    int sc_histogram[seconds+1];

    sceneCommBreakMap.clear();

    memset(sc_histogram, 0, sizeof(sc_histogram));
    for(long long f = 1; f <= framesProcessed; f++)
    {
        if (sceneMap.contains(f))
            sc_histogram[(int)(f / fps)]++;
    }

    for(long long s = 0; s < (seconds + 1); s++)
    {
        if (sc_histogram[s] > 2)
        {
            if (section_start == -1)
            {
                long long f = (long long)(s * fps);
                for(int i = 0; i < fps; i++, f++)
                {
                    if (sceneMap.contains(f))
                    {
                        sceneCommBreakMap[f] = MARK_COMM_START;
                        i = (int)(fps) + 1;
                    }
                }
            }

            section_start = s;
        }

        if ((section_start >= 0) &&
            (s > (section_start + 32)))
        {
            long long f = (long long)(section_start * fps);
            bool found_end = false;

            for(int i = 0; i < fps; i++, f++)
            {
                if (sceneMap.contains(f))
                {
                    if (sceneCommBreakMap.contains(f))
                        sceneCommBreakMap.erase(f);
                    else
                        sceneCommBreakMap[f] = MARK_COMM_END;
                    i = (int)(fps) + 1;
                    found_end = true;
                }
            }
            section_start = -1;

            if (!found_end)
            {
                f = (long long)(section_start * fps);
                sceneCommBreakMap[f] = MARK_COMM_END;
            }
        }
    }

    if (section_start >= 0)
        sceneCommBreakMap[framesProcessed] = MARK_COMM_END;

    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;
    QMap<long long, int> deleteMap;

    it = sceneCommBreakMap.begin();
    prev = it;
    if (it != sceneCommBreakMap.end())
    {
        it++;
        while (it != sceneCommBreakMap.end())
        {
            if ((it.data() == MARK_COMM_END) &&
                (it.key() - prev.key()) < (30 * fps))
            {
                deleteMap[it.key()] = 1;
                deleteMap[prev.key()] = 1;
            }
            prev++;
            if (it != sceneCommBreakMap.end())
                it++;
        }

        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
            sceneCommBreakMap.erase(it.key());
    }

    VERBOSE(VB_COMMFLAG, "Scene-Change Commercial Break Map" );
    for(it = sceneCommBreakMap.begin(); it != sceneCommBreakMap.end(); ++it)
        VERBOSE(VB_COMMFLAG, QString("    %1:%2")
                .arg((long int)it.key()).arg(it.data()));
}


void ClassicCommDetector::BuildLogoCommList()
{
    GetLogoCommBreakMap(logoCommBreakMap);
    CondenseMarkMap(logoCommBreakMap, (int)(25 * fps), (int)(30 * fps));
    ConvertShowMapToCommMap(logoCommBreakMap);

    QMap<long long, int>::Iterator it;
    VERBOSE(VB_COMMFLAG, "Logo Commercial Break Map" );
    for(it = logoCommBreakMap.begin(); it != logoCommBreakMap.end(); ++it)
        VERBOSE(VB_COMMFLAG, QString("    %1:%2")
                .arg((long int)it.key()).arg(it.data()));
}

void ClassicCommDetector::MergeBlankCommList(void)
{
    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;
    QMap<long long, long long> tmpMap;
    QMap<long long, long long>::Iterator tmpMap_it;
    QMap<long long, long long>::Iterator tmpMap_prev;

    blankCommBreakMap.clear();

    if (blankCommMap.isEmpty())
        return;

    for (it = blankCommMap.begin(); it != blankCommMap.end(); ++it)
        blankCommBreakMap[it.key()] = it.data();

    if (blankCommBreakMap.isEmpty())
        return;

    it = blankCommMap.begin();
    prev = it;
    it++;
    for(; it != blankCommMap.end(); ++it, ++prev)
    {
        // if next commercial starts less than 15*fps frames away then merge
        if ((((prev.key() + 1) == it.key()) ||
            ((prev.key() + (15 * fps)) > it.key())) &&
            (prev.data() == MARK_COMM_END) &&
            (it.data() == MARK_COMM_START))
        {
            blankCommBreakMap.erase(prev.key());
            blankCommBreakMap.erase(it.key());
        }
    }


    // make temp copy of commercial break list
    it = blankCommBreakMap.begin();
    prev = it;
    it++;
    tmpMap[prev.key()] = it.key();
    for(; it != blankCommBreakMap.end(); ++it, ++prev)
    {
        if ((prev.data() == MARK_COMM_START) &&
            (it.data() == MARK_COMM_END))
            tmpMap[prev.key()] = it.key();
    }

    tmpMap_it = tmpMap.begin();
    tmpMap_prev = tmpMap_it;
    tmpMap_it++;
    for(; tmpMap_it != tmpMap.end(); ++tmpMap_it, ++tmpMap_prev)
    {
        // if we find any segments less than 35 seconds between commercial
        // breaks include those segments in the commercial break.
        if (((tmpMap_prev.data() + (35 * fps)) > tmpMap_it.key()) &&
            ((tmpMap_prev.data() - tmpMap_prev.key()) > (35 * fps)) &&
            ((tmpMap_it.data() - tmpMap_it.key()) > (35 * fps)))
        {
            blankCommBreakMap.erase(tmpMap_prev.data());
            blankCommBreakMap.erase(tmpMap_it.key());
        }
    }
}

bool ClassicCommDetector::FrameIsInBreakMap(long long f,
                                            QMap<long long, int> &breakMap)
{
    for(long long i = f; i < framesProcessed; i++)
        if (breakMap.contains(i))
        {
            int type = breakMap[i];
            if ((type == MARK_COMM_END) || (i == f))
                return true;
            if (type == MARK_COMM_START)
                return false;
        }

    for(long long i = f; i >= 0; i--)
        if (breakMap.contains(i))
        {
            int type = breakMap[i];
            if ((type == MARK_COMM_START) || (i == f))
                return true;
            if (type == MARK_COMM_END)
                return false;
        }

    return false;
}

void ClassicCommDetector::DumpMap(QMap<long long, int> &map)
{
    QMap<long long, int>::Iterator it;
    QString msg;

    VERBOSE(VB_COMMFLAG, "---------------------------------------------------");
    for (it = map.begin(); it != map.end(); ++it)
    {
        long long frame = it.key();
        int flag = it.data();
        int my_fps = (int)ceil(fps);
        int hour = (frame / my_fps) / 60 / 60;
        int min = (frame / my_fps) / 60 - (hour * 60);
        int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
        int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                           (hour * 60 * 60 * my_fps));
        int my_sec = (int)(frame / my_fps);
        msg.sprintf("%7ld : %d (%02d:%02d:%02d.%02d) (%d)",
                    (long)frame, flag, hour, min, sec, frm, my_sec);
        VERBOSE(VB_COMMFLAG, msg);
    }
    VERBOSE(VB_COMMFLAG, "---------------------------------------------------");
}

void ClassicCommDetector::DumpLogo(bool fromCurrentFrame)
{
    char scrPixels[] = " .oxX";

    if (!logoInfoAvailable)
        return;

    cerr << "\nLogo Data ";
    if (fromCurrentFrame)
        cerr << "from current frame\n";

    cerr << "\n     ";

    for(int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        cerr << (x % 10);
    cerr << "\n";

    for(int y = logoMinY - 2; y <= (logoMaxY + 2); y++)
    {
        cerr << QString::number(y).rightJustify(3, ' ') << ": ";
        for(int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        {
            if (fromCurrentFrame)
            {
                cerr << scrPixels[framePtr[y * width + x] / 50];
            }
            else
            {
                switch (logoMask[y * width + x])
                {
                        case 0:
                        case 2: cerr << " ";
                        break;
                        case 1: cerr << "*";
                        break;
                        case 3: cerr << ".";
                        break;
                }
            }
        }
        cerr << "\n";
    }
    cerr.flush();
}

void ClassicCommDetector::SetLogoMaskArea()
{
    VERBOSE(VB_COMMFLAG, "SetLogoMaskArea()");

    logoMinX = width - 1;
    logoMaxX = 0;
    logoMinY = height - 1;
    logoMaxY = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (edgeMask[y * width + x].isedge)
            {
                if (x < logoMinX)
                    logoMinX = x;
                if (y < logoMinY)
                    logoMinY = y;
                if (x > logoMaxX)
                    logoMaxX = x;
                if (y > logoMaxY)
                    logoMaxY = y;
            }
        }
    }

    logoMinX -= 5;
    logoMaxX += 5;
    logoMinY -= 5;
    logoMaxY += 5;

    if (logoMinX < 4)
        logoMinX = 4;
    if (logoMaxX > (width-5))
        logoMaxX = (width-5);
    if (logoMinY < 4)
        logoMinY = 4;
    if (logoMaxY > (height-5))
        logoMaxY = (height-5);
}

void ClassicCommDetector::SetLogoMask(unsigned char *mask)
{
    int pixels = 0;

    memcpy(logoMask, mask, width * height);

    SetLogoMaskArea();

    for(int y = logoMinY; y <= logoMaxY; y++)
        for(int x = logoMinX; x <= logoMaxX; x++)
            if (!logoMask[y * width + x] == 1)
                pixels++;

    if (pixels < 30)
    {
        detectStationLogo = false;
        return;
    }

    // set the pixels around our logo
    for(int y = (logoMinY - 1); y <= (logoMaxY + 1); y++)
    {
        for(int x = (logoMinX - 1); x <= (logoMaxX + 1); x++)
        {
            if (!logoMask[y * width + x])
            {
                for (int y2 = y - 1; y2 <= (y + 1); y2++)
                {
                    for (int x2 = x - 1; x2 <= (x + 1); x2++)
                    {
                        if ((logoMask[y2 * width + x2] == 1) &&
                            (!logoMask[y * width + x]))
                        {
                            logoMask[y * width + x] = 2;
                            x2 = x + 2;
                            y2 = y + 2;

                            logoCheckMask[y2 * width + x2] = 1;
                            logoCheckMask[y * width + x] = 1;
                        }
                    }
                }
            }
        }
    }

    for(int y = (logoMinY - 2); y <= (logoMaxY + 2); y++)
    {
        for(int x = (logoMinX - 2); x <= (logoMaxX + 2); x++)
        {
            if (!logoMask[y * width + x])
            {
                for (int y2 = y - 1; y2 <= (y + 1); y2++)
                {
                    for (int x2 = x - 1; x2 <= (x + 1); x2++)
                    {
                        if ((logoMask[y2 * width + x2] == 2) &&
                            (!logoMask[y * width + x]))
                        {
                            logoMask[y * width + x] = 3;
                            x2 = x + 2;
                            y2 = y + 2;

                            logoCheckMask[y * width + x] = 1;
                        }
                    }
                }
            }
        }
    }

#ifdef SHOW_DEBUG_WIN
    DumpLogo();
#endif

    logoFrameCount = 0;
    logoInfoAvailable = true;
}

void ClassicCommDetector::CondenseMarkMap(QMap<long long, int>&map, int spacing,
                                          int length)
{
    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;
    QMap<long long, int>tmpMap;

    if (map.size() <= 2)
        return;

    // merge any segments less than 'spacing' frames apart from each other
    VERBOSE(VB_COMMFLAG, "Commercial Map Before condense:" );
    for (it = map.begin(); it != map.end(); it++)
    {
        VERBOSE(VB_COMMFLAG, QString("    %1:%2")
                .arg((long int)it.key()).arg(it.data()));
        tmpMap[it.key()] = it.data();
    }

    prev = tmpMap.begin();
    it = prev;
    it++;
    while(it != tmpMap.end())
    {
        if ((it.data() == MARK_START) &&
            (prev.data() == MARK_END) &&
            ((it.key() - prev.key()) < spacing))
        {
            map.erase(prev.key());
            map.erase(it.key());
        }
        prev++;
        it++;
    }

    if (map.size() == 0)
        return;

    // delete any segments less than 'length' frames in length
    tmpMap.clear();
    for (it = map.begin(); it != map.end(); it++)
        tmpMap[it.key()] = it.data();

    prev = tmpMap.begin();
    it = prev;
    it++;
    while(it != tmpMap.end())
    {
        if ((prev.data() == MARK_START) &&
            (it.data() == MARK_END) &&
            ((it.key() - prev.key()) < length))
        {
            map.erase(prev.key());
            map.erase(it.key());
        }
        prev++;
        it++;
    }

    VERBOSE(VB_COMMFLAG, "Commercial Map After condense:" );
    for (it = map.begin(); it != map.end(); it++)
        VERBOSE(VB_COMMFLAG, QString("    %1:%2")
                .arg((long int)it.key()).arg(it.data()));
}

void ClassicCommDetector::ConvertShowMapToCommMap(QMap<long long, int>&map)
{
    QMap<long long, int>::Iterator it;

    if (map.size() == 0)
        return;

    for (it = map.begin(); it != map.end(); it++)
    {
        if (it.data() == MARK_START)
            map[it.key()] = MARK_COMM_END;
        else
            map[it.key()] = MARK_COMM_START;
    }

    it = map.begin();
    if (it != map.end())
    {
        switch (map[it.key()])
        {
                case MARK_COMM_END:
                if (it.key() == 0)
                    map.erase(0);
                else
                    map[0] = MARK_COMM_START;
                break;
                case MARK_COMM_START:
                break;
                default:
                map.erase(0);
                break;
        }
    }
}


/* ideas for this method ported back from comskip.c mods by Jere Jones
 * which are partially mods based on Myth's original commercial skip
 * code written by Chris Pinkham. */
bool ClassicCommDetector::CheckEdgeLogo(void)
{

    int radius = 2;
    int x, y;
    int pos1, pos2, pos3;
    int pixel;
    int goodEdges = 0;
    int badEdges = 0;
    int testEdges = 0;
    int testNotEdges = 0;

    for (y = logoMinY; y <= logoMaxY; y++ )
    {
        for (x = logoMinX; x <= logoMaxX; x++ )
        {
            pos1 = y * width + x;
            pos2 = (y - radius) * width + x;
            pos3 = (y + radius) * width + x;

            pixel = framePtr[pos1];

            if (edgeMask[pos1].horiz)
            {
                if ((abs(framePtr[pos1 - radius] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos1 + radius] - pixel) >= logoEdgeDiff))
                    goodEdges++;
                testEdges++;
            }
            else
            {
                if ((abs(framePtr[pos1 - radius] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos1 + radius] - pixel) >= logoEdgeDiff))
                    badEdges++;
                testNotEdges++;
            }

            if (edgeMask[pos1].vert)
            {
                if ((abs(framePtr[pos2] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos3] - pixel) >= logoEdgeDiff))
                    goodEdges++;
                testEdges++;
            }
            else
            {
                if ((abs(framePtr[pos2] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos3] - pixel) >= logoEdgeDiff))
                    badEdges++;
                testNotEdges++;
            }
        }
    }

    double goodEdgeRatio = (double)goodEdges / (double)testEdges;
    double badEdgeRatio = (double)badEdges / (double)testNotEdges;

    if ((goodEdgeRatio > commDetectLogoGoodEdgeThreshold) &&
        (badEdgeRatio < commDetectLogoBadEdgeThreshold))
        return true;
    else
        return false;
}


void ClassicCommDetector::SearchForLogo()
{
    int seekIncrement = (int)(commDetectLogoSampleSpacing * fps);
    long long seekFrame;
    int loops;
    int maxLoops = commDetectLogoSamplesNeeded;
    EdgeMaskEntry *edgeCounts;
    int pos, i, x, y, dx, dy;
    int edgeDiffs[] = {5, 7, 10, 15, 20, 30, 40, 50, 60, 0 };


    VERBOSE(VB_COMMFLAG, "Searching for Station Logo");

    logoInfoAvailable = false; 

    edgeCounts = new EdgeMaskEntry[width * height];

    for (i = 0; edgeDiffs[i] != 0 && !logoInfoAvailable; i++)
    {
        int pixelsInMask = 0;

        VERBOSE(VB_COMMFLAG, QString("Trying with edgeDiff == %1")
                .arg(edgeDiffs[i]));

        memset(edgeCounts, 0, sizeof(EdgeMaskEntry) * width * height);
        memset(edgeMask, 0, sizeof(EdgeMaskEntry) * width * height);

        nvp->GetRawVideoFrame(0);

        loops = 0;
        seekFrame = seekIncrement;

        while(loops < maxLoops && !nvp->GetEof())
        {
            VideoFrame* vf = nvp->GetRawVideoFrame(seekFrame);

            emit breathe();

            if (!fullSpeed)
                usleep(10000);

            DetectEdges(vf, edgeCounts, edgeDiffs[i]);

            seekFrame += seekIncrement;
            loops++;
        }

        VERBOSE(VB_COMMFLAG, "Analyzing edge data");

#ifdef SHOW_DEBUG_WIN
        unsigned char *fakeFrame;
        fakeFrame = new unsigned char[width * height * 3 / 2];
        memset(fakeFrame, 0, width * height * 3 / 2);
#endif

        for (y = 0; y < height; y++)
        {
            if ((y > (height/4)) && (y < (height * 3 / 4)))
                continue;

            for (x = 0; x < width; x++)
            {
                if ((x > (width/4)) && (x < (width * 3 / 4)))
                    continue;

                pos = y * width + x;

                if (edgeCounts[pos].isedge > (maxLoops * 0.66))
                {
                    edgeMask[pos].isedge = 1;
                    pixelsInMask++;
#ifdef SHOW_DEBUG_WIN
                    fakeFrame[pos] = 0xff;
#endif

                }

                if (edgeCounts[pos].horiz > (maxLoops * 0.66))
                    edgeMask[pos].horiz = 1;

                if (edgeCounts[pos].vert > (maxLoops * 0.66))
                    edgeMask[pos].vert = 1;

                if (edgeCounts[pos].ldiag > (maxLoops * 0.66))
                    edgeMask[pos].ldiag = 1;

                if (edgeCounts[pos].rdiag > (maxLoops * 0.66))
                    edgeMask[pos].rdiag = 1;
            }
        }

        SetLogoMaskArea();

        for (y = logoMinY; y < logoMaxY; y++)
        {
            for (x = logoMinX; x < logoMaxX; x++)
            {
                int neighbors = 0;

                if (!edgeMask[y * width + x].isedge)
                    continue;

                for (dy = y - 2; dy <= (y + 2); dy++ )
                {
                    for (dx = x - 2; dx <= (x + 2); dx++ )
                    {
                        if (edgeMask[dy * width + dx].isedge)
                            neighbors++;
                    }
                }

                if (neighbors < 5)
                    edgeMask[y * width + x].isedge = 0;
            }
        }

        SetLogoMaskArea();

        VERBOSE(VB_COMMFLAG, QString("Testing Logo area: topleft "
                                     "(%1,%2), bottomright (%3,%4)")
                                     .arg(logoMinX).arg(logoMinY)
                                     .arg(logoMaxX).arg(logoMaxY));

#ifdef SHOW_DEBUG_WIN
        for (x = logoMinX; x < logoMaxX; x++)
        {
            pos = logoMinY * width + x;
            fakeFrame[pos] = 0x7f;
            pos = logoMaxY * width + x;
            fakeFrame[pos] = 0x7f;
        }
        for (y = logoMinY; y < logoMaxY; y++)
        {
            pos = y * width + logoMinX;
            fakeFrame[pos] = 0x7f;
            pos = y * width + logoMaxX;
            fakeFrame[pos] = 0x7f;
        }

        comm_debug_show(fakeFrame);
        delete [] fakeFrame;

        cerr << "Hit ENTER to continue" << endl;
        getchar();
#endif

        if (((logoMaxX - logoMinX) < (width / 4)) &&
            ((logoMaxY - logoMinY) < (height / 4)) &&
            (pixelsInMask > 50))
        {
            logoInfoAvailable = true;
            logoEdgeDiff = edgeDiffs[i];

            VERBOSE(VB_COMMFLAG, QString("Using Logo area: topleft "
                                         "(%1,%2), bottomright (%3,%4)")
                    .arg(logoMinX).arg(logoMinY)
                    .arg(logoMaxX).arg(logoMaxY));
        }
        else
        {
            VERBOSE(VB_COMMFLAG, QString("Rejecting Logo area: topleft "
                                         "(%1,%2), bottomright (%3,%4), "
                                         "pixelsInMask (%5). "
                                         "Not within specified limits.")
                    .arg(logoMinX).arg(logoMinY)
                    .arg(logoMaxX).arg(logoMaxY)
                    .arg(pixelsInMask));
        }
    }

    delete [] edgeCounts;

    if (!logoInfoAvailable)
        VERBOSE(VB_COMMFLAG, "No suitable logo area found.");

    nvp->GetRawVideoFrame(0);
}

void ClassicCommDetector::CleanupFrameInfo(void)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::CleanupFrameInfo()");

    int value;
    int before, after;

    // try to account for noisy signal causing blank frames to be undetected
    if ((framesProcessed > (fps * 60)) &&
        (blankFrameCount < (framesProcessed * 0.0004)))
    {
        int avgHistogram[256];
        int minAvg = -1;
        int newThreshold = -1;

        VERBOSE(VB_COMMFLAG,
                QString("ClassicCommDetect: Only found %1 blank frames but "
                        "wanted at least %2, rechecking data using higher "
                        "threshold.")
                        .arg(blankFrameCount)
                        .arg((int)(framesProcessed * 0.0004)));
        blankFrameMap.clear();
        blankFrameCount = 0;

        memset(avgHistogram, 0, sizeof(avgHistogram));

        for (long i = 1; i <= framesProcessed; i++)
            avgHistogram[frameInfo[i].avgBrightness] += 1;

        for (int i = 1; i <= 255 && minAvg == -1; i++)
            if (avgHistogram[i] > (framesProcessed * 0.0004))
                minAvg = i;

        newThreshold = minAvg + 3;
        VERBOSE(VB_COMMFLAG, QString("Minimum Average Brightness on a frame "
                                     "was %1, will use %2 as new threshold")
                .arg(minAvg).arg(newThreshold));

        for (long i = 1; i <= framesProcessed; i++)
        {
            value = frameInfo[i].flagMask;
            frameInfo[i].flagMask = value & ~COMM_FRAME_BLANK;

            if (( !(frameInfo[i].flagMask & COMM_FRAME_BLANK)) &&
                (frameInfo[i].avgBrightness < newThreshold))
            {
                frameInfo[i].flagMask = value | COMM_FRAME_BLANK;
                blankFrameMap[i] = MARK_BLANK_FRAME;
                blankFrameCount++;
            }
        }

        VERBOSE(VB_COMMFLAG, QString("Found %1 blank frames using new value")
                .arg(blankFrameCount));
    }

    // try to account for fuzzy logo detection
    for (long i = 1; i <= framesProcessed; i++)
    {
        if ((i < 10) || (i > (framesProcessed - 10)))
            continue;

        before = 0;
        for (int offset = 1; offset <= 10; offset++)
            if (frameInfo[i - offset].flagMask & COMM_FRAME_LOGO_PRESENT)
                before++;

        after = 0;
        for (int offset = 1; offset <= 10; offset++)
            if (frameInfo[i + offset].flagMask & COMM_FRAME_LOGO_PRESENT)
                after++;

        value = frameInfo[i].flagMask;
        if (value == -1)
            frameInfo[i].flagMask = 0;

        if (value & COMM_FRAME_LOGO_PRESENT)
        {
            if ((before < 4) && (after < 4))
                frameInfo[i].flagMask = value & ~COMM_FRAME_LOGO_PRESENT;
        }
        else
        {
            if ((before > 6) && (after > 6))
                frameInfo[i].flagMask = value | COMM_FRAME_LOGO_PRESENT;
        }
    }
}

void ClassicCommDetector::DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges,
                                      int edgeDiff)
{
    int r = 2;
    unsigned char *buf = frame->buf;
    unsigned char p;
    int pos, x, y;

    for (y = commDetectBorder + r; y < (height - commDetectBorder - r); y++)
    {
        if ((y > (height/4)) && (y < (height * 3 / 4)))
            continue;

        for (x = commDetectBorder + r; x < (width - commDetectBorder - r); x++)
        {
            int edgeCount = 0;

            if ((x > (width/4)) && (x < (width * 3 / 4)))
                continue;

            pos = y * width + x;
            p = buf[pos];

            if (( abs(buf[y * width + (x - r)] - p) >= edgeDiff) ||
                ( abs(buf[y * width + (x + r)] - p) >= edgeDiff))
            {
                edges[pos].horiz++;
                edgeCount++;
            }

            if (( abs(buf[(y - r) * width + x] - p) >= edgeDiff) ||
                ( abs(buf[(y + r) * width + x] - p) >= edgeDiff))
            {
                edges[pos].vert++;
                edgeCount++;
            }

            if (( abs(buf[(y - r) * width + (x - r)] - p) >= edgeDiff) ||
                ( abs(buf[(y + r) * width + (x + r)] - p) >= edgeDiff))
            {
                edges[pos].ldiag++;
                edgeCount++;
            }

            if (( abs(buf[(y - r) * width + (x + r)] - p) >= edgeDiff) ||
                ( abs(buf[(y + r) * width + (x - r)] - p) >= edgeDiff))
            {
                edges[pos].rdiag++;
                edgeCount++;
            }

            if (edgeCount >= 3)
                edges[pos].isedge++;
        }
    }
}

void ClassicCommDetector::GetLogoCommBreakMap(QMap<long long, int> &map)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetLogoCommBreakMap()");

    map.clear();

    int curFrame;
    bool PrevFrameLogo;
    bool CurrentFrameLogo;

    curFrame = 1;
    PrevFrameLogo = false;

    while (curFrame <= framesProcessed)
    {
        if (frameInfo[curFrame].flagMask & COMM_FRAME_LOGO_PRESENT)
            CurrentFrameLogo = true;
        else
            CurrentFrameLogo = false;

        if (!PrevFrameLogo && CurrentFrameLogo)
            map[curFrame] = MARK_START;
        else if (PrevFrameLogo && !CurrentFrameLogo)
            map[curFrame] = MARK_END;

        curFrame++;
        PrevFrameLogo = CurrentFrameLogo;
    }

}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
