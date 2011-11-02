#ifndef _RESULTSLIST_H
#define _RESULTSLIST_H

#include <QList>

extern "C" {
#include "libavcodec/avcodec.h"
}

typedef enum {
    kFindingAudioHigh,
    kFindingAudioLow,
} FlagFindingsType;

class FlagFindings
{
  public:
    FlagFindings(int type, int64_t value) : m_type(type), m_value(value) {};
    ~FlagFindings() {};

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
        if (m_valid)
        {
            m_pts = pkt->pts;
            m_dts = pkt->dts;
            m_duration = pkt->duration;
            m_pos = pkt->pos;
        }
    }

    FlagResults(FlagFindingsList *list) : m_findings(list) {};
    ~FlagResults() { delete m_findings; };

    bool    m_valid;
    int64_t m_pts;
    int64_t m_dts;
    int     m_duration;
    int64_t m_pos;

    FlagFindingsList *m_findings;
};

typedef QList<FlagResults *> ResultsList;

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

