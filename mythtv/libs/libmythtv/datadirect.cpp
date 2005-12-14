#include "datadirect.h"
#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "util.h"

#include <unistd.h>
#include <qfile.h>
#include <qstring.h>
#include <cerrno>


#define LOC QString("DataDirect: ")
#define LOC_ERR QString("DataDirect, Error: ")

const QString DataDirectURLS[] =
{
    "http://datadirect.webservices.zap2it.com/tvlistings/xtvdService",
    "http://webservices.technovera.tmsdatadirect.com/"
    "technovera/tvlistings/xtvdService",
};

// XXX Program duration should be stored as seconds, not as a QTime.
//     limited to 24 hours this way.

bool DDStructureParser::startElement(const QString &pnamespaceuri, 
                                     const QString &plocalname, 
                                     const QString &pqname,
                                     const QXmlAttributes &pxmlatts) 
{
    (void)pnamespaceuri;
    (void)plocalname;

    currtagname = pqname;
    if (currtagname == "xtvd") 
    {
        QDateTime from = QDateTime::fromString(pxmlatts.value("from"), 
                                               Qt::ISODate);
        QDateTime to = QDateTime::fromString(pxmlatts.value("to"),Qt::ISODate);
        parent.setActualListingsFrom(from);
        parent.setActualListingsTo(to);
    }   
    else if (currtagname == "station") 
    {
        curr_station.clearValues();
        curr_station.stationid = pxmlatts.value("id");
    }
    else if (currtagname == "lineup") 
    {
        curr_lineup.clearValues();
        curr_lineup.name = pxmlatts.value("name");
        curr_lineup.type = pxmlatts.value("type");
        curr_lineup.device = pxmlatts.value("device");
        curr_lineup.postal = pxmlatts.value("postalCode");
        curr_lineup.lineupid = pxmlatts.value("id");
        curr_lineup.displayname = curr_lineup.name + "-" + curr_lineup.type + 
                                  "-" + curr_lineup.device + "-" + 
                                  curr_lineup.postal + "-" + 
                                  curr_lineup.lineupid;

        if (curr_lineup.lineupid.isEmpty()) 
        {
            curr_lineup.lineupid = curr_lineup.name + curr_lineup.postal + 
                                   curr_lineup.device + curr_lineup.type;
        }
    }
    else if (currtagname == "map") 
    {
        int tmpindex;
        curr_lineupmap.clearValues();
        curr_lineupmap.lineupid = curr_lineup.lineupid;
        curr_lineupmap.stationid = pxmlatts.value("station");
        curr_lineupmap.channel = pxmlatts.value("channel");
        tmpindex = pxmlatts.index("channelMinor"); // for ATSC
        if (tmpindex != -1) 
            curr_lineupmap.channelMinor = pxmlatts.value(tmpindex);
    } 
    else if (currtagname == "schedule") 
    {
        curr_schedule.clearValues();
        curr_schedule.programid = pxmlatts.value("program");
        curr_schedule.stationid = pxmlatts.value("station");

        QString timestr = pxmlatts.value("time");
        QDateTime UTCdt = QDateTime::fromString(timestr, Qt::ISODate);

        curr_schedule.time = MythUTCToLocal(UTCdt);
        QString durstr;

        durstr = pxmlatts.value("duration");
        curr_schedule.duration = QTime(durstr.mid(2, 2).toInt(), 
                                       durstr.mid(5, 2).toInt(), 0, 0);

        curr_schedule.repeat = (pxmlatts.value("repeat") == "true");
        curr_schedule.stereo = (pxmlatts.value("stereo") == "true");
        curr_schedule.subtitled = (pxmlatts.value("subtitled") == "true");
        curr_schedule.hdtv = (pxmlatts.value("hdtv") == "true");
        curr_schedule.closecaptioned = (pxmlatts.value("closeCaptioned") == 
                                                                      "true");
        curr_schedule.tvrating = pxmlatts.value("tvRating");
    }
    else if (currtagname == "part") 
    {
        curr_schedule.partnumber = pxmlatts.value("number").toInt();
        curr_schedule.parttotal = pxmlatts.value("total").toInt();
    }
    else if (currtagname == "program") 
    {
        curr_program.clearValues();
        curr_program.programid = pxmlatts.value("id");
    }
    else if (currtagname == "crew") 
    {
        curr_program.clearValues();
        lastprogramid = pxmlatts.value("program");
    }
    else if (currtagname == "programGenre") 
    {
        curr_genre.clearValues();
        lastprogramid = pxmlatts.value("program");
    }        

    return true;
}                      

