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

    logoFrame = NULL;
    logoMask = NULL;
    logoCheckMask = NULL;
    logoMaxValues = NULL;
    logoMinValues = NULL;

    Init(w, h, fps, method);
}

CommDetect::~CommDetect(void)
{
    frame_ptr = NULL;

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

    framesProcessed = 0;

    border = gContext->GetNumSetting("CommBorder", 10);

    aggressiveDetection = true;

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

    frame_ptr = NULL;

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

    logoFrameCount = 0;
    logoInfoAvailable = false;

    ClearAllMaps();
}

void CommDetect::ProcessNextFrame(VideoFrame *frame, long long frame_number)
{
    if (!frame || frame->codec != FMT_YV12)
        return;

    frame_ptr = frame->buf;

#ifdef SHOW_DEBUG_WIN
    comm_debug_show(frame->buf);
#endif

    lastFrameWasBlank = frameIsBlank;
    lastFrameWasSceneChange = sceneHasChanged;

    if (commDetectMethod & COMM_DETECT_BLANKS)
        frameIsBlank = CheckFrameIsBlank();

    if (commDetectMethod & COMM_DETECT_SCENE)
        sceneHasChanged = CheckSceneHasChanged();

    if ((logoInfoAvailable) && (commDetectMethod & COMM_DETECT_LOGO))
    {
        bool nowPresent = CheckStationLogo();

        if (!stationLogoPresent && nowPresent)
            logoCommBreakMap[frame_number] = MARK_START;
        else if (stationLogoPresent && !nowPresent)
            logoCommBreakMap[frame_number] = MARK_END;

        stationLogoPresent = nowPresent;
    }

    if (frame_number != -1)
    {
        if (frameIsBlank)
            blankFrameMap[frame_number] = MARK_BLANK_FRAME;

        if (sceneHasChanged)
            sceneMap[frame_number] = MARK_SCENE_CHANGE;
    }

    framesProcessed++;
}

bool CommDetect::CheckFrameIsBlank(void)
{
    const unsigned int max_brightness = 120;
    const unsigned int test_brightness = 80;
    const int pass_start[7] = {0, 4, 0, 2, 0, 1, 0};
    const int pass_inc[7] = {8, 8, 4, 4, 2, 2, 1};
    const int pass_ystart[7] = {0, 0, 4, 0, 2, 0, 1};
    const int pass_yinc[7] = {8, 8, 8, 4, 4, 2, 2};
    bool isDim = false;
    int dimCount = 0;
    int pixelsChecked = 0;

    if (!width || !height)
        return(false);

    // go through the image in png interlacing style testing if blank
    // skip region 'border' pixels wide/high around border of image.
    for(int pass = 0; pass < 7; pass++)
    {
        for(int y = pass_ystart[pass] + border; y < (height - border);
                                                y += pass_yinc[pass])
        {
            for(int x = pass_start[pass] + border; x < (width - border);
                                                   x += pass_inc[pass])
            {
                pixelsChecked++;

                if (frame_ptr[y * width + x] > max_brightness)
                    return(false);
                if (frame_ptr[y * width + x] > test_brightness)
                {
                    isDim = true;
                    dimCount++;
                }
            }
        }
    }

    if ((dimCount > (int)(.05 * pixelsChecked)) &&
        (dimCount < (int)(.35 * pixelsChecked)))
        return(false);

    // frame is dim so test average
    if (isDim)
    {
        int avg = GetAvgBrightness();

        if (avg > 35)
            return(false);
    }

    return(true);
}

// analyzes every 1 out of 4 pixels (every other column in every other row)
bool CommDetect::CheckSceneHasChanged(void)
{
    if (!width || !height)
        return(false);

    if (lastHistogram[0] == -1)
    {
        memset(lastHistogram, 0, sizeof(lastHistogram));
        for(int y = border; y < (height - border); y += 2)
            for(int x = border; x < (width - border); x += 2)
                lastHistogram[frame_ptr[y * width + x]]++;

        return(false);
    }

    memcpy(lastHistogram, histogram, sizeof(histogram));

    // compare current frame with last frame here
    memset(histogram, 0, sizeof(histogram));
    for(int y = border; y < (height - border); y += 2)
        for(int x = border; x < (width - border); x += 2)
            histogram[frame_ptr[y * width + x]]++;

    if (lastFrameWasSceneChange)
    {
        memcpy(lastHistogram, histogram, sizeof(histogram));
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
                                (height - (border * 2)) / 4));
    if (sceneChangePercent < 91)
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
                unsigned char *fPtr = &frame_ptr[index];
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
        unsigned char *fPtr = &frame_ptr[index];
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
    blankFrameMap.clear();
    blankCommMap.clear();
    blankCommBreakMap.clear();
    sceneMap.clear();
    sceneCommBreakMap.clear();
    commBreakMap.clear();
}

