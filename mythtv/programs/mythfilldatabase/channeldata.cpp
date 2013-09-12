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
    bool insert_channels = m_channelUpdates;
    if (!insert_channels)
    {
        bool isEncoder, isUnscanable;
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
        insert_channels = (isCableCard || isEncoder || isUnscanable) &&
                           !m_removeNewChannels;
    }

    return insert_channels;
}


unsigned int ChannelData::promptForChannelUpdates(
    ChannelInfoList::iterator chaninfo, unsigned int chanid)
{
    (*chaninfo).name = getResponse(QObject::tr("Choose a channel name (any string, "
                                   "long version) "),(*chaninfo).name);
    (*chaninfo).callsign = getResponse(QObject::tr("Choose a channel callsign (any string, "
                                       "short version) "),(*chaninfo).callsign);

    if (m_channelPreset)
    {
        (*chaninfo).channum = getResponse(QObject::tr("Choose a channel preset (0..999) "),
                                         (*chaninfo).channum);
        (*chaninfo).freqid  = getResponse(QObject::tr("Choose a frequency id "),
                                          (*chaninfo).freqid);
    }
    else
    {
        (*chaninfo).channum  = getResponse(QObject::tr("Choose a channel number "),
                                           (*chaninfo).channum);
        (*chaninfo).freqid = (*chaninfo).channum;
    }

    (*chaninfo).finetune = getResponse(QObject::tr("Choose a channel fine tune offset "),
                                       QString::number((*chaninfo).finetune)).toInt();

    (*chaninfo).tvformat = getResponse(QObject::tr("Choose a TV format "
                                       "(PAL/SECAM/NTSC/ATSC/Default) "),
                                       (*chaninfo).tvformat);

    (*chaninfo).icon = getResponse(QObject::tr("Choose a channel icon image "
                                   "(relative path to icon storage group) "),
                                   (*chaninfo).icon);

    return(chanid);
}

QString ChannelData::normalizeChannelKey(const QString &chanName) const
{
    QString result = chanName;

    // Lowercase
    result = result.toLower();
    // Strip all whitespace
    result = result.replace(" ", "");
    
    return result;
}

QHash<QString, ChannelInfo> ChannelData::channelList(int sourceId)
{
    QHash<QString, ChannelInfo> retList;

    ChannelInfoList channelList = ChannelUtil::GetChannels(sourceId, true);
    
    ChannelInfoList::iterator it = channelList.begin();
    for ( ; it != channelList.end(); ++it)
    {
        QString chanName = (*it).name;
        QString key  = normalizeChannelKey(chanName);
        retList[key] = (*it);
    }

    return retList;
}

ChannelInfo ChannelData::FindMatchingChannel(const ChannelInfo &chanInfo,
                            QHash<QString, ChannelInfo> existingChannels) const
{
    QHash<QString, ChannelInfo>::iterator it;
    for (it = existingChannels.begin(); it != existingChannels.end(); ++it)
    {
        if ((*it).xmltvid == chanInfo.xmltvid)
            return (*it);
    }

    QString searchKey = normalizeChannelKey(chanInfo.name);
    ChannelInfo existChan = existingChannels.value(searchKey);

    return existChan;
}

