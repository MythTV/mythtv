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
const QString seasonStr = "\\b(?:Season|Series|S)\\s*(\\d+)\\s*,?";

// Matches Episode 3, Ep 3/4, Ep 3 of 4 etc but not "step 1"
// cap1 = ep, cap2 = total
const QString longEp = "\\b(?:Ep|Episode)\\s*(\\d+)\\s*(?:(?:/|of)\\s*(\\d*))?";

// Matches S2 Ep 3/4, "Season 2, Ep 3 of 4", Episode 3 etc
// cap1 = season, cap2 = ep, cap3 = total
const QString longSeasEp = QString("\\(?(?:%1)?\\s*%2").arg(seasonStr, longEp);

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
      m_ukCEPQ("[:\\!\\.\\?]\\s"),
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
      m_ATVSubtitle(",{0,1}\\sFolge\\s(\\d{1,3})$"),
      m_DisneyChannelSubtitle(",([^,]+)\\s{0,1}(\\d{4})$"),
      m_RTLEpisodeNo1("^(Folge\\s\\d{1,4})\\.*\\s*"),
      m_RTLEpisodeNo2("^(\\d{1,2}\\/[IVX]+)\\.*\\s*"),
      m_fiRerun("\\ ?Uusinta[a-zA-Z\\ ]*\\.?"),
      m_fiRerun2("\\([Uu]\\)"),
      m_fiAgeLimit("\\(((1?[0-9]?)|[ST])\\)$"),
      m_fiFilm("^(Film|Elokuva): "),
      m_dePremiereLength("\\s?[0-9]+\\sMin\\."),
      m_dePremiereAirdate("\\s?([^\\s^\\.]+)\\s((?:1|2)[0-9]{3})\\."),
      m_dePremiereCredits("\\sVon\\s([^,]+)(?:,|\\su\\.\\sa\\.)\\smit\\s([^\\.]*)\\."),
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
      m_grActors("(?:[Ππ]α[ιί]ζουν:|[ΜMμ]ε τους:|Πρωταγωνιστο[υύ]ν:|Πρωταγωνιστε[ιί]:?)(?:\\s+στο ρόλο(?: του| της)?\\s(?:\\w+\\s[οη]\\s))?([-\\w\\s']+(?:,[-\\w\\s']+)*)(?:κ\\.[αά])?(?:\\W?)"),
      // cap(1) actors, just names
      m_grFixnofullstopActors("(\\w\\s(Παίζουν:|Πρωταγων))"),
      m_grFixnofullstopDirectors("((\\w\\s(Σκηνοθ[εέ]))"),
      m_grPeopleSeparator("([,-]\\s+)"),
      m_grDirector("(?:Σκηνοθεσία: |Σκηνοθέτης: |Σκηνοθέτης - Επιμέλεια: )(\\w+\\s\\w+\\s?)(?:\\W?)"),
      m_grPres("(?:Παρουσ[ιί]αση:(?:\\b)*|Παρουσι[αά]ζ(?:ουν|ει)(?::|\\sο|\\sη)|Παρουσι[αά]στ(?:[ηή]ς|ρια|ριες|[εέ]ς)(?::|\\sο|\\sη)|Με τ(?:ον |ην )(?:[\\s|:|ο|η])*(?:\\b)*)([-\\w\\s]+(?:,[-\\w\\s]+)*)(?:\\W?)"),
      m_grYear("(?:\\W?)(?:\\s?παραγωγ[ηή]ς|\\s?-|,)\\s*([1-2]{1}[0-9]{3})(?:-\\d{1,4})?",Qt::CaseInsensitive),
      m_grCountry("(?:\\W|\\b)(?:(ελλην|τουρκ|αμερικ[αά]ν|γαλλ|αγγλ|βρεττ?αν|γερμαν|ρωσσ?|ιταλ|ελβετ|σουηδ|ισπαν|πορτογαλ|μεξικ[αά]ν|κιν[εέ]ζικ|ιαπων|καναδ|βραζιλι[αά]ν)(ικ[ηή][ςσ]))",Qt::CaseInsensitive),
      m_grlongEp("\\b(?:Επ.|επεισ[οό]διο:?)\\s*(\\d+)(?:\\W?)",Qt::CaseInsensitive),
      m_grSeason_as_RomanNumerals(",\\s*([MDCLXVIΙΧ]+)$",Qt::CaseInsensitive),
      m_grSeason("(?:\\W-?)*(?:\\(-\\s*)?\\b(([Α-Ω]{1,2})(?:'|΄)?|(\\d{1,2})(?:ος|ου|oς|os)?)(?:\\s*κ[υύ]κλο(?:[σς]|υ)){1}\\s?",Qt::CaseInsensitive),
      m_grRealTitleinDescription("(?:^\\()([A-Za-z\\s\\d-]+)(?:\\))(?:\\s*)"),
      // cap1 = real title
      // cap0 = real title in parentheses.
      m_grRealTitleinTitle("(?:\\()([A-Za-z\\s\\d-]+)(?:\\))(?:\\s*$)*"),
      // cap1 = real title
      // cap0 = real title in parentheses.
      m_grCommentsinTitle("(?:\\()([Α-Ωα-ω\\s\\d-]+)(?:\\))(?:\\s*$)*"),
      // cap1 = real title
      // cap0 = real title in parentheses.
      m_grNotPreviouslyShown("(?:\\W?)(?:-\\s*)*(?:\\b[Α1]['΄η]?\\s*(?:τηλεοπτικ[ηή]\\s*)?(?:μετ[αά]δοση|προβολ[ηή]))(?:\\W?)",Qt::CaseInsensitive),
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
      m_unitymediaImdbrating("\\s*IMDb Rating: (\\d\\.\\d)\\s?/10$")
{
}

