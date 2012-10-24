#include <QFile>
#include <QDir>

#include "dbsettings.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythdbparams.h"

class MythDbSettings1: public VerticalConfigurationGroup {
public:
    MythDbSettings1(const QString &DBhostOverride = QString::null);

    void Load(void);
    void Save(void);
    void Save(QString /*destination*/) { Save(); }

protected:
    TransLabelSetting    *info;
    TransLineEditSetting *dbHostName;
    TransCheckBoxSetting *dbHostPing;
    TransLineEditSetting *dbPort;
    TransLineEditSetting *dbName;
    TransLineEditSetting *dbUserName;
    TransLineEditSetting *dbPassword;
//    TransComboBoxSetting *dbType;

    QString              m_DBhostOverride;
}; 

class MythDbSettings2: public VerticalConfigurationGroup {
public:
    MythDbSettings2();

    void Load(void);
    void Save(void);
    void Save(QString /*destination*/) { Save(); }

protected:
    TransCheckBoxSetting *localEnabled;
    TransLineEditSetting *localHostName;
    TransCheckBoxSetting *wolEnabled;
    TransSpinBoxSetting  *wolReconnect;
    TransSpinBoxSetting  *wolRetry;
    TransLineEditSetting *wolCommand;
}; 



class LocalHostNameSettings : public TriggeredConfigurationGroup
{
  public:
    LocalHostNameSettings(Setting *checkbox, ConfigurationGroup *group) :
        TriggeredConfigurationGroup(false, false, false, false)
    {
        setLabel(QObject::tr("Use custom identifier for frontend preferences"));
        addChild(checkbox);
        setTrigger(checkbox);

        addTarget("1", group);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
};

class WOLsqlSettings : public TriggeredConfigurationGroup
{
  public:
    WOLsqlSettings(Setting *checkbox, ConfigurationGroup *group) :
        TriggeredConfigurationGroup(false, false, false, false)
    {
        setLabel(QObject::tr("Backend Server Wakeup settings"));

        addChild(checkbox);
        setTrigger(checkbox);

        addTarget("1", group);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
};

MythDbSettings1::MythDbSettings1(const QString &DbHostOverride) :
    VerticalConfigurationGroup(false, true, false, false)
{
    m_DBhostOverride = DbHostOverride;

    setLabel(QObject::tr("Database Configuration") + " 1/2");

    info = new TransLabelSetting();

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
        info->setValue(QObject::tr("All database settings take effect when "
                                   "you restart this program."));
    else
        info->setValue(QObject::tr("MythTV could not connect to the database. "
                                   "Please verify your database settings "
                                   "below."));
    addChild(info);

    VerticalConfigurationGroup* dbServer = new VerticalConfigurationGroup();
    dbServer->setLabel(QObject::tr("Database Server Settings"));
    dbHostName = new TransLineEditSetting(true);
    dbHostName->setLabel(QObject::tr("Hostname"));
    dbHostName->setHelpText(QObject::tr("The host name or IP address of "
                                        "the machine hosting the database. "
                                        "This information is required."));
    dbServer->addChild(dbHostName);

    HorizontalConfigurationGroup* g =
        new HorizontalConfigurationGroup(false, false);

    dbHostPing = new TransCheckBoxSetting();
    dbHostPing->setLabel(QObject::tr("Ping test server?"));
    dbHostPing->setHelpText(QObject::tr("Test basic host connectivity using "
                                        "the ping command. Turn off if your "
                                        "host or network don't support ping "
                                        "(ICMP ECHO) packets"));
    g->addChild(dbHostPing);

    // Some extra horizontal space:
    TransLabelSetting *l = new TransLabelSetting();
    l->setValue("                               ");
    g->addChild(l);

    dbServer->addChild(g);

    dbPort = new TransLineEditSetting(true);
    dbPort->setLabel(QObject::tr("Port"));
    dbPort->setHelpText(QObject::tr("The port number the database is running "
                                    "on.  Leave blank if using the default "
                                    "port (3306)."));
    g->addChild(dbPort);

    dbName = new TransLineEditSetting(true);
    dbName->setLabel(QObject::tr("Database name"));
    dbName->setHelpText(QObject::tr("The name of the database. "
                                    "This information is required."));
    dbServer->addChild(dbName);

    dbUserName = new TransLineEditSetting(true);
    dbUserName->setLabel(QObject::tr("User"));
    dbUserName->setHelpText(QObject::tr("The user name to use while "
                                        "connecting to the database. "
                                        "This information is required."));
    dbServer->addChild(dbUserName);

    dbPassword = new TransLineEditSetting(true);
    dbPassword->setLabel(QObject::tr("Password"));
    dbPassword->setHelpText(QObject::tr("The password to use while "
                                        "connecting to the database. "
                                        "This information is required."));
    dbServer->addChild(dbPassword);

//     dbType = new TransComboBoxSetting(false);
//     dbType->setLabel(QObject::tr("Database type"));
//     dbType->addSelection(QObject::tr("MySQL"), "QMYSQL");
//     dbType->setValue(0);
//     dbType->setHelpText(QObject::tr("The database implementation used "
//                                     "for your server."));
//     dbType->setEnabled(false);
    //dbServer->addChild(dbType);

    addChild(dbServer);

}

MythDbSettings2::MythDbSettings2(void) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Database Configuration") + " 2/2");

