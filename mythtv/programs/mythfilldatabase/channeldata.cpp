// Qt headers
#include <qregexp.h>
#include <qdir.h>
#include <qfile.h>

// C++ headers
#include <iostream>

// libmyth headers
#include "mythcontext.h"
#include "mythdbcon.h"

// libmythtv headers
#include "channelutil.h"
#include "frequencytables.h"
#include "cardutil.h"
#include "sourceutil.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

void get_atsc_stuff(QString channum, int sourceid, int freqid,
                    int &major, int &minor, long long &freq)
{
    major = freqid;
    minor = 0;

    int chansep = channum.find(QRegExp("\\D"));
    if (chansep < 0)
        return;

    major = channum.left(chansep).toInt();
    minor = channum.right(channum.length() - (chansep + 1)).toInt();

    freq = get_center_frequency("atsc", "vsb8", "us", freqid);

    // Check if this is connected to an HDTV card.
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "SELECT cardtype "
        "FROM capturecard, cardinput "
        "WHERE cardinput.cardid = capturecard.cardid AND "
        "      sourceid         = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (query.exec() && query.isActive() && query.next() &&
        query.value(0).toString() == "HDTV")
    {
        freq -= 1750000; // convert to visual carrier freq.
    }
}

bool ChannelData::insert_chan(uint sourceid)
{
    bool insert_channels = channel_updates;
    if (!insert_channels)
    {
        bool isEncoder, isUnscanable;
        if (cardtype.isEmpty())
        {
            isEncoder    = SourceUtil::IsEncoder(sourceid);
            isUnscanable = SourceUtil::IsUnscanable(sourceid);
        }
        else
        {
            isEncoder    = CardUtil::IsEncoder(cardtype);
            isUnscanable = CardUtil::IsUnscanable(cardtype);
        }
        insert_channels = (isEncoder || isUnscanable) && !remove_new_channels;
    }

    return insert_channels;
}


QString getResponse(const QString &query, const QString &def)
{
    cout << query;

    if (def != "")
    {
        cout << " [" << (const char *)def.local8Bit() << "]  ";
    }
    
    char response[80];
    cin.getline(response, 80);

    QString qresponse = QString::fromLocal8Bit(response);

    if (qresponse == "")
        qresponse = def;

    return qresponse;
}

unsigned int ChannelData::promptForChannelUpdates(
    QValueList<ChanInfo>::iterator chaninfo, unsigned int chanid)
{
    if (chanid == 0)
    {
        // Default is 0 to allow rapid skipping of many channels,
        // in some xmltv outputs there may be over 100 channel, but
        // only 10 or so that are available in each area.
        chanid = atoi(getResponse("Choose a channel ID (positive integer) ",
                                  "0"));

        // If we wish to skip this channel, use the default 0 and return.
        if (chanid == 0)
            return(0);
    }

    (*chaninfo).name = getResponse("Choose a channel name (any string, "
                                   "long version) ",(*chaninfo).name);
    (*chaninfo).callsign = getResponse("Choose a channel callsign (any string, "
                                       "short version) ",(*chaninfo).callsign);

    if (channel_preset)
    {
        (*chaninfo).chanstr = getResponse("Choose a channel preset (0..999) ",
                                         (*chaninfo).chanstr);
        (*chaninfo).freqid  = getResponse("Choose a frequency id (just like "
                                          "xawtv) ",(*chaninfo).freqid);
    }
    else
    {
        (*chaninfo).chanstr  = getResponse("Choose a channel number (just like "
                                           "xawtv) ",(*chaninfo).chanstr);
        (*chaninfo).freqid = (*chaninfo).chanstr;
    }

    (*chaninfo).finetune = getResponse("Choose a channel fine tune offset (just"
                                       " like xawtv) ",(*chaninfo).finetune);

    (*chaninfo).tvformat = getResponse("Choose a TV format "
                                       "(PAL/SECAM/NTSC/ATSC/Default) ",
                                       (*chaninfo).tvformat);

    (*chaninfo).iconpath = getResponse("Choose a channel icon image (any path "
                                       "name) ",(*chaninfo).iconpath);

    return(chanid);
}