void EITFixUp::Fix(DBEventEIT &event) const
{
    if (event.m_fixup)
    {
        if (event.m_subtitle == event.m_title)
            event.m_subtitle = QString("");

        if (event.m_description.isEmpty() && !event.m_subtitle.isEmpty())
        {
            event.m_description = event.m_subtitle;
            event.m_subtitle = QString("");
        }
    }

    if (kFixHTML & event.m_fixup)
        FixStripHTML(event);

    if (kFixHDTV & event.m_fixup)
        event.m_videoProps |= VID_HDTV;

    if (kFixBell & event.m_fixup)
        FixBellExpressVu(event);

    if (kFixDish & event.m_fixup)
        FixBellExpressVu(event);

    if (kFixUK & event.m_fixup)
        FixUK(event);

    if (kFixPBS & event.m_fixup)
        FixPBS(event);

    if (kFixComHem & event.m_fixup)
        FixComHem(event, (kFixSubtitle & event.m_fixup) != 0U);

    if (kFixAUStar & event.m_fixup)
        FixAUStar(event);

    if (kFixAUDescription & event.m_fixup)
        FixAUDescription(event);

    if (kFixAUFreeview & event.m_fixup)
        FixAUFreeview(event);

    if (kFixAUNine & event.m_fixup)
        FixAUNine(event);

    if (kFixAUSeven & event.m_fixup)
        FixAUSeven(event);

    if (kFixMCA & event.m_fixup)
        FixMCA(event);

    if (kFixRTL & event.m_fixup)
        FixRTL(event);

    if (kFixP7S1 & event.m_fixup)
        FixPRO7(event);

    if (kFixATV & event.m_fixup)
        FixATV(event);

    if (kFixDisneyChannel & event.m_fixup)
        FixDisneyChannel(event);

    if (kFixFI & event.m_fixup)
        FixFI(event);

    if (kFixPremiere & event.m_fixup)
        FixPremiere(event);

    if (kFixNL & event.m_fixup)
        FixNL(event);

    if (kFixNO & event.m_fixup)
        FixNO(event);

    if (kFixNRK_DVBT & event.m_fixup)
        FixNRK_DVBT(event);

    if (kFixDK & event.m_fixup)
        FixDK(event);

    if (kFixCategory & event.m_fixup)
        FixCategory(event);

    if (kFixGreekSubtitle & event.m_fixup)
        FixGreekSubtitle(event);

    if (kFixGreekEIT & event.m_fixup)
        FixGreekEIT(event);

    if (kFixGreekCategories & event.m_fixup)
        FixGreekCategories(event);

    if (kFixUnitymedia & event.m_fixup)
        FixUnitymedia(event);

    if (event.m_fixup)
    {
        if (!event.m_title.isEmpty())
        {
            event.m_title = event.m_title.replace(QChar('\0'), "");
            event.m_title = event.m_title.trimmed();
        }

        if (!event.m_subtitle.isEmpty())
        {
            event.m_subtitle = event.m_subtitle.replace(QChar('\0'), "");
            event.m_subtitle = event.m_subtitle.trimmed();
        }

        if (!event.m_description.isEmpty())
        {
            event.m_description = event.m_description.replace(QChar('\0'), "");
            event.m_description = event.m_description.trimmed();
        }
    }

    if (kFixGenericDVB & event.m_fixup)
    {
        event.m_programId = AddDVBEITAuthority(event.m_chanid, event.m_programId);
        event.m_seriesId  = AddDVBEITAuthority(event.m_chanid, event.m_seriesId);
    }

    // Are any items left unhandled? report them to allow fixups improvements
    if (!event.m_items.empty())
    {
        QMap<QString,QString>::iterator i;
        for (i = event.m_items.begin(); i != event.m_items.end(); ++i)
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Unhandled item in EIT for"
                " channel id \"%1\", \"%2\": %3").arg(event.m_chanid)
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
 *  \param chanid The channel whose data should be updated.
 *  \param id The ID string to add the authority to.
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
 */
void EITFixUp::FixBellExpressVu(DBEventEIT &event) const
{
    // A 0x0D character is present between the content
    // and the subtitle if its present
    int position = event.m_description.indexOf(0x0D);

    if (position != -1)
    {
        // Subtitle present in the title, so get
        // it and adjust the description
        event.m_subtitle = event.m_description.left(position);
        event.m_description = event.m_description.right(
            event.m_description.length() - position - 2);
    }

    // Take out the content description which is
    // always next with a period after it
    position = event.m_description.indexOf(".");
    // Make sure they didn't leave it out and
    // you come up with an odd category
    if (position < 10)
    {
    }
    else
    {
        event.m_category = "Unknown";
    }

    // If the content descriptor didn't come up with anything, try parsing the category
    // out of the description.
    if (event.m_category.isEmpty())
    {
        // Take out the content description which is
        // always next with a period after it
        position = event.m_description.indexOf(".");
        if ((position + 1) < event.m_description.length())
            position = event.m_description.indexOf(". ");
        // Make sure they didn't leave it out and
        // you come up with an odd category
        if ((position > -1) && position < 20)
        {
            const QString stmp  = event.m_description;
            event.m_description = stmp.right(stmp.length() - position - 2);
            event.m_category    = stmp.left(position);

            int position_p = event.m_category.indexOf("(");
            if (position_p == -1)
                event.m_description = stmp.right(stmp.length() - position - 2);
            else
                event.m_category    = "Unknown";
        }
        else
        {
            event.m_category = "Unknown";
        }

        // When a channel is off air the category is "-"
        // so leave the category as blank
        if (event.m_category == "-")
            event.m_category = "OffAir";

        if (event.m_category.length() > 20)
            event.m_category = "Unknown";
    }
    else if (event.m_categoryType)
    {
        QString theme = dish_theme_type_to_string(event.m_categoryType);
        event.m_description = event.m_description.replace(theme, "");
        if (event.m_description.startsWith("."))
            event.m_description = event.m_description.right(event.m_description.length() - 1);
        if (event.m_description.startsWith(" "))
            event.m_description = event.m_description.right(event.m_description.length() - 1);
    }

    // See if a year is present as (xxxx)
    position = event.m_description.indexOf(m_bellYear);
    if (position != -1 && !event.m_category.isEmpty())
    {
        // Parse out the year
        bool ok;
        uint y = event.m_description.mid(position + 1, 4).toUInt(&ok);
        if (ok)
        {
            event.m_originalairdate = QDate(y, 1, 1);
            event.m_airdate = y;
            event.m_previouslyshown = true;
        }

        // Get the actors if they exist
        if (position > 3)
        {
            QString tmp = event.m_description.left(position-3);
            QStringList actors =
                tmp.split(m_bellActors, QString::SkipEmptyParts);
            QStringList::const_iterator it = actors.begin();
            for (; it != actors.end(); ++it)
                event.AddPerson(DBPerson::kActor, *it);
        }
        // Remove the year and actors from the description
        event.m_description = event.m_description.right(
            event.m_description.length() - position - 7);
    }

    // Check for (CC) in the decription and
    // set the <subtitles type="teletext"> flag
    position = event.m_description.indexOf("(CC)");
    if (position != -1)
    {
        event.m_subtitleType |= SUB_HARDHEAR;
        event.m_description = event.m_description.replace("(CC)", "");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.m_description.indexOf(m_Stereo);
    if (position != -1)
    {
        event.m_audioProps |= AUD_STEREO;
        event.m_description = event.m_description.replace(m_Stereo, "");
    }

    // Check for "title (All Day, HD)" in the title
    position = event.m_title.indexOf(m_bellPPVTitleAllDayHD);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_bellPPVTitleAllDayHD, "");
        event.m_videoProps |= VID_HDTV;
     }

    // Check for "title (All Day)" in the title
    position = event.m_title.indexOf(m_bellPPVTitleAllDay);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_bellPPVTitleAllDay, "");
    }

    // Check for "HD - title" in the title
    position = event.m_title.indexOf(m_bellPPVTitleHD);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_bellPPVTitleHD, "");
        event.m_videoProps |= VID_HDTV;
    }

    // Check for (HD) in the decription
    position = event.m_description.indexOf("(HD)");
    if (position != -1)
    {
        event.m_description = event.m_description.replace("(HD)", "");
        event.m_videoProps |= VID_HDTV;
    }

    // Check for (HD) in the title
    position = event.m_title.indexOf("(HD)");
    if (position != -1)
    {
        event.m_description = event.m_title.replace("(HD)", "");
        event.m_videoProps |= VID_HDTV;
    }

    // Check for HD at the end of the title
    position = event.m_title.indexOf(m_dishPPVTitleHD);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_dishPPVTitleHD, "");
        event.m_videoProps |= VID_HDTV;
    }

    // Check for (DD) at the end of the description
    position = event.m_description.indexOf("(DD)");
    if (position != -1)
    {
        event.m_description = event.m_description.replace("(DD)", "");
        event.m_audioProps |= AUD_DOLBY;
        event.m_audioProps |= AUD_STEREO;
    }

    // Remove SAP from Dish descriptions
    position = event.m_description.indexOf("(SAP)");
    if (position != -1)
    {
        event.m_description = event.m_description.replace("(SAP", "");
        event.m_subtitleType |= SUB_HARDHEAR;
    }

    // Remove any trailing colon in title
    position = event.m_title.indexOf(m_dishPPVTitleColon);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_dishPPVTitleColon, "");
    }

    // Remove New at the end of the description
    position = event.m_description.indexOf(m_dishDescriptionNew);
    if (position != -1)
    {
        event.m_previouslyshown = false;
        event.m_description = event.m_description.replace(m_dishDescriptionNew, "");
    }

    // Remove Series Finale at the end of the desciption
    position = event.m_description.indexOf(m_dishDescriptionFinale);
    if (position != -1)
    {
        event.m_previouslyshown = false;
        event.m_description = event.m_description.replace(m_dishDescriptionFinale, "");
    }

    // Remove Series Finale at the end of the desciption
    position = event.m_description.indexOf(m_dishDescriptionFinale2);
    if (position != -1)
    {
        event.m_previouslyshown = false;
        event.m_description = event.m_description.replace(m_dishDescriptionFinale2, "");
    }

    // Remove Series Premiere at the end of the description
    position = event.m_description.indexOf(m_dishDescriptionPremiere);
    if (position != -1)
    {
        event.m_previouslyshown = false;
        event.m_description = event.m_description.replace(m_dishDescriptionPremiere, "");
    }

    // Remove Series Premiere at the end of the description
    position = event.m_description.indexOf(m_dishDescriptionPremiere2);
    if (position != -1)
    {
        event.m_previouslyshown = false;
        event.m_description = event.m_description.replace(m_dishDescriptionPremiere2, "");
    }

    // Remove Dish's PPV code at the end of the description
    QRegExp ppvcode = m_dishPPVCode;
    ppvcode.setCaseSensitivity(Qt::CaseInsensitive);
    position = event.m_description.indexOf(ppvcode);
    if (position != -1)
    {
        event.m_description = event.m_description.replace(ppvcode, "");
    }

    // Remove trailing garbage
    position = event.m_description.indexOf(m_dishPPVSpacePerenEnd);
    if (position != -1)
    {
        event.m_description = event.m_description.replace(m_dishPPVSpacePerenEnd, "");
    }

    // Check for subtitle "All Day (... Eastern)" in the subtitle
    position = event.m_subtitle.indexOf(m_bellPPVSubtitleAllDay);
    if (position != -1)
    {
        event.m_subtitle = event.m_subtitle.replace(m_bellPPVSubtitleAllDay, "");
    }

    // Check for description "(... Eastern)" in the description
    position = event.m_description.indexOf(m_bellPPVDescriptionAllDay);
    if (position != -1)
    {
        event.m_description = event.m_description.replace(m_bellPPVDescriptionAllDay, "");
    }

    // Check for description "(... ET)" in the description
    position = event.m_description.indexOf(m_bellPPVDescriptionAllDay2);
    if (position != -1)
    {
        event.m_description = event.m_description.replace(m_bellPPVDescriptionAllDay2, "");
    }

    // Check for description "(nnnnn)" in the description
    position = event.m_description.indexOf(m_bellPPVDescriptionEventId);
    if (position != -1)
    {
        event.m_description = event.m_description.replace(m_bellPPVDescriptionEventId, "");
    }

}

