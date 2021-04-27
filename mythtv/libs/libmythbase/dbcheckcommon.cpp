/*
 * Common functions used by all the *dbcheck.cpp files.
 */

#include "mythconfig.h"

#include <cstdio>
#include <iostream>

#include <QString>
#include <QSqlError>
#include "mythdb.h"
#include "mythdbcheck.h"
#include "mythlogging.h"



/** \fn UpdateDBVersionNumber(const QString&, const QString&, const QString&, QString&)
 *  \brief Updates the schema version stored in the database.
 *
 *   Updates "DBSchemaVer" property in the settings table.
 *  \param component Name of componenet being updated. Used for messages.
 *  \param versionkey The name of the key for the schema version.
 *  \param newnumber New schema version.
 *  \param dbver the database version at the end of the function is returned
 *               in this parameter, if things go well this will be 'newnumber'.
 */
bool UpdateDBVersionNumber(const QString &component, const QString &versionkey,
                           const QString &newnumber, QString &dbver)
{
    // delete old schema version
    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery =
        QString("DELETE FROM settings WHERE value='%1';").arg(versionkey);
    query.prepare(thequery);

    if (!query.exec())
    {
        QString msg =
            QString("DB Error (Deleting old %1 DB version number): \n"
                    "Query was: %2 \nError was: %3 \nnew version: %4")
            .arg(component,
                 thequery,
                 MythDB::DBErrorMessage(query.lastError()),
                 newnumber);
        LOG(VB_GENERAL, LOG_ERR, msg);
        return false;
    }

    // set new schema version
    thequery = QString("INSERT INTO settings (value, data, hostname) "
                       "VALUES ('%1', %2, NULL);").arg(versionkey, newnumber);
    query.prepare(thequery);

    if (!query.exec())
    {
        QString msg =
            QString("DB Error (Setting new %1 DB version number): \n"
                    "Query was: %2 \nError was: %3 \nnew version: %4")
            .arg(component,
                 thequery,
                 MythDB::DBErrorMessage(query.lastError()),
                 newnumber);
        LOG(VB_GENERAL, LOG_ERR, msg);
        return false;
    }

    dbver = newnumber;

    return true;
}

/** \fn performUpdateSeries(const QString &, DBUpdates)
 *  \brief Runs a number of SQL commands.
 *
 *  \param updates  array of SQL commands to issue, terminated by a NULL string.
 *  \return true on success, false on failure
 */
bool performUpdateSeries(const QString &component, const DBUpdates& updates)
{
    MSqlQuery query(MSqlQuery::InitCon());

    for (const auto & update : updates)
    {
        const char *thequery = update.c_str();
        if ((strlen(thequery) != 0U) && !query.exec(thequery))
        {
            QString msg =
                QString("DB Error (Performing %1 database upgrade): \n"
                        "Query was: %2 \nError was: %3")
                .arg(component,
                     thequery,
                     MythDB::DBErrorMessage(query.lastError()));
            LOG(VB_GENERAL, LOG_ERR, msg);
            return false;
        }
    }

    return true;
}

/** \fn performActualUpdate(const DBUpdates&, const char*, QString&)
 *  \brief Runs a number of SQL commands, and updates the schema version.
 *
 *  \param component  Name of componenet being updated. Used for messages.
 *  \param updates  array of SQL commands to issue, terminated by a NULL string.
 *  \param version  version we are updating db to.
 *  \param dbver    the database version at the end of the function is returned
 *                  in this parameter, if things go well this will be 'version'.
 *  \return true on success, false on failure
 */
bool performActualUpdate(
    const QString &component, const QString &versionkey,
    const DBUpdates &updates, const QString &version, QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    LOG(VB_GENERAL, LOG_CRIT, QString("Upgrading to %1 schema version %2")
        .arg(component, version));

    if (!performUpdateSeries(component, updates))
        return false;

    if (!UpdateDBVersionNumber(component, versionkey, version, dbver))
        return false;

    return true;
}
