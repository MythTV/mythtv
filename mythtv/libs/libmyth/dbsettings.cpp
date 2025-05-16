#include "dbsettings.h"

// Qt headers
#include <QFile>
#include <QDir>
#include <QObject>

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdbparams.h"

DatabaseSettings::DatabaseSettings(QString DBhostOverride) :
    m_dbHostOverride(std::move(DBhostOverride))
{
    setLabel(DatabaseSettings::tr("Database Configuration"));

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        setHelpText(
            DatabaseSettings::tr("All database settings take effect when "
                                 "you restart this program."));
    }
    else
    {
        setHelpText(
            DatabaseSettings::tr("MythTV could not connect to the database. "
                                 "Please verify your database settings "
                                 "below."));
    }

    m_dbHostName = new TransTextEditSetting();
    m_dbHostName->setLabel(DatabaseSettings::tr("Hostname"));
    m_dbHostName->setHelpText(
        DatabaseSettings::tr("The host name or IP address of "
                             "the machine hosting the database. "
                             "This information is required."));
    addChild(m_dbHostName);

    // Ping database host is no longer used

    m_dbPort = new TransTextEditSetting();
    m_dbPort->setLabel(DatabaseSettings::tr("Port"));
    m_dbPort->setHelpText(
        DatabaseSettings::tr("The port number the database is running "
                             "on.  Leave blank if using the default "
                             "port (3306)."));
    addChild(m_dbPort);

    m_dbName = new TransTextEditSetting();
    m_dbName->setLabel(QCoreApplication::translate("(Common)",
                                                 "Database name"));
    m_dbName->setHelpText(
        DatabaseSettings::tr("The name of the database. "
                             "This information is required."));
    addChild(m_dbName);

    m_dbUserName = new TransTextEditSetting();
    m_dbUserName->setLabel(QCoreApplication::translate("(Common)", "User"));
    m_dbUserName->setHelpText(
        DatabaseSettings::tr("The user name to use while "
                             "connecting to the database. "
                             "This information is required."));
    addChild(m_dbUserName);

    m_dbPassword = new TransTextEditSetting();
    m_dbPassword->setLabel(DatabaseSettings::tr("Password"));
    m_dbPassword->setHelpText(
        DatabaseSettings::tr("The password to use while "
                             "connecting to the database. "
                             "This information is required."));
    addChild(m_dbPassword);

    m_localEnabled = new TransMythUICheckBoxSetting();
    m_localEnabled->setLabel(
        DatabaseSettings::tr("Use custom identifier for frontend "
                             "preferences"));
    m_localEnabled->setHelpText(
        DatabaseSettings::tr("If this frontend's host name "
                             "changes often, check this box "
                             "and provide a network-unique "
                             "name to identify it. "
                             "If unchecked, the frontend "
                             "machine's local host name will "
                             "be used to save preferences in "
                             "the database."));
    addChild(m_localEnabled);

    m_localHostName = new TransTextEditSetting();
    m_localHostName->setLabel(DatabaseSettings::tr("Custom identifier"));
    m_localHostName->setHelpText(
        DatabaseSettings::tr("An identifier to use while "
                             "saving the settings for this "
                             "frontend."));
    m_localEnabled->addTargetedChild("1", m_localHostName);

    m_wolEnabled = new TransMythUICheckBoxSetting();
    m_wolEnabled->setLabel(
        DatabaseSettings::tr("Enable database server wakeup"));
    m_wolEnabled->setHelpText(
        DatabaseSettings::tr("If enabled, the frontend will use "
                             "database wakeup parameters to "
                             "reconnect to the database server."));
    addChild(m_wolEnabled);

    m_wolReconnect = new TransMythUISpinBoxSetting(0, 60, 1, 1);
    m_wolReconnect->setLabel(DatabaseSettings::tr("Reconnect time"));
    m_wolReconnect->setHelpText(
        DatabaseSettings::tr("The time in seconds to wait for "
                             "the server to wake up."));
    m_wolEnabled->addTargetedChild("1", m_wolReconnect);

    m_wolRetry = new TransMythUISpinBoxSetting(1, 10, 1, 1);
    m_wolRetry->setLabel(DatabaseSettings::tr("Retry attempts"));
    m_wolRetry->setHelpText(
        DatabaseSettings::tr("The number of retries to wake the "
                             "server before the frontend gives "
                             "up."));
    m_wolEnabled->addTargetedChild("1", m_wolRetry);

    m_wolCommand = new TransTextEditSetting();
    m_wolCommand->setLabel(DatabaseSettings::tr("Wake command or MAC"));
    m_wolCommand->setHelpText(
        DatabaseSettings::tr("The command executed on this "
                             "frontend or server MAC to wake up the database "
                             "server (eg. sudo /etc/init.d/mysql "
                             "restart or 32:D2:86:00:17:A8)."));
    m_wolEnabled->addTargetedChild("1", m_wolCommand);
}

