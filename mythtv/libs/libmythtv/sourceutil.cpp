// -*- Mode: c++ -*-

// Qt headers
#include <qregexp.h>

// MythTV headers
#include "sourceutil.h"
#include "mythdbcon.h"
#include "util.h"

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
            const int where = channum.find(sepExpr);
            if (channum.right(2).left(1) == "0")
                counts["0"]++;
            else
                counts[(where < 0) ? "" : QString(channum.at(where))]++;
        }
        QString sep = "_";
        uint max = counts["_"];
        static char *spacers[6] = { "", "-", "#", ".", "0", NULL };
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

    if (!query.exec() && !query.isActive())
    {
        MythContext::DBError("SourceUtil::GetListingsLoginData()", query);
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

bool SourceUtil::IsAnalog(uint sourceid)
{
    bool analog = false;
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "SELECT cardtype "
        "FROM capturecard, cardinput "
        "WHERE capturecard.cardid = cardinput.cardid AND "
        "      cardinput.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (query.exec() && query.next())
    {
        analog = true;
        do analog &= ((query.value(0).toString().upper() != "DVB") &&
                      (query.value(0).toString().upper() != "HDTV"));
        while (query.next());
    }

    return analog;
}

bool SourceUtil::UpdateChannelsFromListings(uint sourceid)
{
    myth_system(QString("mythfilldatabase --only-update-channels "
                        "--sourceid %1").arg(sourceid));
    return true;
}
