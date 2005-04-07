#include "httpstatus.h"

#include <iostream>
using namespace std;

#include <qregexp.h>
#include <qstringlist.h>
#include <qtextstream.h>

#include "mainserver.h"
#include "libmyth/mythcontext.h"
#include "jobqueue.h"


HttpStatus::HttpStatus(MainServer *parent, int port)
          : QServerSocket(port, 1)
{
    m_parent = parent;

    if (!ok())
    { 
        cerr << "Failed to bind to port " << port << endl;
        exit(0);
    }
}

void HttpStatus::newConnection(int socket)
{
    QSocket *s = new QSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    s->setSocket(socket);
}

void HttpStatus::readClient(void)
{
    QSocket *socket = (QSocket *)sender();
    if (!socket)
        return;

    if (socket->canReadLine()) 
    {
        QStringList tokens = QStringList::split(QRegExp("[ \r\n][ \r\n]*"), 
                                                socket->readLine());
        if (tokens[0] == "GET") 
        {
            QDomDocument doc( "Status" );
                      
            m_parent->FillStatusXML( &doc );
            
            if (tokens[1] == "/xml")
            {
                QTextStream os(socket);

                os.setEncoding(QTextStream::UnicodeUTF8);

                os << "HTTP/1.0 200 Ok\r\n"
                   << "Content-Type: text/xml; charset=\"UTF-8\"\r\n"
                   << "\r\n"
                   << doc.toString();
            }
            else
                PrintStatus( socket, &doc );

            socket->close();
        }
    }
}

void HttpStatus::discardClient(void)
{
    QSocket *socket = (QSocket *)sender();
    if (!socket)
        return;
    delete socket;
}