void DatabaseSettings::Load(void)
{
    DatabaseParams params = GetMythDB()->GetDatabaseParams();

    if (params.m_dbHostName.isEmpty() ||
        params.m_dbUserName.isEmpty() ||
        params.m_dbPassword.isEmpty() ||
        params.m_dbName.isEmpty())
    {
        setHelpText(getHelpText() + "\n" +
                    DatabaseSettings::tr("Required fields are"
                                         " marked with an asterisk (*)."));
    }

    if (params.m_dbHostName.isEmpty())
    {
        m_dbHostName->setLabel("* " + m_dbHostName->getLabel());
        m_dbHostName->setValue(m_dbHostOverride);
    }
    else
    {
        m_dbHostName->setValue(params.m_dbHostName);
    }

    if (params.m_dbPort)
        m_dbPort->setValue(QString::number(params.m_dbPort));

    m_dbUserName->setValue(params.m_dbUserName);
    if (params.m_dbUserName.isEmpty())
        m_dbUserName->setLabel("* " + m_dbUserName->getLabel());
    m_dbPassword->setValue(params.m_dbPassword);
    if (params.m_dbPassword.isEmpty())
        m_dbPassword->setLabel("* " + m_dbPassword->getLabel());
    m_dbName->setValue(params.m_dbName);
    if (params.m_dbName.isEmpty())
        m_dbName->setLabel("* " + m_dbName->getLabel());

    m_localEnabled->setValue(params.m_localEnabled);
    m_localHostName->setValue(params.m_localHostName);

    m_wolEnabled->setValue(params.m_wolEnabled);
    m_wolReconnect->setValue(params.m_wolReconnect.count());
    m_wolRetry->setValue(params.m_wolRetry);
    m_wolCommand->setValue(params.m_wolCommand);
    //set all the children's m_haveChanged to false
    GroupSetting::Load();
}
void DatabaseSettings::Save(void)
{
    DatabaseParams params = GetMythDB()->GetDatabaseParams();

    params.m_dbHostName = m_dbHostName->getValue();
    // Ping database host is no longer used
    params.m_dbHostPing = false;
    params.m_dbPort = m_dbPort->getValue().toInt();
    params.m_dbUserName = m_dbUserName->getValue();
    params.m_dbPassword = m_dbPassword->getValue();
    params.m_dbName = m_dbName->getValue();
    params.m_dbType = "QMYSQL";

    params.m_localEnabled = m_localEnabled->boolValue();
    params.m_localHostName = m_localHostName->getValue();

    params.m_wolEnabled = m_wolEnabled->boolValue();
    params.m_wolReconnect = std::chrono::seconds(m_wolReconnect->intValue());
    params.m_wolRetry = m_wolRetry->intValue();
    params.m_wolCommand = m_wolCommand->getValue();

    GetMythDB()->SaveDatabaseParams(params, false);
    //set all the children's m_haveChanged to false
    GroupSetting::Save();
}

DatabaseSettings::~DatabaseSettings()
{
    emit isClosing();
}
