/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 */

#ifndef EITFIXUP_H
#define EITFIXUP_H

#include "programdata.h"

/// EIT Fix Up Functions
class MTV_PUBLIC EITFixUp
{
  protected:
    // max length of subtitle field in db.
    static const uint kSubtitleMaxLen = 128;
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
    enum FixUpType : FixupValue
    {
        kFixNone             =       0, // no bits set

        // Regular fixups
        kFixGenericDVB       = 1 <<  0, // LSB set
        kFixBell             = 1 <<  1,
        kFixUK               = 1 <<  2,
        kFixPBS              = 1 <<  3,
        kFixComHem           = 1 <<  4,
        kFixSubtitle         = 1 <<  5,
        kFixAUStar           = 1 <<  6,
        kFixMCA              = 1 <<  7,
        kFixRTL              = 1 <<  8,
        kFixFI               = 1 <<  9,
        kFixPremiere         = 1 << 10,
        kFixHDTV             = 1 << 11,
        kFixNL               = 1 << 12,
        kFixCategory         = 1 << 13,
        kFixNO               = 1 << 14,
        kFixNRK_DVBT         = 1 << 15,
        kFixDish             = 1 << 16,
        kFixDK               = 1 << 17,
        kFixAUFreeview       = 1 << 18,
        kFixAUDescription    = 1 << 19,
        kFixAUNine           = 1 << 20,
        kFixAUSeven          = 1 << 21,
        kFixP7S1             = 1 << 26,
        kFixHTML             = 1 << 27,
        kFixUnitymedia       = 1ULL << 32,
        kFixATV              = 1ULL << 33,
        kFixDisneyChannel    = 1ULL << 34,

        // Early fixups
        kEFixForceISO8859_1  = 1 << 22,
        kEFixForceISO8859_2  = 1 << 23,
        kEFixForceISO8859_9  = 1 << 24,
        kEFixForceISO8859_15 = 1 << 25,
        kEFixForceISO8859_7  = 1 << 28,

        kFixGreekSubtitle    = 1 << 29,
        kFixGreekEIT         = 1 << 30,
        kFixGreekCategories  = 1U << 31,
    };

    EITFixUp() = default;

    static void Fix(DBEventEIT &event);

    static int parseRoman (QString roman);

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
    static void FixBellExpressVu(DBEventEIT &event);// Canada DVB-S
    static void SetUKSubtitle(DBEventEIT &event);
    static void FixUK(DBEventEIT &event);           // UK DVB-T
    static void FixPBS(DBEventEIT &event);          // USA ATSC
    static void FixComHem(DBEventEIT &event,
                          bool process_subtitle);   // Sweden DVB-C
    static void FixAUStar(DBEventEIT &event);       // Australia DVB-S
    static void FixAUFreeview(DBEventEIT &event);   // Australia DVB-T
    static void FixAUNine(DBEventEIT &event);
    static void FixAUSeven(DBEventEIT &event);
    static void FixAUDescription(DBEventEIT &event);
    static void FixMCA(DBEventEIT &event);          // MultiChoice Africa DVB-S
    static void FixRTL(DBEventEIT &event);          // RTL group DVB
    static void FixPRO7(DBEventEIT &event);         // Pro7/Sat1 Group
    static void FixDisneyChannel(DBEventEIT &event);// Disney Channel
    static void FixATV(DBEventEIT &event);          // ATV/ATV2
    static void FixFI(DBEventEIT &event);           // Finland DVB-T
    static void FixPremiere(DBEventEIT &event);     // german pay-tv Premiere
    static void FixNL(DBEventEIT &event);           // Netherlands DVB-C
    static void FixCategory(DBEventEIT &event);     // Generic Category fixes
    static void FixNO(DBEventEIT &event);           // Norwegian DVB-S
    static void FixNRK_DVBT(DBEventEIT &event);     // Norwegian NRK DVB-T
    static void FixDK(DBEventEIT &event);           // Danish YouSee DVB-C
    static void FixStripHTML(DBEventEIT &event);    // Strip HTML tags
    static void FixGreekSubtitle(DBEventEIT &event);// Greek Nat TV fix
    static void FixGreekEIT(DBEventEIT &event);
    static void FixGreekCategories(DBEventEIT &event);// Greek categories from descr.
    static void FixUnitymedia(DBEventEIT &event);     // handle cast/crew from Unitymedia

    static QString AddDVBEITAuthority(uint chanid, const QString &id);
};

#endif // EITFIXUP_H
