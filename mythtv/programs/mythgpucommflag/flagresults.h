#ifndef _FLAGRESULTS_H
#define _FLAGRESULTS_H

#include <QList>
#include <QMap>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "findingdefs.h"

class FlagFindings
{
  public:
    FlagFindings(int type, int64_t value) : m_type(type), m_value(value) {};
    FlagFindings(FlagFindings *other) : m_type(other->m_type),
        m_value(other->m_value), m_offset(other->m_offset) {};
    ~FlagFindings() {};
    QString toString(void);
    QString toGnuplot(void);
    void SetTiming(int64_t timestamp, int duration, int frameDuration,
                   int offset = 0);

    int     m_type;
    int64_t m_value;

    int64_t m_timestamp;
    int     m_duration;
    int     m_frameDuration;
    int     m_offset;       // Offset within video frame
};

class ResultsMap;
class FlagResults : public QList<FlagFindings *>
{
  public:
    FlagResults() {};
    ~FlagResults() {};

    static FlagResults *Create(ResultsMap *map, int64_t timestamp);

    QString toString(void);
    QString toGnuplot(void);
};

class ResultsMap : public QMap<int64_t, FlagResults *>
{
  public:
    QString toString(QString title);
    QString toGnuplot(void);
    ResultsMap *Compress(int frameDuration);
    int64_t GetDuration(void);
};

typedef QMap<int, QString> FlagFindingsMap;

void FindingsInitialize(void);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

