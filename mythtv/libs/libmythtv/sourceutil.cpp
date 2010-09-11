// -*- Mode: c++ -*-

// Qt headers
#include <QRegExp>

// MythTV headers
#include "sourceutil.h"
#include "cardutil.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "util.h"

bool SourceUtil::HasDigitalChannel(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT mplexid, atsc_minor_chan, serviceid "
        "FROM channel "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythDB::DBError("SourceUtil::HasDigitalChannel()", query);
        return false;
    }

    while (query.next())
    {
        uint mplexid = query.value(0).toUInt();
        uint minor   = query.value(1).toUInt();
        uint prognum = query.value(2).toUInt();
        mplexid = (32767 == mplexid) ? 0 : mplexid;

        if (mplexid && (minor || prognum))
            return true;
    }

    return false;
}

QString SourceUtil::GetSourceName(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT name "
        "FROM videosource "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythDB::DBError("SourceUtil::GetSourceName()", query);
        return QString::null;
    }
    else if (!query.next())
    {
        return QString::null;
    }

    return query.value(0).toString();
}

QString SourceUtil::GetChannelSeparator(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT channum "
                  "FROM channel "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        QMap<QString,uint> counts;
        const QRegExp sepExpr("(_|-|#|\\.)");
        while (query.next())
        {
            const QString channum = query.value(0).toString();
            const int where = channum.indexOf(sepExpr);
            if (channum.right(2).left(1) == "0")
                counts["0"]++;
            else
                counts[(where < 0) ? "" : QString(channum.at(where))]++;
        }
        QString sep = "_";
        uint max = counts["_"];
        static const char *spacers[6] = { "", "-", "#", ".", "0", NULL };
        for (uint i=0; (spacers[i] != NULL); ++i)
        {
            if (counts[spacers[i]] > max)
            {
                max = counts[spacers[i]];
                sep = spacers[i];
            }
        }
        return sep;
    }
    return "_"; // default on failure
}

QString SourceUtil::GetChannelFormat(uint sourceid)
{
    return QString("%1") + GetChannelSeparator(sourceid) + QString("%2");
}

uint SourceUtil::GetChannelCount(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT sum(1) "
                  "FROM channel "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);
    if (query.exec() && query.isActive() && query.next())
        return query.value(0).toUInt();
    return 0;
}

