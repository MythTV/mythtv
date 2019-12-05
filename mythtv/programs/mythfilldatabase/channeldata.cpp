// C++ headers
#include <iostream>
#include <cstdlib>

// Qt headers
#include <QRegExp>
#include <QDir>
#include <QFile>

// libmythbase headers
#include "mythdownloadmanager.h"

// libmyth headers
#include "mythlogging.h"
#include "mythdb.h"
#include "mythmiscutil.h"

// libmythtv headers
#include "channelutil.h"
#include "frequencytables.h"
#include "cardutil.h"
#include "sourceutil.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

static void get_atsc_stuff(const QString& channum, int sourceid, int freqid,
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
    MSqlQuery query(MSqlQuery::ChannelCon());
    query.prepare(
        "SELECT cardtype "
        "FROM capturecard "
        "WHERE sourceid         = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (query.exec() && query.isActive() && query.next() &&
        query.value(0).toString() == "HDTV")
    {
        freq -= 1750000; // convert to visual carrier freq.
    }
}

bool ChannelData::insert_chan(uint sourceid)
{
    bool insert_channels = m_channelUpdates;
    if (!insert_channels)
    {
        bool isEncoder = false;
        bool isUnscanable = false;
        bool isCableCard  = SourceUtil::IsCableCardPresent(sourceid);
        if (m_cardType.isEmpty())
        {
            isEncoder    = SourceUtil::IsEncoder(sourceid);
            isUnscanable = SourceUtil::IsUnscanable(sourceid);
        }
        else
        {
            isEncoder    = CardUtil::IsEncoder(m_cardType);
            isUnscanable = CardUtil::IsUnscanable(m_cardType);
        }
        insert_channels = (isCableCard || isEncoder || isUnscanable);
    }

    return insert_channels;
}


unsigned int ChannelData::promptForChannelUpdates(
    ChannelInfoList::iterator chaninfo, unsigned int chanid)
{
    (*chaninfo).m_name = getResponse(QObject::tr("Choose a channel name (any string, "
                                     "long version) "),(*chaninfo).m_name);
    (*chaninfo).m_callsign = getResponse(QObject::tr("Choose a channel callsign (any string, "
                                         "short version) "),(*chaninfo).m_callsign);

    if (m_channelPreset)
    {
        (*chaninfo).m_channum = getResponse(QObject::tr("Choose a channel preset (0..999) "),
                                           (*chaninfo).m_channum);
        (*chaninfo).m_freqid  = getResponse(QObject::tr("Choose a frequency id "),
                                            (*chaninfo).m_freqid);
    }
    else
    {
        (*chaninfo).m_channum  = getResponse(QObject::tr("Choose a channel number "),
                                             (*chaninfo).m_channum);
        (*chaninfo).m_freqid = (*chaninfo).m_channum;
    }

    (*chaninfo).m_finetune = getResponse(QObject::tr("Choose a channel fine tune offset "),
                                         QString::number((*chaninfo).m_finetune)).toInt();

    (*chaninfo).m_tvformat = getResponse(QObject::tr("Choose a TV format "
                                         "(PAL/SECAM/NTSC/ATSC/Default) "),
                                         (*chaninfo).m_tvformat);

    (*chaninfo).m_icon = getResponse(QObject::tr("Choose a channel icon image "
                                     "(relative path to icon storage group) "),
                                     (*chaninfo).m_icon);

    return(chanid);
}

QString ChannelData::normalizeChannelKey(const QString &chanName)
{
    QString result = chanName;

    // Lowercase
    result = result.toLower();
    // Strip all whitespace
    result = result.replace(" ", "");

    return result;
}

ChannelList ChannelData::channelList(int sourceId)
{
    ChannelList retList;

    uint avail = 0;
    ChannelInfoList channelList = ChannelUtil::LoadChannels(0, 0, avail, false,
                                                ChannelUtil::kChanOrderByChanNum,
                                                ChannelUtil::kChanGroupByChanid,
                                                sourceId);

    auto it = channelList.begin();
    for ( ; it != channelList.end(); ++it)
    {
        QString chanName = (*it).m_name;
        QString key  = normalizeChannelKey(chanName);
        retList.insert(key, *it);
    }

    return retList;
}

