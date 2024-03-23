// C++ headers
#include <cstdlib>
#include <iostream>

// Qt headers
#include <QDir>
#include <QFile>
#include <QRegularExpression>

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/frequencytables.h"
#include "libmythtv/sourceutil.h"

// filldata headers
#include "channeldata.h"
#include "fillutil.h"

static const QRegularExpression parseMajorMinor { R"((\d+)\D(\d+))" };

static void get_atsc_stuff(const QString& channum, int sourceid, int freqid,
                           int &major, int &minor, long long &freq)
{
    major = freqid;
    minor = 0;

    auto match = parseMajorMinor.match(channum);
    if (!match.hasMatch())
        return;

    major = match.capturedView(1).toInt();
    minor = match.capturedView(2).toInt();

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

bool ChannelData::insert_chan(uint sourceid) const
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
    ChannelInfoList::iterator chaninfo, unsigned int chanid) const
{
    (*chaninfo).m_name = getResponse(QObject::tr("Choose a channel name (any string, "
                                     "long version) "),(*chaninfo).m_name);
    (*chaninfo).m_callSign = getResponse(QObject::tr("Choose a channel callsign (any string, "
                                         "short version) "),(*chaninfo).m_callSign);

    if (m_channelPreset)
    {
        (*chaninfo).m_chanNum = getResponse(QObject::tr("Choose a channel preset (0..999) "),
                                           (*chaninfo).m_chanNum);
        (*chaninfo).m_freqId  = getResponse(QObject::tr("Choose a frequency id "),
                                            (*chaninfo).m_freqId);
    }
    else
    {
        (*chaninfo).m_chanNum  = getResponse(QObject::tr("Choose a channel number "),
                                             (*chaninfo).m_chanNum);
        (*chaninfo).m_freqId = (*chaninfo).m_chanNum;
    }

    (*chaninfo).m_fineTune = getResponse(QObject::tr("Choose a channel fine tune offset "),
                                         QString::number((*chaninfo).m_fineTune)).toInt();

    (*chaninfo).m_tvFormat = getResponse(QObject::tr("Choose a TV format "
                                         "(PAL/SECAM/NTSC/ATSC/Default) "),
                                         (*chaninfo).m_tvFormat);

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
                                                            sourceId, 0,
                                                            false, "", "",
                                                            false);

    for (auto & channel : channelList)
    {
        QString chanName = channel.m_name;
        QString key  = normalizeChannelKey(chanName);
        retList.insert(key, channel);
    }

    return retList;
}

ChannelInfo ChannelData::FindMatchingChannel(const ChannelInfo &chanInfo,
                                             ChannelList existingChannels)
{
    ChannelList::iterator it;
    for (it = existingChannels.begin(); it != existingChannels.end(); ++it)
    {
        if ((*it).m_xmltvId == chanInfo.m_xmltvId)
            return (*it);
    }

    QString searchKey = normalizeChannelKey(chanInfo.m_name);
    ChannelInfo existChan = existingChannels.value(searchKey);

    if (existChan.m_chanId < 1)
    {
        // Check if it is ATSC
        auto match = parseMajorMinor.match(chanInfo.m_chanNum);
        if (match.hasMatch())
        {
            // Populate xmltvid for scanned ATSC channels
            uint major = match.capturedView(1).toUInt();
            uint minor = match.capturedView(2).toUInt();

            for (it = existingChannels.begin();
                 it != existingChannels.end(); ++it)
            {
                if ((*it).m_atscMajorChan == major &&
                    (*it).m_atscMinorChan == minor)
                    return (*it);
            }
        }
    }

    return existChan;
}

