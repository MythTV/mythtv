#ifndef PROFILEGROUP_H
#define PROFILEGROUP_H

#include <QString>
#include <QCoreApplication>

#include "mythtvexp.h"
#include "settings.h"
#include "standardsettings.h"
#include "mythwidgets.h"

class ProfileGroup;

// A parameter associated with the profile itself
class ProfileGroupStorage : public SimpleDBStorage
{
  public:
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
        Is_default(const ProfileGroup &parent) :
            StandardSetting(new ProfileGroupStorage(this, parent, "is_default"))
        {
            setVisible(false);
        }

        virtual void edit(MythScreenType * /*screen*/) { }
        virtual void resultEdit(DialogCompletionEvent * /*dce*/) { }
    };

    class Name : public MythUITextEditSetting
    {
      public:
        Name(const ProfileGroup &parent) :
            MythUITextEditSetting(new ProfileGroupStorage(this, parent, "name"))
        {
            setLabel(QObject::tr("Profile Group Name"));
        }
    };

    class HostName : public MythUIComboBoxSetting
    {
      public:
        HostName(const ProfileGroup &parent) :
            MythUIComboBoxSetting(new ProfileGroupStorage(this, parent,
                                                          "hostname"))
        {
            setLabel(QObject::tr("Hostname"));
        }
        void fillSelections();
    };

    class CardInfo : public MythUIComboBoxSetting
    {
      public:
        CardInfo(const ProfileGroup &parent) :
            MythUIComboBoxSetting(new ProfileGroupStorage(this, parent,
                                                          "cardtype"))
        {
            setLabel(QObject::tr("Card-Type"));
        }
    };

public:
    ProfileGroup();

    virtual bool keyPressEvent(QKeyEvent *);
    virtual void loadByID(int id);

    static bool addMissingDynamicProfiles(void);
    static void fillSelections(GroupSetting* setting);
    static void getHostNames(QStringList* hostnames);
    int getProfileNum(void) const {
        return id->getValue().toInt();
    };

    int isDefault(void) const {
        return is_default->getValue().toInt();
    };

    QString getName(void) const { return name->getValue(); };
    static QString getName(int group);
    void setName(const QString& newName) { name->setValue(newName); };
    bool allowedGroupName(void);

private:
    void CallDelete(void);

    ID* id;
    Name* name;
    HostName* host;
    Is_default* is_default;
};

class MTV_PUBLIC ProfileGroupEditor :
    public GroupSetting
{
    Q_OBJECT

  public:
    ProfileGroupEditor() { }

    virtual void Load(void);

  protected slots:
    void ShowNewProfileDialog(void);
    void CreateNewProfile(QString name);
};

#endif
