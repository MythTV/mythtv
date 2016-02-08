// C++ headers
#include <algorithm>

// MythTV headers
#include "eitfixup.h"
#include "programinfo.h" // for CategoryType
#include "channelutil.h" // for GetDefaultAuthority()

#include "programinfo.h" // for subtitle types and audio and video properties
#include "dishdescriptors.h" // for dish_theme_type_to_string
#include "mythlogging.h"

/*------------------------------------------------------------------------
 * Event Fix Up Scripts - Turned on by entry in dtv_privatetype table
 *------------------------------------------------------------------------*/

// Constituents of UK season regexp, decomposed for clarity

// Matches Season 2, S 2 and "Series 2," etc but not "hits 2"
// cap1 = season
const QString season = "\\b(?:Season|Series|S)\\s*(\\d+)\\s*,?";

// Matches Episode 3, Ep 3/4, Ep 3 of 4 etc but not "step 1"
// cap1 = ep, cap2 = total
const QString longEp = "\\b(?:Ep|Episode)\\s*(\\d+)\\s*(?:(?:/|of)\\s*(\\d*))?";

// Matches S2 Ep 3/4, "Season 2, Ep 3 of 4", Episode 3 etc
// cap1 = season, cap2 = ep, cap3 = total
const QString longSeasEp = QString("\\(?(?:%1)?\\s*%2").arg(season, longEp);

// Matches long seas/ep with surrounding parenthesis & trailing period
// cap1 = season, cap2 = ep, cap3 = total
const QString longContext = QString("\\(*%1\\s*\\)?\\s*\\.?").arg(longSeasEp);

// Matches 3/4, 3 of 4
// cap1 = ep, cap2 = total
const QString shortEp = "(\\d+)\\s*(?:/|of)\\s*(\\d+)";

// Matches short ep/total, ignoring Parts and idioms such as 9/11, 24/7 etc.
// ie. x/y in parenthesis or has no leading or trailing text in the sentence.
// cap0 may include previous/anchoring period
// cap1 = shortEp with surrounding parenthesis & trailing period (to remove)
// cap2 = ep, cap3 = total,
const QString shortContext =
        QString("(?:^|\\.)(\\s*\\(*\\s*%1[\\s)]*(?:[).:]|$))").arg(shortEp);


