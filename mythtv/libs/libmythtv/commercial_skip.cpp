#include <stdlib.h>
#include <math.h>

#include <qstringlist.h>

#include "commercial_skip.h"
#include "programinfo.h"
#include "util.h"

//#include "commercial_debug.h"

CommDetect::CommDetect(int w, int h, double fps)
{
#ifdef SHOW_DEBUG_WIN
    comm_debug_init(w, h);
#endif

    Init(w, h, fps);
}

CommDetect::~CommDetect(void)
{
    frame_ptr = NULL;

#ifdef SHOW_DEBUG_WIN
    comm_debug_destroy();
#endif
}

void CommDetect::Init(int w, int h, double fps)
{
    width = w;
    height = h;
    frame_rate = fps;

    framesProcessed = 0;

    detectBlankFrames = true;
    detectSceneChanges = false;

    lastFrameWasBlank = false;
    lastFrameWasSceneChange = false;

    memset(lastHistogram, 0, sizeof(lastHistogram));
    memset(histogram, 0, sizeof(histogram));
    lastHistogram[0] = -1;
    histogram[0] = -1;

    frameIsBlank = false;
    sceneHasChanged = false;

    frame_ptr = NULL;

    blankFrameMap.clear();
    blankCommMap.clear();
    blankCommBreakMap.clear();
    sceneChangeMap.clear();
    sceneChangeCommMap.clear();
    commBreakMap.clear();
}

void CommDetect::ProcessNextFrame(unsigned char *buf, long long frame_number)
{
    frame_ptr = buf;

    lastFrameWasBlank = frameIsBlank;
    lastFrameWasSceneChange = sceneHasChanged;

    if (detectBlankFrames)
        frameIsBlank = CheckFrameIsBlank();

    if (detectSceneChanges)
        sceneHasChanged = CheckSceneHasChanged();

    if (frame_number != -1)
    {
        if (frameIsBlank)
            blankFrameMap[frame_number] = MARK_BLANK_FRAME;

        if (sceneHasChanged)
            sceneChangeMap[frame_number] = MARK_SCENE_CHANGE;
    }

    framesProcessed++;

#ifdef SHOW_DEBUG_WIN
    comm_debug_show(buf);
#endif
}

bool CommDetect::CheckFrameIsBlank(void)
{
    const unsigned int max_brightness = 120;
    const unsigned int test_brightness = 80;
    bool line_checked[height];
    bool test = false;

    if (!width || !height)
        return(false);

    for(int y = 0; y < height; y++)
        line_checked[y] = false;

    for(int divisor = 3; divisor < 200; divisor += 3)
    {
        int y_mult = (int)(height / divisor);
        int x_mult = (int)(width / divisor);

        if (x_mult < 1 || y_mult < 1)
            continue;

        for(int y = y_mult; y < height; y += y_mult)
        {
            if (line_checked[y])
                continue;

            for(int x = x_mult; x < width; x += x_mult)
            {
                if (frame_ptr[y * width + x] > max_brightness)
                    return(false);
                if (frame_ptr[y * width + x] > test_brightness)
                    test = true;
            }
            line_checked[y] = true;
        }
    }

    // frame is dim so test average
    if (test)
    {
        int avg = GetAvgBrightness();

        if (avg > 35)
            return(false);
    }

    lastFrameWasBlank = true;

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
        for(int y = 0; y < height; y += 2)
            for(int x = 0; x < width; x += 2)
                lastHistogram[frame_ptr[y * width + x]]++;

        return(false);
    }

    memcpy(lastHistogram, histogram, sizeof(histogram));

    // compare current frame with last frame here
    memset(histogram, 0, sizeof(histogram));
    for(int y = 0; y < height; y += 2)
        for(int x = 0; x < width; x += 2)
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

    if (similar < (width * height / 4 * .91))
    {
        memcpy(lastHistogram, histogram, sizeof(histogram));
        return(true);
    }

    return(false);
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
    sceneChangeMap.clear();
    sceneChangeCommMap.clear();
    commBreakMap.clear();
}