bool DDStructureParser::endElement(const QString &pnamespaceuri, 
                                   const QString &plocalname, 
                                   const QString &pqname) 
{
    (void)pnamespaceuri;
    (void)plocalname;

    MSqlQuery query(MSqlQuery::DDCon());

    if (pqname == "station") 
    {
        parent.stations.append(curr_station);

        query.prepare("INSERT INTO dd_station (stationid, callsign, "
                      "stationname, affiliate, fccchannelnumber) "
                      "VALUES(:STATIONID,:CALLSIGN,:STATIONNAME,:AFFILIATE,"
                      ":FCCCHANNELNUMBER);");
        query.bindValue(":STATIONID", curr_station.stationid);
        query.bindValue(":CALLSIGN", curr_station.callsign);
        query.bindValue(":STATIONNAME", curr_station.stationname);
        query.bindValue(":AFFILIATE", curr_station.affiliate);
        query.bindValue(":FCCCHANNELNUMBER", curr_station.fccchannelnumber);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_station", query);
    }
    else if (pqname == "lineup") 
    {
        parent.lineups.append(curr_lineup);

        query.prepare("INSERT INTO dd_lineup (lineupid, name, type, device, "
                      "postal) VALUES(:LINEUPID,:NAME,:TYPE,:DEVICE,:POSTAL);");
        query.bindValue(":LINEUPID", curr_lineup.lineupid);
        query.bindValue(":NAME", curr_lineup.name);
        query.bindValue(":TYPE", curr_lineup.type);
        query.bindValue(":DEVICE", curr_lineup.device);
        query.bindValue(":POSTAL", curr_lineup.postal);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_lineup", query);
    }
    else if (pqname == "map") 
    {
        parent.lineupmaps.append(curr_lineupmap);

        query.prepare("INSERT INTO dd_lineupmap (lineupid, stationid, "
                      "channel, channelMinor) "
                      "VALUES(:LINEUPID,:STATIONID,:CHANNEL,:CHANNELMINOR);");
        query.bindValue(":LINEUPID", curr_lineupmap.lineupid);
        query.bindValue(":STATIONID", curr_lineupmap.stationid);
        query.bindValue(":CHANNEL", curr_lineupmap.channel);
        query.bindValue(":CHANNELMINOR", curr_lineupmap.channelMinor);
        if (!query.exec())
            MythContext::DBError("Inserting into dd_lineupmap", query);
    }
    else if (pqname == "schedule") 
    {
        QDateTime endtime = curr_schedule.time.addSecs(
            QTime().secsTo(curr_schedule.duration));

        query.prepare("INSERT INTO dd_schedule (programid,stationid,"
                      "scheduletime,duration,isrepeat,stereo,subtitled,hdtv,"
                      "closecaptioned,tvrating,partnumber,parttotal,endtime) "
                      "VALUES(:PROGRAMID,:STATIONID,:TIME,:DURATION,"
                      ":ISREPEAT,:STEREO,:SUBTITLED,:HDTV,:CLOSECAPTIONED,"
                      ":TVRATING,:PARTNUMBER,:PARTTOTAL,:ENDTIME);");
        query.bindValue(":PROGRAMID", curr_schedule.programid);
        query.bindValue(":STATIONID", curr_schedule.stationid);
        query.bindValue(":TIME", curr_schedule.time);
        query.bindValue(":DURATION", curr_schedule.duration);
        query.bindValue(":ISREPEAT", curr_schedule.repeat);
        query.bindValue(":STEREO", curr_schedule.stereo);
        query.bindValue(":SUBTITLED", curr_schedule.subtitled);
        query.bindValue(":HDTV", curr_schedule.hdtv);
        query.bindValue(":CLOSECAPTIONED", curr_schedule.closecaptioned);
        query.bindValue(":TVRATING", curr_schedule.tvrating);
        query.bindValue(":PARTNUMBER", curr_schedule.partnumber);
        query.bindValue(":PARTTOTAL", curr_schedule.parttotal);
        query.bindValue(":ENDTIME", endtime);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_schedule", query);
    }
    else if (pqname == "program") 
    {
        float staravg = 0.0;
        if (!curr_program.starRating.isEmpty()) 
        {
            int fullstarcount = curr_program.starRating.contains("*");
            int halfstarcount = curr_program.starRating.contains("+");
            staravg = (fullstarcount + (halfstarcount * .5)) / 4;
        }

        QString cat_type = "";
        QString prefix = curr_program.programid.left(2);

        if (prefix == "MV")
           cat_type = "movie";
        else if (prefix == "SP")
           cat_type = "sports";
        else if (prefix == "EP" ||
                 curr_program.showtype.contains("series", false))
           cat_type = "series";
        else
           cat_type = "tvshow";

        query.prepare("INSERT INTO dd_program (programid, title, subtitle, "
                      "description, showtype, category_type, mpaarating, "
                      "starrating, stars, runtime, year, seriesid, colorcode, "
                      "syndicatedepisodenumber, originalairdate) "
                      "VALUES(:PROGRAMID,:TITLE,:SUBTITLE,:DESCRIPTION,"
                      ":SHOWTYPE,:CATTYPE,:MPAARATING,:STARRATING,:STARS,"
                      ":RUNTIME,:YEAR,:SERIESID,:COLORCODE,:SYNDNUM,"
                      ":ORIGINALAIRDATE);");
        query.bindValue(":PROGRAMID", curr_program.programid);
        query.bindValue(":TITLE", curr_program.title.utf8());
        query.bindValue(":SUBTITLE", curr_program.subtitle.utf8());
        query.bindValue(":DESCRIPTION", curr_program.description.utf8());
        query.bindValue(":SHOWTYPE", curr_program.showtype.utf8()); 
        query.bindValue(":CATTYPE", cat_type);
        query.bindValue(":MPAARATING", curr_program.mpaaRating);
        query.bindValue(":STARRATING", curr_program.starRating);
        query.bindValue(":STARS", staravg);
        query.bindValue(":RUNTIME", curr_program.duration);
        query.bindValue(":YEAR", curr_program.year);
        query.bindValue(":SERIESID", curr_program.seriesid);
        query.bindValue(":COLORCODE", curr_program.colorcode);
        query.bindValue(":SYNDNUM", curr_program.syndicatedEpisodeNumber);
        query.bindValue(":ORIGINALAIRDATE", curr_program.originalAirDate);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_program", query);
    }    
    else if (pqname == "member") 
    {
        QString roleunderlines = curr_productioncrew.role.replace(" ", "_");

        QString fullname = curr_productioncrew.givenname;
        if (!fullname.isEmpty())
            fullname += " ";
        fullname += curr_productioncrew.surname;

        query.prepare("INSERT INTO dd_productioncrew (programid, role, "
                      "givenname, surname, fullname) VALUES(:PROGRAMID,"
                      ":ROLE,:GIVENNAME,:SURNAME,:FULLNAME);");
        query.bindValue(":PROGRAMID", lastprogramid);
        query.bindValue(":ROLE", roleunderlines.utf8());
        query.bindValue(":GIVENNAME", curr_productioncrew.givenname.utf8());
        query.bindValue(":SURNAME", curr_productioncrew.surname.utf8());
        query.bindValue(":FULLNAME", fullname.utf8());

        if (!query.exec())
            MythContext::DBError("Inserting into dd_productioncrew", query);
    }    
    else if (pqname == "genre") 
    {
        query.prepare("INSERT INTO dd_genre (programid, class, relevance) "
                      "VALUES(:PROGRAMID,:CLASS,:RELEVANCE);");
        query.bindValue(":PROGRAMID", lastprogramid);
        query.bindValue(":CLASS", curr_genre.gclass.utf8());
        query.bindValue(":RELEVANCE", curr_genre.relevance);

        if (!query.exec())
            MythContext::DBError("Inserting into dd_genre", query);
    }

    return true;
} 
 
