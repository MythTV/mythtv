#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <qstringlist.h>

#include "commercial_skip.h"
#include "programinfo.h"
#include "util.h"

//#include "commercial_debug.h"

CommDetect::CommDetect(int w, int h, double fps, int method)
{
#ifdef SHOW_DEBUG_WIN
    comm_debug_init(w, h);
#endif

    edgeMask = NULL;
    logoFrame = NULL;
    logoMask = NULL;
    logoCheckMask = NULL;
    logoMaxValues = NULL;
    logoMinValues = NULL;
    tmpBuf = NULL;

    logoGoodEdgeThreshold = 0.75;
    logoBadEdgeThreshold = 0.85;

    currentAspect = COMM_ASPECT_WIDE;

    curFrameNumber = -1;

    if (getenv("DEBUGCOMMFLAG"))
        verboseDebugging = true;
    else
        verboseDebugging = false;

    Init(w, h, fps, method);
}

CommDetect::~CommDetect(void)
{
    framePtr = NULL;

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

#ifdef SHOW_DEBUG_WIN
    comm_debug_destroy();
#endif
}

void CommDetect::Init(int w, int h, double frame_rate, int method)
{
    VERBOSE(VB_COMMFLAG, "Commercial Detection initialized: width = " <<
            w << ", height = " << h << ", fps = " << frame_rate <<
            ", method = " << method);

    commDetectMethod = method;

    width = w;
    height = h;
    fps = frame_rate;
    fpm = fps * 60;

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

    border = gContext->GetNumSetting("CommBorder", 20);
    blankFrameMaxDiff = gContext->GetNumSetting("CommBlankFrameMaxDiff", 25);

    aggressiveDetection = true;
    currentAspect = COMM_ASPECT_WIDE;
    decoderFoundAspectChanges = false;

    // Check if close to 4:3
    if(fabs(((w*1.0)/h) - 1.333333) < 0.1)
        currentAspect = COMM_ASPECT_NORMAL;

    lastFrameWasBlank = false;
    lastFrameWasSceneChange = false;

    memset(lastHistogram, 0, sizeof(lastHistogram));
    memset(histogram, 0, sizeof(histogram));
    lastHistogram[0] = -1;
    histogram[0] = -1;

    frameIsBlank = false;
    sceneHasChanged = false;
    stationLogoPresent = false;

    skipAllBlanks = true;

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
}

void CommDetect::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if ((watchingRecording) &&
            (message.left(14) == "DONE_RECORDING"))
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int filelen = tokens[2].toInt();

            message = QString("CommDetect::CustomEvent - Recieved a "
                              "DONE_RECORDING event for card %1.  ")
                              .arg(cardnum);

            if (cardnum == recorder->GetRecorderNumber())
            {
                m_parent->SetWatchingRecording(false);
                m_parent->SetLength(filelen);
                watchingRecording = false;

                message += "This is the recording we are flagging, turning "
                           "watchingRecording OFF.";
            }
            else
            {
                message += "Ignoring event, this is not the recording we are "
                           "flaggign.";
            }

            VERBOSE(VB_COMMFLAG, message);
        }
    }
}

void CommDetect::SetWatchingRecording(bool watching, RemoteEncoder *rec,
                                      NuppelVideoPlayer *nvp)
{
    watchingRecording = watching;
    recorder = rec;
    m_parent = nvp;

    if (watching)
        gContext->addListener(this);
    else
        gContext->removeListener(this);
}

void CommDetect::SetVideoParams(float aspect)
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

void CommDetect::ProcessNextFrame(VideoFrame *frame, long long frame_number)
{
    int flagMask = 0;
    FrameInfoEntry fInfo;

    if (!frame || frame_number == -1 || frame->codec != FMT_YV12)
        return;

    curFrameNumber = frame_number;
    framePtr = frame->buf;

    lastFrameWasBlank = frameIsBlank;
    lastFrameWasSceneChange = sceneHasChanged;

    fInfo.minBrightness = -1;
    fInfo.maxBrightness = -1;
    fInfo.avgBrightness = -1;
    fInfo.sceneChangePercent = -1;
    fInfo.aspect = currentAspect;
    fInfo.format = COMM_FORMAT_NORMAL;
    fInfo.flagMask = 0;

    frameInfo[curFrameNumber] = fInfo;

    if (commDetectMethod & COMM_DETECT_BLANKS)
        frameIsBlank = CheckFrameIsBlank();

    if (commDetectMethod & COMM_DETECT_SCENE)
        sceneHasChanged = CheckSceneHasChanged();

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

#ifdef SHOW_DEBUG_WIN
    VERBOSE(VB_COMMFLAG,QString().sprintf("Frame: %6lld, Mask %04x",
                                          curFrameNumber, flagMask ));
    getchar();
    comm_debug_show(frame->buf);
#endif

    framesProcessed++;
}

