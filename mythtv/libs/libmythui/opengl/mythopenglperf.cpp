// C++
#include <utility>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythopenglperf.h"


/*! \class MythOpenGLPerf
 *  \brief A simple overload of QOpenGLTimeMonitor to record and log OpenGL execution intervals
*/
MythOpenGLPerf::MythOpenGLPerf(QString Name,
                               QVector<QString> Names,
                               int SampleCount)
  : m_name(std::move(Name)),
    m_totalSamples(SampleCount),
    m_timerNames(std::move(Names))
{
    while (m_timerData.size() < m_timerNames.size())
        m_timerData.append(0);

    setSampleCount(m_timerNames.size() + 1);
    if (!create())
    {
        LOG(VB_GENERAL, LOG_WARNING, m_name + "Failed to initialise OpenGL timers");
        m_timersReady = false;
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, m_name + QString("Created %1 GL timers- averaging over %2 samples")
        .arg(sampleCount()).arg(m_totalSamples));
}

int MythOpenGLPerf::GetTimersRunning(void) const
{
    return m_timersRunning;
}

void MythOpenGLPerf::RecordSample(void)
{
    if (m_timersReady && isCreated())
    {
        m_timersRunning++;
        recordSample();
    }
}

void MythOpenGLPerf::LogSamples(void)
{
    if (!(isCreated() && isResultAvailable()))
    {
        m_timersReady = false;
        return;
    }

    // retrieve the samples
    QVector<GLuint64> samples = waitForIntervals();

    // ensure we have the correct number
    if (samples.size() != m_timerData.size())
    {
        LOG(VB_GENERAL, LOG_ERR, m_name + "Incorrect sample count %1 %2 %3");
        reset();
        m_timersRunning = 0;
        return;
    }

    // increment our running totals
    for (int i = 0; i < samples.size(); ++i)
        m_timerData[i] += samples[i];
    m_sampleCount++;

    // log and reset when necessary
    if (m_sampleCount > m_totalSamples)
    {
        QStringList results;
        GLuint64 total = 0;
        for (int i = 0; i < m_timerData.size(); ++i)
        {
            results.append(m_timerNames[i] + QString::number((m_timerData[i] / 1000000000.0) / m_sampleCount, '0', 4));
            total += m_timerData[i];
            m_timerData[i] = 0;
        }
        LOG(VB_GPUVIDEO, LOG_INFO, m_name + results.join(" ") +
            QString(" Total fps: %1").arg(1000000000.0 / (static_cast<double>(total) / m_sampleCount)));
        m_sampleCount = 0;
    }

    // clear timers
    m_timersReady = true;
    m_timersRunning = 0;
    reset();
}
