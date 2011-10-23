#ifndef _RESULTSLIST_H
#define _RESULTSLIST_H

#include <QList>

extern "C" {
#include "libavcodec/avcodec.h"
}

class FlagResults
{
  public:
    FlagResults(AVPacket *pkt)
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

    ~FlagResults() {};

    bool    m_valid;
    int64_t m_pts;
    int64_t m_dts;
    int     m_duration;
    int64_t m_pos;

    // To fill in with the various findings
    bool    m_logoFound;
};

typedef QList<FlagResults *> ResultsList;

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