bool CommDetect::CheckFrameIsBlank(void)
{
    int DarkBrightness = 80;
    int DimBrightness = 120;
    int BoxBrightness = 30;
    int DimAVG = 35;
    bool abort = false;
    int max = 0;
    int min = 255;
    int avg = 0;
    unsigned char pixel;
    int pixelsChecked = 0;
    long long totBrightness = 0;
    unsigned char rowMax[height];
    unsigned char colMax[width];
    int topDarkRow = border;
    int bottomDarkRow = height - border - 1;
    int leftDarkCol = border;
    int rightDarkCol = width - border - 1;

    if (!width || !height)
        return(false);

    memset(&rowMax, 0, sizeof(rowMax));
    memset(&colMax, 0, sizeof(colMax));

    for(int y = border; y < (height - border) && !abort;
                        y += vertSpacing)
    {
        for(int x = border; x < (width - border) && !abort;
                            x += horizSpacing)
        {
            if ((logoInfoAvailable) &&
                (y >= logoMinY) && (y <= logoMaxY) &&
                (x >= logoMinX) && (x <= logoMaxX))
                continue;

            pixel = framePtr[y * width + x];

            pixelsChecked++;
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
    }

    for(int y = border; y < (height - border); y += vertSpacing)
    {
        if (rowMax[y] > BoxBrightness)
            break;
        else
            topDarkRow = y;
    }

    for(int y = border; y < (height - border); y += vertSpacing)
        if (rowMax[y] >= BoxBrightness)
            bottomDarkRow = y;

    for(int x = border; x < (width - border); x += horizSpacing)
    {
        if (colMax[x] > BoxBrightness)
            break;
        else
            leftDarkCol = x;
    }

    for(int x = border; x < (width - border); x += horizSpacing)
        if (colMax[x] >= BoxBrightness)
            rightDarkCol = x;

    if ((topDarkRow > border) &&
        (topDarkRow < (height * .20)) &&
        (bottomDarkRow < (height - border)) &&
        (bottomDarkRow > (height * .80)))
    {
        frameInfo[curFrameNumber].format = COMM_FORMAT_LETTERBOX;
    }
    else if ((leftDarkCol > border) &&
             (leftDarkCol < (width * .20)) &&
             (rightDarkCol < (width - border)) &&
             (rightDarkCol > (width * .80)))
    {
        frameInfo[curFrameNumber].format = COMM_FORMAT_PILLARBOX;
    }
    else
    {
        frameInfo[curFrameNumber].format = COMM_FORMAT_NORMAL;
    }


    avg = totBrightness / pixelsChecked;

    frameInfo[curFrameNumber].minBrightness = min;
    frameInfo[curFrameNumber].maxBrightness = max;
    frameInfo[curFrameNumber].avgBrightness = avg;

    if (verboseDebugging)
        VERBOSE(VB_COMMFLAG,QString().sprintf("Fr: %6lld - Br: min %03d, max "
                                             "%03d, avg %03d, fmt %d, asp %d",
                                             curFrameNumber, min, max, avg,
                                             frameInfo[curFrameNumber].format,
                                             frameInfo[curFrameNumber].aspect));

    totalMinBrightness += min;
    DimAVG = min + 10;

    if ((max - min) <= blankFrameMaxDiff)
        return(true);

    if ((!aggressiveDetection) &&
        ((max < DarkBrightness) ||
         ((max < DimBrightness) && (avg < DimAVG))))
        return(true);

    return(false);
}

bool CommDetect::CheckSceneHasChanged(void)
{
    if (!width || !height)
        return(false);

    if (lastHistogram[0] == -1)
    {
        memset(lastHistogram, 0, sizeof(lastHistogram));
        for(int y = border; y < (height - border); y += vertSpacing)
            for(int x = border; x < (width - border); x += horizSpacing)
                lastHistogram[framePtr[y * width + x]]++;

        return(false);
    }

    memcpy(lastHistogram, histogram, sizeof(histogram));

    // compare current frame with last frame here
    memset(histogram, 0, sizeof(histogram));
    for(int y = border; y < (height - border); y += vertSpacing)
        for(int x = border; x < (width - border); x += horizSpacing)
            histogram[framePtr[y * width + x]]++;

    if (lastFrameWasSceneChange)
    {
        memcpy(lastHistogram, histogram, sizeof(histogram));
        frameInfo[curFrameNumber].sceneChangePercent = 0;

        return(false);
    }

    long similar = 0;

    for(int i = 0; i < 256; i++)
    {
        if (histogram[i] < lastHistogram[i])
            similar += histogram[i];
        else
            similar += lastHistogram[i];
    }

    sceneChangePercent = (int)(100.0 * similar /
                               ((width - (border * 2)) *
                                (height - (border * 2)) /
                                 (vertSpacing * horizSpacing)));

    frameInfo[curFrameNumber].sceneChangePercent = sceneChangePercent;

    if (sceneChangePercent < 85)
    {
        memcpy(lastHistogram, histogram, sizeof(histogram));
        return(true);
    }

    return(false);
}

bool CommDetect::CheckStationLogo(void)
{
    long in_sum = 0;
    int in_pixels = 0;
    long out_sum = 0;
    int out_pixels = 0;

    if (!logoInfoAvailable)
        return(false);

    for (int y = logoMinY - 2; y <= (logoMaxY + 2); y += 5)
    {
        for (int x = logoMinX - 2; x <= (logoMaxX + 2); x += 5)
        {
            int index = (y + 2) * width + x + 2;

            if (logoCheckMask[index] == 0)
                continue;

            in_sum = in_pixels = 0;
            out_sum = out_pixels = 0;

            for (int y2 = 0; y2 < 5; y2++)
            {
                index = (y + y2) * width + x;
                unsigned char *maskPtr = &logoMask[index];
                unsigned char *fPtr = &framePtr[index];
                for (int x2 = 0; x2 < 5; x2++)
                {
                    if (*maskPtr == 1)
                    {
                        in_sum += *fPtr;
                        in_pixels++;
                    }
                    else if (*maskPtr == 3)
                    {
                        out_sum += *fPtr;
                        out_pixels++;
                    }
                    index++;
                    maskPtr++;
                    fPtr++;
                }
            }

            if ((in_pixels >= 5) && (out_pixels >= 5))
            {
                int in_avg = in_sum / in_pixels;
                int out_avg = out_sum / out_pixels;

                if (in_avg < (out_avg - 10))
                {
                    return(false);
                }
            }
        }
    }

    in_sum = in_pixels = 0;
    out_sum = out_pixels = 0;

    for (int y = logoMinY - 2; y <= (logoMaxY + 2); y++)
    {
        int index = y * width + logoMinX - 2;
        unsigned char *maskPtr = &logoMask[index];
        unsigned char *fPtr = &framePtr[index];
        for (int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        {
            if (*maskPtr == 1)
            {
                in_sum += *fPtr;
                in_pixels++;
            }
            else if (*maskPtr == 3)
            {
                out_sum += *fPtr;
                out_pixels++;
            }
            index++;
            maskPtr++;
            fPtr++;
        }
    }

    if (!in_pixels || !out_pixels)
        return(false);

    int in_avg = in_sum / in_pixels;
    int out_avg = out_sum / out_pixels;

    if (in_avg > (out_avg + 10))
        return(true);

    return(false);
}

bool CommDetect::CheckRatingSymbol(void)
{
    int min_x = (int)(width * 0.03);
    int max_x = (int)(width * 0.25);
    int min_y = (int)(height * 0.05);
    int max_y = (int)(height * 0.27);
    int ratMax = 160;
    int r, g, b;
    int Y, U, V;
    int index, offset, yOffset;
    int whitePixels = 0;
    unsigned char *uPtr = &framePtr[width * height];
    unsigned char *vPtr = &framePtr[width * height * 5 / 4];

    memset(tmpBuf, ' ', width * height);

    if (min_x < border)
        min_x = border;

    if (min_y < border)
        min_y = border;

    for(int y = min_y; y <= max_y; y++)
    {
        yOffset = y * width / 2;
        for(int x = min_x; x <= max_x; x++)
        {
            index = y * width + x;
            offset = yOffset + (x / 2);
            Y = framePtr[index];
            U = uPtr[offset];
            V = vPtr[offset];

            r = Y + (14075 * (V - 128)) / 10000;
            g = Y - ((3455 * (U - 128)) - (7169 * (V - 128)))/10000;
            b = Y + (17790 * (U - 128))/10000;

            if ((r > ratMax) && (g > ratMax) && (b > ratMax))
            {
                tmpBuf[index] = 'W';
                whitePixels++;
            }
        }
    }

    if (whitePixels < 40)
        return(false);

    int startingCol = max_x;
    int endingCol = min_x;
    int startingRow = max_y;
    int endingRow = min_y;
    int minWidth = (int)(width * 0.025);
    int maxWidth = (int)(width * 0.06);
    int minHeight = (int)(height * 0.035);
    int maxHeight = (int)(height * 0.080);
    for(int x = min_x; x <= max_x; x++)
    {
        int whitePixels = 0;
        for(int y = min_y; y <= max_y; y++)
        {
            if (tmpBuf[y * width + x] == 'W')
                whitePixels++;
        }

        if ((whitePixels < maxHeight) &&
            (whitePixels > minHeight))
        {
            if (x < startingCol)
                startingCol = x;
            if (x > endingCol)
                endingCol = x;
        }
    }

    int symWidth = endingCol - startingCol;
    if ((symWidth < minWidth) && (symWidth > maxWidth))
        return(false);

    for(int y = min_y; y <= max_y; y++)
    {
        int whitePixels = 0;
        int yOffset = y * width;
        for(int x = min_x; x <= max_x; x++)
        {
            if (tmpBuf[yOffset + x] == 'W')
                whitePixels++;
        }

        if ((whitePixels < maxWidth) &&
            (whitePixels > minWidth))
        {
            if (y < startingRow)
                startingRow = y;
            if (y > endingRow)
                endingRow = y;
        }
    }

    int symHeight = endingRow - startingRow;
    if ((symWidth > minWidth) && (symWidth < maxWidth) &&
        (symHeight > minHeight) && (symHeight < maxHeight))
        return(true);

    return(false);
}

void CommDetect::SetMinMaxPixels(VideoFrame *frame)
{
    if (!logoFrameCount)
    {
        memset(logoMaxValues, 0, width * height);
        memset(logoMinValues, 255, width * height);
        memset(logoFrame, 0, width * height);
        memset(logoMask, 0, width * height);
        memset(logoCheckMask, 0, width * height);
    }

    logoFrameCount++;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            unsigned char pixel = frame->buf[y * width + x];
            if (logoMaxValues[y * width + x] < pixel)
                logoMaxValues[y * width + x] = pixel;
            if (logoMinValues[y * width + x] > pixel)
                logoMinValues[y * width + x] = pixel;
        }
    }
}