void HttpStatus::PrintStatus(QSocket *socket, QDomDocument *pDoc )
{

    QTextStream os(socket);
    QString shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");

    os.setEncoding(QTextStream::UnicodeUTF8);

    QDateTime qdtNow = QDateTime::currentDateTime();

    QDomElement docElem = pDoc->documentElement();

    os << "HTTP/1.0 200 Ok\r\n"
       << "Content-Type: text/html; charset=\"UTF-8\"\r\n"
       << "\r\n";

    os << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
       << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
       << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
       << " xml:lang=\"en\" lang=\"en\">\r\n"
       << "<head>\r\n"
       << "  <meta http-equiv=\"Content-Type\""
       << "content=\"text/html; charset=UTF-8\" />\r\n"
       << "  <style type=\"text/css\" title=\"Default\" media=\"all\">\r\n"
       << "  <!--\r\n"
       << "  body {\r\n"
       << "    background-color:#fff;\r\n"
       << "    font:11px verdana, arial, helvetica, sans-serif;\r\n"
       << "    margin:20px;\r\n"
       << "  }\r\n"
       << "  h1 {\r\n"
       << "    font-size:28px;\r\n"
       << "    font-weight:900;\r\n"
       << "    color:#ccc;\r\n"
       << "    letter-spacing:0.5em;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    width:650px;\r\n"
       << "    text-align:center;\r\n"
       << "  }\r\n"
       << "  h2 {\r\n"
       << "    font-size:18px;\r\n"
       << "    font-weight:800;\r\n"
       << "    color:#360;\r\n"
       << "    border:none;\r\n"
       << "    letter-spacing:0.3em;\r\n"
       << "    padding:0px;\r\n"
       << "    margin-bottom:10px;\r\n"
       << "    margin-top:0px;\r\n"
       << "  }\r\n"
       << "  hr {\r\n"
       << "    display:none;\r\n"
       << "  }\r\n"
       << "  div.content {\r\n"
       << "    width:650px;\r\n"
       << "    border-top:1px solid #000;\r\n"
       << "    border-right:1px solid #000;\r\n"
       << "    border-bottom:1px solid #000;\r\n"
       << "    border-left:10px solid #000;\r\n"
       << "    padding:10px;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    -moz-border-radius:8px 0px 0px 8px;\r\n"
       << "  }\r\n"
       << "  div#schedule a {\r\n"
       << "    display:block;\r\n"
       << "    color:#000;\r\n"
       << "    text-decoration:none;\r\n"
       << "    padding:.2em .8em;\r\n"
       << "    border:thin solid #fff;\r\n"
       << "    width:350px;\r\n"
       << "  }\r\n"
       << "  div#schedule a span {\r\n"
       << "    display:none;\r\n"
       << "  }\r\n"
       << "  div#schedule a:hover {\r\n"
       << "    background-color:#F4F4F4;\r\n"
       << "    border-top:thin solid #000;\r\n"
       << "    border-bottom:thin solid #000;\r\n"
       << "    border-left:thin solid #000;\r\n"
       << "    cursor:default;\r\n"
       << "  }\r\n"
       << "  div#schedule a:hover span {\r\n"
       << "    display:block;\r\n"
       << "    position:absolute;\r\n"
       << "    background-color:#F4F4F4;\r\n"
       << "    color:#000;\r\n"
       << "    left:400px;\r\n"
       << "    margin-top:-20px;\r\n"
       << "    width:280px;\r\n"
       << "    padding:5px;\r\n"
       << "    border:thin dashed #000;\r\n"
       << "  }\r\n"
       << "  div.diskstatus {\r\n"
       << "    width:325px;\r\n"
       << "    height:7em;\r\n"
       << "    float:left;\r\n"
       << "  }\r\n"
       << "  div.loadstatus {\r\n"
       << "    width:325px;\r\n"
       << "    height:7em;\r\n"
       << "    float:right;\r\n"
       << "  }\r\n"
       << "  .jobfinished { color: #0000ff; }\r\n"
       << "  .jobaborted { color: #7f0000; }\r\n"
       << "  .joberrored { color: #ff0000; }\r\n"
       << "  .jobrunning { color: #005f00; }\r\n"
       << "  .jobqueued  {  }\r\n"
       << "  -->\r\n"
       << "  </style>\r\n"
       << "  <title>MythTV Status - " 
       << docElem.attribute( "date", qdtNow.toString(shortdateformat)  )
       << " " 
       << docElem.attribute( "time", qdtNow.toString(timeformat) ) << " - "
       << docElem.attribute( "version", MYTH_BINARY_VERSION ) << "</title>\r\n"
       << "</head>\r\n"
       << "<body>\r\n\r\n"
       << "  <h1>MythTV Status</h1>\r\n";

    int nNumEncoders = 0;

    // encoder information ---------------------

    QDomNode node = docElem.namedItem( "Encoders" );

    if (!node.isNull())
        nNumEncoders = PrintEncoderStatus( os, node.toElement() );

    // upcoming shows --------------------------

    node = docElem.namedItem( "Scheduled" );

    if (!node.isNull())
        PrintScheduled( os, node.toElement());

    // Job Queue Entries -----------------------

    node = docElem.namedItem( "JobQueue" );

    if (!node.isNull())
        PrintJobQueue( os, node.toElement());


    // Machine information ---------------------

    node = docElem.namedItem( "MachineInfo" );

    if (!node.isNull())
        PrintMachineInfo( os, node.toElement());

#if USING_DVB
    // DVB Signal Information ------------------

    node = docElem.namedItem( "DVBStatus" );

    if (!node.isNull())
        PrintDVBStatus( os, node.toElement());
#endif

    os << "\r\n</body>\r\n</html>\r\n";
}

QDateTime HttpStatus::GetDateTime( QString sDate )
{
    QDateTime retDate;

    if (!sDate.isNull() && !sDate.isEmpty())
    {
        retDate.setTime_t( (uint)atoi( sDate.ascii() ));
    }

    return( retDate );
}

