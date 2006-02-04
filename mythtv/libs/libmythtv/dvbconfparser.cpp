/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */
#include <qobject.h>
#include <qfile.h>
#include <qapplication.h>
#include "mythcontext.h"
#include "dvbtypes.h"
#include "dvbconfparser.h"
#include "mythdbcon.h"
#include "channelutil.h"

void DVBConfParser::Multiplex::dump()
{
    cerr << frequency<<" "<<inversion.toString()<<" "<<bandwidth.toString()<<" "<<coderate_hp.toString()<<" "<<coderate_lp.toString()<<" "<<constellation.toString()<<" "<<modulation.toString()<<" "<<transmit_mode.toString()<<" "<<guard_interval.toString()<<" "<<hierarchy.toString()<<" "<<polarity.toString()<<" "<<mplexid<<endl;
}
 
void DVBConfParser::Channel::dump()
{
    cerr<<lcn<<" "<<mplexnumber<<" "<<name<<" "<<frequency<<" "<<inversion.toString()<<" "<<bandwidth.toString()<<" "<<coderate_hp.toString()<<" "<<coderate_lp.toString()<<" "<<constellation.toString()<<" "<<transmit_mode.toString()<<" "<<guard_interval.toString()<<" "<<hierarchy.toString()<<" "<<polarity.toString()<<" "<<serviceid<<endl;
}

bool DVBConfParser::Multiplex::operator==(const Multiplex& m) const
{
    if ((frequency == m.frequency) &&
        (inversion == m.inversion) &&
        (bandwidth == m.bandwidth) &&
        (coderate_hp == m.coderate_hp) &&
        (coderate_lp == m.coderate_lp) &&
        (constellation == m.constellation) &&
        (transmit_mode == m.transmit_mode) &&
        (guard_interval == m.guard_interval) &&
        (fec == m.fec) &&
        (polarity == m.polarity) &&
        (hierarchy == m.hierarchy))
        return true;
    else
        return false;
}

DVBConfParser::DVBConfParser(enum TYPE _type,unsigned _sourceid,
                                 const QString& _file)
{
    type=_type;
    filename = _file;
    sourceid=_sourceid;
}

int DVBConfParser::parse()
{
    QFile file( filename );
    if (file.open( IO_ReadOnly ) ) 
    {
        QTextStream stream( &file );
        QString line;
        while ( !stream.atEnd() ) 
        {
            line = stream.readLine(); // line of text excluding '\n'
            line.stripWhiteSpace();
            if (line.startsWith("#"))
                continue;
            QStringList list=QStringList::split(":",line);
            QString str = *list.at(0);
            int channelNo = -1;
            if (str.at(0)=='@')
            {
                channelNo=str.mid(1).toInt();
                line = stream.readLine(); 
                list=QStringList::split(":",line);
            }
            str = *list.at(3);
            if ((str == "T") || (str == "C") || (str=="S"))
            {
                if ((type == OFDM) && (str=="T"))
                    parseVDR(list,channelNo);
                else if ((type == QPSK) && (str=="S"))
                    parseVDR(list,channelNo);
                else if ((type == QAM) && (str=="C"))
                    parseVDR(list,channelNo);
            }
            else if (type==OFDM)
                parseConfOFDM(list);
            else if (type==ATSC)
                parseConfATSC(list);
            else if (type==QPSK)
                parseConfQPSK(list);
            else if (type==QAM)
                parseConfQAM(list);
        }
        file.close();

        processChannels();
        return OK;
    }
    return ERROR_OPEN;
}

bool DVBConfParser::parseConfOFDM(QStringList& tokens)
{
    Channel c;
    QStringList::Iterator i = tokens.begin();
    QStringList::Iterator end = tokens.end();
    if (i != end) c.name = *i++; else return false;
    if (i != end) c.frequency = (*i++).toInt(); else return false;
    if (i == end || !c.inversion.parseConf(*i++)) return false;
    if (i == end || !c.bandwidth.parseConf(*i++)) return false;
    if (i == end || !c.coderate_hp.parseConf(*i++)) return false;
    if (i == end || !c.coderate_lp.parseConf(*i++)) return false;
    if (i == end || !c.constellation.parseConf(*i++)) return false;
    if (i == end || !c.transmit_mode.parseConf(*i++)) return false;
    if (i == end || !c.guard_interval.parseConf(*i++)) return false;
    if (i == end || !c.hierarchy.parseConf(*i++)) return false;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i != end) c.serviceid = (*i++).toInt(); else return false;

    channels.append(c);

    return true;
}

