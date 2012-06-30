/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 */

#ifndef EITFIXUP_H
#define EITFIXUP_H

#include <QRegExp>

#include "programdata.h"

typedef QMap<uint,uint> QMap_uint_t;

/// EIT Fix Up Functions
class EITFixUp
{
  protected:
     // max length of subtitle field in db.
     static const uint SUBTITLE_MAX_LEN = 128;
     // max number of words included in a subtitle
     static const uint kMaxToTitle = 14;
     // max number of words up to a period, question mark
     static const uint kDotToTitle = 9;
     // max number of question/exclamation marks
     static const uint kMaxQuestionExclamation = 2;
     // max number of difference in words between a period and a colon
     static const uint kMaxDotToColon = 5;
     // minimum duration of an event to consider it as movie
     static const int kMinMovieDuration = 75*60;

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
        kFixRTL        = 0x0100,
        kFixFI         = 0x0200,
        kFixPremiere   = 0x0400,
        kFixHDTV       = 0x0800,
        kFixNL         = 0x1000,
        kFixCategory   = 0x8000,
        kFixNO         = 0x10000,
        kFixNRK_DVBT   = 0x20000,
        kFixDish       = 0x40000,
        kFixDK         = 0x80000,

        // Early fixups
        kEFixForceISO8859_1  = 0x2000,
        kEFixForceISO8859_15 = 0x4000,
        kEFixForceISO8859_9  = 0x80000,
    };

    EITFixUp();

    void Fix(DBEventEIT &event) const;

    /** Corrects starttime to the multiple of a minute. 
     *  Used for providers who fail to handle leap seconds timely. Changes the
     *  starttime not more than 3 seconds. Sshould only be used if the
     *  duration is the multiple of a minute. */
    static void TimeFix(QDateTime &dt)
    {
        int secs = dt.time().second();
        if (secs < 4)
            dt = dt.addSecs(-secs);
        if (secs > 56)
            dt = dt.addSecs(60 - secs);
    }

  private:
    void FixBellExpressVu(DBEventEIT &event) const; // Canada DVB-S
    void SetUKSubtitle(DBEventEIT &event) const;
    void FixUK(DBEventEIT &event) const;            // UK DVB-T
    void FixPBS(DBEventEIT &event) const;           // USA ATSC
    void FixComHem(DBEventEIT &event,
                   bool parse_subtitle) const;      // Sweden DVB-C
    void FixAUStar(DBEventEIT &event) const;        // Australia DVB-S
    void FixMCA(DBEventEIT &event) const;           // MultiChoice Africa DVB-S
    void FixRTL(DBEventEIT &event) const;           // RTL group DVB
    void FixFI(DBEventEIT &event) const;            // Finland DVB-T
    void FixPremiere(DBEventEIT &event) const;      // german pay-tv Premiere
    void FixNL(DBEventEIT &event) const;            // Netherlands DVB-C
    void FixCategory(DBEventEIT &event) const;      // Generic Category fixes
    void FixNO(DBEventEIT &event) const;            // Norwegian DVB-S
    void FixNRK_DVBT(DBEventEIT &event) const;      // Norwegian NRK DVB-T
    void FixDK(DBEventEIT &event) const;            // Danish YouSee DVB-C

    static QString AddDVBEITAuthority(uint chanid, const QString &id);

    const QRegExp m_bellYear;
    const QRegExp m_bellActors;
    const QRegExp m_bellPPVTitleAllDayHD;
    const QRegExp m_bellPPVTitleAllDay;
    const QRegExp m_bellPPVTitleHD;
    const QRegExp m_bellPPVSubtitleAllDay;
    const QRegExp m_bellPPVDescriptionAllDay;
    const QRegExp m_bellPPVDescriptionAllDay2;
    const QRegExp m_bellPPVDescriptionEventId;
    const QRegExp m_dishPPVTitleHD;
    const QRegExp m_dishPPVTitleColon;
    const QRegExp m_dishPPVSpacePerenEnd;
    const QRegExp m_dishDescriptionNew;
    const QRegExp m_dishDescriptionFinale;
    const QRegExp m_dishDescriptionFinale2;
    const QRegExp m_dishDescriptionPremiere;
    const QRegExp m_dishDescriptionPremiere2;
    const QRegExp m_dishPPVCode;
    const QRegExp m_ukThen;
    const QRegExp m_ukNew;
    const QRegExp m_ukCEPQ;
    const QRegExp m_ukColonPeriod;
    const QRegExp m_ukDotSpaceStart;
    const QRegExp m_ukDotEnd;
    const QRegExp m_ukSpaceColonStart;
    const QRegExp m_ukSpaceStart;
    const QRegExp m_ukSeries;
    const QRegExp m_ukCC;
    const QRegExp m_ukYear;
    const QRegExp m_uk24ep;
    const QRegExp m_ukStarring;
    const QRegExp m_ukBBC7rpt;
    const QRegExp m_ukDescriptionRemove;
    const QRegExp m_ukTitleRemove;
    const QRegExp m_ukDoubleDotEnd;
    const QRegExp m_ukDoubleDotStart;
    const QRegExp m_ukTime;
    const QRegExp m_ukBBC34;
    const QRegExp m_ukYearColon;
    const QRegExp m_ukExclusionFromSubtitle;
    const QRegExp m_ukCompleteDots;
    const QRegExp m_ukQuotedSubtitle;
    const QRegExp m_ukAllNew;
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
    const QRegExp m_mcaCompleteTitlea;
    const QRegExp m_mcaCompleteTitleb;
    const QRegExp m_mcaSubtitle;
    const QRegExp m_mcaSeries;
    const QRegExp m_mcaCredits;
    const QRegExp m_mcaAvail;
    const QRegExp m_mcaActors;
    const QRegExp m_mcaActorsSeparator;
    const QRegExp m_mcaYear;
    const QRegExp m_mcaCC;
    const QRegExp m_mcaDD;
    const QRegExp m_RTLrepeat;
    const QRegExp m_RTLSubtitle;
    const QRegExp m_RTLSubtitle1;
    const QRegExp m_RTLSubtitle2;
    const QRegExp m_RTLSubtitle3;
    const QRegExp m_RTLSubtitle4;
    const QRegExp m_RTLSubtitle5;
    const QRegExp m_RTLEpisodeNo1;
    const QRegExp m_RTLEpisodeNo2;
    const QRegExp m_fiRerun;
    const QRegExp m_fiRerun2;
    const QRegExp m_dePremiereInfos;
    const QRegExp m_dePremiereOTitle;
    const QRegExp m_nlTxt;
    const QRegExp m_nlWide;
    const QRegExp m_nlRepeat;
    const QRegExp m_nlHD;
    const QRegExp m_nlSub;
    const QRegExp m_nlActors;
    const QRegExp m_nlPres;
    const QRegExp m_nlPersSeparator;
    const QRegExp m_nlRub;
    const QRegExp m_nlYear1;
    const QRegExp m_nlYear2;
    const QRegExp m_nlDirector;
    const QRegExp m_nlCat;
    const QRegExp m_nlOmroep;
    const QRegExp m_noRerun;
    const QRegExp m_noColonSubtitle;
    const QRegExp m_noNRKCategories;
    const QRegExp m_noPremiere;
    const QRegExp m_Stereo;
    const QRegExp m_dkEpisode;
    const QRegExp m_dkPart;
    const QRegExp m_dkSubtitle1;
    const QRegExp m_dkSubtitle2;
    const QRegExp m_dkSeason1;
    const QRegExp m_dkSeason2;
    const QRegExp m_dkFeatures;
    const QRegExp m_dkWidescreen;
    const QRegExp m_dkDolby;
    const QRegExp m_dkSurround;
    const QRegExp m_dkStereo;
    const QRegExp m_dkReplay;
    const QRegExp m_dkTxt;
    const QRegExp m_dkHD;
    const QRegExp m_dkActors;
    const QRegExp m_dkPersonsSeparator;
    const QRegExp m_dkDirector;
    const QRegExp m_dkYear;
};

#endif // EITFIXUP_H