vector<uint> SourceUtil::GetMplexIDs(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    vector<uint> list;
    if (!query.exec())
    {
        MythDB::DBError("SourceUtil::GetMplexIDs()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

bool SourceUtil::GetListingsLoginData(uint sourceid,
                                      QString &grabber, QString &userid,
                                      QString &passwd,  QString &lineupid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT xmltvgrabber, userid, password, lineupid "
        "FROM videosource "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("SourceUtil::GetListingsLoginData()", query);
        return false;
    }

    if (!query.next())
        return false;

    grabber  = query.value(0).toString();
    userid   = query.value(1).toString();
    passwd   = query.value(2).toString();
    lineupid = query.value(3).toString();

    return true;
}

static QStringList get_cardtypes(uint sourceid)
{
    QStringList list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardtype, inputname "
        "FROM capturecard, cardinput "
        "WHERE capturecard.cardid = cardinput.cardid AND "
        "      cardinput.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_cardtypes()", query);
    else
    {
        while (query.next())
        {
/// BEGIN HACK HACK HACK -- return correct card type for child cards
            QString cardtype = query.value(0).toString().toUpper();
            QString inputname = query.value(1).toString().toUpper();
            cardtype = ((cardtype == "DVB") && (inputname.left(3) != "DVB")) ?
                "V4L" : cardtype;
/// END  HACK HACK HACK
            list += cardtype;
        }
    }

    return list;
}

uint SourceUtil::GetConnectionCount(uint sourceid)
{
    QStringList types = get_cardtypes(sourceid);
    return types.size();
}

bool SourceUtil::IsProperlyConnected(uint sourceid, bool strict)
{
    QStringList types = get_cardtypes(sourceid);
    QMap<QString,uint> counts;
    QStringList::const_iterator it = types.begin();
    for (; it != types.end(); ++it)
    {
        counts[*it]++;

        counts[CardUtil::IsEncoder(*it)    ? "ENCODER" : "NOT_ENCODER"]++;
        counts[CardUtil::IsUnscanable(*it) ? "NO_SCAN" : "SCAN"]++;

        if (CardUtil::IsTuningAnalog(*it))
            counts["ANALOG_TUNING"]++;
        else if (CardUtil::IsTuningDigital(*it))
            counts["DIGITAL_TUNING"]++;
        else
            counts["OTHER_TUNING"]++;
    }

    bool tune_mismatch = counts["ANALOG_TUNING"]   && counts["DIGITAL_TUNING"];
    bool enc_mismatch  = counts["ENCODER"]         && counts["NOT_ENCODER"];
    bool scan_mismatch = counts["SCAN"]            && counts["NO_SCAN"];
    bool fw_mismatch   = (counts["FIREWIRE"] &&
                          ((int)counts["FIREWIRE"] < types.size()));

    if (tune_mismatch)
    {
        uint a = counts["ANALOG_TUNERS"];
        uint d = counts["DIGITAL_TUNERS"];
        VERBOSE(VB_GENERAL, QString("SourceUtil::IsProperlyConnected(): ") +
                QString("Source ID %1 ").arg(sourceid) +
                QString("appears to be connected\n\t\t\tto %1 analog tuner%2,")
                .arg(a).arg((1 == a) ? "":"s") +
                QString("and %1 digital tuner%2.\n\t\t\t")
                .arg(d).arg((1 == d) ? "":"s") +
                QString("Can't mix analog and digital tuning information."));
    }

    if (enc_mismatch)
    {
        uint a = counts["ENCODER"];
        uint d = counts["NOT_ENCODER"];
        VERBOSE(VB_GENERAL, QString("SourceUtil::IsProperlyConnected(): ") +
                QString("Source ID %1 ").arg(sourceid) +
                QString("appears to be connected\n\t\t\tto %1 encoder%2, ")
                .arg(a).arg((1 == a) ? "":"s") +
                QString("and %1 non-encoder%2. ")
                .arg(d).arg((1 == d) ? "":"s") +
                QString("This is probably a bad idea."));
    }

    if (scan_mismatch)
    {
        uint a = counts["SCAN"];
        uint d = counts["NO_SCAN"];
        VERBOSE(VB_GENERAL, QString("SourceUtil::IsProperlyConnected(): ") +
                QString("Source ID %1 ").arg(sourceid) +
                QString("appears to be connected\n\t\t\t"
                        "to %1 scanable input%2, ")
                .arg(a).arg((1 == a) ? "":"s") +
                QString("and %1 non-scanable input%2. ")
                .arg(d).arg((1 == d) ? "":"s") +
                QString("This may be a problem."));
    }

    if (fw_mismatch)
    {
        VERBOSE(VB_GENERAL, QString("SourceUtil::IsProperlyConnected(): ") +
                QString(
                    "Source ID %1 appears to be connected\n\t\t\t"
                    "to both firewire and non-firewire inputs. "
                    "This is probably a bad idea.").arg(sourceid));
    }

    if (!strict)
        return !tune_mismatch;

    return !tune_mismatch && !enc_mismatch && !scan_mismatch && !fw_mismatch;
}

bool SourceUtil::IsEncoder(uint sourceid, bool strict)
{
    bool encoder = true;

    QStringList types = get_cardtypes(sourceid);
    QStringList::const_iterator it = types.begin();
    for (; it != types.end(); ++it)
        encoder &= CardUtil::IsEncoder(*it);

    // Source is connected, go by card types for type determination
    if (!types.empty())
        return encoder;

    // Try looking at channels if source is not connected,
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT atsc_minor_chan, serviceid "
        "FROM channel "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    bool has_any_chan = false;
    if (!query.exec() || !query.isActive())
        MythDB::DBError("SourceUtil::IsEncoder", query);
    else
    {
        while (query.next())
        {
            encoder &= !query.value(0).toInt() && !query.value(1).toInt();
            has_any_chan = true;
        }
    }

    return (strict && !has_any_chan) ? false: encoder;
}

bool SourceUtil::IsUnscanable(uint sourceid)
{
    bool unscanable = true;
    QStringList types = get_cardtypes(sourceid);
    QStringList::const_iterator it = types.begin();
    for (; it != types.end(); ++it)
        unscanable &= CardUtil::IsUnscanable(*it);

    return types.empty() || unscanable;
}

bool SourceUtil::IsAnySourceScanable(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT sourceid FROM videosource");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("SourceUtil::IsAnySourceScanable", query);
        return false;
    }

    while (query.next())
    {
        if (!IsUnscanable(query.value(0).toUInt()))
            return true;
    }

    return false;
}

bool SourceUtil::UpdateChannelsFromListings(uint sourceid, QString cardtype)
{
    QString cmd = "mythfilldatabase --only-update-channels ";
    if (sourceid)
        cmd += QString("--sourceid %1 ").arg(sourceid);
    if (!cardtype.isEmpty())
        cmd += QString("--cardtype %1 ").arg(cardtype);

    myth_system(cmd);

    return true;
}

bool SourceUtil::DeleteSource(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Delete the channels associated with the source
    query.prepare("DELETE FROM channel "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Deleting Channels", query);
        return false;
    }

    // Delete the inputs associated with the source
    query.prepare("DELETE FROM cardinput "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Deleting cardinputs", query);
        return false;
    }

    // Delete the source itself
    query.prepare("DELETE FROM videosource "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Deleting VideoSource", query);
        return false;
    }

    // Delete any orphaned inputs & unused input groups
    CardUtil::DeleteOrphanInputs();
    CardUtil::UnlinkInputGroup(0,0);

    return true;
}

bool SourceUtil::DeleteAllSources(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    return (query.exec("TRUNCATE TABLE channel") &&
            query.exec("TRUNCATE TABLE program") &&
            query.exec("TRUNCATE TABLE videosource") &&
            query.exec("TRUNCATE TABLE credits") &&
            query.exec("TRUNCATE TABLE programrating") &&
            query.exec("TRUNCATE TABLE programgenres") &&
            query.exec("TRUNCATE TABLE dtv_multiplex") &&
            query.exec("TRUNCATE TABLE inputgroup") &&
            query.exec("TRUNCATE TABLE diseqc_config") &&
            query.exec("TRUNCATE TABLE diseqc_tree") &&
            query.exec("TRUNCATE TABLE eit_cache") &&
            query.exec("TRUNCATE TABLE channelgroup") &&
            query.exec("TRUNCATE TABLE channelgroupnames") &&
            query.exec("TRUNCATE TABLE cardinput"));
}