/** \fn EITFixUp::SetUKSubtitle(DBEventEIT&) const
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::SetUKSubtitle(DBEventEIT &event) const
{
    QStringList strListColon = event.m_description.split(":");
    QStringList strListEnd;

    bool fColon = false, fQuotedSubtitle = false;
    int nPosition1;
    QString strEnd;
    if (strListColon.count()>1)
    {
         bool fDoubleDot = false;
         bool fSingleDot = true;
         int nLength = strListColon[0].length();

         nPosition1 = event.m_description.indexOf("..");
         if ((nPosition1 < nLength) && (nPosition1 >= 0))
             fDoubleDot = true;
         nPosition1 = event.m_description.indexOf(".");
         if (nPosition1==-1)
             fSingleDot = false;
         if (nPosition1 > nLength)
             fSingleDot = false;
         else
         {
             QString strTmp = event.m_description.mid(nPosition1+1,
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
             for (i =0; (i<strListColon.count()) && (nTitleMax==-1);i++)
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
             for (i=nTitleMax+1;i<strListColon.count();i++)
                 strListEnd.push_back(strListColon[i]);
             fColon = true;
         }
    }
    QRegExp tmpQuotedSubtitle = m_ukQuotedSubtitle;
    if (tmpQuotedSubtitle.indexIn(event.m_description) != -1)
    {
        event.m_subtitle = tmpQuotedSubtitle.cap(1);
        event.m_description.remove(m_ukQuotedSubtitle);
        fQuotedSubtitle = true;
    }
    QStringList strListPeriod;
    QStringList strListQuestion;
    QStringList strListExcl;
    if (!(fColon || fQuotedSubtitle))
    {
        strListPeriod = event.m_description.split(".");
        if (strListPeriod.count() >1)
        {
            nPosition1 = event.m_description.indexOf(".");
            int nPosition2 = event.m_description.indexOf("..");
            if ((nPosition1 < nPosition2) || (nPosition2==-1))
                strListEnd = strListPeriod;
        }

        strListQuestion = event.m_description.split("?");
        strListExcl = event.m_description.split("!");
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
            strEnd.clear();
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
             event.m_subtitle = strListEnd[0]+strEnd;
             event.m_subtitle.remove(m_ukSpaceColonStart);
             event.m_description=
                          event.m_description.mid(strListEnd[0].length()+1);
             event.m_description.remove(m_ukSpaceColonStart);
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

    bool isMovie = event.m_category.startsWith("Movie",Qt::CaseInsensitive) ||
                   event.m_category.startsWith("Film",Qt::CaseInsensitive);
    // BBC three case (could add another record here ?)
    event.m_description = event.m_description.remove(m_ukThen);
    event.m_description = event.m_description.remove(m_ukNew);
    event.m_title = event.m_title.remove(m_ukNewTitle);

    // Removal of Class TV, CBBC and CBeebies etc..
    event.m_title = event.m_title.remove(m_ukTitleRemove);
    event.m_description = event.m_description.remove(m_ukDescriptionRemove);

    // Removal of BBC FOUR and BBC THREE
    event.m_description = event.m_description.remove(m_ukBBC34);

    // BBC 7 [Rpt of ...] case.
    event.m_description = event.m_description.remove(m_ukBBC7rpt);

    // "All New To 4Music!
    event.m_description = event.m_description.remove(m_ukAllNew);

    // Removal of 'Also in HD' text
 	event.m_description = event.m_description.remove(m_ukAlsoInHD);

    // Remove [AD,S] etc.
    bool    ccMatched = false;
    QRegExp tmpCC = m_ukCC;
    position1 = 0;
    while ((position1 = tmpCC.indexIn(event.m_description, position1)) != -1)
    {
        ccMatched = true;
        position1 += tmpCC.matchedLength();

        QStringList tmpCCitems = tmpCC.cap(0).remove("[").remove("]").split(",");
        if (tmpCCitems.contains("AD"))
            event.m_audioProps |= AUD_VISUALIMPAIR;
        if (tmpCCitems.contains("HD"))
            event.m_videoProps |= VID_HDTV;
        if (tmpCCitems.contains("S"))
            event.m_subtitleType |= SUB_NORMAL;
        if (tmpCCitems.contains("SL"))
            event.m_subtitleType |= SUB_SIGNED;
        if (tmpCCitems.contains("W"))
            event.m_videoProps |= VID_WIDESCREEN;
    }

    if(ccMatched)
        event.m_description = event.m_description.remove(m_ukCC);

    event.m_title       = event.m_title.trimmed();
    event.m_description = event.m_description.trimmed();

    // Work out the season and episode numbers (if any)
    // Matching pattern "Season 2 Episode|Ep 3 of 14|3/14" etc
    bool    series  = false;
    QRegExp tmpSeries = m_ukSeries;
    if ((position1 = tmpSeries.indexIn(event.m_title)) != -1
            || (position2 = tmpSeries.indexIn(event.m_description)) != -1)
    {
        if (!tmpSeries.cap(1).isEmpty())
        {
            event.m_season = tmpSeries.cap(1).toUInt();
            series = true;
        }

        if (!tmpSeries.cap(2).isEmpty())
        {
            event.m_episode = tmpSeries.cap(2).toUInt();
            series = true;
        }
        else if (!tmpSeries.cap(5).isEmpty())
        {
            event.m_episode = tmpSeries.cap(5).toUInt();
            series = true;
        }

        if (!tmpSeries.cap(3).isEmpty())
        {
            event.m_totalepisodes = tmpSeries.cap(3).toUInt();
            series = true;
        }
        else if (!tmpSeries.cap(6).isEmpty())
        {
            event.m_totalepisodes = tmpSeries.cap(6).toUInt();
            series = true;
        }

        // Remove long or short match. Short text doesn't start at position2
        int form = tmpSeries.cap(4).isEmpty() ? 0 : 4;

        if (position1 != -1)
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Extracted S%1E%2/%3 from title (%4) \"%5\"")
                .arg(event.m_season).arg(event.m_episode).arg(event.m_totalepisodes)
                .arg(event.m_title, event.m_description));

            event.m_title.remove(tmpSeries.cap(form));
        }
        else
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Extracted S%1E%2/%3 from description (%4) \"%5\"")
                .arg(event.m_season).arg(event.m_episode).arg(event.m_totalepisodes)
                .arg(event.m_title, event.m_description));

            if (position2 == 0)
     		    // Remove from the start of the description.
		        // Otherwise it ends up in the subtitle.
                event.m_description.remove(tmpSeries.cap(form));
        }
    }

    if (isMovie)
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    else if (series)
        event.m_categoryType = ProgramInfo::kCategorySeries;

    // Multi-part episodes, or films (e.g. ITV film split by news)
    // Matches Part 1, Pt 1/2, Part 1 of 2 etc.
    QRegExp tmpPart = m_ukPart;
    if ((position1 = tmpPart.indexIn(event.m_title)) != -1)
    {
        event.m_partnumber = tmpPart.cap(1).toUInt();
        event.m_parttotal  = tmpPart.cap(2).toUInt();

        LOG(VB_EIT, LOG_DEBUG, QString("Extracted Part %1/%2 from title (%3)")
            .arg(event.m_partnumber).arg(event.m_parttotal).arg(event.m_title));

        // Remove from the title
        event.m_title = event.m_title.remove(tmpPart.cap(0));
    }
    else if ((position1 = tmpPart.indexIn(event.m_description)) != -1)
    {
        event.m_partnumber = tmpPart.cap(1).toUInt();
        event.m_parttotal  = tmpPart.cap(2).toUInt();

        LOG(VB_EIT, LOG_DEBUG, QString("Extracted Part %1/%2 from description (%3) \"%4\"")
            .arg(event.m_partnumber).arg(event.m_parttotal)
            .arg(event.m_title, event.m_description));

        // Remove from the start of the description.
        // Otherwise it ends up in the subtitle.
        if (position1 == 0)
        {
            // Retain a single colon (subtitle separator) if we remove any
            QString sub = tmpPart.cap(0).contains(":") ? ":" : "";
            event.m_description = event.m_description.replace(tmpPart.cap(0), sub);
        }
    }

    QRegExp tmpStarring = m_ukStarring;
    if (tmpStarring.indexIn(event.m_description) != -1)
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
                event.m_airdate = y;
                event.m_originalairdate = QDate(y, 1, 1);
            }
        }
    }

    QRegExp tmp24ep = m_uk24ep;
    if (!event.m_title.startsWith("CSI:") && !event.m_title.startsWith("CD:") &&
        !event.m_title.contains(m_ukLaONoSplit) &&
        !event.m_title.startsWith("Mission: Impossible"))
    {
        if (((position1=event.m_title.indexOf(m_ukDoubleDotEnd)) != -1) &&
            ((position2=event.m_description.indexOf(m_ukDoubleDotStart)) != -1))
        {
            QString strPart=event.m_title.remove(m_ukDoubleDotEnd)+" ";
            strFull = strPart + event.m_description.remove(m_ukDoubleDotStart);
            if (isMovie &&
                ((position1 = strFull.indexOf(m_ukCEPQ,strPart.length())) != -1))
            {
                 if (strFull[position1] == '!' || strFull[position1] == '?'
                  || (position1>2 && strFull[position1] == '.' && strFull[position1-2] == '.'))
                     position1++;
                 event.m_title = strFull.left(position1);
                 event.m_description = strFull.mid(position1 + 1);
                 event.m_description.remove(m_ukSpaceStart);
            }
            else if ((position1 = strFull.indexOf(m_ukCEPQ)) != -1)
            {
                 if (strFull[position1] == '!' || strFull[position1] == '?'
                  || (position1>2 && strFull[position1] == '.' && strFull[position1-2] == '.'))
                     position1++;
                 event.m_title = strFull.left(position1);
                 event.m_description = strFull.mid(position1 + 1);
                 event.m_description.remove(m_ukSpaceStart);
                 SetUKSubtitle(event);
            }
            if ((position1 = strFull.indexOf(m_ukYear)) != -1)
            {
                // Looks like they are using the airdate as a delimiter
                if ((uint)position1 < SUBTITLE_MAX_LEN)
                {
                    event.m_description = event.m_title.mid(position1);
                    event.m_title = event.m_title.left(position1);
                }
            }
        }
        else if ((position1 = tmp24ep.indexIn(event.m_description)) != -1)
        {
            // Special case for episodes of 24.
            // -2 from the length cause we don't want ": " on the end
            event.m_subtitle = event.m_description.mid(position1,
                                tmp24ep.cap(0).length() - 2);
            event.m_description = event.m_description.remove(tmp24ep.cap(0));
        }
        else if ((position1 = event.m_description.indexOf(m_ukTime)) == -1)
        {
            if (!isMovie && (event.m_title.indexOf(m_ukYearColon) < 0))
            {
                if (((position1 = event.m_title.indexOf(":")) != -1) &&
                    (event.m_description.indexOf(":") < 0 ))
                {
                    if (event.m_title.mid(position1+1).indexOf(m_ukCompleteDots)==0)
                    {
                        SetUKSubtitle(event);
                        QString strTmp = event.m_title.mid(position1+1);
                        event.m_title.resize(position1);
                        event.m_subtitle = strTmp+event.m_subtitle;
                    }
                    else if ((uint)position1 < SUBTITLE_MAX_LEN)
                    {
                        event.m_subtitle = event.m_title.mid(position1 + 1);
                        event.m_title = event.m_title.left(position1);
                    }
                }
                else
                    SetUKSubtitle(event);
            }
        }
    }

    if (!isMovie && event.m_subtitle.isEmpty() &&
        !event.m_title.startsWith("The X-Files"))
    {
        if ((position1=event.m_description.indexOf(m_ukTime)) != -1)
        {
            position2 = event.m_description.indexOf(m_ukColonPeriod);
            if ((position2>=0) && (position2 < (position1-2)))
                SetUKSubtitle(event);
        }
        else if ((position1=event.m_title.indexOf("-")) != -1)
        {
            if ((uint)position1 < SUBTITLE_MAX_LEN)
            {
                event.m_subtitle = event.m_title.mid(position1 + 1);
                event.m_subtitle.remove(m_ukSpaceColonStart);
                event.m_title = event.m_title.left(position1);
            }
        }
        else
            SetUKSubtitle(event);
    }

    // Work out the year (if any)
    QRegExp tmpUKYear = m_ukYear;
    if ((position1 = tmpUKYear.indexIn(event.m_description)) != -1)
    {
        QString stmp = event.m_description;
        int     itmp = position1 + tmpUKYear.cap(0).length();
        event.m_description = stmp.left(position1) + stmp.mid(itmp);
        bool ok;
        uint y = tmpUKYear.cap(1).toUInt(&ok);
        if (ok)
        {
            event.m_airdate = y;
            event.m_originalairdate = QDate(y, 1, 1);
        }
    }

    // Trim leading/trailing '.'
    event.m_subtitle.remove(m_ukDotSpaceStart);
    if (event.m_subtitle.lastIndexOf("..") != (event.m_subtitle.length()-2))
        event.m_subtitle.remove(m_ukDotEnd);

    // Reverse the subtitle and empty description
    if (event.m_description.isEmpty() && !event.m_subtitle.isEmpty())
    {
        event.m_description=event.m_subtitle;
        event.m_subtitle.clear();
    }
}

/** \fn EITFixUp::FixPBS(DBEventEIT&) const
 *  \brief Use this to standardize PBS ATSC guide in the USA.
 */
void EITFixUp::FixPBS(DBEventEIT &event) const
{
    /* Used for PBS ATSC Subtitles are separated by a colon */
    int position = event.m_description.indexOf(':');
    if (position != -1)
    {
        const QString stmp   = event.m_description;
        event.m_subtitle     = stmp.left(position);
        event.m_description  = stmp.right(stmp.length() - position - 2);
    }
}

/**
 *  \brief Use this to standardize ComHem DVB-C service in Sweden.
 */
