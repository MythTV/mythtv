// -*- Mode: c++ -*-

// Qt headers
#include <QRegExp>

// MythTV headers
#include "sourceutil.h"
#include "cardutil.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythsystemlegacy.h"

bool SourceUtil::HasDigitalChannel(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT mplexid, atsc_minor_chan, serviceid "
        "FROM channel "
        "WHERE deleted IS NULL AND "
        "      sourceid = :SOURCEID");
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
        return QString();
    }
    if (!query.next())
    {
        return QString();
    }

    return query.value(0).toString();
}

uint SourceUtil::GetSourceID(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT sourceid "
        "FROM videosource "
        "WHERE name = :NAME");
    query.bindValue(":NAME", name);

    if (!query.exec())
    {
        MythDB::DBError("SourceUtil::GetSourceID()", query);
        return 0;
    }
    if (!query.next())
    {
        return 0;
    }

    return query.value(0).toUInt();
}

QString SourceUtil::GetChannelSeparator(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT channum "
                  "FROM channel "
                  "WHERE deleted IS NULL AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        QMap<QString,uint> counts;
        const QRegExp sepExpr("(_|-|#|\\.)");
        while (query.next())
        {
            const QString channum = query.value(0).toString();
            const int where = channum.indexOf(sepExpr);
            if (channum.right(2).startsWith("0"))
                counts["0"]++;
            else
                counts[(where < 0) ? "" : QString(channum.at(where))]++;
        }
        QString sep = "_";
        uint max = counts["_"];
        static const char *s_spacers[6] = { "", "-", "#", ".", "0", nullptr };
        for (uint i=0; (s_spacers[i] != nullptr); ++i)
        {
            if (counts[s_spacers[i]] > max)
            {
                max = counts[s_spacers[i]];
                sep = s_spacers[i];
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
                  "WHERE deleted IS NULL AND "
                  "      sourceid = :SOURCEID");
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

static QStringList get_inputtypes(uint sourceid)
{
    QStringList list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardtype, inputname "
        "FROM capturecard "
        "WHERE capturecard.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_inputtypes()", query);
    else
    {
        while (query.next())
        {
/// BEGIN HACK HACK HACK -- return correct input type for child inputs
            QString inputtype = query.value(0).toString().toUpper();
            QString inputname = query.value(1).toString().toUpper();
            inputtype = ((inputtype == "DVB") && (!inputname.startsWith("DVB"))) ?
                "V4L" : inputtype;
/// END  HACK HACK HACK
            list += inputtype;
        }
    }

    return list;
}

uint SourceUtil::GetConnectionCount(uint sourceid)
{
    QStringList types = get_inputtypes(sourceid);
    return types.size();
}

bool SourceUtil::IsProperlyConnected(uint sourceid, bool strict)
{
    QStringList types = get_inputtypes(sourceid);
    QMap<QString,uint> counts;
    foreach (const auto & type, types)
    {
        counts[type]++;

        counts[CardUtil::IsEncoder(type)    ? "ENCODER" : "NOT_ENCODER"]++;
        counts[CardUtil::IsUnscanable(type) ? "NO_SCAN" : "SCAN"]++;

        if (CardUtil::IsTuningAnalog(type))
            counts["ANALOG_TUNING"]++;
        else if (CardUtil::IsTuningDigital(type))
            counts["DIGITAL_TUNING"]++;
        else if (CardUtil::IsTuningVirtual(type))
            counts["VIRTUAL_TUNING"]++;
    }

    bool tune_mismatch =
        (counts["ANALOG_TUNING"]  && counts["DIGITAL_TUNING"]) ||
        (counts["VIRTUAL_TUNING"] && counts["DIGITAL_TUNING"]);
    bool enc_mismatch  = counts["ENCODER"] && counts["NOT_ENCODER"];
    bool scan_mismatch = counts["SCAN"]    && counts["NO_SCAN"];

    if (tune_mismatch)
    {
        uint a = counts["ANALOG_TUNERS"];
        uint d = counts["DIGITAL_TUNERS"];
        uint v = counts["VIRTUAL_TUNERS"];
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("SourceUtil::IsProperlyConnected(): ") +
            QString("Connected to %1 analog, %2 digital and %3 virtual "
                    "tuners\n\t\t\t").arg(a).arg(d).arg(v) +
            QString("Can not mix digital with other tuning information."));
    }

    if (enc_mismatch)
    {
        uint a = counts["ENCODER"];
        uint d = counts["NOT_ENCODER"];
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("SourceUtil::IsProperlyConnected(): ") +
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
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("SourceUtil::IsProperlyConnected(): ") +
            QString("Source ID %1 ").arg(sourceid) +
            QString("appears to be connected\n\t\t\t"
                    "to %1 scanable input%2, ")
                .arg(a).arg((1 == a) ? "":"s") +
            QString("and %1 non-scanable input%2. ")
                .arg(d).arg((1 == d) ? "":"s") +
            QString("This may be a problem."));
    }

    if (!strict)
        return !tune_mismatch;

    return !tune_mismatch && !enc_mismatch && !scan_mismatch;
}