bool CommDetect::FrameIsBlank(void)
{
    return(frameIsBlank);
}

bool CommDetect::SceneHasChanged(void)
{
    return(sceneHasChanged);
}

void CommDetect::ClearAllMaps(void)
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

void CommDetect::GetCommBreakMap(QMap<long long, int> &marks)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetCommBreakMap()");

    marks.clear();

    CleanupFrameInfo();

    if (verboseDebugging)
        DumpFrameInfo();

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

                                      // FIXME, probably should NOT base this
                                      // on whether there's a blank-marked
                                      // comm map?????
        case COMM_DETECT_ALL:         BuildBlankFrameCommList();
                                      if (blankCommMap.size())
                                      {
                                          BuildAllMethodsCommList();
                                          marks = commBreakMap;
                                      }
                                      break;
    }

    VERBOSE(VB_COMMFLAG, "Final Commercial Break Map" );
    DumpMap(marks);
}

void CommDetect::SetBlankFrameMap(QMap<long long, int> &blanks)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::SetBlankFrameMap()");

    blankFrameMap = blanks;
}

void CommDetect::GetBlankFrameMap(QMap<long long, int> &blanks,
                                  long long start_frame)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetBlankFrameMap()");

    QMap<long long, int>::Iterator it;

    if (start_frame == -1)
        blanks.clear();

    for (it = blankFrameMap.begin(); it != blankFrameMap.end(); ++it)
        if ((start_frame == -1) || (it.key() >= start_frame))
            blanks[it.key()] = it.data();
}