bool DVBConfParser::parseConfATSC(QStringList& tokens)
{
    Channel c;
    QStringList::Iterator i = tokens.begin();
    QStringList::Iterator end = tokens.end();
    if (i != end) c.name = *i++; else return false;
    if (i != end) c.frequency = (*i++).toInt(); else return false;
    if (i == end || !c.modulation.parseConf(*i++)) return false;
    // We need the program number in the transport stream,
    // otherwise we cannot "tune" to the program.
    if (i == end ) return false; else i++;   // Ignore video pid
    if (i == end ) return false; else i++;   // Ignore audio pid
    if (i != end) c.serviceid = (*i++).toInt(); else return false;

    channels.append(c);

    return true;
}

bool DVBConfParser::parseConfQAM(QStringList& tokens)
{
    Channel c;
    QStringList::Iterator i = tokens.begin();
    QStringList::Iterator end = tokens.end();

    if (i != end) c.name = *i++; else return false;
    if (i != end) c.frequency = (*i++).toInt(); else return false;
    if (i == end || !c.inversion.parseConf(*i++)) return false;
    if (i != end) c.symbolrate = (*i++).toInt(); else return false;
    if (i == end || !c.fec.parseConf(*i++)) return false;
    if (i == end || !c.modulation.parseConf(*i++)) return false;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i != end) c.serviceid = (*i++).toInt(); else return false;
    
    channels.append(c);
    return true;
}

bool DVBConfParser::parseConfQPSK(QStringList& tokens)
{
    Channel c;
    QStringList::Iterator i = tokens.begin();
    QStringList::Iterator end = tokens.end();

    if (i != end) c.name = *i++; else return false;
    if (i != end) c.frequency = (*i++).toInt(); else return false;
    if (i == end || !c.polarity.parseConf(*i++)) return false;
    if (i == end ) return false; else i++; //Sat num
    if (i != end) c.symbolrate = (*i++).toInt(); else return false;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i != end) c.serviceid = (*i++).toInt(); else return false;
    
    channels.append(c);
    return true;
}

bool DVBConfParser::parseVDR(QStringList& tokens, int channelNo)
{
    Channel c;
    QStringList::Iterator i = tokens.begin();
    QStringList::Iterator end = tokens.end();
    c.lcn = channelNo;

//BBC ONE:754166:I999B8C34D34M16T2G32Y0:T:27500:600:601,602:0:0:4168:0:0:0
    if (i != end) c.name = *i++; else return false;
    if (i != end) c.frequency = (*i++).toInt()*1000; else return false;
    if (i == end) return false;
    QString params = (*i++);
    while (!params.isEmpty())
    {
        QString ori = params;
        int s = *(const char*)params;
        params=params.mid(1);
        switch(s)
        {
        case 'I':
            c.inversion.parseVDR(params);
            break;
        case 'B':
            c.bandwidth.parseVDR(params);
            break;
        case 'C':
            c.coderate_hp.parseVDR(params);
            break;
        case 'D':
            c.coderate_lp.parseVDR(params);
            break;
        case 'M':
            c.constellation.parseVDR(params);
            break;
        case 'T':
            c.transmit_mode.parseVDR(params);
            break;
        case 'G':
            c.guard_interval.parseVDR(params);
            break;
        case 'Y':
            c.hierarchy.parseVDR(params);
            break;
        case 'V':
        case 'H':
        case 'R':
        case 'L':
            c.polarity.parseVDR(ori);
            break;
        default:
            return false;
        }
    }
     
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i == end ) return false; else i++;
    if (i != end) c.serviceid = (*i++).toInt(); else return false;

    channels.append(c);
    return true;
}

int DVBConfParser::generateNewChanID(int sourceID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString theQuery =
        QString("SELECT max(chanid) as maxchan "
                "FROM channel WHERE sourceid=%1").arg(sourceID);
    query.prepare(theQuery);

    if (!query.exec())
        MythContext::DBError("Calculating new ChanID", query);

    if (!query.isActive())
        MythContext::DBError("Calculating new ChanID for Analog Channel",query);

    query.next();

    // If transport not present add it, and move on to the next
    if (query.size() <= 0)
        return sourceID * 1000;

    int MaxChanID = query.value(0).toInt();

    if (MaxChanID == 0)
        return sourceID * 1000;
    else
        return MaxChanID + 1;
}

