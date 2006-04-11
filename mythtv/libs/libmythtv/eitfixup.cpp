#include "eitfixup.h"
#include "mythcontext.h"

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
      m_comHemCountry("^(.+)\\.\\s(.+)\\.\\s(?:([0-9]{2,4})\\.\\s*)?"),
      m_comHemPEnd("\\.$"),
      m_comHemDirector("[Rr]egi"),
      m_comHemActor("[Ss]kådespelare"),
      m_comHemSub("[.\?] "),
      m_comHemRerun1("[Rr]epris\\sfrån\\s([^\\.]+)(?:\\.|$)"),
      m_comHemRerun2("([0-9]+)/([0-9]+)(?:\\s-\\s([0-9]{4}))?")
{
}

void EITFixUp::Fix(Event &event, int fix_type) const
{
    event.Year = event.StartTime.toString(QString("yyyy"));

    if (event.Description.isEmpty() && !event.Event_Subtitle.isEmpty())
    {
        event.Description    = event.Event_Subtitle;
        event.Event_Subtitle = "";
    }

    if (kFixBell == fix_type)
        FixBellExpressVu(event);
    else if (kFixUK == fix_type)
        FixUK(event);
    else if (kFixPBS == fix_type)
        FixPBS(event);
    else if (kFixComHem == fix_type)
        FixComHem(event);
    else if (kFixAUStar == fix_type)
        FixAUStar(event);

    event.Event_Name.stripWhiteSpace();
    event.Event_Subtitle.stripWhiteSpace();
    event.Description.stripWhiteSpace();
}

void EITFixUp::Fix(DBEvent &event) const
{
    EITFixUp::TimeFix(event.starttime);
    EITFixUp::TimeFix(event.endtime);
}

/** \fn EITFixUp::FixBellExpressVu(Event&) const
 *  \brief Use this for the Canadian BellExpressVu to standardize DVB-S guide.
 *  \TODO deal with events that don't have eventype at the begining?
 */