int HttpStatus::PrintEncoderStatus( QTextStream &os, QDomElement encoders )
{
    QString timeformat   = gContext->GetSetting("TimeFormat", "h:mm AP");
    int     nNumEncoders = 0;

    if (encoders.isNull())
        return 0;

    os << "  <div class=\"content\">\r\n"
       << "    <h2>Encoder status</h2>\r\n";

    QDomNode node = encoders.firstChild();

    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            if (e.tagName() == "Encoder")
            {
                QString sIsLocal  = (e.attribute( "local"    , "remote" )== "1")
                                                           ? "local" : "remote";
                QString sCardId   =  e.attribute( "id"       , "0"      );
                QString sHostName =  e.attribute( "hostname" , "Unknown");
                bool    bConnected=  e.attribute( "connected", "0"      ).toInt();

                bool bIsLowOnFreeSpace=e.attribute( "lowOnFreeSpace", "0").toInt();
                                     
                os << "    Encoder " << sCardId << " is " << sIsLocal 
                   << " on " << sHostName;

                if ((sIsLocal == "remote") && !bConnected)
                {
                    os << " (currently not connected).<br />";

                    node = node.nextSibling();
                    continue;
                }

                nNumEncoders++;

                TVState encState = (TVState) e.attribute( "state", "0").toInt();

                switch( encState )
                {   
                    case kState_WatchingLiveTV:
                        os << " and is watching Live TV";
                        break;

                    case kState_RecordingOnly:
                    case kState_WatchingRecording:
                        os << " and is recording";
                        break;

                    default:
                        os << " and is not recording.";
                        break;
                }

                // Display first Program Element listed under the encoder

                QDomNode tmpNode = e.namedItem( "Program" );

                if (!tmpNode.isNull())
                {
                    QDomElement program  = tmpNode.toElement();  

                    if (!program.isNull())
                    {
                        os << ": '" << program.attribute( "title", "Unknown" ) << "'";

                        // Get Channel information
                        
                        tmpNode = program.namedItem( "Channel" );

                        if (!tmpNode.isNull())
                        {
                            QDomElement channel = tmpNode.toElement();

                            if (!channel.isNull())
                                os <<  " on "  
                                   << channel.attribute( "callSign", "unknown" );
                        }

                        // Get Recording Information (if any)
                        
                        tmpNode = program.namedItem( "Recording" );

                        if (!tmpNode.isNull())
                        {
                            QDomElement recording = tmpNode.toElement();

                            if (!recording.isNull())
                            {
                                QDateTime endTs = GetDateTime( recording.attribute( "recEndTs", "" ));

                                os << ". This recording will end "
                                   << "at " << endTs.toString(timeformat);
                            }
                        }
                    }

                    os << ".";
                }

                if (bIsLowOnFreeSpace)
                {
                    os << " <strong>WARNING</strong>:"
                       << " This backend is low on free disk space!";
                }

                os << "<br />\r\n";
            }
        }

        node = node.nextSibling();
    }

    os << "  </div>\r\n\r\n";

    return( nNumEncoders );
}