int DVBConfParser::findMultiplex(const DVBConfParser::Multiplex& m)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString queryStr=QString("SELECT mplexid FROM dtv_multiplex WHERE "
             "sourceid= %1 AND frequency=%2 AND inversion=\"%3\" AND ")
             .arg(sourceid).arg(m.frequency).arg(m.inversion.toString());

    switch (type)
    {
    case OFDM:
        queryStr+=QString("sistandard=\"dvb\" AND bandwidth=\"%1\" AND "
                     "hp_code_rate=\"%2\" AND "
                     "lp_code_rate=\"%3\" AND constellation=\"%4\" AND "
                     "transmission_mode=\"%5\" AND guard_interval=\"%6\" AND "
                     "hierarchy=\"%7\";")
                      .arg(m.bandwidth.toString())
                      .arg(m.coderate_hp.toString())
                      .arg(m.coderate_lp.toString())
                      .arg(m.constellation.toString())
                      .arg(m.transmit_mode.toString())
                      .arg(m.guard_interval.toString())
                      .arg(m.hierarchy.toString());
        break;
    case QPSK:
        queryStr+=QString("sistandard=\"dvb\" AND symbolrate=%1 AND "
                          "polarity=\"%2\";").arg(m.symbolrate)
                         .arg(m.polarity.toString());
        break; 
    case QAM:
        queryStr+=QString("symbolrate=%1 AND modulation=\"%2\" AND fec=\"%3\";")
                         .arg(m.symbolrate)
                         .arg(m.modulation.toString())
                         .arg(m.fec.toString());
        break;
    case ATSC:
        queryStr+=QString("modulation=\"%1\";")
                         .arg(m.modulation.toString());
        break;
    } 
    query.prepare(queryStr);
    if (!query.exec())
        MythContext::DBError("searching for transport", query);
    if (!query.isActive())
        MythContext::DBError("searching for transport.", query);
    if (query.size() > 0)
    {
       query.next();
       return query.value(0).toInt();
    }
    return -1;
}