void ChannelData::handleChannels(int id, ChannelInfoList *chanlist) const
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
        if ((*i).m_xmltvId.isEmpty())
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

        if (!(*i).m_oldXmltvId.isEmpty())
        {
            query.prepare(
                "SELECT xmltvid "
                "FROM channel "
                "WHERE xmltvid = :XMLTVID");
            query.bindValue(":XMLTVID", (*i).m_oldXmltvId);

            if (!query.exec())
            {
                MythDB::DBError("xmltvid conversion 1", query);
            }
            else if (query.next())
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Converting old xmltvid (%1) to new (%2)")
                        .arg((*i).m_oldXmltvId, (*i).m_xmltvId));

                query.prepare("UPDATE channel "
                              "SET xmltvid = :NEWXMLTVID"
                              "WHERE xmltvid = :OLDXMLTVID");
                query.bindValue(":NEWXMLTVID", (*i).m_xmltvId);
                query.bindValue(":OLDXMLTVID", (*i).m_oldXmltvId);

                if (!query.exec())
                {
                    MythDB::DBError("xmltvid conversion 2", query);
                }
            }
        }

        ChannelInfo dbChan = FindMatchingChannel(*i, existingChannels);
        if (dbChan.m_chanId > 0) // Channel exists, updating
        {
            LOG(VB_XMLTV, LOG_DEBUG,
                    QString("Match found for xmltvid %1 to channel %2 (%3)")
                .arg((*i).m_xmltvId, dbChan.m_name, QString::number(dbChan.m_chanId)));
            if (m_interactive)
            {

                std::cout << "### " << std::endl;
                std::cout << "### Existing channel found" << std::endl;
                std::cout << "### " << std::endl;
                std::cout << "### xmltvid  = "
                          << (*i).m_xmltvId.toLocal8Bit().constData()    << std::endl;
                std::cout << "### chanid   = "
                          << dbChan.m_chanId                             << std::endl;
                std::cout << "### name     = "
                          << dbChan.m_name.toLocal8Bit().constData()     << std::endl;
                std::cout << "### callsign = "
                          << dbChan.m_callSign.toLocal8Bit().constData() << std::endl;
                std::cout << "### channum  = "
                          << dbChan.m_chanNum.toLocal8Bit().constData()  << std::endl;
                if (m_channelPreset)
                {
                    std::cout << "### freqid   = "
                              << dbChan.m_freqId.toLocal8Bit().constData() << std::endl;
                }
                std::cout << "### finetune = "
                          << dbChan.m_fineTune                           << std::endl;
                std::cout << "### tvformat = "
                          << dbChan.m_tvFormat.toLocal8Bit().constData() << std::endl;
                std::cout << "### icon     = "
                          << dbChan.m_icon.toLocal8Bit().constData()     << std::endl;
                std::cout << "### " << std::endl;

                // The only thing the xmltv data supplies here is the icon
                (*i).m_name     = dbChan.m_name;
                (*i).m_callSign = dbChan.m_callSign;
                (*i).m_chanNum  = dbChan.m_chanNum;
                (*i).m_fineTune = dbChan.m_fineTune;
                (*i).m_freqId   = dbChan.m_freqId;
                (*i).m_tvFormat = dbChan.m_tvFormat;

                promptForChannelUpdates(i, dbChan.m_chanId);

                if ((*i).m_callSign.isEmpty())
                    (*i).m_callSign = dbChan.m_name;

                if (dbChan.m_name     != (*i).m_name ||
                    dbChan.m_callSign != (*i).m_callSign ||
                    dbChan.m_chanNum  != (*i).m_chanNum ||
                    dbChan.m_fineTune != (*i).m_fineTune ||
                    dbChan.m_freqId   != (*i).m_freqId ||
                    dbChan.m_icon     != localfile ||
                    dbChan.m_tvFormat != (*i).m_tvFormat)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET chanid = :CHANID, "
                                     "name = :NAME, callsign = :CALLSIGN, "
                                     "channum = :CHANNUM, finetune = :FINE, "
                                     "icon = :ICON, freqid = :FREQID, "
                                     "tvformat = :TVFORMAT "
                                     " WHERE xmltvid = :XMLTVID "
                                     "AND sourceid = :SOURCEID;");
                    subquery.bindValue(":CHANID",    dbChan.m_chanId);
                    subquery.bindValue(":NAME",     (*i).m_name);
                    subquery.bindValue(":CALLSIGN", (*i).m_callSign);
                    subquery.bindValue(":CHANNUM",  (*i).m_chanNum);
                    subquery.bindValue(":FINE",     (*i).m_fineTune);
                    subquery.bindValue(":ICON",     localfile);
                    subquery.bindValue(":FREQID",   (*i).m_freqId);
                    subquery.bindValue(":TVFORMAT", (*i).m_tvFormat);
                    subquery.bindValue(":XMLTVID",  (*i).m_xmltvId);
                    subquery.bindValue(":SOURCEID", id);

                    if (!subquery.exec())
                    {
                        MythDB::DBError("update failed", subquery);
                    }
                    else
                    {
                        std::cout << "### " << std::endl;
                        std::cout << "### Change performed" << std::endl;
                        std::cout << "### " << std::endl;
                    }
                }
                else
                {
                    std::cout << "### " << std::endl;
                    std::cout << "### Nothing changed" << std::endl;
                    std::cout << "### " << std::endl;
                }
            }
            else if ((dbChan.m_icon != localfile) ||
                     (dbChan.m_xmltvId != (*i).m_xmltvId))
            {
                LOG(VB_XMLTV, LOG_NOTICE, QString("Updating channel %1 (%2)")
                                        .arg(dbChan.m_name).arg(dbChan.m_chanId));

                if (localfile.isEmpty())
                    localfile = dbChan.m_icon;

                if (dbChan.m_xmltvId != (*i).m_xmltvId)
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());

                    subquery.prepare("UPDATE channel SET icon = :ICON "
                            ", xmltvid:= :XMLTVID WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":XMLTVID", (*i).m_xmltvId);
                    subquery.bindValue(":CHANID", dbChan.m_chanId);

                    if (!subquery.exec())
                        MythDB::DBError("Channel icon change", subquery);
                }
                else
                {
                    MSqlQuery subquery(MSqlQuery::InitCon());
                    subquery.prepare("UPDATE channel SET icon = :ICON WHERE "
                                     "chanid = :CHANID;");
                    subquery.bindValue(":ICON", localfile);
                    subquery.bindValue(":CHANID", dbChan.m_chanId);

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
            get_atsc_stuff((*i).m_chanNum, id, (*i).m_freqId.toInt(), major, minor, freq);

            if (m_interactive && ((minor == 0) || (freq > 0)))
            {
                std::cout << "### " << std::endl;
                std::cout << "### New channel found" << std::endl;
                std::cout << "### " << std::endl;
                std::cout << "### name     = "
                          << (*i).m_name.toLocal8Bit().constData()       << std::endl;
                std::cout << "### callsign = "
                          << (*i).m_callSign.toLocal8Bit().constData()   << std::endl;
                std::cout << "### channum  = "
                          << (*i).m_chanNum.toLocal8Bit().constData()    << std::endl;
                if (m_channelPreset)
                {
                    std::cout << "### freqid   = "
                              << (*i).m_freqId.toLocal8Bit().constData() << std::endl;
                }
                std::cout << "### finetune = "
                          << (*i).m_fineTune                             << std::endl;
                std::cout << "### tvformat = "
                          << (*i).m_tvFormat.toLocal8Bit().constData()   << std::endl;
                std::cout << "### icon     = "
                          << localfile.toLocal8Bit().constData()         << std::endl;
                std::cout << "### " << std::endl;

                uint chanid = promptForChannelUpdates(i,0);

                if ((*i).m_callSign.isEmpty())
                    (*i).m_callSign = QString::number(chanid);

                int mplexid = 0;
                if ((chanid > 0) && (minor > 0))
                    mplexid = ChannelUtil::CreateMultiplex(id,   "atsc",
                                                           freq, "8vsb");

                if (((mplexid > 0) || ((minor == 0) && (chanid > 0))) &&
                    ChannelUtil::CreateChannel(
                        mplexid,          id,               chanid,
                        (*i).m_callSign,  (*i).m_name,      (*i).m_chanNum,
                        0 /*service id*/, major,            minor,
                        false /*use on air guide*/, kChannelVisible,
                        (*i).m_freqId,    localfile,        (*i).m_tvFormat,
                        (*i).m_xmltvId))
                {
                    std::cout << "### " << std::endl;
                    std::cout << "### Channel inserted" << std::endl;
                    std::cout << "### " << std::endl;
                }
                else
                {
                    std::cout << "### " << std::endl;
                    std::cout << "### Channel skipped" << std::endl;
                    std::cout << "### " << std::endl;
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
                    chanid = ChannelUtil::CreateChanID(id, (*i).m_chanNum);

                if ((*i).m_callSign.isEmpty())
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
                    (*i).m_callSign = callsign;
                }

                if (chanid > 0)
                {
                    QString cstr = (*i).m_chanNum;
                    if(m_channelPreset && cstr.isEmpty())
                        cstr = QString::number(chanid % 1000);

                    bool retval = ChannelUtil::CreateChannel(
                                                     mplexid, id,
                                                     chanid,
                                                     (*i).m_callSign,
                                                     (*i).m_name, cstr,
                                                     0 /*service id*/,
                                                     major, minor,
                                                     false /*use on air guide*/,
                                                     kChannelVisible,
                                                     (*i).m_freqId,
                                                     localfile,
                                                     (*i).m_tvFormat,
                                                     (*i).m_xmltvId
                                                            );
                    if (!retval)
                        std::cout << "Channel " << chanid << " creation failed"
                                  << std::endl;
                }
            }
        }
    }
}
