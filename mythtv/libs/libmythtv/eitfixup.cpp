// C++ headers
#include <algorithm>
#include <array>

// Qt Headers
#include <QRegularExpression>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h" // for CategoryType, subtitle types and audio and video properties

#include "channelutil.h" // for GetDefaultAuthority()
#include "eitfixup.h"
#include "mpeg/dishdescriptors.h" // for dish_theme_type_to_string

/*------------------------------------------------------------------------
 * Event Fix Up Scripts - Turned on by entry in dtv_privatetype table
 *------------------------------------------------------------------------*/

static const QRegularExpression kStereo { R"(\b\(?[sS]tereo\)?\b)" };
static const QRegularExpression kUKSpaceColonStart { R"(^[ |:]*)" };
static const QRegularExpression kDotAtEnd { "\\.$" };

static const QMap<QChar,quint16> r2v = {
    {'I' ,   1}, {'V' ,   5}, {'X' ,   10}, {'L' , 50},
    {'C' , 100}, {'D' , 500}, {'M' , 1000},
    {QChar(0x399), 1}, // Greek Ι
};

int EITFixUp::parseRoman (QString roman)
{
    if (roman.isEmpty())
        return 0;

    uint result = 0;
    for (int i = 0; i < roman.size() - 1; i++)
    {
        int v1 = r2v[roman.at(i)];
        int v2 = r2v[roman.at(i+1)];
        result += (v1 >= v2) ? v1 : -v1;
    }
    return result + r2v[roman.back()];
}


void EITFixUp::Fix(DBEventEIT &event)
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

    // Clean up text strings after all fixups have been applied.
    if (event.m_fixup)
    {
        static const QRegularExpression emptyParens { R"(\(\s*\))" };
        if (!event.m_title.isEmpty())
        {
            event.m_title.remove(QChar('\0')).remove(emptyParens);
            event.m_title = event.m_title.simplified();
        }

        if (!event.m_subtitle.isEmpty())
        {
            event.m_subtitle.remove(QChar('\0'));
            event.m_subtitle.remove(emptyParens);
            event.m_subtitle = event.m_subtitle.simplified();
        }

        if (!event.m_description.isEmpty())
        {
            event.m_description.remove(QChar('\0'));
            event.m_description.remove(emptyParens);
            event.m_description = event.m_description.simplified();
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
        for (auto i = event.m_items.begin(); i != event.m_items.end(); ++i)
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Unhandled item in EIT for"
                " channel id \"%1\", \"%2\": %3").arg(event.m_chanid)
                .arg(i.key(), i.value()));
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
void EITFixUp::FixBellExpressVu(DBEventEIT &event)
{
    // A 0x0D character is present between the content
    // and the subtitle if its present
    int position = event.m_description.indexOf('\r');

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
    static const QRegularExpression bellYear { R"(\([0-9]{4}\))" };
    position = event.m_description.indexOf(bellYear);
    if (position != -1 && !event.m_category.isEmpty())
    {
        // Parse out the year
        bool ok = false;
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
            static const QRegularExpression bellActors { R"(\set\s|,)" };
            QString tmp = event.m_description.left(position-3);
            QStringList actors =
                tmp.split(bellActors, Qt::SkipEmptyParts);

            /* Possible TODO: if EIT inlcude the priority and/or character
             * names for the actors, include them in AddPerson call. */
            for (const auto & actor : std::as_const(actors))
                event.AddPerson(DBPerson::kActor, actor);
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
    auto match = kStereo.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_audioProps |= AUD_STEREO;
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
    }

    // Check for "title (All Day, HD)" in the title
    static const QRegularExpression bellPPVTitleAllDayHD { R"(\s*\(All Day\, HD\)\s*$)" };
    match = bellPPVTitleAllDayHD.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_title.remove(match.capturedStart(), match.capturedLength());
        event.m_videoProps |= VID_HDTV;
     }

    // Check for "title (All Day)" in the title
    static const QRegularExpression bellPPVTitleAllDay { R"(\s*\(All Day.*\)\s*$)" };
    match = bellPPVTitleAllDay.match(event.m_title);
    if (match.hasMatch())
        event.m_title.remove(match.capturedStart(), match.capturedLength());

    // Check for "HD - title" in the title
    static const QRegularExpression bellPPVTitleHD { R"(^HD\s?-\s?)" };
    match = bellPPVTitleHD.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_title.remove(match.capturedStart(), match.capturedLength());
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
        event.m_title = event.m_title.replace("(HD)", "");
        event.m_videoProps |= VID_HDTV;
    }

    // Check for HD at the end of the title
    static const QRegularExpression dishPPVTitleHD { R"(\sHD\s*$)" };
    match = dishPPVTitleHD.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_title.remove(match.capturedStart(), match.capturedLength());
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
    static const QRegularExpression dishPPVTitleColon { R"(\:\s*$)" };
    match = dishPPVTitleColon.match(event.m_title);
    if (match.hasMatch())
        event.m_title.remove(match.capturedStart(), match.capturedLength());

    // Remove New at the end of the description
    static const QRegularExpression dishDescriptionNew { R"(\s*New\.\s*)" };
    match = dishDescriptionNew.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = false;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove Series Finale at the end of the desciption
    static const QRegularExpression dishDescriptionFinale { R"(\s*(Series|Season)\sFinale\.\s*)" };
    match = dishDescriptionFinale.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = false;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove Series Finale at the end of the desciption
    static const QRegularExpression dishDescriptionFinale2 { R"(\s*Finale\.\s*)" };
    match = dishDescriptionFinale2.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = false;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove Series Premiere at the end of the description
    static const QRegularExpression dishDescriptionPremiere { R"(\s*(Series|Season)\s(Premier|Premiere)\.\s*)" };
    match = dishDescriptionPremiere.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = false;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove Series Premiere at the end of the description
    static const QRegularExpression dishDescriptionPremiere2 { R"(\s*(Premier|Premiere)\.\s*)" };
    match = dishDescriptionPremiere2.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = false;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove Dish's PPV code at the end of the description
    static const QRegularExpression ppvcode { R"(\s*\(([A-Z]|[0-9]){5}\)\s*$)",
        QRegularExpression::CaseInsensitiveOption };
    match = ppvcode.match(event.m_description);
    if (match.hasMatch())
       event.m_description.remove(match.capturedStart(), match.capturedLength());

    // Remove trailing garbage
    static const QRegularExpression dishPPVSpacePerenEnd { R"(\s\)\s*$)" };
    match = dishPPVSpacePerenEnd.match(event.m_description);
    if (match.hasMatch())
       event.m_description.remove(match.capturedStart(), match.capturedLength());

    // Check for subtitle "All Day (... Eastern)" in the subtitle
    static const QRegularExpression bellPPVSubtitleAllDay { R"(^All Day \(.*\sEastern\)\s*$)" };
    match = bellPPVSubtitleAllDay.match(event.m_subtitle);
    if (match.hasMatch())
       event.m_subtitle.remove(match.capturedStart(), match.capturedLength());

    // Check for description "(... Eastern)" in the description
    static const QRegularExpression bellPPVDescriptionAllDay { R"(^\(.*\sEastern\))" };
    match = bellPPVDescriptionAllDay.match(event.m_description);
    if (match.hasMatch())
       event.m_description.remove(match.capturedStart(), match.capturedLength());

    // Check for description "(... ET)" in the description
    static const QRegularExpression bellPPVDescriptionAllDay2 { R"(^\([0-9].*am-[0-9].*am\sET\))" };
    match = bellPPVDescriptionAllDay2.match(event.m_description);
    if (match.hasMatch())
       event.m_description.remove(match.capturedStart(), match.capturedLength());

    // Check for description "(nnnnn)" in the description
    static const QRegularExpression bellPPVDescriptionEventId { R"(\([0-9]{5}\))" };
    match = bellPPVDescriptionEventId.match(event.m_description);
    if (match.hasMatch())
       event.m_description.remove(match.capturedStart(), match.capturedLength());
}