void ChannelData::handleChannels(int id, ChannelInfoList *chanlist)
{
    if (m_guideDataOnly)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Skipping Channel Updates");
        return;
    }

    QHash<QString, ChannelInfo> existingChannels = channelList(id);
    QString fileprefix = SetupIconCacheDirectory();

    QDir::setCurrent(fileprefix);

    fileprefix += "/";

    ChannelInfoList::iterator i = chanlist->begin();
    for (; i != chanlist->end(); ++i)
    {
        if ((*i).xmltvid.isEmpty())
            continue;

        QString localfile;

        if (!(*i).icon.isEmpty())
        {
            QDir remotefile = QDir((*i).icon);
            QString filename = remotefile.dirName();

            localfile = fileprefix + filename;
            QFile actualfile(localfile);
            if (!actualfile.exists() &&
                !GetMythDownloadManager()->download((*i).icon, localfile))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to fetch icon from '%1'")
                        .arg((*i).icon));
            }

            localfile = filename;
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
                LOG(VB_GENERAL, LOG_INFO,
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
        
        ChannelInfo dbChan = FindMatchingChannel(*i, existingChannels);
        if (dbChan.chanid > 0) // Channel exists, updating
        {
            LOG(VB_XMLTV, LOG_NOTICE,
                    QString("Match found for xmltvid %1 to channel %2 (%3)")
                        .arg((*i).xmltvid).arg(dbChan.name).arg(dbChan.chanid));
            if (m_interactive)
            {

                cout << "### " << endl;
                cout << "### Existing channel found" << endl;
                cout << "### " << endl;
                cout << "### xmltvid  = "
                     << (*i).xmltvid.toLocal8Bit().constData()        << endl;
                cout << "### chanid   = "
                     << dbChan.chanid                                 << endl;
                cout << "### name     = "
                     << dbChan.name.toLocal8Bit().constData()         << endl;
                cout << "### callsign = "
                     << dbChan.callsign.toLocal8Bit().constData()     << endl;
                cout << "### channum  = "
                     << dbChan.channum.toLocal8Bit().constData()      << endl;
                if (m_channelPreset)
                {
                    cout << "### freqid   = "
                         << dbChan.freqid.toLocal8Bit().constData()   << endl;
                }
                cout << "### finetune = "
                     << dbChan.finetune                               << endl;
                cout << "### tvformat = "
                     << dbChan.tvformat.toLocal8Bit().constData()     << endl;
                cout << "### icon     = "
                     << dbChan.icon.toLocal8Bit().constData()         << endl;
                cout << "### " << endl;

                // The only thing the xmltv data supplies here is the icon
                (*i).name = dbChan.name;
                (*i).callsign = dbChan.callsign;
                (*i).channum  = dbChan.channum;
                (*i).finetune = dbChan.finetune;
                (*i).freqid = dbChan.freqid;
                (*i).tvformat = dbChan.tvformat;

                promptForChannelUpdates(i, dbChan.chanid);

                if ((*i).callsign.isEmpty())
                    (*i).callsign = dbChan.name;

                if (dbChan.name     != (*i).name ||
                    dbChan.callsign != (*i).callsign ||
                    dbChan.channum  != (*i).channum ||
                    dbChan.finetune != (*i).finetune ||
                    dbChan.freqid   != (*i).freqid ||
                    dbChan.icon     != localfile ||
                    dbChan.tvformat != (*i).tvformat)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET chanid = :CHANID, "
                                     "name = :NAME, callsign = :CALLSIGN, "
                                     "channum = :CHANNUM, finetune = :FINE, "
                                     "icon = :ICON, freqid = :FREQID, "
                                     "tvformat = :TVFORMAT "
                                     " WHERE xmltvid = :XMLTVID "
                                     "AND sourceid = :SOURCEID;");
                    subquery.bindValue(":CHANID", dbChan.chanid);
                    subquery.bindValue(":NAME", (*i).name);
                    subquery.bindValue(":CALLSIGN", (*i).callsign);
                    subquery.bindValue(":CHANNUM", (*i).channum);
                    subquery.bindValue(":FINE", (*i).finetune);
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
            else if ((dbChan.icon != localfile) ||
                     (dbChan.xmltvid != (*i).xmltvid))
            {
                LOG(VB_XMLTV, LOG_NOTICE, QString("Updating channel %1 (%2)")
                                        .arg(dbChan.name).arg(dbChan.chanid));

                if (localfile.isEmpty())
                    localfile = dbChan.icon;

                if (dbChan.xmltvid != (*i).xmltvid)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());

                    subquery.prepare("UPDATE channel SET icon = :ICON "
                            ", xmltvid:= :XMLTVID WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":XMLTVID", (*i).xmltvid);
                    subquery.bindValue(":CHANID", dbChan.chanid);

                    if (!subquery.exec())
                        MythDB::DBError("Channel icon change", subquery);
                }
                else
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET icon = :ICON WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":CHANID", dbChan.chanid);

                    if (!subquery.exec())
                        MythDB::DBError("Channel icon change", subquery);
                }

            }
        }
        else if (insert_chan(id)) // Only insert channels for non-scannable sources
        {
            int major, minor = 0;
            long long freq = 0;
            get_atsc_stuff((*i).channum, id, (*i).freqid.toInt(), major, minor, freq);

            if (m_interactive && ((minor == 0) || (freq > 0)))
            {
                cout << "### " << endl;
                cout << "### New channel found" << endl;
                cout << "### " << endl;
                cout << "### name     = "
                     << (*i).name.toLocal8Bit().constData()     << endl;
                cout << "### callsign = "
                     << (*i).callsign.toLocal8Bit().constData() << endl;
                cout << "### channum  = "
                     << (*i).channum.toLocal8Bit().constData()  << endl;
                if (m_channelPreset)
                {
                    cout << "### freqid   = "
                         << (*i).freqid.toLocal8Bit().constData() << endl;
                }
                cout << "### finetune = "
                     << (*i).finetune                           << endl;
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
                        (*i).callsign,    (*i).name,        (*i).channum,
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
            else if ((minor == 0) || (freq > 0))
            {
                // We only do this if we are not asked to skip it with the
                // --update-guide-only (formerly --update) flag.
                int mplexid = 0, chanid = 0;
                if (minor > 0)
                {
                    mplexid = ChannelUtil::CreateMultiplex(
                        id, "atsc", freq, "8vsb");
                }

                if ((mplexid > 0) || (minor == 0))
                    chanid = ChannelUtil::CreateChanID(id, (*i).channum);

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
                    QString cstr = QString((*i).channum);
                    if(m_channelPreset && cstr.isEmpty())
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