void EITFixUp::FixComHem(DBEventEIT &event, bool process_subtitle) const
{
    // Reverse what EITFixUp::Fix() did
    if (event.m_subtitle.isEmpty() && !event.m_description.isEmpty())
    {
        event.m_subtitle = event.m_description;
        event.m_description = "";
    }

    // Remove subtitle, it contains the category and we already know that
    event.m_subtitle = "";

    bool isSeries = false;
    // Try to find episode numbers
    int pos;
    QRegExp tmpSeries1 = m_comHemSeries1;
    QRegExp tmpSeries2 = m_comHemSeries2;
    if ((pos = tmpSeries2.indexIn(event.m_title)) != -1)
    {
        QStringList list = tmpSeries2.capturedTexts();
        event.m_partnumber = list[2].toUInt();
        event.m_title = event.m_title.replace(list[0],"");
    }
    else if ((pos = tmpSeries1.indexIn(event.m_description)) != -1)
    {
        QStringList list = tmpSeries1.capturedTexts();
        if (!list[1].isEmpty())
        {
            event.m_partnumber = list[1].toUInt();
        }
        if (!list[2].isEmpty())
        {
            event.m_parttotal = list[2].toUInt();
        }

        // Remove the episode numbers, but only if it's not at the begining
        // of the description (subtitle code might use it)
        if(pos > 0)
            event.m_description = event.m_description.replace(list[0],"");
        isSeries = true;
    }

    // Add partnumber/parttotal to subtitle
    // This will be overwritten if we find a better subtitle
    if (event.m_partnumber > 0)
    {
        event.m_subtitle = QString("Del %1").arg(event.m_partnumber);
        if (event.m_parttotal > 0)
        {
            event.m_subtitle += QString(" av %1").arg(event.m_parttotal);
        }
    }

    // Move subtitle info from title to subtitle
    QRegExp tmpTSub = m_comHemTSub;
    if (tmpTSub.indexIn(event.m_title) != -1)
    {
        event.m_subtitle = tmpTSub.cap(1);
        event.m_title = event.m_title.replace(tmpTSub.cap(0),"");
    }

    // No need to continue without a description.
    if (event.m_description.length() <= 0)
        return;

    // Try to find country category, year and possibly other information
    // from the begining of the description
    QRegExp tmpCountry = m_comHemCountry;
    pos = tmpCountry.indexIn(event.m_description);
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
            if(event.m_category.isEmpty())
            {
                event.m_category = list[3];
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
                event.m_airdate = y;
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
        event.m_description = event.m_description.replace(list[0],replacement);
    }

    if (isSeries)
        event.m_categoryType = ProgramInfo::kCategorySeries;

    // Look for additional persons in the description
    QRegExp tmpPersons = m_comHemPersons;
    while(pos = tmpPersons.indexIn(event.m_description),pos!=-1)
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
            event.m_description=event.m_description.replace(list[0],"");
            continue;
        }

        const QStringList actors =
            list[2].split(m_comHemPersSeparator, QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
            event.AddPerson(role, *it);

        // Remove it
        event.m_description=event.m_description.replace(list[0],"");
    }

    // Is this event on a channel we shoud look for a subtitle?
    // The subtitle is the first sentence in the description, but the
    // subtitle can't be the only thing in the description and it must be
    // shorter than 55 characters or we risk picking up the wrong thing.
    if (process_subtitle)
    {
        int pos2 = event.m_description.indexOf(m_comHemSub);
        bool pvalid = pos2 != -1 && pos2 <= 55;
        if (pvalid && (event.m_description.length() - (pos2 + 2)) > 0)
        {
            event.m_subtitle = event.m_description.left(
                pos2 + (event.m_description[pos2] == '?' ? 1 : 0));
            event.m_description = event.m_description.mid(pos2 + 2);
        }
    }

    // Teletext subtitles?
    int position = event.m_description.indexOf(m_comHemTT);
    if (position != -1)
    {
        event.m_subtitleType |= SUB_NORMAL;
    }

    // Try to findout if this is a rerun and if so the date.
    QRegExp tmpRerun1 = m_comHemRerun1;
    if (tmpRerun1.indexIn(event.m_description) == -1)
        return;

    // Rerun from today
    QStringList list = tmpRerun1.capturedTexts();
    if (list[1] == "i dag")
    {
        event.m_originalairdate = event.m_starttime.date();
        return;
    }

    // Rerun from yesterday afternoon
    if (list[1] == "eftermiddagen")
    {
        event.m_originalairdate = event.m_starttime.date().addDays(-1);
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
        //    year = event.m_starttime.date().year();

        if (day > 0 && month > 0)
        {
            QDate date(event.m_starttime.date().year(), month, day);
            // it's a rerun so it must be in the past
            if (date > event.m_starttime.date())
                date = date.addYears(-1);
            event.m_originalairdate = date;
        }
        return;
    }
}

/** \fn EITFixUp::FixAUStar(DBEventEIT&) const
 *  \brief Use this to standardize DVB-S guide in Australia.
 */
void EITFixUp::FixAUStar(DBEventEIT &event) const
{
    event.m_category = event.m_subtitle;
    /* Used for DVB-S Subtitles are separated by a colon */
    int position = event.m_description.indexOf(':');
    if (position != -1)
    {
        const QString stmp  = event.m_description;
        event.m_subtitle    = stmp.left(position);
        event.m_description = stmp.right(stmp.length() - position - 2);
    }
}

/** \fn EITFixUp::FixAUDescription(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (fix common annoyances common to most networks)
 */
void EITFixUp::FixAUDescription(DBEventEIT &event) const
{
    if (event.m_description.startsWith("[Program data ") || event.m_description.startsWith("[Program info "))//TEN
    {
        event.m_description = "";//event.m_subtitle;
    }
    if (event.m_description.endsWith("Copyright West TV Ltd. 2011)"))
        event.m_description.resize(event.m_description.length()-40);

    if (event.m_description.isEmpty() && !event.m_subtitle.isEmpty())//due to ten's copyright info, this won't be caught before
    {
        event.m_description = event.m_subtitle;
        event.m_subtitle.clear();
    }
    if (event.m_description.startsWith(event.m_title+" - "))
        event.m_description.remove(0,event.m_title.length()+3);
    if (event.m_title.startsWith("LIVE: ", Qt::CaseInsensitive))
    {
        event.m_title.remove(0, 6);
        event.m_description.prepend("(Live) ");
    }
}
/** \fn EITFixUp::FixAUNine(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (Nine network)
 */
void EITFixUp::FixAUNine(DBEventEIT &event) const
{
    QRegExp rating("\\((G|PG|M|MA)\\)");
    if (rating.indexIn(event.m_description) == 0)
    {
      EventRating prograting;
      prograting.m_system="AU"; prograting.m_rating = rating.cap(1);
      event.m_ratings.push_back(prograting);
      event.m_description.remove(0,rating.matchedLength()+1);
    }
    if (event.m_description.startsWith("[HD]"))
    {
      event.m_videoProps |= VID_HDTV;
      event.m_description.remove(0,5);
    }
    if (event.m_description.startsWith("[CC]"))
    {
      event.m_subtitleType |= SUB_NORMAL;
      event.m_description.remove(0,5);
    }
    if (event.m_subtitle == "Movie")
    {
        event.m_subtitle.clear();
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    }
    if (event.m_description.startsWith(event.m_title))
      event.m_description.remove(0,event.m_title.length()+1);
}
/** \fn EITFixUp::FixAUSeven(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (Seven network)
 */
void EITFixUp::FixAUSeven(DBEventEIT &event) const
{
    if (event.m_description.endsWith(" Rpt"))
    {
        event.m_previouslyshown = true;
        event.m_description.resize(event.m_description.size()-4);
    }
    QRegExp year("(\\d{4})$");
    if (year.indexIn(event.m_description) != -1)
    {
        event.m_airdate = year.cap(3).toUInt();
        event.m_description.resize(event.m_description.size()-5);
    }
    if (event.m_description.endsWith(" CC"))
    {
      event.m_subtitleType |= SUB_NORMAL;
      event.m_description.resize(event.m_description.size()-3);
    }
    QString advisories;//store the advisories to append later
    QRegExp adv("(\\([A-Z,]+\\))$");
    if (adv.indexIn(event.m_description) != -1)
    {
        advisories = adv.cap(1);
        event.m_description.resize(event.m_description.size()-(adv.matchedLength()+1));
    }
    QRegExp rating("(C|G|PG|M|MA)$");
    if (rating.indexIn(event.m_description) != -1)
    {
        EventRating prograting;
        prograting.m_system=""; prograting.m_rating = rating.cap(1);
        if (!advisories.isEmpty())
            prograting.m_rating.append(" ").append(advisories);
        event.m_ratings.push_back(prograting);
        event.m_description.resize(event.m_description.size()-(rating.matchedLength()+1));
    }
}
/** \fn EITFixUp::FixAUFreeview(DBEventEIT&) const
 *  \brief Use this to standardize DVB-T guide in Australia. (generic freeview - extra info in brackets at end of desc)
 */
void EITFixUp::FixAUFreeview(DBEventEIT &event) const
{
    if (event.m_description.endsWith(".."))//has been truncated to fit within the 'subtitle' eit field, so none of the following will work (ABC)
        return;

    if (m_AUFreeviewSY.indexIn(event.m_description.trimmed(), 0) != -1)
    {
        if (event.m_subtitle.isEmpty())//nine sometimes has an actual subtitle field and the brackets thingo)
            event.m_subtitle = m_AUFreeviewSY.cap(2);
        event.m_airdate = m_AUFreeviewSY.cap(3).toUInt();
        event.m_description = m_AUFreeviewSY.cap(1);
    }
    else if (m_AUFreeviewY.indexIn(event.m_description.trimmed(), 0) != -1)
    {
        event.m_airdate = m_AUFreeviewY.cap(2).toUInt();
        event.m_description = m_AUFreeviewY.cap(1);
    }
    else if (m_AUFreeviewSYC.indexIn(event.m_description.trimmed(), 0) != -1)
    {
        if (event.m_subtitle.isEmpty())
            event.m_subtitle = m_AUFreeviewSYC.cap(2);
        event.m_airdate = m_AUFreeviewSYC.cap(3).toUInt();
        QStringList actors = m_AUFreeviewSYC.cap(4).split("/");
        for (int i = 0; i < actors.size(); ++i)
            event.AddPerson(DBPerson::kActor, actors.at(i));
        event.m_description = m_AUFreeviewSYC.cap(1);
    }
    else if (m_AUFreeviewYC.indexIn(event.m_description.trimmed(), 0) != -1)
    {
        event.m_airdate = m_AUFreeviewYC.cap(2).toUInt();
        QStringList actors = m_AUFreeviewYC.cap(3).split("/");
        for (int i = 0; i < actors.size(); ++i)
            event.AddPerson(DBPerson::kActor, actors.at(i));
        event.m_description = m_AUFreeviewYC.cap(1);
    }
}

/** \fn EITFixUp::FixMCA(DBEventEIT&) const
 *  \brief Use this to standardise the MultiChoice Africa DVB-S guide.
 */