void CommDetect::GetCommBreakMap(QMap<long long, int> &marks)
{
    QMap<long long, int>::Iterator it;

    marks.clear();

    if (detectBlankFrames)
        BuildBlankFrameCommList();
    else
        blankCommBreakMap.clear();

    if (detectSceneChanges)
        BuildSceneChangeCommList();
    else
        sceneChangeCommMap.clear();

    BuildMasterCommList();

    if (commBreakMap.isEmpty())
        for (it = blankCommBreakMap.begin(); it != blankCommBreakMap.end();
                ++it)
            commBreakMap[it.key()] = it.data();

    if (commBreakMap.isEmpty())
        for (it = sceneChangeCommMap.begin(); it != sceneChangeCommMap.end();
                ++it)
            commBreakMap[it.key()] = it.data();

    for (it = commBreakMap.begin(); it != commBreakMap.end(); ++it)
        marks[it.key()] = it.data();
}

void CommDetect::SetBlankFrameMap(QMap<long long, int> &blanks)
{
    QMap<long long, int>::Iterator it;

    blankFrameMap.clear();

    for (it = blanks.begin(); it != blanks.end(); ++it)
        blankFrameMap[it.key()] = it.data();
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
    QMap<long long, int>::Iterator it;

    if (blankCommMap.isEmpty())
        BuildBlankFrameCommList();

    comms.clear();

    for (it = blankCommMap.begin(); it != blankCommMap.end(); ++it)
        comms[it.key()] = it.data();
}

void CommDetect::GetBlankCommBreakMap(QMap<long long, int> &comms)
{
    QMap<long long, int>::Iterator it;

    if (blankCommBreakMap.isEmpty())
        BuildBlankFrameCommList();

    comms.clear();

    for (it = blankCommBreakMap.begin(); it != blankCommBreakMap.end(); ++it)
        comms[it.key()] = it.data();
}

void CommDetect::GetSceneChangeMap(QMap<long long, int> &scenes,
                                   long long start_frame)
{
    QMap<long long, int>::Iterator it;

    if (start_frame == -1)
        scenes.clear();

    for (it = sceneChangeMap.begin(); it != sceneChangeMap.end(); ++it)
        if ((start_frame == -1) || (it.key() >= start_frame))
            scenes[it.key()] = it.data();
}