/**
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::SetUKSubtitle(DBEventEIT &event)
{
    QStringList strListColon = event.m_description.split(":");
    QStringList strListEnd;

    bool fColon = false;
    bool fQuotedSubtitle = false;
    QString strEnd;
    if (strListColon.count()>1)
    {
         bool fDoubleDot = false;
         bool fSingleDot = true;
         int nLength = strListColon[0].length();

         int nPosition1 = event.m_description.indexOf("..");
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
             for (int i =0; (i<strListColon.count()) && (nTitleMax==-1);i++)
             {
                 const QStringList tmp = strListColon[i].split(" ");

                 nTitle += tmp.size();

                 if (nTitle < kMaxToTitle)
                     strListTmp.push_back(strListColon[i]);
                 else
                     nTitleMax=i;
             }
             QString strPartial;
             for (int i=0;i<(nTitleMax-1);i++)
                 strPartial+=strListTmp[i]+":";
             if (nTitleMax>0)
             {
                 strPartial+=strListTmp[nTitleMax-1];
                 strListEnd.push_back(strPartial);
             }
             for (int i=nTitleMax+1;i<strListColon.count();i++)
                 strListEnd.push_back(strListColon[i]);
             fColon = true;
         }
    }
    static const QRegularExpression ukQuotedSubtitle { R"(^'([\w\s\-,]+?)\.' )" };
    auto match = ukQuotedSubtitle.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_subtitle = match.captured(1);
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
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
            int nPosition1 = event.m_description.indexOf(".");
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
        {
            strEnd.clear();
        }
    }

    if (!strListEnd.empty())
    {
        QStringList strListSpace = strListEnd[0].split(
            " ", Qt::SkipEmptyParts);
        if (fColon && ((uint)strListSpace.size() > kMaxToTitle))
             return;
        if ((uint)strListSpace.size() > kDotToTitle)
             return;
        static const QRegularExpression ukExclusionFromSubtitle {
            "(starring|stars\\s|drama|seres|sitcom)",
            QRegularExpression::CaseInsensitiveOption };
        if (strListSpace.filter(ukExclusionFromSubtitle).empty())
        {
             event.m_subtitle = strListEnd[0]+strEnd;
             event.m_subtitle.remove(kUKSpaceColonStart);
             event.m_description=
                          event.m_description.mid(strListEnd[0].length()+1);
             event.m_description.remove(kUKSpaceColonStart);
        }
    }
}


/**
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::FixUK(DBEventEIT &event)
{
    static const QRegularExpression uk24ep { R"(^\d{1,2}:00[ap]m to \d{1,2}:00[ap]m: )" };
    static const QRegularExpression ukTime { R"(\d{1,2}[\.:]\d{1,2}\s*(am|pm|))" };
    QString strFull;

    bool isMovie = event.m_category.startsWith("Movie",Qt::CaseInsensitive) ||
                   event.m_category.startsWith("Film",Qt::CaseInsensitive);
    // BBC three case (could add another record here ?)
    static const QRegularExpression ukThen { R"(\s*?(Then|Followed by) 60 Seconds\.)",
        QRegularExpression::CaseInsensitiveOption };
    static const QRegularExpression ukNew { R"((New\.|\s*?(Brand New|New)\s*?(Series|Episode)\s*?[:\.\-]))",
        QRegularExpression::CaseInsensitiveOption };
    static const QRegularExpression ukNewTitle { R"(^(Brand New|New:)\s*)",
        QRegularExpression::CaseInsensitiveOption };
    event.m_description = event.m_description.remove(ukThen);
    event.m_description = event.m_description.remove(ukNew);
    event.m_title = event.m_title.remove(ukNewTitle);

    // Removal of Class TV, CBBC and CBeebies etc..
    static const QRegularExpression ukTitleRemove { "^(?:[tT]4:|Schools\\s*?:)" };
    static const QRegularExpression ukDescriptionRemove { R"(^(?:CBBC\s*?\.|CBeebies\s*?\.|Class TV\s*?:|BBC Switch\.))" };
    event.m_title = event.m_title.remove(ukTitleRemove);
    event.m_description = event.m_description.remove(ukDescriptionRemove);

    // Removal of BBC FOUR and BBC THREE
    static const QRegularExpression ukBBC34 { R"(BBC (?:THREE|FOUR) on BBC (?:ONE|TWO)\.)",
        QRegularExpression::CaseInsensitiveOption };
    event.m_description = event.m_description.remove(ukBBC34);

    // BBC 7 [Rpt of ...] case.
    static const QRegularExpression ukBBC7rpt { R"(\[Rptd?[^]]+?\d{1,2}\.\d{1,2}[ap]m\]\.)" };
    event.m_description = event.m_description.remove(ukBBC7rpt);

    // "All New To 4Music!
    static const QRegularExpression ukAllNew { R"(All New To 4Music!\s?)" };
    event.m_description = event.m_description.remove(ukAllNew);

    // Removal of 'Also in HD' text
    static const QRegularExpression ukAlsoInHD { R"(\s*Also in HD\.)",
        QRegularExpression::CaseInsensitiveOption };
    event.m_description = event.m_description.remove(ukAlsoInHD);

    // Remove [AD,S] etc.
    static const QRegularExpression ukCC { R"(\[(?:(AD|SL|S|W|HD),?)+\])" };
    auto match = ukCC.match(event.m_description);
    while (match.hasMatch())
    {
        QStringList tmpCCitems = match.captured(0).remove("[").remove("]").split(",");
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
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
        match = ukCC.match(event.m_description, match.capturedStart(0));
    }

    event.m_title       = event.m_title.trimmed();
    event.m_description = event.m_description.trimmed();

    // Constituents of UK season regexp, decomposed for clarity

    // Matches Season 2, S 2 and "Series 2," etc but not "hits 2"
    // cap1 = season
    static const QString seasonStr = R"(\b(?:Season|Series|S)\s*(\d+)\s*,?)";

    // Work out the season and episode numbers (if any)
    // Matching pattern "Season 2 Episode|Ep 3 of 14|3/14" etc

    // Matches Episode 3, Ep 3/4, Ep 3 of 4 etc but not "step 1"
    // cap1 = ep, cap2 = total
    static const QString longEp = R"(\b(?:Ep|Episode)\s*(\d+)\s*(?:(?:/|of)\s*(\d*))?)";

    // Matches S2 Ep 3/4, "Season 2, Ep 3 of 4", Episode 3 etc
    // cap1 = season, cap2 = ep, cap3 = total
    static const QString longSeasEp = QString("\\(?(?:%1)?\\s*%2").arg(seasonStr, longEp);

    // Matches long seas/ep with surrounding parenthesis & trailing period
    // cap1 = season, cap2 = ep, cap3 = total
    static const QString longContext = QString(R"(\(*%1\s*\)?\s*\.?)").arg(longSeasEp);

    // Matches 3/4, 3 of 4
    // cap1 = ep, cap2 = total
    static const QString shortEp = R"((\d+)\s*(?:/|of)\s*(\d+))";

    // Matches short ep/total, ignoring Parts and idioms such as 9/11, 24/7 etc.
    // ie. x/y in parenthesis or has no leading or trailing text in the sentence.
    // cap0 may include previous/anchoring period
    // cap1 = shortEp with surrounding parenthesis & trailing period (to remove)
    // cap2 = ep, cap3 = total,
    static const QString shortContext =
            QString(R"((?:^|\.)(\s*\(*\s*%1[\s)]*(?:[).:]|$)))").arg(shortEp);

    // Prefer long format resorting to short format
    // cap0 = long match to remove, cap1 = long season, cap2 = long ep, cap3 = long total,
    // cap4 = short match to remove, cap5 = short ep, cap6 = short total
    static const QRegularExpression ukSeries { "(?:" + longContext + "|" + shortContext + ")",
        QRegularExpression::CaseInsensitiveOption };

    bool series  = false;
    bool fromTitle = true;
    match = ukSeries.match(event.m_title);
    if (!match.hasMatch())
    {
        fromTitle = false;
        match = ukSeries.match(event.m_description);
    }
    if (match.hasMatch())
    {
        if (!match.captured(1).isEmpty())
        {
            event.m_season = match.captured(1).toUInt();
            series = true;
        }

        if (!match.captured(2).isEmpty())
        {
            event.m_episode = match.captured(2).toUInt();
            series = true;
        }
        else if (!match.captured(5).isEmpty())
        {
            event.m_episode = match.captured(5).toUInt();
            series = true;
        }

        if (!match.captured(3).isEmpty())
        {
            event.m_totalepisodes = match.captured(3).toUInt();
            series = true;
        }
        else if (!match.captured(6).isEmpty())
        {
            event.m_totalepisodes = match.captured(6).toUInt();
            series = true;
        }

        // Remove long or short match. Short text doesn't start at position2
        int form = match.captured(4).isEmpty() ? 0 : 4;

        if (fromTitle)
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Extracted S%1E%2/%3 from title (%4) \"%5\"")
                .arg(event.m_season).arg(event.m_episode).arg(event.m_totalepisodes)
                .arg(event.m_title, event.m_description));

            event.m_title.remove(match.capturedStart(form),
                                 match.capturedLength(form));
        }
        else
        {
            LOG(VB_EIT, LOG_DEBUG, QString("Extracted S%1E%2/%3 from description (%4) \"%5\"")
                .arg(event.m_season).arg(event.m_episode).arg(event.m_totalepisodes)
                .arg(event.m_title, event.m_description));

            if (match.capturedStart(form) == 0)
            {
                // Remove from the start of the description.
                // Otherwise it ends up in the subtitle.
                event.m_description.remove(match.capturedStart(form),
                                 match.capturedLength(form));
            }
        }
    }

    if (isMovie)
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    else if (series)
        event.m_categoryType = ProgramInfo::kCategorySeries;

    // Multi-part episodes, or films (e.g. ITV film split by news)
    // Matches Part 1, Pt 1/2, Part 1 of 2 etc.
    static const QRegularExpression ukPart { R"([-(\:,.]\s*(?:Part|Pt)\s*(\d+)\s*(?:(?:of|/)\s*(\d+))?\s*[-):,.])",
        QRegularExpression::CaseInsensitiveOption };
    match = ukPart.match(event.m_title);
    auto match2 = ukPart.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_partnumber = match.captured(1).toUInt();
        event.m_parttotal  = match.captured(2).toUInt();

        LOG(VB_EIT, LOG_DEBUG, QString("Extracted Part %1/%2 from title (%3)")
            .arg(event.m_partnumber).arg(event.m_parttotal).arg(event.m_title));

        // Remove from the title
        event.m_title.remove(match.capturedStart(0),
                             match.capturedLength(0));
    }
    else if (match2.hasMatch())
    {
        event.m_partnumber = match2.captured(1).toUInt();
        event.m_parttotal  = match2.captured(2).toUInt();

        LOG(VB_EIT, LOG_DEBUG, QString("Extracted Part %1/%2 from description (%3) \"%4\"")
            .arg(event.m_partnumber).arg(event.m_parttotal)
            .arg(event.m_title, event.m_description));

        // Remove from the start of the description.
        // Otherwise it ends up in the subtitle.
        if (match2.capturedStart(0) == 0)
        {
            // Retain a single colon (subtitle separator) if we remove any
            QString sub = match2.captured(0).contains(":") ? ":" : "";
            event.m_description = event.m_description.replace(match2.captured(0), sub);
        }
    }

    static const QRegularExpression ukStarring { R"((?:Western\s)?[Ss]tarring ([\w\s\-']+?)[Aa]nd\s([\w\s\-']+?)[\.|,]\s*(\d{4})?(?:\.\s)?)" };
    match = ukStarring.match(event.m_description);
    if (match.hasMatch())
    {
        // if we match this we've captured 2 actors and an (optional) airdate
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
        event.AddPerson(DBPerson::kActor, match.captured(1));
        event.AddPerson(DBPerson::kActor, match.captured(2));
        if (match.captured(3).length() > 0)
        {
            bool ok = false;
            uint y = match.captured(3).toUInt(&ok);
            if (ok)
            {
                event.m_airdate = y;
                event.m_originalairdate = QDate(y, 1, 1);
            }
        }
    }

    static const QRegularExpression ukLaONoSplit { "^Law & Order: (?:Criminal Intent|LA|"
        "Special Victims Unit|Trial by Jury|UK|You the Jury)" };
    if (!event.m_title.startsWith("CSI:") && !event.m_title.startsWith("CD:") &&
        !event.m_title.contains(ukLaONoSplit) &&
        !event.m_title.startsWith("Mission: Impossible"))
    {
        static const QRegularExpression ukDoubleDotStart { R"(^\.\.+)" };
        static const QRegularExpression ukDoubleDotEnd   { R"(\.\.+$)" };
        if ((event.m_title.indexOf(ukDoubleDotEnd) != -1) &&
            (event.m_description.indexOf(ukDoubleDotStart) != -1))
        {
            QString strPart=event.m_title.remove(ukDoubleDotEnd)+" ";
            strFull = strPart + event.m_description.remove(ukDoubleDotStart);
            static const QRegularExpression ukCEPQ { R"([:\!\.\?]\s)" };
            static const QRegularExpression ukSpaceStart { "^ " };
            int position1 = strFull.indexOf(ukCEPQ,strPart.length());
            if (isMovie && (position1 != -1))
            {
                 if (strFull[position1] == '!' || strFull[position1] == '?'
                  || (position1>2 && strFull[position1] == '.' && strFull[position1-2] == '.'))
                     position1++;
                 event.m_title = strFull.left(position1);
                 event.m_description = strFull.mid(position1 + 1);
                 event.m_description.remove(ukSpaceStart);
            }
            else
            {
                position1 = strFull.indexOf(ukCEPQ);
                if (position1 != -1)
                {
                     if (strFull[position1] == '!' || strFull[position1] == '?'
                      || (position1>2 && strFull[position1] == '.' && strFull[position1-2] == '.'))
                         position1++;
                     event.m_title = strFull.left(position1);
                     event.m_description = strFull.mid(position1 + 1);
                     event.m_description.remove(ukSpaceStart);
                     SetUKSubtitle(event);
                }
            }
        }
        else if (event.m_description.indexOf(uk24ep) != -1)
        {
            auto match24 = uk24ep.match(event.m_description);
            if (match24.hasMatch())
            {
                // Special case for episodes of 24.
                // -2 from the length cause we don't want ": " on the end
                event.m_subtitle = event.m_description.mid(match24.capturedStart(0),
                                                           match24.captured(0).length() - 2);
                event.m_description = event.m_description.remove(match24.captured(0));
            }
        }
        else if (event.m_description.indexOf(ukTime) == -1)
        {
            static const QRegularExpression ukYearColon { R"(^[\d]{4}:)" };
            if (!isMovie && (event.m_title.indexOf(ukYearColon) < 0))
            {
                int position1 = event.m_title.indexOf(":");
                if ((position1 != -1) &&
                    (event.m_description.indexOf(":") < 0 ))
                {
                    static const QRegularExpression ukCompleteDots { R"(^\.\.+$)" };
                    if (event.m_title.mid(position1+1).indexOf(ukCompleteDots)==0)
                    {
                        SetUKSubtitle(event);
                        QString strTmp = event.m_title.mid(position1+1);
                        event.m_title.resize(position1);
                        event.m_subtitle = strTmp+event.m_subtitle;
                    }
                    else if ((uint)position1 < kSubtitleMaxLen)
                    {
                        event.m_subtitle = event.m_title.mid(position1 + 1);
                        event.m_title = event.m_title.left(position1);
                    }
                }
                else
                {
                    SetUKSubtitle(event);
                }
            }
        }
    }

    if (!isMovie && event.m_subtitle.isEmpty() &&
        !event.m_title.startsWith("The X-Files"))
    {
        int position1 = event.m_description.indexOf(ukTime);
        if (position1 != -1)
        {
            static const QRegularExpression ukColonPeriod { R"([:\.])" };
            int position2 = event.m_description.indexOf(ukColonPeriod);
            if ((position2>=0) && (position2 < (position1-2)))
                SetUKSubtitle(event);
        }
        else
        {
            position1 = event.m_title.indexOf("-");
            if (position1 != -1)
            {
                if ((uint)position1 < kSubtitleMaxLen)
                {
                    event.m_subtitle = event.m_title.mid(position1 + 1);
                    event.m_subtitle.remove(kUKSpaceColonStart);
                    event.m_title = event.m_title.left(position1);
                }
            }
            else
            {
                SetUKSubtitle(event);
            }
        }
    }

    // Work out the year (if any)
    static const QRegularExpression ukYear { R"([\[\(]([\d]{4})[\)\]])" };
    match = ukYear.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
        bool ok = false;
        uint y = match.captured(1).toUInt(&ok);
        if (ok)
        {
            event.m_airdate = y;
            event.m_originalairdate = QDate(y, 1, 1);
        }
    }

    // Trim leading/trailing '.'
    static const QRegularExpression ukDotSpaceStart { R"(^\. )" };
    static const QRegularExpression ukDotEnd { R"(\.$)" };
    event.m_subtitle.remove(ukDotSpaceStart);
    if (event.m_subtitle.lastIndexOf("..") != (event.m_subtitle.length()-2))
        event.m_subtitle.remove(ukDotEnd);

    // Reverse the subtitle and empty description
    if (event.m_description.isEmpty() && !event.m_subtitle.isEmpty())
    {
        event.m_description=event.m_subtitle;
        event.m_subtitle.clear();
    }
}

/**
 *  \brief Use this to standardize PBS ATSC guide in the USA.
 */
