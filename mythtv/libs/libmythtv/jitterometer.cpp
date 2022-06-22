// MythTV
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythlogging.h"
#include "jitterometer.h"

// Std
#include <array>
#include <cmath>
#include <cstdlib>
#include <utility>

#if defined(__linux__) || defined(Q_OS_ANDROID)
static constexpr const char* UNIX_PROC_STAT { "/proc/stat" };
#endif
#if defined(__linux__) || defined(Q_OS_ANDROID) || defined(Q_OS_MACOS)
static constexpr size_t MAX_CORES { 8 };
#endif

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
    auto timenow = nowAsDuration<std::chrono::microseconds>();

    if (m_starttime > 0ms)
    {
        m_times[m_count] = timenow - m_starttime;
        m_count++;
    }

    m_starttime = -1us;

    if (m_count >= cycles)
    {
        /* compute and display stuff, reset count to -1  */

        /* compute the mean */
        std::chrono::microseconds tottime = std::accumulate(m_times.cbegin(), m_times.cend(), 0us);
        using doublemics = std::chrono::duration<double, std::micro>;
        doublemics mean = duration_cast<doublemics>(tottime) / cycles;

        if (tottime > 0us)
            m_lastFps = cycles * (1000000.0F / tottime.count());

        /* compute the sum of the squares of each deviation from the mean */
        double sum_of_squared_deviations =
            std::accumulate(m_times.cbegin(), m_times.cend(), 0.0,
                            [mean](double sum, std::chrono::microseconds time)
                                {double delta = (mean - duration_cast<doublemics>(time)).count();
                                    return sum + (delta * delta); });

        /* compute standard deviation */
        double standard_deviation = sqrt(sum_of_squared_deviations / (cycles - 1));
        if (mean > 0us)
            m_lastSd = standard_deviation / mean.count();

        /* retrieve load if available */
        QString extra;
        m_lastCpuStats = GetCPUStat();
        if (!m_lastCpuStats.isEmpty())
            extra = QString("CPUs: ") + m_lastCpuStats;

        LOG(VB_GENERAL, LOG_INFO,
            m_name + QString("FPS: %1 Mean: %2 Std.Dev: %3 ")
                .arg(m_lastFps, 7, 'f', 2).arg((int)mean.count(), 5)
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
    m_starttime = nowAsDuration<std::chrono::microseconds>();
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
        size_t cores = 0;
        int ptr   = 0;
        line = m_cpuStat->readLine(256);
        while (!line.isEmpty() && cores < MAX_CORES)
        {
            std::array<unsigned long long,9> stats {};
            int num = 0;
            if (sscanf(line.constData(),
                       "cpu%30d %30llu %30llu %30llu %30llu %30llu "
                       "%30llu %30llu %30llu %30llu %*5000s\n",
                       &num, stats.data(), &stats[1], &stats[2], &stats[3],
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
                std::copy(stats.cbegin(), stats.cend(), &m_lastStats[ptr]);
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
            std::array<unsigned long long,2> stats {};
            stats[0] = load[i].cpu_ticks[CPU_STATE_IDLE];
            stats[1] = load[i].cpu_ticks[CPU_STATE_IDLE] + load[i].cpu_ticks[CPU_STATE_USER] +
                       load[i].cpu_ticks[CPU_STATE_SYSTEM] + load[i].cpu_ticks[CPU_STATE_NICE];
            double idledelta  = stats[0] - m_lastStats[ptr];
            double totaldelta = stats[1] - m_lastStats[ptr + 1];
            if (totaldelta > 0)
                result += QString("%1% ").arg(((totaldelta - idledelta) / totaldelta) * 100.0, 0, 'f', 0);
            std::copy(stats.cbegin(), stats.cend(), &m_lastStats[ptr]);
            ptr += 2;
        }
    }
#endif
    return result;
}