void CommDetect::GetCommBreakMap(QMap<long long, int> &marks)
{
    marks.clear();

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

        case COMM_DETECT_ALL:         BuildBlankFrameCommList();
                                      BuildSceneChangeCommList();
                                      BuildLogoCommList();
                                      BuildAllMethodsCommList();
                                      marks = commBreakMap;
                                      break;
    }
}

void CommDetect::SetBlankFrameMap(QMap<long long, int> &blanks)
{
    blankFrameMap = blanks;
}

void CommDetect::GetBlankFrameMap(QMap<long long, int> &blanks,
                                  long long start_frame)
{
    QMap<long long, int>::Iterator it;

    if (start_frame == -1)
        blanks.clear();

    for (it = blankFrameMap.begin(); it != blankFrameMap.end(); ++it)
        if ((start_frame == -1) || (it.key() >= start_frame))
            blanks[it.key()] = it.data();
}

void CommDetect::GetBlankCommMap(QMap<long long, int> &comms)
{
    if (blankCommMap.isEmpty())
        BuildBlankFrameCommList();

    comms = blankCommMap;
}

void CommDetect::GetBlankCommBreakMap(QMap<long long, int> &comms)
{
    if (blankCommBreakMap.isEmpty())
        BuildBlankFrameCommList();

    comms = blankCommBreakMap;
}

void CommDetect::GetSceneChangeMap(QMap<long long, int> &scenes,
                                   long long start_frame)
{
    QMap<long long, int>::Iterator it;

    if (start_frame == -1)
        scenes.clear();

    for (it = sceneMap.begin(); it != sceneMap.end(); ++it)
        if ((start_frame == -1) || (it.key() >= start_frame))
            scenes[it.key()] = it.data();
}

int CommDetect::GetAvgBrightness()
{
    int brightness = 0;
    int pixels = 0;

    for(int y = border; y < (height - border); y += 4)
        for(int x = border; x < (width - border); x += 4)
        {
            brightness += frame_ptr[y * width + x];
            pixels++;
        }

    return(brightness/pixels);
}