void EITFixUp::FixPBS(DBEventEIT &event)
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
void EITFixUp::FixComHem(DBEventEIT &event, bool process_subtitle)
{
    static const QRegularExpression comHemPersSeparator { R"((, |\soch\s))" };

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
    static const QRegularExpression comHemSeries1
        { R"(\s?(?:[dD]el|[eE]pisode)\s([0-9]+)(?:\s?(?:/|:|av)\s?([0-9]+))?\.)" };
    static const QRegularExpression  comHemSeries2 { R"(\s?-?\s?([Dd]el\s+([0-9]+)))" };
    auto match = comHemSeries1.match(event.m_description);
    auto match2 = comHemSeries2.match(event.m_title);
    if (match2.hasMatch())
    {
        event.m_partnumber = match2.capturedView(2).toUInt();
        event.m_title.remove(match2.capturedStart(), match2.capturedLength());
    }
    else if (match.hasMatch())
    {
        if (match.capturedStart(1) != -1)
            event.m_partnumber = match.capturedView(1).toUInt();
        if (match.capturedStart(2) != -1)
            event.m_parttotal = match.capturedView(2).toUInt();

        // Remove the episode numbers, but only if it's not at the begining
        // of the description (subtitle code might use it)
        if (match.capturedStart() > 0)
            event.m_description.remove(match.capturedStart(),
                                       match.capturedLength());
        isSeries = true;
    }

    // Add partnumber/parttotal to subtitle
    // This will be overwritten if we find a better subtitle
    if (event.m_partnumber > 0)
    {
        event.m_subtitle = QString("Del %1").arg(event.m_partnumber);
        if (event.m_parttotal > 0)
            event.m_subtitle += QString(" av %1").arg(event.m_parttotal);
    }

    // Move subtitle info from title to subtitle
    static const QRegularExpression comHemTSub { R"(\s+-\s+([^\-]+))" };
    match = comHemTSub.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_subtitle = match.captured(1);
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    // No need to continue without a description.
    if (event.m_description.length() <= 0)
        return;

    // Try to find country category, year and possibly other information
    // from the begining of the description
    static const QRegularExpression comHemCountry
        { R"(^(\(.+\))?\s?([^ ]+)\s([^\.0-9]+)\sfrån\s([0-9]{4})(?:\smed\s([^\.]+))?\.?)" };
    match = comHemCountry.match(event.m_description);
    if (match.hasMatch())
    {
        QString replacement;

        // Original title, usually english title
        // note: list[1] contains extra () around the text that needs removing
        if (!match.capturedView(1).isEmpty())
        {
            replacement = match.captured(1) + " ";
            //store it somewhere?
        }

        // Countr(y|ies)
        if (!match.capturedView(2).isEmpty())
        {
            replacement += match.captured(2) + " ";
            //store it somewhere?
        }

        // Category
        if (!match.capturedView(3).isEmpty())
        {
            replacement += match.captured(3) + ".";
            if(event.m_category.isEmpty())
            {
                event.m_category = match.captured(3);
            }

            if(match.captured(3).indexOf("serie")!=-1)
            {
                isSeries = true;
            }
        }

        // Year
        if (!match.capturedView(4).isEmpty())
        {
            bool ok = false;
            uint y = match.capturedView(4).trimmed().toUInt(&ok);
            if (ok)
                event.m_airdate = y;
        }

        // Actors
        if (!match.capturedView(5).isEmpty())
        {
            const QStringList actors =
                match.captured(5).split(comHemPersSeparator, Qt::SkipEmptyParts);
            /* Possible TODO: if EIT inlcude the priority and/or character
             * names for the actors, include them in AddPerson call. */
            for (const auto & actor : std::as_const(actors))
                event.AddPerson(DBPerson::kActor, actor);
        }

        // Remove year and actors.
        // The reason category is left in the description is because otherwise
        // the country would look wierd like "Amerikansk. Rest of description."
        event.m_description = event.m_description.replace(match.captured(0),replacement);
    }

    if (isSeries)
        event.m_categoryType = ProgramInfo::kCategorySeries;

    // Look for additional persons in the description
    static const QRegularExpression comHemPersons
        { R"(\s?([Rr]egi|[Ss]kådespelare|[Pp]rogramledare|[Ii] rollerna):\s([^\.]+)\.)" };
    auto iter  = comHemPersons.globalMatch(event.m_description);
    while (iter.hasNext())
    {
        auto pmatch = iter.next();
        DBPerson::Role role = DBPerson::kUnknown;

        static const QRegularExpression comHemDirector { "[Rr]egi" };
        static const QRegularExpression comHemActor    { "[Ss]kådespelare|[Ii] rollerna" };
        static const QRegularExpression comHemHost     { "[Pp]rogramledare" };
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        auto dmatch = comHemDirector.match(pmatch.capturedView(1));
        auto amatch = comHemActor.match(pmatch.capturedView(1));
        auto hmatch = comHemHost.match(pmatch.capturedView(1));
#else
        auto dmatch = comHemDirector.matchView(pmatch.capturedView(1));
        auto amatch = comHemActor.matchView(pmatch.capturedView(1));
        auto hmatch = comHemHost.matchView(pmatch.capturedView(1));
#endif
        if (dmatch.hasMatch())
            role = DBPerson::kDirector;
        else if (amatch.hasMatch())
            role = DBPerson::kActor;
        else if (hmatch.hasMatch())
            role = DBPerson::kHost;
        else
        {
            event.m_description.remove(pmatch.capturedStart(), pmatch.capturedLength());
            continue;
        }

        const QStringList actors =
            pmatch.captured(2).split(comHemPersSeparator, Qt::SkipEmptyParts);
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
        for (const auto & actor : std::as_const(actors))
            event.AddPerson(role, actor);

        // Remove it
        event.m_description=event.m_description.replace(pmatch.captured(0),"");
    }

    // Is this event on a channel we shoud look for a subtitle?
    // The subtitle is the first sentence in the description, but the
    // subtitle can't be the only thing in the description and it must be
    // shorter than 55 characters or we risk picking up the wrong thing.
    if (process_subtitle)
    {
        static const QRegularExpression comHemSub { R"([.\?\!] )" };
        int pos2 = event.m_description.indexOf(comHemSub);
        bool pvalid = pos2 != -1 && pos2 <= 55;
        if (pvalid && (event.m_description.length() - (pos2 + 2)) > 0)
        {
            event.m_subtitle = event.m_description.left(
                pos2 + (event.m_description[pos2] == '?' ? 1 : 0));
            event.m_description = event.m_description.mid(pos2 + 2);
        }
    }

    // Teletext subtitles?
    static const QRegularExpression comHemTT { "[Tt]ext-[Tt][Vv]" };
    if (event.m_description.indexOf(comHemTT) != -1)
        event.m_subtitleType |= SUB_NORMAL;

    // Try to findout if this is a rerun and if so the date.
    static const QRegularExpression comHemRerun1 { R"([Rr]epris\sfrån\s([^\.]+)(?:\.|$))" };
    static const QRegularExpression comHemRerun2 { R"(([0-9]+)/([0-9]+)(?:\s-\s([0-9]{4}))?)" };
    match = comHemRerun1.match(event.m_description);
    if (!match.hasMatch())
        return;

    // Rerun from today
    if (match.captured(1) == "i dag")
    {
        event.m_originalairdate = event.m_starttime.date();
        return;
    }

    // Rerun from yesterday afternoon
    if (match.captured(1) == "eftermiddagen")
    {
        event.m_originalairdate = event.m_starttime.date().addDays(-1);
        return;
    }

    // Rerun with day, month and possibly year specified
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    match2 = comHemRerun2.match(match.capturedView(1));
#else
    match2 = comHemRerun2.matchView(match.capturedView(1));
#endif
    if (match2.hasMatch())
    {
        int day   = match2.capturedView(1).toInt();
        int month = match2.capturedView(2).toInt();
        //int year;
        //if (match2.capturedLength(3) > 0)
        //    year = match2.capturedView(3).toInt();
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

/**
 *  \brief Use this to standardize DVB-S guide in Australia.
 */
void EITFixUp::FixAUStar(DBEventEIT &event)
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

/**
 *  \brief Use this to standardize DVB-T guide in Australia. (fix common annoyances common to most networks)
 */
void EITFixUp::FixAUDescription(DBEventEIT &event)
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

/**
 *  \brief Use this to standardize DVB-T guide in Australia. (Nine network)
 */
void EITFixUp::FixAUNine(DBEventEIT &event)
{
    static const QRegularExpression rating { "\\((G|PG|M|MA)\\)" };
    auto match = rating.match(event.m_description);
    if (match.hasMatch())
    {
      EventRating prograting;
      prograting.m_system="AU"; prograting.m_rating = match.captured(1);
      event.m_ratings.push_back(prograting);
      event.m_description.remove(0,match.capturedLength()+1);
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

/**
 *  \brief Use this to standardize DVB-T guide in Australia. (Seven network)
 */
void EITFixUp::FixAUSeven(DBEventEIT &event)
{
    if (event.m_description.endsWith(" Rpt"))
    {
        event.m_previouslyshown = true;
        event.m_description.resize(event.m_description.size()-4);
    }
    static const QRegularExpression year { "(\\d{4})$" };
    auto match = year.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_airdate = match.capturedView(1).toUInt();
        event.m_description.resize(event.m_description.size()-5);
    }
    if (event.m_description.endsWith(" CC"))
    {
      event.m_subtitleType |= SUB_NORMAL;
      event.m_description.resize(event.m_description.size()-3);
    }
    QString advisories;//store the advisories to append later
    static const QRegularExpression adv { "(\\([A-Z,]+\\))$" };
    match = adv.match(event.m_description);
    if (match.hasMatch())
    {
        advisories = match.captured(1);
        event.m_description.remove(match.capturedStart()-1, match.capturedLength()+1);
    }
    static const QRegularExpression rating { "(C|G|PG|M|MA)$" };
    match = rating.match(event.m_description);
    if (match.hasMatch())
    {
        EventRating prograting;
        prograting.m_system="AU"; prograting.m_rating = match.captured(1);
        if (!advisories.isEmpty())
            prograting.m_rating.append(" ").append(advisories);
        event.m_ratings.push_back(prograting);
        event.m_description.remove(match.capturedStart()-1, match.capturedLength()+1);
    }
}
/**
 *  \brief Use this to standardize DVB-T guide in Australia. (generic freeview - extra info in brackets at end of desc)
 */
void EITFixUp::FixAUFreeview(DBEventEIT &event)
{
    // If the description has been truncated to fit within the
    // 'subtitle' eit field, none of the following will work (ABC)
    if (event.m_description.endsWith(".."))
        return;
    event.m_description = event.m_description.trimmed();

    static const QRegularExpression auFreeviewSY { R"((.*) \((.+)\) \(([12][0-9][0-9][0-9])\)$)" };
    auto match = auFreeviewSY.match(event.m_description);
    if (match.hasMatch())
    {
        if (event.m_subtitle.isEmpty())//nine sometimes has an actual subtitle field and the brackets thingo)
            event.m_subtitle = match.captured(2);
        event.m_airdate = match.capturedView(3).toUInt();
        event.m_description = match.captured(1);
        return;
    }
    static const QRegularExpression auFreeviewY { "(.*) \\(([12][0-9][0-9][0-9])\\)$" };
    match = auFreeviewY.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_airdate = match.capturedView(2).toUInt();
        event.m_description = match.captured(1);
        return;
    }
    static const QRegularExpression auFreeviewSYC { R"((.*) \((.+)\) \(([12][0-9][0-9][0-9])\) \((.+)\)$)" };
    match = auFreeviewSYC.match(event.m_description);
    if (match.hasMatch())
    {
        if (event.m_subtitle.isEmpty())
            event.m_subtitle = match.captured(2);
        event.m_airdate = match.capturedView(3).toUInt();
        QStringList actors = match.captured(4).split("/");
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
        for (const QString& actor : std::as_const(actors))
            event.AddPerson(DBPerson::kActor, actor);
        event.m_description = match.captured(1);
        return;
    }
    static const QRegularExpression auFreeviewYC { R"((.*) \(([12][0-9][0-9][0-9])\) \((.+)\)$)" };
    match = auFreeviewYC.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_airdate = match.capturedView(2).toUInt();
        QStringList actors = match.captured(3).split("/");
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
        for (const QString& actor : std::as_const(actors))
            event.AddPerson(DBPerson::kActor, actor);
        event.m_description = match.captured(1);
    }
}

/**
 *  \brief Use this to standardise the MultiChoice Africa DVB-S guide.
 */
void EITFixUp::FixMCA(DBEventEIT &event)
{
    const uint SUBTITLE_PCT     = 60; // % of description to allow subtitle to
    const uint lSUBTITLE_MAX_LEN = 128;// max length of subtitle field in db.

    // Remove subtitle, it contains category information too specific to use
    event.m_subtitle = QString("");

    // No need to continue without a description.
    if (event.m_description.length() <= 0)
        return;

    // Replace incomplete title if the full one is in the description
    static const QRegularExpression mcaIncompleteTitle { R"((.*).\.\.\.$)" };
    auto match = mcaIncompleteTitle.match(event.m_title);
    if (match.hasMatch())
    {
        static const QString mcaCompleteTitlea { "^'?(" };
        static const QString mcaCompleteTitleb { R"([^\.\?]+[^\'])'?[\.\?]\s+(.+))" };
        static const QRegularExpression mcaCompleteTitle
            { mcaCompleteTitlea + match.captured(1) + mcaCompleteTitleb,
              QRegularExpression::CaseInsensitiveOption};
        match = mcaCompleteTitle.match(event.m_description);
        if (match.hasMatch())
        {
            event.m_title       = match.captured(1).trimmed();
            event.m_description = match.captured(2).trimmed();
        }
    }

    // Try to find subtitle in description
    static const QRegularExpression mcaSubtitle { R"(^'([^\.]+)'\.\s+(.+))" };
    match = mcaSubtitle.match(event.m_description);
    if (match.hasMatch())
    {
        uint matchLen = match.capturedLength(1);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        uint evDescLen = std::max(event.m_description.length(), 1);
#else
        uint evDescLen = std::max(event.m_description.length(), 1LL);
#endif

        if ((matchLen < lSUBTITLE_MAX_LEN) &&
            ((matchLen * 100 / evDescLen) < SUBTITLE_PCT))
        {
            event.m_subtitle    = match.captured(1);
            event.m_description = match.captured(2);
        }
    }

    // Try to find episode numbers in subtitle
    static const QRegularExpression mcaSeries { R"(^S?(\d+)\/E?(\d+)\s-\s(.*)$)" };
    match = mcaSeries.match(event.m_subtitle);
    if (match.hasMatch())
    {
        uint season      = match.capturedView(1).toUInt();
        uint episode     = match.capturedView(2).toUInt();
        event.m_subtitle = match.captured(3).trimmed();
        event.m_syndicatedepisodenumber =
                QString("S%1E%2").arg(season).arg(episode);
        event.m_season = season;
        event.m_episode = episode;
        event.m_categoryType = ProgramInfo::kCategorySeries;
    }

    // Closed captioned?
    static const QRegularExpression mcaCC { R"(,?\s(HI|English) Subtitles\.?)" };
    int position = event.m_description.indexOf(mcaCC);
    if (position > 0)
    {
        event.m_subtitleType |= SUB_HARDHEAR;
        event.m_description.remove(mcaCC);
    }

    // Dolby Digital 5.1?
    static const QRegularExpression mcaDD { R"(,?\sDD\.?)" };
    position = event.m_description.indexOf(mcaDD);
    if ((position > 0) && (position > event.m_description.length() - 7))
    {
        event.m_audioProps |= AUD_DOLBY;
        event.m_description.remove(mcaDD);
    }

    // Remove bouquet tags
    static const QRegularExpression mcaAvail { R"(\s(Only available on [^\.]*bouquet|Not available in RSA [^\.]*)\.?)" };
    event.m_description.remove(mcaAvail);

    // Try to find year and director from the end of the description
    bool isMovie = false;
    static const QRegularExpression mcaCredits { R"((.*)\s\((\d{4})\)\s*([^\.]+)\.?\s*$)" };
    match = mcaCredits.match(event.m_description);
    if (match.hasMatch())
    {
        isMovie = true;
        event.m_description = match.captured(1).trimmed();
        bool ok = false;
        uint y = match.captured(2).trimmed().toUInt(&ok);
        if (ok)
            event.m_airdate = y;
        event.AddPerson(DBPerson::kDirector, match.captured(3).trimmed());
    }
    else
    {
        // Try to find year only from the end of the description
        static const QRegularExpression mcaYear { R"((.*)\s\((\d{4})\)\s*$)" };
        match = mcaYear.match(event.m_description);
        if (match.hasMatch())
        {
            isMovie = true;
            event.m_description = match.captured(1).trimmed();
            bool ok = false;
            uint y = match.captured(2).trimmed().toUInt(&ok);
            if (ok)
                event.m_airdate = y;
        }
    }

    if (isMovie)
    {
        static const QRegularExpression mcaActors { R"((.*\.)\s+([^\.]+\s[A-Z][^\.]+)\.\s*)" };
        match = mcaActors.match(event.m_description);
        if (match.hasMatch())
        {
            static const QRegularExpression mcaActorsSeparator { "(,\\s+)" };
            const QStringList actors = match.captured(2).split(
                mcaActorsSeparator, Qt::SkipEmptyParts);
            /* Possible TODO: if EIT inlcude the priority and/or character
             * names for the actors, include them in AddPerson call. */
            for (const auto & actor : std::as_const(actors))
                event.AddPerson(DBPerson::kActor, actor.trimmed());
            event.m_description = match.captured(1).trimmed();
        }
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    }
}

/**
 *  \brief Use this to standardise the RTL group guide in Germany.
 */
void EITFixUp::FixRTL(DBEventEIT &event)
{
    // subtitle with episode number: "Folge *: 'subtitle'
    static const QRegularExpression superRTLSubtitle { R"(^Folge\s(\d{1,3}):\s'(.*)')" };
    auto match = superRTLSubtitle.match(event.m_subtitle);
    if (match.hasMatch())
    {
        event.m_season = 0;
        event.m_episode  = match.capturedView(1).toUInt();
        event.m_subtitle = match.captured(2);
    }

    // No need to continue without a description or with an subtitle.
    if (event.m_description.length() <= 0 || event.m_subtitle.length() > 0)
        return;

    // Repeat
    static const QRegularExpression rtlRepeat
        { R"([\s\(]?Wiederholung.+vo[m|n].+(\d{2}\.\d{2}\.\d{4}|\d{2}[:\.]\d{2}\sUhr)\)?)" };
    match = rtlRepeat.match(event.m_description);
    if (match.hasMatch())
    {
        // remove '.' if it matches at the beginning of the description
        int pos = match.capturedStart(0);
        int length = match.capturedLength(0) + (pos ? 0 : 1);
        event.m_description = event.m_description.remove(pos, length).trimmed();
    }

    // should be (?:\x{8a}|\\.\\s*|$) but 0x8A gets replaced with 0x20
    static const QRegularExpression rtlSubtitle1  { R"(^Folge\s(\d{1,4})\s*:\s+'(.*)'(?:\s|\.\s*|$))" };
    static const QRegularExpression rtlSubtitle2  { R"(^Folge\s(\d{1,4})\s+(.{0,5}[^\?!\.]{0,120})[\?!\.]\s*)" };
    static const QRegularExpression rtlSubtitle3  { R"(^(?:Folge\s)?(\d{1,4}(?:\/[IVX]+)?)\s+(.{0,5}[^\?!\.]{0,120})[\?!\.]\s*)" };
    static const QRegularExpression rtlSubtitle4  { R"(^Thema.{0,5}:\s([^\.]+)\.\s*)" };
    static const QRegularExpression rtlSubtitle5  { "^'(.+)'\\.\\s*" };
    static const QRegularExpression rtlEpisodeNo1 { R"(^(Folge\s\d{1,4})\.*\s*)" };
    static const QRegularExpression rtlEpisodeNo2 { R"(^(\d{1,2}\/[IVX]+)\.*\s*)" };

    auto match1 = rtlSubtitle1.match(event.m_description);
    auto match2 = rtlSubtitle2.match(event.m_description);
    auto match3 = rtlSubtitle3.match(event.m_description);
    auto match4 = rtlSubtitle4.match(event.m_description);
    auto match5 = rtlSubtitle5.match(event.m_description);
    auto match6 = rtlEpisodeNo1.match(event.m_description);
    auto match7 = rtlEpisodeNo2.match(event.m_description);

    // subtitle with episode number: "Folge *: 'subtitle'. description
    if (match1.hasMatch())
    {
        event.m_syndicatedepisodenumber = match1.captured(1);
        event.m_subtitle = match1.captured(2);
        event.m_description =
            event.m_description.remove(0, match1.capturedLength());
    }
    // episode number subtitle
    else if (match2.hasMatch())
    {
        event.m_syndicatedepisodenumber = match2.captured(1);
        event.m_subtitle = match2.captured(2);
        event.m_description =
            event.m_description.remove(0, match2.capturedLength());
    }
    // episode number subtitle
    else if (match3.hasMatch())
    {
        event.m_syndicatedepisodenumber = match3.captured(1);
        event.m_subtitle = match3.captured(2);
        event.m_description =
            event.m_description.remove(0, match3.capturedLength());
    }
    // "Thema..."
    else if (match4.hasMatch())
    {
        event.m_subtitle = match4.captured(1);
        event.m_description =
            event.m_description.remove(0, match4.capturedLength());
    }
    // "'...'"
    else if (match5.hasMatch())
    {
        event.m_subtitle = match5.captured(1);
        event.m_description =
            event.m_description.remove(0, match5.capturedLength());
    }
    // episode number
    else if (match6.hasMatch())
    {
        event.m_syndicatedepisodenumber = match6.captured(2);
        event.m_subtitle = match6.captured(1);
        event.m_description =
            event.m_description.remove(0, match6.capturedLength());
    }
    // episode number
    else if (match7.hasMatch())
    {
        event.m_syndicatedepisodenumber = match7.captured(2);
        event.m_subtitle = match7.captured(1);
        event.m_description =
            event.m_description.remove(0, match7.capturedLength());
    }

    /* got an episode title now? (we did not have one at the start of this function) */
    if (!event.m_subtitle.isEmpty())
        event.m_categoryType = ProgramInfo::kCategorySeries;

    /* if we do not have an episode title by now try some guessing as last resort */
    if (event.m_subtitle.length() == 0)
    {
        const uint SUBTITLE_PCT = 35; // % of description to allow subtitle up to
        const uint lSUBTITLE_MAX_LEN = 50; // max length of subtitle field in db

        static const QRegularExpression rtlSubtitle { R"(^([^\.]{3,})\.\s+(.+))" };
        match = rtlSubtitle.match(event.m_description);
        if (match.hasMatch())
        {
            uint matchLen = match.capturedLength(1);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            uint evDescLen = std::max(event.m_description.length(), 1);
#else
            uint evDescLen = std::max(event.m_description.length(), 1LL);
#endif

            if ((matchLen < lSUBTITLE_MAX_LEN) &&
                (matchLen * 100 / evDescLen < SUBTITLE_PCT))
            {
                event.m_subtitle    = match.captured(1);
                event.m_description = match.captured(2);
            }
        }
    }
}

// FIXME add more jobs
static const QMap<QString,DBPerson::Role> deCrewTitle {
    { "Regie",    DBPerson::kDirector },
    { "Drehbuch", DBPerson::kWriter },
    { "Autor",    DBPerson::kWriter },
};

/**
 *  \brief Use this to standardise the PRO7/Sat1 group guide in Germany.
 */
void EITFixUp::FixPRO7(DBEventEIT &event)
{
    // strip repeat info and set previouslyshown flag
    static const QRegularExpression pro7Repeat
        { R"((?<=\s|^)\(WH vom \w+, \d{2}\.\d{2}\.\d{4}, \d{2}:\d{2} Uhr\)$)" };
    auto match = pro7Repeat.match(event.m_subtitle);
    if (match.hasMatch())
    {
        event.m_previouslyshown = true;
        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
        event.m_subtitle = event.m_subtitle.trimmed();
    }

    // strip "Mit Gebärdensprache (Online-Stream)"
    static const QRegularExpression pro7signLanguage
        { R"((?<=\s|^)Mit Gebärdensprache \(Online\-Stream\)$)" };
    match = pro7signLanguage.match(event.m_subtitle);
    if (match.hasMatch())
    {
        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
        event.m_subtitle = event.m_subtitle.trimmed();
    }

    // move age ratings into metadata
    static const QRegularExpression pro7ratingAllAges
        { R"((?<=\s|^)Altersfreigabe: Ohne Altersbeschränkung$)" };
    match = pro7ratingAllAges.match(event.m_subtitle);
    if (match.hasMatch())
    {
        EventRating prograting;
        prograting.m_system="DE";
        prograting.m_rating = "0";
        event.m_ratings.push_back(prograting);

        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
        event.m_subtitle = event.m_subtitle.trimmed();
    }
    static const QRegularExpression pro7rating
        { R"((?<=\s|^)Altersfreigabe: ab (\d+)$)" };
    match = pro7rating.match(event.m_subtitle);
    if (match.hasMatch())
    {
        EventRating prograting;
        prograting.m_system="DE";
        prograting.m_rating = match.captured(1);
        event.m_ratings.push_back(prograting);

        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
        event.m_subtitle = event.m_subtitle.trimmed();
    }

    // move category and (original) airdate into metadata, add country and airdate to description
    static const QRegularExpression pro7CategoryOriginalairdate
        { R"((?<=\s|^)(Late Night Show|Live Shopping|Real Crime|Real Life Doku|Romantic Comedy|Scripted Reality|\S+), ([A-Z]+(?:\/[A-Z]+)*) (\d{4})$)" };
    match = pro7CategoryOriginalairdate.match(event.m_subtitle);
    if (match.hasMatch())
    {
        event.m_category = match.captured(1);

        event.m_description.append(" (").append(match.captured(2)).append(" ").append(match.captured(3)).append(")");

        uint y = match.captured(3).toUInt();
        event.m_originalairdate = QDate(y, 1, 1);
        if (event.m_airdate == 0)
        {
            event.m_airdate = y;
        }

        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
        event.m_subtitle = event.m_subtitle.trimmed();
    }

    // remove subtitle if equal to title
    if (event.m_title == event.m_subtitle) {
        event.m_subtitle = "";
    }
}

/**
*  \brief Use this to standardise the Disney Channel guide in Germany.
*/
void EITFixUp::FixDisneyChannel(DBEventEIT &event)
{
    static const QRegularExpression deDisneyChannelSubtitle { R"(,([^,]+?)\s{0,1}(\d{4})$)" };
    auto match = deDisneyChannelSubtitle.match(event.m_subtitle);
    if (match.hasMatch())
    {
        if (event.m_airdate == 0)
        {
            event.m_airdate = match.captured(3).toUInt();
        }
        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
    }
    static const QRegularExpression tmp { R"(\s[^\s]+?-(Serie))" };
    match = tmp.match(event.m_subtitle);
    if (match.hasMatch())
    {
        event.m_categoryType = ProgramInfo::kCategorySeries;
        event.m_category=match.captured(0).trimmed();
        event.m_subtitle.remove(match.capturedStart(0),
                                match.capturedLength(0));
    }
}

/**
*  \brief Use this to standardise the ATV/ATV2 guide in Germany.
**/
void EITFixUp::FixATV(DBEventEIT &event)
{
    static const QRegularExpression atvSubtitle { R"(,{0,1}\sFolge\s(\d{1,3})$)" };
    event.m_subtitle.replace(atvSubtitle, "");
}


/**
 *  \brief Use this to clean DVB-T guide in Finland.
 */
void EITFixUp::FixFI(DBEventEIT &event)
{
    static const QRegularExpression fiRerun { R"(\s?Uusinta[a-zA-Z\s]*\.?)" };
    auto match = fiRerun.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = true;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    static const QRegularExpression fiRerun2 { R"(\([Uu]\))" };
    match = fiRerun2.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_previouslyshown = true;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    match = kStereo.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_audioProps |= AUD_STEREO;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove age limit in parenthesis at end of title
    static const QRegularExpression fiAgeLimit { R"(\((\d{1,2}|[ST])\)$)" };
    match = fiAgeLimit.match(event.m_title);
    if (match.hasMatch())
    {
        EventRating prograting;
        prograting.m_system="FI"; prograting.m_rating = match.captured(1);
        event.m_ratings.push_back(prograting);
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    // Remove Film or Elokuva at start of title
    static const QRegularExpression fiFilm { "^(Film|Elokuva): " };
    match = fiFilm.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_category = "Film";
        event.m_categoryType = ProgramInfo::kCategoryMovie;
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }
}

/**
 *  \brief Use this to standardize DVB-C guide in Germany
 *         for the providers Kabel Deutschland and Premiere.
 */
void EITFixUp::FixPremiere(DBEventEIT &event)
{
    QString country = "";

    static const QRegularExpression dePremiereLength  { R"(\s?[0-9]+\sMin\.)" };
    event.m_description = event.m_description.replace(dePremiereLength, "");

    static const QRegularExpression dePremiereAirdate { R"(\s?([^\s^\.]+)\s((?:1|2)[0-9]{3})\.)" };
    auto match = dePremiereAirdate.match(event.m_description);
    if ( match.hasMatch())
    {
        country = match.captured(1).trimmed();
        bool ok = false;
        uint y = match.captured(2).toUInt(&ok);
        if (ok)
            event.m_airdate = y;
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
    }

    static const QRegularExpression dePremiereCredits { R"(\sVon\s([^,]+)(?:,|\su\.\sa\.)\smit\s([^\.]*)\.)" };
    match = dePremiereCredits.match(event.m_description);
    if (match.hasMatch())
    {
        event.AddPerson(DBPerson::kDirector, match.captured(1));
        const QStringList actors = match.captured(2).split(
            ", ", Qt::SkipEmptyParts);
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
        for (const auto & actor : std::as_const(actors))
            event.AddPerson(DBPerson::kActor, actor);
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
    }

    event.m_description = event.m_description.replace("\u000A$", "");
    event.m_description = event.m_description.replace("\u000A", " ");

    // move the original titel from the title to subtitle
    static const QRegularExpression dePremiereOTitle  { R"(\s*\(([^\)]*)\)$)" };
    match = dePremiereOTitle.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_subtitle = QString("%1, %2").arg(match.captured(1), country);
        event.m_title.remove(match.capturedStart(0),
                             match.capturedLength(0));
    }

    // Find infos about season and episode number
    static const QRegularExpression deSkyDescriptionSeasonEpisode { R"(^(\d{1,2}).\sStaffel,\sFolge\s(\d{1,2}):\s)" };
    match = deSkyDescriptionSeasonEpisode.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_season = match.captured(1).trimmed().toUInt();
        event.m_episode = match.captured(2).trimmed().toUInt();
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
    }
}