int HttpStatus::PrintScheduled( QTextStream &os, QDomElement scheduled )
{
    QDateTime qdtNow          = QDateTime::currentDateTime();
    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");

    if (scheduled.isNull())
        return( 0 );

    int     nNumRecordings= scheduled.attribute( "count", "0" ).toInt();
    
    os << "  <div class=\"content\">\r\n"
       << "    <h2>Schedule</h2>\r\n";

    if (nNumRecordings == 0)
    {
        os << "    There are no shows scheduled for recording.\r\n"
           << "    </div>\r\n";
        return( 0 );
    }

    os << "    The next " << nNumRecordings << " show" << (nNumRecordings == 1 ? "" : "s" )
       << " that " << (nNumRecordings == 1 ? "is" : "are") 
       << " scheduled for recording:\r\n";

    os << "    <div id=\"schedule\">\r\n";

    // Iterate through all scheduled programs

    QDomNode node = scheduled.firstChild();

    while (!node.isNull())
    {
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            QDomNode recNode  = e.namedItem( "Recording" );
            QDomNode chanNode = e.namedItem( "Channel"   );

            if ((e.tagName() == "Program") && !recNode.isNull() && !chanNode.isNull())
            {
                QDomElement r =  recNode.toElement();
                QDomElement c =  chanNode.toElement();

                QString   sTitle       = e.attribute( "title"   , "" );    
                QString   sSubTitle    = e.attribute( "subTitle", "" );
                QDateTime startTs      = GetDateTime( e.attribute( "startTime" ,"" ));
                QDateTime endTs        = GetDateTime( e.attribute( "endTime"   ,"" ));
                QDateTime recStartTs   = GetDateTime( r.attribute( "recStartTs","" ));
//                QDateTime recEndTs     = GetDateTime( r.attribute( "recEndTs"  ,"" ));
                int       nPreRollSecs = r.attribute( "preRollSeconds", "0" ).toInt();
                int       nEncoderId   = r.attribute( "encoderId"     , "0" ).toInt();
                QString   sProfile     = r.attribute( "recProfile"    , ""  );
                QString   sChanName    = c.attribute( "channelName"   , ""  );
                QString   sDesc        = "";

                QDomText  text         = e.firstChild().toText();
                if (!text.isNull())
                    sDesc = text.nodeValue();

                // Build Time to recording start.

                int nTotalSecs = qdtNow.secsTo( recStartTs ) - nPreRollSecs;

                //since we're not displaying seconds
    
                nTotalSecs -= 60;          

                int nTotalDays  =  nTotalSecs / 86400;
                int nTotalHours = (nTotalSecs / 3600)
                                - (nTotalDays * 24);
                int nTotalMins  = (nTotalSecs / 60) % 60;

                QString sTimeToStart = "in";

                if ( nTotalDays > 1 )
                    sTimeToStart += QString(" %1 days,").arg( nTotalDays );
                else if ( nTotalDays == 1 )
                    sTimeToStart += (" 1 day,");

                if ( nTotalHours != 1)
                    sTimeToStart += QString(" %1 hours and").arg( nTotalHours );
                else if (nTotalHours == 1)
                    sTimeToStart += " 1 hour and";
 
                if ( nTotalMins != 1)
                    sTimeToStart += QString(" %1 minutes").arg( nTotalMins );
                else
                    sTimeToStart += " 1 minute";

                if ( nTotalHours == 0 && nTotalMins == 0)
                    sTimeToStart = "within one minute";

                if ( nTotalSecs < 0)
                    sTimeToStart = "soon";

                    // Output HTML

                os << "      <a href=\"#\">"
                   << recStartTs.addSecs(-nPreRollSecs).toString("ddd") << " "
                   << recStartTs.addSecs(-nPreRollSecs).toString(shortdateformat) << " "
                   << recStartTs.addSecs(-nPreRollSecs).toString(timeformat) << " - ";

                if (nEncoderId > 0)
                    os << "Encoder " << nEncoderId << " - ";

                os << sChanName << " - " << sTitle << "<br />"
                   << "<span><strong>" << sTitle << "</strong> ("
                   << startTs.toString(timeformat) << "-"
                   << endTs.toString(timeformat) << ")<br />";

                if ( !sSubTitle.isNull() && !sSubTitle.isEmpty())
                    os << "<em>" << sSubTitle << "</em><br /><br />";

                os << sDesc << "<br /><br />"
                   << "This recording will start "  << sTimeToStart
                   << " using encoder " << nEncoderId << " with the '"
                   << sProfile << "' profile.</span></a><hr />\r\n";
            }
        }

        node = node.nextSibling();
    }
    os  << "    </div>\r\n";
    os << "  </div>\r\n\r\n";

    return( nNumRecordings );
}