bool DDStructureParser::startDocument() 
{
    parent.createTempTables();
    return true;
}

bool DDStructureParser::endDocument() 
{
    return true;
}
 
bool DDStructureParser::characters(const QString& pchars) 
{
    // cerr << "Characters : " << pchars << "\n";
    if (pchars.stripWhiteSpace().isEmpty())
        return true;

    if (currtagname == "message")
    {
        if (pchars.contains("expire"))
        {
           QString ExtractDateFromMessage = pchars.right(20);
           QDateTime EDFM = QDateTime::fromString(ExtractDateFromMessage, Qt::ISODate);
           QString ExpirationDate = EDFM.toString(Qt::LocalDate);
           QString ExpirationDateMessage = "Your subscription expires on " + ExpirationDate;

           QDateTime curTime = QDateTime::currentDateTime();
           if (curTime.daysTo(EDFM) <= 5)
               VERBOSE(VB_ALL, LOC + QString("WARNING: ") + ExpirationDateMessage);
           else
               VERBOSE(VB_IMPORTANT, LOC + ExpirationDateMessage);

           MSqlQuery query(MSqlQuery::DDCon());

           QString querystr = (QString("UPDATE settings SET data ='%1' "
                              "WHERE value='DataDirectMessage'")
                              .arg(ExpirationDateMessage));
           // cout << "querystr is:" << querystr << endl;
           query.prepare(querystr);

           if (!query.exec())
               MythContext::DBError("Updating DataDirect Status Message", query);
        }
    }
    if (currtagname == "callSign") 
        curr_station.callsign = pchars;
    else if (currtagname == "name")
        curr_station.stationname = pchars;
    else if (currtagname == "affiliate")
        curr_station.affiliate = pchars;
    else if (currtagname == "fccChannelNumber")
        curr_station.fccchannelnumber = pchars;
    else if (currtagname == "title")
        curr_program.title = pchars;
    else if (currtagname == "subtitle")
        curr_program.subtitle = pchars;
    else if (currtagname == "description")
        curr_program.description = pchars;
    else if (currtagname == "showType")
        curr_program.showtype = pchars;
    else if (currtagname == "series")
        curr_program.seriesid = pchars;
    else if (currtagname == "colorCode")
        curr_program.colorcode = pchars;
    else if (currtagname == "mpaaRating")
        curr_program.mpaaRating = pchars;
    else if (currtagname == "starRating")
        curr_program.starRating = pchars;
    else if (currtagname == "year")
        curr_program.year = pchars;
    else if (currtagname == "syndicatedEpisodeNumber")
        curr_program.syndicatedEpisodeNumber = pchars; 
    else if (currtagname == "runTime") 
    {
        QString runtimestr = pchars;
        QTime runtime = QTime(runtimestr.mid(2,2).toInt(),
                              runtimestr.mid(5,2).toInt(), 0, 0);
        curr_program.duration = runtime;
    }
    else if (currtagname == "originalAirDate") 
    {
        QDate airdate = QDate::fromString(pchars, Qt::ISODate);
        curr_program.originalAirDate = airdate;
    }
    else if (currtagname == "role")
        curr_productioncrew.role = pchars;
    else if (currtagname == "givenname")
        curr_productioncrew.givenname = pchars;
    else if (currtagname == "surname")
        curr_productioncrew.surname = pchars;  
    else if (currtagname == "class") 
        curr_genre.gclass = pchars;
    else if (currtagname == "relevance") 
        curr_genre.relevance = pchars;

    return true;
}

