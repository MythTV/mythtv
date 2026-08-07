#ifndef MYTHOPENGLPERF_H
#define MYTHOPENGLPERF_H

// Qt
#include <QVector>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QOpenGLTimeMonitor>
#else
#include <QtOpenGL/QOpenGLTimeMonitor>
#endif

// QOpenGLTimeMonitor is not available with Qt GLES2 builds
#if defined(QT_OPENGL_ES_2) || (QT_FEATURE_opengles2 == 1)

#ifndef GLuint64
using GLuint64 = uint64_t;
#endif

// This defines the Qt QOpenGLTimeMonitor class (for those systems
// whose Qt doesn't have it).  This has to match the Qt definition of
// the class.
//
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class QOpenGLTimeMonitor
{
  public:
    QOpenGLTimeMonitor() = default;
    void setSampleCount(int /*count*/) {};
    int  sampleCount() { return 0; };
    bool create() { return false; };
    bool isCreated() { return false; };
    int  recordSample() { return 0; };
    bool isResultAvailable() { return false; };
    QVector<GLuint64> waitForIntervals() { return {}; };
    void reset() {};
};
// NOLINTEND(readability-convert-member-functions-to-static)
#endif

// MythTV
#include "libmythui/mythuiexp.h"

class MUI_PUBLIC MythOpenGLPerf : public QOpenGLTimeMonitor
{
  public:
    MythOpenGLPerf(QString Name, QVector<QString> Names, int SampleCount = 30);
    void RecordSample    (void);
    void LogSamples      (void);
    int  GetTimersRunning(void) const;

  private:
    QString m_name;
    int  m_sampleCount             { 0 };
    int  m_totalSamples            { 30 };
    bool m_timersReady             { true };
    int  m_timersRunning           { 0 };
    QVector<GLuint64> m_timerData  { 0 };
    QVector<QString>  m_timerNames;
};

#endif // MYTHOPENGLPERF_H
