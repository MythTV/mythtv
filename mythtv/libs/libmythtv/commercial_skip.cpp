#include <stdlib.h>

#include <qstringlist.h>

#include "commercial_skip.h"
#include "util.h"

bool CheckFrameIsBlank(unsigned char *buf, int width, int height)
{
    const int pixels_to_check = 500;
    const unsigned int max_brightness = 80;

    int pixels_over_max = 0;
    unsigned int average_Y;
    unsigned int pixel_Y[pixels_to_check];
    unsigned int variance, temp;
    int i,x,y,top,bottom;

    average_Y = variance = temp = 0;
    i = x = y = top = bottom = 0;

    top = (int)(height * .10);
    bottom = (int)(height * .90);

    // get the pixel values
    for (i = 0; i < pixels_to_check; i++)
    {
        x = 10 + (rand() % (width - 10));  // Get away from the edges.
        y = top + (rand() % (bottom - top));
        pixel_Y[i] = buf[y * width + x];

        if (pixel_Y[i] > max_brightness)
        {
            pixels_over_max++;
            if (pixels_over_max > 3)
                return false;
        }

        average_Y += pixel_Y[i];
    }
    average_Y /= pixels_to_check;

    // get the sum of the squared differences
    for (i = 0; i < pixels_to_check; i++)
        temp += (pixel_Y[i] - average_Y) * (pixel_Y[i] - average_Y);

    // get the variance
    variance = (unsigned int)(temp / pixels_to_check);

    if (variance <= 30)
        return true;
    return false;
}

void BuildCommListFromBlanks(QMap<long long, int> &blanks, double fps,
         QMap<long long, int> &commMap)
{
    long long bframes[10240];
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
                (abs((int)(gap_length - (30 * fps))) < 12 ) ||
                (abs((int)(gap_length - (45 * fps))) < 13 ) ||
                (abs((int)(gap_length - (60 * fps))) < 15 ))
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

        commMap[r] = MARK_COMM_START;

        r = c_end[i];
        if( i < (commercials-1))
        {
            for(x=0; x<frames; x++ )
                if (bframes[x] == r)
                    break;
            while(((bframes[x] + 1 ) == bframes[x+1]) &&
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
}

void MergeCommList(QMap<long long, int> &commMap,
        QMap<long long, int> &commBreakMap)
{
    QMap<long long, int>::Iterator it;
    QMap<long long, int>::Iterator prev;

    commBreakMap.clear();

    for (it = commMap.begin(); it != commMap.end(); ++it)
        commBreakMap[it.key()] = it.data();

    it = commMap.begin();
    prev = it;
    it++;
    for(; it != commMap.end(); ++it, ++prev)
    {
        if ((((prev.key() + 1) == it.key()) ||
             ((prev.key() + 300) >= it.key())) &&
            (prev.data() == MARK_COMM_END) &&
            (it.data() == MARK_COMM_START))
        {
            commBreakMap.erase(prev.key());
            commBreakMap.erase(it.key());
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

