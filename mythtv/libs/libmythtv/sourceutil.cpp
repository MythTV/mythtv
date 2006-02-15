// -*- Mode: c++ -*-

// Qt headers
#include <qregexp.h>

// MythTV headers
#include "sourceutil.h"
#include "mythdbcon.h"

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
