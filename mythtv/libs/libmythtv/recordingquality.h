// -*- Mode: c++ -*-
#ifndef _RECORDER_QUALITY_H_
#define _RECORDER_QUALITY_H_

#include <QDateTime>
#include <QList>

#include "mythtvexp.h"

class RecordingInfo;

class RecordingGap
{
  public:
    RecordingGap(const QDateTime &start, const QDateTime &end) :
        m_start(start), m_end(end) { }
    QDateTime GetStart(void) const { return m_start; }
    QDateTime GetEnd(void) const { return m_end; }
    QString toString(void) const
    {
        return QString("<<%1 to %2>>")
            .arg(m_start.toString(Qt::ISODate))
            .arg(m_end.toString(Qt::ISODate));
    }
    bool operator<(const RecordingGap &o) const { return m_start < o.m_start; }
  private:
    QDateTime m_start;
    QDateTime m_end;
};
typedef QList<RecordingGap> RecordingGaps;

class MTV_PUBLIC RecordingQuality
{
  public:
    RecordingQuality(const RecordingInfo *ri,
                     const RecordingGaps &rg);
    RecordingQuality(
        const RecordingInfo*, const RecordingGaps&,
        const QDateTime &firstData, const QDateTime &latestData);

    void AddTSStatistics(int continuity_error_count, int packet_count);
    bool IsDamaged(void) const;
    QString toStringXML(void) const;

  private:
    int           m_continuity_error_count;
    int           m_packet_count;
    QString       m_program_key;
    double        m_overall_score;
    RecordingGaps m_recording_gaps;
};

#endif // _RECORDER_QUALITY_H_
