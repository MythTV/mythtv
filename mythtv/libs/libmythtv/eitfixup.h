/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 */

#ifndef EITFIXUP_H
#define EITFIXUP_H

#include <QRegExp>

#include "programdata.h"

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
    static void FixPBS(DBEventEIT &event);          // USA ATSC
    void FixComHem(DBEventEIT &event,
                   bool process_subtitle) const;    // Sweden DVB-C
    static void FixAUStar(DBEventEIT &event);       // Australia DVB-S
    void FixAUFreeview(DBEventEIT &event) const;    // Australia DVB-T
    static void FixAUNine(DBEventEIT &event);
    static void FixAUSeven(DBEventEIT &event);
    static void FixAUDescription(DBEventEIT &event);
    void FixMCA(DBEventEIT &event) const;           // MultiChoice Africa DVB-S
    void FixRTL(DBEventEIT &event) const;           // RTL group DVB
    void FixPRO7(DBEventEIT &event) const;          // Pro7/Sat1 Group
    void FixDisneyChannel(DBEventEIT &event) const; // Disney Channel
    void FixATV(DBEventEIT &event) const;           // ATV/ATV2
    void FixFI(DBEventEIT &event) const;            // Finland DVB-T
    void FixPremiere(DBEventEIT &event) const;      // german pay-tv Premiere
    void FixNL(DBEventEIT &event) const;            // Netherlands DVB-C
    static void FixCategory(DBEventEIT &event);     // Generic Category fixes
    void FixNO(DBEventEIT &event) const;            // Norwegian DVB-S
    void FixNRK_DVBT(DBEventEIT &event) const;      // Norwegian NRK DVB-T
    void FixDK(DBEventEIT &event) const;            // Danish YouSee DVB-C
    void FixStripHTML(DBEventEIT &event) const;     // Strip HTML tags
    static void FixGreekSubtitle(DBEventEIT &event);// Greek Nat TV fix
    void FixGreekEIT(DBEventEIT &event) const;
    void FixGreekCategories(DBEventEIT &event) const; // Greek categories from descr.
    void FixUnitymedia(DBEventEIT &event) const;    // handle cast/crew from Unitymedia

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
    const QRegExp m_ukNewTitle;
    const QRegExp m_ukAlsoInHD;
    const QRegExp m_ukCEPQ;
    const QRegExp m_ukColonPeriod;
    const QRegExp m_ukDotSpaceStart;
    const QRegExp m_ukDotEnd;
    const QRegExp m_ukSpaceColonStart;
    const QRegExp m_ukSpaceStart;
    const QRegExp m_ukPart;
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
    const QRegExp m_ukLaONoSplit;
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
    const QRegExp m_PRO7Subtitle;
    const QRegExp m_PRO7Crew;
    const QRegExp m_PRO7CrewOne;
    const QRegExp m_PRO7Cast;
    const QRegExp m_PRO7CastOne;
    const QRegExp m_ATVSubtitle;
    const QRegExp m_DisneyChannelSubtitle;
    const QRegExp m_RTLEpisodeNo1;
    const QRegExp m_RTLEpisodeNo2;
    const QRegExp m_fiRerun;
    const QRegExp m_fiRerun2;
    const QRegExp m_fiAgeLimit;
    const QRegExp m_fiFilm;
    const QRegExp m_dePremiereLength;
    const QRegExp m_dePremiereAirdate;
    const QRegExp m_dePremiereCredits;
    const QRegExp m_dePremiereOTitle;
    const QRegExp m_deSkyDescriptionSeasonEpisode;
    const QRegExp m_nlTxt;
    const QRegExp m_nlWide;
    const QRegExp m_nlRepeat;
    const QRegExp m_nlHD;
    const QRegExp m_nlSub;
    const QRegExp m_nlSub2;
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
    const QRegExp m_noHD;
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
    const QRegExp m_AUFreeviewSY;//subtitle, year
    const QRegExp m_AUFreeviewY;//year
    const QRegExp m_AUFreeviewYC;//year, cast
    const QRegExp m_AUFreeviewSYC;//subtitle, year, cast
    const QRegExp m_HTML;
    const QRegExp m_grReplay; //Greek rerun
    const QRegExp m_grDescriptionFinale; //Greek last m_grEpisode
    const QRegExp m_grActors; //Greek actors
    const QRegExp m_grFixnofullstopActors; //bad punctuation makes the "Παίζουν:" and the actors' names part of the directors...
    const QRegExp m_grFixnofullstopDirectors; //bad punctuation makes the "Σκηνοθ...:" and the previous sentence.
    const QRegExp m_grPeopleSeparator; // The comma that separates the actors.
    const QRegExp m_grDirector;
    const QRegExp m_grPres; // Greek Presenters for shows
    const QRegExp m_grYear; // Greek release year.
    const QRegExp m_grCountry; // Greek event country of origin.
    const QRegExp m_grlongEp; // Greek Episode
    const QRegExp m_grSeason_as_RomanNumerals; // Greek Episode in Roman numerals
    const QRegExp m_grSeason; // Greek Season
    const QRegExp m_grSeries;
    const QRegExp m_grRealTitleinDescription; // The original title is often in the descr in parenthesis.
    const QRegExp m_grRealTitleinTitle; // The original title is often in the title in parenthesis.
    const QRegExp m_grCommentsinTitle; // Sometimes esp. national stations include comments in the title eg "(ert arxeio)"
    const QRegExp m_grNotPreviouslyShown; // Not previously shown on TV
    const QRegExp m_grEpisodeAsSubtitle; // Description field: "^Episode: Lion in the cage. (Description follows)"
    const QRegExp m_grCategFood; // Greek category food
    const QRegExp m_grCategDrama; // Greek category social/drama
    const QRegExp m_grCategComedy; // Greek category comedy
    const QRegExp m_grCategChildren; // Greek category for children / cartoons
    const QRegExp m_grCategMystery; // Greek category for mystery
    const QRegExp m_grCategFantasy; // Greek category for fantasy
    const QRegExp m_grCategHistory; //Greek category for historical movie/series
    const QRegExp m_grCategTeleMag; //Greek category for Telemagazine show
    const QRegExp m_grCategTeleShop; //Greek category for teleshopping
    const QRegExp m_grCategGameShow; //Greek category for game show
    const QRegExp m_grCategDocumentary; // Greek category for Documentaries
    const QRegExp m_grCategBiography; // Greek category for biography
    const QRegExp m_grCategNews; // Greek category for News
    const QRegExp m_grCategSports; // Greek category for Sports
    const QRegExp m_grCategMusic; // Greek category for Music
    const QRegExp m_grCategReality; // Greek category for reality shows
    const QRegExp m_grCategReligion; //Greek category for religion
    const QRegExp m_grCategCulture; //Greek category for Arts/Culture
    const QRegExp m_grCategNature; //Greek category for Nature/Science
    const QRegExp m_grCategSciFi;  // Greek category for Science Fiction
    const QRegExp m_grCategHealth; //Greek category for Health
    const QRegExp m_grCategSpecial; //Greek category for specials.
    const QRegExp m_unitymediaImdbrating; ///< IMDb Rating
};

#endif // EITFIXUP_H