void EITFixUp::FixMCA(DBEventEIT &event) const
{
    const uint SUBTITLE_PCT     = 60; // % of description to allow subtitle to
    const uint lSUBTITLE_MAX_LEN = 128;// max length of subtitle field in db.
    int        position;
    QRegExp    tmpExp1;

    // Remove subtitle, it contains category information too specific to use
    event.m_subtitle = QString("");

    // No need to continue without a description.
    if (event.m_description.length() <= 0)
        return;

    // Replace incomplete title if the full one is in the description
    tmpExp1 = m_mcaIncompleteTitle;
    if (tmpExp1.indexIn(event.m_title) != -1)
    {
        tmpExp1 = QRegExp( m_mcaCompleteTitlea.pattern() + tmpExp1.cap(1) +
                                   m_mcaCompleteTitleb.pattern());
        tmpExp1.setCaseSensitivity(Qt::CaseInsensitive);
        if (tmpExp1.indexIn(event.m_description) != -1)
        {
            event.m_title       = tmpExp1.cap(1).trimmed();
            event.m_description = tmpExp1.cap(2).trimmed();
        }
        tmpExp1.setCaseSensitivity(Qt::CaseSensitive);
    }

    // Try to find subtitle in description
    tmpExp1 = m_mcaSubtitle;
    if ((position = tmpExp1.indexIn(event.m_description)) != -1)
    {
        uint tmpExp1Len = tmpExp1.cap(1).length();
        uint evDescLen = max(event.m_description.length(), 1);

        if ((tmpExp1Len < lSUBTITLE_MAX_LEN) &&
            ((tmpExp1Len * 100 / evDescLen) < SUBTITLE_PCT))
        {
            event.m_subtitle    = tmpExp1.cap(1);
            event.m_description = tmpExp1.cap(2);
        }
    }

    // Try to find episode numbers in subtitle
    tmpExp1 = m_mcaSeries;
    if ((position = tmpExp1.indexIn(event.m_subtitle)) != -1)
    {
        uint season    = tmpExp1.cap(1).toUInt();
        uint episode   = tmpExp1.cap(2).toUInt();
        event.m_subtitle = tmpExp1.cap(3).trimmed();
        event.m_syndicatedepisodenumber =
                QString("S%1E%2").arg(season).arg(episode);
        event.m_season = season;
        event.m_episode = episode;
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }

    // Close captioned?
    position = event.m_description.indexOf(m_mcaCC);
    if (position > 0)
    {
        event.m_subtitleType |= SUB_HARDHEAR;
        event.m_description.replace(m_mcaCC, "");
    }

    // Dolby Digital 5.1?
    position = event.m_description.indexOf(m_mcaDD);
    if ((position > 0) && (position > event.m_description.length() - 7))
    {
        event.m_audioProps |= AUD_DOLBY;
        event.m_description.replace(m_mcaDD, "");
    }

    // Remove bouquet tags
    event.m_description.replace(m_mcaAvail, "");

    // Try to find year and director from the end of the description
    bool isMovie = false;
    tmpExp1  = m_mcaCredits;
    position = tmpExp1.indexIn(event.m_description);
    if (position != -1)
    {
        isMovie = true;
        event.m_description = tmpExp1.cap(1).trimmed();
        bool ok;
        uint y = tmpExp1.cap(2).trimmed().toUInt(&ok);
        if (ok)
            event.m_airdate = y;
        event.AddPerson(DBPerson::kDirector, tmpExp1.cap(3).trimmed());
    }
    else
    {
        // Try to find year only from the end of the description
        tmpExp1  = m_mcaYear;
        position = tmpExp1.indexIn(event.m_description);
        if (position != -1)
        {
            isMovie = true;
            event.m_description = tmpExp1.cap(1).trimmed();
            bool ok;
            uint y = tmpExp1.cap(2).trimmed().toUInt(&ok);
            if (ok)
                event.m_airdate = y;
        }
    }

    if (isMovie)
    {
        tmpExp1  = m_mcaActors;
        position = tmpExp1.indexIn(event.m_description);
        if (position != -1)
        {
            const QStringList actors = tmpExp1.cap(2).split(
                m_mcaActorsSeparator, QString::SkipEmptyParts);
            QStringList::const_iterator it = actors.begin();
            for (; it != actors.end(); ++it)
                event.AddPerson(DBPerson::kActor, (*it).trimmed());
            event.m_description = tmpExp1.cap(1).trimmed();
        }
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    }

}

/** \fn EITFixUp::FixRTL(DBEventEIT&) const
 *  \brief Use this to standardise the RTL group guide in Germany.
 */