/*
 * Mapping table from English category names to Dutch names and types
 */
struct NLMapResult {
    QString name;
    ProgramInfo::CategoryType type {ProgramInfo::kCategoryNone};
};
static const QMap<QString, NLMapResult> categoryTrans = {
    { "Documentary",                { "Documentaire",         ProgramInfo::kCategoryNone   } },
    { "News",                       { "Nieuws/actualiteiten", ProgramInfo::kCategoryNone   } },
    { "Kids",                       { "Jeugd",                ProgramInfo::kCategoryNone   } },
    { "Show/game Show",             { "Amusement",            ProgramInfo::kCategoryTVShow } },
    { "Music/Ballet/Dance",         { "Muziek",               ProgramInfo::kCategoryNone   } },
    { "News magazine",              { "Informatief",          ProgramInfo::kCategoryNone   } },
    { "Movie",                      { "Film",                 ProgramInfo::kCategoryMovie  } },
    { "Nature/animals/Environment", { "Natuur",               ProgramInfo::kCategoryNone   } },
    { "Movie - Adult",              { "Erotiek",              ProgramInfo::kCategoryNone   } },
    { "Movie - Soap/melodrama/folkloric",
                                    { "Serie/soap",           ProgramInfo::kCategorySeries } },
    { "Arts/Culture",               { "Kunst/Cultuur",        ProgramInfo::kCategoryNone   } },
    { "Sports",                     { "Sport",                ProgramInfo::kCategorySports } },
    { "Cartoons/Puppets",           { "Animatie",             ProgramInfo::kCategoryNone   } },
    { "Movie - Comedy",             { "Comedy",               ProgramInfo::kCategorySeries } },
    { "Movie - Detective/Thriller", { "Misdaad",              ProgramInfo::kCategoryNone   } },
    { "Social/Spiritual Sciences",  { "Religieus",            ProgramInfo::kCategoryNone   } },
};