    localEnabled = new TransCheckBoxSetting();
    localEnabled->setLabel(QObject::tr("Use custom identifier for frontend "
                                       "preferences"));
    localEnabled->setHelpText(QObject::tr("If this frontend's host name "
                                          "changes often, check this box "
                                          "and provide a network-unique "
                                          "name to identify it. "
                                          "If unchecked, the frontend "
                                          "machine's local host name will "
                                          "be used to save preferences in "
                                          "the database."));

    localHostName = new TransLineEditSetting(true);
    localHostName->setLabel(QObject::tr("Custom identifier"));
    localHostName->setHelpText(QObject::tr("An identifier to use while "
                                           "saving the settings for this "
                                           "frontend."));

    VerticalConfigurationGroup *group1 =
        new VerticalConfigurationGroup(false);
    group1->addChild(localHostName);

    LocalHostNameSettings *sub3 =
        new LocalHostNameSettings(localEnabled, group1);
    addChild(sub3);

    wolEnabled = new TransCheckBoxSetting();
    wolEnabled->setLabel(QObject::tr("Enable database server wakeup"));
    wolEnabled->setHelpText(QObject::tr("If enabled, the frontend will use "
                                        "database wakeup parameters to "
                                        "reconnect to the database server."));

    wolReconnect = new TransSpinBoxSetting(0, 60, 1, true);
    wolReconnect->setLabel(QObject::tr("Reconnect time"));
    wolReconnect->setHelpText(QObject::tr("The time in seconds to wait for "
                                          "the server to wake up."));

    wolRetry = new TransSpinBoxSetting(1, 10, 1, true);
    wolRetry->setLabel(QObject::tr("Retry attempts"));
    wolRetry->setHelpText(QObject::tr("The number of retries to wake the "
                                      "server before the frontend gives "
                                      "up."));

    wolCommand = new TransLineEditSetting(true);
    wolCommand->setLabel(QObject::tr("Wake command"));
    wolCommand->setHelpText(QObject::tr("The command executed on this "
                                        "frontend to wake up the database "
                                        "server (eg. sudo /etc/init.d/mysql "
                                        "restart)."));

    HorizontalConfigurationGroup *group2 =
        new HorizontalConfigurationGroup(false, false);
    group2->addChild(wolReconnect);
    group2->addChild(wolRetry);

    VerticalConfigurationGroup *group3 =
        new VerticalConfigurationGroup(false);
    group3->addChild(group2);
    group3->addChild(wolCommand);

    WOLsqlSettings *sub4 =
        new WOLsqlSettings(wolEnabled, group3);
    addChild(sub4);
}

void MythDbSettings1::Load(void)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    if (params.dbHostName.isEmpty() ||
        params.dbUserName.isEmpty() ||
        params.dbPassword.isEmpty() ||
        params.dbName.isEmpty())
        info->setValue(info->getValue() + "\n" +
                       QObject::tr("Required fields are"
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

//     if (params.dbType == "QMYSQL")
//         dbType->setValue(0);
//     else if (params.dbType == "QPSQL7")
//         dbType->setValue(1);
}

void MythDbSettings2::Load(void)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    localEnabled->setValue(params.localEnabled);
    localHostName->setValue(params.localHostName);

    wolEnabled->setValue(params.wolEnabled);
    wolReconnect->setValue(params.wolReconnect);
    wolRetry->setValue(params.wolRetry);
    wolCommand->setValue(params.wolCommand);
}

void MythDbSettings1::Save(void)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    params.dbHostName    = dbHostName->getValue();
    params.dbHostPing    = dbHostPing->boolValue();
    params.dbPort        = dbPort->getValue().toInt();
    params.dbUserName    = dbUserName->getValue();
    params.dbPassword    = dbPassword->getValue();
    params.dbName        = dbName->getValue();
//    params.dbType        = dbType->getValue();
    params.dbType        = "QMYSQL";

    gContext->SaveDatabaseParams(params);
}

void MythDbSettings2::Save(void)
{
    DatabaseParams params = gContext->GetDatabaseParams();

    params.localEnabled  = localEnabled->boolValue();
    params.localHostName = localHostName->getValue();

    params.wolEnabled    = wolEnabled->boolValue();
    params.wolReconnect  = wolReconnect->intValue();
    params.wolRetry      = wolRetry->intValue();
    params.wolCommand    = wolCommand->getValue();

    gContext->SaveDatabaseParams(params);
}

DatabaseSettings::DatabaseSettings(const QString &DBhostOverride)
{
    addChild(new MythDbSettings1(DBhostOverride));
    addChild(new MythDbSettings2());
}

void DatabaseSettings::addDatabaseSettings(ConfigurationWizard *wizard)
{
    wizard->addChild(new MythDbSettings1());
    wizard->addChild(new MythDbSettings2());
}