void DataDirectProcessor::updateStationViewTable() 
{
    MSqlQuery query(MSqlQuery::DDCon());
   
    if (!query.exec("TRUNCATE TABLE dd_v_station;")) 
        MythContext::DBError("Truncating temporary table dd_v_station", query);

    query.prepare("INSERT INTO dd_v_station (stationid, callsign, "
                  "    stationname, affiliate, fccchannelnumber, channel, "
                  "    channelMinor) "
                  "SELECT dd_station.stationid, callsign, stationname, "
                  "    affiliate, fccchannelnumber, channel, channelMinor "
                  "FROM dd_station, dd_lineupmap WHERE "
                  "    ( (dd_station.stationid = dd_lineupmap.stationid) AND "
                  "    ( dd_lineupmap.lineupid = :LINEUP) );");
    query.bindValue(":LINEUP", getLineup());

    if (!query.exec())
        MythContext::DBError("Populating temporary table dd_v_station", query);
}

void DataDirectProcessor::updateProgramViewTable(int sourceid) 
{
    MSqlQuery query(MSqlQuery::DDCon());
   
    if (!query.exec("TRUNCATE TABLE dd_v_program;"))
        MythContext::DBError("Truncating temporary table dd_v_program", query);

    query.prepare("INSERT INTO dd_v_program (chanid, starttime, endtime, "
                  "title, subtitle, description, airdate, stars, "
                  "previouslyshown, stereo, subtitled, hdtv, "
                  "closecaptioned, partnumber, parttotal, seriesid, "
                  "originalairdate, showtype, category_type, colorcode, "
                  "syndicatedepisodenumber, tvrating, mpaarating, "
                  "programid) "
                  "SELECT chanid, scheduletime, endtime, title, "
                  "subtitle, description, year, stars, isrepeat, stereo, "
                  "subtitled, hdtv, closecaptioned, partnumber, "
                  "parttotal, seriesid, originalairdate, showtype, "
                  "category_type, colorcode, syndicatedepisodenumber, "
                  "tvrating, mpaarating, dd_program.programid "
                  "FROM channel, dd_schedule, dd_program WHERE "
                  " ( (dd_schedule.programid = dd_program.programid) AND "
                  "   (channel.xmltvid = dd_schedule.stationid) AND "
                  "   (channel.sourceid = :SOURCEID ));");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
        MythContext::DBError("Populating temporary table dd_v_program", query);

    if (!query.exec("ANALYZE TABLE dd_v_program;"))
        MythContext::DBError("Analyzing table dd_v_program", query);

    if (!query.exec("ANALYZE TABLE dd_productioncrew;"))
        MythContext::DBError("Analyzing table dd_productioncrew", query);
}

