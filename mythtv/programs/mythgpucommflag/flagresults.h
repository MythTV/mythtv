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
        m_value(other->m_value), m_timestamp(other->m_timestamp),
        m_duration(other->m_duration), m_frameDuration(other->m_frameDuration),
        m_offset(other->m_offset) {};
    virtual ~FlagFindings() {};
    QString toString(void);
    QString toGnuplot(void);
    void SetTiming(int64_t timestamp, int duration, int frameDuration,
                   int offset = 0);
    int64_t GetAbsStart(void) const 
        { return (m_timestamp * m_frameDuration) + m_offset; };
    int64_t GetAbsEnd(void) const { return GetAbsStart() + m_duration - 1; };

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
    virtual ~FlagResults();

    static FlagResults *Create(ResultsMap *map, int64_t timestamp);

    QString toString(void);
    QString toGnuplot(void);
};

class ResultsTypeMap;
class ResultsMap : public QMap<int64_t, FlagResults *>
{
  public:
    virtual ~ResultsMap();
    QString toString(QString title);
    QString toGnuplot(void);
    ResultsMap *Normalize(int frameDuration);
    void Compress(void);
    ResultsTypeMap *RemapByType(void);
    int64_t GetFrameDuration(void);
};

typedef QMap<int, QString> FlagFindingsMap;

class ResultsTypeMap : public QMap<int, ResultsMap *>
{
  public:
    virtual ~ResultsTypeMap();
    QString toString(void);
    FlagResults *FindingsAtTime(int64_t timestart, int64_t timeend,
                                int duration);
};

void FindingsInitialize(void);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

