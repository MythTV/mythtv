// Qt headers
#include <QFile>
#include <QDir>
#include <QObject>

// MythTV headers
#include "dbsettings.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythdbparams.h"

DatabaseSettings::DatabaseSettings(const QString &DBhostOverride)
    : GroupSetting()
{
    m_DBhostOverride = DBhostOverride;

    setLabel(DatabaseSettings::tr("Database Configuration"));

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
        setHelpText(
            DatabaseSettings::tr("All database settings take effect when "
                                 "you restart this program."));
    else
        setHelpText(
            DatabaseSettings::tr("MythTV could not connect to the database. "
                                 "Please verify your database settings "
                                 "below."));

    dbHostName = new TransTextEditSetting();
    dbHostName->setLabel(DatabaseSettings::tr("Hostname"));
    dbHostName->setHelpText(
        DatabaseSettings::tr("The host name or IP address of "
                             "the machine hosting the database. "
                             "This information is required."));
    addChild(dbHostName);

    dbHostPing = new TransMythUICheckBoxSetting();
    dbHostPing->setLabel(DatabaseSettings::tr("Ping test server?"));
    dbHostPing->setHelpText(
        DatabaseSettings::tr("Test basic host connectivity using "
                             "the ping command. Turn off if your "
                             "host or network don't support ping "
                             "(ICMP ECHO) packets"));
    addChild(dbHostPing);

    dbPort = new TransTextEditSetting();
    dbPort->setLabel(DatabaseSettings::tr("Port"));
    dbPort->setHelpText(
        DatabaseSettings::tr("The port number the database is running "
                             "on.  Leave blank if using the default "
                             "port (3306)."));
    addChild(dbPort);

    dbName = new TransTextEditSetting();
    dbName->setLabel(QCoreApplication::translate("(Common)",
                                                 "Database name"));
    dbName->setHelpText(
        DatabaseSettings::tr("The name of the database. "
                             "This information is required."));
    addChild(dbName);

    dbUserName = new TransTextEditSetting();
    dbUserName->setLabel(QCoreApplication::translate("(Common)", "User"));
    dbUserName->setHelpText(
        DatabaseSettings::tr("The user name to use while "
                             "connecting to the database. "
                             "This information is required."));
    addChild(dbUserName);

    dbPassword = new TransTextEditSetting();
    dbPassword->setLabel(DatabaseSettings::tr("Password"));
    dbPassword->setHelpText(
        DatabaseSettings::tr("The password to use while "
                             "connecting to the database. "
                             "This information is required."));
    addChild(dbPassword);

    localEnabled = new TransMythUICheckBoxSetting();
    localEnabled->setLabel(
        DatabaseSettings::tr("Use custom identifier for frontend "
                             "preferences"));
    localEnabled->setHelpText(
        DatabaseSettings::tr("If this frontend's host name "
                             "changes often, check this box "
                             "and provide a network-unique "
                             "name to identify it. "
                             "If unchecked, the frontend "
                             "machine's local host name will "
                             "be used to save preferences in "
                             "the database."));
    addChild(localEnabled);

    localHostName = new TransTextEditSetting();
    localHostName->setLabel(DatabaseSettings::tr("Custom identifier"));
    localHostName->setHelpText(
        DatabaseSettings::tr("An identifier to use while "
                             "saving the settings for this "
                             "frontend."));
    localEnabled->addTargetedChild("1", localHostName);

    wolEnabled = new TransMythUICheckBoxSetting();
    wolEnabled->setLabel(
        DatabaseSettings::tr("Enable database server wakeup"));
    wolEnabled->setHelpText(
        DatabaseSettings::tr("If enabled, the frontend will use "
                             "database wakeup parameters to "
                             "reconnect to the database server."));
    addChild(wolEnabled);

    wolReconnect = new TransMythUISpinBoxSetting(0, 60, 1, true);
    wolReconnect->setLabel(DatabaseSettings::tr("Reconnect time"));
    wolReconnect->setHelpText(
        DatabaseSettings::tr("The time in seconds to wait for "
                             "the server to wake up."));
    wolEnabled->addTargetedChild("1", wolReconnect);

    wolRetry = new TransMythUISpinBoxSetting(1, 10, 1, true);
    wolRetry->setLabel(DatabaseSettings::tr("Retry attempts"));
    wolRetry->setHelpText(
        DatabaseSettings::tr("The number of retries to wake the "
                             "server before the frontend gives "
                             "up."));
    wolEnabled->addTargetedChild("1", wolRetry);

    wolCommand = new TransTextEditSetting();
    wolCommand->setLabel(DatabaseSettings::tr("Wake command or MAC"));
    wolCommand->setHelpText(
        DatabaseSettings::tr("The command executed on this "
                             "frontend or server MAC to wake up the database "
                             "server (eg. sudo /etc/init.d/mysql "
                             "restart or 32:D2:86:00:17:A8)."));
    wolEnabled->addTargetedChild("1", wolCommand);
}

void DatabaseSettings::Load(void)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    if (params.dbHostName.isEmpty() ||
        params.dbUserName.isEmpty() ||
        params.dbPassword.isEmpty() ||
        params.dbName.isEmpty())
        setHelpText(getHelpText() + "\n" +
                    DatabaseSettings::tr("Required fields are"
                                         " marked with an asterisk (*)."));

    if (params.dbHostName.isEmpty())
    {
        dbHostName->setLabel("* " + dbHostName->getLabel());
        dbHostName->setValue(m_DBhostOverride);
    }
    else
        dbHostName->setValue(params.dbHostName);

    dbHostPing->setValue(params.dbHostPing);

    if (params.dbPort)
        dbPort->setValue(QString::number(params.dbPort));

    dbUserName->setValue(params.dbUserName);
    if (params.dbUserName.isEmpty())
        dbUserName->setLabel("* " + dbUserName->getLabel());
    dbPassword->setValue(params.dbPassword);
    if (params.dbPassword.isEmpty())
        dbPassword->setLabel("* " + dbPassword->getLabel());
    dbName->setValue(params.dbName);
    if (params.dbName.isEmpty())
        dbName->setLabel("* " + dbName->getLabel());

    localEnabled->setValue(params.localEnabled);
    localHostName->setValue(params.localHostName);

    wolEnabled->setValue(params.wolEnabled);
    wolReconnect->setValue(params.wolReconnect);
    wolRetry->setValue(params.wolRetry);
    wolCommand->setValue(params.wolCommand);
    //set all the children's m_haveChanged to false
    GroupSetting::Load();
}
void DatabaseSettings::Save(void)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    params.dbHostName = dbHostName->getValue();
    params.dbHostPing = dbHostPing->boolValue();
    params.dbPort = dbPort->getValue().toInt();
    params.dbUserName = dbUserName->getValue();
    params.dbPassword = dbPassword->getValue();
    params.dbName = dbName->getValue();
    params.dbType = "QMYSQL";

    params.localEnabled = localEnabled->boolValue();
    params.localHostName = localHostName->getValue();

    params.wolEnabled = wolEnabled->boolValue();
    params.wolReconnect = wolReconnect->intValue();
    params.wolRetry = wolRetry->intValue();
    params.wolCommand = wolCommand->getValue();

    gContext->SaveDatabaseParams(params);
    //set all the children's m_haveChanged to false
    GroupSetting::Save();
}

DatabaseSettings::~DatabaseSettings()
{
    emit isClosing();
}