bool SourceUtil::IsEncoder(uint sourceid, bool strict)
{
    bool encoder = true;

    QStringList types = get_inputtypes(sourceid);
    foreach (const auto & type, types)
        encoder &= CardUtil::IsEncoder(type);

    // Source is connected, go by input types for type determination
    if (!types.empty())
        return encoder;

    // Try looking at channels if source is not connected,
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT atsc_minor_chan, serviceid "
        "FROM channel "
        "WHERE deleted IS NULL AND "
        "      sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    bool has_any_chan = false;
    if (!query.exec() || !query.isActive())
        MythDB::DBError("SourceUtil::IsEncoder", query);
    else
    {
        while (query.next())
        {
            encoder &= !query.value(0).toBool() && !query.value(1).toBool();
            has_any_chan = true;
        }
    }

    return (strict && !has_any_chan) ? false: encoder;
}

bool SourceUtil::IsUnscanable(uint sourceid)
{
    bool unscanable = true;
    QStringList types = get_inputtypes(sourceid);
    foreach (const auto & type, types)
        unscanable &= CardUtil::IsUnscanable(type);

    return types.empty() || unscanable;
}

bool SourceUtil::IsCableCardPresent(uint sourceid)
{
    bool ccpresent = false;
    vector<uint> inputs = CardUtil::GetInputIDs(sourceid);
    for (uint & input : inputs)
    {
        if (CardUtil::IsCableCardPresent(input, CardUtil::GetRawInputType(input))
            || CardUtil::GetRawInputType(input) == "HDHOMERUN")
            ccpresent = true;
    }

    return ccpresent;
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

bool SourceUtil::IsSourceIDValid(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid FROM videosource WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("SourceUtil::IsSourceIDValid", query);
        return false;
    }

    return query.next();
}

bool SourceUtil::UpdateChannelsFromListings(uint sourceid, const QString& inputtype, bool wait)
{
    if (wait)
    {
        QString cmd = GetAppBinDir() +
                      "mythfilldatabase";
        QStringList args;
        args.append("--only-update-channels");

        if (sourceid)
        {
            args.append(QString("--sourceid"));
            args.append(QString::number(sourceid));
        }
        if (!inputtype.isEmpty())
        {
            args.append(QString("--cardtype"));
            args.append(inputtype);
        }

        MythSystemLegacy getchan(cmd, args, kMSRunShell | kMSAutoCleanup );
        getchan.Run();
        getchan.Wait();
    }
    else
    {
        QString cmd = GetAppBinDir() +
                      "mythfilldatabase --only-update-channels";
        if (sourceid)
            cmd += QString(" --sourceid %1").arg(sourceid);
        if (!inputtype.isEmpty())
            cmd += QString(" --cardtype %1").arg(inputtype);
        cmd += logPropagateArgs;

        myth_system(cmd);
    }

    return true;
}