void CommDetect::BuildMasterCommList(void)
{
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

                while ((f < framesProcessed) && (f < it_b.key()) && (allTrue))
                    allTrue = FrameIsInCommBreak(f++, sceneCommBreakMap);
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

void CommDetect::BuildAllMethodsCommList(void)
{
    bool includeBlankFrameMethod  = (blankCommBreakMap.size() > 2);
    bool includeSceneChangeMethod = (sceneCommBreakMap.size() > 2);
    bool includeLogoMethod        = (logoCommBreakMap.size() > 2);

    QMap<long long, int> frameMap;
//    QMap<long long, int>::Iterator it;

    commBreakMap.clear();

    for (int i = 0; i <= framesProcessed; i++)
    {
        int value = 0;

        if (blankFrameMap.contains(i))
            value += 0x01;

        if ((includeBlankFrameMethod) &&
            (blankCommBreakMap.contains(i)))
            value += 0x02;

        if ((includeSceneChangeMethod) &&
            (sceneCommBreakMap.contains(i)))
            value += 0x04;

        if ((includeLogoMethod) &&
            (logoCommBreakMap.contains(i)))
            value += 0x08;

        frameMap[i] = value;
    }


//    for (int i = 0; i <= framesProcessed; i++)
//    {
//        int value = frameMap[i];
        
//    }
}

void CommDetect::BuildBlankFrameCommList(void)
{
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
                 ((abs((int)(gap_length - (15 * fps))) < 10 ) ||
                  (abs((int)(gap_length - (20 * fps))) < 11 ) ||
                  (abs((int)(gap_length - (30 * fps))) < 12 ) ||
                  (abs((int)(gap_length - (45 * fps))) < 13 ) ||
                  (abs((int)(gap_length - (60 * fps))) < 15 ) ||
                  (abs((int)(gap_length - (90 * fps))) < 10 ) ||
                  (abs((int)(gap_length - (120 * fps))) < 10 ))) ||
                ((!aggressiveDetection) &&
                 ((abs((int)(gap_length - (15 * fps))) < 13 ) ||
                  (abs((int)(gap_length - (20 * fps))) < 15 ) ||
                  (abs((int)(gap_length - (30 * fps))) < 17 ) ||
                  (abs((int)(gap_length - (45 * fps))) < 19 ) ||
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
        if( i < (commercials-1))
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

    MergeBlankCommList();
}

void CommDetect::BuildSceneChangeCommList(void)
{
    int section_start = -1;
    int seconds = (int)(framesProcessed / fps);
    int sc_histogram[seconds+1];

    sceneCommBreakMap.clear();

    memset(sc_histogram, 0, sizeof(sc_histogram));
    for(long long f = 0; f < framesProcessed; f++)
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
}

void CommDetect::BuildLogoCommList()
{
    CondenseMarkMap(logoCommBreakMap, (int)(25 * fps), (int)(30 * fps));
    ConvertShowMapToCommMap(logoCommBreakMap);
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

bool CommDetect::FrameIsInCommBreak(long long f, QMap<long long, int> &breakMap)
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

    cerr << "DumpMap()" << endl;
    cerr << "---------------------" << endl;
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
        printf( "%7lld : %d (%02d:%02d:%02d.%02d) (%d)\n",
            it.key(), it.data(), hour, min, sec, frm,
            (int)(frame / my_fps));
    }
    cerr << "---------------------" << endl;
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
                printf( "%c", scrPixels[frame_ptr[y * width + x] / 50]);
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
    logoMinX = width - 1;
    logoMaxX = 0;
    logoMinY = height - 1;
    logoMaxY = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (logoMask[y * width + x] == 1)
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

    if (logoMinX < 2)
        logoMinX = 2;
    if (logoMaxX > (width-3))
        logoMaxX = (width-3);
    if (logoMinY < 2)
        logoMinY = 2;
    if (logoMaxY > (height-3))
        logoMaxY = (height-3);
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

//    DumpLogo();

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
    for (it = map.begin(); it != map.end(); it++)
        tmpMap[it.key()] = it.data();

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

void CommDetect::SearchForLogo(NuppelVideoPlayer *nvp, bool fullSpeed,
                               bool verbose)
{
    int secs = 10;
    const int loops = 8;
    int maxLoops = 12;
    int loop = 0;
    int sampleSpacing = 1;
    int seekIncrement = (int)(sampleSpacing * 60 * fps);
    long long seekFrame = seekIncrement;
    long long endFrame = seekFrame + (long long)(secs * fps);
    int counter = 0;
    unsigned char *mask[loops];

    VERBOSE(VB_COMMFLAG, "Searching for Station Logo");

    for (int i = 0; i < loops; i++)
        mask[i] = new unsigned char[height * width];

    if (verbose)
    {
        printf( "Logo Search" );
        fflush( stdout );
    }

    nvp->GetFrame(1,true);

    while(counter < loops && loop < maxLoops && !nvp->eof) 
    {
        nvp->JumpToFrame(seekFrame);
        nvp->ClearAfterSeek();

        for (int i = 0; i < (int)(secs * fps) && !nvp->eof; i++)
        {
            nvp->GetFrame(1,true);

            SetMinMaxPixels(nvp->videoOutput->GetLastDecodedFrame());

            // sleep a little so we don't use all cpu even if we're niced
            if (!fullSpeed)
                usleep(10000);
        }

        GetLogoMask(mask[counter]);

        int pixelsInMask = 0;
        for (int i = 0; i < (width * height); i++)
            if (mask[counter][i])
                pixelsInMask++;

        if (pixelsInMask < (int)(width * height * .10))
            counter++;

        seekFrame += seekIncrement;
        endFrame += seekIncrement;

        loop++;
    }

    for (int i=0; i < (width * height); i++)
    {
        int sum = 0;
        for (int loop=0; loop < counter; loop++)
            if (mask[loop][i])
                sum++;

        if (sum > (int)(counter * .33))
            mask[0][i] = 1;
        else
            mask[0][i] = 0;
    }

    SetLogoMask(mask[0]);

    nvp->JumpToFrame(0);
    nvp->ClearAfterSeek();

    if (verbose)
    {
        printf( "\b\b\b\b\b\b\b\b\b\b\b" );
        fflush( stdout );
    }

    for (int i = 0; i < loops; i++)
        delete [] mask[i];
}

