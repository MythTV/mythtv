#ifndef PROFILEGROUP_H
#define PROFILEGROUP_H

#include <QString>
#include <QCoreApplication>

#include "mythtvexp.h"
#include "settings.h"
#include "mythwidgets.h"

class ProfileGroup;

// A parameter associated with the profile itself
class ProfileGroupStorage : public SimpleDBStorage
{
  protected:
    ProfileGroupStorage(StorageUser        *_user,
                        const ProfileGroup &_parentProfile,
                        QString             _name) :
        SimpleDBStorage(_user, "profilegroups", _name),
        m_parent(_parentProfile)
    {
    }

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
    const ProfileGroup& m_parent;
};

class ProfileGroup : public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(ProfileGroup)

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

class MTV_PUBLIC ProfileGroupEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    ProfileGroupEditor() :
        listbox(new ListBoxSetting(this)), dialog(NULL), redraw(true)
        { addChild(listbox); }

    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }

    virtual void Load(void);
    virtual void Save(void) {}
    virtual void Save(QString /*destination*/) {}

  protected slots:
    void open(int id);
    void callDelete(void);

  protected:
    ListBoxSetting *listbox;
    MythDialog     *dialog;
    bool            redraw;
};

#endif