void DataDirectProcessor::setInputFile(const QString &filename)
{
    inputfilename = filename;
}

FILE *DataDirectProcessor::getInputFile(bool plineupsOnly, QDateTime pstartDate,
            QDateTime pendDate, QString &err_txt, QString &tmpfilename)
{
    FILE *ret = NULL;
    if (inputfilename.isNull())
    {   
        //QString ddurl("http://datadirect.webservices.zap2it.com/tvlistings/xtvdService");
        QString ddurl(DataDirectURLS[source]);
         
        if (plineupsOnly) 
        {
            pstartDate = QDateTime(QDate::currentDate().addDays(2),
                                   QTime::QTime(23, 59, 0));
            pendDate = pstartDate;
            pendDate = pendDate.addSecs(1);
        }

        QString startdatestr = pstartDate.toString(Qt::ISODate) + "Z";
        QString enddatestr = pendDate.toString(Qt::ISODate) + "Z";

        char ctempfilename[] = "/tmp/mythpostXXXXXX";
        if (mkstemp(ctempfilename) == -1) 
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Creating temp files -- %1").
                    arg(strerror(errno)));
            return NULL;
        }

        tmpfilename = QString(ctempfilename);
        QFile postfile(tmpfilename);

        if (postfile.open(IO_WriteOnly)) 
        {
            QTextStream poststream(&postfile);
            poststream << "<?xml version='1.0' encoding='utf-8'?>\n";
            poststream << "<SOAP-ENV:Envelope\n";
            poststream << "xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'\n";
            poststream << "xmlns:xsd='http://www.w3.org/2001/XMLSchema'\n";
            poststream << "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n";
            poststream << "xmlns:SOAP-ENC='http://schemas.xmlsoap.org/soap/encoding/'>\n";
            poststream << "<SOAP-ENV:Body>\n";
            poststream << "<ns1:download  xmlns:ns1='urn:TMSWebServices'>\n";
            poststream << "<startTime xsi:type='xsd:dateTime'>";
            poststream << startdatestr << "</startTime>\n";
            poststream << "<endTime xsi:type='xsd:dateTime'>";
            poststream << enddatestr << "</endTime>\n";
            poststream << "</ns1:download>\n";
            poststream << "</SOAP-ENV:Body>\n";
            poststream << "</SOAP-ENV:Envelope>\n";
        }
        else
        {
            err_txt = "Unable to open post data output file.";
            return NULL;
        }

        postfile.close();

        QString command = QString("wget --http-user='%1' --http-passwd='%2' "
                                  "--post-file='%3' --header='Accept-Encoding:gzip'"
                                  " %4 --output-document=- ")
                                 .arg(getUserID())
                                 .arg(getPassword())
                                 .arg(tmpfilename)
                                 .arg(ddurl);

        if ((print_verbose_messages & VB_GENERAL) == 0)
            command += " 2> /dev/null ";

        command += " | gzip -df";

        err_txt = command;

        ret = popen(command.ascii(), "r");
    }
    else
    {
        err_txt = inputfilename;
        ret = fopen(inputfilename.ascii(), "r");
    }

    return ret;
}