void CommDetect::GetBlankCommMap(QMap<long long, int> &comms)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetBlankCommMap()");

    if (blankCommMap.isEmpty())
        BuildBlankFrameCommList();

    comms = blankCommMap;
}

void CommDetect::GetBlankCommBreakMap(QMap<long long, int> &comms)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::GetBlankCommBreakMap()");

    if (blankCommBreakMap.isEmpty())
        BuildBlankFrameCommList();

    comms = blankCommBreakMap;
}

void CommDetect::GetSceneChangeMap(QMap<long long, int> &scenes,
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

void CommDetect::BuildMasterCommList(void)
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

void CommDetect::UpdateFrameBlock(FrameBlock *fbp, FrameInfoEntry finfo,
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

void CommDetect::BuildAllMethodsCommList(void)
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
    long formatCounts[COMM_FORMAT_MAX];

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
        for(long i = 0; i < framesProcessed; i++ )
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
        for(long i = 0; i < framesProcessed; i++ )
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

        msg.sprintf("%5d %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d %5.2f %6d %6d %5d",
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

            if (fbp->length > MAX_COMM_LENGTH)
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      length > max_comm_length, +20");
                fbp->score += 20;
            }

            if (fbp->length > MAX_COMM_BREAK_LENGTH)
            {
                if (verboseDebugging)
                    VERBOSE(VB_COMMFLAG, "      length > max_comm_break_length,"
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
                if (fbp->length > MAX_COMM_BREAK_LENGTH)
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      length > "
                                             "max_comm_break_length, +20");
                    fbp->score += 20;
                }
                else
                {
                    if (verboseDebugging)
                        VERBOSE(VB_COMMFLAG, "      length <= "
                                            "max_comm_break_length, +10");
                    fbp->score += 10;
                }
            }

            if ((logoInfoAvailable) && (fbp->logoCount < (fbp->frames * 0.50)))
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

        msg.sprintf("  NOW %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d %5.2f %6d %6d %5d",
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

        msg.sprintf("%5d %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d %5.2f %6d %6d %5d",
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

        msg.sprintf("  NOW %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d %5.2f %6d %6d %5d",
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
            ((fbp->end - breakStart) > (MAX_COMM_BREAK_LENGTH * fps)))
        {
            if (verboseDebugging)
                VERBOSE(VB_COMMFLAG,
                        QString("Closing commercial block at start of frame "
                                "block %1 with length %2, frame block length "
                                "of %3 frames would put comm block "
                                "length over max of %4 seconds.")
                                .arg(curBlock).arg(fbp->start - breakStart)
                                .arg(fbp->frames).arg(MAX_COMM_BREAK_LENGTH));

            commBreakMap[breakStart] = MARK_COMM_START;
            commBreakMap[fbp->start] = MARK_COMM_END;
            lastStart = breakStart;
            lastEnd = fbp->start;
            breakStart = -1;
        }
        if (thisScore == 0)
        {
            thisScore = lastScore;
        }
        else if (thisScore < 0)
        {
            if ((lastScore > 0) || (curBlock == 0))
            {
                if ((fbp->start - lastEnd) < (MIN_SHOW_LENGTH * fps))
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
                                            .arg(MIN_SHOW_LENGTH));
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
                if ((fbp->end - breakStart) > (MIN_COMM_BREAK_LENGTH * fps))
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
                                        " block %1 with length %2, frame block "
                                        "length of %3 frames would put comm "
                                        "block length under min of %4 seconds.")
                                        .arg(curBlock)
                                        .arg(fbp->start - breakStart)
                                        .arg(fbp->frames)
                                        .arg(MIN_COMM_BREAK_LENGTH));
                    breakStart = -1;
                }
            }
        }
        else if ((thisScore > 0) && (lastScore < 0) && (breakStart != -1))
        {
            if ((fbp->start - breakStart) > (MIN_COMM_BREAK_LENGTH * fps))
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
                                    "block %1 with length %2, frame block "
                                    "length of %3 frames would put comm block "
                                    "length under min of %4 seconds.")
                                    .arg(curBlock).arg(fbp->start - breakStart)
                                    .arg(fbp->frames)
                                    .arg(MIN_COMM_BREAK_LENGTH));
            }
            breakStart = -1;
        }

        msg.sprintf("%5d %3d:%02d %6ld %6ld %6ld %7.2f %3d %6d %6d %6d %5.2f %6d %6d %5d",
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

    delete [] fblock;
}

