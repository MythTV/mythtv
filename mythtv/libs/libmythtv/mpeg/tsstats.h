// -*- Mode: c++ -*-
// This file, "tsstats.h" is in the public domain, written by Daniel Kristjansson, 2004 CE
#ifndef __TS_STATS__
#define __TS_STATS__

#include <qstring.h>
#include <qmap.h>

/** \class TSStats
 *  \brief Collects statistics on the number of TSPacket's seen on each PID.
 *
 *  \sa TSPacket, HDTVRecorder
 */
class TSStats
{
  public:
    TSStats() : _tspacket_count(0) { ; }
    void IncrPIDCount(int pid)  { _pid_counts[pid]++;  }
    void IncrTSPacketCount() { _tspacket_count++; }
    long long TSPacketCount() { return _tspacket_count; }
    void Reset() { _tspacket_count = 0; _pid_counts.clear(); }
    inline QString toString();
  private:
    long long _tspacket_count;
    QMap<int, long long> _pid_counts;
};

inline QString TSStats::toString() {
    QString str("Transport Stream Statistics\n");
    str.append(QString("TSPacket Count: %1").arg((long)_tspacket_count));
    QMapIterator<int, long long> it = _pid_counts.begin();
    for (; it != _pid_counts.end(); it++)
        str.append(QString("\nPID 0x%1 Count: %2").
                   arg((int)it.key(),0,16).arg((long)it.data(),10,10));
    return str;
}

#endif // __TS_STATS__