void ChannelData::handleChannels(int id, QValueList<ChanInfo> *chanlist)
{
    QString fileprefix = SetupIconCacheDirectory();

    QDir::setCurrent(fileprefix);

    fileprefix += "/";

    QValueList<ChanInfo>::iterator i = chanlist->begin();
    for (; i != chanlist->end(); i++)
    {
        QString localfile = "";

        if ((*i).iconpath != "")
        {
            QDir remotefile = QDir((*i).iconpath);
            QString filename = remotefile.dirName();

            localfile = fileprefix + filename;
            QFile actualfile(localfile);
            if (!actualfile.exists())
            {
                QString command = QString("wget ") + (*i).iconpath;
                system(command);
            }
        }

        MSqlQuery query(MSqlQuery::InitCon());

        QString querystr;

        if ((*i).old_xmltvid != "")
        {
            querystr.sprintf("SELECT xmltvid FROM channel WHERE xmltvid = '%s'",
                             (*i).old_xmltvid.ascii());
            query.exec(querystr);

            if (query.isActive() && query.size() > 0)
            {
                if (!quiet)
                    cout << "Converting old xmltvid (" << (*i).old_xmltvid << ") to new ("
                         << (*i).xmltvid << ")\n";

                query.exec(QString("UPDATE channel SET xmltvid = '%1' WHERE xmltvid = '%2';")
                            .arg((*i).xmltvid)
                            .arg((*i).old_xmltvid));

                if (!query.numRowsAffected())
                    MythContext::DBError("xmltvid conversion",query);
            }
        }

        querystr.sprintf("SELECT chanid,name,callsign,channum,finetune,"
                         "icon,freqid,tvformat FROM channel WHERE "
                         "xmltvid = '%s' AND sourceid = %d;", 
                         (*i).xmltvid.ascii(), id); 

        query.exec(querystr);
        if (query.isActive() && query.size() > 0)
        {
            query.next();

            QString chanid = query.value(0).toString();
            if (interactive)
            {
                QString name     = QString::fromUtf8(query.value(1).toString());
                QString callsign = QString::fromUtf8(query.value(2).toString());
                QString chanstr  = QString::fromUtf8(query.value(3).toString());
                QString finetune = QString::fromUtf8(query.value(4).toString());
                QString icon     = QString::fromUtf8(query.value(5).toString());
                QString freqid   = QString::fromUtf8(query.value(6).toString());
                QString tvformat = QString::fromUtf8(query.value(7).toString());

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = " << (*i).xmltvid.local8Bit() << endl;
                cout << "### chanid   = " << chanid.local8Bit()       << endl;
                cout << "### name     = " << name.local8Bit()         << endl;
                cout << "### callsign = " << callsign.local8Bit()     << endl;
                cout << "### channum  = " << chanstr.local8Bit()      << endl;
                if (channel_preset)
                    cout << "### freqid   = " << freqid.local8Bit()   << endl;
                cout << "### finetune = " << finetune.local8Bit()     << endl;
                cout << "### tvformat = " << tvformat.local8Bit()     << endl;
                cout << "### icon     = " << icon.local8Bit()         << endl;
                cout << "### " << endl;

                (*i).name = name;
                (*i).callsign = callsign;
                (*i).chanstr  = chanstr;
                (*i).finetune = finetune;
                (*i).freqid = freqid;
                (*i).tvformat = tvformat;

                promptForChannelUpdates(i, atoi(chanid.ascii()));

                if ((*i).callsign == "")
                    (*i).callsign = chanid;

                if (name     != (*i).name ||
                    callsign != (*i).callsign ||
                    chanstr  != (*i).chanstr ||
                    finetune != (*i).finetune ||
                    freqid   != (*i).freqid ||
                    icon     != localfile ||
                    tvformat != (*i).tvformat)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET chanid = :CHANID, "
                                     "name = :NAME, callsign = :CALLSIGN, "
                                     "channum = :CHANNUM, finetune = :FINE, "
                                     "icon = :ICON, freqid = :FREQID, "
                                     "tvformat = :TVFORMAT "
                                     " WHERE xmltvid = :XMLTVID "
                                     "AND sourceid = :SOURCEID;");
                    subquery.bindValue(":CHANID", chanid);
                    subquery.bindValue(":NAME", (*i).name.utf8());
                    subquery.bindValue(":CALLSIGN", (*i).callsign.utf8());
                    subquery.bindValue(":CHANNUM", (*i).chanstr);
                    subquery.bindValue(":FINE", (*i).finetune.toInt());
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":FREQID", (*i).freqid);
                    subquery.bindValue(":TVFORMAT", (*i).tvformat);
                    subquery.bindValue(":XMLTVID", (*i).xmltvid);
                    subquery.bindValue(":SOURCEID", id);

                    if (!subquery.exec())
                    {
                        cerr << "DB Error: Channel update failed, SQL query "
                             << "was:" << endl;
                        cerr << querystr << endl;
                    }
                    else
                    {
                        cout << "### " << endl;
                        cout << "### Change performed" << endl;
                        cout << "### " << endl;
                    }
                }
                else
                {
                    cout << "### " << endl;
                    cout << "### Nothing changed" << endl;
                    cout << "### " << endl;
                }
            }
            else
            {
                if (!non_us_updating && localfile != "")
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET icon = :ICON WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":CHANID", chanid);

                    if (!subquery.exec())
                        MythContext::DBError("Channel icon change", subquery);
                }
            }
        }
        else
        {
            int major, minor;
            long long freq;
            get_atsc_stuff((*i).chanstr, id, (*i).freqid.toInt(), major, minor, freq);

            if (interactive && ((minor == 0) || (freq > 0)))
            {
                cout << "### " << endl;
                cout << "### New channel found" << endl;
                cout << "### " << endl;
                cout << "### name     = " << (*i).name.local8Bit()     << endl;
                cout << "### callsign = " << (*i).callsign.local8Bit() << endl;
                cout << "### channum  = " << (*i).chanstr.local8Bit()  << endl;
                if (channel_preset)
                    cout << "### freqid   = " << (*i).freqid.local8Bit() << endl;
                cout << "### finetune = " << (*i).finetune.local8Bit() << endl;
                cout << "### tvformat = " << (*i).tvformat.local8Bit() << endl;
                cout << "### icon     = " << localfile.local8Bit()     << endl;
                cout << "### " << endl;

                uint chanid = promptForChannelUpdates(i,0);

                if ((*i).callsign == "")
                    (*i).callsign = QString::number(chanid);

                int mplexid = 0;
                if ((chanid > 0) && (minor > 0))
                    mplexid = ChannelUtil::CreateMultiplex(id,   "atsc",
                                                           freq, "8vsb");

                if (((mplexid > 0) || ((minor == 0) && (chanid > 0))) &&
                    ChannelUtil::CreateChannel(
                        mplexid,          id,               chanid,
                        (*i).callsign,    (*i).name,        (*i).chanstr,
                        0 /*service id*/, major,            minor,
                        false /*use on air guide*/, false /*hidden*/,
                        false /*hidden in guide*/,
                        (*i).freqid,      localfile,        (*i).tvformat,
                        (*i).xmltvid))
                {
                    cout << "### " << endl;
                    cout << "### Channel inserted" << endl;
                    cout << "### " << endl;
                }
                else
                {
                    cout << "### " << endl;
                    cout << "### Channel skipped" << endl;
                    cout << "### " << endl;
                }
            }
            else if (!non_us_updating && ((minor == 0) || (freq > 0)))
            {
                // We only do this if we are not asked to skip it with the
                // --updating flag.
                int mplexid = 0, chanid = 0;
                if (minor > 0)
                {
                    mplexid = ChannelUtil::CreateMultiplex(
                        id, "atsc", freq, "8vsb");
                }

                if ((mplexid > 0) || (minor == 0))
                    chanid = ChannelUtil::CreateChanID(id, (*i).chanstr);

                if ((*i).callsign.isEmpty())
                {
                    QStringList words = QStringList::split(" ",
                                        (*i).name.simplifyWhiteSpace().upper());
                    QString callsign = "";
                    if (words[0].isEmpty())
                        callsign = QString::number(chanid);
                    else if (words[1].isEmpty())
                        callsign = words[0].left(5);
                    else
                    {
                        callsign = words[0].left(words[1].length() == 1 ? 4:3);
                        callsign += words[1].left(5 - callsign.length());
                    }
                    (*i).callsign = callsign;
                }

                if (chanid > 0)
                {
                    QString cstr = QString((*i).chanstr);
                    if(channel_preset && cstr.isEmpty())
                        cstr = QString::number(chanid % 1000);

                    ChannelUtil::CreateChannel(
                        mplexid,          id,        chanid,
                        (*i).callsign,    (*i).name, cstr,
                        0 /*service id*/, major,     minor,
                        false /*use on air guide*/,  false /*hidden*/,
                        false /*hidden in guide*/,
                        (*i).freqid,      localfile, (*i).tvformat,
                        (*i).xmltvid);
                }
            }
        }
    }
}
