#ifndef PROFILEGROUP_H
#define PROFILEGROUP_H

#include "libmyth/settings.h"
#include "libmyth/mythwidgets.h"

class ProfileGroup;

// A parameter associated with the profile itself
class ProfileGroupParam: public SimpleDBStorage {
protected:
    ProfileGroupParam(const ProfileGroup& parentProfile, QString name):
        SimpleDBStorage("profilegroups", name),
        parent(parentProfile) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);
    const ProfileGroup& parent;
};

class ProfileGroup: public ConfigurationWizard {
protected:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("profilegroups", "id") {
            setVisible(false);
        };

        // Should never be called because this setting is not visible
        virtual QWidget* configWidget(ConfigurationGroup *cg,
                                      QWidget* parent = NULL,
                                      const char* widgetName = NULL) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };

    class Is_default: virtual public IntegerSetting, public ProfileGroupParam {
    public:
        Is_default(const ProfileGroup& parent):
            ProfileGroupParam(parent, "is_default") {
            setVisible(false);
        };

        // Should never be called because this setting is not visible
        virtual QWidget* configWidget(ConfigurationGroup *cg,
                                      QWidget* parent = NULL,
                                      const char* widgetName = NULL) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };

    class Name: public LineEditSetting, public ProfileGroupParam {
    public:
        Name(const ProfileGroup& parent): ProfileGroupParam(parent, "name") {
            setLabel(QObject::tr("Profile Group Name"));
        };
    };

    class HostName: public ComboBoxSetting, public ProfileGroupParam {
    public:
        HostName(const ProfileGroup& parent):
            ProfileGroupParam(parent, "hostname") {
            setLabel(QObject::tr("Hostname"));
        };
        void fillSelections(QSqlDatabase *db);
    };

    class CardInfo: public ComboBoxSetting, public ProfileGroupParam {
    public:
        CardInfo(const ProfileGroup& parent):
            ProfileGroupParam(parent, "cardtype") {
            setLabel(QObject::tr("Card-Type"));
        };
    };

public:
    ProfileGroup(QSqlDatabase* _db);

    virtual void loadByID(int id);

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);
    static void getHostNames(QSqlDatabase* db, QStringList* hostnames);
    int getProfileNum(void) const {
        return id->intValue();
    };

    int isDefault(void) const {
        return is_default->intValue();
    };

    QString getName(void) const { return name->getValue(); };
    static QString getName(QSqlDatabase* db, int group);
    void setName(QString newName) { name->setValue(newName); };
    bool allowedGroupName(void);

private:
    ID* id;
    Name* name;
    HostName* host;
    Is_default* is_default;
    QSqlDatabase* db;
};

class ProfileGroupEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    ProfileGroupEditor(QSqlDatabase* _db):
        db(_db) {};

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected slots:
    void open(int id);
    void callDelete(void);

protected:
    QSqlDatabase* db;
    MythDialog* dialog;
    bool redraw;
};

#endif

