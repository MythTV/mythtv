#ifndef MYTHOPENGLPERF_H
#define MYTHOPENGLPERF_H

// Qt
#include <QVector>
#include <QOpenGLTimeMonitor>

// MythTV
#include "mythuiexp.h"

class MUI_PUBLIC MythOpenGLPerf : public QOpenGLTimeMonitor
{
  public:
    MythOpenGLPerf(const QString &Name, QVector<QString> Names, int SampleCount = 30);
    void RecordSample    (void);
    void LogSamples      (void);
    int  GetTimersRunning(void);

  private:
    QString m_name                 { };
    int  m_sampleCount             { 0 };
    int  m_totalSamples            { 30 };
    bool m_timersReady             { true };
    int  m_timersRunning           { 0 };
    QVector<GLuint64> m_timerData  { 0 };
    QVector<QString>  m_timerNames { };
};

#endif // MYTHOPENGLPERF_H
