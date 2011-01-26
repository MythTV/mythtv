// C++ headers
#include <iostream>
#include <cstdlib>

// Qt headers
#include <QRegExp>
#include <QDir>
#include <QFile>

// libmythbase headers
#include "httpcomms.h"

// libmyth headers
#include "mythverbose.h"
#include "mythdb.h"
#include "util.h"

// libmythtv headers
#include "channelutil.h"
#include "frequencytables.h"
#include "cardutil.h"
#include "sourceutil.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

static void get_atsc_stuff(QString channum, int sourceid, int freqid,
                           int &major, int &minor, long long &freq)
{
    major = freqid;
    minor = 0;

    int chansep = channum.indexOf(QRegExp("\\D"));
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


unsigned int ChannelData::promptForChannelUpdates(
    QList<ChanInfo>::iterator chaninfo, unsigned int chanid)
{
    if (chanid == 0)
    {
        // Default is 0 to allow rapid skipping of many channels,
        // in some xmltv outputs there may be over 100 channel, but
        // only 10 or so that are available in each area.
        chanid = getResponse("Choose a channel ID (positive integer) ", "0")
            .toUInt();

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

void ChannelData::handleChannels(int id, QList<ChanInfo> *chanlist)
{
    QString fileprefix = SetupIconCacheDirectory();

    QDir::setCurrent(fileprefix);

    fileprefix += "/";

    QList<ChanInfo>::iterator i = chanlist->begin();
    for (; i != chanlist->end(); ++i)
    {
        QString localfile;

        if (!(*i).iconpath.isEmpty())
        {
            QDir remotefile = QDir((*i).iconpath);
            QString filename = remotefile.dirName();

            localfile = fileprefix + filename;
            QFile actualfile(localfile);
            if (!actualfile.exists() &&
                !HttpComms::getHttpFile(localfile, (*i).iconpath))
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Failed to fetch icon from '%1'")
                        .arg((*i).iconpath));
            }
        }

        MSqlQuery query(MSqlQuery::InitCon());

        if (!(*i).old_xmltvid.isEmpty())
        {
            query.prepare(
                "SELECT xmltvid "
                "FROM channel "
                "WHERE xmltvid = :XMLTVID");
            query.bindValue(":XMLTVID", (*i).old_xmltvid);

            if (!query.exec())
            {
                MythDB::DBError("xmltvid conversion 1", query);
            }
            else if (query.next())
            {
                VERBOSE(VB_GENERAL,
                        QString("Converting old xmltvid (%1) to new (%2)")
                        .arg((*i).old_xmltvid).arg((*i).xmltvid));

                query.prepare("UPDATE channel "
                              "SET xmltvid = :NEWXMLTVID"
                              "WHERE xmltvid = :OLDXMLTVID");
                query.bindValue(":NEWXMLTVID", (*i).xmltvid);
                query.bindValue(":OLDXMLTVID", (*i).old_xmltvid);

                if (!query.exec())
                {
                    MythDB::DBError("xmltvid conversion 2", query);
                }
            }
        }

        query.prepare(
            "SELECT chanid,   name, callsign, channum, "
            "       finetune, icon, freqid,   tvformat "
            "FROM channel "
            "WHERE xmltvid  = :XMLTVID AND "
            "      sourceid = :SOURCEID");
        query.bindValue(":XMLTVID",  (*i).xmltvid);
        query.bindValue(":SOURCEID", id);

        if (!query.exec())
        {
            MythDB::DBError("handleChannels", query);
        }
        else if (query.next())
        {
            QString chanid = query.value(0).toString();
            if (interactive)
            {
                QString name     = query.value(1).toString();
                QString callsign = query.value(2).toString();
                QString chanstr  = query.value(3).toString();
                QString finetune = query.value(4).toString();
                QString icon     = query.value(5).toString();
                QString freqid   = query.value(6).toString();
                QString tvformat = query.value(7).toString();

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = "
                     << (*i).xmltvid.toLocal8Bit().constData() << endl;
                cout << "### chanid   = "
                     << chanid.toLocal8Bit().constData()       << endl;
                cout << "### name     = "
                     << name.toLocal8Bit().constData()         << endl;
                cout << "### callsign = "
                     << callsign.toLocal8Bit().constData()     << endl;
                cout << "### channum  = "
                     << chanstr.toLocal8Bit().constData()      << endl;
                if (channel_preset)
                {
                    cout << "### freqid   = "
                         << freqid.toLocal8Bit().constData()   << endl;
                }
                cout << "### finetune = "
                     << finetune.toLocal8Bit().constData()     << endl;
                cout << "### tvformat = "
                     << tvformat.toLocal8Bit().constData()     << endl;
                cout << "### icon     = "
                     << icon.toLocal8Bit().constData()         << endl;
                cout << "### " << endl;

                (*i).name = name;
                (*i).callsign = callsign;
                (*i).chanstr  = chanstr;
                (*i).finetune = finetune;
                (*i).freqid = freqid;
                (*i).tvformat = tvformat;

                promptForChannelUpdates(i, chanid.toUInt());

                if ((*i).callsign.isEmpty())
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
                    subquery.bindValue(":NAME", (*i).name);
                    subquery.bindValue(":CALLSIGN", (*i).callsign);
                    subquery.bindValue(":CHANNUM", (*i).chanstr);
                    subquery.bindValue(":FINE", (*i).finetune.toInt());
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":FREQID", (*i).freqid);
                    subquery.bindValue(":TVFORMAT", (*i).tvformat);
                    subquery.bindValue(":XMLTVID", (*i).xmltvid);
                    subquery.bindValue(":SOURCEID", id);

                    if (!subquery.exec())
                    {
                        MythDB::DBError("update failed", subquery);
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
                if (!non_us_updating && !localfile.isEmpty())
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET icon = :ICON WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":CHANID", chanid);

                    if (!subquery.exec())
                        MythDB::DBError("Channel icon change", subquery);
                }
            }
        }
        else
        {
            int major, minor = 0;
            long long freq = 0;
            get_atsc_stuff((*i).chanstr, id, (*i).freqid.toInt(), major, minor, freq);

            if (interactive && ((minor == 0) || (freq > 0)))
            {
                cout << "### " << endl;
                cout << "### New channel found" << endl;
                cout << "### " << endl;
                cout << "### name     = "
                     << (*i).name.toLocal8Bit().constData()     << endl;
                cout << "### callsign = "
                     << (*i).callsign.toLocal8Bit().constData() << endl;
                cout << "### channum  = "
                     << (*i).chanstr.toLocal8Bit().constData()  << endl;
                if (channel_preset)
                {
                    cout << "### freqid   = "
                         << (*i).freqid.toLocal8Bit().constData() << endl;
                }
                cout << "### finetune = "
                     << (*i).finetune.toLocal8Bit().constData() << endl;
                cout << "### tvformat = "
                     << (*i).tvformat.toLocal8Bit().constData() << endl;
                cout << "### icon     = "
                     << localfile.toLocal8Bit().constData()     << endl;
                cout << "### " << endl;

                uint chanid = promptForChannelUpdates(i,0);

                if ((*i).callsign.isEmpty())
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
                    QStringList words = (*i).name.simplified().toUpper()
                        .split(" ");
                    QString callsign = "";
                    QString w1 = words.size() > 0 ? words[0] : QString();
                    QString w2 = words.size() > 1 ? words[1] : QString();
                    if (w1.isEmpty())
                        callsign = QString::number(chanid);
                    else if (w2.isEmpty())
                        callsign = words[0].left(5);
                    else
                    {
                        callsign = w1.left(w2.length() == 1 ? 4:3);
                        callsign += w2.left(5 - callsign.length());
                    }
                    (*i).callsign = callsign;
                }

                if (chanid > 0)
                {
                    QString cstr = QString((*i).chanstr);
                    if(channel_preset && cstr.isEmpty())
                        cstr = QString::number(chanid % 1000);

                    bool retval = ChannelUtil::CreateChannel(
                                                     mplexid, id,
                                                     chanid,
                                                     (*i).callsign,
                                                     (*i).name, cstr,
                                                     0 /*service id*/,
                                                     major, minor,
                                                     false /*use on air guide*/,
                                                     false /*hidden*/,
                                                     false /*hidden in guide*/,
                                                     (*i).freqid,
                                                     localfile,
                                                     (*i).tvformat,
                                                     (*i).xmltvid
                                                            );
                    if (!retval)
                        cout << "Channel " << chanid << " creation failed"
                             << endl;
                }
            }
        }
    }
}
