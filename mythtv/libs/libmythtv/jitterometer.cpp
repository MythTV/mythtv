#include <sys/time.h>
#include <cstdlib>
#include <cmath>

#include "mythlogging.h"
#include "jitterometer.h"

#define UNIX_PROC_STAT "/proc/stat"
#define MAX_CORES 8

Jitterometer::Jitterometer(const QString &nname, int ncycles)
  : m_num_cycles(ncycles), m_name(nname)
{
    m_times.resize(m_num_cycles);

    if (m_name.isEmpty())
        m_name = "Jitterometer";

#ifdef __linux__
    if (QFile::exists(UNIX_PROC_STAT))
    {
        m_cpustat = new QFile(UNIX_PROC_STAT);
        if (m_cpustat)
        {
            if (!m_cpustat->open(QIODevice::ReadOnly))
            {
                delete m_cpustat;
                m_cpustat = nullptr;
            }
            else
            {
                m_laststats = new unsigned long long[MAX_CORES * 9];
                memset(m_laststats, 0, sizeof(unsigned long long) * MAX_CORES * 9);
            }
        }
    }
#endif
}

Jitterometer::~Jitterometer()
{
    if (m_cpustat)
        m_cpustat->close();
    delete m_cpustat;
    delete [] m_laststats;
}

void Jitterometer::SetNumCycles(int cycles)
{
    m_num_cycles = cycles;
    m_times.resize(m_num_cycles);
    m_count = 0;
}

bool Jitterometer::RecordCycleTime()
{
    if (!m_num_cycles)
        return false;
    bool ret = RecordEndTime();
    RecordStartTime();
    return ret;
}

bool Jitterometer::RecordEndTime()
{
    if (!m_num_cycles)
        return false;

    int cycles = m_num_cycles;
    struct timeval timenow {};
    gettimeofday(&timenow, nullptr);

    if (m_starttime_valid)
    {
        m_times[m_count] = (timenow.tv_sec  - m_starttime.tv_sec ) * 1000000 +
                           (timenow.tv_usec - m_starttime.tv_usec) ;
        m_count++;
    }

    m_starttime_valid = false;

    if (m_count >= cycles)
    {
        /* compute and display stuff, reset count to -1  */
        double mean = 0, sum_of_squared_deviations=0;
        double standard_deviation;
        double tottime = 0;
        int i;

        /* compute the mean */
        for(i = 0; i < cycles; i++)
            mean += m_times[i];

        tottime = mean;
        mean /= cycles;

        if (tottime > 0)
            m_last_fps = cycles / tottime * 1000000;

        /* compute the sum of the squares of each deviation from the mean */
        for(i = 0; i < cycles; i++)
            sum_of_squared_deviations += (mean - m_times[i]) * (mean - m_times[i]);

        /* compute standard deviation */
        standard_deviation = sqrt(sum_of_squared_deviations / (cycles - 1));
        if (mean > 0)
            m_last_sd = standard_deviation / mean;

        /* retrieve load if available */
        QString extra;
        m_lastcpustats = GetCPUStat();
        if (!m_lastcpustats.isEmpty())
            extra = QString("CPUs: ") + m_lastcpustats;

        LOG(VB_GENERAL, LOG_INFO,
            m_name + QString("FPS: %1 Mean: %2 Std.Dev: %3 ")
                .arg(m_last_fps, 7, 'f', 2).arg((int)mean, 5)
                .arg((int)standard_deviation, 5) + extra);

        m_count = 0;
        return true;
    }
    return false;
}

void Jitterometer::RecordStartTime()
{
    if (!m_num_cycles)
        return;
    gettimeofday(&m_starttime, nullptr);
    m_starttime_valid = true;
}

QString Jitterometer::GetCPUStat(void)
{
    if (!m_cpustat)
        return "N/A";

#ifdef __linux__
    QString res;
    m_cpustat->seek(0);
    m_cpustat->flush();

    QByteArray line = m_cpustat->readLine(256);
    if (line.isEmpty())
        return res;

    int cores = 0;
    int ptr   = 0;
    line = m_cpustat->readLine(256);
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
                          m_laststats[ptr + 0] - m_laststats[ptr + 1] -
                          m_laststats[ptr + 2] - m_laststats[ptr + 4] -
                          m_laststats[ptr + 5] - m_laststats[ptr + 6] -
                          m_laststats[ptr + 7] - m_laststats[ptr + 8];
            float total = load + stats[3] - m_laststats[ptr + 3];
            if (total > 0)
                res += QString("%1% ").arg(load / total * 100, 0, 'f', 0);
            memcpy(&m_laststats[ptr], stats, size);
        }
        line = m_cpustat->readLine(256);
        cores++;
        ptr += 9;
    }
    return res;
#else
    return "N/A";
#endif
}
