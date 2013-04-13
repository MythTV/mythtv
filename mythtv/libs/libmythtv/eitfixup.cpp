// C++ headers
#include <algorithm>

// MythTV headers
#include "eitfixup.h"
#include "programinfo.h" // for CategoryType
#include "channelutil.h" // for GetDefaultAuthority()

#include "programinfo.h" // for subtitle types and audio and video properties
#include "dishdescriptors.h" // for dish_theme_type_to_string

/*------------------------------------------------------------------------
 * Event Fix Up Scripts - Turned on by entry in dtv_privatetype table
 *------------------------------------------------------------------------*/

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
      m_ukCEPQ("[:\\!\\.\\?]"),
      m_ukColonPeriod("[:\\.]"),
      m_ukDotSpaceStart("^\\. "),
      m_ukDotEnd("\\.$"),
      m_ukSpaceColonStart("^[ |:]*"),
      m_ukSpaceStart("^ "),
      m_ukSeries("\\s*\\(?\\s*(?:Episode|Part|Pt)?\\s*(\\d{1,2})\\s*(?:of|/)\\s*(\\d{1,2})\\s*\\)?\\s*(?:\\.|:)?", Qt::CaseInsensitive),
      m_ukCC("\\[(?:(AD|SL|S|W),?)+\\]"),
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
      m_comHemCountry("^(\\(.+\\))?\\s?([^ ]+)\\s([^\\.0-9]+)"
                      "(?:\\sfrån\\s([0-9]{4}))(?:\\smed\\s([^\\.]+))?\\.?"),
      m_comHemDirector("[Rr]egi"),
      m_comHemActor("[Ss]kådespelare|[Ii] rollerna"),
      m_comHemHost("[Pp]rogramledare"),
      m_comHemSub("[.\\?\\!] "),
      m_comHemRerun1("[Rr]epris\\sfrån\\s([^\\.]+)(?:\\.|$)"),
      m_comHemRerun2("([0-9]+)/([0-9]+)(?:\\s-\\s([0-9]{4}))?"),
      m_comHemTT("[Tt]ext-[Tt][Vv]"),
      m_comHemPersSeparator("(, |\\soch\\s)"),
      m_comHemPersons("\\s?([Rr]egi|[Ss]kådespelare|[Pp]rogramledare|"
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
      m_RTLEpisodeNo1("^(Folge\\s\\d{1,4})\\.*\\s*"),
      m_RTLEpisodeNo2("^(\\d{1,2}\\/[IVX]+)\\.*\\s*"),
      m_fiRerun("\\ ?Uusinta[a-zA-Z\\ ]*\\.?"),
      m_fiRerun2("\\([Uu]\\)"),
      m_dePremiereInfos("([^.]+)?\\s?([0-9]{4})\\.\\s[0-9]+\\sMin\\.(?:\\sVon"
                        "\\s([^,]+)(?:,|\\su\\.\\sa\\.)\\smit\\s(.+)\\.)?"),
      m_dePremiereOTitle("\\s*\\(([^\\)]*)\\)$"),
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
      m_dkSeason1("Sæson ([0-9]+)\\."),
      m_dkSeason2("- år ([0-9]+)(?: :)"),
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
      m_AUFreeviewSYC("(.*) \\((.+)\\) \\(([12][0-9][0-9][0-9])\\) \\((.+)\\)$")
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

    bool isMovie = event.category.startsWith("Movie",Qt::CaseInsensitive);
    // BBC three case (could add another record here ?)
    event.description = event.description.remove(m_ukThen);
    event.description = event.description.remove(m_ukNew);

    // Removal of Class TV, CBBC and CBeebies etc..
    event.title = event.title.remove(m_ukTitleRemove);
    event.description = event.description.remove(m_ukDescriptionRemove);

    // Removal of BBC FOUR and BBC THREE
    event.description = event.description.remove(m_ukBBC34);

    // BBC 7 [Rpt of ...] case.
    event.description = event.description.remove(m_ukBBC7rpt);

    // "All New To 4Music!
    event.description = event.description.remove(m_ukAllNew);

    // Remove [AD,S] etc.
    QRegExp tmpCC = m_ukCC;
    if ((position1 = tmpCC.indexIn(event.description)) != -1)
    {
        QStringList tmpCCitems = tmpCC.cap(0).remove("[").remove("]").split(",");
        if (tmpCCitems.contains("AD"))
            event.audioProps |= AUD_VISUALIMPAIR;
        if (tmpCCitems.contains("S"))
            event.subtitleType |= SUB_NORMAL;
        if (tmpCCitems.contains("SL"))
            event.subtitleType |= SUB_SIGNED;
        if (tmpCCitems.contains("W"))
            event.videoProps |= VID_WIDESCREEN;
        event.description = event.description.remove(m_ukCC);
    }

    event.title       = event.title.trimmed();
    event.description = event.description.trimmed();

    // Work out the episode numbers (if any)
    bool    series  = false;
    QRegExp tmpExp1 = m_ukSeries;
    if ((position1 = tmpExp1.indexIn(event.title)) != -1)
    {
        if ((tmpExp1.cap(1).toUInt() <= tmpExp1.cap(2).toUInt())
            && tmpExp1.cap(2).toUInt()<=50)
        {
            event.partnumber = tmpExp1.cap(1).toUInt();
            event.parttotal  = tmpExp1.cap(2).toUInt();
            // Remove from the title
            event.title = event.title.left(position1) +
                event.title.mid(position1 + tmpExp1.cap(0).length());
            series = true;
        }
    }
    else if ((position1 = tmpExp1.indexIn(event.description)) != -1)
    {
        if ((tmpExp1.cap(1).toUInt() <= tmpExp1.cap(2).toUInt())
            && tmpExp1.cap(2).toUInt()<=50)
        {
            event.partnumber = tmpExp1.cap(1).toUInt();
            event.parttotal  = tmpExp1.cap(2).toUInt();
#if 0       // Remove from the description
            event.description = event.description.left(position1) +
                event.description.mid(position1+tmpExp1.cap(0).length());
#endif
            series = true;
        }
    }
    if (series)
        event.categoryType = ProgramInfo::kCategorySeries;

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
    if (!event.title.startsWith("CSI:") && !event.title.startsWith("CD:"))
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

    if (!isMovie && event.subtitle.isEmpty())
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
                QString("E%1S%2").arg(episode).arg(season);
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
        int position;
        const uint SUBTITLE_PCT = 35; // % of description to allow subtitle up to
        const uint SUBTITLE_MAX_LEN = 50; // max length of subtitle field in db

        if ((position = tmpExp1.indexIn(event.description)) != -1)
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
        country = tmpInfos.cap(1).trimmed();
        bool ok;
        uint y = tmpInfos.cap(2).toUInt(&ok);
        if (ok)
            event.airdate = y;
        event.AddPerson(DBPerson::kDirector, tmpInfos.cap(3));
        const QStringList actors = tmpInfos.cap(4).split(
            ", ", QString::SkipEmptyParts);
        QStringList::const_iterator it = actors.begin();
        for (; it != actors.end(); ++it)
            event.AddPerson(DBPerson::kActor, *it);
        event.description = event.description.replace(tmpInfos.cap(0), "");
    }

    // move the original titel from the title to subtitle
    QRegExp tmpOTitle = m_dePremiereOTitle;
    if (tmpOTitle.indexIn(event.title) != -1)
    {
        event.subtitle = QString("%1, %2").arg(tmpOTitle.cap(1)).arg(country);
        event.title = event.title.replace(tmpOTitle.cap(0), "");
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
        event.categoryType = ProgramInfo::                                                                      kCategorySeries;
    }

    // Get stereo info
    int position;
    if ((position = fullinfo.indexOf(m_Stereo)) != -1)
    {
        event.audioProps |= AUD_STEREO;
        fullinfo = fullinfo.replace(m_Stereo, ".");
    }

    //Get widescreen info
    if ((position = fullinfo.indexOf(m_nlWide)) != -1)
    {
        fullinfo = fullinfo.replace("breedbeeld", ".");
    }

    // Get repeat info
    if ((position = fullinfo.indexOf(m_nlRepeat)) != -1)
    {
        fullinfo = fullinfo.replace("herh.", ".");
    }

    // Get teletext subtitle info
    if ((position = fullinfo.indexOf(m_nlTxt)) != -1)
    {
        event.subtitleType |= SUB_NORMAL;
        fullinfo = fullinfo.replace("txt", ".");
    }

    // Get HDTV information
    if ((position = event.title.indexOf(m_nlHD)) != -1)
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
    if ((position = tmpYear1.indexIn(fullinfo)) != -1)
    {
        bool ok;
        uint y = tmpYear1.cap(0).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
    }

    if ((position = tmpYear2.indexIn(fullinfo)) != -1)
    {
        bool ok;
        uint y = tmpYear2.cap(2).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
    }

    // Try to find director
    QRegExp tmpDirector = m_nlDirector;
    QString tmpDirectorString;
    if ((position = fullinfo.indexOf(m_nlDirector)) != -1)
    {
        tmpDirectorString = tmpDirector.cap(0);
        event.AddPerson(DBPerson::kDirector, tmpDirectorString);
    }

    // Strip leftovers
    if ((position = fullinfo.indexOf(m_nlRub)) != -1)
    {
        fullinfo = fullinfo.replace(m_nlRub, "");
    }

    // Strip category info from description
    if ((position = fullinfo.indexOf(m_nlCat)) != -1)
    {
        fullinfo = fullinfo.replace(m_nlCat, "");
    }

    // Remove omroep from title
    if ((position = event.title.indexOf(m_nlOmroep)) != -1)
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
    int        position;
    QRegExp    tmpExp1;
    // Check for "title (R)" in the title
    position = event.title.indexOf(m_noRerun);
    if (position != -1)
    {
      event.previouslyshown = true;
      event.title = event.title.replace(m_noRerun, "");
    }
    // Check for "(R)" in the description
    position = event.description.indexOf(m_noRerun);
    if (position != -1)
    {
      event.previouslyshown = true;
    }
    // Move colon separated category from program-titles into description
    // Have seen "NRK2s historiekveld: Film: bla-bla"
    tmpExp1 =  m_noNRKCategories;
    while (((position = tmpExp1.indexIn(event.title)) != -1) &&
           (tmpExp1.cap(2).length() > 1))
    {
        event.title  = tmpExp1.cap(2);
        event.description = "(" + tmpExp1.cap(1) + ") " + event.description;
    }
    // Remove season premiere markings
    tmpExp1 = m_noPremiere;
    if ((position = tmpExp1.indexIn(event.title)) >=3)
    {
        event.title.remove(m_noPremiere);
    }
    // Try to find colon-delimited subtitle in title, only tested for NRK channels
    tmpExp1 = m_noColonSubtitle;
    if (!event.title.startsWith("CSI:") &&
        !event.title.startsWith("CD:") &&
        !event.title.startsWith("Distriktsnyheter: fra"))
    {
        if ((position = tmpExp1.indexIn(event.title)) != -1)
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
    if(episode>0)
    {
        event.subtitle = QString("%1 (%2").arg(event.subtitle).arg(episode);
        if (event.parttotal >0)
            event.subtitle = QString("%1:%2").arg(event.subtitle).
                    arg(event.parttotal);
        if (season>0)
        {
            event.syndicatedepisodenumber =
                    QString("E%1S%2").arg(episode).arg(season);
            event.subtitle = QString("%1 Sæson %2").arg(event.subtitle).
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
