#ifndef PROFILEGROUP_H
#define PROFILEGROUP_H

#include "libmyth/settings.h"
#include "libmyth/mythwidgets.h"

class ProfileGroup;

// A parameter associated with the profile itself
class ProfileGroupStorage : public SimpleDBStorage
{
  protected:
    ProfileGroupStorage(Setting            *_setting,
                        const ProfileGroup &_parentProfile,
                        QString             _name) :
        SimpleDBStorage(_setting, "profilegroups", _name),
        parent(_parentProfile)
    {
        _setting->setName(_name);
    }

    virtual QString setClause(MSqlBindings& bindings);
    virtual QString whereClause(MSqlBindings& bindings);
    const ProfileGroup& parent;
};

class ProfileGroup : public ConfigurationWizard
{
    friend class ProfileGroupEditor;
  protected:
    class ID : public AutoIncrementDBSetting
    {
      public:
        ID() : AutoIncrementDBSetting("profilegroups", "id")
        {
            setVisible(false);
        }
    };

    class Is_default : public IntegerSetting, public ProfileGroupStorage
    {
      public:
        Is_default(const ProfileGroup &parent) :
            IntegerSetting(this),
            ProfileGroupStorage(this, parent, "is_default")
        {
            setVisible(false);
        }
    };

    class Name : public LineEditSetting, public ProfileGroupStorage
    {
      public:
        Name(const ProfileGroup &parent) :
            LineEditSetting(this),
            ProfileGroupStorage(this, parent, "name")
        {
            setLabel(QObject::tr("Profile Group Name"));
        }
    };

    class HostName : public ComboBoxSetting, public ProfileGroupStorage
    {
      public:
        HostName(const ProfileGroup &parent) :
            ComboBoxSetting(this),
            ProfileGroupStorage(this, parent, "hostname")
        {
            setLabel(QObject::tr("Hostname"));
        }
        void fillSelections();
    };

    class CardInfo : public ComboBoxSetting, public ProfileGroupStorage
    {
      public:
        CardInfo(const ProfileGroup &parent) :
            ComboBoxSetting(this),
            ProfileGroupStorage(this, parent, "cardtype")
        {
            setLabel(QObject::tr("Card-Type"));
        }
    };

public:
    ProfileGroup();

    virtual void loadByID(int id);

    static void fillSelections(SelectSetting* setting);
    static void getHostNames(QStringList* hostnames);
    int getProfileNum(void) const {
        return id->intValue();
    };

    int isDefault(void) const {
        return is_default->intValue();
    };

    QString getName(void) const { return name->getValue(); };
    static QString getName(int group);
    void setName(QString newName) { name->setValue(newName); };
    bool allowedGroupName(void);

private:
    ID* id;
    Name* name;
    HostName* host;
    Is_default* is_default;
};

class MPUBLIC ProfileGroupEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT
  public:
    ProfileGroupEditor() :
        listbox(new ListBoxSetting(this)), dialog(NULL), redraw(true)
        { addChild(listbox); }

    virtual int exec();
    virtual void load();
    virtual void save() {};

  protected slots:
    void open(int id);
    void callDelete(void);

  protected:
    ListBoxSetting *listbox;
    MythDialog     *dialog;
    bool            redraw;
};

#endif

