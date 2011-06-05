#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "jitterometer.h"

Jitterometer::Jitterometer(const char *nname, int ncycles)
  : count(0), num_cycles(ncycles), starttime_valid(0)
{
    times = (unsigned*) malloc(ncycles * sizeof(unsigned));
    memset(&starttime, 0, sizeof(struct timeval));

    if (nname)
        name = strdup(nname);
    else
        name = strdup("jitterometer");
}

Jitterometer::~Jitterometer()
{
    free(times);
    free(name);
}

bool Jitterometer::RecordCycleTime()
{
    bool ret = RecordEndTime();
    RecordStartTime();
    return ret;
}

bool Jitterometer::RecordEndTime()
{
    struct timeval timenow;
    gettimeofday(&timenow, NULL);

    if (starttime_valid)
    {
        times[count] = (timenow.tv_sec  - starttime.tv_sec ) * 1000000 +
                       (timenow.tv_usec - starttime.tv_usec) ;
        count++;
    }

    starttime_valid = 0;

    if (count == num_cycles)
    {
        /* compute and display stuff, reset count to -1  */
        double mean = 0, sum_of_squared_deviations=0;
        double standard_deviation;
        double fps = 0, tottime = 0;
        int i;

        /* compute the mean */
        for(i = 0; i < num_cycles; i++)
            mean += times[i];

        tottime = mean;
        mean /= num_cycles;

        fps = num_cycles / tottime * 1000000;

        /* compute the sum of the squares of each deviation from the mean */
        for(i = 0; i < num_cycles; i++)
            sum_of_squared_deviations += (mean - times[i]) * (mean - times[i]);

        /* compute standard deviation */
        standard_deviation = sqrt(sum_of_squared_deviations / (num_cycles - 1));

        printf("'%s' mean = '%.2f', std. dev. = '%.2f', fps = '%.2f'\n", name, mean, standard_deviation, fps);

        count = 0;
        return true;
    }
    return false;
}

void Jitterometer::RecordStartTime()
{
    gettimeofday(&starttime, NULL);
    starttime_valid = 1;
}