/**
 *  \brief Use this to standardize \@Home DVB-C guide in the Netherlands.
 */
void EITFixUp::FixNL(DBEventEIT &event)
{
    QString fullinfo = event.m_subtitle + event.m_description;
    event.m_subtitle = "";

    // Convert categories to Dutch categories Myth knows.
    // nog invoegen: comedy, sport, misdaad

    if (categoryTrans.contains(event.m_category))
    {
        auto [name, type]    = categoryTrans[event.m_category];
        event.m_category     = name;
        event.m_categoryType = type;
    }

    // Film - categories are usually not Films
    if (event.m_category.startsWith("Film -"))
        event.m_categoryType = ProgramInfo::kCategorySeries;

    // Get stereo info
    auto match = kStereo.match(fullinfo);
    if (match.hasMatch())
    {
        event.m_audioProps |= AUD_STEREO;
        fullinfo.remove(match.capturedStart(), match.capturedLength());
    }

    //Get widescreen info
    static const QRegularExpression nlWide { "breedbeeld" };
    match = nlWide.match(fullinfo);
    if (match.hasMatch())
    {
        event.m_videoProps |= VID_WIDESCREEN;
        fullinfo = fullinfo.replace("breedbeeld", ".");
    }

    // Get repeat info
    static const QRegularExpression nlRepeat { "herh." };
    match = nlRepeat.match(fullinfo);
    if (match.hasMatch())
        fullinfo = fullinfo.replace("herh.", ".");

    // Get teletext subtitle info
    static const QRegularExpression nlTxt { "txt" };
    match = nlTxt.match(fullinfo);
    if (match.hasMatch())
    {
        event.m_subtitleType |= SUB_NORMAL;
        fullinfo = fullinfo.replace("txt", ".");
    }

    // Get HDTV information
    static const QRegularExpression nlHD { R"(\sHD$)" };
    match = nlHD.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_videoProps |= VID_HDTV;
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    // Try to make subtitle from Afl.:
    static const QRegularExpression nlSub { R"(\sAfl\.:\s([^\.]+)\.)" };
    match = nlSub.match(fullinfo);
    if (match.hasMatch())
    {
        QString tmpSubString = match.captured(0);
        tmpSubString = tmpSubString.right(match.capturedLength() - 7);
        event.m_subtitle = tmpSubString.left(tmpSubString.length() -1);
        fullinfo.remove(match.capturedStart(), match.capturedLength());
    }

    // Try to make subtitle from " "
    static const QRegularExpression nlSub2 { R"(\s\"([^\"]+)\")" };
    match = nlSub2.match(fullinfo);
    if (match.hasMatch())
    {
        QString tmpSubString = match.captured(0);
        tmpSubString = tmpSubString.right(match.capturedLength() - 2);
        event.m_subtitle = tmpSubString.left(tmpSubString.length() -1);
        fullinfo.remove(match.capturedStart(), match.capturedLength());
    }


    // This is trying to catch the case where the subtitle is in the main title
    // but avoid cases where it isn't a subtitle e.g cd:uk
    int position = event.m_title.indexOf(":");
    if ((position != -1) &&
        (event.m_title[position + 1].toUpper() == event.m_title[position + 1]) &&
        (event.m_subtitle.isEmpty()))
    {
        event.m_subtitle = event.m_title.mid(position + 1);
        event.m_title = event.m_title.left(position);
    }


    // Get the actors
    static const QRegularExpression nlActors { R"(\sMet:\s.+e\.a\.)" };
    static const QRegularExpression nlPersSeparator { R"((, |\sen\s))" };
    match = nlActors.match(fullinfo);
    if (match.hasMatch())
    {
        QString tmpActorsString = match.captured(0);
        tmpActorsString = tmpActorsString.right(tmpActorsString.length() - 6);
        tmpActorsString = tmpActorsString.left(tmpActorsString.length() - 5);
        const QStringList actors =
            tmpActorsString.split(nlPersSeparator, Qt::SkipEmptyParts);
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
        for (const auto & actor : std::as_const(actors))
            event.AddPerson(DBPerson::kActor, actor);
        fullinfo.remove(match.capturedStart(), match.capturedLength());
    }

    // Try to find presenter
    static const QRegularExpression nlPres { R"(\sPresentatie:\s([^\.]+)\.)" };
    match = nlPres.match(fullinfo);
    if (match.hasMatch())
    {
        QString tmpPresString = match.captured(0);
        tmpPresString = tmpPresString.right(tmpPresString.length() - 14);
        tmpPresString = tmpPresString.left(tmpPresString.length() -1);
        const QStringList presenters =
            tmpPresString.split(nlPersSeparator, Qt::SkipEmptyParts);
        for (const auto & presenter : std::as_const(presenters))
            event.AddPerson(DBPerson::kPresenter, presenter);
        fullinfo.remove(match.capturedStart(), match.capturedLength());
    }

    // Try to find year
    static const QRegularExpression nlYear1 { R"(\suit\s([1-2][0-9]{3}))" };
    static const QRegularExpression nlYear2 { R"((\s\([A-Z]{0,3}/?)([1-2][0-9]{3})\))",
        QRegularExpression::CaseInsensitiveOption };
    match = nlYear1.match(fullinfo);
    if (match.hasMatch())
    {
        bool ok = false;
        uint y = match.capturedView(1).toUInt(&ok);
        if (ok)
            event.m_originalairdate = QDate(y, 1, 1);
    }

    match = nlYear2.match(fullinfo);
    if (match.hasMatch())
    {
        bool ok = false;
        uint y = match.capturedView(2).toUInt(&ok);
        if (ok)
            event.m_originalairdate = QDate(y, 1, 1);
    }

    // Try to find director
    static const QRegularExpression nlDirector { R"(\svan\s(([A-Z][a-z]+\s)|([A-Z]\.\s)))" };
    match = nlDirector.match(fullinfo);
    if (match.hasMatch())
        event.AddPerson(DBPerson::kDirector, match.captured(1));

    // Strip leftovers
    static const QRegularExpression nlRub { R"(\s?\(\W+\)\s?)" };
    fullinfo.remove(nlRub);

    // Strip category info from description
    static const QRegularExpression nlCat { "^(Amusement|Muziek|Informatief|Nieuws/actualiteiten|Jeugd|Animatie|Sport|Serie/soap|Kunst/Cultuur|Documentaire|Film|Natuur|Erotiek|Comedy|Misdaad|Religieus)\\.\\s" };
    fullinfo.remove(nlCat);

    // Remove omroep from title
    static const QRegularExpression nlOmroep { R"(\s\(([A-Z]+/?)+\)$)" };
    event.m_title.remove(nlOmroep);

    // Put information back in description

    event.m_description = fullinfo;
}