ChannelInfo ChannelData::FindMatchingChannel(const ChannelInfo &chanInfo,
                                             ChannelList existingChannels)
{
    ChannelList::iterator it;
    for (it = existingChannels.begin(); it != existingChannels.end(); ++it)
    {
        if ((*it).m_xmltvid == chanInfo.m_xmltvid)
            return (*it);
    }

    QString searchKey = normalizeChannelKey(chanInfo.m_name);
    ChannelInfo existChan = existingChannels.value(searchKey);

    if (existChan.m_chanid < 1)
    {
        // Check if it is ATSC
        int chansep = chanInfo.m_channum.indexOf(QRegExp("\\D"));
        if (chansep > 0)
        {
            // Populate xmltvid for scanned ATSC channels
            uint major = chanInfo.m_channum.left(chansep).toInt();
            uint minor = chanInfo.m_channum.right
                         (chanInfo.m_channum.length() - (chansep + 1)).toInt();

            for (it = existingChannels.begin();
                 it != existingChannels.end(); ++it)
            {
                if ((*it).m_atsc_major_chan == major &&
                    (*it).m_atsc_minor_chan == minor)
                    return (*it);
            }
        }
    }

    return existChan;
}

void ChannelData::handleChannels(int id, ChannelInfoList *chanlist)
{
    if (m_guideDataOnly)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Skipping Channel Updates");
        return;
    }

    ChannelList existingChannels = channelList(id);
    QString fileprefix = SetupIconCacheDirectory();

    QDir::setCurrent(fileprefix);

    fileprefix += "/";

    bool insertChan = insert_chan(id);  // unscannable source

    auto i = chanlist->begin();
    for (; i != chanlist->end(); ++i)
    {
        if ((*i).m_xmltvid.isEmpty())
            continue;

        QString localfile;

        if (!(*i).m_icon.isEmpty())
        {
            QDir remotefile = QDir((*i).m_icon);
            QString filename = remotefile.dirName();

            localfile = fileprefix + filename;
            QFile actualfile(localfile);
            if (!actualfile.exists() &&
                !GetMythDownloadManager()->download((*i).m_icon, localfile))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to fetch icon from '%1'")
                        .arg((*i).m_icon));
            }

            localfile = filename;
        }

        MSqlQuery query(MSqlQuery::InitCon());

        if (!(*i).m_old_xmltvid.isEmpty())
        {
            query.prepare(
                "SELECT xmltvid "
                "FROM channel "
                "WHERE xmltvid = :XMLTVID");
            query.bindValue(":XMLTVID", (*i).m_old_xmltvid);

            if (!query.exec())
            {
                MythDB::DBError("xmltvid conversion 1", query);
            }
            else if (query.next())
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Converting old xmltvid (%1) to new (%2)")
                        .arg((*i).m_old_xmltvid).arg((*i).m_xmltvid));

                query.prepare("UPDATE channel "
                              "SET xmltvid = :NEWXMLTVID"
                              "WHERE xmltvid = :OLDXMLTVID");
                query.bindValue(":NEWXMLTVID", (*i).m_xmltvid);
                query.bindValue(":OLDXMLTVID", (*i).m_old_xmltvid);

                if (!query.exec())
                {
                    MythDB::DBError("xmltvid conversion 2", query);
                }
            }
        }

        ChannelInfo dbChan = FindMatchingChannel(*i, existingChannels);
        if (dbChan.m_chanid > 0) // Channel exists, updating
        {
            LOG(VB_XMLTV, LOG_NOTICE,
                    QString("Match found for xmltvid %1 to channel %2 (%3)")
                        .arg((*i).m_xmltvid).arg(dbChan.m_name).arg(dbChan.m_chanid));
            if (m_interactive)
            {

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = "
                     << (*i).m_xmltvid.toLocal8Bit().constData()      << endl;
                cout << "### chanid   = "
                     << dbChan.m_chanid                               << endl;
                cout << "### name     = "
                     << dbChan.m_name.toLocal8Bit().constData()       << endl;
                cout << "### callsign = "
                     << dbChan.m_callsign.toLocal8Bit().constData()   << endl;
                cout << "### channum  = "
                     << dbChan.m_channum.toLocal8Bit().constData()    << endl;
                if (m_channelPreset)
                {
                    cout << "### freqid   = "
                         << dbChan.m_freqid.toLocal8Bit().constData() << endl;
                }
                cout << "### finetune = "
                     << dbChan.m_finetune                             << endl;
                cout << "### tvformat = "
                     << dbChan.m_tvformat.toLocal8Bit().constData()   << endl;
                cout << "### icon     = "
                     << dbChan.m_icon.toLocal8Bit().constData()       << endl;
                cout << "### " << endl;

                // The only thing the xmltv data supplies here is the icon
                (*i).m_name     = dbChan.m_name;
                (*i).m_callsign = dbChan.m_callsign;
                (*i).m_channum  = dbChan.m_channum;
                (*i).m_finetune = dbChan.m_finetune;
                (*i).m_freqid   = dbChan.m_freqid;
                (*i).m_tvformat = dbChan.m_tvformat;

                promptForChannelUpdates(i, dbChan.m_chanid);

                if ((*i).m_callsign.isEmpty())
                    (*i).m_callsign = dbChan.m_name;

                if (dbChan.m_name     != (*i).m_name ||
                    dbChan.m_callsign != (*i).m_callsign ||
                    dbChan.m_channum  != (*i).m_channum ||
                    dbChan.m_finetune != (*i).m_finetune ||
                    dbChan.m_freqid   != (*i).m_freqid ||
                    dbChan.m_icon     != localfile ||
                    dbChan.m_tvformat != (*i).m_tvformat)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET chanid = :CHANID, "
                                     "name = :NAME, callsign = :CALLSIGN, "
                                     "channum = :CHANNUM, finetune = :FINE, "
                                     "icon = :ICON, freqid = :FREQID, "
                                     "tvformat = :TVFORMAT "
                                     " WHERE xmltvid = :XMLTVID "
                                     "AND sourceid = :SOURCEID;");
                    subquery.bindValue(":CHANID",    dbChan.m_chanid);
                    subquery.bindValue(":NAME",     (*i).m_name);
                    subquery.bindValue(":CALLSIGN", (*i).m_callsign);
                    subquery.bindValue(":CHANNUM",  (*i).m_channum);
                    subquery.bindValue(":FINE",     (*i).m_finetune);
                    subquery.bindValue(":ICON",     localfile);
                    subquery.bindValue(":FREQID",   (*i).m_freqid);
                    subquery.bindValue(":TVFORMAT", (*i).m_tvformat);
                    subquery.bindValue(":XMLTVID",  (*i).m_xmltvid);
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
            else if ((dbChan.m_icon != localfile) ||
                     (dbChan.m_xmltvid != (*i).m_xmltvid))
            {
                LOG(VB_XMLTV, LOG_NOTICE, QString("Updating channel %1 (%2)")
                                        .arg(dbChan.m_name).arg(dbChan.m_chanid));

                if (localfile.isEmpty())
                    localfile = dbChan.m_icon;

                if (dbChan.m_xmltvid != (*i).m_xmltvid)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());

                    subquery.prepare("UPDATE channel SET icon = :ICON "
                            ", xmltvid:= :XMLTVID WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":XMLTVID", (*i).m_xmltvid);
                    subquery.bindValue(":CHANID", dbChan.m_chanid);

                    if (!subquery.exec())
                        MythDB::DBError("Channel icon change", subquery);
                }
                else
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET icon = :ICON WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":CHANID", dbChan.m_chanid);

                    if (!subquery.exec())
                        MythDB::DBError("Channel icon change", subquery);
                }

            }
        }
        else if (insertChan) // Only insert channels for non-scannable sources
        {
            int major = 0;
            int minor = 0;
            long long freq = 0;
            get_atsc_stuff((*i).m_channum, id, (*i).m_freqid.toInt(), major, minor, freq);

            if (m_interactive && ((minor == 0) || (freq > 0)))
            {
                cout << "### " << endl;
                cout << "### New channel found" << endl;
                cout << "### " << endl;
                cout << "### name     = "
                     << (*i).m_name.toLocal8Bit().constData()       << endl;
                cout << "### callsign = "
                     << (*i).m_callsign.toLocal8Bit().constData()   << endl;
                cout << "### channum  = "
                     << (*i).m_channum.toLocal8Bit().constData()    << endl;
                if (m_channelPreset)
                {
                    cout << "### freqid   = "
                         << (*i).m_freqid.toLocal8Bit().constData() << endl;
                }
                cout << "### finetune = "
                     << (*i).m_finetune                             << endl;
                cout << "### tvformat = "
                     << (*i).m_tvformat.toLocal8Bit().constData()   << endl;
                cout << "### icon     = "
                     << localfile.toLocal8Bit().constData()         << endl;
                cout << "### " << endl;

                uint chanid = promptForChannelUpdates(i,0);

                if ((*i).m_callsign.isEmpty())
                    (*i).m_callsign = QString::number(chanid);

                int mplexid = 0;
                if ((chanid > 0) && (minor > 0))
                    mplexid = ChannelUtil::CreateMultiplex(id,   "atsc",
                                                           freq, "8vsb");

                if (((mplexid > 0) || ((minor == 0) && (chanid > 0))) &&
                    ChannelUtil::CreateChannel(
                        mplexid,          id,               chanid,
                        (*i).m_callsign,  (*i).m_name,      (*i).m_channum,
                        0 /*service id*/, major,            minor,
                        false /*use on air guide*/, false /*hidden*/,
                        false /*hidden in guide*/,
                        (*i).m_freqid,    localfile,        (*i).m_tvformat,
                        (*i).m_xmltvid))
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
            else if ((minor == 0) || (freq > 0))
            {
                // We only do this if we are not asked to skip it with the
                // --update-guide-only (formerly --update) flag.
                int mplexid = 0;
                int chanid = 0;
                if (minor > 0)
                {
                    mplexid = ChannelUtil::CreateMultiplex(
                        id, "atsc", freq, "8vsb");
                }

                if ((mplexid > 0) || (minor == 0))
                    chanid = ChannelUtil::CreateChanID(id, (*i).m_channum);

                if ((*i).m_callsign.isEmpty())
                {
                    QStringList words = (*i).m_name.simplified().toUpper()
                        .split(" ");
                    QString callsign = "";
                    QString w1 = !words.empty() ? words[0] : QString();
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
                    (*i).m_callsign = callsign;
                }

                if (chanid > 0)
                {
                    QString cstr = (*i).m_channum;
                    if(m_channelPreset && cstr.isEmpty())
                        cstr = QString::number(chanid % 1000);

                    bool retval = ChannelUtil::CreateChannel(
                                                     mplexid, id,
                                                     chanid,
                                                     (*i).m_callsign,
                                                     (*i).m_name, cstr,
                                                     0 /*service id*/,
                                                     major, minor,
                                                     false /*use on air guide*/,
                                                     false /*hidden*/,
                                                     false /*hidden in guide*/,
                                                     (*i).m_freqid,
                                                     localfile,
                                                     (*i).m_tvformat,
                                                     (*i).m_xmltvid
                                                            );
                    if (!retval)
                        cout << "Channel " << chanid << " creation failed"
                             << endl;
                }
            }
        }
    }
}
