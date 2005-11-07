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
#include "libmyth/util.h"


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


bool checkBufferSize(QString path, QString *probs)
{
    bool problemFound = false;

    MSqlQuery *query = new MSqlQuery(MSqlQuery::InitCon());

    query->prepare("SELECT data FROM settings"
                   " WHERE value='BufferSize' AND hostname = :HOSTNAME;");
    query->bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query->exec() || !query->isActive())
    {
        MythContext::DBError("checkBufferSize", *query);
        return false;
    }

    if (query->size())
    {
        query->first();
        int bufferSize = query->value(0).toInt();

        // Count the number of cards (each of which may create a buffer file).
        // Note there might be a better way to do this somewhere in libmythtv?

        int numCards = 0;
        MSqlQuery *cardsQuery = new MSqlQuery(MSqlQuery::InitCon());

        cardsQuery->prepare("SELECT cardinputid FROM cardinput;");

        if (!cardsQuery->exec() || !cardsQuery->isActive())
        {
            MythContext::DBError("checkBufferSize", *cardsQuery);
            delete query;
            return false;
        }

        while (cardsQuery->next())
            ++numCards;

        // How much space on volume? Enough for bufferSize * numCards?

        long long free, total, used;

        free = getDiskSpace(path, total, used);

        // Note that free and total are in KB, not MB or GB.
        if (free > -1 && bufferSize * 1024 * 1024 * numCards > free)
        {
            probs->append(QObject::tr("Not enough space for buffers on disk"));
            probs->append(QString(" %1 - %2 ").arg(path).arg(numCards));
            probs->append(QObject::tr("cards at"));
            probs->append(QString(" %1 GB ").arg(bufferSize));
            probs->append(QObject::tr("each is larger than"));
            probs->append(QString(" %1 MB ").arg(free/1024));
            probs->append(QObject::tr("free space"));
            probs->append("\n");

            if (bufferSize * 1024 * 1024 * numCards > total)
            {
                probs->append(QObject::tr("It is also larger than the total drive size of"));
                probs->append(QString(" %1 GB").arg(total/1024/1024));
                probs->append("\n");
            }

            problemFound = true;
        }

        delete cardsQuery;
    }
    else
        VERBOSE(VB_GENERAL, QString("BufferSize is not set?"));

    delete query;

    return problemFound;
}


// Do the recording and Live TV filesystem paths exist? Are they writable?
// Is the Live TV filesystem large enough?

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

        QString bufferPath = query2->value(0).toString();

        // Only check this path if it is different to the recordings path:
        if (query->value(0).toString() != bufferPath)
            if (checkPath(bufferPath, probs))
                problemFound = true;

        if (checkBufferSize(bufferPath, probs))
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
                               " WHERE channum='%1' AND sourceid=%2;")
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
