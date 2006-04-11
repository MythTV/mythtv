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
        kFixNone    = 0x00,
        kFixBell    = 0x01,
        kFixUK      = 0x02,
        kFixPBS     = 0x04,
        kFixComHem  = 0x08,
        kFixSubtitle= 0x10,
        kFixAUStar  = 0x20,
    };

    EITFixUp();

    void Fix(DBEvent &event) const;

    static void TimeFix(QDateTime &dt)
    {
        int secs = dt.time().second();
        if (secs < 5)
            dt.addSecs(-secs);
        if (secs > 55)
            dt.addSecs(60 - secs);
    }

  private:
    void FixBellExpressVu(DBEvent &event) const; // Canada DVB-S
    void FixUK(DBEvent &event) const;            // UK DVB-T
    void FixPBS(DBEvent &event) const;           // USA ATSC
    void FixComHem(DBEvent &event, bool parse_subtitle) const; // Sweden DVB-C
    void FixAUStar(DBEvent &event) const;        // Australia DVB-S

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
