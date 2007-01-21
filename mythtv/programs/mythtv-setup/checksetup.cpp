// checksetup.cpp
//
// Some functions to do simple sanity checks on the MythTV setup.
// CheckSetup() is currently meant for the mythtv-setup program,
// but the other functions could probably be called from anywhere.
// They all return true if any problems are found, and add to a
// caller-supplied QString a message describing the problem.

#include <qstring.h>
#include <qdir.h>
#include "libmyth/mythdbcon.h"
#include "libmyth/mythcontext.h"
#include "libmyth/util.h"

/// Check that a directory path exists and is writable

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

/// Do the Storage Group filesystem paths exist? Are they writable?
/// Is the Live TV filesystem large enough?

bool checkStoragePaths(QString *probs)
{
    bool problemFound = false;

    QString recordFilePrefix = gContext->GetSetting("RecordFilePrefix", "EMPTY");

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT count(groupname) FROM storagegroup;");
    if (!query.exec() || !query.isActive() || query.size() < 1)
    {
        MythContext::DBError("checkStoragePaths", query);
        return false;
    }

    query.next();
    if (query.value(0).toInt() == 0)
    {
        QString trMesg;

        trMesg = QObject::tr("No Storage Group directories are defined.  You "
                             "must add at least one directory to the Default "
                             "Storage Group where new recordings will be "
                             "stored.");
        probs->append(trMesg + "\n");
        VERBOSE(VB_IMPORTANT, trMesg);
        return true;
    }

    query.prepare("SELECT groupname, dirname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query.exec() || !query.isActive() || query.size() < 1)
    {
        MythContext::DBError("checkStoragePaths", query);
        return false;
    }

    QDir checkDir("");
    while (query.next())
    {
        QString sgDir = query.value(0).toString();
        QStringList tokens = QStringList::split(",", query.value(1).toString());
        unsigned int curToken = 0;
        while (curToken < tokens.size())
        {
            checkDir.setPath(tokens[curToken]);
            if (checkPath(tokens[curToken], probs))
            {
                problemFound = true;
            }
            curToken++;
        }
    }

    return problemFound;
}


// I keep forgetting to change the preset (starting channel) when I add cards,
// so this checks that the assigned channel (which may be the default of 3)
// actually exists. This should save a few beginner Live TV problems

bool checkChannelPresets(QString *probs)
{
    bool problemFound = false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cardid, startchan, sourceid, inputname"
                  " FROM cardinput;");

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("checkChannelPresets", query);
        return false;
    }

    while (query.next())
    {
        int cardid    = query.value(0).toInt();
        QString startchan = query.value(1).toString();
        int sourceid  = query.value(2).toInt();

        if (query.value(1).toString() == "")    // Logic from tv_rec.cpp
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
            probs->append(QString(" %1) ").arg(query.value(3).toString()));
            probs->append(QObject::tr("is set to start on channel"));
            probs->append(QString(" %1, ").arg(startchan));
            probs->append(QObject::tr("which does not exist"));
            probs->append(".\n");
            problemFound = true;
        }
    }

    return problemFound;
}

/// Build up a string of common problems that the
/// user should correct in the MythTV-Setup program

bool CheckSetup(QString *problems)
{
    return checkStoragePaths(problems)
        || checkChannelPresets(problems);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
