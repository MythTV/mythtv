// checksetup.cpp
//
// Some functions to do simple sanity checks on the MythTV setup.
// CheckSetup() is currently meant for the mythtv-setup program,
// but the other functions could probably be called from anywhere.
// They all return true if any problems are found.

#include <qstring.h>
#include <qdir.h>
#include "libmyth/mythdbcon.h"
#include "libmyth/mythcontext.h"


static bool checkPath(QString path, QString *probs)
{
    QDir dir(path);
    if (!dir.exists())
    {
        probs->append(QObject::tr("Path"));
        probs->append(QString(" %1 ").arg(path));
        probs->append(QObject::tr("doesn't exist"));
        probs->append(".\n");
        return true;
    }

    QFile test(path.append("/.test"));
    if (test.open(IO_WriteOnly))
        test.remove();
    else
    {
        probs->append(QObject::tr("Cannot create a file"));
        probs->append(QString(" %1 - ").arg(path));
        probs->append(QObject::tr("directory is not writable?"));
        probs->append("\n");
        return true;
    }

    return false;
}


// Do the recording and Live TV filesystem paths exist? Are they writable?
// Note that this should be checking by hostname, but doesn't yet.

bool checkStoragePaths(QString *probs)
{
    bool problemFound = false;

    MSqlQuery *query = new MSqlQuery(MSqlQuery::InitCon());

    query->prepare("SELECT data FROM settings"
                   " WHERE value='RecordFilePrefix' AND hostname = :HOSTNAME;");
    query->bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query->exec() || !query->isActive())
    {
        MythContext::DBError("checkStoragePaths", *query);
        return false;
    }

    if (query->size())
    {
        query->first();
        if (checkPath(query->value(0).toString(), probs))
            problemFound = true;
    }
    else
        VERBOSE(VB_GENERAL, QString("RecordFilePrefix is not set?"));


    MSqlQuery *query2 = new MSqlQuery(MSqlQuery::InitCon());

    query2->prepare("SELECT data FROM settings"
                    " WHERE value='LiveBufferDir' AND hostname = :HOSTNAME;");
    query2->bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query2->exec() || !query2->isActive())
    {
        MythContext::DBError("checkStoragePaths", *query2);
        return false;
    }

    if (query2->size())
    {
        query2->first();
        if (query->value(0).toString() != query2->value(0).toString())
            if (checkPath(query2->value(0).toString(), probs))
                problemFound = true;
    }
    else
        VERBOSE(VB_GENERAL, QString("LiveBufferDir is not set?"));

    delete query; delete query2;

    return problemFound;
}


// I keep forgetting to change the preset (starting channel) when I add cards,
// so this checks that the assigned channel (which may be the default of 3)
// actually exists. This should save a few beginner Live TV problems

bool checkChannelPresets(QString *probs)
{
    bool problemFound = false;

    MSqlQuery *query = new MSqlQuery(MSqlQuery::InitCon());

    query->prepare("SELECT cardid, startchan, sourceid, inputname"
                   " FROM cardinput;");

    if (!query->exec() || !query->isActive())
    {
        MythContext::DBError("checkChannelPresets", *query);
        return false;
    }

    while (query->next())
    {
        int cardid    = query->value(0).toInt();
        QString startchan = query->value(1).toString();
        int sourceid  = query->value(2).toInt();

        if (query->value(1).toString() == "")    // Logic from tv_rec.cpp
            startchan = "3";

        MSqlQuery channelExists(MSqlQuery::InitCon());
        QString   channelQuery;
        channelQuery = QString("SELECT chanid FROM channel"
                               " WHERE channum=\"%1\" AND sourceid=%2;")
                              .arg(startchan).arg(sourceid);
        channelExists.prepare(channelQuery);

        if (!channelExists.exec() || !channelExists.isActive())
        {
            MythContext::DBError("checkChannelPresets", channelExists);
            return problemFound;
        }

        if (channelExists.size() == 0)
        {
            probs->append(QObject::tr("Card"));
            probs->append(QString(" %1 (").arg(cardid));
            probs->append(QObject::tr("type"));
            probs->append(QString(" %1) ").arg(query->value(3).toString()));
            probs->append(QObject::tr("is set to start on channel"));
            probs->append(QString(" %1, ").arg(startchan));
            probs->append(QObject::tr("which does not exist"));
            probs->append(".\n");
            problemFound = true;
        }
    }

    delete query;

    return problemFound;
}


bool CheckSetup(QString *problems)
{
    return checkStoragePaths(problems)
        || checkChannelPresets(problems);
}
