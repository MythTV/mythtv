#include <stdlib.h>

#include <qstringlist.h>

#include "commercial_skip.h"
#include "util.h"

bool CheckFrameIsBlank(unsigned char *buf, int width, int height)
{
    const int pixels_to_check = 200;
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

    for (it = blanks.begin(); it != blanks.end(); ++it)
		bframes[frames++] = it.key();

	if (frames == 0)
		return;

	for(i=0, x=0; i < frames; i++ )
	{
		if ((( bframes[i] - bframes[i-1] ) == 1 ) ||
			(( bframes[i] - bframes[i-1] ) > (10 * fps)))
		{
			bframes[x++] = bframes[i];
		}
	}
	frames = x;

    for(i = 1; i < frames; i++ )
    {
        for(x=i; x < frames; x++ )
        {
            /* check 15, 20, 30, and 60 second spots */
			int gap_length = bframes[x] - bframes[i];
            if ((abs((int)(gap_length - (15 * fps))) < 10 ) ||
                (abs((int)(gap_length - (20 * fps))) < 10 ) ||
                (abs((int)(gap_length - (30 * fps))) < 10 ) ||
                (abs((int)(gap_length - (60 * fps))) < 10 ))
            {
                c_start[commercials] = bframes[i];
                c_end[commercials] = bframes[x];
                commercials++;
                x = frames;
                while(( bframes[i] + 1 ) == bframes[i+1] )
                    i++;
            }
        }
    }

    for(i = 0; i < (commercials-1); i++ )
    {
		long long int r = c_start[i];
		for(x=0; x<frames; x++ )
			if (bframes[x] == r)
				break;
		while((x>0) && ((bframes[x]-1) == bframes[x-1]))
		{
			r--;
			x--;
		}

		commMap[r] = MARK_COMM_START;
        while((( c_end[i] == c_start[i+1] ) ||
               (( c_end[i] + 450 ) > c_start[i+1] )) &&
              ( i < (commercials-1)))
            i++;

		r = c_end[i];
		for(x=0; x<frames; x++ )
			if (bframes[x] == r)
				break;
		while((bframes[x] + 1 ) == bframes[x+1])
		{
			r++;
			x++;
		}
		commMap[r] = MARK_COMM_END;
    }
}

