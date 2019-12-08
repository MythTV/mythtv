// MythTV
#include "mythlogging.h"
#include "jitterometer.h"

// Std
#include <cmath>
#include <cstdlib>
#include <utility>

#define UNIX_PROC_STAT "/proc/stat"
#define MAX_CORES 8

#ifdef Q_OS_MACOS
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#endif

Jitterometer::Jitterometer(QString nname, int ncycles)
  : m_numCycles(ncycles), m_name(std::move(nname))
{
    m_times.resize(m_numCycles);

    if (m_name.isEmpty())
        m_name = "Jitterometer";

#if defined(__linux__) || defined(Q_OS_ANDROID)
    // N.B. Access to /proc/stat was revoked on Android for API >=26 (Oreo)
    if (QFile::exists(UNIX_PROC_STAT))
    {
        m_cpuStat = new QFile(UNIX_PROC_STAT);
        if (m_cpuStat)
        {
            if (!m_cpuStat->open(QIODevice::ReadOnly))
            {
                delete m_cpuStat;
                m_cpuStat = nullptr;
            }
            else
            {
                m_lastStats = new unsigned long long[MAX_CORES * 9];
                memset(m_lastStats, 0, sizeof(unsigned long long) * MAX_CORES * 9);
            }
        }
    }
#endif

#ifdef Q_OS_MACOS
    m_lastStats = new unsigned long long[MAX_CORES * 2];
    memset(m_lastStats, 0, sizeof(unsigned long long) * MAX_CORES * 2);
#endif
}

Jitterometer::~Jitterometer()
{
    if (m_cpuStat)
        m_cpuStat->close();
    delete m_cpuStat;
    delete [] m_lastStats;
}

void Jitterometer::SetNumCycles(int cycles)
{
    m_numCycles = cycles;
    m_times.resize(m_numCycles);
    m_count = 0;
}

bool Jitterometer::RecordCycleTime()
{
    if (!m_numCycles)
        return false;
    bool ret = RecordEndTime();
    RecordStartTime();
    return ret;
}

bool Jitterometer::RecordEndTime()
{
    if (!m_numCycles)
        return false;

    int cycles = m_numCycles;
    struct timeval timenow {};
    gettimeofday(&timenow, nullptr);

    if (m_starttimeValid)
    {
        m_times[m_count] = (timenow.tv_sec  - m_starttime.tv_sec ) * 1000000 +
                           (timenow.tv_usec - m_starttime.tv_usec) ;
        m_count++;
    }

    m_starttimeValid = false;

    if (m_count >= cycles)
    {
        /* compute and display stuff, reset count to -1  */
        double mean = 0;
        double sum_of_squared_deviations=0;

        /* compute the mean */
        for (int i = 0; i < cycles; i++)
            mean += m_times[i];

        double tottime = mean;
        mean /= cycles;

        if (tottime > 0)
            m_lastFps = cycles / tottime * 1000000;

        /* compute the sum of the squares of each deviation from the mean */
        for (int i = 0; i < cycles; i++)
            sum_of_squared_deviations += (mean - m_times[i]) * (mean - m_times[i]);

        /* compute standard deviation */
        double standard_deviation = sqrt(sum_of_squared_deviations / (cycles - 1));
        if (mean > 0)
            m_lastSd = standard_deviation / mean;

        /* retrieve load if available */
        QString extra;
        m_lastCpuStats = GetCPUStat();
        if (!m_lastCpuStats.isEmpty())
            extra = QString("CPUs: ") + m_lastCpuStats;

        LOG(VB_GENERAL, LOG_INFO,
            m_name + QString("FPS: %1 Mean: %2 Std.Dev: %3 ")
                .arg(m_lastFps, 7, 'f', 2).arg((int)mean, 5)
                .arg((int)standard_deviation, 5) + extra);

        m_count = 0;
        return true;
    }
    return false;
}

void Jitterometer::RecordStartTime()
{
    if (!m_numCycles)
        return;
    gettimeofday(&m_starttime, nullptr);
    m_starttimeValid = true;
}

QString Jitterometer::GetCPUStat(void)
{
    QString result = "N/A";

#if defined(__linux__) || defined(Q_OS_ANDROID)
    if (m_cpuStat)
    {
        m_cpuStat->seek(0);
        m_cpuStat->flush();

        QByteArray line = m_cpuStat->readLine(256);
        if (line.isEmpty())
            return result;

        result = "";
        int cores = 0;
        int ptr   = 0;
        line = m_cpuStat->readLine(256);
        while (!line.isEmpty() && cores < MAX_CORES)
        {
            static constexpr int kSize = sizeof(unsigned long long) * 9;
            unsigned long long stats[9];
            memset(stats, 0, kSize);
            int num = 0;
            if (sscanf(line.constData(),
                       "cpu%30d %30llu %30llu %30llu %30llu %30llu "
                       "%30llu %30llu %30llu %30llu %*5000s\n",
                       &num, &stats[0], &stats[1], &stats[2], &stats[3],
                       &stats[4], &stats[5], &stats[6], &stats[7], &stats[8]) >= 4)
            {
                float load  = stats[0] + stats[1] + stats[2] + stats[4] +
                              stats[5] + stats[6] + stats[7] + stats[8] -
                              m_lastStats[ptr + 0] - m_lastStats[ptr + 1] -
                              m_lastStats[ptr + 2] - m_lastStats[ptr + 4] -
                              m_lastStats[ptr + 5] - m_lastStats[ptr + 6] -
                              m_lastStats[ptr + 7] - m_lastStats[ptr + 8];
                float total = load + stats[3] - m_lastStats[ptr + 3];
                if (total > 0)
                    result += QString("%1% ").arg(load / total * 100, 0, 'f', 0);
                memcpy(&m_lastStats[ptr], stats, kSize);
            }
            line = m_cpuStat->readLine(256);
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
            double idledelta  = stats[0] - m_lastStats[ptr];
            double totaldelta = stats[1] - m_lastStats[ptr + 1];
            if (totaldelta > 0)
                result += QString("%1% ").arg(((totaldelta - idledelta) / totaldelta) * 100.0, 0, 'f', 0);
            memcpy(&m_lastStats[ptr], stats, sizeof(unsigned long long) * 2);
            ptr += 2;
        }
    }
#endif
    return result;
}