void CommDetect::BuildBlankFrameCommList(void)
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

void CommDetect::BuildSceneChangeCommList(void)
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

void CommDetect::BuildLogoCommList()
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

void CommDetect::MergeBlankCommList(void)
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

void CommDetect::DeleteCommAtFrame(QMap<long long, int> &commMap,
                                   long long frame)
{
    long long start = -1;
    long long end = -1;
    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;

    it = commMap.begin();
    prev = it;
    it++;
    for(; it != commMap.end(); ++it, ++prev)
    {
        if ((prev.data() == MARK_COMM_START) &&
            (prev.key() <= frame) &&
            (it.data() == MARK_COMM_END) &&
            (it.key() >= frame))
        {
            start = prev.key();
            end = it.key();
        }

        if ((prev.key() > frame) && (start == -1))
            return;
    }

    if (start != -1)
    {
        commMap.erase(start);
        commMap.erase(end);
    }
}

bool CommDetect::FrameIsInBreakMap(long long f, QMap<long long, int> &breakMap)
{
    for(long long i = f; i < framesProcessed; i++)
        if (breakMap.contains(i)) 
        {
            int type = breakMap[i];
            if ((type == MARK_COMM_END) || (i == f))
                return(true);
            if (type == MARK_COMM_START)
                return(false);
        }

    for(long long i = f; i >= 0; i--)
        if (breakMap.contains(i)) 
        {
            int type = breakMap[i];
            if ((type == MARK_COMM_START) || (i == f))
                return(true);
            if (type == MARK_COMM_END)
                return(false);
        }

    return(false);
}