bool DataDirectProcessor::getNextSuggestedTime(void)
{
    QString ddurl(DataDirectURLS[source]);
         
    char ctempfilenamesend[] = "/tmp/mythpostXXXXXX";
    if (mkstemp(ctempfilenamesend) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Creating temp file for sending -- %1").
                arg(strerror(errno)));
        return FALSE;
    }
    QString tmpfilenamesend = QString(ctempfilenamesend);

    char ctempfilenamerecv[] = "/tmp/mythpostXXXXXX";
    if (mkstemp(ctempfilenamerecv) == -1) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Creating temp file for receiving -- %1").
                arg(strerror(errno)));
        return FALSE;
    }
    QString tmpfilenamereceive = QString(ctempfilenamerecv);

    QFile postfile(tmpfilenamesend);
    if (postfile.open(IO_WriteOnly)) 
    {
        QTextStream poststream(&postfile);
        poststream << "<?xml version='1.0' encoding='utf-8'?>\n";
        poststream << "<SOAP-ENV:Envelope\n";
        poststream << "xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'\n";
        poststream << "xmlns:xsd='http://www.w3.org/2001/XMLSchema'\n";
        poststream << "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n";
        poststream << "xmlns:SOAP-ENC='http://schemas.xmlsoap.org/soap/encoding/'>\n";
        poststream << "<SOAP-ENV:Body>\n";
        poststream << "<tms:acknowledge xmlns:tms='urn:TMSWebServices'>\n";
        poststream << "</SOAP-ENV:Body>\n";
        poststream << "</SOAP-ENV:Envelope>\n";
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Creating tmpfilesend -- %1").
                arg(strerror(errno)));
        return FALSE;
    }
    postfile.close();

    QString command = QString("wget --http-user='%1' --http-passwd='%2' "
                              "--post-file='%3' "
                              " %4 --output-document='%5'")
                              .arg(getUserID())
                              .arg(getPassword())
                              .arg(tmpfilenamesend)
                              .arg(ddurl)
                              .arg(tmpfilenamereceive);

    myth_system(command.ascii());

    QDateTime NextSuggestedTime;

    QFile file(tmpfilenamereceive);

    bool GotNextSuggestedTime = FALSE;

    if (file.open(IO_ReadOnly)) 
    {
        QTextStream stream(&file);
        QString line;
        while (!stream.atEnd()) 
        {
            line = stream.readLine();
            if (line.startsWith("<suggestedTime>"))
            {
                GotNextSuggestedTime = TRUE;
                QDateTime UTCdt = QDateTime::fromString(line.mid(15,20),
                                                        Qt::ISODate);
                NextSuggestedTime = MythUTCToLocal(UTCdt);
                VERBOSE(VB_GENERAL, LOC + QString("NextSuggestedTime is: ") 
                        + NextSuggestedTime.toString(Qt::ISODate));
            }
        }
        file.close();
    }
    unlink(tmpfilenamesend.ascii());
    unlink(tmpfilenamereceive.ascii());

    if (GotNextSuggestedTime)
    {
        int daysToSuggested =
                QDateTime::currentDateTime().daysTo(NextSuggestedTime);
        int desiredPeriod = gContext->GetNumSetting("MythFillPeriod", 1);


        if (daysToSuggested != desiredPeriod)
        {
            QDateTime newTime =
                NextSuggestedTime.addDays(desiredPeriod - daysToSuggested);
            VERBOSE(VB_IMPORTANT, LOC + QString("Provider suggested running "
                    "again at %1, but MythFillPeriod is %2.  Next run time "
                    "will be adjusted to be %3.")
                    .arg(NextSuggestedTime.toString(Qt::ISODate))
                    .arg(desiredPeriod)
                    .arg(newTime.toString(Qt::ISODate)));
            NextSuggestedTime = newTime;
        }

        int minhr = NextSuggestedTime.toString("h").toInt();
        int maxhr = NextSuggestedTime.addSecs(7200).toString("h").toInt();

        if (maxhr < minhr)
        {
            minhr = 22;
            maxhr = 24;
        }

        MSqlQuery query(MSqlQuery::DDCon());
        QString querystr =
            QString("UPDATE settings SET data = '%1' WHERE value = '%2';");

        query.prepare(querystr.arg(minhr).arg("MythFillMinHour"));
        if (!query.exec())
            MythContext::DBError("Updating DataDirect MythFillMinHour", query);

        query.prepare(querystr.arg(maxhr).arg("MythFillMaxHour"));
        if (!query.exec())
            MythContext::DBError("Updating DataDirect MythFillMaxHour", query);

        query.prepare(querystr.arg(NextSuggestedTime.toString(Qt::ISODate))
                              .arg("MythFillSuggestedRunTime"));
        if (!query.exec())
            MythContext::DBError("Updating DataDirect Suggested RunTime", query);
    }
    return GotNextSuggestedTime;
}

