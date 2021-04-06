// -*- Mode: c++ -*-
#ifndef RECORDING_QUALITY_H
#define RECORDING_QUALITY_H

#include <QDateTime>
#include <QList>
#include <utility>


#include "mythtvexp.h"

class RecordingInfo;

class RecordingGap
{
  public:
    RecordingGap(QDateTime start, QDateTime end) :
        m_start(std::move(start)), m_end(std::move(end)) { }
    QDateTime GetStart(void) const { return m_start; }
    QDateTime GetEnd(void) const { return m_end; }
    QString toString(void) const
    {
        return QString("<<%1 to %2>>")
            .arg(m_start.toString(Qt::ISODate),
                 m_end.toString(Qt::ISODate));
    }
    bool operator<(const RecordingGap &o) const { return m_start < o.m_start; }
  private:
    QDateTime m_start;
    QDateTime m_end;
};
using RecordingGaps = QList<RecordingGap>;

class MTV_PUBLIC RecordingQuality
{
  public:
    RecordingQuality(const RecordingInfo *ri,
                     RecordingGaps rg);
    RecordingQuality(
        const RecordingInfo *ri, RecordingGaps rg,
        const QDateTime &firstData, const QDateTime &latestData);

    void AddTSStatistics(int continuity_error_count, int packet_count);
    bool IsDamaged(void) const;
    QString toStringXML(void) const;

  private:
    int           m_continuityErrorCount {0};
    int           m_packetCount          {0};
    QString       m_programKey;
    double        m_overallScore         {1.0};
    RecordingGaps m_recordingGaps;
};

#endif // RECORDING_QUALITY_H
