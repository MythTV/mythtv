#include <stdlib.h>

#include <qstringlist.h>

#include "commercial_skip.h"
#include "util.h"

bool CheckFrameIsBlank(unsigned char *buf, int width, int height)
{
    const unsigned int max_brightness = 120;
    const unsigned int test_brightness = 80;
    bool line_checked[height];
    bool test = false;

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
        int avg = GetFrameAvgBrightness(buf, width, height);

        if (avg > 35)
            return(false);
    }

    return(true);
}

int GetFrameAvgBrightness(unsigned char *buf, int width, int height)
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

void BuildCommListFromBlanks(QMap<long long, int> &blanks, double fps,
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
            if ((abs((int)(gap_length - (15 * fps))) < 10 ) ||
                (abs((int)(gap_length - (20 * fps))) < 11 ) ||
                (abs((int)(gap_length - (30 * fps))) < 12 ) ||
                (abs((int)(gap_length - (45 * fps))) < 13 ) ||
                (abs((int)(gap_length - (60 * fps))) < 15 ) ||
                (abs((int)(gap_length - (90 * fps))) < 10 ))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x] - 1;
                commercials++;
                i = x-1;
                x = frames;
            }
        }
    }

    // eliminate any blank frames at end of commercials
    for(i = 0; i < (commercials-1); i++ )
    {
        long long int r = c_start[i];

        if (r < (30 * fps)) 
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

void MergeCommList(QMap<long long, int> &commMap, double fps,
        QMap<long long, int> &commBreakMap)
{
    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;
    QMap<long long, long long> tmpMap;
    QMap<long long, long long>::Iterator tmpMap_it;
    QMap<long long, long long>::Iterator tmpMap_prev;

    commBreakMap.clear();

    for (it = commMap.begin(); it != commMap.end(); ++it)
        commBreakMap[it.key()] = it.data();

    it = commMap.begin();
    prev = it;
    it++;
    for(; it != commMap.end(); ++it, ++prev)
    {
        // if next commercial starts less than 15*fps frames away then merge
        if ((((prev.key() + 1) == it.key()) ||
             ((prev.key() + (15 * fps)) > it.key())) &&
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
        if (((tmpMap_prev.data() + (35 * fps)) > tmpMap_it.key()) &&
            ((tmpMap_prev.data() - tmpMap_prev.key()) > (35 * fps)) &&
            ((tmpMap_it.data() - tmpMap_it.key()) > (35 * fps)))
        {
            commBreakMap.erase(tmpMap_prev.data());
            commBreakMap.erase(tmpMap_it.key());
        }
    }
}

void DeleteCommAtFrame(QMap<long long, int> &commMap, long long frame)
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

