#include "eitfixup.h"
#include "mythcontext.h"
#include "dvbdescriptors.h" // for MythCategoryType

/*------------------------------------------------------------------------
 * Event Fix Up Scripts - Turned on by entry in dtv_privatetype table
 *------------------------------------------------------------------------*/

EITFixUp::EITFixUp()
    : m_bellYear("[\\(]{1}[0-9]{4}[\\)]{1}"),
      m_bellActors("\\set\\s|,"),
      m_ukSubtitle("\\[.*S\\]"),
      m_ukThen("\\s*(Then|Followed by) 60 Seconds\\.", false),
      m_ukNew("\\s*(Brand New|New) Series\\s*[:\\.\\-]"),
      m_ukT4("^[tT]4:"),
      m_ukEQ("[\\!\\?]"),
      m_ukEPQ("[:\\!\\.\\?]"),
      m_ukPStart("^\\.+"),
      m_ukPEnd("\\.+$"),
      m_ukSeries1("^\\s*(\\d{1,2})/(\\d{1,2})\\."),
      m_ukSeries2("\\((Part|Pt)\\s+(\\d{1,2})\\s+of\\s+(\\d{1,2})\\)", false),
      m_ukCC("\\[(AD)(,(S)){,1}(,SL){,1}\\]|\\[(S)(,AD){,1}(,SL){,1}\\]|"
             "\\[(SL)(,AD){,1}(,(S)){,1}\\]"),
      m_ukYear("[\\[\\(]([\\d]{4})[\\)\\]]"),
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
      m_comHemTSub("\\s+-\\s+([^\\-]+)")
{
}

void EITFixUp::Fix(DBEvent &event) const
{
    if (event.fixup)
    {
        if (event.subtitle == event.title)
            event.subtitle = QString::null;

        if (event.description.isEmpty() && !event.subtitle.isEmpty())
        {
            event.description = event.subtitle;
            event.subtitle = QString::null;
        }
    }

    if (kFixBell & event.fixup)
        FixBellExpressVu(event);

    if (kFixUK & event.fixup)
        FixUK(event);

    if (kFixPBS & event.fixup)
        FixPBS(event);

    if (kFixComHem & event.fixup)
        FixComHem(event, kFixSubtitle & event.fixup);

    if (kFixAUStar & event.fixup)
        FixAUStar(event);

    if (event.fixup)
    {
        if (!event.title.isEmpty())
            event.title = event.title.stripWhiteSpace();
        if (!event.subtitle.isEmpty())
            event.subtitle = event.subtitle.stripWhiteSpace();
        if (!event.description.isEmpty())
            event.description = event.description.stripWhiteSpace();
    }
}

/**
 *  \brief Use this for the Canadian BellExpressVu to standardize DVB-S guide.
 *  \todo  deal with events that don't have eventype at the begining?
 *  \TODO
 */
