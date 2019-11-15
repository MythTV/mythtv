// MythTV
#include "mythlogging.h"
#include "jitterometer.h"

// Std
#include <cstdlib>
#include <cmath>

#define UNIX_PROC_STAT "/proc/stat"
#define MAX_CORES 8

#ifdef Q_OS_MACOS
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#endif

Jitterometer::Jitterometer(const QString &nname, int ncycles)
  : m_num_cycles(ncycles), m_name(nname)
{
    m_times.resize(m_num_cycles);

    if (m_name.isEmpty())
        m_name = "Jitterometer";

#if defined(__linux__) || defined(Q_OS_ANDROID)
    // N.B. Access to /proc/stat was revoked on Android for API >=26 (Oreo)
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

#ifdef Q_OS_MACOS
    m_laststats = new unsigned long long[MAX_CORES * 2];
    memset(m_laststats, 0, sizeof(unsigned long long) * MAX_CORES * 2);
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
    QString result = "N/A";

#if defined(__linux__) || defined(Q_OS_ANDROID)
    if (m_cpustat)
    {
        m_cpustat->seek(0);
        m_cpustat->flush();

        QByteArray line = m_cpustat->readLine(256);
        if (line.isEmpty())
            return result;

        result = "";
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
                    result += QString("%1% ").arg(load / total * 100, 0, 'f', 0);
                memcpy(&m_laststats[ptr], stats, size);
            }
            line = m_cpustat->readLine(256);
            cores++;
            ptr += 9;
        }
    }
#endif

#ifdef Q_OS_MACOS
    processor_cpu_load_info_t load;
    mach_msg_type_number_t    msgcount;
    natural_t                 processorcount;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &processorcount, (processor_info_array_t *)&load, &msgcount) == KERN_SUCCESS)
    {
        result = "";
        int ptr = 0;
        for (natural_t i = 0; i < processorcount && i < MAX_CORES; i++)
        {
            unsigned long long stats[9];
            memset(stats, 0, sizeof(unsigned long long) * 2);
            stats[0] = load[i].cpu_ticks[CPU_STATE_IDLE];
            stats[1] = load[i].cpu_ticks[CPU_STATE_IDLE] + load[i].cpu_ticks[CPU_STATE_USER] +
                       load[i].cpu_ticks[CPU_STATE_SYSTEM] + load[i].cpu_ticks[CPU_STATE_NICE];
            double idledelta  = stats[0] - m_laststats[ptr];
            double totaldelta = stats[1] - m_laststats[ptr + 1];
            if (totaldelta > 0)
                result += QString("%1% ").arg(((totaldelta - idledelta) / totaldelta) * 100.0, 0, 'f', 0);
            memcpy(&m_laststats[ptr], stats, sizeof(unsigned long long) * 2);
            ptr += 2;
        }
    }
#endif
    return result;
}