void EITFixUp::FixRTL(DBEventEIT &event) const
{
    int        pos;

    // No need to continue without a description or with an subtitle.
    if (event.m_description.length() <= 0 || event.m_subtitle.length() > 0)
        return;

    // Repeat
    QRegExp tmpExpRepeat = m_RTLrepeat;
    if ((pos = tmpExpRepeat.indexIn(event.m_description)) != -1)
    {
        // remove '.' if it matches at the beginning of the description
        int length = tmpExpRepeat.cap(0).length() + (pos ? 0 : 1);
        event.m_description = event.m_description.remove(pos, length).trimmed();
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
    if (tmpExpSubtitle1.indexIn(event.m_description) != -1)
    {
        event.m_syndicatedepisodenumber = tmpExpSubtitle1.cap(1);
        event.m_subtitle    = tmpExpSubtitle1.cap(2);
        event.m_description =
            event.m_description.remove(0, tmpExpSubtitle1.matchedLength());
    }
    // episode number subtitle
    else if (tmpExpSubtitle2.indexIn(event.m_description) != -1)
    {
        event.m_syndicatedepisodenumber = tmpExpSubtitle2.cap(1);
        event.m_subtitle    = tmpExpSubtitle2.cap(2);
        event.m_description =
            event.m_description.remove(0, tmpExpSubtitle2.matchedLength());
    }
    // episode number subtitle
    else if (tmpExpSubtitle3.indexIn(event.m_description) != -1)
    {
        event.m_syndicatedepisodenumber = tmpExpSubtitle3.cap(1);
        event.m_subtitle    = tmpExpSubtitle3.cap(2);
        event.m_description =
            event.m_description.remove(0, tmpExpSubtitle3.matchedLength());
    }
    // "Thema..."
    else if (tmpExpSubtitle4.indexIn(event.m_description) != -1)
    {
        event.m_subtitle    = tmpExpSubtitle4.cap(1);
        event.m_description =
            event.m_description.remove(0, tmpExpSubtitle4.matchedLength());
    }
    // "'...'"
    else if (tmpExpSubtitle5.indexIn(event.m_description) != -1)
    {
        event.m_subtitle    = tmpExpSubtitle5.cap(1);
        event.m_description =
            event.m_description.remove(0, tmpExpSubtitle5.matchedLength());
    }
    // episode number
    else if (tmpExpEpisodeNo1.indexIn(event.m_description) != -1)
    {
        event.m_syndicatedepisodenumber = tmpExpEpisodeNo1.cap(2);
        event.m_subtitle    = tmpExpEpisodeNo1.cap(1);
        event.m_description =
            event.m_description.remove(0, tmpExpEpisodeNo1.matchedLength());
    }
    // episode number
    else if (tmpExpEpisodeNo2.indexIn(event.m_description) != -1)
    {
        event.m_syndicatedepisodenumber = tmpExpEpisodeNo2.cap(2);
        event.m_subtitle    = tmpExpEpisodeNo2.cap(1);
        event.m_description =
            event.m_description.remove(0, tmpExpEpisodeNo2.matchedLength());
    }

    /* got an episode title now? (we did not have one at the start of this function) */
    if (!event.m_subtitle.isEmpty())
    {
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }

    /* if we do not have an episode title by now try some guessing as last resort */
    if (event.m_subtitle.length() == 0)
    {
        const uint SUBTITLE_PCT = 35; // % of description to allow subtitle up to
        const uint lSUBTITLE_MAX_LEN = 50; // max length of subtitle field in db

        if (tmpExp1.indexIn(event.m_description) != -1)
        {
            uint tmpExp1Len = tmpExp1.cap(1).length();
            uint evDescLen = max(event.m_description.length(), 1);

            if ((tmpExp1Len < lSUBTITLE_MAX_LEN) &&
                (tmpExp1Len * 100 / evDescLen < SUBTITLE_PCT))
            {
                event.m_subtitle    = tmpExp1.cap(1);
                event.m_description = tmpExp1.cap(2);
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

    int pos = tmp.indexIn(event.m_subtitle);
    if (pos != -1)
    {
        if (event.m_airdate == 0)
        {
            event.m_airdate = tmp.cap(3).toUInt();
        }
        event.m_subtitle.replace(tmp, "");
    }

    /* handle cast, the very last in description */
    tmp = m_PRO7Cast;
    pos = tmp.indexIn(event.m_description);
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
        event.m_description.replace(tmp, "");
    }

    /* handle crew, the new very last in description
     * format: "Role: Name" or "Role: Name1, Name2"
     */
    tmp = m_PRO7Crew;
    pos = tmp.indexIn(event.m_description);
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
                else if ((QString::compare (tmpOne.cap(1), "Drehbuch") == 0) ||
                         (QString::compare (tmpOne.cap(1), "Autor") == 0))
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
        event.m_description.replace(tmp, "");
    }

    /* FIXME unless its Jamie Oliver, then there is neither Crew nor Cast only
     * \n\nKoch: Jamie Oliver
     */
}

/** \fn EITFixUp::FixDisneyChannel(DBEventEIT&) const
*  \brief Use this to standardise the Disney Channel guide in Germany.
*/
void EITFixUp::FixDisneyChannel(DBEventEIT &event) const
{
    QRegExp tmp = m_DisneyChannelSubtitle;
    int pos = tmp.indexIn(event.m_subtitle);
    if (pos != -1)
    {
        if (event.m_airdate == 0)
        {
            event.m_airdate = tmp.cap(3).toUInt();
        }
	event.m_subtitle.replace(tmp, "");
    }
    tmp = QRegExp("\\s[^\\s]+-(Serie)");
    pos = tmp.indexIn(event.m_subtitle);
    if (pos != -1)
    {
        event.m_categoryType = ProgramInfo::kCategorySeries;
        event.m_category=tmp.cap(0).trimmed();
        event.m_subtitle.replace(tmp, "");
    }
}

/** \fn EITFixUp::FixATV(DBEventEIT&) const
*  \brief Use this to standardise the ATV/ATV2 guide in Germany.
**/
void EITFixUp::FixATV(DBEventEIT &event) const
{
    event.m_subtitle.replace(m_ATVSubtitle, "");
}


/** \fn EITFixUp::FixFI(DBEventEIT&) const
 *  \brief Use this to clean DVB-T guide in Finland.
 */
void EITFixUp::FixFI(DBEventEIT &event) const
{
    int position = event.m_description.indexOf(m_fiRerun);
    if (position != -1)
    {
        event.m_previouslyshown = true;
        event.m_description = event.m_description.replace(m_fiRerun, "");
    }

    position = event.m_description.indexOf(m_fiRerun2);
    if (position != -1)
    {
        event.m_previouslyshown = true;
        event.m_description = event.m_description.replace(m_fiRerun2, "");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.m_description.indexOf(m_Stereo);
    if (position != -1)
    {
        event.m_audioProps |= AUD_STEREO;
        event.m_description = event.m_description.replace(m_Stereo, "");
    }

    // Remove age limit in parenthesis at end of title
    position = event.m_title.indexOf(m_fiAgeLimit);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_fiAgeLimit, "");
    }

    // Remove Film or Elokuva at start of title
    position = event.m_title.indexOf(m_fiFilm);
    if (position != -1)
    {
        event.m_title = event.m_title.replace(m_fiFilm, "");
    }

}

/** \fn EITFixUp::FixPremiere(DBEventEIT&) const
 *  \brief Use this to standardize DVB-C guide in Germany
 *         for the providers Kabel Deutschland and Premiere.
 */
void EITFixUp::FixPremiere(DBEventEIT &event) const
{
    QString country = "";

    QRegExp tmplength =  m_dePremiereLength;
    QRegExp tmpairdate =  m_dePremiereAirdate;
    QRegExp tmpcredits =  m_dePremiereCredits;

    event.m_description = event.m_description.replace(tmplength, "");

    if (tmpairdate.indexIn(event.m_description) != -1)
    {
        country = tmpairdate.cap(1).trimmed();
        bool ok;
        uint y = tmpairdate.cap(2).toUInt(&ok);
        if (ok)
            event.m_airdate = y;
        event.m_description = event.m_description.replace(tmpairdate, "");
    }

    if (tmpcredits.indexIn(event.m_description) != -1)
    {
        event.AddPerson(DBPerson::kDirector, tmpcredits.cap(1));
        const QStringList actors = tmpcredits.cap(2).split(
            ", ", QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
            event.AddPerson(DBPerson::kActor, *it);
        event.m_description = event.m_description.replace(tmpcredits, "");
    }

    event.m_description = event.m_description.replace("\u000A$", "");
    event.m_description = event.m_description.replace("\u000A", " ");

    // move the original titel from the title to subtitle
    QRegExp tmpOTitle = m_dePremiereOTitle;
    if (tmpOTitle.indexIn(event.m_title) != -1)
    {
        event.m_subtitle = QString("%1, %2").arg(tmpOTitle.cap(1)).arg(country);
        event.m_title = event.m_title.replace(tmpOTitle, "");
    }

    // Find infos about season and episode number
    QRegExp tmpSeasonEpisode =  m_deSkyDescriptionSeasonEpisode;
    if (tmpSeasonEpisode.indexIn(event.m_description) != -1)
    {
        event.m_season = tmpSeasonEpisode.cap(1).trimmed().toUInt();
        event.m_episode = tmpSeasonEpisode.cap(2).trimmed().toUInt();
        event.m_description.replace(tmpSeasonEpisode, "");
    }
}

/** \fn EITFixUp::FixNL(DBEventEIT&) const
 *  \brief Use this to standardize \@Home DVB-C guide in the Netherlands.
 */
void EITFixUp::FixNL(DBEventEIT &event) const
{
    QString fullinfo = "";
    fullinfo.append (event.m_subtitle);
    fullinfo.append (event.m_description);
    event.m_subtitle = "";

    // Convert categories to Dutch categories Myth knows.
    // nog invoegen: comedy, sport, misdaad

    if (event.m_category == "Documentary")
    {
        event.m_category = "Documentaire";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "News")
    {
        event.m_category = "Nieuws/actualiteiten";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Kids")
    {
        event.m_category = "Jeugd";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Show/game Show")
    {
        event.m_category = "Amusement";
        event.m_categoryType = ProgramInfo::kCategoryTVShow;
    }
    if (event.m_category == "Music/Ballet/Dance")
    {
        event.m_category = "Muziek";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "News magazine")
    {
        event.m_category = "Informatief";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Movie")
    {
        event.m_category = "Film";
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    }
    if (event.m_category == "Nature/animals/Environment")
    {
        event.m_category = "Natuur";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Movie - Adult")
    {
        event.m_category = "Erotiek";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Movie - Soap/melodrama/folkloric")
    {
        event.m_category = "Serie/soap";
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }
    if (event.m_category == "Arts/Culture")
    {
        event.m_category = "Kunst/Cultuur";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Sports")
    {
        event.m_category = "Sport";
        event.m_categoryType = ProgramInfo::kCategorySports;
    }
    if (event.m_category == "Cartoons/Puppets")
    {
        event.m_category = "Animatie";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Movie - Comedy")
    {
        event.m_category = "Comedy";
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }
    if (event.m_category == "Movie - Detective/Thriller")
    {
        event.m_category = "Misdaad";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }
    if (event.m_category == "Social/Spiritual Sciences")
    {
        event.m_category = "Religieus";
        event.m_categoryType = ProgramInfo::kCategoryNone;
    }

    // Film - categories are usually not Films
    if (event.m_category.startsWith("Film -"))
    {
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }

    // Get stereo info
    if (fullinfo.indexOf(m_Stereo) != -1)
    {
        event.m_audioProps |= AUD_STEREO;
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
        event.m_subtitleType |= SUB_NORMAL;
        fullinfo = fullinfo.replace("txt", ".");
    }

    // Get HDTV information
    if (event.m_title.indexOf(m_nlHD) != -1)
    {
        event.m_videoProps |= VID_HDTV;
        event.m_title = event.m_title.replace(m_nlHD, "");
    }

    // Try to make subtitle from Afl.:
    QRegExp tmpSub = m_nlSub;
    QString tmpSubString;
    if (tmpSub.indexIn(fullinfo) != -1)
    {
        tmpSubString = tmpSub.cap(0);
        tmpSubString = tmpSubString.right(tmpSubString.length() - 7);
        event.m_subtitle = tmpSubString.left(tmpSubString.length() -1);
        fullinfo = fullinfo.replace(tmpSub.cap(0), "");
    }

    // Try to make subtitle from " "
    QRegExp tmpSub2 = m_nlSub2;
    //QString tmpSubString2;
    if (tmpSub2.indexIn(fullinfo) != -1)
    {
        tmpSubString = tmpSub2.cap(0);
        tmpSubString = tmpSubString.right(tmpSubString.length() - 2);
        event.m_subtitle = tmpSubString.left(tmpSubString.length() -1);
        fullinfo = fullinfo.replace(tmpSub2.cap(0), "");
    }


    // This is trying to catch the case where the subtitle is in the main title
    // but avoid cases where it isn't a subtitle e.g cd:uk
    int position;
    if (((position = event.m_title.indexOf(":")) != -1) &&
        (event.m_title[position + 1].toUpper() == event.m_title[position + 1]) &&
        (event.m_subtitle.isEmpty()))
    {
        event.m_subtitle = event.m_title.mid(position + 1);
        event.m_title = event.m_title.left(position);
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
            event.m_originalairdate = QDate(y, 1, 1);
    }

    if (tmpYear2.indexIn(fullinfo) != -1)
    {
        bool ok;
        uint y = tmpYear2.cap(2).toUInt(&ok);
        if (ok)
            event.m_originalairdate = QDate(y, 1, 1);
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
    if (event.m_title.indexOf(m_nlOmroep) != -1)
    {
        event.m_title = event.m_title.replace(m_nlOmroep, "");
    }

    // Put information back in description

    event.m_description = fullinfo;
    event.m_description = event.m_description.trimmed();
    event.m_title       = event.m_title.trimmed();
    event.m_subtitle    = event.m_subtitle.trimmed();

}

void EITFixUp::FixCategory(DBEventEIT &event) const
{
    // remove category movie from short events
    if (event.m_categoryType == ProgramInfo::kCategoryMovie &&
        event.m_starttime.secsTo(event.m_endtime) < kMinMovieDuration)
    {
        /* default taken from ContentDescriptor::GetMythCategory */
        event.m_categoryType = ProgramInfo::kCategoryTVShow;
    }
}

/** \fn EITFixUp::FixNO(DBEventEIT&) const
 *  \brief Use this to clean DVB-S guide in Norway.
 */
void EITFixUp::FixNO(DBEventEIT &event) const
{
    // Check for "title (R)" in the title
    int position = event.m_title.indexOf(m_noRerun);
    if (position != -1)
    {
      event.m_previouslyshown = true;
      event.m_title = event.m_title.replace(m_noRerun, "");
    }
    // Check for "subtitle (HD)" in the subtitle
    position = event.m_subtitle.indexOf(m_noHD);
    if (position != -1)
    {
      event.m_videoProps |= VID_HDTV;
      event.m_subtitle = event.m_subtitle.replace(m_noHD, "");
    }
   // Check for "description (HD)" in the description
    position = event.m_description.indexOf(m_noHD);
    if (position != -1)
    {
      event.m_videoProps |= VID_HDTV;
      event.m_description = event.m_description.replace(m_noHD, "");
    }
}

/** \fn EITFixUp::FixNRK_DVBT(DBEventEIT&) const
 *  \brief Use this to clean DVB-T guide in Norway (NRK)
 */
void EITFixUp::FixNRK_DVBT(DBEventEIT &event) const
{
    QRegExp    tmpExp1;
    // Check for "title (R)" in the title
    if (event.m_title.indexOf(m_noRerun) != -1)
    {
      event.m_previouslyshown = true;
      event.m_title = event.m_title.replace(m_noRerun, "");
    }
    // Check for "(R)" in the description
    if (event.m_description.indexOf(m_noRerun) != -1)
    {
      event.m_previouslyshown = true;
    }
    // Move colon separated category from program-titles into description
    // Have seen "NRK2s historiekveld: Film: bla-bla"
    tmpExp1 =  m_noNRKCategories;
    while ((tmpExp1.indexIn(event.m_title) != -1) &&
           (tmpExp1.cap(2).length() > 1))
    {
        event.m_title  = tmpExp1.cap(2);
        event.m_description = "(" + tmpExp1.cap(1) + ") " + event.m_description;
    }
    // Remove season premiere markings
    tmpExp1 = m_noPremiere;
    if (tmpExp1.indexIn(event.m_title) >= 3)
    {
        event.m_title.remove(m_noPremiere);
    }
    // Try to find colon-delimited subtitle in title, only tested for NRK channels
    tmpExp1 = m_noColonSubtitle;
    if (!event.m_title.startsWith("CSI:") &&
        !event.m_title.startsWith("CD:") &&
        !event.m_title.startsWith("Distriktsnyheter: fra"))
    {
        if (tmpExp1.indexIn(event.m_title) != -1)
        {

            if (event.m_subtitle.length() <= 0)
            {
                event.m_title    = tmpExp1.cap(1);
                event.m_subtitle = tmpExp1.cap(2);
            }
            else if (event.m_subtitle == tmpExp1.cap(2))
            {
                event.m_title    = tmpExp1.cap(1);
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
    int        episode = -1;
    int        season = -1;
    QRegExp    tmpRegEx;
    // Title search
    // episode and part/part total
    tmpRegEx = m_dkEpisode;
    int position = event.m_title.indexOf(tmpRegEx);
    if (position != -1)
    {
      episode = tmpRegEx.cap(1).toInt();
      event.m_partnumber = tmpRegEx.cap(1).toInt();
      event.m_title = event.m_title.replace(tmpRegEx, "");
    }

    tmpRegEx = m_dkPart;
    position = event.m_title.indexOf(tmpRegEx);
    if (position != -1)
    {
      episode = tmpRegEx.cap(1).toInt();
      event.m_partnumber = tmpRegEx.cap(1).toInt();
      event.m_parttotal = tmpRegEx.cap(2).toInt();
      event.m_title = event.m_title.replace(tmpRegEx, "");
    }

    // subtitle delimiters
    tmpRegEx = m_dkSubtitle1;
    position = event.m_title.indexOf(tmpRegEx);
    if (position != -1)
    {
      event.m_title = tmpRegEx.cap(1);
      event.m_subtitle = tmpRegEx.cap(2);
    }
    else
    {
        tmpRegEx = m_dkSubtitle2;
        if(event.m_title.indexOf(tmpRegEx) != -1)
        {
            event.m_title = tmpRegEx.cap(1);
            event.m_subtitle = tmpRegEx.cap(2);
        }
    }
    // Description search
    // Season (Sæson [:digit:]+.) => episode = season episode number
    // or year (- år [:digit:]+(\\)|:) ) => episode = total episode number
    tmpRegEx = m_dkSeason1;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
      season = tmpRegEx.cap(1).toInt();
    }
    else
    {
        tmpRegEx = m_dkSeason2;
        if(event.m_description.indexOf(tmpRegEx) !=  -1)
        {
            season = tmpRegEx.cap(1).toInt();
        }
    }

    if (episode > 0)
        event.m_episode = episode;

    if (season > 0)
        event.m_season = season;

    //Feature:
    tmpRegEx = m_dkFeatures;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        QString features = tmpRegEx.cap(1);
        event.m_description = event.m_description.replace(tmpRegEx, "");
        // 16:9
        if (features.indexOf(m_dkWidescreen) !=  -1)
            event.m_videoProps |= VID_WIDESCREEN;
        // HDTV
        if (features.indexOf(m_dkHD) !=  -1)
            event.m_videoProps |= VID_HDTV;
        // Dolby Digital surround
        if (features.indexOf(m_dkDolby) !=  -1)
            event.m_audioProps |= AUD_DOLBY;
        // surround
        if (features.indexOf(m_dkSurround) !=  -1)
            event.m_audioProps |= AUD_SURROUND;
        // stereo
        if (features.indexOf(m_dkStereo) !=  -1)
            event.m_audioProps |= AUD_STEREO;
        // (G)
        if (features.indexOf(m_dkReplay) !=  -1)
            event.m_previouslyshown = true;
        // TTV
        if (features.indexOf(m_dkTxt) !=  -1)
            event.m_subtitleType |= SUB_NORMAL;
    }

    // Series and program id
    // programid is currently not transmitted
    // YouSee doesn't use a default authority but uses the first byte after
    // the / to indicate if the seriesid is global unique or unique on the
    // service id
    if (event.m_seriesId.length() >= 1 && event.m_seriesId[0] == '/')
    {
        QString newid;
        if (event.m_seriesId[1] == '1')
            newid = QString("%1%2").arg(event.m_chanid).
                    arg(event.m_seriesId.mid(2,8));
        else
            newid = event.m_seriesId.mid(2,8);
        event.m_seriesId = newid;
    }

    if (event.m_programId.length() >= 1 && event.m_programId[0] == '/')
        event.m_programId[0]='_';

    // Add season and episode number to subtitle
    if (episode > 0)
    {
        event.m_subtitle = QString("%1 (%2").arg(event.m_subtitle).arg(episode);
        if (event.m_parttotal >0)
            event.m_subtitle = QString("%1:%2").arg(event.m_subtitle).
                    arg(event.m_parttotal);
        if (season > 0)
        {
            event.m_season = season;
            event.m_episode = episode;
            event.m_syndicatedepisodenumber =
                    QString("S%1E%2").arg(season).arg(episode);
            event.m_subtitle = QString("%1 S\xE6son %2").arg(event.m_subtitle).
                    arg(season);
        }
        event.m_subtitle = QString("%1)").arg(event.m_subtitle);
    }

    // Find actors and director in description
    tmpRegEx = m_dkDirector;
    bool directorPresent = false;
    position = event.m_description.indexOf(tmpRegEx);
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
    position = event.m_description.indexOf(tmpRegEx);
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
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        bool ok;
        uint y = tmpRegEx.cap(1).toUInt(&ok);
        if (ok)
            event.m_originalairdate = QDate(y, 1, 1);
    }
    // Remove white spaces
    event.m_description = event.m_description.trimmed();
    event.m_title       = event.m_title.trimmed();
    event.m_subtitle    = event.m_subtitle.trimmed();
}

/** \fn EITFixUp::FixStripHTML(DBEventEIT&) const
 *  \brief Use this to clean HTML Tags from EIT Data
 */
void EITFixUp::FixStripHTML(DBEventEIT &event) const
{
    LOG(VB_EIT, LOG_INFO, QString("Applying html strip to %1").arg(event.m_title));
    event.m_title.remove(m_HTML);
}

// Moves the subtitle field into the description since it's just used
// as more description field. All the sort-out will happen in the description
// field. Also, sometimes the description is just a repeat of the title. If so,
// we remove it.
void EITFixUp::FixGreekSubtitle(DBEventEIT &event) const
{
    if (event.m_title == event.m_description)
    {
        event.m_description = QString("");
    }
    if (!event.m_subtitle.isEmpty())
    {
        if (event.m_subtitle.trimmed().right(1) != ".'" )
            event.m_subtitle = event.m_subtitle.trimmed() + ".";
        event.m_description = event.m_subtitle.trimmed() + QString(" ") + event.m_description;
        event.m_subtitle = QString("");
    }
}

void EITFixUp::FixGreekEIT(DBEventEIT &event) const
{
    //Live show
    int position;
    QRegExp tmpRegEx;
    position = event.m_title.indexOf("(Ζ)");
    if (position != -1)
    {
        event.m_title = event.m_title.replace("(Ζ)", "");
        event.m_description.prepend("Ζωντανή Μετάδοση. ");
    }

    // Greek not previously Shown
    position = event.m_title.indexOf(m_grNotPreviouslyShown);
    if (position != -1)
    {
        event.m_previouslyshown = false;
        event.m_title = event.m_title.replace(m_grNotPreviouslyShown, "");
    }

    // Greek Replay (Ε)
    // it might look redundant compared to previous check but at least it helps
    // remove the (Ε) From the title.
    tmpRegEx = m_grReplay;
    if (event.m_title.indexOf(tmpRegEx) !=  -1)
    {
        event.m_previouslyshown = true;
        event.m_title = event.m_title.replace(tmpRegEx, "");
    }

    // Check for (HD) in the decription
    position = event.m_description.indexOf("(HD)");
    if (position != -1)
    {
        event.m_description = event.m_description.replace("(HD)", "");
        event.m_videoProps |= VID_HDTV;
    }

    // Check for (Full HD) in the decription
    position = event.m_description.indexOf("(Full HD)");
    if (position != -1)
    {
        event.m_description = event.m_description.replace("(Full HD)", "");
        event.m_videoProps |= VID_HDTV;
    }


    tmpRegEx = m_grFixnofullstopActors;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.m_description.insert(position + 1, ".");
    }

    // If they forgot the "." at the end of the sentence before the actors/directors begin, let's insert it.
    tmpRegEx = m_grFixnofullstopDirectors;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.m_description.insert(position + 1, ".");
    }

    // Find actors and director in description
    // I am looking for actors first and then for directors/presenters because
    // sometimes punctuation is missing and the "Παίζουν:" label is mistaken
    // for a director's/presenter's surname (directors/presenters are shown
    // before actors in the description field.). So removing the text after
    // adding the actors AND THEN looking for dir/pres helps to clear things up.
    tmpRegEx = m_grActors;
    position = event.m_description.indexOf(tmpRegEx);
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
        event.m_description.replace(tmpRegEx.cap(0), "");
    }
    // Director
    tmpRegEx = m_grDirector;
    position = event.m_description.indexOf(tmpRegEx);
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
        event.m_description.replace(tmpRegEx.cap(0), "");
    }

    //Try to find presenter
    tmpRegEx = m_grPres;
    position = event.m_description.indexOf(tmpRegEx);
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
        event.m_description.replace(tmpRegEx.cap(0), "");
    }

    //find year e.g Παραγωγής 1966 ή ΝΤΟΚΙΜΑΝΤΕΡ - 1998 Κατάλληλο για όλους
    // Used in Private channels (not 'secret', just not owned by Government!)
    tmpRegEx = m_grYear;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        bool ok;
        uint y = tmpRegEx.cap(1).toUInt(&ok);
        if (ok)
        {
            event.m_originalairdate = QDate(y, 1, 1);
            event.m_description.replace(tmpRegEx, "");
        }
    }
    // Remove white spaces
    event.m_description = event.m_description.trimmed();
    event.m_title       = event.m_title.trimmed();
    event.m_subtitle    = event.m_subtitle.trimmed();
    // Remove " ."
    event.m_description = event.m_description.replace(" .",".").trimmed();

    //find country of origin and remove it from description.
    tmpRegEx = m_grCountry;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.m_description.replace(tmpRegEx, "");
    }

    // Work out the season and episode numbers (if any)
    // Matching pattern "Επεισ[όο]διο:?|Επ 3 από 14|3/14" etc
    bool    series  = false;
    QRegExp tmpSeries = m_grSeason;
    // cap(2) is the season for ΑΒΓΔ
    // cap(3) is the season for 1234
    int position1 = tmpSeries.indexIn(event.m_title);
    int position2 = tmpSeries.indexIn(event.m_description);
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
                    event.m_season = 6;
                else
                {
                    QString LettToNumber = "0ΑΒΓΔΕ6ΖΗΘΙΚΛΜΝ";
                    tmpinteger = LettToNumber.indexOf(tmpSeries.cap(2));
                    if (tmpinteger != -1)
                        event.m_season = tmpinteger;
                }
            }
        }
        else if (!tmpSeries.cap(3).isEmpty()) //number
        {
            event.m_season = tmpSeries.cap(3).toUInt();
        }
        series = true;
        if (position1 != -1)
            event.m_title.replace(tmpSeries.cap(0),"");
        if (position2 != -1)
            event.m_description.replace(tmpSeries.cap(0),"");
    }
    // If Season is in Roman Numerals (I,II,etc)
    tmpSeries = m_grSeason_as_RomanNumerals;
    if ((position1 = tmpSeries.indexIn(event.m_title)) != -1
          || (position2 = tmpSeries.indexIn(event.m_description)) != -1)
    {
        if (!tmpSeries.isEmpty()) //number
        {
            // make sure I replace greek Ι with english I
            QString romanSeries = tmpSeries.cap(1).replace("Ι","I").toUpper();
            if (romanSeries == "I")
                event.m_season = 1;
            else if (romanSeries == "II")
                event.m_season = 2;
            else if (romanSeries== "III")
                event.m_season = 3;
            else if (romanSeries == "IV")
                event.m_season = 4;
            else if (romanSeries == "V")
                event.m_season = 5;
            else if (romanSeries== "VI")
                event.m_season = 6;
            else if (romanSeries == "VII")
                event.m_season = 7;
            else if (romanSeries == "VIII")
                event.m_season = 8;
            else if (romanSeries == "IX")
                event.m_season = 9;
            else if (romanSeries == "X")
                event.m_season = 10;
            else if (romanSeries == "XI")
                event.m_season = 11;
            else if (romanSeries == "XII")
                event.m_season = 12;
            else if (romanSeries == "XII")
                event.m_season = 13;
            else if (romanSeries == "XIV")
                event.m_season = 14;
            else if (romanSeries == "XV")
                event.m_season = 15;
            else if (romanSeries == "XVI")
                event.m_season = 16;
            else if (romanSeries == "XVII")
                event.m_season = 17;
            else if (romanSeries == "XIII")
                event.m_season = 18;
            else if (romanSeries == "XIX")
                event.m_season = 19;
            else if (romanSeries == "XX")
                event.m_season = 20;
        }
        series = true;
        if (position1 != -1)
        {
            event.m_title.replace(tmpSeries.cap(0),"");
            event.m_title = event.m_title.trimmed();
            if (event.m_title.right(1) == ",")
               event.m_title.chop(1);
        }
        if (position2 != -1)
        {
            event.m_description.replace(tmpSeries.cap(0),"");
            event.m_description = event.m_description.trimmed();
            if (event.m_description.right(1) == ",")
               event.m_description.chop(1);
        }
    }


    QRegExp tmpEpisode = m_grlongEp;
    //tmpEpisode.setMinimal(true);
    // cap(1) is the Episode No.
    if ((position1 = tmpEpisode.indexIn(event.m_title)) != -1
            || (position2 = tmpEpisode.indexIn(event.m_description)) != -1)
    {
        if (!tmpEpisode.cap(1).isEmpty())
        {
            event.m_episode = tmpEpisode.cap(1).toUInt();
            series = true;
            if (position1 != -1)
                event.m_title.replace(tmpEpisode.cap(0),"");
            if (position2 != -1)
                event.m_description.replace(tmpEpisode.cap(0),"");
            // Sometimes description omits Season if it's 1. We fix this
            if (0 == event.m_season)
                event.m_season = 1;
        }
    }
    // Sometimes, especially on greek national tv, they include comments in the
    // title, e.g "connection to ert1", "ert archives".
    // Because they obscure the real title, I'll isolate and remove them.

    QRegExp tmpComment = m_grCommentsinTitle;
    tmpComment.setMinimal(true);
    position = event.m_title.indexOf(tmpComment);
    if (position != -1)
    {
        event.m_title.replace(tmpComment.cap(0),"");
    }

    // Sometimes the real (mostly English) title of a movie or series is
    // enclosed in parentheses in the event title, subtitle or description.
    // Since the subtitle has been moved to the description field by
    // EITFixUp::FixGreekSubtitle, I will search for it only in the description.
    // It will replace the translated one to get better chances of metadata
    // retrieval. The old title will be moved in the description.
    QRegExp tmptitle = m_grRealTitleinDescription;
    tmptitle.setMinimal(true);
    position = event.m_description.indexOf(tmptitle);
    if (position != -1)
    {
        event.m_description = event.m_description.replace(tmptitle, "");
        if (tmptitle.cap(0) != event.m_title.trimmed())
        {
            event.m_description = "(" + event.m_title.trimmed() + "). " + event.m_description;
        }
        event.m_title = tmptitle.cap(1);
        // Remove the real title from the description
    }
    else // search in title
    {
        tmptitle = m_grRealTitleinTitle;
        position = event.m_title.indexOf(tmptitle);
        if (position != -1) // found in title instead
        {
            event.m_title.replace(tmptitle.cap(0),"");
            QString tmpTranslTitle = event.m_title;
            //QString tmpTranslTitle = event.m_title.replace(tmptitle.cap(0),"");
            event.m_title = tmptitle.cap(1);
            event.m_description = "(" + tmpTranslTitle.trimmed() + "). " + event.m_description;
        }
    }

    // Description field: "^Episode: Lion in the cage. (Description follows)"
    tmpRegEx = m_grEpisodeAsSubtitle;
    position = event.m_description.indexOf(tmpRegEx);
    if (position != -1)
    {
        event.m_subtitle = tmpRegEx.cap(1).trimmed();
        event.m_description.replace(tmpRegEx, "");
    }
    QRegExp m_grMovie("\\bταιν[ιί]α\\b",Qt::CaseInsensitive);
    bool isMovie = (event.m_description.indexOf(m_grMovie) !=-1) ;
    if (isMovie)
    {
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    }
    else if (series)
    {
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }
    // just for luck, retrim fields.
    event.m_description = event.m_description.trimmed();
    event.m_title       = event.m_title.trimmed();
    event.m_subtitle    = event.m_subtitle.trimmed();

// να σβήσω τα κομμάτια που περισσεύουν από την περιγραφή πχ παραγωγής χχχχ
}

