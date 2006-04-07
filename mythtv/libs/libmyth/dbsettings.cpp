#include "mythcontext.h"
#include "mythdbcon.h"
#include "dbsettings.h"
#include <qfile.h>
#include <qdir.h>

class TransientSetting: public TransientStorage, virtual public Configurable {
public:
    TransientSetting() { };
};

class TransientSpinBox: public SpinBoxSetting, public TransientSetting {
  public:
    TransientSpinBox(int min, int max, int step, 
                   bool allow_single_step = false) :
        SpinBoxSetting(min, max, step, allow_single_step) { };
};

class TransientCheckBox: public CheckBoxSetting, public TransientSetting {
  public:
    TransientCheckBox() { };
};

class TransientLineEdit: public LineEditSetting, public TransientSetting {
  public:
    TransientLineEdit(bool rw = true) :
        LineEditSetting(rw) { };
};

class TransientComboBox: public ComboBoxSetting, public TransientSetting {
  public:
    TransientComboBox(bool rw = true) :
        ComboBoxSetting(rw) { };
};

class TransientLabel: public LabelSetting, public TransientSetting {
  public:
    TransientLabel() { };
};

class MythDbSettings1: public VerticalConfigurationGroup {
public:
    MythDbSettings1();
    
    void load();
    void save();
    
protected:
    TransientLabel    *info;
    TransientLineEdit *dbHostName;
    TransientLineEdit *dbName;
    TransientLineEdit *dbUserName;
    TransientLineEdit *dbPassword;
    TransientComboBox *dbType;
}; 

class MythDbSettings2: public VerticalConfigurationGroup {
public:
    MythDbSettings2();
    
    void load();
    void save();
    
protected:
    TransientCheckBox *localEnabled;
    TransientLineEdit *localHostName;
    TransientCheckBox *wolEnabled;
    TransientSpinBox  *wolReconnect;
    TransientSpinBox  *wolRetry;
    TransientLineEdit *wolCommand;
}; 



