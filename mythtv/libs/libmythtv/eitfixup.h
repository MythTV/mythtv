/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 */

#ifndef EITFIXUP_H
#define EITFIXUP_H

#include <qregexp.h>

#include "eit.h"

typedef QMap<uint,uint> QMap_uint_t;

/// EIT Fix Up Functions
class EITFixUp
{
  public:
    enum FixUpType
    {
        kFixNone    = 0,
        kFixBell    = 1,
        kFixUK      = 2,
        kFixPBS     = 3,
        kFixComHem  = 4,
        kFixAUStar  = 5,
    };

    EITFixUp();

    void Fix(Event &event, int fix_type) const;

    void clearSubtitleServiceIDs(void)
        { parseSubtitleServiceIDs.clear(); }
    void addSubtitleServiceID(uint st_pid)
        { parseSubtitleServiceIDs[st_pid] = 1; }

  private:
    void FixBellExpressVu(Event &event) const; // Canada DVB-S
    void FixUK(Event &event) const;            // UK DVB-T
    void FixPBS(Event &event) const;           // USA ATSC
    void FixComHem(Event &event) const;        // Sweden DVB-C
    void FixAUStar(Event &event) const;        // Australia DVB-S

    /** List of ServiceID's for which to parse out subtitle
     *  from the description. Used in EITFixUpStyle4().
     */
    QMap_uint_t parseSubtitleServiceIDs;

    const QRegExp m_bellYear;
    const QRegExp m_bellActors;
    const QRegExp m_ukSubtitle;
    const QRegExp m_ukThen;
    const QRegExp m_ukNew;
    const QRegExp m_ukT4;
    const QRegExp m_ukEQ;
    const QRegExp m_ukEPQ;
    const QRegExp m_ukPStart;
    const QRegExp m_ukPEnd;
    const QRegExp m_ukSeries1;
    const QRegExp m_ukSeries2;
    const QRegExp m_ukCC;
    const QRegExp m_ukYear;
    const QRegExp m_comHemCountry;
    const QRegExp m_comHemPEnd;
    const QRegExp m_comHemDirector;
    const QRegExp m_comHemActor;
    const QRegExp m_comHemSub;
    const QRegExp m_comHemRerun1;
    const QRegExp m_comHemRerun2;
};

#endif // EITFIXUP_H
