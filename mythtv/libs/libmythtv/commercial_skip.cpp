#include <stdlib.h>

#include <qstringlist.h>

#include "commercial_skip.h"
#include "programinfo.h"
#include "util.h"

CommDetect::CommDetect(int w, int h, double fps)
{
    width = w;
    height = h;
    frame_rate = fps;

    sceneChangeTolerance = 0.95;

    lastFrameWasBlank = false;
    lastFrame = NULL;
    memset(lastHistogram, 0, sizeof(lastHistogram));
}

CommDetect::~CommDetect(void)
{
    if (lastFrame)
        delete [] lastFrame;
}

void CommDetect::ReInit(int w, int h, double fps)
{
    if (lastFrame)
        delete [] lastFrame;

    CommDetect(w, h, fps);
}

bool CommDetect::FrameIsBlank(unsigned char *buf)
{
    const unsigned int max_brightness = 120;
    const unsigned int test_brightness = 80;
    bool line_checked[height];
    bool test = false;

    lastFrameWasBlank = false;

    for(int y = 0; y < height; y++)
        line_checked[y] = false;

    for(int divisor = 3; divisor < 200; divisor += 3)
    {
        int y_mult = (int)(height / divisor);
        int x_mult = (int)(width / divisor);
        for(int y = y_mult; y < height; y += y_mult)
        {
            if (line_checked[y])
                continue;

            for(int x = x_mult; x < width; x += x_mult)
            {
                if (buf[y * width + x] > max_brightness)
                    return(false);
                if (buf[y * width + x] > test_brightness)
                    test = true;
            }
            line_checked[y] = true;
        }
    }

    // frame is dim so test average
    if (test)
    {
        int avg = GetAvgBrightness(buf);

        if (avg > 35)
            return(false);
    }

    lastFrameWasBlank = true;

    return(true);
}

bool CommDetect::SceneChanged(unsigned char *buf)
{
    int histogram[256];

    if (!lastFrame)
    {
        // first frame so copy to buffer and return
        lastFrame = new unsigned char[width * height];

        if (lastFrame)
        {
            memcpy(lastFrame, buf, width * height);
            memset(lastHistogram, 0, sizeof(lastHistogram));
            for(int i = 0; i < (width * height); i++)
                lastHistogram[buf[i]]++;
        }
        else
            return(false);

        return(false);
    }

    // compare current frame with last frame here
    memset(histogram, 0, sizeof(histogram));
    for(int i = 0; i < (width * height); i++)
        histogram[buf[i]]++;

    if (lastFrameWasSceneChange)
    {
        lastFrameWasSceneChange = false;
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

    if (similar < (width * height * .91))
    {
        lastFrameWasSceneChange = true;
        memcpy(lastHistogram, histogram, sizeof(histogram));
        return(true);
    }

    lastFrameWasSceneChange = false;
    memcpy(lastHistogram, histogram, sizeof(histogram));
    return(false);
}

int CommDetect::GetAvgBrightness(unsigned char *buf)
{
    int brightness = 0;
    int pixels = 0;

    for(int y = 0; y < height; y += 4)
        for(int x = 0; x < width; x += 4)
        {
            brightness += buf[y * width + x];
            pixels++;
        }

    return(brightness/pixels);
}

void CommDetect::BuildCommListFromBlanks(QMap<long long, int> &blanks,
                                         QMap<long long, int> &commMap)
{
    long long bframes[blanks.count()*2];
    long long c_start[512];
    long long c_end[512];
    int frames = 0;
    int commercials = 0;
    int i, x;
    QMap<long long, int>::Iterator it;

    commMap.clear();

    for (it = blanks.begin(); it != blanks.end(); ++it)
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
    for(; i < (commercials-1); i++)
    {
        long long int r = c_start[i];

        if (r < (30 * frame_rate)) 
            r = 1;

        commMap[r] = MARK_COMM_START;

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

            while((blanks.contains(r+1)) &&
                  (c_start[i+1] != (r+1)))
                r++;
        }
        else
        {
            while(blanks.contains(r+1))
                r++;
        }

        commMap[r] = MARK_COMM_END;
    }
    commMap[c_start[i]] = MARK_COMM_START;
    commMap[c_end[i]] = MARK_COMM_END;
}

void CommDetect::MergeCommList(QMap<long long, int> &commMap,
                               QMap<long long, int> &commBreakMap)
{
    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;
    QMap<long long, long long> tmpMap;
    QMap<long long, long long>::Iterator tmpMap_it;
    QMap<long long, long long>::Iterator tmpMap_prev;

    commBreakMap.clear();

    if (commMap.isEmpty())
        return;

    for (it = commMap.begin(); it != commMap.end(); ++it)
        commBreakMap[it.key()] = it.data();

    if (commBreakMap.isEmpty())
        return;

    it = commMap.begin();
    prev = it;
    it++;
    for(; it != commMap.end(); ++it, ++prev)
    {
        // if next commercial starts less than 15*fps frames away then merge
        if ((((prev.key() + 1) == it.key()) ||
             ((prev.key() + (15 * frame_rate)) > it.key())) &&
            (prev.data() == MARK_COMM_END) &&
            (it.data() == MARK_COMM_START))
        {
            commBreakMap.erase(prev.key());
            commBreakMap.erase(it.key());
        }
    }

    it = commBreakMap.begin();
    prev = it;
    it++;
    tmpMap[prev.key()] = it.key();
    for(; it != commBreakMap.end(); ++it, ++prev)
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
            commBreakMap.erase(tmpMap_prev.data());
            commBreakMap.erase(tmpMap_it.key());
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