class LocalHostNameSettings: public VerticalConfigurationGroup,
                             public TriggeredConfigurationGroup {
public:
    LocalHostNameSettings(Setting *checkbox, ConfigurationGroup *group):
        ConfigurationGroup(false, true, false, false),
        VerticalConfigurationGroup(false, true, false, false)
    {
        setLabel(QObject::tr("Use custom identifier for frontend preferences"));
        setUseLabel(false);
        setUseFrame(false);
        addChild(checkbox);
        setTrigger(checkbox);

        addTarget("1", group);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
};

class WOLsqlSettings: public VerticalConfigurationGroup,
                      public TriggeredConfigurationGroup {
public:
    WOLsqlSettings(Setting *checkbox, ConfigurationGroup *group):
        ConfigurationGroup(false, true, false, false),
        VerticalConfigurationGroup(false, true, false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Wake-On-LAN settings"));
        setUseLabel(false);
        setUseFrame(false);
        addChild(checkbox);
        setTrigger(checkbox);
        
        addTarget("1", group);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
};

MythDbSettings1::MythDbSettings1() :
    ConfigurationGroup(false, true, false, false),
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Database Configuration") + " 1/2");
    setUseLabel(false);
    
    info = new TransientLabel();

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
        info->setValue(QObject::tr("All database settings take effect when "
                                   "you restart this program."));
    else
        info->setValue(QObject::tr("Myth could not connect to the database. "
                                   "Please verify your database settings "
                                   "below."));
    addChild(info);
    
    dbHostName = new TransientLineEdit(true);
    dbHostName->setLabel(QObject::tr("Host name"));
    dbHostName->setHelpText(QObject::tr("The host name or IP address of "
                                        "the machine hosting the database. "
                                        "This information is required."));
    addChild(dbHostName);
    
    dbName = new TransientLineEdit(true);
    dbName->setLabel(QObject::tr("Database"));
    dbName->setHelpText(QObject::tr("The name of the database. "
                                    "This information is required."));
    addChild(dbName);

    dbUserName = new TransientLineEdit(true);
    dbUserName->setLabel(QObject::tr("User"));
    dbUserName->setHelpText(QObject::tr("The user name to use while "
                                        "connecting to the database. "
                                        "This information is required."));
    addChild(dbUserName);
    
    dbPassword = new TransientLineEdit(true);
    dbPassword->setLabel(QObject::tr("Password"));
    dbPassword->setHelpText(QObject::tr("The password to use while "
                                        "connecting to the database. "
                                        "This information is required."));
    addChild(dbPassword);
    
    dbType = new TransientComboBox(false);
    dbType->setLabel(QObject::tr("Database type"));
    dbType->addSelection(QObject::tr("MySQL"), "QMYSQL3");
    //dbType->addSelection(QObject::tr("PostgreSQL"), "QPSQL7");
    dbType->setValue(0);
    dbType->setHelpText(QObject::tr("The database implementation used "
                                    "for your server."));
    addChild(dbType);
}

MythDbSettings2::MythDbSettings2(void) :
    ConfigurationGroup(false, true, false, false),
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Database Configuration") + " 2/2");
    setUseLabel(false);
    
    localEnabled = new TransientCheckBox();
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
    
    localHostName = new TransientLineEdit(true);
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
    
    wolEnabled = new TransientCheckBox();
    wolEnabled->setLabel(QObject::tr("Use Wake-On-LAN to wake database"));
    wolEnabled->setHelpText(QObject::tr("If checked, the frontend will use "
                                        "Wake-On-LAN parameters to "
                                        "reconnect to the database server."));
    
    wolReconnect = new TransientSpinBox(0, 60, 1, true);
    wolReconnect->setLabel(QObject::tr("Reconnect time"));
    wolReconnect->setHelpText(QObject::tr("The time in seconds to wait for "
                                          "the server to wake up."));
    
    wolRetry = new TransientSpinBox(1, 10, 1, true);
    wolRetry->setLabel(QObject::tr("Retry attempts"));
    wolRetry->setHelpText(QObject::tr("The number of retries to wake the "
                                      "server before the frontend gives "
                                      "up."));
    
    wolCommand = new TransientLineEdit(true);
    wolCommand->setLabel(QObject::tr("Wake command"));
    wolCommand->setHelpText(QObject::tr("The command executed on this "
                                        "frontend to wake up the database "
                                        "server."));
    
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

void MythDbSettings1::load()
{
    DatabaseParams params = gContext->GetDatabaseParams();
    
    if (params.dbHostName.isEmpty() ||
        params.dbUserName.isEmpty() ||
        params.dbPassword.isEmpty() ||
        params.dbName.isEmpty())
        info->setValue(info->getValue() + "\nRequired fields are marked "
                                          "with an asterisk (*).");
    
    dbHostName->setValue(params.dbHostName);
    if (params.dbHostName.isEmpty())
        dbHostName->setLabel("* " + dbHostName->getLabel());
    dbUserName->setValue(params.dbUserName);
    if (params.dbUserName.isEmpty())
        dbUserName->setLabel("* " + dbUserName->getLabel());
    dbPassword->setValue(params.dbPassword);
    if (params.dbPassword.isEmpty())
        dbPassword->setLabel("* " + dbPassword->getLabel());
    dbName->setValue(params.dbName);
    if (params.dbName.isEmpty())
        dbName->setLabel("* " + dbName->getLabel());
        
    if (params.dbType == "QMYSQL3")
        dbType->setValue(0);
    else if (params.dbType == "QPSQL7")
        dbType->setValue(1);
}

void MythDbSettings2::load()
{
    DatabaseParams params = gContext->GetDatabaseParams();
    
    localEnabled->setValue(params.localEnabled);
    localHostName->setValue(params.localHostName);
    
    wolEnabled->setValue(params.wolEnabled);
    wolReconnect->setValue(params.wolReconnect);
    wolRetry->setValue(params.wolRetry);
    wolCommand->setValue(params.wolCommand);
}

void MythDbSettings1::save()
{
    DatabaseParams params = gContext->GetDatabaseParams();
    
    params.dbHostName    = dbHostName->getValue();
    params.dbUserName    = dbUserName->getValue();
    params.dbPassword    = dbPassword->getValue();
    params.dbName        = dbName->getValue();
    params.dbType        = dbType->getValue();
        
    gContext->SaveDatabaseParams(params);
}

void MythDbSettings2::save()
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

DatabaseSettings::DatabaseSettings()
{
    addChild(new MythDbSettings1());
    addChild(new MythDbSettings2());
}

void DatabaseSettings::addDatabaseSettings(ConfigurationWizard *wizard)
{
    wizard->addChild(new MythDbSettings1());
    wizard->addChild(new MythDbSettings2());
}