void CommDetect::DumpMap(QMap<long long, int> &map)
{
    QMap<long long, int>::Iterator it;
    QString msg;

    VERBOSE(VB_COMMFLAG, "---------------------------------------------------");
    cerr << endl;
    for (it = map.begin(); it != map.end(); ++it)
    {
        long long frame = it.key();
        int my_fps = (int)ceil(fps);
        int hour = (frame / my_fps) / 60 / 60;
        int min = (frame / my_fps) / 60 - (hour * 60);
        int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
        int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                    (hour * 60 * 60 * my_fps));
        msg.sprintf("%7lld : %d (%02d:%02d:%02d.%02d) (%d)",
                    it.key(), it.data(), hour, min, sec, frm,
                    (int)(frame / my_fps));
        VERBOSE(VB_COMMFLAG, msg);
    }
    VERBOSE(VB_COMMFLAG, "---------------------------------------------------");
}

void CommDetect::DumpLogo(bool fromCurrentFrame)
{
    char scrPixels[] = " .oxX";

    if (!logoInfoAvailable)
        return;

    printf( "\nLogo Data " );
    if (fromCurrentFrame)
        printf( "from current frame\n" );

    printf( "\n     " );

    for(int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        printf( "%d", x % 10);
    printf( "\n" );
    for(int y = logoMinY - 2; y <= (logoMaxY + 2); y++)
    {
        printf( "%3d: ", y );
        for(int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        {
            if (fromCurrentFrame)
            {
                printf( "%c", scrPixels[framePtr[y * width + x] / 50]);
            }
            else
            {
                switch (logoMask[y * width + x])
                {
                    case 0:
                    case 2: printf(" ");
                            break;
                    case 1: printf("*");
                            break;
                    case 3: printf(".");
                            break;
                }
            }
        }
        printf( "\n" );
    }
}

void CommDetect::GetLogoMask(unsigned char *mask)
{
    logoFrameCount = 0;
    memset(mask, 0, width * height);
    memcpy(logoFrame, logoMinValues, width * height);

    int maxValue = 0;
    for(int i = 0; i < (width * height); i++)
        if (logoFrame[i] > maxValue)
            maxValue = logoFrame[i];

    for(int i = 0; i < (width * height); i++)
    {
        if (logoFrame[i] > (maxValue - 70))
            logoMask[i] = 255;
        else
            logoMask[i] = 0;
    }

    int exclMinX = (int)(width * .45);
    int exclMaxX = (int)(width * .55);
    int exclMinY = (int)(height * .35);
    int exclMaxY = (int)(height * .65);

    for (int y = 0; y < height; y++)
    {
        if ((y > exclMinY) && (y < exclMaxY))
            continue;

        for (int x = 0; x < width; x++)
        {
            if ((x > exclMinX) && (x < exclMaxX))
                continue;

            // need to add some detection for non-opaque logos here
            if (logoMask[y * width + x] > 80)
                mask[y * width + x] = 1;
            else
                mask[y * width + x] = 0;
        }
    }
}

void CommDetect::SetLogoMaskArea()
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

void CommDetect::SetLogoMask(unsigned char *mask)
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

void CommDetect::CondenseMarkMap(QMap<long long, int>&map, int spacing,
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

void CommDetect::ConvertShowMapToCommMap(QMap<long long, int>&map)
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
bool CommDetect::CheckEdgeLogo(void)
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

    if ((goodEdgeRatio > logoGoodEdgeThreshold) &&
        (badEdgeRatio < logoBadEdgeThreshold))
        return true;
    else
        return false;
}

void CommDetect::SearchForLogo(NuppelVideoPlayer *nvp, bool fullSpeed)
{
    int seekIncrement = (int)(LOGO_SAMPLE_SPACING * fps);
    long long seekFrame;
    int loops;
    int maxLoops = LOGO_SAMPLES_NEEDED;
    EdgeMaskEntry *edgeCounts;
    int pos, i, x, y, dx, dy;
    int edgeDiffs[] = { 5, 7, 10, 15, 20, 30, 40, 50, 60, 0 };

 
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

        nvp->JumpToFrame(0);
        nvp->ClearAfterSeek();

        nvp->GetFrame(1,true);

        loops = 0;
        seekFrame = seekIncrement;

        while(loops < maxLoops && !nvp->eof) 
        {
            nvp->JumpToFrame(seekFrame);
            nvp->ClearAfterSeek();
            nvp->GetFrame(1,true);

            if (!fullSpeed)
                usleep(10000);

            DetectEdges(nvp->videoOutput->GetLastDecodedFrame(), edgeCounts,
                        edgeDiffs[i]);

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

    nvp->JumpToFrame(0);
    nvp->ClearAfterSeek();
    nvp->GetFrame(1,true);
}

bool CommDetect::CheckFrameIsInCommMap(long long frameNumber,
                                       QMap<long long, int>)
{
    QMap<long long, int>::Iterator it;
    long long lastStart = -1;

    for (it = blankCommMap.begin(); it != blankCommMap.end(); ++it)
    {
        if ((it.data() == MARK_COMM_END) &&
            (frameNumber <= it.key()) &&
            (frameNumber >= lastStart))
            return true;
        else if (it.data() == MARK_COMM_START)
            lastStart = it.key();
    }

    return false;
}

void CommDetect::CleanupFrameInfo(void)
{
    VERBOSE(VB_COMMFLAG, "CommDetect::CleanupFrameInfo()");

    int value;
    int before, after;

    // try to account for noisy signal causing blank frames to be undetected
    if (blankFrameCount < (framesProcessed * 0.0004))
    {
        int avgHistogram[256];
        int minAvg = -1;
        int newThreshold = -1;

        VERBOSE(VB_COMMFLAG, "Didn't find enough blank frames, rechecking data "
                             "using higher threshold.");
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

void CommDetect::DumpFrameInfo(void)
{
    FrameInfoEntry fi;

    for (long i = 1; i <= framesProcessed; i++)
    {
        fi = frameInfo[i];
        VERBOSE(VB_COMMFLAG, QString().sprintf(
               "Frame: %6ld -> %3d %3d %3d %3d %04x",
               i, fi.minBrightness, fi.maxBrightness, fi.avgBrightness,
               fi.sceneChangePercent, fi.flagMask ));
    }
}

void CommDetect::DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges,
                             int edgeDiff)
{
    int r = 2;
    unsigned char *buf = frame->buf;
    unsigned char p;
    int pos, x, y;

    for (y = border + r; y < (height - border - r); y++)
    {
        if ((y > (height/4)) && (y < (height * 3 / 4)))
            continue;

        for (x = border + r; x < (width - border - r); x++)
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

void CommDetect::GetLogoCommBreakMap(QMap<long long, int> &map)
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
