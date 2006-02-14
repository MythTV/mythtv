#include <qregexp.h>

#include "eitfixup.h"

/*------------------------------------------------------------------------
 * Event Fix Up Scripts - Turned on by entry in dtv_privatetype table
 *------------------------------------------------------------------------*/

void EITFixUp::Fix(Event &event, int fix_type)
{
    event.Year = event.StartTime.toString(QString("yyyy"));

    if (event.Description.isEmpty() && !event.Event_Subtitle.isEmpty())
    {
        event.Description    = event.Event_Subtitle;
        event.Event_Subtitle = "";
    }

    switch (fix_type)
    {
        case 1:
            FixStyle1(event);
            break;
        case 2:
            FixStyle2(event);
            break;
        case 3:
            FixStyle3(event);
            break;
        case 4:
            FixStyle4(event);
            break;
        default:
            break;
    }

    event.Event_Name.stripWhiteSpace();
    event.Event_Subtitle.stripWhiteSpace();
    event.Description.stripWhiteSpace();
}

/** \fn EITFixUp::FixStyle1(Event&)
 *  \brief This function does some regexps on Guide data to get
 *         more powerful guide. Use this for BellExpressVu
 *  \TODO deal with events that don't have eventype at the begining?
 */
void EITFixUp::FixStyle1(Event &event)
{
    QString Temp = "";

    int8_t position;

    // A 0x0D character is present between the content
    // and the subtitle if its present
    position = event.Description.find(0x0D);

    if (position != -1)
    {
        // Subtitle present in the title, so get
        // it and adjust the description
        event.Event_Subtitle = event.Description.left(position);
        event.Description = event.Description.right(
            event.Description.length()-position-2);
    }

    // Take out the content description which is
    // always next with a period after it
    position = event.Description.find(".");
    // Make sure they didn't leave it out and
    // you come up with an odd category
    if (position < 10)
    {
       event.ContentDescription = event.Description.left(position);
       event.Description = event.Description.right(
           event.Description.length()-position-2);
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
    position = event.Description.find(QRegExp("[\\(]{1}[0-9]{4}[\\)]{1}"));
    if (position != -1 && event.ContentDescription != "")
    {
       Temp = "";
       // Parse out the year
       event.Year = event.Description.mid(position+1,4);
       // Get the actors if they exist
       if (position > 3)
       {
          Temp = event.Description.left(position-3);
          event.Actors = QStringList::split(QRegExp("\\set\\s|,"), Temp);
       }
       // Remove the year and actors from the description
       event.Description = event.Description.right(
           event.Description.length()-position-7);
    }

    // Check for (CC) in the decription and
    // set the <subtitles type="teletext"> flag
    position = event.Description.find("(CC)");
    if (position != -1)
    {
       event.SubTitled = true;
       event.Description = event.Description.replace("(CC)","");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.Description.find("(Stereo)");
    if (position != -1)
    {
       event.Stereo = true;
       event.Description = event.Description.replace("(Stereo)","");
    }
}

/** \fn EITFixUp::FixStyle2(Event&)
 *  \brief This function does some regexps on Guide data to get
 *         more powerful guide. Use this for the United Kingdom.
 */
void EITFixUp::FixStyle2(Event &event)
{
    const uint SUBTITLE_PCT = 50; //% of description to allow subtitle up to
    int position = event.Description.find("New Series");
    if (position != -1)
    {
        //Do something here
    }
    //BBC three case (could add another record here ?)
    QRegExp rx("\\s*(Then|Followed by) 60 Seconds\\.");
    rx.setCaseSensitive(false);
    event.Description = event.Description.replace(rx,"");

    rx.setPattern("\\s*(Brand New|New) Series\\s*[:\\.\\-]");
    event.Description = event.Description.replace(rx,"");

    rx.setPattern("^[tT]4:");
    event.Event_Name = event.Event_Name.replace(rx,"");

    QRegExp terminatesWith("[\\!\\?]");
    //This is trying to catch the case where the subtitle is in the main title
    //but avoid cases where it isn't a subtitle e.g cd:uk
    if (((position = event.Event_Name.find(":")) != -1) && 
        (event.Description.find(":") == -1) && 
        (event.Event_Name[position+1].upper()==event.Event_Name[position+1]))
    {
        event.Event_Subtitle = event.Event_Name.mid(position+1);
        event.Event_Name = event.Event_Name.left(position);
    }
    else if ((position = event.Description.find(":")) != -1)
    {
        // if the subtitle is less than 50% of the description use it.
        if ((position*100)/event.Description.length() < SUBTITLE_PCT)
        {
            event.Event_Subtitle = event.Description.left(position);
            event.Description = event.Description.mid(position+1);
        }
    }
    else if ((position = event.Description.find(terminatesWith)) != -1)
    {
        if ((position*100)/event.Description.length() < SUBTITLE_PCT)
        {
            event.Event_Subtitle = event.Description.left(position+1);
            event.Description = event.Description.mid(position+2);
        }
    }

    QRegExp endsWith("\\.+$");
    QRegExp startsWith("^\\.+");
    terminatesWith.setPattern("[:\\!\\.\\?]");
    if (event.Event_Name.endsWith("...") && 
        event.Event_Subtitle.startsWith(".."))
    {
        //try and make the subtitle
        QString Full = event.Event_Name.replace(endsWith,"")+" "+
                       event.Event_Subtitle.replace(startsWith,"");

        if ((position = Full.find(terminatesWith)) != -1)
        {
           if (Full[position] == '!' || Full[position] == '?')
               position++;
           event.Event_Name = Full.left(position);
           event.Event_Subtitle = Full.mid(position+1);
        }
        else
        {
           event.Event_Name = Full;
           event.Event_Subtitle="";
        }
    }
    else if (event.Event_Subtitle.endsWith("...") && 
        event.Description.startsWith("..."))
    {
        QString Full = event.Event_Subtitle.replace(endsWith,"")+" "+
                       event.Description.replace(startsWith,"");
        if ((position = Full.find(terminatesWith)) != -1)
        {
           if (Full[position] == '!' || Full[position] == '?')
               position++;
           event.Event_Subtitle = Full.left(position);
           event.Description = Full.mid(position+1);
        }
    }
    else if (event.Event_Name.endsWith("...") &&
        event.Description.startsWith("...") && event.Event_Subtitle.isEmpty())
    {
        QString Full = event.Event_Name.replace(endsWith,"")+" "+ 
                       event.Description.replace(startsWith,"");
        if ((position = Full.find(terminatesWith)) != -1)
        {
           if (Full[position] == '!' || Full[position] == '?')
               position++;
           event.Event_Name = Full.left(position);
           event.Description = Full.mid(position+1);
        }
    }

    //Work out the episode numbers (if any)
    bool series = false;
    rx.setPattern("^\\s*(\\d{1,2})/(\\d{1,2})\\.");
    QRegExp rx1("\\((Part|Pt)\\s+(\\d{1,2})\\s+of\\s+(\\d{1,2})\\)");
    rx1.setCaseSensitive(false);
    if ((position = rx.search(event.Event_Name)) != -1)
    {
        event.PartNumber=rx.cap(1).toUInt();
        event.PartTotal=rx.cap(2).toUInt();
        //Remove from the title
        event.Event_Name=event.Event_Name.mid(position+rx.cap(0).length());
        //but add it to the description
        event.Description+=rx.cap(0);
        series=true;
    }
    else if ((position = rx.search(event.Event_Subtitle)) != -1)
    {
        event.PartNumber=rx.cap(1).toUInt();
        event.PartTotal=rx.cap(2).toUInt();
        //Remove from the sub title
        event.Event_Subtitle=event.Event_Subtitle.mid(position+rx.cap(0).length());
        //but add it to the description
        event.Description+=rx.cap(0);
        series=true;
    }
    else if ((position = rx.search(event.Description)) != -1)
    {
        event.PartNumber=rx.cap(1).toUInt();
        event.PartTotal=rx.cap(2).toUInt();
        //Don't cut it from the description
        //event.Description=event.Description.left(position)+
        //                  event.Description.mid(position+rx.cap(0).length());
        series=true;
    }
    else if ((position = rx1.search(event.Description)) != -1)
    {
        event.PartNumber=rx1.cap(2).toUInt();
        event.PartTotal=rx1.cap(3).toUInt();
        //Don't cut it from the description
        //event.Description=event.Description.left(position)+
        //                  event.Description.mid(position+rx1.cap(0).length());
        series=true;
    }
    if (series)
        event.CategoryType="series";
    //Work out the closed captions and Audio descriptions  (if any)
    rx.setPattern("\\[(AD)(,(S)){,1}(,SL){,1}\\]|"
               "\\[(S)(,AD){,1}(,SL){,1}\\]|"
               "\\[(SL)(,AD){,1}(,(S)){,1}\\]");
    if ((position = rx.search(event.Description)) != -1)
    {
        //Enumerate throught and see if we have subtitles, don't modify
        //the description as we might destroy other useful information
        QStringList captures = rx.capturedTexts();
        QStringList::Iterator i = captures.begin();
        QStringList::Iterator end = captures.end();
        while (i!=end)
            if (*(i++) == "S")
                 event.SubTitled = true;
    }
    else if ((position = rx.search(event.Event_Subtitle)) != -1)
    {
        QStringList captures = rx.capturedTexts();
        QStringList::Iterator i = captures.begin();
        QStringList::Iterator end = captures.end();
        while (i!=end)
            if (*(i++) == "S")
                 event.SubTitled = true;
        //We do remove [AD,S] from the subtitle as it probably shouldn't be
        //there.
        QString Temp = event.Event_Subtitle;
        event.Event_Subtitle = Temp.left(position)+Temp.mid(position+rx.cap(0).length());
    }
    //Work out the year (if any)
    rx.setPattern("[\\[\\(]([\\d]{4})[\\)\\]]");
    if ((position = rx.search(event.Description)) != -1)
    {
        event.Description=event.Description.left(position)+
                          event.Description.mid(position+rx.cap(0).length());
        event.Year=rx.cap(1);
    }

    event.Event_Name = event.Event_Name.stripWhiteSpace();
    event.Event_Subtitle = event.Event_Subtitle.stripWhiteSpace();
    event.Description = event.Description.stripWhiteSpace();
}

/** \fn EITFixUp::FixStyle3(Event&)
 *  \brief This function does some regexps on Guide data to get
 *         more powerful guide. Use this for PBS channels.
 */
void EITFixUp::FixStyle3(Event &event)
{
    /* Used for PBS ATSC Subtitles are seperated by a colon */
    int16_t position = event.Description.find(':');
    if (position != -1)
    {
        event.Event_Subtitle = event.Description.left(position);
        event.Description = event.Description.right(
            event.Description.length()-position-2);
    }
}

/** \fn EITFixUp::FixStyle4(Event&)
 *  \brief This function does some regexps on Guide data to get
 *         more powerful guide. Use this in Sweden (ComHem DVB).
 */
void EITFixUp::FixStyle4(Event &event)
{
    // Used for swedish dvb cable provider ComHem

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
    QRegExp rx("^(.+)\\.\\s(.+)\\.\\s(?:([0-9]{2,4})\\.\\s*)?");
    int pos = rx.search(event.Event_Subtitle);
    if (pos != -1)
    {
        QStringList list = rx.capturedTexts();

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
        event.Event_Subtitle="";
    }

    if (event.Description.length() > 0)
    {
        // everything up to the first 3 spaces is duplicated from title and
        // subtitle so remove it
        int pos = event.Description.find("   ");
        if (pos != -1)
        {
            event.Description =
                event.Description.mid(pos + 3).stripWhiteSpace();
            //fprintf(stdout,"EITFixUp::FixStyle4: New: %s\n",
            //event.Description.mid(pos+3).stripWhiteSpace().ascii());
        }

        // in the description at this point everything up to the first 4 spaces
        // in a row is a list of director(s) and actors.
        // different lists are separated by 3 spaces in a row
        // end of all lists is when there is 4 spaces in a row
        bool bDontRemove=false;
        pos = event.Description.find("    ");
        if (pos != -1)
        {
            QStringList lists;
            lists=QStringList::split("   ",event.Description.left(pos));
            for (QStringList::Iterator it=lists.begin();it!=lists.end();it++)
            {
                QStringList list =
                    QStringList::split(": ",(*it).remove(QRegExp("\\.$")));
                if (list.count() == 2)
                {
                    if (list[0].find(QRegExp("[Rr]egi"))!=-1)
                    {
                        //director(s)
                        QStringList persons = QStringList::split(", ",list[1]);
                        for (QStringList::Iterator it2 = persons.begin();
                             it2 != persons.end(); it2++)
                        {
                            event.Credits.append(Person("director",*it2));
                        }
                    }
                    else if (list[0].find(QRegExp("[Ss]kådespelare"))!=-1)
                    {
                        //actor(s)
                        QStringList persons = QStringList::split(", ",list[1]);
                        for(QStringList::Iterator it2 = persons.begin();
                            it2 != persons.end(); it2++)
                        {
                            event.Credits.append(Person("actor",*it2));
                        }
                    }
                    else
                    {
                        // unknown type, posibly a new one that
                        // this code shoud be updated to handle
                        fprintf(stdout, "EITFixUp::FixStyle4: "
                                "%s is not actor or director\n",
                                list[0].ascii());
                        bDontRemove=true;
                    }
                }
                else
                {
                    //oops, improperly formated list, ignore it
                    //fprintf(stdout,"EITFixUp::FixStyle4:"
                    //"%s is not a properly formated list of persons\n",
                    //(*it).ascii());
                    bDontRemove=true;
                }
            }
            // remove list of persons from description if
            // we coud parse it properly
            if (!bDontRemove)
            {
                event.Description=event.Description.mid(pos).stripWhiteSpace();
            }
        }

        //fprintf(stdout,"EITFixUp::FixStyle4: Number of persons: %d\n",
        //event.Credits.count());

        /*
        a regexp like this coud be used to get the episode number, shoud be at
        the begining of the description
        "^(?:[dD]el|[eE]pisode)\\s([0-9]+)(?:\\s?(?:/|:|av)\\s?([0-9]+))?\\."
        */

        //is this event on a channel we shoud look for a subtitle?
        if (parseSubtitleServiceIDs.contains(event.ServiceID))
        {
            int pos=event.Description.find(QRegExp("[.\?] "));
            if (pos!=-1 && pos<=55 && (event.Description.length()-(pos+2))>0 )
            {
                event.Event_Subtitle = event.Description.left(
                    pos+(event.Description[pos]=='?' ? 1 : 0));
                event.Description = event.Description.mid(pos+2);
            }
        }

        //try to findout if this is a rerun and if so the date.
        QRegExp rx("[Rr]epris\\sfrån\\s([^\\.]+)(?:\\.|$)");
        if (rx.search(event.Description) != -1)
        {
            QStringList list = rx.capturedTexts();
            if (list[1]=="i dag")
            {
                event.OriginalAirDate=event.StartTime.date();
            }
            else if (list[1]=="eftermiddagen")
            {
                event.OriginalAirDate=event.StartTime.date().addDays(-1);
            }
            else
            {
                QRegExp rxdate("([0-9]+)/([0-9]+)(?:\\s-\\s([0-9]{4}))?");
                if (rxdate.search(list[1]) != -1)
                {
                    QStringList datelist = rxdate.capturedTexts();
                    int day=datelist[1].toInt();
                    int month=datelist[2].toInt();
                    int year;
                    if (datelist[3].length() > 0)
                    {
                        year=datelist[3].toInt();
                    }
                    else
                    {
                        year=event.StartTime.date().year();
                    }

                    if (day>0 && month>0)
                    {
                        QDate date(event.StartTime.date().year(),month,day);
                        //it's a rerun so it must be in the past
                        if (date>event.StartTime.date())
                        {
                            date=date.addYears(-1);
                        }
                        event.OriginalAirDate=date;
                        //fprintf(stdout,"EITFixUp::FixStyle4: "
                        //"OriginalAirDate set to: %s for '%s'\n",
                        //event.OriginalAirDate.toString(Qt::ISODate).ascii(),
                        //event.Description.ascii());
                    }
                }
                else
                {
                    //unknown date, posibly only a year.
                    //fprintf(stdout,"EITFixUp::FixStyle4: "
                    //"unknown rerun date: %s\n",list[1].ascii());
                }
            }
        }
    }
}