int HttpStatus::PrintJobQueue( QTextStream &os, QDomElement jobs )
{
    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");

    if (jobs.isNull())
        return( 0 );

    int nNumJobs= jobs.attribute( "count", "0" ).toInt();
    
    os << "  <div class=\"content\">\r\n"
       << "    <h2>Job Queue</h2>\r\n";

    if (nNumJobs != 0)
    {
        QString statusColor;
        QString jobColor;
        QString timeDateFormat;

        timeDateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d") +
                         " " + gContext->GetSetting("TimeFormat", "h:mm AP");

        os << "    Jobs currently in Queue or recently ended:\r\n<br />"
           << "    <div id=\"schedule\">\r\n";

        
        QDomNode node = jobs.firstChild();

        while (!node.isNull())
        {
            QDomElement e = node.toElement();

            if (!e.isNull())
            {
                QDomNode progNode = e.namedItem( "Program"   );

                if ((e.tagName() == "Job") && !progNode.isNull() )
                {
                    QDomElement p =  progNode.toElement();

                    QDomNode recNode  = p.namedItem( "Recording" );
                    QDomNode chanNode = p.namedItem( "Channel"   );

                    QDomElement r =  recNode.toElement();
                    QDomElement c =  chanNode.toElement();

                    int    nType   = e.attribute( "type"  , "0" ).toInt();
                    int nStatus = e.attribute( "status", "0" ).toInt();

                    switch( nStatus )
                    {
                        case JOB_ABORTED:
                            statusColor = " class=\"jobaborted\"";
                            jobColor = "";
                            break;

                        case JOB_ERRORED:
                            statusColor = " class=\"joberrored\"";
                            jobColor = " class=\"joberrored\"";
                            break;

                        case JOB_FINISHED:
                            statusColor = " class=\"jobfinished\"";
                            jobColor = " class=\"jobfinished\"";
                            break;

                        case JOB_RUNNING:
                            statusColor = " class=\"jobrunning\"";
                            jobColor = " class=\"jobrunning\"";
                            break;

                        default:
                            statusColor = "class=\"jobqueued";
                            jobColor = "class=\"jobqueued";
                            break;
                    }

                    QString   sTitle       = p.attribute( "title"   , "" );       //.replace(QRegExp("\""), "&quot;");
                    QString   sSubTitle    = p.attribute( "subTitle", "" );
                    QDateTime startTs      = GetDateTime( p.attribute( "startTime" ,"" ));
                    QDateTime endTs        = GetDateTime( p.attribute( "endTime"   ,"" ));
                    QDateTime recStartTs   = GetDateTime( r.attribute( "recStartTs","" ));
                    QDateTime statusTime   = GetDateTime( e.attribute( "statusTime","" ));
                    QString   sHostname    = e.attribute( "hostname", "master" );
                    QString   sComment     = "";

                    QDomText  text         = e.firstChild().toText();
                    if (!text.isNull())
                        sComment = text.nodeValue();

                    os << "<a href=\"#\">"
                       << recStartTs.toString("ddd") << " "
                       << recStartTs.toString(shortdateformat) << " "
                       << recStartTs.toString(timeformat) << " - "
                       << sTitle << " - <font" << jobColor << ">"
                       << JobQueue::JobText( nType ) << "</font><br />"
                       << "<span><strong>" << sTitle << "</strong> ("
                       << startTs.toString(timeformat) << "-"
                       << endTs.toString(timeformat) << ")<br />";

                    if ( !sSubTitle.isNull() && !sSubTitle.isEmpty())
                        os << "<em>" << sSubTitle << "</em><br /><br />";

                    os << "Job: " << JobQueue::JobText( nType ) << "<br />"
                       << "Status: <font" << statusColor << ">"
                       << JobQueue::StatusText( nStatus )
                       << "</font><br />"
                       << "Status Time: "
                       << statusTime.toString(timeDateFormat)
                       << "<br />";

                    if ( nStatus != JOB_QUEUED)
                        os << "Host: " << sHostname << "<br />";

                    if (!sComment.isNull() && !sComment.isEmpty())
                        os << "<br />Comments:<br />" << sComment << "<br />";

                    os << "</span></a><hr />\r\n";
                }
            }

            node = node.nextSibling();
        }
        os << "      </div>\r\n";
    }
    else
        os << "    Job Queue is currently empty.\r\n\r\n";

    os << "  </div>\r\n\r\n ";

    return( nNumJobs );

}

