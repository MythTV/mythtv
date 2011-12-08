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
    ~FlagFindings() {};
    QString toString(void);

    int m_type;
    int64_t m_value;
};

typedef QList<FlagFindings *> FlagFindingsList;

class FlagResults
{
  public:
    FlagResults(AVPacket *pkt, FlagFindingsList *list) : m_findings(list)
    {
        m_valid = (pkt != NULL);
    }

    FlagResults(FlagFindingsList *list) : m_findings(list) {};
    ~FlagResults() { delete m_findings; };
    QString toString(void);

    bool    m_valid;
    int64_t m_timestamp;
    int     m_duration;

    FlagFindingsList *m_findings;
};

class ResultsMap : public QMap<int64_t, FlagResults *>
{
  public:
    QString toString(QString title);
};

typedef QMap<int, QString> FlagFindingsMap;

void FindingsInitialize(void);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