void EITFixUp::FixBellExpressVu(Event &event) const
{
    QString tmp = "";

    // A 0x0D character is present between the content
    // and the subtitle if its present
    int position = event.Description.find(0x0D);

    if (position != -1)
    {
        // Subtitle present in the title, so get
        // it and adjust the description
        event.Event_Subtitle = event.Description.left(position);
        event.Description = event.Description.right(
            event.Description.length() - position - 2);
    }

    // Take out the content description which is
    // always next with a period after it
    position = event.Description.find(".");
    // Make sure they didn't leave it out and
    // you come up with an odd category
    if (position < 10)
    {
        const QString stmp       = event.Description;
        event.Description        = stmp.right(stmp.length() - position - 2);
        event.ContentDescription = stmp.left(position);
    }
    else
    {
        event.ContentDescription = "Unknown";
    }

    // When a channel is off air the category is "-"
    // so leave the category as blank
    if (event.ContentDescription == "-")
        event.ContentDescription = "OffAir";

    if (event.ContentDescription.length() > 10)
        event.ContentDescription = "Unknown";


    // See if a year is present as (xxxx)
    position = event.Description.find(m_bellYear);
    if (position != -1 && event.ContentDescription != "")
    {
        tmp = "";
        // Parse out the year
        event.Year = event.Description.mid(position + 1, 4);
        // Get the actors if they exist
        if (position > 3)
        {
            tmp = event.Description.left(position-3);
            event.Actors = QStringList::split(m_bellActors, tmp);
        }
        // Remove the year and actors from the description
        event.Description = event.Description.right(
            event.Description.length() - position - 7);
    }

    // Check for (CC) in the decription and
    // set the <subtitles type="teletext"> flag
    position = event.Description.find("(CC)");
    if (position != -1)
    {
        event.SubTitled = true;
        event.Description = event.Description.replace("(CC)", "");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.Description.find("(Stereo)");
    if (position != -1)
    {
        event.Stereo = true;
        event.Description = event.Description.replace("(Stereo)", "");
    }
}

/** \fn EITFixUp::FixUK(Event&) const
 *  \brief Use this in the United Kingdom to standardize DVB-T guide.
 */
void EITFixUp::FixUK(Event &event) const
{
    const uint SUBTITLE_PCT = 35; //% of description to allow subtitle up to
    const uint SUBTITLE_MAX_LEN = 128; // max length of subtitle field in db.
    int position = event.Description.find("New Series");
    if (position != -1)
    {
        // Do something here
    }

    position = event.Description.find(m_ukSubtitle);
    event.SubTitled |= (position != -1);

    // BBC three case (could add another record here ?)
    event.Description = event.Description.replace(m_ukThen, "");
    event.Description = event.Description.replace(m_ukThen, "");
    event.Event_Name  = event.Event_Name.replace(m_ukT4, "");

    // This is trying to catch the case where the subtitle is in the main title
    // but avoid cases where it isn't a subtitle e.g cd:uk
    if (((position = event.Event_Name.find(":")) != -1) &&
        (event.Description.find(":") == -1) &&
        (event.Event_Name[position + 1].upper() == event.Event_Name[position + 1]))
    {
        event.Event_Subtitle = event.Event_Name.mid(position + 1);
        event.Event_Name = event.Event_Name.left(position);
    }
    else if ((position = event.Description.find(":")) != -1)
    {
        // if the subtitle is less than 50% of the description use it.
        if (((uint)position < SUBTITLE_MAX_LEN) &&
            ((position*100)/event.Description.length() < SUBTITLE_PCT))
        {
            event.Event_Subtitle = event.Description.left(position);
            event.Description = event.Description.mid(position + 1);
        }
    }
    else if ((position = event.Description.find(m_ukEQ)) != -1)
    {
        if (((uint)position < SUBTITLE_MAX_LEN) &&
            ((position*100)/event.Description.length() < SUBTITLE_PCT))
        {
            event.Event_Subtitle = event.Description.left(position + 1);
            event.Description = event.Description.mid(position + 2);
        }
    }

    if (event.Event_Name.endsWith("...") &&
        event.Event_Subtitle.startsWith(".."))
    {
        // try and make the subtitle
        QString Full = event.Event_Name.replace(m_ukPEnd, "") + " " +
            event.Event_Subtitle.replace(m_ukPStart, "");

        if ((position = Full.find(m_ukEPQ)) != -1)
        {
            if (Full[position] == '!' || Full[position] == '?')
                position++;
            event.Event_Name = Full.left(position);
            event.Event_Subtitle = Full.mid(position + 1);
        }
        else
        {
            event.Event_Name = Full;
            event.Event_Subtitle = "";
        }
    }
    else if (event.Event_Subtitle.endsWith("...") &&
             event.Description.startsWith("..."))
    {
        QString Full = event.Event_Subtitle.replace(m_ukPEnd, "") + " " +
            event.Description.replace(m_ukPStart, "");
        if ((position = Full.find(m_ukEPQ)) != -1)
        {
            if (Full[position] == '!' || Full[position] == '?')
                position++;
            event.Event_Subtitle = Full.left(position);
            event.Description = Full.mid(position + 1);
        }
    }
    else if (event.Event_Name.endsWith("...") &&
             event.Description.startsWith("...") && event.Event_Subtitle.isEmpty())
    {
        QString Full = event.Event_Name.replace(m_ukPEnd, "") + " " +
            event.Description.replace(m_ukPStart, "");
        if ((position = Full.find(m_ukEPQ)) != -1)
        {
            if (Full[position] == '!' || Full[position] == '?')
                position++;
            event.Event_Name = Full.left(position);
            event.Description = Full.mid(position + 1);
        }
    }

    // Work out the episode numbers (if any)
    bool    series  = false;
    QRegExp tmpExp1 = m_ukSeries1;
    QRegExp tmpExp2 = m_ukSeries2;
    if ((position = tmpExp1.search(event.Event_Name)) != -1)
    {
        event.PartNumber = tmpExp1.cap(1).toUInt();
        event.PartTotal  = tmpExp1.cap(2).toUInt();
        // Remove from the title
        event.Event_Name =
            event.Event_Name.mid(position + tmpExp1.cap(0).length());
        // but add it to the description
        event.Description += tmpExp1.cap(0);
        series = true;
    }
    else if ((position = tmpExp1.search(event.Event_Subtitle)) != -1)
    {
        event.PartNumber = tmpExp1.cap(1).toUInt();
        event.PartTotal  = tmpExp1.cap(2).toUInt();
        // Remove from the sub title
        event.Event_Subtitle =
            event.Event_Subtitle.mid(position + tmpExp1.cap(0).length());
        // but add it to the description
        event.Description += tmpExp1.cap(0);
        series = true;
    }
    else if ((position = tmpExp1.search(event.Description)) != -1)
    {
        event.PartNumber = tmpExp1.cap(1).toUInt();
        event.PartTotal  = tmpExp1.cap(2).toUInt();
        // Don't cut it from the description
        //event.Description = event.Description.left(position) +
        //    event.Description.mid(position + tmpExp1.cap(0).length());
        series = true;
    }
    else if ((position = tmpExp2.search(event.Description)) != -1)
    {
        event.PartNumber = tmpExp2.cap(2).toUInt();
        event.PartTotal  = tmpExp2.cap(3).toUInt();
        // Don't cut it from the description
        //event.Description = event.Description.left(position) +
        //    event.Description.mid(position + tmpExp2.cap(0).length());
        series = true;
    }
    event.CategoryType = (series) ? "series" : event.CategoryType;

    // Work out the closed captions and Audio descriptions  (if any)
    QStringList captures;
    QStringList::const_iterator it;
    QRegExp tmpUKCC = m_ukCC;
    if ((position = tmpUKCC.search(event.Description)) != -1)
    {
        // Enumerate throught and see if we have subtitles, don't modify
        // the description as we might destroy other useful information
        captures = tmpUKCC.capturedTexts();
        for (it = captures.begin(); it != captures.end(); ++it)
            event.SubTitled != (*it == "S");
    }
    else if ((position = tmpUKCC.search(event.Event_Subtitle)) != -1)
    {
        captures = tmpUKCC.capturedTexts();
        for (it = captures.begin(); it != captures.end(); ++it)
            event.SubTitled != (*it == "S");

        // We remove [AD,S] from the subtitle.
        QString stmp = event.Event_Subtitle;
        int     itmp = position + tmpUKCC.cap(0).length();
        event.Event_Subtitle = stmp.left(position) + stmp.mid(itmp);
    }

    // Work out the year (if any)
    QRegExp tmpUKYear = m_ukYear;
    if ((position = tmpUKYear.search(event.Description)) != -1)
    {
        QString stmp = event.Description;
        int     itmp = position + tmpUKYear.cap(0).length();
        event.Description = stmp.left(position) + stmp.mid(itmp);
        event.Year = tmpUKYear.cap(1);
    }
}

/** \fn EITFixUp::FixPBS(Event&) const
 *  \brief Use this to standardize PBS ATSC guide in the USA.
 */
void EITFixUp::FixPBS(Event &event) const
{
    /* Used for PBS ATSC Subtitles are seperated by a colon */
    int position = event.Description.find(':');
    if (position != -1)
    {
        const QString stmp   = event.Description;
        event.Event_Subtitle = stmp.left(position);
        event.Description    = stmp.right(stmp.length() - position - 2);
    }
}

/** \fn EITFixUp::FixComHem(Event&) const
 *  \brief Use this to standardize ComHem DVB-C service in Sweden.
 */
void EITFixUp::FixComHem(Event &event) const
{
    // the format of the subtitle is:
    // country. category. year.
    // the year is optional and if the subtitle is
    // empty the same information is in the Description instead.

    if (event.Event_Subtitle.length() == 0 && event.Description.length() > 0)
    {
        event.Event_Subtitle = event.Description;
        event.Description = "";
    }

    // try to find country category and year
    QRegExp tmpCountry = m_comHemCountry;
    int pos = tmpCountry.search(event.Event_Subtitle);
    if (pos != -1)
    {
        QStringList list = tmpCountry.capturedTexts();

        // sometimes the category is empty, in that case use the category from
        // the subtitle. this category is in swedish and all others
        // are in english
        if (event.ContentDescription.length() == 0)
        {
            event.ContentDescription = list[2].stripWhiteSpace();
        }

        if (list[3].length() > 0)
        {
            event.Year = list[3].stripWhiteSpace();
        }

        // not 100% sure about this one.
        event.Event_Subtitle = "";
    }

    if (event.Description.length() <= 0)
        return;

    // everything up to the first 3 spaces is duplicated from title and
    // subtitle so remove it
    pos = event.Description.find("   ");
    if (pos != -1)
    {
        event.Description =
            event.Description.mid(pos + 3).stripWhiteSpace();
        //fprintf(stdout, "EITFixUp::FixStyle4: New: %s\n",
        //event.Description.mid(pos + 3).stripWhiteSpace().ascii());
    }

    // in the description at this point everything up to the first 4 spaces
    // in a row is a list of director(s) and actors.
    // different lists are separated by 3 spaces in a row
    // end of all lists is when there is 4 spaces in a row
    bool bDontRemove = false;
    pos = event.Description.find("    ");
    if (pos != -1)
    {
        QStringList lists;
        lists = QStringList::split("   ", event.Description.left(pos));
        QStringList::iterator it;
        for (it = lists.begin(); it != lists.end(); it++)
        {
            (*it).remove(m_comHemPEnd);
            QStringList list = QStringList::split(": ", *it);
            if (list.count() != 2)
            {
                // oops, improperly formated list, ignore it
                //fprintf(stdout,"EITFixUp::FixStyle4:"
                //"%s is not a properly formated list of persons\n",
                //(*it).ascii());
                bDontRemove = true;
                continue;
            }

            if (list[0].find(m_comHemDirector) != -1)
            {
                // director(s)
                QStringList persons = QStringList::split(", ",list[1]);
                for (QStringList::const_iterator it2 = persons.begin();
                     it2 != persons.end(); it2++)
                {
                    event.Credits.append(Person("director", *it2));
                }
                continue;
            }

            if (list[0].find(m_comHemActor) != -1)
            {
                // actor(s)
                QStringList persons = QStringList::split(", ", list[1]);
                for (QStringList::const_iterator it2 = persons.begin();
                     it2 != persons.end(); it2++)
                {
                    event.Credits.append(Person("actor", *it2));
                }
                continue;
            }

            // unknown type, posibly a new one that
            // this code shoud be updated to handle
            VERBOSE(VB_EIT, "EITFixUp::FixComHem: " +
                    QString("%1 is not actor or director")
                    .arg(list[0]));
            bDontRemove = true;
        }
        // remove list of persons from description if
        // we coud parse it properly
        if (!bDontRemove)
        {
            event.Description = event.Description.mid(pos).stripWhiteSpace();
        }
    }

    //fprintf(stdout, "EITFixUp::FixStyle4: Number of persons: %d\n",
    //event.Credits.count());

    /*
      a regexp like this coud be used to get the episode number, shoud be at
      the begining of the description
      "^(?:[dD]el|[eE]pisode)\\s([0-9]+)(?:\\s?(?:/|:|av)\\s?([0-9]+))?\\."
    */

    // Is this event on a channel we shoud look for a subtitle?
    if (parseSubtitleServiceIDs.contains(event.ServiceID))
    {
        int pos = event.Description.find(m_comHemSub);
        bool pvalid = pos != -1 && pos <= 55;
        if (pvalid && (event.Description.length() - (pos + 2)) > 0)
        {
            event.Event_Subtitle = event.Description.left(
                pos + (event.Description[pos] == '?' ? 1 : 0));
            event.Description = event.Description.mid(pos + 2);
        }
    }

    // Try to findout if this is a rerun and if so the date.
    QRegExp tmpRerun1 = m_comHemRerun1;
    if (tmpRerun1.search(event.Description) == -1)
        return;

    QStringList list = tmpRerun1.capturedTexts();
    if (list[1] == "i dag")
    {
        event.OriginalAirDate = event.StartTime.date();
        return;
    }

    if (list[1] == "eftermiddagen")
    {
        event.OriginalAirDate = event.StartTime.date().addDays(-1);
        return;
    }

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
            year = event.StartTime.date().year();

        if (day > 0 && month > 0)
        {
            QDate date(event.StartTime.date().year(), month, day);
            // it's a rerun so it must be in the past
            if (date > event.StartTime.date())
                date = date.addYears(-1);
            event.OriginalAirDate = date;
            //fprintf(stdout, "EITFixUp::FixStyle4: "
            //"OriginalAirDate set to: %s for '%s'\n",
            //event.OriginalAirDate.toString(Qt::ISODate).ascii(),
            //event.Description.ascii());
        }
        return;
    }
    // unknown date, posibly only a year.
    //fprintf(stdout,"EITFixUp::FixStyle4: "
    //"unknown rerun date: %s\n",list[1].ascii());
}

/** \fn EITFixUp::FixAUStar(Event&) const
 *  \brief Use this to standardize DVB-S guide in Australia.
 */
void EITFixUp::FixAUStar(Event &event) const
{
    event.ContentDescription = event.Event_Subtitle;
    /* Used for DVB-S Subtitles are seperated by a colon */
    int position = event.Description.find(':');
    if (position != -1)
    {
        const QString stmp   = event.Description;
        event.Event_Subtitle = stmp.left(position);
        event.Description    = stmp.right(stmp.length() - position - 2);
    }
    else
    {
        event.Event_Subtitle = "";
    }
}
