#include <sys/time.h>
#include <cstdlib>
#include <cmath>

#include "mythlogging.h"
#include "jitterometer.h"

#define UNIX_PROC_STAT "/proc/stat"
#define MAX_CORES 8

Jitterometer::Jitterometer(const QString &nname, int ncycles)
  : count(0), num_cycles(ncycles), starttime_valid(0), last_fps(0),
    last_sd(0), name(nname), cpustat(NULL), laststats(NULL)
{
    times.resize(num_cycles);
    memset(&starttime, 0, sizeof(struct timeval));

    if (name.isEmpty())
        name = "Jitterometer";

#ifdef __linux__
    if (QFile::exists(UNIX_PROC_STAT))
    {
        cpustat = new QFile(UNIX_PROC_STAT);
        if (cpustat)
        {
            if (!cpustat->open(QIODevice::ReadOnly))
            {
                delete cpustat;
                cpustat = NULL;
            }
            else
            {
                laststats = new unsigned long long[MAX_CORES * 9];
                memset(laststats, 0, sizeof(unsigned long long) * MAX_CORES * 9);
            }
        }
    }
#endif
}

Jitterometer::~Jitterometer()
{
    if (cpustat)
        cpustat->close();
    delete cpustat;
    delete [] laststats;
}

void Jitterometer::SetNumCycles(int cycles)
{
    num_cycles = cycles;
    times.resize(num_cycles);
    count = 0;
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

        if (tottime > 0)
            last_fps = cycles / tottime * 1000000;

        /* compute the sum of the squares of each deviation from the mean */
        for(i = 0; i < cycles; i++)
            sum_of_squared_deviations += (mean - times[i]) * (mean - times[i]);

        /* compute standard deviation */
        standard_deviation = sqrt(sum_of_squared_deviations / (cycles - 1));
        if (mean > 0)
            last_sd = standard_deviation / mean;

        /* retrieve load if available */
        QString extra;
        lastcpustats = GetCPUStat();
        if (!lastcpustats.isEmpty())
            extra = QString("CPUs: ") + lastcpustats;

        LOG(VB_GENERAL, LOG_INFO,
            name + QString("FPS: %1 Mean: %2 Std.Dev: %3 ")
                .arg(last_fps, 7, 'f', 2).arg((int)mean, 5)
                .arg((int)standard_deviation, 5) + extra);

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

QString Jitterometer::GetCPUStat(void)
{
    if (!cpustat)
        return "N/A";

#ifdef __linux__
    QString res;
    cpustat->seek(0);
    cpustat->flush();

    QByteArray line = cpustat->readLine(256);
    if (line.isEmpty())
        return res;

    int cores = 0;
    int ptr   = 0;
    line = cpustat->readLine(256);
    while (!line.isEmpty() && cores < MAX_CORES)
    {
        static const int size = sizeof(unsigned long long) * 9;
        unsigned long long stats[9];
        memset(stats, 0, size);
        int num = 0;
        if (sscanf(line.constData(),
                   "cpu%30d %30llu %30llu %30llu %30llu %30llu "
                   "%30llu %30llu %30llu %30llu %*5000s\n",
                   &num, &stats[0], &stats[1], &stats[2], &stats[3],
                   &stats[4], &stats[5], &stats[6], &stats[7], &stats[8]) >= 4)
        {
            float load  = stats[0] + stats[1] + stats[2] + stats[4] +
                          stats[5] + stats[6] + stats[7] + stats[8] -
                          laststats[ptr + 0] - laststats[ptr + 1] -
                          laststats[ptr + 2] - laststats[ptr + 4] -
                          laststats[ptr + 5] - laststats[ptr + 6] -
                          laststats[ptr + 7] - laststats[ptr + 8];
            float total = load + stats[3] - laststats[ptr + 3];
            if (total > 0)
                res += QString("%1% ").arg(load / total * 100, 0, 'f', 0);
            memcpy(&laststats[ptr], stats, size);
        }
        line = cpustat->readLine(256);
        cores++;
        ptr += 9;
    }
    return res;
#else
    return "N/A";
#endif
}