int CommDetect::GetAvgBrightness()
{
    int brightness = 0;
    int pixels = 0;

    for(int y = 0; y < height; y += 4)
        for(int x = 0; x < width; x += 4)
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
        (sceneChangeCommMap.size() > 1))
    {
        // see if beginning of the recording looks like a commercial
        QMap<long long, int>::Iterator it_a;
        QMap<long long, int>::Iterator it_b;

        it_a = blankCommBreakMap.begin();
        it_b = sceneChangeCommMap.begin();

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

        it = sceneChangeCommMap.begin();
        for(unsigned int i = 0; i < sceneChangeCommMap.size(); i++)
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
        (sceneChangeCommMap.size() > 1))
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

            if (fdiff < (62 * frame_rate))
            {
                long long f = it_a.key() + 1;

                allTrue = true;

                while ((f < it_b.key()) && (allTrue))
                    allTrue = FrameIsInCommBreak(f++, sceneChangeCommMap);
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
            if ((abs((int)(gap_length - (15 * frame_rate))) < 10 ) ||
                (abs((int)(gap_length - (20 * frame_rate))) < 11 ) ||
                (abs((int)(gap_length - (30 * frame_rate))) < 12 ) ||
                (abs((int)(gap_length - (45 * frame_rate))) < 13 ) ||
                (abs((int)(gap_length - (60 * frame_rate))) < 15 ) ||
                (abs((int)(gap_length - (90 * frame_rate))) < 10 ))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x] - 1;
                commercials++;
                i = x-1;
                x = frames;
            }
        }
    }

    i = 0;

    // don't allow single commercial at head
    // of show unless followed by another
    if ((commercials > 1) &&
        (c_end[0] < (33 * frame_rate)) &&
        (c_start[1] > (c_end[0] + 40 * frame_rate)))
        i = 1;

    // eliminate any blank frames at end of commercials
    bool first_comm = true;
    for(; i < (commercials-1); i++)
    {
        long long r = c_start[i];

        if ((r < (30 * frame_rate)) &&
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

            while((blankFrameMap.contains(r+1)) &&
                  (c_start[i+1] != (r+1)))
                r++;
        }
        else
        {
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
    int seconds = (int)(framesProcessed / frame_rate);
    int sc_histogram[seconds+1];

    memset(sc_histogram, 0, sizeof(sc_histogram));
    for(long long f = 0; f < framesProcessed; f++)
    {
        if (sceneChangeMap.contains(f))
            sc_histogram[(int)(f / frame_rate)]++;
    }

    for(long long s = 0; s < (seconds + 1); s++)
    {
        if (sc_histogram[s] > 2)
        {
            if (section_start == -1)
            {
                long long f = (long long)(s * frame_rate);
                for(int i = 0; i < frame_rate; i++, f++)
                {
                    if (sceneChangeMap.contains(f))
                    {
                        sceneChangeCommMap[f] = MARK_COMM_START;
                        i = (int)(frame_rate) + 1;
                    }
                }
            }

            section_start = s;
        }

        if ((section_start >= 0) &&
            (s > (section_start + 32)))
        {
            long long f = (long long)(section_start * frame_rate);
            bool found_end = false;

            for(int i = 0; i < frame_rate; i++, f++)
            {
                if (sceneChangeMap.contains(f))
                {
                    if (sceneChangeCommMap.contains(f))
                        sceneChangeCommMap.erase(f);
                    else
                        sceneChangeCommMap[f] = MARK_COMM_END;
                    i = (int)(frame_rate) + 1;
                    found_end = true;
                }
            }
            section_start = -1;

            if (!found_end)
            {
                f = (long long)(section_start * frame_rate);
                sceneChangeCommMap[f] = MARK_COMM_END;
            }
        }
    }

    if (section_start >= 0)
        sceneChangeCommMap[framesProcessed] = MARK_COMM_END;

    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;
    QMap<long long, int> deleteMap;

    it = sceneChangeCommMap.begin();
    prev = it;
    if (it != sceneChangeCommMap.end())
    {
        it++;
        while (it != sceneChangeCommMap.end())
        {
            if ((it.data() == MARK_COMM_END) &&
                (it.key() - prev.key()) < (30 * frame_rate))
            {
                deleteMap[it.key()] = 1;
                deleteMap[prev.key()] = 1;
            }
            prev++;
            if (it != sceneChangeCommMap.end())
                it++;
        }

        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
            sceneChangeCommMap.erase(it.key());
    }
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
             ((prev.key() + (15 * frame_rate)) > it.key())) &&
            (prev.data() == MARK_COMM_END) &&
            (it.data() == MARK_COMM_START))
        {
            blankCommBreakMap.erase(prev.key());
            blankCommBreakMap.erase(it.key());
        }
    }

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
        if (((tmpMap_prev.data() + (35 * frame_rate)) > tmpMap_it.key()) &&
            ((tmpMap_prev.data() - tmpMap_prev.key()) > (35 * frame_rate)) &&
            ((tmpMap_it.data() - tmpMap_it.key()) > (35 * frame_rate)))
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

    for (it = map.begin(); it != map.end(); ++it)
    {
        long long frame = it.key();
        int my_fps = (int)ceil(frame_rate);
        int hour = (frame / my_fps) / 60 / 60;
        int min = (frame / my_fps) / 60 - (hour * 60);
        int sec = (frame / my_fps) - (min * 60) - (hour * 60 * 60);
        int frm = frame - ((sec * my_fps) + (min * 60 * my_fps) +
                    (hour * 60 * 60 * my_fps));
        printf( "%7lld : %d (%02d:%02d:%02d.%02d) (%d)\n",
            it.key(), it.data(), hour, min, sec, frm,
            (int)(frame / my_fps));
    }
}
