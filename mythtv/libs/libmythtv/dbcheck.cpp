#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythcontext.h"

const QString currentDatabaseVersion = "1004";

void UpdateDBVersionNumber(const QString &newnumber)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    db_conn->exec("DELETE FROM settings WHERE value='DBSchemaVer';");
    db_conn->exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('DBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

void InitializeDatabase(void)
{
    cerr << "Not written yet.\n";
    // set up all of the database tables.
}

void UpgradeTVDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("DBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        InitializeDatabase();        
        return;
    }


    // insert older upgrades here, later


    if (dbver == "1003")
    {
        VERBOSE(VB_ALL, "Upgrading to schema ver 1004");
        UpdateDBVersionNumber("1004");
        dbver = "1004";    
    }
}