void EITFixUp::FixGreekCategories(DBEventEIT &event) const
{
    if (event.m_description.indexOf(m_grCategComedy) != -1)
    {
        event.m_category = "Κωμωδία";
    }
    else if (event.m_description.indexOf(m_grCategTeleMag) != -1)
    {
        event.m_category = "Τηλεπεριοδικό";
    }
    else if (event.m_description.indexOf(m_grCategNature) != -1)
    {
        event.m_category = "Επιστήμη/Φύση";
    }
    else if (event.m_description.indexOf(m_grCategHealth) != -1)
    {
        event.m_category = "Υγεία";
    }
    else if (event.m_description.indexOf(m_grCategReality) != -1)
    {
        event.m_category = "Ριάλιτι";
    }
    else if (event.m_description.indexOf(m_grCategDrama) != -1)
    {
        event.m_category = "Κοινωνικό";
    }
    else if (event.m_description.indexOf(m_grCategChildren) != -1)
    {
        event.m_category = "Παιδικό";
    }
    else if (event.m_description.indexOf(m_grCategSciFi) != -1)
    {
        event.m_category = "Επιστ.Φαντασίας";
    }
    else if ((event.m_description.indexOf(m_grCategFantasy) != -1)
             && (event.m_description.indexOf(m_grCategMystery) != -1))
    {
        event.m_category = "Φαντασίας/Μυστηρίου";
    }
    else if (event.m_description.indexOf(m_grCategMystery) != -1)
    {
        event.m_category = "Μυστηρίου";
    }
    else if (event.m_description.indexOf(m_grCategFantasy) != -1)
    {
        event.m_category = "Φαντασίας";
    }
    else if (event.m_description.indexOf(m_grCategHistory) != -1)
    {
        event.m_category = "Ιστορικό";
    }
    else if (event.m_description.indexOf(m_grCategTeleShop) != -1
            || event.m_title.indexOf(m_grCategTeleShop) != -1)
    {
        event.m_category = "Τηλεπωλήσεις";
    }
    else if (event.m_description.indexOf(m_grCategFood) != -1)
    {
        event.m_category = "Γαστρονομία";
    }
    else if (event.m_description.indexOf(m_grCategGameShow) != -1
             || event.m_title.indexOf(m_grCategGameShow) != -1)
    {
        event.m_category = "Τηλεπαιχνίδι";
    }
    else if (event.m_description.indexOf(m_grCategBiography) != -1)
    {
        event.m_category = "Βιογραφία";
    }
    else if (event.m_title.indexOf(m_grCategNews) != -1)
    {
        event.m_category = "Ειδήσεις";
    }
    else if (event.m_description.indexOf(m_grCategSports) != -1)
    {
        event.m_category = "Αθλητικά";
    }
    else if (event.m_description.indexOf(m_grCategMusic) != -1
            || event.m_title.indexOf(m_grCategMusic) != -1)
    {
        event.m_category = "Μουσική";
    }
    else if (event.m_description.indexOf(m_grCategDocumentary) != -1)
    {
        event.m_category = "Ντοκιμαντέρ";
    }
    else if (event.m_description.indexOf(m_grCategReligion) != -1)
    {
        event.m_category = "Θρησκεία";
    }
    else if (event.m_description.indexOf(m_grCategCulture) != -1)
    {
        event.m_category = "Τέχνες/Πολιτισμός";
    }
    else if (event.m_description.indexOf(m_grCategSpecial) != -1)
    {
        event.m_category = "Αφιέρωμα";
    }

}