bool SourceUtil::UpdateSource( uint sourceid, const QString& sourcename,
                               const QString& grabber, const QString& userid,
                               const QString& freqtable, const QString& lineupid,
                               const QString& password, bool useeit,
                               const QString& configpath, int nitid,
                               uint bouquetid, uint regionid, uint scanfrequency)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE videosource SET name = :NAME, xmltvgrabber = :XMLTVGRABBER, "
                  "userid = :USERID, freqtable = :FREQTABLE, lineupid = :LINEUPID,"
                  "password = :PASSWORD, useeit = :USEEIT, configpath = :CONFIGPATH, "
                  "dvb_nit_id = :NITID, bouquet_id = :BOUQUETID, region_id = :REGIONID, "
                  "scanfrequency = :SCANFREQUENCY "
                  "WHERE sourceid = :SOURCEID");

    query.bindValue(":NAME", sourcename);
    query.bindValue(":XMLTVGRABBER", grabber);
    query.bindValue(":USERID", userid);
    query.bindValue(":FREQTABLE", freqtable);
    query.bindValue(":LINEUPID", lineupid);
    query.bindValue(":PASSWORD", password);
    query.bindValue(":USEEIT", useeit);
    query.bindValue(":CONFIGPATH", configpath.isEmpty() ? nullptr : configpath);
    query.bindValue(":NITID", nitid);
    query.bindValue(":BOUQUETID", bouquetid);
    query.bindValue(":REGIONID", regionid);
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":SCANFREQUENCY", scanfrequency);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Updating Video Source", query);
        return false;
    }

    return true;
}

int SourceUtil::CreateSource( const QString& sourcename,
                               const QString& grabber, const QString& userid,
                               const QString& freqtable, const QString& lineupid,
                               const QString& password, bool useeit,
                               const QString& configpath, int nitid,
                               uint bouquetid, uint regionid, uint scanfrequency)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO videosource (name,xmltvgrabber,userid,freqtable,lineupid,"
                  "password,useeit,configpath,dvb_nit_id,bouquet_id,region_id, scanfrequency) "
                  "VALUES (:NAME, :XMLTVGRABBER, :USERID, :FREQTABLE, :LINEUPID, :PASSWORD, "
                  ":USEEIT, :CONFIGPATH, :NITID, :BOUQUETID, :REGIONID, :SCANFREQUENCY)");

    query.bindValue(":NAME", sourcename);
    query.bindValue(":XMLTVGRABBER", grabber);
    query.bindValue(":USERID", userid);
    query.bindValue(":FREQTABLE", freqtable);
    query.bindValue(":LINEUPID", lineupid);
    query.bindValue(":PASSWORD", password);
    query.bindValue(":USEEIT", useeit);
    query.bindValue(":CONFIGPATH", configpath.isEmpty() ? nullptr : configpath);
    query.bindValue(":NITID", nitid);
    query.bindValue(":BOUQUETID", bouquetid);
    query.bindValue(":REGIONID", regionid);
    query.bindValue(":SCANFREQUENCY", scanfrequency);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Adding Video Source", query);
        return -1;
    }

    query.prepare("SELECT MAX(sourceid) FROM videosource");

    if (!query.exec())
    {
        MythDB::DBError("CreateSource maxsource", query);
        return -1;
    }

    int sourceid = -1; /* must be int not uint because of return type. */

    if (query.next())
        sourceid = query.value(0).toInt();

    return sourceid;
}

bool SourceUtil::DeleteSource(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Delete the channels associated with the source
    query.prepare("UPDATE channel "
                  "SET deleted = NOW() "
                  "WHERE deleted IS NULL AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Deleting Channels", query);
        return false;
    }

    // Detach the inputs associated with the source
    query.prepare("UPDATE capturecard "
                  "SET sourceid = 0 "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Deleting inputs", query);
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

    return true;
}

bool SourceUtil::DeleteAllSources(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Detach all inputs from any source
    query.prepare("UPDATE capturecard "
                  "SET sourceid = 0");
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Deleting sources", query);
        return false;
    }

    return (query.exec("TRUNCATE TABLE channel") &&
            query.exec("TRUNCATE TABLE program") &&
            query.exec("TRUNCATE TABLE videosource") &&
            query.exec("TRUNCATE TABLE credits") &&
            query.exec("TRUNCATE TABLE programrating") &&
            query.exec("TRUNCATE TABLE programgenres") &&
            query.exec("TRUNCATE TABLE dtv_multiplex") &&
            query.exec("TRUNCATE TABLE diseqc_config") &&
            query.exec("TRUNCATE TABLE diseqc_tree") &&
            query.exec("TRUNCATE TABLE eit_cache") &&
            query.exec("TRUNCATE TABLE channelgroup") &&
            query.exec("TRUNCATE TABLE channelgroupnames"));
}
