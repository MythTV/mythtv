#ifndef PROFILEGROUP_H
#define PROFILEGROUP_H

#include <QString>
#include <QCoreApplication>

#include "mythtvexp.h"
#include "libmythui/standardsettings.h"

class ProfileGroup;

// A parameter associated with the profile itself
class ProfileGroupStorage : public SimpleDBStorage
{
  public:
    ProfileGroupStorage(StorageUser        *_user,
                        const ProfileGroup &_parentProfile,
                        const QString&      _name) :
        SimpleDBStorage(_user, "profilegroups", _name),
        m_parent(_parentProfile)
    {
    }

    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    const ProfileGroup& m_parent;
};

class ProfileGroup : public GroupSetting
{
    Q_DECLARE_TR_FUNCTIONS(ProfileGroup)

    friend class ProfileGroupEditor;
  protected:
    class ID : public AutoIncrementSetting
    {
      public:
        ID() : AutoIncrementSetting("profilegroups", "id")
        {
            setVisible(false);
        }
    };

    class Is_default : public StandardSetting
    {
      public:
        explicit Is_default(const ProfileGroup &parent) :
            StandardSetting(new ProfileGroupStorage(this, parent, "is_default"))
        {
            setVisible(false);
        }

        ~Is_default() override
        {
            delete GetStorage();
        }

        void edit(MythScreenType * /*screen*/) override { } // StandardSetting
        void resultEdit(DialogCompletionEvent * /*dce*/) override { } // StandardSetting
    };

    class Name : public MythUITextEditSetting
    {
      public:
        explicit Name(const ProfileGroup &parent) :
            MythUITextEditSetting(new ProfileGroupStorage(this, parent, "name"))
        {
            setLabel(QObject::tr("Profile Group Name"));
        }

        ~Name() override
        {
            delete GetStorage();
        }
    };

    class HostName : public MythUIComboBoxSetting
    {
      public:
        explicit HostName(const ProfileGroup &parent) :
            MythUIComboBoxSetting(new ProfileGroupStorage(this, parent,
                                                          "hostname"))
        {
            setLabel(QObject::tr("Hostname"));
        }

        ~HostName() override
        {
            delete GetStorage();
        }
        void fillSelections();
    };

    class CardInfo : public MythUIComboBoxSetting
    {
      public:
        explicit CardInfo(const ProfileGroup &parent) :
            MythUIComboBoxSetting(new ProfileGroupStorage(this, parent,
                                                          "cardtype"))
        {
            setLabel(QObject::tr("Card-Type"));
        }

        ~CardInfo() override
        {
            delete GetStorage();
        }
    };

public:
    ProfileGroup();

    virtual void loadByID(int id);

    static bool addMissingDynamicProfiles(void);
    static void fillSelections(GroupSetting* setting);
    static void getHostNames(QStringList* hostnames);
    int getProfileNum(void) const {
        return m_id->getValue().toInt();
    };

    int isDefault(void) const {
        return m_isDefault->getValue().toInt();
    };

    QString getName(void) const { return m_name->getValue(); };
    static QString getName(int group);
    void setName(const QString& newName) override // StandardSetting
        { m_name->setValue(newName); };
    bool allowedGroupName(void);

private:

    ID         *m_id         {nullptr};
    Name       *m_name       {nullptr};
    HostName   *m_host       {nullptr};
    Is_default *m_isDefault  {nullptr};
};

class MTV_PUBLIC ProfileGroupEditor :
    public GroupSetting
{
    Q_OBJECT

  public:
    ProfileGroupEditor() { setLabel(tr("Profile Group")); }

    void Load(void) override; // StandardSetting
};

#endif
