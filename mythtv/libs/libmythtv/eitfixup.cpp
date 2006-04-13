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
      m_comHemCountry("^(.+)\\.\\s(.+)\\.\\s(?:([0-9]{2,4})\\.\\s*)?"),
      m_comHemPEnd("\\.$"),
      m_comHemDirector("[Rr]egi"),
      m_comHemActor("[Ss]kådespelare"),
      m_comHemSub("[.\?] "),
      m_comHemRerun1("[Rr]epris\\sfrån\\s([^\\.]+)(?:\\.|$)"),
      m_comHemRerun2("([0-9]+)/([0-9]+)(?:\\s-\\s([0-9]{4}))?")
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

/** \fn EITFixUp::FixBellExpressVu(Event&) const
 *  \brief Use this for the Canadian BellExpressVu to standardize DVB-S guide.
 *  \TODO deal with events that don't have eventype at the begining?
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
        event.flags |= DBEvent::kSubtitled;

    // BBC three case (could add another record here ?)
    event.description = event.description.replace(m_ukThen, "");
    event.description = event.description.replace(m_ukThen, "");
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

/** \fn EITFixUp::FixComHem(DBEvent&) const
 *  \brief Use this to standardize ComHem DVB-C service in Sweden.
 */
void EITFixUp::FixComHem(DBEvent &event, bool process_subtitle) const
{
    // the format of the subtitle is:
    // country. category. year.
    // the year is optional and if the subtitle is
    // empty the same information is in the Description instead.

    if (event.subtitle.length() == 0 && event.description.length() > 0)
    {
        event.subtitle = event.description;
        event.description = "";
    }

    // try to find country category and year
    QRegExp tmpCountry = m_comHemCountry;
    int pos = tmpCountry.search(event.subtitle);
    if (pos != -1)
    {
        QStringList list = tmpCountry.capturedTexts();

        // sometimes the category is empty, in that case use the category from
        // the subtitle. this category is in swedish and all others
        // are in english
        if (event.category.length() == 0)
        {
            event.category = list[2].stripWhiteSpace();
        }

        if (list[3].length() > 0)
        {
            bool ok;
            uint y = list[3].stripWhiteSpace().toUInt(&ok);
            if (ok)
                event.originalairdate = QDate(y, 1, 1);
        }

        // not 100% sure about this one.
        event.subtitle = "";
    }

    if (event.description.length() <= 0)
        return;

    // everything up to the first 3 spaces is duplicated from title and
    // subtitle so remove it
    pos = event.description.find("   ");
    if (pos != -1)
    {
        event.description =
            event.description.mid(pos + 3).stripWhiteSpace();
        //fprintf(stdout, "EITFixUp::FixStyle4: New: %s\n",
        //event.description.mid(pos + 3).stripWhiteSpace().ascii());
    }

    // in the description at this point everything up to the first 4 spaces
    // in a row is a list of director(s) and actors.
    // different lists are separated by 3 spaces in a row
    // end of all lists is when there is 4 spaces in a row
    bool bDontRemove = false;
    pos = event.description.find("    ");
    if (pos != -1)
    {
        QStringList lists;
        lists = QStringList::split("   ", event.description.left(pos));
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
                    event.AddPerson(DBPerson::kDirector, *it2);
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
                    event.AddPerson(DBPerson::kActor, *it2);
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
            event.description = event.description.mid(pos).stripWhiteSpace();
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

    // Try to findout if this is a rerun and if so the date.
    QRegExp tmpRerun1 = m_comHemRerun1;
    if (tmpRerun1.search(event.description) == -1)
        return;

    QStringList list = tmpRerun1.capturedTexts();
    if (list[1] == "i dag")
    {
        event.originalairdate = event.starttime.date();
        return;
    }

    if (list[1] == "eftermiddagen")
    {
        event.originalairdate = event.starttime.date().addDays(-1);
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
            year = event.starttime.date().year();

        if (day > 0 && month > 0)
        {
            QDate date(event.starttime.date().year(), month, day);
            // it's a rerun so it must be in the past
            if (date > event.starttime.date())
                date = date.addYears(-1);
            event.originalairdate = date;
            //fprintf(stdout, "EITFixUp::FixStyle4: "
            //"OriginalAirDate set to: %s for '%s'\n",
            //event.OriginalAirDate.toString(Qt::ISODate).ascii(),
            //event.description.ascii());
        }
        return;
    }
    // unknown date, posibly only a year.
    //fprintf(stdout,"EITFixUp::FixStyle4: "
    //"unknown rerun date: %s\n",list[1].ascii());
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
