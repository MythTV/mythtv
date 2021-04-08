// checksetup.cpp
//
// Some functions to do simple sanity checks on the MythTV setup.
// CheckSetup() is currently meant for the mythtv-setup program,
// but the other functions could probably be called from anywhere.
// They all return true if any problems are found, and add to a
// caller-supplied QString a message describing the problem.

#include <QDir>

#include "mythdb.h"
#include "mythcorecontext.h"
#include "mythdate.h"
#include "checksetup.h"

/// Check that a directory path exists and is writable

static bool checkPath(QString path, QStringList &probs)
{
    QDir dir(path);
    if (!dir.exists())
    {
        probs.push_back(QObject::tr("Path \"%1\" doesn't exist.").arg(path));
        return true;
    }

    QFile test(path.append("/.test"));
    if (test.open(QIODevice::WriteOnly))
        test.remove();
    else
    {
        probs.push_back(QObject::tr("Unable to create file \"%1\" - directory "
                        "is not writable?").arg(path));
        return true;
    }

    return false;
}

/// Do the Storage Group filesystem paths exist? Are they writable?
/// Is the Live TV filesystem large enough?

bool checkStoragePaths(QStringList &probs)
{
    bool problemFound = false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT count(groupname) FROM storagegroup;");
    if (!query.exec() || !query.next())
    {
        MythDB::DBError("checkStoragePaths", query);
        return false;
    }

    if (query.value(0).toInt() == 0)
    {
        QString trMesg =
                QObject::tr("No Storage Group directories are defined.  You "
                            "must add at least one directory to the Default "
                            "Storage Group where new recordings will be "
                            "stored.");
        probs.push_back(trMesg);
        LOG(VB_GENERAL, LOG_ERR, trMesg);
        return true;
    }

    query.prepare("SELECT groupname, dirname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkStoragePaths", query);
        return false;
    }
    if (query.size() < 1)
    {
        if (gCoreContext->IsMasterHost())
        {
            // Master backend must have a defined Default SG
            QString trMesg =
                    QObject::tr("No Storage Group directories are defined.  "
                                "You must add at least one directory to the "
                                "Default Storage Group where new recordings "
                                "will be stored.");
            probs.push_back(trMesg);
            LOG(VB_GENERAL, LOG_ERR, trMesg);
            return true;
        }
        return false;
    }

    QDir checkDir("");
    QString dirname;
    while (query.next())
    {
        /* The storagegroup.dirname column uses utf8_bin collation, so Qt
         * uses QString::fromAscii() for toString(). Explicitly convert the
         * value using QString::fromUtf8() to prevent corruption. */
        dirname = QString::fromUtf8(query.value(1)
                                    .toByteArray().constData());
        QStringList tokens = dirname.split(",");
        int curToken = 0;
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

bool checkImageStoragePaths(QStringList &probs)
{
    bool problemFound = false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT groupname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkImageStoragePaths", query);
        return false;
    }
    if (query.size() < 1)
    {
        return false;
    }

    QStringList groups;
    while (query.next())
    {
        groups += query.value(0).toString();
    }

    if (groups.contains("Videos"))
    {
        if (groups.contains("Fanart") &&
                groups.contains("Coverart") &&
                groups.contains("Screenshots") &&
                groups.contains("Banners"))
            problemFound = false;
        else
        {
            QString trMesg =
                QObject::tr("You have a Video Storage "
                            "Group, but have not set up "
                            "all Image Groups.  If you continue, "
                            "video image downloads will be saved in "
                            "your Videos Storage Group.  Do you want "
                            "to store them in their own groups?");
            probs.push_back(trMesg);
            LOG(VB_GENERAL, LOG_ERR, trMesg);
            problemFound = true;
        }
    }            

    return problemFound;
}

// I keep forgetting to change the preset (starting channel) when I add cards,
// so this checks that the assigned channel (which may be the default of 3)
// actually exists. This should save a few beginner Live TV problems

bool checkChannelPresets(QStringList &probs)
{
    bool problemFound = false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cardid, startchan, sourceid, inputname, parentid"
                  " FROM capturecard;");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkChannelPresets", query);
        return false;
    }

    while (query.next())
    {
        int cardid    = query.value(0).toInt();
        QString startchan = query.value(1).toString();
        int sourceid  = query.value(2).toInt();
        int parentid = query.value(4).toInt();

        // Warnings only for real devices
        if (parentid != 0)
            continue;

        if (0 == sourceid)
        {
            probs.push_back(QObject::tr("Card %1 (type %2) is not connected "
                            "to a video source.")
                    .arg(cardid).arg(query.value(3).toString()));
            problemFound = true;
            continue;
        }

        if (query.value(1).toString().isEmpty())    // Logic from tv_rec.cpp
            startchan = "3";

        MSqlQuery channelExists(MSqlQuery::InitCon());
        QString   channelQuery;
        channelQuery = QString("SELECT chanid FROM channel"
                               " WHERE deleted IS NULL AND "
                               "       channum = '%1' AND "
                               "       sourceid = %2;")
                              .arg(startchan).arg(sourceid);
        channelExists.prepare(channelQuery);

        if (!channelExists.exec() || !channelExists.isActive())
        {
            MythDB::DBError("checkChannelPresets", channelExists);
            return problemFound;
        }

        if (channelExists.size() == 0)
        {
            probs.push_back(QObject::tr("Card %1 (type %2) is set to start on "
                            "channel %3, which does not exist.")
                    .arg(cardid).arg(query.value(3).toString(), startchan));
            problemFound = true;
        }
    }

    return problemFound;
}

// Check that the display names for all parent inputs are set and that
// the last two characters are unique.

bool checkInputDisplayNames(QStringList &probs)
{
    bool problemFound = false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT count(*) total, "
                  "       count(distinct right(if(displayname<>'',"
                  "                            displayname, NULL), 2)) uniq "
                  "FROM capturecard "
                  "WHERE parentid = 0");

    if (!query.exec() || !query.next())
    {
        MythDB::DBError("checkInputDisplayNames", query);
        return false;
    }

    int total = query.value(0).toInt();
    int uniq = query.value(1).toInt();
    if (uniq != total)
    {
        probs.push_back(QObject::tr(
                            "The display names for one or more inputs are not "
                            "sufficiently unique.  They must be set and the "
                            "last two characters must be unique because some "
                            "themes use them to identify the input."));
        problemFound = true;
    }

    return problemFound;
}

/// Build up a string of common problems that the
/// user should correct in the MythTV-Setup program

bool CheckSetup(QStringList &problems)
{
    return checkStoragePaths(problems)
        || checkChannelPresets(problems)
        || checkInputDisplayNames(problems)
        || checkImageStoragePaths(problems);
}

bool needsMFDBReminder()
{
    bool needsReminder = false;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT sourceid "
                  "FROM videosource "
                  "WHERE xmltvgrabber LIKE 'tv_grab_%';");
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("needsMFDBReminder", query);
    }
    else if (query.size() >= 1)
    {
        needsReminder = true;
    }

    return needsReminder;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