void EITFixUp::FixBellExpressVu(DBEvent &event) const
{
    QString tmp = "";

    // A 0x0D character is present between the content
    // and the subtitle if its present
    int position = event.description.find(0x0D);

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
    position = event.description.find(".");
    // Make sure they didn't leave it out and
    // you come up with an odd category
    if (position < 10)
    {
        const QString stmp       = event.description;
        event.description        = stmp.right(stmp.length() - position - 2);
        event.category = stmp.left(position);
    }
    else
    {
        event.category = "Unknown";
    }

    // When a channel is off air the category is "-"
    // so leave the category as blank
    if (event.category == "-")
        event.category = "OffAir";

    if (event.category.length() > 10)
        event.category = "Unknown";


    // See if a year is present as (xxxx)
    position = event.description.find(m_bellYear);
    if (position != -1 && event.category != "")
    {
        tmp = "";
        // Parse out the year
        bool ok;
        uint y = event.description.mid(position + 1, 4).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
            
        // Get the actors if they exist
        if (position > 3)
        {
            tmp = event.description.left(position-3);
            QStringList actors = QStringList::split(m_bellActors, tmp);
            QStringList::const_iterator it = actors.begin();
            for (; it != actors.end(); it++)
                event.AddPerson(DBPerson::kActor, *it);
        }
        // Remove the year and actors from the description
        event.description = event.description.right(
            event.description.length() - position - 7);
    }

    // Check for (CC) in the decription and
    // set the <subtitles type="teletext"> flag
    position = event.description.find("(CC)");
    if (position != -1)
    {
        event.flags |= DBEvent::kCaptioned;
        event.description = event.description.replace("(CC)", "");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.description.find("(Stereo)");
    if (position != -1)
    {
        event.flags |= DBEvent::kStereo;
        event.description = event.description.replace("(Stereo)", "");
    }
}

/** \fn EITFixUp::FixUK(DBEvent&) const
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::FixUK(DBEvent &event) const
{
    const uint SUBTITLE_PCT = 35; //% of description to allow subtitle up to
    const uint SUBTITLE_MAX_LEN = 128; // max length of subtitle field in db.
    int position = event.description.find("New Series");
    if (position != -1)
    {
        // Do something here
    }

    position = event.description.find(m_ukSubtitle);
    if (position != -1)
    {
        event.flags |= DBEvent::kSubtitled;
        event.description.replace(m_ukSubtitle, "");
    }

    // BBC three case (could add another record here ?)
    event.description = event.description.replace(m_ukThen, "");
    event.description = event.description.replace(m_ukNew, "");
    event.title  = event.title.replace(m_ukT4, "");

    // This is trying to catch the case where the subtitle is in the main title
    // but avoid cases where it isn't a subtitle e.g cd:uk
    if (((position = event.title.find(":")) != -1) &&
        (event.description.find(":") == -1) &&
        (event.title[position + 1].upper() == event.title[position + 1]))
    {
        event.subtitle = event.title.mid(position + 1);
        event.title = event.title.left(position);
    }
    else if ((position = event.description.find(":")) != -1)
    {
        // if the subtitle is less than 50% of the description use it.
        if (((uint)position < SUBTITLE_MAX_LEN) &&
            ((position*100)/event.description.length() < SUBTITLE_PCT))
        {
            event.subtitle = event.description.left(position);
            event.description = event.description.mid(position + 1);
        }
    }
    else if ((position = event.description.find(m_ukEQ)) != -1)
    {
        if (((uint)position < SUBTITLE_MAX_LEN) &&
            ((position*100)/event.description.length() < SUBTITLE_PCT))
        {
            event.subtitle = event.description.left(position + 1);
            event.description = event.description.mid(position + 2);
        }
    }

    if (event.title.endsWith("...") &&
        event.subtitle.startsWith(".."))
    {
        // try and make the subtitle
        QString Full = event.title.replace(m_ukPEnd, "") + " " +
            event.subtitle.replace(m_ukPStart, "");

        if ((position = Full.find(m_ukEPQ)) != -1)
        {
            if (Full[position] == '!' || Full[position] == '?')
                position++;
            event.title = Full.left(position);
            event.subtitle = Full.mid(position + 1);
        }
        else
        {
            event.title = Full;
            event.subtitle = "";
        }
    }
    else if (event.subtitle.endsWith("...") &&
             event.description.startsWith("..."))
    {
        QString Full = event.subtitle.replace(m_ukPEnd, "") + " " +
            event.description.replace(m_ukPStart, "");
        if ((position = Full.find(m_ukEPQ)) != -1)
        {
            if (Full[position] == '!' || Full[position] == '?')
                position++;
            event.subtitle = Full.left(position);
            event.description = Full.mid(position + 1);
        }
    }
    else if (event.title.endsWith("...") &&
             event.description.startsWith("...") && event.subtitle.isEmpty())
    {
        QString Full = event.title.replace(m_ukPEnd, "") + " " +
            event.description.replace(m_ukPStart, "");
        if ((position = Full.find(m_ukEPQ)) != -1)
        {
            if (Full[position] == '!' || Full[position] == '?')
                position++;
            event.title = Full.left(position);
            event.description = Full.mid(position + 1);
        }
    }

    // Work out the episode numbers (if any)
    bool    series  = false;
    QRegExp tmpExp1 = m_ukSeries1;
    QRegExp tmpExp2 = m_ukSeries2;
    if ((position = tmpExp1.search(event.title)) != -1)
    {
        event.partnumber = tmpExp1.cap(1).toUInt();
        event.parttotal  = tmpExp1.cap(2).toUInt();
        // Remove from the title
        event.title =
            event.title.mid(position + tmpExp1.cap(0).length());
        // but add it to the description
        event.description += tmpExp1.cap(0);
        series = true;
    }
    else if ((position = tmpExp1.search(event.subtitle)) != -1)
    {
        event.partnumber = tmpExp1.cap(1).toUInt();
        event.parttotal  = tmpExp1.cap(2).toUInt();
        // Remove from the sub title
        event.subtitle =
            event.subtitle.mid(position + tmpExp1.cap(0).length());
        // but add it to the description
        event.description += tmpExp1.cap(0);
        series = true;
    }
    else if ((position = tmpExp1.search(event.description)) != -1)
    {
        event.partnumber = tmpExp1.cap(1).toUInt();
        event.parttotal  = tmpExp1.cap(2).toUInt();
        // Don't cut it from the description
        //event.description = event.description.left(position) +
        //    event.description.mid(position + tmpExp1.cap(0).length());
        series = true;
    }
    else if ((position = tmpExp2.search(event.description)) != -1)
    {
        event.partnumber = tmpExp2.cap(2).toUInt();
        event.parttotal  = tmpExp2.cap(3).toUInt();
        // Don't cut it from the description
        //event.description = event.description.left(position) +
        //    event.description.mid(position + tmpExp2.cap(0).length());
        series = true;
    }
    if (series)
        event.category_type = kCategorySeries;

    // Work out the closed captions and Audio descriptions  (if any)
    QStringList captures;
    QStringList::const_iterator it;
    QRegExp tmpUKCC = m_ukCC;
    if ((position = tmpUKCC.search(event.description)) != -1)
    {
        // Enumerate throught and see if we have subtitles, don't modify
        // the description as we might destroy other useful information
        captures = tmpUKCC.capturedTexts();
        for (it = captures.begin(); it != captures.end(); ++it)
        {
            if (*it == "S")
                event.flags |= DBEvent::kSubtitled;
        }
    }
    else if ((position = tmpUKCC.search(event.subtitle)) != -1)
    {
        captures = tmpUKCC.capturedTexts();
        for (it = captures.begin(); it != captures.end(); ++it)
        {
            if (*it == "S")
                event.flags |= DBEvent::kSubtitled;
        }

        // We remove [AD,S] from the subtitle.
        QString stmp = event.subtitle;
        int     itmp = position + tmpUKCC.cap(0).length();
        event.subtitle = stmp.left(position) + stmp.mid(itmp);
    }

    // Work out the year (if any)
    QRegExp tmpUKYear = m_ukYear;
    if ((position = tmpUKYear.search(event.description)) != -1)
    {
        QString stmp = event.description;
        int     itmp = position + tmpUKYear.cap(0).length();
        event.description = stmp.left(position) + stmp.mid(itmp);
        bool ok;
        uint y = tmpUKYear.cap(1).toUInt(&ok);
        if (ok)
            event.originalairdate = QDate(y, 1, 1);
    }
}

/** \fn EITFixUp::FixPBS(DBEvent&) const
 *  \brief Use this to standardize PBS ATSC guide in the USA.
 */
void EITFixUp::FixPBS(DBEvent &event) const
{
    /* Used for PBS ATSC Subtitles are seperated by a colon */
    int position = event.description.find(':');
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
void EITFixUp::FixComHem(DBEvent &event, bool process_subtitle) const
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
    if ((pos = tmpSeries2.search(event.title)) != -1)
    {
        QStringList list = tmpSeries2.capturedTexts();
        event.partnumber = list[2].toUInt();
        event.title = event.title.replace(list[0],"");
    }
    else if ((pos = tmpSeries1.search(event.description)) != -1)
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
    if (tmpTSub.search(event.title) != -1)
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
    pos = tmpCountry.search(event.description);
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

            if(list[3].find("serie")!=-1)
            {
                isSeries = true;
            }
        }

        // Year
        if (list[4].length() > 0)
        {
            event.airdate = list[4].stripWhiteSpace();
        }

        // Actors
        if (list[5].length() > 0)
        {
            QStringList actors;
            actors=QStringList::split(m_comHemPersSeparator,list[5]);
            for(QStringList::size_type i=0;i<actors.count();i++)
            {
                event.AddPerson(DBPerson::kActor, actors[i]);
            }
        }

        // Remove year and actors.
        // The reason category is left in the description is because otherwise
        // the country would look wierd like "Amerikansk. Rest of description."
        event.description = event.description.replace(list[0],replacement);
    }

    if (isSeries)
        event.category_type = kCategorySeries;

    // Look for additional persons in the description
    QRegExp tmpPersons = m_comHemPersons;
    while(pos = tmpPersons.search(event.description),pos!=-1)
    {
        DBPerson::Role role;
        QStringList list = tmpPersons.capturedTexts();
        
        QRegExp tmpDirector = m_comHemDirector;
        QRegExp tmpActor = m_comHemActor;
        QRegExp tmpHost = m_comHemHost;
        if (tmpDirector.search(list[1])!=-1)
        {
            role = DBPerson::kDirector;
        }
        else if(tmpActor.search(list[1])!=-1)
        {
            role = DBPerson::kActor;
        }
        else if(tmpHost.search(list[1])!=-1)
        {
            role = DBPerson::kHost;
        }

        QStringList actors;
        actors = QStringList::split(m_comHemPersSeparator, list[2]);
        for(QStringList::size_type i=0;i<actors.count();i++)
        {
            event.AddPerson(role, actors[i]);
        }

        // Remove it
        event.description=event.description.replace(list[0],"");
    }

    // Is this event on a channel we shoud look for a subtitle?
    // The subtitle is the first sentence in the description, but the
    // subtitle can't be the only thing in the description and it must be
    // shorter than 55 characters or we risk picking up the wrong thing.
    if (process_subtitle)
    {
        int pos = event.description.find(m_comHemSub);
        bool pvalid = pos != -1 && pos <= 55;
        if (pvalid && (event.description.length() - (pos + 2)) > 0)
        {
            event.subtitle = event.description.left(
                pos + (event.description[pos] == '?' ? 1 : 0));
            event.description = event.description.mid(pos + 2);
        }
    }

    // Teletext subtitles?
    int position = event.description.find(m_comHemTT);
    if (position != -1)
    {
        event.flags |= DBEvent::kSubtitled;
    }

    // Try to findout if this is a rerun and if so the date.
    QRegExp tmpRerun1 = m_comHemRerun1;
    if (tmpRerun1.search(event.description) == -1)
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
    if (tmpRerun2.search(list[1]) != -1)
    {
        QStringList datelist = tmpRerun2.capturedTexts();
        int day   = datelist[1].toInt();
        int month = datelist[2].toInt();
        int year;

        if (datelist[3].length() > 0)
            year = datelist[3].toInt();
        else
            year = event.starttime.date().year();

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

/** \fn EITFixUp::FixAUStar(DBEvent&) const
 *  \brief Use this to standardize DVB-S guide in Australia.
 */
void EITFixUp::FixAUStar(DBEvent &event) const
{
    event.category = event.subtitle;
    /* Used for DVB-S Subtitles are seperated by a colon */
    int position = event.description.find(':');
    if (position != -1)
    {
        const QString stmp   = event.description;
        event.subtitle       = stmp.left(position);
        event.description    = stmp.right(stmp.length() - position - 2);
    }
}