void EITFixUp::FixCategory(DBEventEIT &event)
{
    // remove category movie from short events
    if (event.m_categoryType == ProgramInfo::kCategoryMovie &&
        event.m_starttime.secsTo(event.m_endtime) < kMinMovieDuration)
    {
        /* default taken from ContentDescriptor::GetMythCategory */
        event.m_categoryType = ProgramInfo::kCategoryTVShow;
    }
}

/**
 *  \brief Use this to clean DVB-S guide in Norway.
 */
void EITFixUp::FixNO(DBEventEIT &event)
{
    // Check for "title (R)" in the title
    static const QRegularExpression noRerun { "\\(R\\)" };
    auto match = noRerun.match(event.m_title);
    if (match.hasMatch())
    {
      event.m_previouslyshown = true;
      event.m_title.remove(match.capturedStart(), match.capturedLength());
    }
    // Check for "subtitle (HD)" in the subtitle
    static const QRegularExpression noHD { R"([\(\[]HD[\)\]])" };
    match = noHD.match(event.m_subtitle);
    if (match.hasMatch())
    {
      event.m_videoProps |= VID_HDTV;
      event.m_subtitle.remove(match.capturedStart(), match.capturedLength());
    }
   // Check for "description (HD)" in the description
    match = noHD.match(event.m_description);
    if (match.hasMatch())
    {
      event.m_videoProps |= VID_HDTV;
      event.m_description.remove(match.capturedStart(), match.capturedLength());
    }
}

/**
 *  \brief Use this to clean DVB-T guide in Norway (NRK)
 */
void EITFixUp::FixNRK_DVBT(DBEventEIT &event)
{
    // Check for "title (R)" in the title
    static const QRegularExpression noRerun { "\\(R\\)" };
    auto match = noRerun.match(event.m_title);
    if (match.hasMatch())
    {
      event.m_previouslyshown = true;
      event.m_title.remove(match.capturedStart(), match.capturedLength());
    }
    // Check for "(R)" in the description
    match = noRerun.match(event.m_description);
    if (match.hasMatch())
    {
      event.m_previouslyshown = true;
    }

    // Move colon separated category from program-titles into description
    // Have seen "NRK2s historiekveld: Film: bla-bla"
    static const QRegularExpression noNRKCategories
        { "^(Superstrek[ea]r|Supersomm[ea]r|Superjul|Barne-tv|Fantorangen|Kuraffen|Supermorg[eo]n|Julemorg[eo]n|Sommermorg[eo]n|"
          "Kuraffen-TV|Sport i dag|NRKs sportsl.rdag|NRKs sportss.ndag|Dagens dokumentar|"
          "NRK2s historiekveld|Detektimen|Nattkino|Filmklassiker|Film|Kortfilm|P.skemorg[eo]n|"
          "Radioteatret|Opera|P2-Akademiet|Nyhetsmorg[eo]n i P2 og Alltid Nyheter:): (.+)" };
    match = noNRKCategories.match(event.m_title);
    if (match.hasMatch() && (match.capturedLength(2) > 1))
    {
        event.m_title = match.captured(2);
        event.m_description = "(" + match.captured(1) + ") " + event.m_description;
    }

    // Remove season premiere markings
    static const QRegularExpression noPremiere { "\\s+-\\s+(Sesongpremiere|Premiere|premiere)!?$" };
    match = noPremiere.match(event.m_title);
    if (match.hasMatch() && (match.capturedStart() >= 3))
        event.m_title.remove(match.capturedStart(), match.capturedLength());

    // Try to find colon-delimited subtitle in title, only tested for NRK channels
    if (!event.m_title.startsWith("CSI:") &&
        !event.m_title.startsWith("CD:") &&
        !event.m_title.startsWith("Distriktsnyheter: fra"))
    {
        static const QRegularExpression noColonSubtitle { "^([^:]+): (.+)" };
        match = noColonSubtitle.match(event.m_title);
        if (match.hasMatch())
        {
            if (event.m_subtitle.length() <= 0)
            {
                event.m_title    = match.captured(1);
                event.m_subtitle = match.captured(2);
            }
            else if (event.m_subtitle == match.captured(2))
            {
                event.m_title    = match.captured(1);
            }
        }
    }
}

/**
 *  \brief Use this to clean YouSee's DVB-C guide in Denmark.
 */
