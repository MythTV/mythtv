// c/c++
#include <iostream>

// myth
#include "mythlogging.h"
#include "mythdb.h"
#include "channelinfo.h"
#include "channelutil.h"
#include "channelgroup.h"
#include "channelgroupsettings.h"

#define LOC QString("Channel Group Settings: ")

// Storage class for channel group editor in settings
class ChannelGroupStorage : public Storage
{
  public:
    ChannelGroupStorage(Setting *_setting,
                    uint _chanid, QString _grpname) :
        setting(_setting), chanid(_chanid), grpname(_grpname), grpid(0) {}
    virtual ~ChannelGroupStorage() {};

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString destination);

  protected:
    Setting *setting;
    uint    chanid;
    QString grpname;
    int     grpid;
};

void ChannelGroupStorage::Load(void)
{
    setting->setValue("0");

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr = "SELECT grpid FROM channelgroupnames WHERE name = :GRPNAME";

    query.prepare(qstr);
    query.bindValue(":GRPNAME", grpname);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("ChannelGroupStorage::Load", query);
    else if (query.next())
    {
      grpid = query.value(0).toUInt();

      qstr = "SELECT * FROM channelgroup WHERE grpid = :GRPID AND "
             "chanid = :CHANID";
      query.prepare(qstr);
      query.bindValue(":GRPID",  grpid);
      query.bindValue(":CHANID", chanid);

      if (!query.exec() || !query.isActive())
          MythDB::DBError("ChannelGroupStorage::Load", query);
      else if (query.size() > 0)
        setting->setValue("1");
    }
}

void ChannelGroupStorage::Save(void)
{
    QString value = setting->getValue();

    if (value == "1")
        ChannelGroup::AddChannel(chanid, grpid);
    else
        ChannelGroup::DeleteChannel(chanid, grpid);
}

void ChannelGroupStorage::Save(QString destination)
{
    Save();
}

class ChannelCheckBox : public CheckBoxSetting, public ChannelGroupStorage
{
    Q_DECLARE_TR_FUNCTIONS(ChannelCheckBox)

  public:
    ChannelCheckBox(const ChannelGroupConfig& _parent, const uint chanid, const QString &channum,
               const QString &channame, const QString &grpname):
        CheckBoxSetting(this),
        ChannelGroupStorage(this, chanid, grpname)
    {
        //: %1 is the channel number, %2 is the channel name
        setLabel(tr("%1 %2", "Channel number with channel name")
                 .arg(channum).arg(channame));
        setHelpText(tr("Select/Unselect channels for this channel group"));
    };
};

ChannelGroupConfig::ChannelGroupConfig(QString _name)
    : name(_name)
{
    VerticalConfigurationGroup   *cgroup;
    HorizontalConfigurationGroup *columns;

    ChannelInfoList chanlist = ChannelUtil::GetChannels(0, true, "channum, callsign");
    ChannelUtil::SortChannels(chanlist, "channum", true);

    ChannelInfoList::iterator it = chanlist.begin();
    int i,j = 0;
    int p = 1;
    int pages = (int)((float)chanlist.size() / 8.0 / 3.0 + 0.5);

    do
    {
        columns = new HorizontalConfigurationGroup(false,false,false,false);
        columns->setLabel(tr("%1 Channel Group - Page %2 of %3")
                          .arg(getName())
                          .arg(p)
                          .arg(pages));

        for (j = 0; ((j < 3) && (it < chanlist.end())); ++j)
        {
            cgroup = new VerticalConfigurationGroup(false,false,false,false);

            for (i = 0; ((i < 8) && (it < chanlist.end())); ++i)
            {
                cgroup->addChild(new ChannelCheckBox(*this, it->chanid, it->channum, it->name, _name));
                ++it;
            }
            columns->addChild(cgroup);
        }

        ++p;
        addChild(columns);
    } while (it < chanlist.end());

}

ChannelGroupEditor::ChannelGroupEditor(void) :
    listbox(new ListBoxSetting(this)), lastValue("__CREATE_NEW_GROUP__")
{
    listbox->setLabel(tr("Channel Groups"));
    addChild(listbox);
}

void ChannelGroupEditor::open(QString name)
{
    lastValue = name;
    bool created = false;

    if (name == "__CREATE_NEW_GROUP__")
    {
        name = "";

        bool ok = MythPopupBox::showGetTextPopup(GetMythMainWindow(), 
            tr("Create New Channel Group"),
            tr("Enter group name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (!ok)
            return;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO channelgroupnames (name) VALUES (:NAME);");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythDB::DBError("ChannelGroupEditor::open", query);
        else
            created = true;
    }

    ChannelGroupConfig group(name);

    if (group.exec() == QDialog::Accepted || !created)
        lastValue = name;

};

void ChannelGroupEditor::doDelete(void)
{
    QString name = listbox->getValue();
    if (name == "__CREATE_NEW_GROUP__")
        return;

    QString message = tr("Delete '%1' Channel group?").arg(name);

    DialogCode value = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(),
        "", message,
        tr("Yes, delete group"),
        tr("No, Don't delete group"), kDialogCodeButton1);

    if (kDialogCodeButton0 == value)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        // Find out channel group id
        query.prepare("SELECT grpid FROM channelgroupnames WHERE name = :NAME;");
        query.bindValue(":NAME", name);
        if (!query.exec())
        {
            MythDB::DBError("ChannelGroupEditor::doDelete", query);
            return;
        }

        if (!query.next())
            return;

        uint grpid = query.value(0).toUInt();

        // Delete channels from this group
        query.prepare("DELETE FROM channelgroup WHERE grpid = :GRPID;");
        query.bindValue(":GRPID", grpid);
        if (!query.exec())
            MythDB::DBError("ChannelGroupEditor::doDelete", query);

        // Now delete the group from channelgroupnames
        query.prepare("DELETE FROM channelgroupnames WHERE name = :NAME;");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythDB::DBError("ChannelGroupEditor::doDelete", query);

        lastValue = "__CREATE_NEW_GROUP__";
        Load();
    }

    listbox->setFocus();
}

void ChannelGroupEditor::Load(void)
{
    listbox->clearSelections();

    ChannelGroupList changrplist;

    changrplist = ChannelGroup::GetChannelGroups();

    ChannelGroupList::iterator it;

    for (it = changrplist.begin(); it < changrplist.end(); ++it)
       listbox->addSelection(it->name);

    listbox->addSelection(tr("(Create new group)"), "__CREATE_NEW_GROUP__");

    listbox->setValue(lastValue);
}

DialogCode ChannelGroupEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        open(listbox->getValue());

    return kDialogCodeRejected;
}

MythDialog* ChannelGroupEditor::dialogWidget(MythMainWindow* parent,
                                             const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(doDelete()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(doDelete()));
    return dialog;
}