int DVBConfParser::findChannel(const DVBConfParser::Channel &c, int &mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // try to find exact match first
    query.prepare("SELECT chanid "
                  "FROM channel "
                  "WHERE callsign = :CALLSIGN AND "
                  "      mplexid  = :MPLEXID  AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":MPLEXID",  multiplexes[c.mplexnumber].mplexid);
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CALLSIGN", c.name.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("searching for channel", query);
    else if (query.next())
    {
        mplexid = multiplexes[c.mplexnumber].mplexid;
        return query.value(0).toInt();
    }

    // if we didn't find exact match, try to match just the source & callsign
    query.prepare("SELECT chanid, mplexid "
                  "FROM channel "
                  "WHERE callsign = :CALLSIGN AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CALLSIGN", c.name.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("searching for channel", query);
    else if (query.next())
    {
        mplexid = query.value(1).toInt();
        return query.value(0).toInt();
    }

    return -1;
} 

void DVBConfParser::processChannels()
{
    ListChannels::iterator iter;
    for (iter=channels.begin();iter!=channels.end();iter++)
    {
        bool fFound = false;
        for (unsigned i=0;i<multiplexes.size() && !fFound;i++ )
        {
            if (multiplexes[i] == (Multiplex)(*iter))
            {
                (*iter).mplexnumber = i;
                fFound = true;
            }
        } 
        if (!fFound)
        {
            (*iter).mplexnumber = multiplexes.size();
            multiplexes.append((Multiplex)(*iter));
        }
    } 
/*
    for (iter=channels.begin();iter!=channels.end();iter++)
        (*iter).dump();
    for (unsigned i=0;i<multiplexes.size() ;i++ )
        multiplexes[i].dump();
*/
    QString standard = (type == ATSC) ? "atsc" : "dvb"; 
    //create the multiplexes
    MSqlQuery query(MSqlQuery::InitCon());
    for (unsigned i=0;i<multiplexes.size() ;i++ )
    {
        int mplexid = findMultiplex(multiplexes[i]);
        if (mplexid < 0)
        {
            query.prepare("INSERT into dtv_multiplex (frequency, "
                "sistandard, sourceid,inversion,bandwidth,hp_code_rate,"
                "lp_code_rate,constellation,transmission_mode,guard_interval,"
                "hierarchy,modulation,symbolrate,fec,polarity) "
                "VALUES (:FREQUENCY,:STANDARD,:SOURCEID,:INVERSION,:BANDWIDTH,"
                ":CODERATE_HP,:CODERATE_LP,:CONSTELLATION,:TRANS_MODE,"
                ":GUARD_INTERVAL,:HIERARCHY,:MODULATION,:SYMBOLRATE,"
                ":FEC,:POLARITY);");
            query.bindValue(":STANDARD",standard);
            query.bindValue(":SOURCEID",sourceid);
            query.bindValue(":FREQUENCY",multiplexes[i].frequency);
            query.bindValue(":INVERSION",multiplexes[i].inversion.toString());
            query.bindValue(":BANDWIDTH",multiplexes[i].bandwidth.toString());
            query.bindValue(":CODERATE_HP",multiplexes[i].coderate_hp.toString());
            query.bindValue(":CODERATE_LP",multiplexes[i].coderate_lp.toString());
            query.bindValue(":CONSTELLATION",multiplexes[i].constellation.toString());
            query.bindValue(":TRANS_MODE",multiplexes[i].transmit_mode.toString());
            query.bindValue(":GUARD_INTERVAL",multiplexes[i].guard_interval.toString());
            query.bindValue(":HIERARCHY",multiplexes[i].hierarchy.toString());
            query.bindValue(":MODULATION",multiplexes[i].modulation.toString());
            query.bindValue(":SYMBOLRATE",multiplexes[i].symbolrate);
            query.bindValue(":FEC",multiplexes[i].fec.toString());
            query.bindValue(":POLARITY",multiplexes[i].polarity.toString());

            if (!query.exec())
                MythContext::DBError("Inserting new transport", query);
            if (!query.isActive())
                MythContext::DBError("Adding transport to Database.", query);
            query.prepare("select max(mplexid) from dtv_multiplex;");
            if (!query.exec())
                MythContext::DBError("Getting ID of new Transport", query);
            if (!query.isActive())
                MythContext::DBError("Getting ID of new Transport.", query);
            if (query.size() > 0)
            {
               query.next();
               multiplexes[i].mplexid = query.value(0).toInt();
            }
       }
       else
           multiplexes[i].mplexid = mplexid;
    }

    // If the channel number cannot be determined from the config
    // file, assign temporary unique numbers. First determine the
    // highest channel number already assigned. This will likely
    // fail if there are any ATSC channels, since channum is not
    // really an integer. But in that case 501 is a generally safe
    // offset for the first unknown channel.
    int maxchannum = 500;
    query.prepare("SELECT MAX(channum) FROM channel");
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting highest channel number.", query);
    else if (query.next())
        maxchannum = max(maxchannum, query.value(0).toInt());
    for (iter = channels.begin(); iter != channels.end(); ++iter)
        maxchannum = max(maxchannum, (*iter).lcn);

    // Now insert the channels
    for (iter=channels.begin();iter!=channels.end();iter++)
    {
        int mplexid    = multiplexes[(*iter).mplexnumber].mplexid;
        int db_mplexid = 0;
        int chanid     = findChannel(*iter, db_mplexid);
        if (chanid < 0)
        {
            // The channel does not exist in the DB at all, insert it.
            query.prepare("INSERT INTO channel (chanid, channum, "
                  "sourceid, callsign, name,  mplexid, "
                  "serviceid) "
                  "VALUES (:CHANID,:CHANNUM,:SOURCEID,:CALLSIGN,"
                  ":NAME,:MPLEXID,:SERVICEID);");

            // If the channel number is unknown, get next unique number
            int channum = (*iter).lcn;
            if (-1 == channum)
                channum = ++maxchannum;

            int chanid = ChannelUtil::CreateChanID(
                sourceid, QString::number(channum));

            query.bindValue(":CHANID",    chanid);
            query.bindValue(":CHANNUM",   channum);
            query.bindValue(":SOURCEID",  sourceid);
            query.bindValue(":CALLSIGN",  (*iter).name.utf8());
            query.bindValue(":NAME",      (*iter).name.utf8());
            query.bindValue(":MPLEXID",   mplexid);
            query.bindValue(":SERVICEID", (*iter).serviceid);
            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError("Adding new DVB Channel", query);
                emit updateText(QObject::tr("Failed to add %1: DB error")
                                .arg((*iter).name));
            }
            else
            {
                emit updateText(QObject::tr("Adding %1").arg((*iter).name));
            }
        }
        else if (db_mplexid == 32767)
        {
            // The channel in the database if from the listings provider amd
            // does not have tuning information. Just fill in the tuning info.
            query.prepare("UPDATE channel "
                          "SET mplexid   = :MPLEXID,  "
                          "    serviceid = :SERVICEID "
                          "WHERE chanid   = :CHANID   AND "
                          "      sourceid = :SOURCEID     ");

            query.bindValue(":MPLEXID",   mplexid);
            query.bindValue(":SERVICEID", (*iter).serviceid);
            query.bindValue(":CHANID",    chanid);
            query.bindValue(":SOURCEID",  sourceid);

            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError("Updating DVB Channel", query);
                emit updateText(QObject::tr("Failed to add %1: DB error")
                                .arg((*iter).name));
            }
            else
            {
                emit updateText(QObject::tr("Updating %1").arg((*iter).name));
            }
        }
        else
            emit updateText(QObject::tr("Skipping %1").arg((*iter).name));
    }
}
