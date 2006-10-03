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
        kFixNone       = 0x0000,

        // Regular fixups
        kFixGenericDVB = 0x0001,
        kFixBell       = 0x0002,
        kFixUK         = 0x0004,
        kFixPBS        = 0x0008,
        kFixComHem     = 0x0010,
        kFixSubtitle   = 0x0020,
        kFixAUStar     = 0x0040,
        kFixMCA        = 0x0080,

        // Early fixups
        kEFixPro7Sat   = 0x0100,
        kEFixBell      = 0x0200,
    };

    EITFixUp();

    void Fix(DBEvent &event) const;

    static void TimeFix(QDateTime &dt)
    {
        int secs = dt.time().second();
        if (secs < 5)
            dt = dt.addSecs(-secs);
        if (secs > 55)
            dt = dt.addSecs(60 - secs);
    }

  private:
    void FixBellExpressVu(DBEvent &event) const; // Canada DVB-S
    void FixUK(DBEvent &event) const;            // UK DVB-T
    void FixPBS(DBEvent &event) const;           // USA ATSC
    void FixComHem(DBEvent &event, bool parse_subtitle) const; // Sweden DVB-C
    void FixAUStar(DBEvent &event) const;        // Australia DVB-S
    void FixMCA(DBEvent &event) const;        // MultiChoice Africa DVB-S

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
    const QRegExp m_comHemDirector;
    const QRegExp m_comHemActor;
    const QRegExp m_comHemHost;
    const QRegExp m_comHemSub;
    const QRegExp m_comHemRerun1;
    const QRegExp m_comHemRerun2;
    const QRegExp m_comHemTT;
    const QRegExp m_comHemPersSeparator;
    const QRegExp m_comHemPersons;
    const QRegExp m_comHemSubEnd;
    const QRegExp m_comHemSeries1;
    const QRegExp m_comHemSeries2;
    const QRegExp m_comHemTSub;
    const QRegExp m_mcaIncompleteTitle;
    const QRegExp m_mcaSubtitle;
    const QRegExp m_mcaSeries;
    const QRegExp m_mcaCredits;
    const QRegExp m_mcaActors;
    const QRegExp m_mcaActorsSeparator;
    const QRegExp m_mcaYear;
    const QRegExp m_mcaCC;
};

#endif // EITFIXUP_H