bool DataDirectProcessor::grabData(bool plineupsOnly, QDateTime pstartDate, 
                                   QDateTime pendDate) 
{
    QString ferror;
    QString tempfile;
    FILE *fp = getInputFile(plineupsOnly, pstartDate, pendDate, ferror,
                            tempfile);

    if (fp == NULL) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to get data (%1) -- %2").
                arg(ferror.ascii()).arg(strerror(errno)));
        return false;
    }

    QFile f;

    if (!f.open(IO_ReadOnly, fp)) 
    {
       VERBOSE(VB_GENERAL, LOC_ERR + "Error opening DataDirect file");
       return false;
    }
    
    DDStructureParser ddhandler(*this);

    QXmlInputSource xmlsource(&f);
    QXmlSimpleReader xmlsimplereader;
    xmlsimplereader.setContentHandler(&ddhandler);
    xmlsimplereader.parse(xmlsource);
    f.close();

    if (!tempfile.isNull())
    {
        QFile tmpfile(tempfile);
        tmpfile.remove();
    }
    return true;
}

bool DataDirectProcessor::grabLineupsOnly() 
{
    bool ok = true;
    if ((lastrunuserid != getUserID()) || (lastrunpassword != getPassword())) 
    {
        lastrunuserid = getUserID();
        lastrunpassword = getPassword();
        ok = grabData(true, QDateTime::currentDateTime(),
                      QDateTime::currentDateTime());
    }
    return ok;
}   