EITFixUp::EITFixUp()
    : m_bellYear("[\\(]{1}[0-9]{4}[\\)]{1}"),
      m_bellActors("\\set\\s|,"),
      m_bellPPVTitleAllDayHD("\\s*\\(All Day\\, HD\\)\\s*$"),
      m_bellPPVTitleAllDay("\\s*\\(All Day.*\\)\\s*$"),
      m_bellPPVTitleHD("^HD\\s?-\\s?"),
      m_bellPPVSubtitleAllDay("^All Day \\(.*\\sEastern\\)\\s*$"),
      m_bellPPVDescriptionAllDay("^\\(.*\\sEastern\\)"),
      m_bellPPVDescriptionAllDay2("^\\([0-9].*am-[0-9].*am\\sET\\)"),
      m_bellPPVDescriptionEventId("\\([0-9]{5}\\)"),
      m_dishPPVTitleHD("\\sHD\\s*$"),
      m_dishPPVTitleColon("\\:\\s*$"),
      m_dishPPVSpacePerenEnd("\\s\\)\\s*$"),
      m_dishDescriptionNew("\\s*New\\.\\s*"),
      m_dishDescriptionFinale("\\s*(Series|Season)\\sFinale\\.\\s*"),
      m_dishDescriptionFinale2("\\s*Finale\\.\\s*"),
      m_dishDescriptionPremiere("\\s*(Series|Season)\\s(Premier|Premiere)\\.\\s*"),
      m_dishDescriptionPremiere2("\\s*(Premier|Premiere)\\.\\s*"),
      m_dishPPVCode("\\s*\\(([A-Z]|[0-9]){5}\\)\\s*$"),
      m_ukThen("\\s*(Then|Followed by) 60 Seconds\\.", Qt::CaseInsensitive),
      m_ukNew("(New\\.|\\s*(Brand New|New)\\s*(Series|Episode)\\s*[:\\.\\-])",Qt::CaseInsensitive),
      m_ukNewTitle("^(Brand New|New:)\\s*",Qt::CaseInsensitive),
      m_ukAlsoInHD("\\s*Also in HD\\.",Qt::CaseInsensitive),
      m_ukCEPQ("[:\\!\\.\\?]"),
      m_ukColonPeriod("[:\\.]"),
      m_ukDotSpaceStart("^\\. "),
      m_ukDotEnd("\\.$"),
      m_ukSpaceColonStart("^[ |:]*"),
      m_ukSpaceStart("^ "),
      m_ukPart("[-(\\:,.]\\s*(?:Part|Pt)\\s*(\\d+)\\s*(?:(?:of|/)\\s*(\\d+))?\\s*[-):,.]", Qt::CaseInsensitive),
      // Prefer long format resorting to short format
      // cap0 = long match to remove, cap1 = long season, cap2 = long ep, cap3 = long total,
      // cap4 = short match to remove, cap5 = short ep, cap6 = short total
      m_ukSeries("(?:" + longContext + "|" + shortContext + ")", Qt::CaseInsensitive),
      m_ukCC("\\[(?:(AD|SL|S|W|HD),?)+\\]"),
      m_ukYear("[\\[\\(]([\\d]{4})[\\)\\]]"),
      m_uk24ep("^\\d{1,2}:00[ap]m to \\d{1,2}:00[ap]m: "),
      m_ukStarring("(?:Western\\s)?[Ss]tarring ([\\w\\s\\-']+)[Aa]nd\\s([\\w\\s\\-']+)[\\.|,](?:\\s)*(\\d{4})?(?:\\.\\s)?"),
      m_ukBBC7rpt("\\[Rptd?[^]]+\\d{1,2}\\.\\d{1,2}[ap]m\\]\\."),
      m_ukDescriptionRemove("^(?:CBBC\\s*\\.|CBeebies\\s*\\.|Class TV\\s*:|BBC Switch\\.)"),
      m_ukTitleRemove("^(?:[tT]4:|Schools\\s*:)"),
      m_ukDoubleDotEnd("\\.\\.+$"),
      m_ukDoubleDotStart("^\\.\\.+"),
      m_ukTime("\\d{1,2}[\\.:]\\d{1,2}\\s*(am|pm|)"),
      m_ukBBC34("BBC (?:THREE|FOUR) on BBC (?:ONE|TWO)\\.",Qt::CaseInsensitive),
      m_ukYearColon("^[\\d]{4}:"),
      m_ukExclusionFromSubtitle("(starring|stars\\s|drama|series|sitcom)",Qt::CaseInsensitive),
      m_ukCompleteDots("^\\.\\.+$"),
      m_ukQuotedSubtitle("(?:^')([\\w\\s\\-,]+)(?:\\.' )"),
      m_ukAllNew("All New To 4Music!\\s?"),
      m_ukLaONoSplit("^Law & Order: (?:Criminal Intent|LA|Special Victims Unit|Trial by Jury|UK|You the Jury)"),
      m_comHemCountry("^(\\(.+\\))?\\s?([^ ]+)\\s([^\\.0-9]+)"
                      "(?:\\sfr\xE5n\\s([0-9]{4}))(?:\\smed\\s([^\\.]+))?\\.?"),
      m_comHemDirector("[Rr]egi"),
      m_comHemActor("[Ss]k\xE5""despelare|[Ii] rollerna"),
      m_comHemHost("[Pp]rogramledare"),
      m_comHemSub("[.\\?\\!] "),
      m_comHemRerun1("[Rr]epris\\sfr\xE5n\\s([^\\.]+)(?:\\.|$)"),
      m_comHemRerun2("([0-9]+)/([0-9]+)(?:\\s-\\s([0-9]{4}))?"),
      m_comHemTT("[Tt]ext-[Tt][Vv]"),
      m_comHemPersSeparator("(, |\\soch\\s)"),
      m_comHemPersons("\\s?([Rr]egi|[Ss]k\xE5""despelare|[Pp]rogramledare|"
                      "[Ii] rollerna):\\s([^\\.]+)\\."),
      m_comHemSubEnd("\\s?\\.\\s?$"),
      m_comHemSeries1("\\s?(?:[dD]el|[eE]pisode)\\s([0-9]+)"
                      "(?:\\s?(?:/|:|av)\\s?([0-9]+))?\\."),
      m_comHemSeries2("\\s?-?\\s?([Dd]el\\s+([0-9]+))"),
      m_comHemTSub("\\s+-\\s+([^\\-]+)"),
      m_mcaIncompleteTitle("(.*).\\.\\.\\.$"),
      m_mcaCompleteTitlea("^'?("),
      m_mcaCompleteTitleb("[^\\.\\?]+[^\\'])'?[\\.\\?]\\s+(.+)"),
      m_mcaSubtitle("^'([^\\.]+)'\\.\\s+(.+)"),
      m_mcaSeries("^S?(\\d+)\\/E?(\\d+)\\s-\\s(.*)$"),
      m_mcaCredits("(.*)\\s\\((\\d{4})\\)\\s*([^\\.]+)\\.?\\s*$"),
      m_mcaAvail("\\s(Only available on [^\\.]*bouquet|Not available in RSA [^\\.]*)\\.?"),
      m_mcaActors("(.*\\.)\\s+([^\\.]+\\s[A-Z][^\\.]+)\\.\\s*"),
      m_mcaActorsSeparator("(,\\s+)"),
      m_mcaYear("(.*)\\s\\((\\d{4})\\)\\s*$"),
      m_mcaCC(",?\\s(HI|English) Subtitles\\.?"),
      m_mcaDD(",?\\sDD\\.?"),
      m_RTLrepeat("(\\(|\\s)?Wiederholung.+vo[m|n].+((?:\\d{2}\\.\\d{2}\\.\\d{4})|(?:\\d{2}[:\\.]\\d{2}\\sUhr))\\)?"),
      m_RTLSubtitle("^([^\\.]{3,})\\.\\s+(.+)"),
      /* should be (?:\x{8a}|\\.\\s*|$) but 0x8A gets replaced with 0x20 */
      m_RTLSubtitle1("^Folge\\s(\\d{1,4})\\s*:\\s+'(.*)'(?:\\s|\\.\\s*|$)"),
      m_RTLSubtitle2("^Folge\\s(\\d{1,4})\\s+(.{,5}[^\\.]{,120})[\\?!\\.]\\s*"),
      m_RTLSubtitle3("^(?:Folge\\s)?(\\d{1,4}(?:\\/[IVX]+)?)\\s+(.{,5}[^\\.]{,120})[\\?!\\.]\\s*"),
      m_RTLSubtitle4("^Thema.{0,5}:\\s([^\\.]+)\\.\\s*"),
      m_RTLSubtitle5("^'(.+)'\\.\\s*"),
      m_PRO7Subtitle(",{0,1}([^,]*),([^,]+)\\s{0,1}(\\d{4})$"),
      m_PRO7Crew("\n\n(Regie:.*)$"),
      m_PRO7CrewOne("^(.*):\\s+(.*)$"),
      m_PRO7Cast("\n\nDarsteller:\n(.*)$"),
      m_PRO7CastOne("^([^\\(]*)\\((.*)\\)$"),
      m_RTLEpisodeNo1("^(Folge\\s\\d{1,4})\\.*\\s*"),
      m_RTLEpisodeNo2("^(\\d{1,2}\\/[IVX]+)\\.*\\s*"),
      m_fiRerun("\\ ?Uusinta[a-zA-Z\\ ]*\\.?"),
      m_fiRerun2("\\([Uu]\\)"),
      m_dePremiereInfos("\\s*[0-9]+\\sMin\\.([^.]+)?\\s?([0-9]{4})\\.(?:\\sVon"
                        "\\s([^,]+)(?:,|\\su\\.\\sa\\.)\\smit\\s(.+)\\.)?"),
      m_dePremiereOTitle("\\s*\\(([^\\)]*)\\)$"),
      m_deSkyDescriptionSeasonEpisode("^(\\d{1,2}).\\sStaffel,\\sFolge\\s(\\d{1,2}):\\s"),
      m_nlTxt("txt"),
      m_nlWide("breedbeeld"),
      m_nlRepeat("herh."),
      m_nlHD("\\sHD$"),
      m_nlSub("\\sAfl\\.:\\s([^\\.]+)\\."),
      m_nlSub2("\\s\"([^\"]+)\""),
      m_nlActors("\\sMet:\\s.+e\\.a\\."),
      m_nlPres("\\sPresentatie:\\s([^\\.]+)\\."),
      m_nlPersSeparator("(, |\\sen\\s)"),
      m_nlRub("\\s?\\({1}\\W+\\){1}\\s?"),
      m_nlYear1("(?=\\suit\\s)([1-2]{2}[0-9]{2})"),
      m_nlYear2("([\\s]{1}[\\(]{1}[A-Z]{0,3}/?)([1-2]{2}[0-9]{2})([\\)]{1})"),
      m_nlDirector("(?=\\svan\\s)(([A-Z]{1}[a-z]+\\s)|([A-Z]{1}\\.\\s))"),
      m_nlCat("^(Amusement|Muziek|Informatief|Nieuws/actualiteiten|Jeugd|Animatie|Sport|Serie/soap|Kunst/Cultuur|Documentaire|Film|Natuur|Erotiek|Comedy|Misdaad|Religieus)\\.\\s"),
      m_nlOmroep ("\\s\\(([A-Z]+/?)+\\)$"),
      m_noRerun("\\(R\\)"),
      m_noHD("[\\(\\[]HD[\\)\\]]"),
      m_noColonSubtitle("^([^:]+): (.+)"),
      m_noNRKCategories("^(Superstrek[ea]r|Supersomm[ea]r|Superjul|Barne-tv|Fantorangen|Kuraffen|Supermorg[eo]n|Julemorg[eo]n|Sommermorg[eo]n|"
                        "Kuraffen-TV|Sport i dag|NRKs sportsl.rdag|NRKs sportss.ndag|Dagens dokumentar|"
                        "NRK2s historiekveld|Detektimen|Nattkino|Filmklassiker|Film|Kortfilm|P.skemorg[eo]n|"
                        "Radioteatret|Opera|P2-Akademiet|Nyhetsmorg[eo]n i P2 og Alltid Nyheter:): (.+)"),
      m_noPremiere("\\s+-\\s+(Sesongpremiere|Premiere|premiere)!?$"),
      m_Stereo("\\b\\(?[sS]tereo\\)?\\b"),
      m_dkEpisode("\\(([0-9]+)\\)"),
      m_dkPart("\\(([0-9]+):([0-9]+)\\)"),
      m_dkSubtitle1("^([^:]+): (.+)"),
      m_dkSubtitle2("^([^:]+) - (.+)"),
      m_dkSeason1("S\xE6son ([0-9]+)\\."),
      m_dkSeason2("- \xE5r ([0-9]+)(?: :)"),
      m_dkFeatures("Features:(.+)"),
      m_dkWidescreen(" 16:9"),
      m_dkDolby(" 5:1"),
      m_dkSurround(" \\(\\(S\\)\\)"),
      m_dkStereo(" S"),
      m_dkReplay(" \\(G\\)"),
      m_dkTxt(" TTV"),
      m_dkHD(" HD"),
      m_dkActors("(?:Medvirkende: |Medv\\.: )(.+)"),
      m_dkPersonsSeparator("(, )|(og )"),
      m_dkDirector("(?:Instr.: |Instrukt.r: )(.+)$"),
      m_dkYear(" fra ([0-9]{4})[ \\.]"),
      m_AUFreeviewSY("(.*) \\((.+)\\) \\(([12][0-9][0-9][0-9])\\)$"),
      m_AUFreeviewY("(.*) \\(([12][0-9][0-9][0-9])\\)$"),
      m_AUFreeviewYC("(.*) \\(([12][0-9][0-9][0-9])\\) \\((.+)\\)$"),
      m_AUFreeviewSYC("(.*) \\((.+)\\) \\(([12][0-9][0-9][0-9])\\) \\((.+)\\)$"),
      m_HTML("</?EM>", Qt::CaseInsensitive),
      m_grReplay("\\([ΕE]\\)"),
      m_grDescriptionFinale("\\s*Τελευταίο\\sΕπεισόδιο\\.\\s*"),
      m_grActors("(?:[Ππ]α[ιί]ζουν:|[Μμ]ε τους:|Πρωταγωνιστο[υύ]ν:|Πρωταγωνιστε[ιί]:?)(?:\\s+στο ρόλο(?: του| της)?\\s(?:\\w+\\s[οη]\\s))?([-\\w\\s']+(?:,[-\\w\\s']+)*)(?:κ\\.[αά])?(?:\\W?)"),
      // cap(1) actors, just names
      m_grFixnofullstopActors("(\\w\\s(Παίζουν:|Πρωταγων))"),
      m_grFixnofullstopDirectors("((\\w\\s(Σκηνοθ[εέ]))"),
      m_grPeopleSeparator("(,\\s+)"),
      m_grDirector("(?:Σκηνοθεσία: |Σκηνοθέτης: )(\\w+\\s\\w+\\s?)(?:\\W?)"),
      m_grPres("(?:Παρουσ[ιί]αση:(?:\\b)*|Παρουσι[αά]ζ(?:ουν|ει)(?::|\\sο|\\sη)|Με τ(?:ον |ην )(?:[\\s|:|ο|η])*(?:\\b)*)([-\\w\\s]+(?:,[-\\w\\s]+)*)(?:\\W?)"),
      m_grYear("(?:\\W?)(?:\\s?παραγωγ[ηή]ς|\\s?-|,)\\s*([1-2]{1}[0-9]{3})(?:-\\d{1,4})?",Qt::CaseInsensitive),
      m_grCountry("(?:\\W|\\b)(?:(ελλην|τουρκ|αμερικ[αά]ν|γαλλ|αγγλ|βρεττ?αν|γερμαν|ρωσσ?|ιταλ|ελβετ|σουηδ|ισπαν|πορτογαλ|μεξικ[αά]ν|κιν[εέ]ζικ|ιαπων|καναδ|βραζιλι[αά]ν)(ικ[ηή][ςσ]))",Qt::CaseInsensitive),
      m_grlongEp("\\b(?:Επ.|επεισ[οό]διο:?)\\s*(\\d+)(?:\\W?)",Qt::CaseInsensitive),
      m_grSeason("(?:-\\s)?\\b((\\D{1,2})(?:')?|(\\d{1,2})(?:ος|ου)?)(?:\\sκ[υύ]κλο(?:[σς]|υ)){1}\\s?",Qt::CaseInsensitive),
      m_grRealTitleinDescription("(?:^\\()([\\w\\s\\d\\D-]+)(?:\\))(?:\\s*)"),
      // cap1 = real title
      // cap0 = real title in parentheses.
      m_grRealTitleinTitle("(?:\\()([\\w\\s\\d\\D-]+)(?:\\))(?:\\s*$)*"),
      // cap1 = real title
      // cap0 = real title in parentheses.
      m_grNotPreviouslyShown("(?:\\b[Α1]['η]?\\s*(?:τηλεοπτικ[ηή]\\s*)?(?:μετ[αά]δοση|προβολ[ηή]))(?:\\W?)"),
      // Try to exctract Greek categories from keywords in description.
      m_grEpisodeAsSubtitle("(?:^Επεισ[οό]διο:\\s?)([\\w\\s-,']+)\\.(?:\\s)?"),
      m_grCategFood("(?:\\W)?(?:εκπομπ[ηή]\\W)?(Γαστρονομ[ιί]α[σς]?|μαγειρικ[ηή][σς]?|chef|συνταγ[εέηή]|διατροφ|wine|μ[αά]γειρα[σς]?)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategDrama("(?:\\W)?(κοινωνικ[ηήό]|δραματικ[ηή]|δρ[αά]μα)(?:\\W)(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",Qt::CaseInsensitive),
      m_grCategComedy("(?:\\W)?(κωμικ[ηήοό]|χιουμοριστικ[ηήοό]|κωμωδ[ιί]α)(?:\\W)(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",Qt::CaseInsensitive),
      m_grCategChildren("(?:\\W)?(παιδικ[ηήοό]|κινο[υύ]μ[εέ]ν(ων|α)\\sσχ[εέ]δ[ιί](ων|α))(?:\\W)(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",Qt::CaseInsensitive),
      m_grCategMystery("(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?(?:\\W)?(μυστηρ[ιί]ου)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategFantasy("(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?(?:\\W)?(φαντασ[ιί]ας)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategHistory("(?:\\W)?(ιστορικ[ηήοό])(?:\\W)?(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",Qt::CaseInsensitive),
      m_grCategTeleMag("(?:\\W)?(ενημερωτικ[ηή]|ψυχαγωγικ[ηή]|τηλεπεριοδικ[οό]|μαγκαζ[ιί]νο)(?:\\W)?(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",Qt::CaseInsensitive),
      m_grCategTeleShop("(?:\\W)?(οδηγ[οό][σς]?\\sαγορ[ωώ]ν|τηλεπ[ωώ]λ[ηή]σ|τηλεαγορ|τηλεμ[αά]ρκετ|telemarket)(?:\\W)?(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",Qt::CaseInsensitive),
      m_grCategGameShow("(?:\\W)?(τηλεπαιχν[ιί]δι|quiz)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategDocumentary("(?:\\W)?(ντοκ[ιυ]μαντ[εέ]ρ)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategBiography("(?:\\W)?(βιογραφ[ιί]α|βιογραφικ[οό][σς]?)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategNews("(?:\\W)?(δελτ[ιί]ο\\W?|ειδ[ηή]σε(ι[σς]|ων))(?:\\W)?",Qt::CaseInsensitive),
      m_grCategSports("(?:\\W)?(champion|αθλητικ[αάοόηή]|πρωτ[αά]θλημα|ποδ[οό]σφαιρο(ου)?|κολ[υύ]μβηση|πατιν[αά]ζ|formula|μπ[αά]σκετ|β[οό]λε[ιϊ])(?:\\W)?",Qt::CaseInsensitive),
      m_grCategMusic("(?:\\W)?(μουσικ[οόηή]|eurovision|τραγο[υύ]δι)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategReality("(?:\\W)?(ρι[αά]λιτι|reality)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategReligion("(?:\\W)?(θρησκε[ιί]α|θρησκευτικ|να[οό][σς]?|θε[ιί]α λειτουργ[ιί]α)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategCulture("(?:\\W)?(τ[εέ]χν(η|ε[σς])|πολιτισμ)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategNature("(?:\\W)?(φ[υύ]ση|περιβ[αά]λλο|κατασκευ|επιστ[ηή]μ(?!ονικ[ηή]ς φαντασ[ιί]ας))(?:\\W)?",Qt::CaseInsensitive),
      m_grCategSciFi("(?:\\W)?(επιστ(.|ημονικ[ηή]ς)\\s?φαντασ[ιί]ας)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategHealth("(?:\\W)?(υγε[ιί]α|υγειιν|ιατρικ|διατροφ)(?:\\W)?",Qt::CaseInsensitive),
      m_grCategSpecial("(?:\\W)?(αφι[εέ]ρωμα)(?:\\W)?",Qt::CaseInsensitive),
      m_unitymediaImdbrating("\\s*IMDb Rating: (\\d\\.\\d) /10$")
{
}

void EITFixUp::Fix(DBEventEIT &event) const
{
    if (event.fixup)
    {
        if (event.subtitle == event.title)
            event.subtitle = QString("");

        if (event.description.isEmpty() && !event.subtitle.isEmpty())
        {
            event.description = event.subtitle;
            event.subtitle = QString("");
        }
    }

    if (kFixHTML & event.fixup)
        FixStripHTML(event);

    if (kFixHDTV & event.fixup)
        event.videoProps |= VID_HDTV;

    if (kFixBell & event.fixup)
        FixBellExpressVu(event);

    if (kFixDish & event.fixup)
        FixBellExpressVu(event);

    if (kFixUK & event.fixup)
        FixUK(event);

    if (kFixPBS & event.fixup)
        FixPBS(event);

    if (kFixComHem & event.fixup)
        FixComHem(event, kFixSubtitle & event.fixup);

    if (kFixAUStar & event.fixup)
        FixAUStar(event);

    if (kFixAUDescription & event.fixup)
        FixAUDescription(event);

    if (kFixAUFreeview & event.fixup)
        FixAUFreeview(event);

    if (kFixAUNine & event.fixup)
        FixAUNine(event);

    if (kFixAUSeven & event.fixup)
        FixAUSeven(event);

    if (kFixMCA & event.fixup)
        FixMCA(event);

    if (kFixRTL & event.fixup)
        FixRTL(event);

    if (kFixP7S1 & event.fixup)
        FixPRO7(event);

    if (kFixFI & event.fixup)
        FixFI(event);

    if (kFixPremiere & event.fixup)
        FixPremiere(event);

    if (kFixNL & event.fixup)
        FixNL(event);

    if (kFixNO & event.fixup)
        FixNO(event);

    if (kFixNRK_DVBT & event.fixup)
        FixNRK_DVBT(event);

    if (kFixDK & event.fixup)
        FixDK(event);

    if (kFixCategory & event.fixup)
        FixCategory(event);

    if (kFixGreekSubtitle & event.fixup)
        FixGreekSubtitle(event);

    if (kFixGreekEIT & event.fixup)
        FixGreekEIT(event);

    if (kFixGreekCategories & event.fixup)
        FixGreekCategories(event);

    if (kFixUnitymedia & event.fixup)
        FixUnitymedia(event);

    if (event.fixup)
    {
        if (!event.title.isEmpty())
        {
            event.title = event.title.replace(QChar('\0'), "");
            event.title = event.title.trimmed();
        }

        if (!event.subtitle.isEmpty())
        {
            event.subtitle = event.subtitle.replace(QChar('\0'), "");
            event.subtitle = event.subtitle.trimmed();
        }

        if (!event.description.isEmpty())
        {
            event.description = event.description.replace(QChar('\0'), "");
            event.description = event.description.trimmed();
        }
    }

    if (kFixGenericDVB & event.fixup)
    {
        event.programId = AddDVBEITAuthority(event.chanid, event.programId);
        event.seriesId  = AddDVBEITAuthority(event.chanid, event.seriesId);
    }

    // Are any items left unhandled? report them to allow fixups improvements
    if (!event.items.empty())
    {
        QMap<QString,QString>::iterator i;
        for (i = event.items.begin(); i != event.items.end(); ++i)
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Unhandled item in EIT for"
                " channel id \"%1\", \"%2\": %3").arg(event.chanid)
                .arg(i.key()).arg(i.value()));
        }
    }
}

/**
 *  This adds a DVB EIT default authority to series id or program id if
 *  one exists in the DB for that channel, otherwise it returns a blank
 *  id instead of the id passed in.
 *
 *  If a series id or program id is a CRID URI, just keep important info
 *  ID's are case insensitive, so lower case the whole id.
 *  If there is no authority on the ID, add the default one.
 *  If there is no default, return an empty id.
 *
 *  \param id The ID string to add the authority to.
 *  \param query Object to use for SQL queries.
 *
 *  \return ID with the authority added or empty string if not a valid CRID.
 */
QString EITFixUp::AddDVBEITAuthority(uint chanid, const QString &id)
{
    if (id.isEmpty())
        return id;

    // CRIDs are not case sensitive, so change all to lower case
    QString crid = id.toLower();

    // remove "crid://"
    if (crid.startsWith("crid://"))
        crid.remove(0,7);

    // if id is a CRID with authority, return it
    if (crid.length() >= 1 && crid[0] != '/')
        return crid;

    QString authority = ChannelUtil::GetDefaultAuthority(chanid);
    if (authority.isEmpty())
        return ""; // no authority, not a valid CRID, return empty

    return authority + crid;
}

/**
 *  \brief Use this for the Canadian BellExpressVu to standardize DVB-S guide.
 *  \todo  deal with events that don't have eventype at the begining?
 *  \TODO
 */
void EITFixUp::FixBellExpressVu(DBEventEIT &event) const
{
    QString tmp;

    // A 0x0D character is present between the content
    // and the subtitle if its present
    int position = event.description.indexOf(0x0D);

    if (position != -1)
    {
        // Subtitle present in the title, so get
        // it and adjust the description
        event.subtitle = event.description.left(position);
        event.description = event.description.right(
            event.description.length() - position - 2);
    }

    // Take out the content description which is
    // always next with a period after it
    position = event.description.indexOf(".");
    // Make sure they didn't leave it out and
    // you come up with an odd category
    if (position < 10)
    {
    }
    else
    {
        event.category = "Unknown";
    }

    // If the content descriptor didn't come up with anything, try parsing the category
    // out of the description.
    if (event.category.isEmpty())
    {
        // Take out the content description which is
        // always next with a period after it
        position = event.description.indexOf(".");
        if ((position + 1) < event.description.length())
            position = event.description.indexOf(". ");
        // Make sure they didn't leave it out and
        // you come up with an odd category
        if ((position > -1) && position < 20)
        {
            const QString stmp       = event.description;
            event.description        = stmp.right(stmp.length() - position - 2);
            event.category = stmp.left(position);

            int position_p = event.category.indexOf("(");
            if (position_p == -1)
                event.description = stmp.right(stmp.length() - position - 2);
            else
                event.category    = "Unknown";
        }
        else
        {
            event.category = "Unknown";
        }

        // When a channel is off air the category is "-"
        // so leave the category as blank
        if (event.category == "-")
            event.category = "OffAir";

        if (event.category.length() > 20)
            event.category = "Unknown";
    }
    else if (event.categoryType)
    {
        QString theme = dish_theme_type_to_string(event.categoryType);
        event.description = event.description.replace(theme, "");
        if (event.description.startsWith("."))
            event.description = event.description.right(event.description.length() - 1);
        if (event.description.startsWith(" "))
            event.description = event.description.right(event.description.length() - 1);
    }

    // See if a year is present as (xxxx)
    position = event.description.indexOf(m_bellYear);
    if (position != -1 && !event.category.isEmpty())
    {
        tmp = "";
        // Parse out the year
        bool ok;
        uint y = event.description.mid(position + 1, 4).toUInt(&ok);
        if (ok)
        {
            event.originalairdate = QDate(y, 1, 1);
            event.airdate = y;
            event.previouslyshown = true;
        }

        // Get the actors if they exist
        if (position > 3)
        {
            tmp = event.description.left(position-3);
            QStringList actors =
                tmp.split(m_bellActors, QString::SkipEmptyParts);
            QStringList::const_iterator it = actors.begin();
            for (; it != actors.end(); ++it)
                event.AddPerson(DBPerson::kActor, *it);
        }
        // Remove the year and actors from the description
        event.description = event.description.right(
            event.description.length() - position - 7);
    }

    // Check for (CC) in the decription and
    // set the <subtitles type="teletext"> flag
    position = event.description.indexOf("(CC)");
    if (position != -1)
    {
        event.subtitleType |= SUB_HARDHEAR;
        event.description = event.description.replace("(CC)", "");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.description.indexOf(m_Stereo);
    if (position != -1)
    {
        event.audioProps |= AUD_STEREO;
        event.description = event.description.replace(m_Stereo, "");
    }

    // Check for "title (All Day, HD)" in the title
    position = event.title.indexOf(m_bellPPVTitleAllDayHD);
    if (position != -1)
    {
        event.title = event.title.replace(m_bellPPVTitleAllDayHD, "");
        event.videoProps |= VID_HDTV;
     }

    // Check for "title (All Day)" in the title
    position = event.title.indexOf(m_bellPPVTitleAllDay);
    if (position != -1)
    {
        event.title = event.title.replace(m_bellPPVTitleAllDay, "");
    }

    // Check for "HD - title" in the title
    position = event.title.indexOf(m_bellPPVTitleHD);
    if (position != -1)
    {
        event.title = event.title.replace(m_bellPPVTitleHD, "");
        event.videoProps |= VID_HDTV;
    }

    // Check for (HD) in the decription
    position = event.description.indexOf("(HD)");
    if (position != -1)
    {
        event.description = event.description.replace("(HD)", "");
        event.videoProps |= VID_HDTV;
    }

    // Check for (HD) in the title
    position = event.title.indexOf("(HD)");
    if (position != -1)
    {
        event.description = event.title.replace("(HD)", "");
        event.videoProps |= VID_HDTV;
    }

    // Check for HD at the end of the title
    position = event.title.indexOf(m_dishPPVTitleHD);
    if (position != -1)
    {
        event.title = event.title.replace(m_dishPPVTitleHD, "");
        event.videoProps |= VID_HDTV;
    }

    // Check for (DD) at the end of the description
    position = event.description.indexOf("(DD)");
    if (position != -1)
    {
        event.description = event.description.replace("(DD)", "");
        event.audioProps |= AUD_DOLBY;
        event.audioProps |= AUD_STEREO;
    }

    // Remove SAP from Dish descriptions
    position = event.description.indexOf("(SAP)");
    if (position != -1)
    {
        event.description = event.description.replace("(SAP", "");
        event.subtitleType |= SUB_HARDHEAR;
    }

    // Remove any trailing colon in title
    position = event.title.indexOf(m_dishPPVTitleColon);
    if (position != -1)
    {
        event.title = event.title.replace(m_dishPPVTitleColon, "");
    }

    // Remove New at the end of the description
    position = event.description.indexOf(m_dishDescriptionNew);
    if (position != -1)
    {
        event.previouslyshown = false;
        event.description = event.description.replace(m_dishDescriptionNew, "");
    }

    // Remove Series Finale at the end of the desciption
    position = event.description.indexOf(m_dishDescriptionFinale);
    if (position != -1)
    {
        event.previouslyshown = false;
        event.description = event.description.replace(m_dishDescriptionFinale, "");
    }

    // Remove Series Finale at the end of the desciption
    position = event.description.indexOf(m_dishDescriptionFinale2);
    if (position != -1)
    {
        event.previouslyshown = false;
        event.description = event.description.replace(m_dishDescriptionFinale2, "");
    }

    // Remove Series Premiere at the end of the description
    position = event.description.indexOf(m_dishDescriptionPremiere);
    if (position != -1)
    {
        event.previouslyshown = false;
        event.description = event.description.replace(m_dishDescriptionPremiere, "");
    }

    // Remove Series Premiere at the end of the description
    position = event.description.indexOf(m_dishDescriptionPremiere2);
    if (position != -1)
    {
        event.previouslyshown = false;
        event.description = event.description.replace(m_dishDescriptionPremiere2, "");
    }

    // Remove Dish's PPV code at the end of the description
    QRegExp ppvcode = m_dishPPVCode;
    ppvcode.setCaseSensitivity(Qt::CaseInsensitive);
    position = event.description.indexOf(ppvcode);
    if (position != -1)
    {
        event.description = event.description.replace(ppvcode, "");
    }

    // Remove trailing garbage
    position = event.description.indexOf(m_dishPPVSpacePerenEnd);
    if (position != -1)
    {
        event.description = event.description.replace(m_dishPPVSpacePerenEnd, "");
    }

    // Check for subtitle "All Day (... Eastern)" in the subtitle
    position = event.subtitle.indexOf(m_bellPPVSubtitleAllDay);
    if (position != -1)
    {
        event.subtitle = event.subtitle.replace(m_bellPPVSubtitleAllDay, "");
    }

    // Check for description "(... Eastern)" in the description
    position = event.description.indexOf(m_bellPPVDescriptionAllDay);
    if (position != -1)
    {
        event.description = event.description.replace(m_bellPPVDescriptionAllDay, "");
    }

    // Check for description "(... ET)" in the description
    position = event.description.indexOf(m_bellPPVDescriptionAllDay2);
    if (position != -1)
    {
        event.description = event.description.replace(m_bellPPVDescriptionAllDay2, "");
    }

    // Check for description "(nnnnn)" in the description
    position = event.description.indexOf(m_bellPPVDescriptionEventId);
    if (position != -1)
    {
        event.description = event.description.replace(m_bellPPVDescriptionEventId, "");
    }

}

/** \fn EITFixUp::SetUKSubtitle(DBEventEIT&) const
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::SetUKSubtitle(DBEventEIT &event) const
{
    QStringList strListColon = event.description.split(":");
    QStringList strListEnd;

    bool fColon = false, fQuotedSubtitle = false;
    int nPosition1;
    QString strEnd;
    if (strListColon.count()>1)
    {
         bool fDoubleDot = false;
         bool fSingleDot = true;
         int nLength = strListColon[0].length();

         nPosition1 = event.description.indexOf("..");
         if ((nPosition1 < nLength) && (nPosition1 >= 0))
             fDoubleDot = true;
         nPosition1 = event.description.indexOf(".");
         if (nPosition1==-1)
             fSingleDot = false;
         if (nPosition1 > nLength)
             fSingleDot = false;
         else
         {
             QString strTmp = event.description.mid(nPosition1+1,
                                     nLength-nPosition1);

             QStringList tmp = strTmp.split(" ");
             if (((uint) tmp.size()) < kMaxDotToColon)
                 fSingleDot = false;
         }

         if (fDoubleDot)
         {
             strListEnd = strListColon;
             fColon = true;
         }
         else if (!fSingleDot)
         {
             QStringList strListTmp;
             uint nTitle=0;
             int nTitleMax=-1;
             int i;
             for (i =0; (i<(int)strListColon.count()) && (nTitleMax==-1);i++)
             {
                 const QStringList tmp = strListColon[i].split(" ");

                 nTitle += tmp.size();

                 if (nTitle < kMaxToTitle)
                     strListTmp.push_back(strListColon[i]);
                 else
                     nTitleMax=i;
             }
             QString strPartial;
             for (i=0;i<(nTitleMax-1);i++)
                 strPartial+=strListTmp[i]+":";
             if (nTitleMax>0)
             {
                 strPartial+=strListTmp[nTitleMax-1];
                 strListEnd.push_back(strPartial);
             }
             for (i=nTitleMax+1;i<(int)strListColon.count();i++)
                 strListEnd.push_back(strListColon[i]);
             fColon = true;
         }
    }
    QRegExp tmpQuotedSubtitle = m_ukQuotedSubtitle;
    if (tmpQuotedSubtitle.indexIn(event.description) != -1)
    {
        event.subtitle = tmpQuotedSubtitle.cap(1);
        event.description.remove(m_ukQuotedSubtitle);
        fQuotedSubtitle = true;
    }
    QStringList strListPeriod;
    QStringList strListQuestion;
    QStringList strListExcl;
    if (!(fColon || fQuotedSubtitle))
    {
        strListPeriod = event.description.split(".");
        if (strListPeriod.count() >1)
        {
            nPosition1 = event.description.indexOf(".");
            int nPosition2 = event.description.indexOf("..");
            if ((nPosition1 < nPosition2) || (nPosition2==-1))
                strListEnd = strListPeriod;
        }

        strListQuestion = event.description.split("?");
        strListExcl = event.description.split("!");
        if ((strListQuestion.size() > 1) &&
            ((uint)strListQuestion.size() <= kMaxQuestionExclamation))
        {
            strListEnd = strListQuestion;
            strEnd = "?";
        }
        else if ((strListExcl.size() > 1) &&
                 ((uint)strListExcl.size() <= kMaxQuestionExclamation))
        {
            strListEnd = strListExcl;
            strEnd = "!";
        }
        else
            strEnd = QString::null;
    }

    if (!strListEnd.empty())
    {
        QStringList strListSpace = strListEnd[0].split(
            " ", QString::SkipEmptyParts);
        if (fColon && ((uint)strListSpace.size() > kMaxToTitle))
             return;
        if ((uint)strListSpace.size() > kDotToTitle)
             return;
        if (strListSpace.filter(m_ukExclusionFromSubtitle).empty())
        {
             event.subtitle = strListEnd[0]+strEnd;
             event.subtitle.remove(m_ukSpaceColonStart);
             event.description=
                          event.description.mid(strListEnd[0].length()+1);
             event.description.remove(m_ukSpaceColonStart);
        }
    }
}


/** \fn EITFixUp::FixUK(DBEventEIT&) const
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::FixUK(DBEventEIT &event) const
{
    int position1;
    int position2;
    QString strFull;

    bool isMovie = event.category.startsWith("Movie",Qt::CaseInsensitive) ||
                   event.category.startsWith("Film",Qt::CaseInsensitive);
    // BBC three case (could add another record here ?)
    event.description = event.description.remove(m_ukThen);
    event.description = event.description.remove(m_ukNew);
    event.title = event.title.remove(m_ukNewTitle);

    // Removal of Class TV, CBBC and CBeebies etc..
    event.title = event.title.remove(m_ukTitleRemove);
    event.description = event.description.remove(m_ukDescriptionRemove);

    // Removal of BBC FOUR and BBC THREE
    event.description = event.description.remove(m_ukBBC34);

    // BBC 7 [Rpt of ...] case.
    event.description = event.description.remove(m_ukBBC7rpt);

    // "All New To 4Music!
    event.description = event.description.remove(m_ukAllNew);

    // Removal of 'Also in HD' text
 	event.description = event.description.remove(m_ukAlsoInHD);

    // Remove [AD,S] etc.
    bool    ccMatched = false;
    QRegExp tmpCC = m_ukCC;
    position1 = 0;
    while ((position1 = tmpCC.indexIn(event.description, position1)) != -1)
    {
        ccMatched = true;
        position1 += tmpCC.matchedLength();

        QStringList tmpCCitems = tmpCC.cap(0).remove("[").remove("]").split(",");
        if (tmpCCitems.contains("AD"))
            event.audioProps |= AUD_VISUALIMPAIR;
        if (tmpCCitems.contains("HD"))
            event.videoProps |= VID_HDTV;
        if (tmpCCitems.contains("S"))
            event.subtitleType |= SUB_NORMAL;
        if (tmpCCitems.contains("SL"))
            event.subtitleType |= SUB_SIGNED;
        if (tmpCCitems.contains("W"))
            event.videoProps |= VID_WIDESCREEN;
    }

    if(ccMatched)
        event.description = event.description.remove(m_ukCC);

    event.title       = event.title.trimmed();
    event.description = event.description.trimmed();

    // Work out the season and episode numbers (if any)
    // Matching pattern "Season 2 Episode|Ep 3 of 14|3/14" etc
    bool    series  = false;
    QRegExp tmpSeries = m_ukSeries;
    if ((position1 = tmpSeries.indexIn(event.title)) != -1
            || (position2 = tmpSeries.indexIn(event.description)) != -1)
    {
        if (!tmpSeries.cap(1).isEmpty())
        {
            event.season = tmpSeries.cap(1).toUInt();
            series = true;
        }

        if (!tmpSeries.cap(2).isEmpty())
        {
            event.episode = tmpSeries.cap(2).toUInt();
            series = true;
        }
        else if (!tmpSeries.cap(5).isEmpty())
        {
            event.episode = tmpSeries.cap(5).toUInt();
            series = true;
        }

        if (!tmpSeries.cap(3).isEmpty())
        {
            event.totalepisodes = tmpSeries.cap(3).toUInt();
            series = true;
        }
        else if (!tmpSeries.cap(6).isEmpty())
        {
            event.totalepisodes = tmpSeries.cap(6).toUInt();
            series = true;
        }

        // Remove long or short match. Short text doesn't start at position2
        int form = tmpSeries.cap(4).isEmpty() ? 0 : 4;

        if (position1 != -1)
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Extracted S%1E%2/%3 from title (%4) \"%5\"")
                .arg(event.season).arg(event.episode).arg(event.totalepisodes)
                .arg(event.title, event.description));

            event.title.remove(tmpSeries.cap(form));
        }
        else
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Extracted S%1E%2/%3 from description (%4) \"%5\"")
                .arg(event.season).arg(event.episode).arg(event.totalepisodes)
                .arg(event.title, event.description));

            if (position2 == 0)
     		    // Remove from the start of the description.
		        // Otherwise it ends up in the subtitle.
                event.description.remove(tmpSeries.cap(form));
        }
    }

    if (isMovie)
        event.categoryType = ProgramInfo::kCategoryMovie;
    else if (series)
        event.categoryType = ProgramInfo::kCategorySeries;

    // Multi-part episodes, or films (e.g. ITV film split by news)
    // Matches Part 1, Pt 1/2, Part 1 of 2 etc.
    QRegExp tmpPart = m_ukPart;
    if ((position1 = tmpPart.indexIn(event.title)) != -1)
    {
        event.partnumber = tmpPart.cap(1).toUInt();
        event.parttotal  = tmpPart.cap(2).toUInt();

        LOG(VB_EIT, LOG_DEBUG, QString("Extracted Part %1/%2 from title (%3)")
            .arg(event.partnumber).arg(event.parttotal).arg(event.title));

        // Remove from the title
        event.title = event.title.remove(tmpPart.cap(0));
    }
    else if ((position1 = tmpPart.indexIn(event.description)) != -1)
    {
        event.partnumber = tmpPart.cap(1).toUInt();
        event.parttotal  = tmpPart.cap(2).toUInt();

        LOG(VB_EIT, LOG_DEBUG, QString("Extracted Part %1/%2 from description (%3) \"%4\"")
            .arg(event.partnumber).arg(event.parttotal)
            .arg(event.title, event.description));

        // Remove from the start of the description.
        // Otherwise it ends up in the subtitle.
        if (position1 == 0)
        {
            // Retain a single colon (subtitle separator) if we remove any
            QString sub = tmpPart.cap(0).contains(":") ? ":" : "";
            event.description = event.description.replace(tmpPart.cap(0), sub);
        }
    }

    QRegExp tmpStarring = m_ukStarring;
    if (tmpStarring.indexIn(event.description) != -1)
    {
        // if we match this we've captured 2 actors and an (optional) airdate
        event.AddPerson(DBPerson::kActor, tmpStarring.cap(1));
        event.AddPerson(DBPerson::kActor, tmpStarring.cap(2));
        if (tmpStarring.cap(3).length() > 0)
        {
            bool ok;
            uint y = tmpStarring.cap(3).toUInt(&ok);
            if (ok)
            {
                event.airdate = y;
                event.originalairdate = QDate(y, 1, 1);
            }
        }
    }

    QRegExp tmp24ep = m_uk24ep;
    if (!event.title.startsWith("CSI:") && !event.title.startsWith("CD:") &&
        !event.title.contains(m_ukLaONoSplit) &&
        !event.title.startsWith("Mission: Impossible"))
    {
        if (((position1=event.title.indexOf(m_ukDoubleDotEnd)) != -1) &&
            ((position2=event.description.indexOf(m_ukDoubleDotStart)) != -1))
        {
            QString strPart=event.title.remove(m_ukDoubleDotEnd)+" ";
            strFull = strPart + event.description.remove(m_ukDoubleDotStart);
            if (isMovie &&
                ((position1 = strFull.indexOf(m_ukCEPQ,strPart.length())) != -1))
            {
                 if (strFull[position1] == '!' || strFull[position1] == '?')
                     position1++;
                 event.title = strFull.left(position1);
                 event.description = strFull.mid(position1 + 1);
                 event.description.remove(m_ukSpaceStart);
            }
            else if ((position1 = strFull.indexOf(m_ukCEPQ)) != -1)
            {
                 if (strFull[position1] == '!' || strFull[position1] == '?')
                     position1++;
                 event.title = strFull.left(position1);
                 event.description = strFull.mid(position1 + 1);
                 event.description.remove(m_ukSpaceStart);
                 SetUKSubtitle(event);
            }
            if ((position1 = strFull.indexOf(m_ukYear)) != -1)
            {
                // Looks like they are using the airdate as a delimiter
                if ((uint)position1 < SUBTITLE_MAX_LEN)
                {
                    event.description = event.title.mid(position1);
                    event.title = event.title.left(position1);
                }
            }
        }
        else if ((position1 = tmp24ep.indexIn(event.description)) != -1)
        {
            // Special case for episodes of 24.
            // -2 from the length cause we don't want ": " on the end
            event.subtitle = event.description.mid(position1,
                                tmp24ep.cap(0).length() - 2);
            event.description = event.description.remove(tmp24ep.cap(0));
        }
        else if ((position1 = event.description.indexOf(m_ukTime)) == -1)
        {
            if (!isMovie && (event.title.indexOf(m_ukYearColon) < 0))
            {
                if (((position1 = event.title.indexOf(":")) != -1) &&
                    (event.description.indexOf(":") < 0 ))
                {
                    if (event.title.mid(position1+1).indexOf(m_ukCompleteDots)==0)
                    {
                        SetUKSubtitle(event);
                        QString strTmp = event.title.mid(position1+1);
                        event.title.resize(position1);
                        event.subtitle = strTmp+event.subtitle;
                    }
                    else if ((uint)position1 < SUBTITLE_MAX_LEN)
                    {
                        event.subtitle = event.title.mid(position1 + 1);
                        event.title = event.title.left(position1);
                    }
                }
                else
                    SetUKSubtitle(event);
            }
        }
    }

    if (!isMovie && event.subtitle.isEmpty() &&
        !event.title.startsWith("The X-Files"))
    {
        if ((position1=event.description.indexOf(m_ukTime)) != -1)
        {
            position2 = event.description.indexOf(m_ukColonPeriod);
            if ((position2>=0) && (position2 < (position1-2)))
                SetUKSubtitle(event);
        }
        else if ((position1=event.title.indexOf("-")) != -1)
        {
            if ((uint)position1 < SUBTITLE_MAX_LEN)
            {
                event.subtitle = event.title.mid(position1 + 1);
                event.subtitle.remove(m_ukSpaceColonStart);
                event.title = event.title.left(position1);
            }
        }
        else
            SetUKSubtitle(event);
    }

    // Work out the year (if any)
    QRegExp tmpUKYear = m_ukYear;
    if ((position1 = tmpUKYear.indexIn(event.description)) != -1)
    {
        QString stmp = event.description;
        int     itmp = position1 + tmpUKYear.cap(0).length();
        event.description = stmp.left(position1) + stmp.mid(itmp);
        bool ok;
        uint y = tmpUKYear.cap(1).toUInt(&ok);
        if (ok)
        {
            event.airdate = y;
            event.originalairdate = QDate(y, 1, 1);
        }
    }

    // Trim leading/trailing '.'
    event.subtitle.remove(m_ukDotSpaceStart);
    if (event.subtitle.lastIndexOf("..") != (((int)event.subtitle.length())-2))
        event.subtitle.remove(m_ukDotEnd);

    // Reverse the subtitle and empty description
    if (event.description.isEmpty() && !event.subtitle.isEmpty())
    {
        event.description=event.subtitle;
        event.subtitle=QString::null;
    }
}

/** \fn EITFixUp::FixPBS(DBEventEIT&) const
 *  \brief Use this to standardize PBS ATSC guide in the USA.
 */
void EITFixUp::FixPBS(DBEventEIT &event) const
{
    /* Used for PBS ATSC Subtitles are separated by a colon */
    int position = event.description.indexOf(':');
    if (position != -1)
    {
        const QString stmp   = event.description;
        event.subtitle = stmp.left(position);
        event.description    = stmp.right(stmp.length() - position - 2);
    }
}

/**
 *  \brief Use this to standardize ComHem DVB-C service in Sweden.
 */
void EITFixUp::FixComHem(DBEventEIT &event, bool process_subtitle) const
{
    // Reverse what EITFixUp::Fix() did
    if (event.subtitle.isEmpty() && !event.description.isEmpty())
    {
        event.subtitle = event.description;
        event.description = "";
    }

    // Remove subtitle, it contains the category and we already know that
    event.subtitle = "";

    bool isSeries = false;
    // Try to find episode numbers
    int pos;
    QRegExp tmpSeries1 = m_comHemSeries1;
    QRegExp tmpSeries2 = m_comHemSeries2;
    if ((pos = tmpSeries2.indexIn(event.title)) != -1)
    {
        QStringList list = tmpSeries2.capturedTexts();
        event.partnumber = list[2].toUInt();
        event.title = event.title.replace(list[0],"");
    }
    else if ((pos = tmpSeries1.indexIn(event.description)) != -1)
    {
        QStringList list = tmpSeries1.capturedTexts();
        if (!list[1].isEmpty())
        {
            event.partnumber = list[1].toUInt();
        }
        if (!list[2].isEmpty())
        {
            event.parttotal = list[2].toUInt();
        }

        // Remove the episode numbers, but only if it's not at the begining
        // of the description (subtitle code might use it)
        if(pos > 0)
            event.description = event.description.replace(list[0],"");
        isSeries = true;
    }

    // Add partnumber/parttotal to subtitle
    // This will be overwritten if we find a better subtitle
    if (event.partnumber > 0)
    {
        event.subtitle = QString("Del %1").arg(event.partnumber);
        if (event.parttotal > 0)
        {
            event.subtitle += QString(" av %1").arg(event.parttotal);
        }
    }

    // Move subtitle info from title to subtitle
    QRegExp tmpTSub = m_comHemTSub;
    if (tmpTSub.indexIn(event.title) != -1)
    {
        event.subtitle = tmpTSub.cap(1);
        event.title = event.title.replace(tmpTSub.cap(0),"");
    }

    // No need to continue without a description.
    if (event.description.length() <= 0)
        return;

    // Try to find country category, year and possibly other information
    // from the begining of the description
    QRegExp tmpCountry = m_comHemCountry;
    pos = tmpCountry.indexIn(event.description);
    if (pos != -1)
    {
        QStringList list = tmpCountry.capturedTexts();
        QString replacement;

        // Original title, usually english title
        // note: list[1] contains extra () around the text that needs removing
        if (list[1].length() > 0)
        {
            replacement = list[1] + " ";
            //store it somewhere?
        }

        // Countr(y|ies)
        if (list[2].length() > 0)
        {
            replacement += list[2] + " ";
            //store it somewhere?
        }

        // Category
        if (list[3].length() > 0)
        {
            replacement += list[3] + ".";
            if(event.category.isEmpty())
            {
                event.category = list[3];
            }

            if(list[3].indexOf("serie")!=-1)
            {
                isSeries = true;
            }
        }

        // Year
        if (list[4].length() > 0)
        {
            bool ok;
            uint y = list[4].trimmed().toUInt(&ok);
            if (ok)
                event.airdate = y;
        }

        // Actors
        if (list[5].length() > 0)
        {
            const QStringList actors =
                list[5].split(m_comHemPersSeparator, QString::SkipEmptyParts);
            QStringList::const_iterator it = actors.begin();
            for (; it != actors.end(); ++it)
                event.AddPerson(DBPerson::kActor, *it);
        }

        // Remove year and actors.
        // The reason category is left in the description is because otherwise
        // the country would look wierd like "Amerikansk. Rest of description."
        event.description = event.description.replace(list[0],replacement);
    }

    if (isSeries)
        event.categoryType = ProgramInfo::kCategorySeries;

    // Look for additional persons in the description
    QRegExp tmpPersons = m_comHemPersons;
    while(pos = tmpPersons.indexIn(event.description),pos!=-1)
    {
        DBPerson::Role role;
        QStringList list = tmpPersons.capturedTexts();

        QRegExp tmpDirector = m_comHemDirector;
        QRegExp tmpActor = m_comHemActor;
        QRegExp tmpHost = m_comHemHost;
        if (tmpDirector.indexIn(list[1])!=-1)
        {
            role = DBPerson::kDirector;
        }
        else if(tmpActor.indexIn(list[1])!=-1)
        {
            role = DBPerson::kActor;
        }
        else if(tmpHost.indexIn(list[1])!=-1)
        {
            role = DBPerson::kHost;
        }
        else
        {
            event.description=event.description.replace(list[0],"");
            continue;
        }

        const QStringList actors =
            list[2].split(m_comHemPersSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
            event.AddPerson(role, *it);

        // Remove it
        event.description=event.description.replace(list[0],"");
    }

    // Is this event on a channel we shoud look for a subtitle?
    // The subtitle is the first sentence in the description, but the
    // subtitle can't be the only thing in the description and it must be
    // shorter than 55 characters or we risk picking up the wrong thing.
    if (process_subtitle)
    {
        int pos = event.description.indexOf(m_comHemSub);
        bool pvalid = pos != -1 && pos <= 55;
        if (pvalid && (event.description.length() - (pos + 2)) > 0)
        {
            event.subtitle = event.description.left(
                pos + (event.description[pos] == '?' ? 1 : 0));
            event.description = event.description.mid(pos + 2);
        }
    }

    // Teletext subtitles?
    int position = event.description.indexOf(m_comHemTT);
    if (position != -1)
    {
        event.subtitleType |= SUB_NORMAL;
    }

    // Try to findout if this is a rerun and if so the date.
    QRegExp tmpRerun1 = m_comHemRerun1;
    if (tmpRerun1.indexIn(event.description) == -1)
        return;

    // Rerun from today
    QStringList list = tmpRerun1.capturedTexts();
    if (list[1] == "i dag")
    {
        event.originalairdate = event.starttime.date();
        return;
    }

    // Rerun from yesterday afternoon
    if (list[1] == "eftermiddagen")
    {
        event.originalairdate = event.starttime.date().addDays(-1);
        return;
    }

    // Rerun with day, month and possibly year specified
    QRegExp tmpRerun2 = m_comHemRerun2;
    if (tmpRerun2.indexIn(list[1]) != -1)
    {
        QStringList datelist = tmpRerun2.capturedTexts();
        int day   = datelist[1].toInt();
        int month = datelist[2].toInt();
        //int year;

        //if (datelist[3].length() > 0)
        //    year = datelist[3].toInt();
        //else
        //    year = event.starttime.date().year();

        if (day > 0 && month > 0)
        {
            QDate date(event.starttime.date().year(), month, day);
            // it's a rerun so it must be in the past
            if (date > event.starttime.date())
                date = date.addYears(-1);
            event.originalairdate = date;
        }
        return;
    }
}

/** \fn EITFixUp::FixAUStar(DBEventEIT&) const
 *  \brief Use this to standardize DVB-S guide in Australia.
 */
void EITFixUp::FixAUStar(DBEventEIT &event) const
{
    event.category = event.subtitle;
    /* Used for DVB-S Subtitles are separated by a colon */
    int position = event.description.indexOf(':');
    if (position != -1)
    {
        const QString stmp   = event.description;
        event.subtitle       = stmp.left(position);
        event.description    = stmp.right(stmp.length() - position - 2);
    }
}

/** \fn EITFixUp::FixAUDescription(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (fix common annoyances common to most networks)
 */
void EITFixUp::FixAUDescription(DBEventEIT &event) const
{
    if (event.description.startsWith("[Program data ") || event.description.startsWith("[Program info "))//TEN
    {
        event.description = "";//event.subtitle;
    }
    if (event.description.endsWith("Copyright West TV Ltd. 2011)"))
        event.description.resize(event.description.length()-40);

    if (event.description.isEmpty() && !event.subtitle.isEmpty())//due to ten's copyright info, this won't be caught before
    {
        event.description = event.subtitle;
        event.subtitle = QString::null;
    }
    if (event.description.startsWith(event.title+" - "))
        event.description.remove(0,event.title.length()+3);
    if (event.title.startsWith("LIVE: ", Qt::CaseInsensitive))
    {
        event.title.remove(0, 6);
        event.description.prepend("(Live) ");
    }
}
/** \fn EITFixUp::FixAUNine(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (Nine network)
 */
void EITFixUp::FixAUNine(DBEventEIT &event) const
{
    QRegExp rating("\\((G|PG|M|MA)\\)");
    if (rating.indexIn(event.description) == 0)
    {
      EventRating prograting;
      prograting.system="AU"; prograting.rating = rating.cap(1);
      event.ratings.push_back(prograting);
      event.description.remove(0,rating.matchedLength()+1);
    }
    if (event.description.startsWith("[HD]"))
    {
      event.videoProps |= VID_HDTV;
      event.description.remove(0,5);
    }
    if (event.description.startsWith("[CC]"))
    {
      event.subtitleType |= SUB_NORMAL;
      event.description.remove(0,5);
    }
    if (event.subtitle == "Movie")
    {
        event.subtitle = QString::null;
        event.categoryType = ProgramInfo::kCategoryMovie;
    }
    if (event.description.startsWith(event.title))
      event.description.remove(0,event.title.length()+1);
}
/** \fn EITFixUp::FixAUSeven(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (Seven network)
 */
void EITFixUp::FixAUSeven(DBEventEIT &event) const
{
    if (event.description.endsWith(" Rpt"))
    {
        event.previouslyshown = true;
        event.description.resize(event.description.size()-4);
    }
    QRegExp year("(\\d{4})$");
    if (year.indexIn(event.description) != -1)
    {
        event.airdate = year.cap(3).toUInt();
        event.description.resize(event.description.size()-5);
    }
    if (event.description.endsWith(" CC"))
    {
      event.subtitleType |= SUB_NORMAL;
      event.description.resize(event.description.size()-3);
    }
    QString advisories;//store the advisories to append later
    QRegExp adv("(\\([A-Z,]+\\))$");
    if (adv.indexIn(event.description) != -1)
    {
        advisories = adv.cap(1);
        event.description.resize(event.description.size()-(adv.matchedLength()+1));
    }
    QRegExp rating("(C|G|PG|M|MA)$");
    if (rating.indexIn(event.description) != -1)
    {
        EventRating prograting;
        prograting.system=""; prograting.rating = rating.cap(1);
        if (!advisories.isEmpty())
            prograting.rating.append(" ").append(advisories);
        event.ratings.push_back(prograting);
        event.description.resize(event.description.size()-(rating.matchedLength()+1));
    }
}
/** \fn EITFixUp::FixAUFreeview(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (generic freeview - extra info in brackets at end of desc)
 */
void EITFixUp::FixAUFreeview(DBEventEIT &event) const
{
    if (event.description.endsWith(".."))//has been truncated to fit within the 'subtitle' eit field, so none of the following will work (ABC)
        return;

    if (m_AUFreeviewSY.indexIn(event.description.trimmed(), 0) != -1)
    {
        if (event.subtitle.isEmpty())//nine sometimes has an actual subtitle field and the brackets thingo)
            event.subtitle = m_AUFreeviewSY.cap(2);
        event.airdate = m_AUFreeviewSY.cap(3).toUInt();
        event.description = m_AUFreeviewSY.cap(1);
    }
    else if (m_AUFreeviewY.indexIn(event.description.trimmed(), 0) != -1)
    {
        event.airdate = m_AUFreeviewY.cap(2).toUInt();
        event.description = m_AUFreeviewY.cap(1);
    }
    else if (m_AUFreeviewSYC.indexIn(event.description.trimmed(), 0) != -1)
    {
        if (event.subtitle.isEmpty())
            event.subtitle = m_AUFreeviewSYC.cap(2);
        event.airdate = m_AUFreeviewSYC.cap(3).toUInt();
        QStringList actors = m_AUFreeviewSYC.cap(4).split("/");
        for (int i = 0; i < actors.size(); ++i)
            event.AddPerson(DBPerson::kActor, actors.at(i));
        event.description = m_AUFreeviewSYC.cap(1);
    }
    else if (m_AUFreeviewYC.indexIn(event.description.trimmed(), 0) != -1)
    {
        event.airdate = m_AUFreeviewYC.cap(2).toUInt();
        QStringList actors = m_AUFreeviewYC.cap(3).split("/");
        for (int i = 0; i < actors.size(); ++i)
            event.AddPerson(DBPerson::kActor, actors.at(i));
        event.description = m_AUFreeviewYC.cap(1);
    }
}

/** \fn EITFixUp::FixMCA(DBEventEIT&) const
 *  \brief Use this to standardise the MultiChoice Africa DVB-S guide.
 */
void EITFixUp::FixMCA(DBEventEIT &event) const
{
    const uint SUBTITLE_PCT     = 60; // % of description to allow subtitle to
    const uint SUBTITLE_MAX_LEN = 128;// max length of subtitle field in db.
    int        position;
    QRegExp    tmpExp1;

    // Remove subtitle, it contains category information too specific to use
    event.subtitle = QString("");

    // No need to continue without a description.
    if (event.description.length() <= 0)
        return;

    // Replace incomplete title if the full one is in the description
    tmpExp1 = m_mcaIncompleteTitle;
    if (tmpExp1.indexIn(event.title) != -1)
    {
        tmpExp1 = QRegExp( QString(m_mcaCompleteTitlea.pattern() + tmpExp1.cap(1) +
                                   m_mcaCompleteTitleb.pattern()));
        tmpExp1.setCaseSensitivity(Qt::CaseInsensitive);
        if (tmpExp1.indexIn(event.description) != -1)
        {
            event.title       = tmpExp1.cap(1).trimmed();
            event.description = tmpExp1.cap(2).trimmed();
        }
        tmpExp1.setCaseSensitivity(Qt::CaseSensitive);
    }

    // Try to find subtitle in description
    tmpExp1 = m_mcaSubtitle;
    if ((position = tmpExp1.indexIn(event.description)) != -1)
    {
        uint tmpExp1Len = tmpExp1.cap(1).length();
        uint evDescLen = max(event.description.length(), 1);

        if ((tmpExp1Len < SUBTITLE_MAX_LEN) &&
            ((tmpExp1Len * 100 / evDescLen) < SUBTITLE_PCT))
        {
            event.subtitle    = tmpExp1.cap(1);
            event.description = tmpExp1.cap(2);
        }
    }

    // Try to find episode numbers in subtitle
    tmpExp1 = m_mcaSeries;
    if ((position = tmpExp1.indexIn(event.subtitle)) != -1)
    {
        uint season    = tmpExp1.cap(1).toUInt();
        uint episode   = tmpExp1.cap(2).toUInt();
        event.subtitle = tmpExp1.cap(3).trimmed();
        event.syndicatedepisodenumber =
                QString("S%1E%2").arg(season).arg(episode);
        event.season = season;
        event.episode = episode;
        event.categoryType = ProgramInfo::kCategorySeries;
    }

    // Close captioned?
    position = event.description.indexOf(m_mcaCC);
    if (position > 0)
    {
        event.subtitleType |= SUB_HARDHEAR;
        event.description.replace(m_mcaCC, "");
    }

    // Dolby Digital 5.1?
    position = event.description.indexOf(m_mcaDD);
    if ((position > 0) && (position > (int) (event.description.length() - 7)))
    {
        event.audioProps |= AUD_DOLBY;
        event.description.replace(m_mcaDD, "");
    }

    // Remove bouquet tags
    event.description.replace(m_mcaAvail, "");

    // Try to find year and director from the end of the description
    bool isMovie = false;
    tmpExp1  = m_mcaCredits;
    position = tmpExp1.indexIn(event.description);
    if (position != -1)
    {
        isMovie = true;
        event.description = tmpExp1.cap(1).trimmed();
        bool ok;
        uint y = tmpExp1.cap(2).trimmed().toUInt(&ok);
        if (ok)
            event.airdate = y;
        event.AddPerson(DBPerson::kDirector, tmpExp1.cap(3).trimmed());
    }
    else
    {
        // Try to find year only from the end of the description
        tmpExp1  = m_mcaYear;
        position = tmpExp1.indexIn(event.description);
        if (position != -1)
        {
            isMovie = true;
            event.description = tmpExp1.cap(1).trimmed();
            bool ok;
            uint y = tmpExp1.cap(2).trimmed().toUInt(&ok);
            if (ok)
                event.airdate = y;
        }
    }

    if (isMovie)
    {
        tmpExp1  = m_mcaActors;
        position = tmpExp1.indexIn(event.description);
        if (position != -1)
        {
            const QStringList actors = tmpExp1.cap(2).split(
                m_mcaActorsSeparator, QString::SkipEmptyParts);
            QStringList::const_iterator it = actors.begin();
            for (; it != actors.end(); ++it)
                event.AddPerson(DBPerson::kActor, (*it).trimmed());
            event.description = tmpExp1.cap(1).trimmed();
        }
        event.categoryType = ProgramInfo::kCategoryMovie;
    }

}

/** \fn EITFixUp::FixRTL(DBEventEIT&) const
 *  \brief Use this to standardise the RTL group guide in Germany.
 */
void EITFixUp::FixRTL(DBEventEIT &event) const
{
    int        pos;

    // No need to continue without a description or with an subtitle.
    if (event.description.length() <= 0 || event.subtitle.length() > 0)
        return;

    // Repeat
    QRegExp tmpExpRepeat = m_RTLrepeat;
    if ((pos = tmpExpRepeat.indexIn(event.description)) != -1)
    {
        // remove '.' if it matches at the beginning of the description
        int length = tmpExpRepeat.cap(0).length() + (pos ? 0 : 1);
        event.description = event.description.remove(pos, length).trimmed();
    }

    QRegExp tmpExp1 = m_RTLSubtitle;
    QRegExp tmpExpSubtitle1 = m_RTLSubtitle1;
    tmpExpSubtitle1.setMinimal(true);
    QRegExp tmpExpSubtitle2 = m_RTLSubtitle2;
    QRegExp tmpExpSubtitle3 = m_RTLSubtitle3;
    QRegExp tmpExpSubtitle4 = m_RTLSubtitle4;
    QRegExp tmpExpSubtitle5 = m_RTLSubtitle5;
    tmpExpSubtitle5.setMinimal(true);
    QRegExp tmpExpEpisodeNo1 = m_RTLEpisodeNo1;
    QRegExp tmpExpEpisodeNo2 = m_RTLEpisodeNo2;

    // subtitle with episode number: "Folge *: 'subtitle'. description
    if (tmpExpSubtitle1.indexIn(event.description) != -1)
    {
        event.syndicatedepisodenumber = tmpExpSubtitle1.cap(1);
        event.subtitle    = tmpExpSubtitle1.cap(2);
        event.description =
            event.description.remove(0, tmpExpSubtitle1.matchedLength());
    }
    // episode number subtitle
    else if (tmpExpSubtitle2.indexIn(event.description) != -1)
    {
        event.syndicatedepisodenumber = tmpExpSubtitle2.cap(1);
        event.subtitle    = tmpExpSubtitle2.cap(2);
        event.description =
            event.description.remove(0, tmpExpSubtitle2.matchedLength());
    }
    // episode number subtitle
    else if (tmpExpSubtitle3.indexIn(event.description) != -1)
    {
        event.syndicatedepisodenumber = tmpExpSubtitle3.cap(1);
        event.subtitle    = tmpExpSubtitle3.cap(2);
        event.description =
            event.description.remove(0, tmpExpSubtitle3.matchedLength());
    }
    // "Thema..."
    else if (tmpExpSubtitle4.indexIn(event.description) != -1)
    {
        event.subtitle    = tmpExpSubtitle4.cap(1);
        event.description =
            event.description.remove(0, tmpExpSubtitle4.matchedLength());
    }
    // "'...'"
    else if (tmpExpSubtitle5.indexIn(event.description) != -1)
    {
        event.subtitle    = tmpExpSubtitle5.cap(1);
        event.description =
            event.description.remove(0, tmpExpSubtitle5.matchedLength());
    }
    // episode number
    else if (tmpExpEpisodeNo1.indexIn(event.description) != -1)
    {
        event.syndicatedepisodenumber = tmpExpEpisodeNo1.cap(2);
        event.subtitle    = tmpExpEpisodeNo1.cap(1);
        event.description =
            event.description.remove(0, tmpExpEpisodeNo1.matchedLength());
    }
    // episode number
    else if (tmpExpEpisodeNo2.indexIn(event.description) != -1)
    {
        event.syndicatedepisodenumber = tmpExpEpisodeNo2.cap(2);
        event.subtitle    = tmpExpEpisodeNo2.cap(1);
        event.description =
            event.description.remove(0, tmpExpEpisodeNo2.matchedLength());
    }

    /* got an episode title now? (we did not have one at the start of this function) */
    if (!event.subtitle.isEmpty())
    {
        event.categoryType = ProgramInfo::kCategorySeries;
    }

    /* if we do not have an episode title by now try some guessing as last resort */
    if (event.subtitle.length() == 0)
    {
        const uint SUBTITLE_PCT = 35; // % of description to allow subtitle up to
        const uint SUBTITLE_MAX_LEN = 50; // max length of subtitle field in db

        if (tmpExp1.indexIn(event.description) != -1)
        {
            uint tmpExp1Len = tmpExp1.cap(1).length();
            uint evDescLen = max(event.description.length(), 1);

            if ((tmpExp1Len < SUBTITLE_MAX_LEN) &&
                (tmpExp1Len * 100 / evDescLen < SUBTITLE_PCT))
            {
                event.subtitle    = tmpExp1.cap(1);
                event.description = tmpExp1.cap(2);
            }
        }
    }
}

/** \fn EITFixUp::FixPRO7(DBEventEIT&) const
 *  \brief Use this to standardise the PRO7/Sat1 group guide in Germany.
 */
void EITFixUp::FixPRO7(DBEventEIT &event) const
{
    QRegExp tmp = m_PRO7Subtitle;

    int pos = tmp.indexIn(event.subtitle);
    if (pos != -1)
    {
        if (event.airdate == 0)
        {
            event.airdate = tmp.cap(3).toUInt();
        }
        event.subtitle.replace(tmp, "");
    }

    /* handle cast, the very last in description */
    tmp = m_PRO7Cast;
    pos = tmp.indexIn(event.description);
    if (pos != -1)
    {
        QStringList cast = tmp.cap(1).split("\n");
        QRegExp tmpOne = m_PRO7CastOne;
        QStringListIterator i(cast);
        while (i.hasNext())
        {
            pos = tmpOne.indexIn (i.next());
            if (pos != -1)
            {
                event.AddPerson (DBPerson::kActor, tmpOne.cap(1).simplified());
            }
        }
        event.description.replace(tmp, "");
    }

    /* handle crew, the new very last in description
     * format: "Role: Name" or "Role: Name1, Name2"
     */
    tmp = m_PRO7Crew;
    pos = tmp.indexIn(event.description);
    if (pos != -1)
    {
        QStringList crew = tmp.cap(1).split("\n");
        QRegExp tmpOne = m_PRO7CrewOne;
        QStringListIterator i(crew);
        while (i.hasNext())
        {
            pos = tmpOne.indexIn (i.next());
            if (pos != -1)
            {
                DBPerson::Role role = DBPerson::kUnknown;
                if (QString::compare (tmpOne.cap(1), "Regie") == 0)
                {
                    role = DBPerson::kDirector;
                }
                else if (QString::compare (tmpOne.cap(1), "Drehbuch") == 0)
                {
                    role = DBPerson::kWriter;
                }
                else if (QString::compare (tmpOne.cap(1), "Autor") == 0)
                {
                    role = DBPerson::kWriter;
                }
                // FIXME add more jobs

                QStringList names = tmpOne.cap(2).simplified().split("\\s*,\\s*");
                QStringListIterator j(names);
                while (j.hasNext())
                {
                    event.AddPerson (role, j.next());
                }
            }
        }
        event.description.replace(tmp, "");
    }

    /* FIXME unless its Jamie Oliver, then there is neither Crew nor Cast only
     * \n\nKoch: Jamie Oliver
     */
}

/** \fn EITFixUp::FixFI(DBEventEIT&) const
 *  \brief Use this to clean DVB-T guide in Finland.
 */
void EITFixUp::FixFI(DBEventEIT &event) const
{
    int position = event.description.indexOf(m_fiRerun);
    if (position != -1)
    {
        event.previouslyshown = true;
        event.description = event.description.replace(m_fiRerun, "");
    }

    position = event.description.indexOf(m_fiRerun2);
    if (position != -1)
    {
        event.previouslyshown = true;
        event.description = event.description.replace(m_fiRerun2, "");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.description.indexOf(m_Stereo);
    if (position != -1)
    {
        event.audioProps |= AUD_STEREO;
        event.description = event.description.replace(m_Stereo, "");
    }
}

/** \fn EITFixUp::FixPremiere(DBEventEIT&) const
 *  \brief Use this to standardize DVB-C guide in Germany
 *         for the providers Kabel Deutschland and Premiere.
 */
void EITFixUp::FixPremiere(DBEventEIT &event) const
{
    QString country = "";

    // Find infos about country and year, regisseur and actors
    QRegExp tmpInfos =  m_dePremiereInfos;
    if (tmpInfos.indexIn(event.description) != -1)
    {
        country = tmpInfos.cap(2).trimmed();
        bool ok;
        uint y = tmpInfos.cap(1).toUInt(&ok);
        if (ok)
            event.airdate = y;
        event.AddPerson(DBPerson::kDirector, tmpInfos.cap(3));
        const QStringList actors = tmpInfos.cap(4).split(
            ", ", QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
            event.AddPerson(DBPerson::kActor, *it);
        event.description = event.description.replace(tmpInfos, "");
    }

    // move the original titel from the title to subtitle
    QRegExp tmpOTitle = m_dePremiereOTitle;
    if (tmpOTitle.indexIn(event.title) != -1)
    {
        event.subtitle = QString("%1, %2").arg(tmpOTitle.cap(1)).arg(country);
        event.title = event.title.replace(tmpOTitle, "");
    }

    // Find infos about season and episode number
    QRegExp tmpSeasonEpisode =  m_deSkyDescriptionSeasonEpisode;
    if (tmpSeasonEpisode.indexIn(event.description) != -1)
    {
        event.season = tmpSeasonEpisode.cap(1).trimmed().toUInt();
        event.episode = tmpSeasonEpisode.cap(2).trimmed().toUInt();
        event.description.replace(tmpSeasonEpisode, "");
    }
}

/** \fn EITFixUp::FixNL(DBEventEIT&) const
 *  \brief Use this to standardize \@Home DVB-C guide in the Netherlands.
 */
void EITFixUp::FixNL(DBEventEIT &event) const
{
    QString fullinfo = "";
    fullinfo.append (event.subtitle);
    fullinfo.append (event.description);
    event.subtitle = "";

    // Convert categories to Dutch categories Myth knows.
    // nog invoegen: comedy, sport, misdaad

    if (event.category == "Documentary")
    {
        event.category = "Documentaire";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "News")
    {
        event.category = "Nieuws/actualiteiten";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Kids")
    {
        event.category = "Jeugd";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Show/game Show")
    {
        event.category = "Amusement";
        event.categoryType = ProgramInfo::kCategoryTVShow;
    }
    if (event.category == "Music/Ballet/Dance")
    {
        event.category = "Muziek";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "News magazine")
    {
        event.category = "Informatief";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Movie")
    {
        event.category = "Film";
        event.categoryType = ProgramInfo::kCategoryMovie;
    }
    if (event.category == "Nature/animals/Environment")
    {
        event.category = "Natuur";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Movie - Adult")
    {
        event.category = "Erotiek";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Movie - Soap/melodrama/folkloric")
    {
        event.category = "Serie/soap";
        event.categoryType = ProgramInfo::kCategorySeries;
    }
    if (event.category == "Arts/Culture")
    {
        event.category = "Kunst/Cultuur";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Sports")
    {
        event.category = "Sport";
        event.categoryType = ProgramInfo::kCategorySports;
    }
    if (event.category == "Cartoons/Puppets")
    {
        event.category = "Animatie";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Movie - Comedy")
    {
        event.category = "Comedy";
        event.categoryType = ProgramInfo::kCategorySeries;
    }
    if (event.category == "Movie - Detective/Thriller")
    {
        event.category = "Misdaad";
        event.categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.category == "Social/Spiritual Sciences")
    {
        event.category = "Religieus";
        event.categoryType = ProgramInfo::kCategoryNone;
    }

    // Film - categories are usually not Films
    if (event.category.startsWith("Film -"))
    {
        event.categoryType = ProgramInfo::kCategorySeries;
    }

    // Get stereo info
    if (fullinfo.indexOf(m_Stereo) != -1)
    {
        event.audioProps |= AUD_STEREO;
        fullinfo = fullinfo.replace(m_Stereo, ".");
    }

    //Get widescreen info
    if (fullinfo.indexOf(m_nlWide) != -1)
    {
        fullinfo = fullinfo.replace("breedbeeld", ".");
    }

    // Get repeat info
    if (fullinfo.indexOf(m_nlRepeat) != -1)
    {
        fullinfo = fullinfo.replace("herh.", ".");
    }

    // Get teletext subtitle info
    if (fullinfo.indexOf(m_nlTxt) != -1)
    {
        event.subtitleType |= SUB_NORMAL;
        fullinfo = fullinfo.replace("txt", ".");
    }

    // Get HDTV information
    if (event.title.indexOf(m_nlHD) != -1)
    {
        event.videoProps |= VID_HDTV;
        event.title = event.title.replace(m_nlHD, "");
    }

    // Try to make subtitle from Afl.:
    QRegExp tmpSub = m_nlSub;
    QString tmpSubString;
    if (tmpSub.indexIn(fullinfo) != -1)
    {
        tmpSubString = tmpSub.cap(0);
        tmpSubString = tmpSubString.right(tmpSubString.length() - 7);
        event.subtitle = tmpSubString.left(tmpSubString.length() -1);
        fullinfo = fullinfo.replace(tmpSub.cap(0), "");
    }

    // Try to make subtitle from " "
    QRegExp tmpSub2 = m_nlSub2;
    //QString tmpSubString2;
    if (tmpSub2.indexIn(fullinfo) != -1)
    {
        tmpSubString = tmpSub2.cap(0);
        tmpSubString = tmpSubString.right(tmpSubString.length() - 2);
        event.subtitle = tmpSubString.left(tmpSubString.length() -1);
        fullinfo = fullinfo.replace(tmpSub2.cap(0), "");
    }


    // This is trying to catch the case where the subtitle is in the main title
    // but avoid cases where it isn't a subtitle e.g cd:uk
    int position;
    if (((position = event.title.indexOf(":")) != -1) &&
        (event.title[position + 1].toUpper() == event.title[position + 1]) &&
        (event.subtitle.isEmpty()))
    {
        event.subtitle = event.title.mid(position + 1);
        event.title = event.title.left(position);
    }


    // Get the actors
    QRegExp tmpActors = m_nlActors;
    if (tmpActors.indexIn(fullinfo) != -1)
    {
        QString tmpActorsString = tmpActors.cap(0);
        tmpActorsString = tmpActorsString.right(tmpActorsString.length() - 6);
        tmpActorsString = tmpActorsString.left(tmpActorsString.length() - 5);
        const QStringList actors =
            tmpActorsString.split(", ", QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
            event.AddPerson(DBPerson::kActor, *it);
        fullinfo = fullinfo.replace(tmpActors.cap(0), "");
    }

    // Try to find presenter
    QRegExp tmpPres = m_nlPres;
    if (tmpPres.indexIn(fullinfo) != -1)
    {
        QString tmpPresString = tmpPres.cap(0);
        tmpPresString = tmpPresString.right(tmpPresString.length() - 14);
        tmpPresString = tmpPresString.left(tmpPresString.length() -1);
        const QStringList host =
            tmpPresString.split(m_nlPersSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = host.begin();
        for (; it != host.end(); ++it)
            event.AddPerson(DBPerson::kPresenter, *it);
        fullinfo = fullinfo.replace(tmpPres.cap(0), "");
    }

    // Try to find year
    QRegExp tmpYear1 = m_nlYear1;
    QRegExp tmpYear2 = m_nlYear2;
    if (tmpYear1.indexIn(fullinfo) != -1)
    {
        bool ok;
        uint y = tmpYear1.cap(0).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
    }

    if (tmpYear2.indexIn(fullinfo) != -1)
    {
        bool ok;
        uint y = tmpYear2.cap(2).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
    }

    // Try to find director
    QRegExp tmpDirector = m_nlDirector;
    QString tmpDirectorString;
    if (fullinfo.indexOf(m_nlDirector) != -1)
    {
        tmpDirectorString = tmpDirector.cap(0);
        event.AddPerson(DBPerson::kDirector, tmpDirectorString);
    }

    // Strip leftovers
    if (fullinfo.indexOf(m_nlRub) != -1)
    {
        fullinfo = fullinfo.replace(m_nlRub, "");
    }

    // Strip category info from description
    if (fullinfo.indexOf(m_nlCat) != -1)
    {
        fullinfo = fullinfo.replace(m_nlCat, "");
    }

    // Remove omroep from title
    if (event.title.indexOf(m_nlOmroep) != -1)
    {
        event.title = event.title.replace(m_nlOmroep, "");
    }

    // Put information back in description

    event.description = fullinfo;
    event.description = event.description.trimmed();
    event.title       = event.title.trimmed();
    event.subtitle    = event.subtitle.trimmed();

}

void EITFixUp::FixCategory(DBEventEIT &event) const
{
    // remove category movie from short events
    if (event.categoryType == ProgramInfo::kCategoryMovie &&
        event.starttime.secsTo(event.endtime) < kMinMovieDuration)
    {
        /* default taken from ContentDescriptor::GetMythCategory */
        event.categoryType = ProgramInfo::kCategoryTVShow;
    }
}

/** \fn EITFixUp::FixNO(DBEventEIT&) const
 *  \brief Use this to clean DVB-S guide in Norway.
 */
void EITFixUp::FixNO(DBEventEIT &event) const
{
    // Check for "title (R)" in the title
    int position = event.title.indexOf(m_noRerun);
    if (position != -1)
    {
      event.previouslyshown = true;
      event.title = event.title.replace(m_noRerun, "");
    }
    // Check for "subtitle (HD)" in the subtitle
    position = event.subtitle.indexOf(m_noHD);
    if (position != -1)
    {
      event.videoProps |= VID_HDTV;
      event.subtitle = event.subtitle.replace(m_noHD, "");
    }
   // Check for "description (HD)" in the description
    position = event.description.indexOf(m_noHD);
    if (position != -1)
    {
      event.videoProps |= VID_HDTV;
      event.description = event.description.replace(m_noHD, "");
    }
}

/** \fn EITFixUp::FixNRK_DVBT(DBEventEIT&) const
 *  \brief Use this to clean DVB-T guide in Norway (NRK)
 */
void EITFixUp::FixNRK_DVBT(DBEventEIT &event) const
{
    QRegExp    tmpExp1;
    // Check for "title (R)" in the title
    if (event.title.indexOf(m_noRerun) != -1)
    {
      event.previouslyshown = true;
      event.title = event.title.replace(m_noRerun, "");
    }
    // Check for "(R)" in the description
    if (event.description.indexOf(m_noRerun) != -1)
    {
      event.previouslyshown = true;
    }
    // Move colon separated category from program-titles into description
    // Have seen "NRK2s historiekveld: Film: bla-bla"
    tmpExp1 =  m_noNRKCategories;
    while ((tmpExp1.indexIn(event.title) != -1) &&
           (tmpExp1.cap(2).length() > 1))
    {
        event.title  = tmpExp1.cap(2);
        event.description = "(" + tmpExp1.cap(1) + ") " + event.description;
    }
    // Remove season premiere markings
    tmpExp1 = m_noPremiere;
    if (tmpExp1.indexIn(event.title) >= 3)
    {
        event.title.remove(m_noPremiere);
    }
    // Try to find colon-delimited subtitle in title, only tested for NRK channels
    tmpExp1 = m_noColonSubtitle;
    if (!event.title.startsWith("CSI:") &&
        !event.title.startsWith("CD:") &&
        !event.title.startsWith("Distriktsnyheter: fra"))
    {
        if (tmpExp1.indexIn(event.title) != -1)
        {

            if (event.subtitle.length() <= 0)
            {
                event.title    = tmpExp1.cap(1);
                event.subtitle = tmpExp1.cap(2);
            }
            else if (event.subtitle == tmpExp1.cap(2))
            {
                event.title    = tmpExp1.cap(1);
            }
        }
    }
}

/** \fn EITFixUp::FixDK(DBEventEIT&) const
 *  \brief Use this to clean YouSee's DVB-C guide in Denmark.
 */
void EITFixUp::FixDK(DBEventEIT &event) const
{
    // Source: YouSee Rules of Operation v1.16
    // url: http://yousee.dk/~/media/pdf/CPE/Rules_Operation.ashx
    int        position = -1;
    int        episode = -1;
    int        season = -1;
    QRegExp    tmpRegEx;
    // Title search
    // episode and part/part total
    tmpRegEx = m_dkEpisode;
    position = event.title.indexOf(tmpRegEx);
    if (position != -1)
    {
      episode = tmpRegEx.cap(1).toInt();
      event.partnumber = tmpRegEx.cap(1).toInt();
      event.title = event.title.replace(tmpRegEx, "");
    }

    tmpRegEx = m_dkPart;
    position = event.title.indexOf(tmpRegEx);
    if (position != -1)
    {
      episode = tmpRegEx.cap(1).toInt();
      event.partnumber = tmpRegEx.cap(1).toInt();
      event.parttotal = tmpRegEx.cap(2).toInt();
      event.title = event.title.replace(tmpRegEx, "");
    }

    // subtitle delimiters
    tmpRegEx = m_dkSubtitle1;
    position = event.title.indexOf(tmpRegEx);
    if (position != -1)
    {
      event.title = tmpRegEx.cap(1);
      event.subtitle = tmpRegEx.cap(2);
    }
    else
    {
        tmpRegEx = m_dkSubtitle2;
        if(event.title.indexOf(tmpRegEx) != -1)
        {
            event.title = tmpRegEx.cap(1);
            event.subtitle = tmpRegEx.cap(2);
        }
    }
    // Description search
    // Season (Sæson [:digit:]+.) => episode = season episode number
    // or year (- år [:digit:]+(\\)|:) ) => episode = total episode number
    tmpRegEx = m_dkSeason1;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
      season = tmpRegEx.cap(1).toInt();
    }
    else
    {
        tmpRegEx = m_dkSeason2;
        if(event.description.indexOf(tmpRegEx) !=  -1)
        {
            season = tmpRegEx.cap(1).toInt();
        }
    }

    if (episode > 0)
        event.episode = episode;

    if (season > 0)
        event.season = season;

    //Feature:
    tmpRegEx = m_dkFeatures;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString features = tmpRegEx.cap(1);
        event.description = event.description.replace(tmpRegEx, "");
        // 16:9
        if (features.indexOf(m_dkWidescreen) !=  -1)
            event.videoProps |= VID_WIDESCREEN;
        // HDTV
        if (features.indexOf(m_dkHD) !=  -1)
            event.videoProps |= VID_HDTV;
        // Dolby Digital surround
        if (features.indexOf(m_dkDolby) !=  -1)
            event.audioProps |= AUD_DOLBY;
        // surround
        if (features.indexOf(m_dkSurround) !=  -1)
            event.audioProps |= AUD_SURROUND;
        // stereo
        if (features.indexOf(m_dkStereo) !=  -1)
            event.audioProps |= AUD_STEREO;
        // (G)
        if (features.indexOf(m_dkReplay) !=  -1)
            event.previouslyshown = true;
        // TTV
        if (features.indexOf(m_dkTxt) !=  -1)
            event.subtitleType |= SUB_NORMAL;
    }

    // Series and program id
    // programid is currently not transmitted
    // YouSee doesn't use a default authority but uses the first byte after
    // the / to indicate if the seriesid is global unique or unique on the
    // service id
    if (event.seriesId.length() >= 1 && event.seriesId[0] == '/')
    {
        QString newid;
        if (event.seriesId[1] == '1')
            newid = QString("%1%2").arg(event.chanid).
                    arg(event.seriesId.mid(2,8));
        else
            newid = event.seriesId.mid(2,8);
        event.seriesId = newid;
    }

    if (event.programId.length() >= 1 && event.programId[0] == '/')
        event.programId[0]='_';

    // Add season and episode number to subtitle
    if (episode > 0)
    {
        event.subtitle = QString("%1 (%2").arg(event.subtitle).arg(episode);
        if (event.parttotal >0)
            event.subtitle = QString("%1:%2").arg(event.subtitle).
                    arg(event.parttotal);
        if (season > 0)
        {
            event.season = season;
            event.episode = episode;
            event.syndicatedepisodenumber =
                    QString("S%1E%2").arg(season).arg(episode);
            event.subtitle = QString("%1 S\xE6son %2").arg(event.subtitle).
                    arg(season);
        }
        event.subtitle = QString("%1)").arg(event.subtitle);
    }

    // Find actors and director in description
    tmpRegEx = m_dkDirector;
    bool directorPresent = false;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString tmpDirectorsString = tmpRegEx.cap(1);
        const QStringList directors =
            tmpDirectorsString.split(m_dkPersonsSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = directors.begin();
        for (; it != directors.end(); ++it)
        {
            tmpDirectorsString = it->split(":").last().trimmed().
                    remove(QRegExp("\\.$"));
            if (tmpDirectorsString != "")
                event.AddPerson(DBPerson::kDirector, tmpDirectorsString);
        }
        directorPresent = true;
    }

    tmpRegEx = m_dkActors;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString tmpActorsString = tmpRegEx.cap(1);
        if (directorPresent)
            tmpActorsString = tmpActorsString.replace(m_dkDirector,"");
        const QStringList actors =
            tmpActorsString.split(m_dkPersonsSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
        {
            tmpActorsString = it->split(":").last().trimmed().
                    remove(QRegExp("\\.$"));
            if (tmpActorsString != "")
                event.AddPerson(DBPerson::kActor, tmpActorsString);
        }
    }
    //find year
    tmpRegEx = m_dkYear;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        bool ok;
        uint y = tmpRegEx.cap(1).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
    }
    // Remove white spaces
    event.description = event.description.trimmed();
    event.title       = event.title.trimmed();
    event.subtitle    = event.subtitle.trimmed();
}

/** \fn EITFixUp::FixStripHTML(DBEventEIT&) const
 *  \brief Use this to clean HTML Tags from EIT Data
 */
void EITFixUp::FixStripHTML(DBEventEIT &event) const
{
    LOG(VB_EIT, LOG_INFO, QString("Applying html strip to %1").arg(event.title));
    event.title.remove(m_HTML);
}

// Moves the subtitle field into the description since it's just used
// as more description field. All the sort-out will happen in the description
// field. Also, sometimes the description is just a repeat of the title. If so,
// we remove it.
void EITFixUp::FixGreekSubtitle(DBEventEIT &event) const
{
    if (event.title == event.description)
    {
        event.description = QString("");
    }
    if (!event.subtitle.isEmpty())
    {
        if (event.subtitle.trimmed().right(1) != ".'" )
            event.subtitle = event.subtitle.trimmed() + ".";
        event.description = event.subtitle.trimmed() + QString(" ") + event.description;
        event.subtitle = QString("");
    }
}

void EITFixUp::FixGreekEIT(DBEventEIT &event) const
{
    //Live show
    int position;
    QRegExp tmpRegEx;
    position = event.title.indexOf("(Ζ)");
    if (position != -1)
    {
        event.title = event.title.replace("(Ζ)", "");
        event.description.prepend("Ζωντανή Μετάδοση. ");
    }

    // Greek not previously Shown
    position = event.title.indexOf(m_grNotPreviouslyShown);
    if (position != -1)
    {
        event.previouslyshown = false;
        event.title = event.title.replace(m_grNotPreviouslyShown.cap(0), "");
    }

    // Greek Replay (Ε)
    // it might look redundant compared to previous check but at least it helps
    // remove the (Ε) From the title.
    tmpRegEx = m_grReplay;
    if (event.title.indexOf(tmpRegEx) !=  -1)
    {
        event.previouslyshown = true;
        event.title = event.title.replace(tmpRegEx, "");
    }

    tmpRegEx = m_grFixnofullstopActors;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.description.insert(position + 1, ".");
    }

    // If they forgot the "." at the end of the sentence before the actors/directors begin, let's insert it.
    tmpRegEx = m_grFixnofullstopDirectors;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.description.insert(position + 1, ".");
    }

    // Find actors and director in description
    // I am looking for actors first and then for directors/presenters because
    // sometimes punctuation is missing and the "Παίζουν:" label is mistaken
    // for a director's/presenter's surname (directors/presenters are shown
    // before actors in the description field.). So removing the text after
    // adding the actors AND THEN looking for dir/pres helps to clear things up.
    tmpRegEx = m_grActors;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString tmpActorsString = tmpRegEx.cap(1);
        const QStringList actors =
            tmpActorsString.split(m_grPeopleSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
        {
            tmpActorsString = it->split(":").last().trimmed().
                    remove(QRegExp("\\.$"));
            if (tmpActorsString != "")
                event.AddPerson(DBPerson::kActor, tmpActorsString);
        }
        event.description.replace(tmpRegEx.cap(0), "");
    }
    // Director
    tmpRegEx = m_grDirector;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString tmpDirectorsString = tmpRegEx.cap(1);
        const QStringList directors =
            tmpDirectorsString.split(m_grPeopleSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = directors.begin();
        for (; it != directors.end(); ++it)
        {
            tmpDirectorsString = it->split(":").last().trimmed().
                    remove(QRegExp("\\.$"));
            if (tmpDirectorsString != "")
            {
                event.AddPerson(DBPerson::kDirector, tmpDirectorsString);
            }
        }
        event.description.replace(tmpRegEx.cap(0), "");
    }

/*

    tmpRegEx = m_grDirector;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        // director is captured in cap(1).Due to sometimes miswritten data in
        // eit, e.g no fullstop present(!)
        QString tmpDirectorsString = tmpRegEx.cap(1);
        // look for actors in director string (lack of fullstop, etc)
        tmpRegEx = m_grActors;
        int actposition = tmpDirectorsString.indexOf(tmpRegEx);
        if (actposition != -1)
        {
            tmpDirectorsString = tmpDirectorsString.replace(tmpRegEx,"");
        }
        const QStringList directors =
            tmpDirectorsString.split(m_grPeopleSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = directors.begin();
        for (; it != directors.end(); ++it)
        {
            tmpDirectorsString = it->split(":").last().trimmed().
                    remove(QRegExp("\\.$"));
            if (tmpDirectorsString != "")
                event.AddPerson(DBPerson::kDirector, tmpDirectorsString);

        }
        event.description.replace(tmpRegEx.cap(0), "");
       // directorPresent = true;
    }
*/
    //Try to find presenter
    tmpRegEx = m_grPres;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString tmpPresentersString = tmpRegEx.cap(1);
        const QStringList presenters =
            tmpPresentersString.split(m_grPeopleSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = presenters.begin();
        for (; it != presenters.end(); ++it)
        {
            tmpPresentersString = it->split(":").last().trimmed().
                    remove(QRegExp("\\.$"));
            if (tmpPresentersString != "")
            {
                event.AddPerson(DBPerson::kPresenter, tmpPresentersString);
            }
        }
        event.description.replace(tmpRegEx.cap(0), "");
    }

    //find year e.g Παραγωγής 1966 ή ΝΤΟΚΙΜΑΝΤΕΡ - 1998 Κατάλληλο για όλους
    // Used in Private channels (not 'secret', just not owned by Government!)
    tmpRegEx = m_grYear;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        bool ok;
        uint y = tmpRegEx.cap(1).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
            event.description.replace(tmpRegEx, "");
    }
    // Remove white spaces
    event.description = event.description.trimmed();
    event.title       = event.title.trimmed();
    event.subtitle    = event.subtitle.trimmed();

    //find country of origin and remove it from description.
    tmpRegEx = m_grCountry;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.description.replace(tmpRegEx, "");
    }

    // Work out the season and episode numbers (if any)
    // Matching pattern "Επεισ[όο]διο:?|Επ 3 από 14|3/14" etc
    bool    series  = false;
    QRegExp tmpSeries = m_grSeason;
    // cap(2) is the season for ΑΒΓΔ
    // cap(3) is the season for 1234
    int position1 = tmpSeries.indexIn(event.title);
    int position2 = tmpSeries.indexIn(event.description);
    if ((position1 != -1) || (position2 != -1))
    {
        if (!tmpSeries.cap(2).isEmpty()) // we found a letter representing a number
        {
            //sometimes Nat. TV writes numbers as letters, i.e Α=1, Β=2, Γ=3, etc
            //must convert them to numbers.
            int tmpinteger = tmpSeries.cap(2).toUInt();
            if (tmpinteger < 1)
            {
                if (tmpSeries.cap(2) == "ΣΤ") // 6, don't ask!
                    event.season = 6;
                else
                {
                    QString LettToNumber = "0ΑΒΓΔΕ6ΖΗΘΙΚΛΜΝ";
                    tmpinteger = LettToNumber.indexOf(tmpSeries.cap(2));
                    if (tmpinteger != -1)
                        event.season = tmpinteger;
                }
            }
        }
        else if (!tmpSeries.cap(3).isEmpty()) //number
        {
            event.season = tmpSeries.cap(3).toUInt();
        }
        series = true;
        if (position1 != -1)
            event.title.replace(tmpSeries.cap(0),"");
        if (position2 != -1)
            event.description.replace(tmpSeries.cap(0),"");
    }

    QRegExp tmpEpisode = m_grlongEp;
    //tmpEpisode.setMinimal(true);
    // cap(1) is the Episode No.
    if ((position1 = tmpEpisode.indexIn(event.title)) != -1
            || (position2 = tmpEpisode.indexIn(event.description)) != -1)
    {
        if (!tmpEpisode.cap(1).isEmpty())
        {
            event.episode = tmpEpisode.cap(1).toUInt();
            series = true;
            if (position1 != -1)
                event.title.replace(tmpEpisode.cap(0),"");
            if (position2 != -1)
                event.description.replace(tmpEpisode.cap(0),"");
            // Sometimes description omits Season if it's 1. We fix this
            if (0 == event.season)
                event.season = 1;
        }
    }

    // Sometimes the real (mostly English) title of a movie or series is
    // enclosed in parentheses in the event title, subtitle or description.
    // Since the subtitle has been moved to the description field by
    // EITFixUp::FixGreekSubtitle, I will search for it only in the description.
    // It will replace the translated one to get better chances of metadata
    // retrieval. The old title will be moved in the description.
    QRegExp tmptitle = m_grRealTitleinDescription;
    tmptitle.setMinimal(true);
    position = event.description.indexOf(tmptitle);
    if (position != -1)
    {
        event.description = event.description.replace(tmptitle, "");
        if (QString(tmptitle.cap(0)) != event.title.trimmed())
        {
            event.description = "(" + event.title.trimmed() + "). " + event.description;
        }
        event.title = tmptitle.cap(1);
        // Remove the real title from the description
    }
    else // search in title
    {
        tmptitle = m_grRealTitleinTitle;
        position = event.title.indexOf(tmptitle);
        if (position != -1) // found in title instead
        {
            event.title.replace(tmptitle.cap(0),"");
            QString tmpTranslTitle = event.title;
            //QString tmpTranslTitle = event.title.replace(tmptitle.cap(0),"");
            event.title = tmptitle.cap(1);
            event.description = "(" + tmpTranslTitle.trimmed() + "). " + event.description;
        }
    }

    // Description field: "^Episode: Lion in the cage. (Description follows)"
    tmpRegEx = m_grEpisodeAsSubtitle;
    position = event.description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.subtitle = tmpRegEx.cap(1).trimmed();
        event.description.replace(tmpRegEx, "");
    }
    QRegExp m_grMovie("\\bταιν[ιί]α\\b",Qt::CaseInsensitive);
    bool isMovie = (event.description.indexOf(m_grMovie) !=-1) ;
    if (isMovie)
    {
        event.categoryType = ProgramInfo::kCategoryMovie;
    }
    else if (series)
    {
        event.categoryType = ProgramInfo::kCategorySeries;
    }
    // just for luck, retrim fields.
    event.description = event.description.trimmed();
    event.title       = event.title.trimmed();
    event.subtitle    = event.subtitle.trimmed();

// να σβήσω τα κομμάτια που περισσεύουν από την περιγραφή πχ παραγωγής χχχχ
}

void EITFixUp::FixGreekCategories(DBEventEIT &event) const
{
    if (event.description.indexOf(m_grCategComedy) != -1)
    {
        event.category = "Κωμωδία";
    }
    else if (event.description.indexOf(m_grCategTeleMag) != -1)
    {
        event.category = "Τηλεπεριοδικό";
    }
    else if (event.description.indexOf(m_grCategNature) != -1)
    {
        event.category = "Επιστήμη/Φύση";
    }
    else if (event.description.indexOf(m_grCategHealth) != -1)
    {
        event.category = "Υγεία";
    }
    else if (event.description.indexOf(m_grCategReality) != -1)
    {
        event.category = "Ριάλιτι";
    }
    else if (event.description.indexOf(m_grCategDrama) != -1)
    {
        event.category = "Κοινωνικό";
    }
    else if (event.description.indexOf(m_grCategChildren) != -1)
    {
        event.category = "Παιδικό";
    }
    else if (event.description.indexOf(m_grCategSciFi) != -1)
    {
        event.category = "Επιστ.Φαντασίας";
    }
    else if ((event.description.indexOf(m_grCategFantasy) != -1)
             && (event.description.indexOf(m_grCategMystery) != -1))
    {
        event.category = "Φαντασίας/Μυστηρίου";
    }
    else if (event.description.indexOf(m_grCategMystery) != -1)
    {
        event.category = "Μυστηρίου";
    }
    else if (event.description.indexOf(m_grCategFantasy) != -1)
    {
        event.category = "Φαντασίας";
    }
    else if (event.description.indexOf(m_grCategHistory) != -1)
    {
        event.category = "Ιστορικό";
    }
    else if (event.description.indexOf(m_grCategTeleShop) != -1
            || event.title.indexOf(m_grCategTeleShop) != -1)
    {
        event.category = "Τηλεπωλήσεις";
    }
    else if (event.description.indexOf(m_grCategFood) != -1)
    {
        event.category = "Γαστρονομία";
    }
    else if (event.description.indexOf(m_grCategGameShow) != -1
             || event.title.indexOf(m_grCategGameShow) != -1)
    {
        event.category = "Τηλεπαιχνίδι";
    }
    else if (event.description.indexOf(m_grCategBiography) != -1)
    {
        event.category = "Βιογραφία";
    }
    else if (event.title.indexOf(m_grCategNews) != -1)
    {
        event.category = "Ειδήσεις";
    }
    else if (event.description.indexOf(m_grCategSports) != -1)
    {
        event.category = "Αθλητικά";
    }
    else if (event.description.indexOf(m_grCategMusic) != -1
            || event.title.indexOf(m_grCategMusic) != -1)
    {
        event.category = "Μουσική";
    }
    else if (event.description.indexOf(m_grCategDocumentary) != -1)
    {
        event.category = "Ντοκιμαντέρ";
    }
    else if (event.description.indexOf(m_grCategReligion) != -1)
    {
        event.category = "Θρησκεία";
    }
    else if (event.description.indexOf(m_grCategCulture) != -1)
    {
        event.category = "Τέχνες/Πολιτισμός";
    }
    else if (event.description.indexOf(m_grCategSpecial) != -1)
    {
        event.category = "Αφιέρωμα";
    }

}

void EITFixUp::FixUnitymedia(DBEventEIT &event) const
{
    // TODO handle scraping the category and category_type from localized text in the short/long description
    // TODO remove short description (stored as episode title) which is just the beginning of the long description (actual description)

    // drop the short description if its copy the start of the long description
    if (event.description.startsWith (event.subtitle))
    {
        event.subtitle = "";
    }

    // handle cast and crew in items in the DVB Extended Event Descriptor
    // remove handled items from the map, so the left overs can be reported
    QMap<QString,QString>::iterator i = event.items.begin();
    while (i != event.items.end())
    {
        if (QString::compare (i.key(), "Role Player") == 0)
        {
            event.AddPerson (DBPerson::kActor, i.value());
            i = event.items.erase (i);
        }
        else if (QString::compare (i.key(), "Director") == 0)
        {
            event.AddPerson (DBPerson::kDirector, i.value());
            i = event.items.erase (i);
        }
        else if (QString::compare (i.key(), "Commentary or Commentator") == 0)
        {
            event.AddPerson (DBPerson::kCommentator, i.value());
            i = event.items.erase (i);
        }
        else if (QString::compare (i.key(), "Performing Artist") == 0)
        {
            event.AddPerson (DBPerson::kActor, i.value());
            i = event.items.erase (i);
        }
        else if (QString::compare (i.key(), "Presenter") == 0)
        {
            event.AddPerson (DBPerson::kPresenter, i.value());
            i = event.items.erase (i);
        }
        else if (QString::compare (i.key(), "Producer") == 0)
        {
            event.AddPerson (DBPerson::kProducer, i.value());
            i = event.items.erase (i);
        }
        else if (QString::compare (i.key(), "Scriptwriter") == 0)
        {
            event.AddPerson (DBPerson::kWriter, i.value());
            i = event.items.erase (i);
        }
        else
        {
            ++i;
        }
    }

    // handle star rating in the description
    QRegExp tmp = m_unitymediaImdbrating;
    if (event.description.indexOf (tmp) != -1)
    {
        float stars = tmp.cap(1).toFloat();
        event.stars = stars / 10.0f;
        event.description.replace (m_unitymediaImdbrating, "");
    }
}
