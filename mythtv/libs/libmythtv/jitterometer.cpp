#include <sys/time.h>
#include <cstdlib>
#include <cmath>

#include "mythverbose.h"
#include "jitterometer.h"

Jitterometer::Jitterometer(QString nname, int ncycles)
  : count(0), num_cycles(ncycles), starttime_valid(0), last_fps(0), name(nname)
{
    times = (unsigned*) malloc(num_cycles * sizeof(unsigned));
    memset(&starttime, 0, sizeof(struct timeval));

    if (name.isEmpty())
        name = "Jitterometer";
}

Jitterometer::~Jitterometer()
{
    free(times);
}

void Jitterometer::SetNumCycles(int cycles)
{
    num_cycles = cycles;
    times = (unsigned*) realloc(times, num_cycles * sizeof(unsigned));
}

bool Jitterometer::RecordCycleTime()
{
    if (!num_cycles)
        return false;
    bool ret = RecordEndTime();
    RecordStartTime();
    return ret;
}

bool Jitterometer::RecordEndTime()
{
    if (!num_cycles)
        return false;

    int cycles = num_cycles;
    struct timeval timenow;
    gettimeofday(&timenow, NULL);

    if (starttime_valid)
    {
        times[count] = (timenow.tv_sec  - starttime.tv_sec ) * 1000000 +
                       (timenow.tv_usec - starttime.tv_usec) ;
        count++;
    }

    starttime_valid = 0;

    if (count >= cycles)
    {
        /* compute and display stuff, reset count to -1  */
        double mean = 0, sum_of_squared_deviations=0;
        double standard_deviation;
        double tottime = 0;
        int i;

        /* compute the mean */
        for(i = 0; i < cycles; i++)
            mean += times[i];

        tottime = mean;
        mean /= cycles;

        last_fps = cycles / tottime * 1000000;

        /* compute the sum of the squares of each deviation from the mean */
        for(i = 0; i < cycles; i++)
            sum_of_squared_deviations += (mean - times[i]) * (mean - times[i]);

        /* compute standard deviation */
        standard_deviation = sqrt(sum_of_squared_deviations / (cycles - 1));

        VERBOSE(VB_PLAYBACK, name + QString("Mean: %1 Std.Dev: %2 fps: %3")
            .arg((int)mean).arg((int)standard_deviation).arg(last_fps, 0, 'f', 2));

        count = 0;
        return true;
    }
    return false;
}

void Jitterometer::RecordStartTime()
{
    if (!num_cycles)
        return;
    gettimeofday(&starttime, NULL);
    starttime_valid = 1;
}