void EITFixUp::FixUnitymedia(DBEventEIT &event) const
{
    // TODO handle scraping the category and category_type from localized text in the short/long description
    // TODO remove short description (stored as episode title) which is just the beginning of the long description (actual description)

    // drop the short description if its copy the start of the long description
    if (event.m_description.startsWith (event.m_subtitle))
    {
        event.m_subtitle = "";
    }

    // handle cast and crew in items in the DVB Extended Event Descriptor
    // remove handled items from the map, so the left overs can be reported
    QMap<QString,QString>::iterator i = event.m_items.begin();
    while (i != event.m_items.end())
    {
        if ((QString::compare (i.key(), "Role Player") == 0) ||
            (QString::compare (i.key(), "Performing Artist") == 0))
        {
            event.AddPerson (DBPerson::kActor, i.value());
            i = event.m_items.erase (i);
        }
        else if (QString::compare (i.key(), "Director") == 0)
        {
            event.AddPerson (DBPerson::kDirector, i.value());
            i = event.m_items.erase (i);
        }
        else if (QString::compare (i.key(), "Commentary or Commentator") == 0)
        {
            event.AddPerson (DBPerson::kCommentator, i.value());
            i = event.m_items.erase (i);
        }
        else if (QString::compare (i.key(), "Presenter") == 0)
        {
            event.AddPerson (DBPerson::kPresenter, i.value());
            i = event.m_items.erase (i);
        }
        else if (QString::compare (i.key(), "Producer") == 0)
        {
            event.AddPerson (DBPerson::kProducer, i.value());
            i = event.m_items.erase (i);
        }
        else if (QString::compare (i.key(), "Scriptwriter") == 0)
        {
            event.AddPerson (DBPerson::kWriter, i.value());
            i = event.m_items.erase (i);
        }
        else
        {
            ++i;
        }
    }

    // handle star rating in the description
    QRegExp tmp = m_unitymediaImdbrating;
    if (event.m_description.indexOf (tmp) != -1)
    {
        float stars = tmp.cap(1).toFloat();
        event.m_stars = stars / 10.0F;
        event.m_description.replace (m_unitymediaImdbrating, "");
    }
}
