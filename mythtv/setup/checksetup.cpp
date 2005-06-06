// checksetup.cpp
//
// Some functions to do simple sanity checks on the MythTV setup.
// CheckSetup() is currently meant for the mythtv-setup program,
// but the other functions could probably be called from anywhere.

#include <qstring.h>
#include <qdir.h>
#include "libmyth/mythdbcon.h"
#include "libmyth/mythcontext.h"


static void checkPath(QString path, QString *probs)
{
    QDir dir(path);
    if (!dir.exists())
    {
        probs->append(QObject::tr("Path"));
        probs->append(QString(" %1 ").arg(path));
        probs->append(QObject::tr("doesn't exist"));
        probs->append(".\n");
        return;
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
    }
}


// Do the recording and Live TV filesystem paths exist? Are they writable?
// Note that this should be checking by hostname, but doesn't yet.

void checkStoragePaths(QString *probs)
{
    MSqlQuery *query = new MSqlQuery(MSqlQuery::InitCon());

    query->prepare("SELECT data FROM settings"
                   " WHERE value='RecordFilePrefix' AND hostname = :HOSTNAME;");
    query->bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query->exec() || !query->isActive())
    {
        MythContext::DBError("checkStoragePaths", *query);
        return;
    }

    if (query->size())
    {
        query->first();
        checkPath(query->value(0).toString(), probs);
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
        return;
    }

    if (query2->size())
    {
        query2->first();
        if (query->value(0).toString() != query2->value(0).toString())
            checkPath(query2->value(0).toString(), probs);
    }
    else
        VERBOSE(VB_GENERAL, QString("LiveBufferDir is not set?"));

    delete query; delete query2;
}


// I keep forgetting to change the preset (starting channel) when I add cards,
// so this checks that the assigned channel (which may be the default of 3)
// actually exists. This should save a few beginner Live TV problems

void checkChannelPresets(QString *probs)
{
    MSqlQuery *query = new MSqlQuery(MSqlQuery::InitCon());

    query->prepare("SELECT cardid, startchan, sourceid, inputname"
                   " FROM cardinput;");

    if (!query->exec() || !query->isActive())
    {
        MythContext::DBError("checkChannelPresets", *query);
        return;
    }

    while (query->next())
    {
        int cardid    = query->value(0).toInt();
        int startchan = query->value(1).toInt();
        int sourceid  = query->value(2).toInt();

        if (query->value(1).toString() == "")    // Logic from tv_rec.cpp
            startchan = 3;

        MSqlQuery channelExists(MSqlQuery::InitCon());
        QString   channelQuery;
        channelQuery = QString("SELECT chanid FROM channel"
                               " WHERE channum=%1 AND sourceid=%2;")
                              .arg(startchan).arg(sourceid);
        channelExists.prepare(channelQuery);

        if (!channelExists.exec() || !channelExists.isActive())
        {
            MythContext::DBError("checkChannelPresets", channelExists);
            return;
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
        }
    }

    delete query;
}


bool CheckSetup(QString *problems)
{
    checkStoragePaths(problems);
    checkChannelPresets(problems);

    if (problems->isNull())
        return false;
 
    return true;
}