int HttpStatus::PrintMachineInfo( QTextStream &os, QDomElement info )
{
    QString   shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString   timeformat      = gContext->GetSetting("TimeFormat", "h:mm AP");
    QString   sRep;

    if (info.isNull())
        return( 0 );

    os << "<div class=\"content\">\r\n"
       << "    <h2>Machine information</h2>\r\n";

    // drive space   ---------------------
    
    QDomNode node = info.namedItem( "Storage" );

    if (!node.isNull())
    {    
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            int nFree = e.attribute( "free" , "0" ).toInt();
            int nTotal= e.attribute( "total", "0" ).toInt();
            int nUsed = e.attribute( "used" , "0" ).toInt();

             os << "    <div class=\"diskstatus\">\r\n";

            os << "      Disk Usage:\r\n      <ul>\r\n        <li>Total Space: ";
            sRep.sprintf(tr("%d,%03d MB "), (nTotal) / 1000, (nTotal) % 1000);
            os << sRep << "</li>\r\n";

            os << "        <li>Space Used: ";
            sRep.sprintf(tr("%d,%03d MB "), (nUsed) / 1000, (nUsed) % 1000);
            os << sRep << "</li>\r\n";

            os << "        <li>Space Free: ";
            sRep.sprintf(tr("%d,%03d MB "), (nFree) / 1000, (nFree) % 1000);
            os << sRep << "</li>\r\n      </ul>\r\n    </div>\r\n\r\n";
        }
    }

    // load average ---------------------

    node = info.namedItem( "Load" );

    if (!node.isNull())
    {    
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            double dAvg1 = e.attribute( "avg1" , "0" ).toDouble();
            double dAvg2 = e.attribute( "avg2" , "0" ).toDouble();
            double dAvg3 = e.attribute( "avg3" , "0" ).toDouble();

            os << "    <div class=\"loadstatus\">\r\n"
               << "      This machine's load average:"
               << "\r\n      <ul>\r\n        <li>"
               << "1 Minute: " << dAvg1 << "</li>\r\n"
               << "        <li>5 Minutes: " << dAvg2 << "</li>\r\n"
               << "        <li>15 Minutes: " << dAvg3
               << "</li>\r\n      </ul>\r\n"
               << "    </div>\r\n";    
        }
    }

    // Guide Info ---------------------

    node = info.namedItem( "Guide" );

    if (!node.isNull())
    {    
        QDomElement e = node.toElement();

        if (!e.isNull())
        {
            int     nDays   = e.attribute( "guideDays", "0" ).toInt();
            QString    sStatus = e.attribute( "status"   , ""  );
            QString sStart  = e.attribute( "start"    , ""  );
            QString sEnd    = e.attribute( "end"      , ""  );
            QString sMsg    = "";

            QDateTime thru  = QDateTime::fromString( e.attribute( "guideThru", ""  ), Qt::ISODate);

            QDomText  text  = e.firstChild().toText();

            if (!text.isNull())
                sMsg = text.nodeValue();


            os << "    Last mythfilldatabase run started on " << sStart
               << " and ";

            if (sEnd < sStart)   
                os << "is ";
            else 
                os << "ended on " << sEnd << ". ";

            os << sStatus << "<br />\r\n";    

            if (!thru.isNull())
            {
                os << "    There's guide data until "
                   << QDateTime( thru ).toString("yyyy-MM-dd hh:mm");

                if (nDays > 0)
                    os << " (" << nDays << " day" << (nDays == 1 ? "" : "s" ) << ")";

                os << ".";

                if (nDays <= 3)
                    os << " <strong>WARNING</strong>: is mythfilldatabase running?";
            }
            else
                os << "    There's <strong>no guide data</strong> available! "
                   << "Have you run mythfilldatabase?";

            if (!sMsg.isNull() && !sMsg.isEmpty())
                os << "<br />\r\nDataDirect Status: " << sMsg;

            os << "\r\n  </div>\r\n";
        }
    }

    return( 1 );
}

#if USING_DVB

int HttpStatus::PrintDVBStatus( QTextStream &os, QDomElement status )
{
    QString timeDateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d") +
                       " " + gContext->GetSetting("TimeFormat", "h:mm AP");

    int nCount = 0;

    os << "\r\n  <div class=\"content\">\r\n" <<
        "    <h2>DVB Signal Information</h2>\r\n" <<
        "    Details of DVB error statistics for last 48 hours:<br />\r\n";


    QDomNode node = status.firstChild();

    while (!node.isNull())
    {
        QDomElement eStatus = node.toElement();
        QDateTime   start   = GetDateTime( eStatus.attribute( "start" ,"" ));
        QDateTime   end     = GetDateTime( eStatus.attribute( "end"   ,"" ));

        QDomNode nodeInfo = node.firstChild();

        if (!nodeInfo.isNull() )
            os << "    <br />Recording period from " 
               << start.toString(timeDateFormat)
               << " to " << end.toString( timeDateFormat ) 
               << "<br />\n";

        while (!nodeInfo.isNull())
        {
            QDomElement e = nodeInfo.toElement();
                
            os << "    Encoder " << e.attribute( "cardId"   , "0").toInt()
               << " Min SNR: "   << e.attribute( "minSNR"   , "0").toInt()
               << " Avg SNR: "   << e.attribute( "avgSNR"   , "0").toInt()
               << " Min BER: "   << e.attribute( "minBER"   , "0").toInt()
               << " Avg BER: "   << e.attribute( "avgBER"   , "0").toInt()
               << " Cont Errs: " << e.attribute( "contErrs" , "0").toInt()
               << " Overflows: " << e.attribute( "overflows", "0").toInt()
               << "<br />\r\n";

            nCount++;

            nodeInfo = nodeInfo.nextSibling();
        }

        node = node.nextSibling();
    }

    if (nCount == 0)
    {
        os << "    <br />There is no DVB signal quality data available to "
            "display.<br />\r\n";
    }

    os << "  </div>\r\n";

    return( nCount );
}

#endif