bool DataDirectProcessor::grabAllData() 
{
    return grabData(false, QDateTime(QDate::currentDate()).addDays(-2),
                    QDateTime(QDate::currentDate()).addDays(15));
}

void DataDirectProcessor::createATempTable(const QString &ptablename, 
                                           const QString &ptablestruct) 
{
    MSqlQuery query(MSqlQuery::DDCon());
    QString querystr;
    querystr = "CREATE TEMPORARY TABLE IF NOT EXISTS " + ptablename + " " + 
               ptablestruct + ";";

    if (!query.exec(querystr))
        MythContext::DBError("Creating temporary table", query);

    querystr = "TRUNCATE TABLE " + ptablename + ";";

    if (!query.exec(querystr))
        MythContext::DBError("Truncating temporary table", query);
}      

void DataDirectProcessor::createTempTables() 
{
    QString table;

    table = "( stationid char(12), callsign char(10), stationname varchar(40), "
            "affiliate varchar(25), fccchannelnumber char(15) )";
    createATempTable("dd_station", table);

    table = "( lineupid char(100), name char(42), type char(20), "
            "postal char(6), device char(30) )";
    createATempTable("dd_lineup", table);  

    table = "( lineupid char(100), stationid char(12), channel char(5), "
        "channelMinor char(3) )";
    createATempTable("dd_lineupmap", table);

    table = "( stationid char(12), callsign char(10), stationname varchar(40), "
            "affiliate varchar(25), fccchannelnumber char(15), "
            "channel char(5), channelMinor char(3) )"; 
    createATempTable ("dd_v_station", table);

    table = "( programid char(12), stationid char(12), scheduletime datetime, "
            "duration time, isrepeat bool, stereo bool, subtitled bool, "
            "hdtv bool, closecaptioned bool, tvrating char(5), partnumber int, "
            "parttotal int, endtime datetime, INDEX progidx (programid) )";
    createATempTable("dd_schedule", table); 

    table = "( programid char(12) NOT NULL, seriesid char(12), "
            "title varchar(120), subtitle varchar(150), description text, "
            "mpaarating char(5), starrating char(5), runtime time, "
            "year char(4), showtype char(30), category_type char(64), "
            "colorcode char(20), originalairdate date, "
            "syndicatedepisodenumber char(20), stars float unsigned, "
            "PRIMARY KEY (programid))";
    createATempTable("dd_program", table); 

    table = "( chanid int unsigned NOT NULL, starttime datetime NOT NULL, "
            "endtime datetime, title varchar(128), subtitle varchar(128), "
            "description text, category varchar(64), "
            "category_type varchar(64), airdate year, stars float unsigned, "
            "previouslyshown tinyint, isrepeat bool, stereo bool, "
            "subtitled bool, hdtv bool,  closecaptioned bool, partnumber int, "
            "parttotal int, seriesid char(12), originalairdate date, "
            "showtype varchar(30), colorcode varchar(20), "
            "syndicatedepisodenumber varchar(20), programid char(12), "
            "tvrating char(5), mpaarating char(5), INDEX progidx (programid))";
    createATempTable("dd_v_program", table);

    table = "( programid char(12), role char(30), givenname char(20), "
            "surname char(20), fullname char(41), INDEX progidx (programid), "
            "INDEX nameidx (fullname))";
    createATempTable("dd_productioncrew", table);  

    table = "( programid char(12) NOT NULL, class char(30), "
            "relevance char(1), INDEX progidx (programid))";
    createATempTable("dd_genre", table);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