void EITFixUp::FixDK(DBEventEIT &event)
{
    // Source: YouSee Rules of Operation v1.16
    // url: http://yousee.dk/~/media/pdf/CPE/Rules_Operation.ashx
    int        episode = -1;
    int        season = -1;

    // Title search
    // episode and part/part total
    static const QRegularExpression dkEpisode { R"(\(([0-9]+)\))" };
    auto match = dkEpisode.match(event.m_title);
    if (match.hasMatch())
    {
        episode = match.capturedView(1).toInt();
        event.m_partnumber = match.capturedView(1).toInt();
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    static const QRegularExpression dkPart { R"(\(([0-9]+):([0-9]+)\))" };
    match = dkPart.match(event.m_title);
    if (match.hasMatch())
    {
        episode = match.capturedView(1).toInt();
        event.m_partnumber = match.capturedView(1).toInt();
        event.m_parttotal = match.capturedView(2).toInt();
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    // subtitle delimiters
    static const QRegularExpression dkSubtitle1 { "^([^:]+): (.+)" };
    match = dkSubtitle1.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_title =    match.captured(1);
        event.m_subtitle = match.captured(2);
    }
    else
    {
        static const QRegularExpression dkSubtitle2 { "^([^:]+) - (.+)" };
        match = dkSubtitle2.match(event.m_title);
        if (match.hasMatch())
        {
            event.m_title =    match.captured(1);
            event.m_subtitle = match.captured(2);
        }
    }

    // Description search
    // Season (Sæson [:digit:]+.) => episode = season episode number
    // or year (- år [:digit:]+(\\)|:) ) => episode = total episode number
    static const QRegularExpression dkSeason1 { "Sæson ([0-9]+)\\." };
    match = dkSeason1.match(event.m_description);
    if (match.hasMatch())
    {
        season = match.capturedView(1).toInt();
    }
    else
    {
        static const QRegularExpression dkSeason2 { "- år ([0-9]+) :" };
        match = dkSeason2.match(event.m_description);
        if (match.hasMatch())
        {
            season = match.capturedView(1).toInt();
        }
    }

    if (episode > 0)
        event.m_episode = episode;

    if (season > 0)
        event.m_season = season;

    //Feature:
    static const QRegularExpression dkFeatures { "Features:(.+)" };
    match = dkFeatures.match(event.m_description);
    if (match.hasMatch())
    {
        QString features = match.captured(1);
        event.m_description.remove(match.capturedStart(),
                                   match.capturedLength());
        // 16:9
        static const QRegularExpression dkWidescreen { " 16:9" };
        if (features.indexOf(dkWidescreen) !=  -1)
            event.m_videoProps |= VID_WIDESCREEN;
        // HDTV
        static const QRegularExpression dkHD { " HD" };
        if (features.indexOf(dkHD) !=  -1)
            event.m_videoProps |= VID_HDTV;
        // Dolby Digital surround
        static const QRegularExpression dkDolby { " 5:1" };
        if (features.indexOf(dkDolby) !=  -1)
            event.m_audioProps |= AUD_DOLBY;
        // surround
        static const QRegularExpression dkSurround { R"( \(\(S\)\))" };
        if (features.indexOf(dkSurround) !=  -1)
            event.m_audioProps |= AUD_SURROUND;
        // stereo
        static const QRegularExpression dkStereo { " S" };
        if (features.indexOf(dkStereo) !=  -1)
            event.m_audioProps |= AUD_STEREO;
        // (G)
        static const QRegularExpression dkReplay { " \\(G\\)" };
        if (features.indexOf(dkReplay) !=  -1)
            event.m_previouslyshown = true;
        // TTV
        static const QRegularExpression dkTxt { " TTV" };
        if (features.indexOf(dkTxt) !=  -1)
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
        {
            newid = QString("%1%2").arg(event.m_chanid).
                    arg(event.m_seriesId.mid(2,8));
        }
        else
        {
            newid = event.m_seriesId.mid(2,8);
        }
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
            event.m_subtitle = QString("%1 Sæson %2").arg(event.m_subtitle).
                    arg(season);
        }
        event.m_subtitle = QString("%1)").arg(event.m_subtitle);
    }

    // Find actors and director in description
    static const QRegularExpression dkDirector { "(?:Instr.: |Instrukt.r: )(.+)$" };
    static const QRegularExpression dkPersonsSeparator { "(, )|(og )" };
    QStringList directors {};
    match = dkDirector.match(event.m_description);
    if (match.hasMatch())
    {
        QString tmpDirectorsString = match.captured(1);
        directors = tmpDirectorsString.split(dkPersonsSeparator, Qt::SkipEmptyParts);
        for (const auto & director : std::as_const(directors))
        {
            tmpDirectorsString = director.split(":").last().trimmed().
                remove(kDotAtEnd);
            if (tmpDirectorsString != "")
                event.AddPerson(DBPerson::kDirector, tmpDirectorsString);
        }
        //event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    static const QRegularExpression dkActors { "(?:Medvirkende: |Medv\\.: )(.+)" };
    match = dkActors.match(event.m_description);
    if (match.hasMatch())
    {
        QString tmpActorsString = match.captured(1);
        const QStringList actors =
            tmpActorsString.split(dkPersonsSeparator, Qt::SkipEmptyParts);
        for (const auto & actor : std::as_const(actors))
        {
            tmpActorsString = actor.split(":").last().trimmed().remove(kDotAtEnd);
            if (!tmpActorsString.isEmpty() && !directors.contains(tmpActorsString))
                event.AddPerson(DBPerson::kActor, tmpActorsString);
        }
        //event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    //find year
    static const QRegularExpression dkYear { " fra ([0-9]{4})[ \\.]" };
    match = dkYear.match(event.m_description);
    if (match.hasMatch())
    {
        bool ok = false;
        uint y = match.capturedView(1).toUInt(&ok);
        if (ok)
            event.m_originalairdate = QDate(y, 1, 1);
    }
}

/**
 *  \brief Use this to clean HTML Tags from EIT Data
 */
void EITFixUp::FixStripHTML(DBEventEIT &event)
{
    LOG(VB_EIT, LOG_INFO, QString("Applying html strip to %1").arg(event.m_title));
    static const QRegularExpression html { "</?EM>", QRegularExpression::CaseInsensitiveOption };
    event.m_title.remove(html);
}

// Moves the subtitle field into the description since it's just used
// as more description field. All the sort-out will happen in the description
// field. Also, sometimes the description is just a repeat of the title. If so,
// we remove it.
void EITFixUp::FixGreekSubtitle(DBEventEIT &event)
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

void EITFixUp::FixGreekEIT(DBEventEIT &event)
{
    // Program ratings
    static const QRegularExpression grRating { R"(\[(K|Κ|8|12|16|18)\]\s*)",
        QRegularExpression::CaseInsensitiveOption };
    auto match = grRating.match(event.m_title);
    if (match.hasMatch())
    {
      EventRating prograting;
      prograting.m_system="GR"; prograting.m_rating = match.captured(1);
      event.m_ratings.push_back(prograting);
      event.m_title.remove(match.capturedStart(), match.capturedLength());
      event.m_title = event.m_title.trimmed();
    }

    //Live show
    int position = event.m_title.indexOf("(Ζ)");
    if (position != -1)
    {
        event.m_title = event.m_title.replace("(Ζ)", "");
        event.m_description.prepend("Ζωντανή Μετάδοση. ");
    }

    // Greek not previously Shown
    static const QRegularExpression grNotPreviouslyShown {
        R"(\W?(?:-\s*)*(?:\b[Α1]['΄η]?\s*(?:τηλεοπτικ[ηή]\s*)?(?:μετ[αά]δοση|προβολ[ηή]))\W?)",
        QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption };
    match = grNotPreviouslyShown.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_previouslyshown = false;
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    // Greek Replay (Ε)
    // it might look redundant compared to previous check but at least it helps
    // remove the (Ε) From the title.
    static const QRegularExpression grReplay { R"(\([ΕE]\))" };
    match = grReplay.match(event.m_title);
    if (match.hasMatch())
    {
        event.m_previouslyshown = true;
        event.m_title.remove(match.capturedStart(), match.capturedLength());
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

    static const QRegularExpression grFixnofullstopActors { R"(\w\s(Παίζουν:|Πρωταγων))" };
    match = grFixnofullstopActors.match(event.m_description);
    if (match.hasMatch())
        event.m_description.insert(match.capturedStart() + 1, ".");

    // If they forgot the "." at the end of the sentence before the actors/directors begin, let's insert it.
    static const QRegularExpression grFixnofullstopDirectors { R"(\w\s(Σκηνοθ[εέ]))" };
    match = grFixnofullstopDirectors.match(event.m_description);
    if (match.hasMatch())
        event.m_description.insert(match.capturedStart() + 1, ".");

    // Find actors and director in description
    // I am looking for actors first and then for directors/presenters because
    // sometimes punctuation is missing and the "Παίζουν:" label is mistaken
    // for a director's/presenter's surname (directors/presenters are shown
    // before actors in the description field.). So removing the text after
    // adding the actors AND THEN looking for dir/pres helps to clear things up.
    static const QRegularExpression grActors { R"((?:[Ππ]α[ιί]ζουν:|[ΜMμ]ε τους:|Πρωταγωνιστο[υύ]ν:|Πρωταγωνιστε[ιί]:?)(?:\s+στο ρόλο(?: του| της)?\s(?:\w+\s[οη]\s))?([-\w\s']+(?:,[-\w\s']+)*)(?:κ\.[αά])?\W?)" };
      // cap(1) actors, just names
    static const QRegularExpression grPeopleSeparator { R"(([,-]\s+))" };
    match = grActors.match(event.m_description);
    if (match.hasMatch())
    {
        QString tmpActorsString = match.captured(1);
        const QStringList actors =
            tmpActorsString.split(grPeopleSeparator, Qt::SkipEmptyParts);
        for (const auto & actor : std::as_const(actors))
        {
            tmpActorsString = actor.split(":").last().trimmed().remove(kDotAtEnd);
            if (tmpActorsString != "")
                event.AddPerson(DBPerson::kActor, tmpActorsString);
        }
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    // Director
    static const QRegularExpression grDirector { R"((?:Σκηνοθεσία: |Σκηνοθέτης: |Σκηνοθέτης - Επιμέλεια: )(\w+\s\w+\s?)(?:\W?))" };
    match = grDirector.match(event.m_description);
    if (match.hasMatch())
    {
        QString tmpDirectorsString = match.captured(1);
        const QStringList directors =
            tmpDirectorsString.split(grPeopleSeparator, Qt::SkipEmptyParts);
        for (const auto & director : std::as_const(directors))
        {
            tmpDirectorsString = director.split(":").last().trimmed().
                remove(kDotAtEnd);
            if (tmpDirectorsString != "")
            {
                event.AddPerson(DBPerson::kDirector, tmpDirectorsString);
            }
        }
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    //Try to find presenter
    static const QRegularExpression grPres { R"((?:Παρουσ[ιί]αση:(?:\b)*|Παρουσι[αά]ζ(?:ουν|ει)(?::|\sο|\sη)|Παρουσι[αά]στ(?:[ηή]ς|ρια|ριες|[εέ]ς)(?::|\sο|\sη)|Με τ(?:ον |ην )(?:[\s|:|ο|η])*(?:\b)*)([-\w\s]+(?:,[-\w\s]+)*)\W?)",
        QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption };
    match = grPres.match(event.m_description);
    if (match.hasMatch())
    {
        QString tmpPresentersString = match.captured(1);
        const QStringList presenters =
            tmpPresentersString.split(grPeopleSeparator, Qt::SkipEmptyParts);
        for (const auto & presenter : std::as_const(presenters))
        {
            tmpPresentersString = presenter.split(":").last().trimmed().
                remove(kDotAtEnd);
            if (tmpPresentersString != "")
            {
                event.AddPerson(DBPerson::kPresenter, tmpPresentersString);
            }
        }
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }

    //find year e.g Παραγωγής 1966 ή ΝΤΟΚΙΜΑΝΤΕΡ - 1998 Κατάλληλο για όλους
    // Used in Private channels (not 'secret', just not owned by Government!)
    static const QRegularExpression grYear { R"(\W?(?:\s?παραγωγ[ηή]ς|\s?-|,)\s*([1-2][0-9]{3})(?:-\d{1,4})?)",
        QRegularExpression::CaseInsensitiveOption };
    match = grYear.match(event.m_description);
    if (match.hasMatch())
    {
        bool ok = false;
        uint y = match.capturedView(1).toUInt(&ok);
        if (ok)
        {
            event.m_originalairdate = QDate(y, 1, 1);
            event.m_description.remove(match.capturedStart(), match.capturedLength());
        }
    }
    // Remove " ."
    event.m_description = event.m_description.replace(" .",".").trimmed();

    //find country of origin and remove it from description.
    static const QRegularExpression grCountry {
        R"((?:\W|\b)(?:(ελλην|τουρκ|αμερικ[αά]ν|γαλλ|αγγλ|βρεττ?αν|γερμαν|ρωσσ?|ιταλ|ελβετ|σουηδ|ισπαν|πορτογαλ|μεξικ[αά]ν|κιν[εέ]ζικ|ιαπων|καναδ|βραζιλι[αά]ν)(ικ[ηή][ςσ])))",
        QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption  };
    match = grCountry.match(event.m_description);
    if (match.hasMatch())
        event.m_description.remove(match.capturedStart(), match.capturedLength());

    // Work out the season and episode numbers (if any)
    // Matching pattern "Επεισ[όο]διο:?|Επ 3 από 14|3/14" etc
    bool    series  = false;
    static const QRegularExpression grSeason {
        R"((?:\W-?)*(?:\(-\s*)?\b(([Α-Ω|A|B|E|Z|H|I|K|M|N]{1,2})(?:'|΄)?|(\d{1,2})(?:ος|ου|oς|os)?)(?:\s*[ΚκKk][υύ]κλο(?:[σς]|υ))\s?)",
        QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption };
    // cap(2) is the season for ΑΒΓΔ
    // cap(3) is the season for 1234
    match = grSeason.match(event.m_title);
    if (match.hasMatch())
    {
        if (!match.capturedView(2).isEmpty()) // we found a letter representing a number
        {
            //sometimes Nat. TV writes numbers as letters, i.e Α=1, Β=2, Γ=3, etc
            //must convert them to numbers.
            int tmpinteger = match.capturedView(2).toUInt();
            if (tmpinteger < 1)
            {
                if (match.captured(2) == "ΣΤ") // 6, don't ask!
                    event.m_season = 6;
                else
                {
                    static const QString LettToNumber = "0ΑΒΓΔΕ6ΖΗΘΙΚΛΜΝ";
                    tmpinteger = LettToNumber.indexOf(match.capturedView(2));
                    if (tmpinteger != -1)
                        event.m_season = tmpinteger;
                    else
                    //sometimes they use english letters instead of greek. Compensating:
                    {
                        static const QString LettToNumber2 = "0ABΓΔE6ZHΘIKΛMN";
                        tmpinteger = LettToNumber2.indexOf(match.capturedView(2));
                        if (tmpinteger != -1)
                           event.m_season = tmpinteger;
                    }
                }
            }
        }
        else if (!match.capturedView(3).isEmpty()) //number
        {
            event.m_season = match.capturedView(3).toUInt();
        }
        series = true;
        event.m_title.remove(match.capturedStart(), match.capturedLength());
    }

    // I have to search separately for season in title and description because it wouldn't work when in both.
    match = grSeason.match(event.m_description);
    if (match.hasMatch())
    {
        if (!match.capturedView(2).isEmpty()) // we found a letter representing a number
        {
            //sometimes Nat. TV writes numbers as letters, i.e Α=1, Β=2, Γ=3, etc
            //must convert them to numbers.
            int tmpinteger = match.capturedView(2).toUInt();
            if (tmpinteger < 1)
            {
                if (match.captured(2) == "ΣΤ") // 6, don't ask!
                    event.m_season = 6;
                else
                {
                    static const QString LettToNumber = "0ΑΒΓΔΕ6ΖΗΘΙΚΛΜΝ";
                    tmpinteger = LettToNumber.indexOf(match.capturedView(2));
                    if (tmpinteger != -1)
                        event.m_season = tmpinteger;
                }
            }
        }
        else if (!match.capturedView(3).isEmpty()) //number
        {
            event.m_season = match.capturedView(3).toUInt();
        }
        series = true;
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }


    // If Season is in Roman Numerals (I,II,etc)
    static const QRegularExpression grSeasonAsRomanNumerals { ",\\s*([MDCLXVIΙΧ]+)$",
        QRegularExpression::CaseInsensitiveOption };
    match = grSeasonAsRomanNumerals.match(event.m_title);
    auto match2 = grSeasonAsRomanNumerals.match(event.m_description);
    if (match.hasMatch())
    {
        if (!match.capturedView(1).isEmpty()) //number
            event.m_season = parseRoman(match.captured(1).toUpper());
        series = true;
        event.m_title.remove(match.capturedStart(), match.capturedLength());
        event.m_title = event.m_title.trimmed();
        if (event.m_title.right(1) == ",")
            event.m_title.chop(1);
    }
    else if (match2.hasMatch())
    {
        if (!match2.capturedView(1).isEmpty()) //number
            event.m_season = parseRoman(match2.captured(1).toUpper());
        series = true;
        event.m_description.remove(match2.capturedStart(), match2.capturedLength());
        event.m_description = event.m_description.trimmed();
        if (event.m_description.right(1) == ",")
            event.m_description.chop(1);
    }

    static const QRegularExpression grlongEp { R"(\b(?:Επ.|επεισ[οό]διο:?)\s*(\d+)\W?)",
        QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption };
    // cap(1) is the Episode No.
    match  = grlongEp.match(event.m_title);
    match2 = grlongEp.match(event.m_description);
    if (match.hasMatch() || match2.hasMatch())
    {
        if (!match.capturedView(1).isEmpty())
        {
            event.m_episode = match.capturedView(1).toUInt();
            series = true;
            event.m_title.remove(match.capturedStart(), match.capturedLength());
        }
        else if (!match2.capturedView(1).isEmpty())
        {
            event.m_episode = match2.capturedView(1).toUInt();
            series = true;
            event.m_description.remove(match2.capturedStart(), match2.capturedLength());
        }
        // Sometimes description omits Season if it's 1. We fix this
        if (0 == event.m_season)
            event.m_season = 1;
    }

    // Sometimes, especially on greek national tv, they include comments in the
    // title, e.g "connection to ert1", "ert archives".
    // Because they obscure the real title, I'll isolate and remove them.

    static const QRegularExpression grCommentsinTitle { R"(\(([Α-Ωα-ω\s\d-]+)\)(?:\s*$)*)" };
      // cap1 = real title
      // cap0 = real title in parentheses.
    match = grCommentsinTitle.match(event.m_title);
    if (match.hasMatch()) // found in title instead
        event.m_title.remove(match.capturedStart(), match.capturedLength());

    // Sometimes the real (mostly English) title of a movie or series is
    // enclosed in parentheses in the event title, subtitle or description.
    // Since the subtitle has been moved to the description field by
    // EITFixUp::FixGreekSubtitle, I will search for it only in the description.
    // It will replace the translated one to get better chances of metadata
    // retrieval. The old title will be moved in the description.
    static const QRegularExpression grRealTitleInDescription { R"(^\(([A-Za-z\s\d-]+)\)\s*)" };
    // cap1 = real title
    // cap0 = real title in parentheses.
    match = grRealTitleInDescription.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_description.remove(0, match.capturedLength());
        if (match.captured(0) != event.m_title.trimmed())
        {
            event.m_description = "(" + event.m_title.trimmed() + "). " + event.m_description;
        }
        event.m_title = match.captured(1);
        // Remove the real title from the description
    }
    else // search in title
    {
        static const QRegularExpression grRealTitleInTitle { R"(\(([A-Za-z\s\d-]+)\)(?:\s*$)?)" };
        // cap1 = real title
        // cap0 = real title in parentheses.
        match = grRealTitleInTitle.match(event.m_title);
        if (match.hasMatch()) // found in title instead
        {
            event.m_title.remove(match.capturedStart(), match.capturedLength());
            QString tmpTranslTitle = event.m_title;
            //QString tmpTranslTitle = event.m_title.replace(tmptitle.cap(0),"");
            event.m_title = match.captured(1);
            event.m_description = "(" + tmpTranslTitle.trimmed() + "). " + event.m_description;
        }
    }

    // Description field: "^Episode: Lion in the cage. (Description follows)"
    static const QRegularExpression grEpisodeAsSubtitle { R"(^Επεισ[οό]διο:\s?([\w\s\-,']+)\.\s?)" };
    match = grEpisodeAsSubtitle.match(event.m_description);
    if (match.hasMatch())
    {
        event.m_subtitle = match.captured(1).trimmed();
        event.m_description.remove(match.capturedStart(), match.capturedLength());
    }
    static const QRegularExpression grMovie { R"(\bταιν[ιί]α\b)",
        QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption };
    bool isMovie = (event.m_description.indexOf(grMovie) !=-1) ;
    if (isMovie)
        event.m_categoryType = ProgramInfo::kCategoryMovie;
    else if (series)
        event.m_categoryType = ProgramInfo::kCategorySeries;
    // clear double commas.
    event.m_description.replace(",,", ",");

// να σβήσω τα κομμάτια που περισσεύουν από την περιγραφή πχ παραγωγής χχχχ
}

void EITFixUp::FixGreekCategories(DBEventEIT &event)
{
    struct grCategoryEntry {
        QRegularExpression expr;
        QString category;
    };
    static const QRegularExpression grCategFood { "\\W?(?:εκπομπ[ηή]\\W)?(Γαστρονομ[ιί]α[σς]?|μαγειρικ[ηή][σς]?|chef|συνταγ[εέηή]|διατροφ|wine|μ[αά]γειρα[σς]?)\\W?",
            QRegularExpression::CaseInsensitiveOption };
    static const QRegularExpression grCategDrama { "\\W?(κοινωνικ[ηήό]|δραματικ[ηή]|δρ[αά]μα)\\W(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategComedy { "\\W?(κωμικ[ηήοό]|χιουμοριστικ[ηήοό]|κωμωδ[ιί]α)\\W(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategChildren { "\\W?(παιδικ[ηήοό]|κινο[υύ]μ[εέ]ν(ων|α)\\sσχ[εέ]δ[ιί](ων|α))\\W(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategMystery { "(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?\\W?(μυστηρ[ιί]ου)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategFantasy { "(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?\\W?(φαντασ[ιί]ας)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategHistory { "\\W?(ιστορικ[ηήοό])\\W?(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategTeleMag { "\\W?(ενημερωτικ[ηή]|ψυχαγωγικ[ηή]|τηλεπεριοδικ[οό]|μαγκαζ[ιί]νο)\\W?(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategTeleShop { "\\W?(οδηγ[οό][σς]?\\sαγορ[ωώ]ν|τηλεπ[ωώ]λ[ηή]σ|τηλεαγορ|τηλεμ[αά]ρκετ|telemarket)\\W?(?:(?:εκπομπ[ηή]|σειρ[αά]|ταιν[ιί]α)\\W)?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategGameShow { "\\W?(τηλεπαιχν[ιί]δι|quiz)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategDocumentary { "\\W?(ντοκ[ιυ]μαντ[εέ]ρ)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategBiography { "\\W?(βιογραφ[ιί]α|βιογραφικ[οό][σς]?)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategNews { "\\W?(δελτ[ιί]ο\\W?|ειδ[ηή]σε(ι[σς]|ων))\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategSports { "\\W?(champion|αθλητικ[αάοόηή]|πρωτ[αά]θλημα|ποδ[οό]σφαιρο(ου)?|κολ[υύ]μβηση|πατιν[αά]ζ|formula|μπ[αά]σκετ|β[οό]λε[ιϊ])\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategMusic { "\\W?(μουσικ[οόηή]|eurovision|τραγο[υύ]δι)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategReality { "\\W?(ρι[αά]λιτι|reality)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategReligion { "\\W?(θρησκε[ιί]α|θρησκευτικ|να[οό][σς]?|θε[ιί]α λειτουργ[ιί]α)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategCulture { "\\W?(τ[εέ]χν(η|ε[σς])|πολιτισμ)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategNature { "\\W?(φ[υύ]ση|περιβ[αά]λλο|κατασκευ|επιστ[ηή]μ(?!ονικ[ηή]ς φαντασ[ιί]ας))\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategSciFi { "\\W?(επιστ(.|ημονικ[ηή]ς)\\s?φαντασ[ιί]ας)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategHealth { "\\W?(υγε[ιί]α|υγειιν|ιατρικ|διατροφ)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression grCategSpecial { "\\W?(αφι[εέ]ρωμα)\\W?",
            QRegularExpression::CaseInsensitiveOption};
    static const QList<grCategoryEntry> grCategoryDescData = {
        { grCategComedy,      "Κωμωδία" },
        { grCategTeleMag,     "Τηλεπεριοδικό" },
        { grCategNature,      "Επιστήμη/Φύση" },
        { grCategHealth,      "Υγεία" },
        { grCategReality,     "Ριάλιτι" },
        { grCategDrama,       "Κοινωνικό" },
        { grCategChildren,    "Παιδικό" },
        { grCategSciFi,       "Επιστ.Φαντασίας" },
        { grCategMystery,     "Μυστηρίου" },
        { grCategFantasy,     "Φαντασίας" },
        { grCategHistory,     "Ιστορικό" },
        { grCategTeleShop,    "Τηλεπωλήσεις" },
        { grCategFood,        "Γαστρονομία" },
        { grCategGameShow,    "Τηλεπαιχνίδι" },
        { grCategBiography,   "Βιογραφία" },
        { grCategSports,      "Αθλητικά" },
        { grCategMusic,       "Μουσική" },
        { grCategDocumentary, "Ντοκιμαντέρ" },
        { grCategReligion,    "Θρησκεία" },
        { grCategCulture,     "Τέχνες/Πολιτισμός" },
        { grCategSpecial,     "Αφιέρωμα" },
    };
    static const QList<grCategoryEntry> grCategoryTitleData = {
        { grCategTeleShop,    "Τηλεπωλήσεις" },
        { grCategGameShow,    "Τηλεπαιχνίδι" },
        { grCategMusic,       "Μουσική" },
        { grCategNews,        "Ειδήσεις" },
    };

    // Handle special cases
    if ((event.m_description.indexOf(grCategFantasy) != -1)
        && (event.m_description.indexOf(grCategMystery) != -1))
    {
        event.m_category = "Φαντασίας/Μυστηρίου";
        return;
    }

    // Find categories in the description
    for (const auto& [expression, category] : grCategoryDescData)
    {
        if (event.m_description.indexOf(expression) != -1) {
            event.m_category = category;
            return;
        }
    }

    // Find categories in the title
    for (const auto& [expression, category] : grCategoryTitleData)
    {
        if (event.m_title.indexOf(expression) != -1) {
            event.m_category = category;
            return;
        }
    }
}

void EITFixUp::FixUnitymedia(DBEventEIT &event)
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
    auto i = event.m_items.begin();
    while (i != event.m_items.end())
    {
        /* Possible TODO: if EIT inlcude the priority and/or character
         * names for the actors, include them in AddPerson call. */
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
    static const QRegularExpression unitymediaImdbrating { R"(\s*IMDb Rating: (\d\.\d)\s?/10$)" };
    auto match = unitymediaImdbrating.match(event.m_description);
    if (match.hasMatch())
    {
        float stars = match.captured(1).toFloat();
        event.m_stars = stars / 10.0F;
        event.m_description.remove(match.capturedStart(0),
                                   match.capturedLength(0));
    }
}
