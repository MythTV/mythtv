// Qt
#include <QSqlError>

// myth
#include <mythcontext.h>
#include <mythdb.h>

// mythbrowser
#include "browserdbutil.h"
#include "bookmarkmanager.h"

const QString currentDatabaseVersion = "1002";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gCoreContext->SaveSettingOnHost("BrowserDBSchemaVer", newnumber, NULL))
    {
        VERBOSE(VB_IMPORTANT,
                QString("DB Error (Setting new DB version number): %1\n")
                .arg(newnumber));

        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, "Upgrading to MythBrowser schema version " + version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        if (!query.exec(thequery))
        {
            QString msg =
                QString("DB Error (Performing database upgrade): \n"
                        "Query was: %1 \nError was: %2 \nnew version: %3")
                .arg(thequery)
                .arg(MythDB::DBErrorMessage(query.lastError()))
                .arg(version);
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }

        counter++;
        thequery = updates[counter];
    }

    if (!UpdateDBVersionNumber(version))
        return false;

    dbver = version;
    return true;
}

bool UpgradeBrowserDatabaseSchema(void)
{
    QString dbver = gCoreContext->GetSetting("BrowserDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythBrowser initial database information.");

        const QString updates[] =
        {
            "DROP TABLE IF EXISTS websites;",
            "CREATE TABLE websites ("
            "id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY, "
            "category VARCHAR(100) NOT NULL, "
            "name VARCHAR(100) NOT NULL, "
            "url VARCHAR(255) NOT NULL);",
            ""
        };
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000") 
    { 
        const QString updates[] =
        {
            "UPDATE settings SET data = 'Internal' WHERE data LIKE '%mythbrowser' AND value = 'WebBrowserCommand';", 
            "" 
        };
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    if (dbver == "1001")
    {
        const QString updates[] =
        {
            "DELETE FROM keybindings "
            " WHERE action = 'DELETETAB' AND context = 'Browser';",
            ""
        };
        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }

    return true;
}

bool FindInDB(const QString &category, const QString& name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM websites "
                  "WHERE category = :CATEGORY AND name = :NAME ;");
    query.bindValue(":CATEGORY", category);
    query.bindValue(":NAME", name);
    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: find in db", query);
        return false;
    }

    return (query.size() > 0);
}

bool InsertInDB(Bookmark* site)
{
    if (!site)
        return false;

    return InsertInDB(site->category, site->name, site->url);
}

bool InsertInDB(const QString &category,
                const QString &name, const QString &url)
{
    if (category.isEmpty() || name.isEmpty() || url.isEmpty())
        return false;

    if (FindInDB(category, name))
        return false;

    QString _url = url.trimmed();
    if (!_url.startsWith("http://") && !_url.startsWith("https://") &&
            !_url.startsWith("file:/"))
        _url.prepend("http://");

    _url.replace("&amp;","&");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO websites (category, name, url) "
                  "VALUES(:CATEGORY, :NAME, :URL);");
    query.bindValue(":CATEGORY", category);
    query.bindValue(":NAME", name);
    query.bindValue(":URL", _url);
    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool RemoveFromDB(Bookmark *site)
{
    if (!site)
        return false;

    return RemoveFromDB(site->category, site->name);
}

bool RemoveFromDB(const QString &category, const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM websites "
                  "WHERE category = :CATEGORY AND name = :NAME;");
    query.bindValue(":CATEGORY", category);
    query.bindValue(":NAME", name);
    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

int GetCategoryList(QStringList &list)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT category FROM websites "
                  "ORDER BY category;");

    if (!query.exec())
    {
        MythDB::DBError("mythbrowser: get category list", query);
        return false;
    }
    else
    {
        while (query.next())
        {
            list << query.value(0).toString();
        }
    }

    return list.size();
}

int GetSiteList(QList<Bookmark*>  &siteList)
{
    while (!siteList.isEmpty())
        delete siteList.takeFirst();

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.exec("SELECT category, name, url FROM websites "
               "ORDER BY category, name"))
    {
        VERBOSE(VB_IMPORTANT, "BookmarkManager: Error in loading from DB");
    }
    else
    {
        while (query.next())
        {
            Bookmark *site = new Bookmark();
            site->category = query.value(0).toString();
            site->name = query.value(1).toString();
            site->url = query.value(2).toString();
            site->selected = false;
            siteList.append(site);
        }
    }

    return siteList.size();
}
