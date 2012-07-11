// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythdb.h"
#include "playgroup.h"
#include "programinfo.h"
#include "mythwidgets.h"

class PlayGroupConfig: public ConfigurationWizard
{
    Q_DECLARE_TR_FUNCTIONS(PlayGroupConfig)

 public:
    PlayGroupConfig(QString _name);
    QString getName(void) const { return name; }

 private:
    QString name;
};

// A parameter associated with the profile itself
class PlayGroupDBStorage : public SimpleDBStorage
{
  protected:
    PlayGroupDBStorage(Setting         *_setting,
                       const PlayGroupConfig &_parent,
                       QString          _name) :
        SimpleDBStorage(_setting, "playgroup", _name), parent(_parent)
    {
        _setting->setName(_name);
    }

    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const PlayGroupConfig &parent;
};

QString PlayGroupDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString nameTag(":WHERENAME");
    QString query("name = " + nameTag);

    bindings.insert(nameTag, parent.getName());

    return query;
}

class TitleMatch : public LineEditSetting, public PlayGroupDBStorage
{
  public:
    TitleMatch(const PlayGroupConfig& _parent):
        LineEditSetting(this), PlayGroupDBStorage(this, _parent, "titlematch")
    {
        setLabel(PlayGroupConfig::tr("Title match (regex)"));
        setHelpText(PlayGroupConfig::tr("Automatically set new recording rules "
                                         "to use this group if the title "
                                         "matches this regular expression. "
                                         "For example, \"(News|CNN)\" would "
                                         "match any title in which \"News\" or "
                                         "\"CNN\" appears."));
    };
};

class SkipAhead : public SpinBoxSetting, public PlayGroupDBStorage
{
  public:
    SkipAhead(const PlayGroupConfig& _parent):
        SpinBoxSetting(this, 0, 600, 5, true, PlayGroupConfig::tr("(default)")),
        PlayGroupDBStorage(this, _parent, "skipahead") 
    {
        setLabel(PlayGroupConfig::tr("Skip ahead (seconds)"));
        setHelpText(PlayGroupConfig::tr("How many seconds to skip forward on "
                                        "a fast forward."));
    };
};

class SkipBack : public SpinBoxSetting, public PlayGroupDBStorage
{
  public:
    SkipBack(const PlayGroupConfig& _parent):
        SpinBoxSetting(this, 0, 600, 5, true, PlayGroupConfig::tr("(default)")),
        PlayGroupDBStorage(this, _parent, "skipback")
    {
        setLabel(PlayGroupConfig::tr("Skip back (seconds)"));
        setHelpText(PlayGroupConfig::tr("How many seconds to skip backward on "
                                        "a rewind."));
    };
};

class JumpMinutes : public SpinBoxSetting, public PlayGroupDBStorage
{
  public:
    JumpMinutes(const PlayGroupConfig& _parent):
        SpinBoxSetting(this, 0, 30, 10, true, PlayGroupConfig::tr("(default)")),
        PlayGroupDBStorage(this, _parent, "jump")
    {
        setLabel(PlayGroupConfig::tr("Jump amount (minutes)"));
        setHelpText(PlayGroupConfig::tr("How many minutes to jump forward or "
                                        "backward when the jump keys are "
                                        "pressed."));
    };
};

class TimeStretch : public SpinBoxSetting, public PlayGroupDBStorage
{
  public:
    TimeStretch(const PlayGroupConfig& _parent):
        SpinBoxSetting(this, 45, 200, 5, false, 
            PlayGroupConfig::tr("(default)")),
        PlayGroupDBStorage(this, _parent, "timestretch")
    {
        setValue(45);
        setLabel(PlayGroupConfig::tr("Time stretch (speed x 100)"));
        setHelpText(PlayGroupConfig::tr("Initial playback speed with adjusted "
                                        "audio. Use 100 for normal speed, 50 "
                                        "for half speed and 200 for double "
                                        "speed."));
    };

    virtual void Load(void)
    {
        PlayGroupDBStorage::Load();
        if (intValue() < 50 || intValue() > 200)
            setValue(45);
    }

    virtual void Save(void)
    {
        if (intValue() < 50 || intValue() > 200)
        {
            // We need to bypass the bounds checking that would
            // normally occur in order to get the special value of 0
            // into the database
            IntegerSetting::setValue(0);
        }
        PlayGroupDBStorage::Save();
    }

    virtual void Save(QString /*destination*/) { Save(); }
};

PlayGroupConfig::PlayGroupConfig(QString _name) : name(_name)
{
    ConfigurationGroup* cgroup = new VerticalConfigurationGroup(false);
    //: %1 is the name of the playgroup
    cgroup->setLabel(tr("%1 Group", "Play Group").arg(getName()));

    cgroup->addChild(new TitleMatch(*this));
    cgroup->addChild(new SkipAhead(*this));
    cgroup->addChild(new SkipBack(*this));
    cgroup->addChild(new JumpMinutes(*this));
    cgroup->addChild(new TimeStretch(*this));

    addChild(cgroup);
};

int PlayGroup::GetCount(void)
{
    int names = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT COUNT(name) FROM playgroup "
                  "WHERE name <> 'Default' ORDER BY name;");
    if (!query.exec())
        MythDB::DBError("PlayGroupConfig::GetCount()", query);
    else if (query.next())
        names = query.value(0).toInt();

    return names;
}

QStringList PlayGroup::GetNames(void)
{
    QStringList names;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM playgroup "
                  "WHERE name <> 'Default' ORDER BY name;");
    if (!query.exec())
        MythDB::DBError("PlayGroupConfig::GetNames()", query);
    else
    {
        while (query.next())
            names << query.value(0).toString();
    }

    return names;
}

QString PlayGroup::GetInitialName(const ProgramInfo *pi)
{
    QString res = "Default";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM playgroup "
                  "WHERE name = :TITLE1 OR "
                  "      name = :CATEGORY OR "
                  "      (titlematch <> '' AND "
                  "       :TITLE2 REGEXP titlematch) ");
    query.bindValue(":TITLE1", pi->GetTitle());
    query.bindValue(":TITLE2", pi->GetTitle());
    query.bindValue(":CATEGORY", pi->GetCategory());

    if (!query.exec())
        MythDB::DBError("GetInitialName", query);
    else if (query.next())
        res = query.value(0).toString();

    return res;
}

int PlayGroup::GetSetting(const QString &name, const QString &field,
                          int defval)
{
    int res = defval;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT name, %1 FROM playgroup "
                          "WHERE (name = :NAME OR name = 'Default') "
                          "      AND %2 <> 0 "
                          "ORDER BY name = 'Default';")
                  .arg(field).arg(field));
    query.bindValue(":NAME", name);
    if (!query.exec())
        MythDB::DBError("PlayGroupConfig::GetSetting", query);
    else if (query.next())
        res = query.value(1).toInt();

    return res;
}

PlayGroupEditor::PlayGroupEditor(void) :
    listbox(new ListBoxSetting(this)), lastValue("Default")
{
    listbox->setLabel(tr("Playback Groups"));
    addChild(listbox);
}

void PlayGroupEditor::open(QString name)
{
    lastValue = name;
    bool created = false;

    if (name == "__CREATE_NEW_GROUP__")
    {
        name = "";
        bool ok = MythPopupBox::showGetTextPopup(GetMythMainWindow(),
            tr("Create New Playback Group"),
            tr("Enter group name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (!ok)
            return;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO playgroup (name) VALUES (:NAME);");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythDB::DBError("PlayGroupEditor::open", query);
        else
            created = true;
    }

    PlayGroupConfig group(name);
    if (group.exec() == QDialog::Accepted || !created)
        lastValue = name;
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM playgroup WHERE name = :NAME;");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythDB::DBError("PlayGroupEditor::open", query);
    }
};

void PlayGroupEditor::doDelete(void)
{
    QString name = listbox->getValue();
    if (name == "__CREATE_NEW_GROUP__" || name == "Default")
        return;

    QString message = tr("Delete playback group:\n'%1'?").arg(name);

    DialogCode value = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(),
        "", message,
        tr("Yes, delete group"),
        tr("No, Don't delete group"), kDialogCodeButton1);

    if (kDialogCodeButton0 == value)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM playgroup WHERE name = :NAME;");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythDB::DBError("PlayGroupEditor::doDelete", query);

        int lastIndex = listbox->getValueIndex(name);
        lastValue = "";
        Load();
        listbox->setValue(lastIndex);
    }

    listbox->setFocus();
}

void PlayGroupEditor::Load(void)
{
    listbox->clearSelections();

    listbox->addSelection(tr("Default"), "Default");

    QStringList names = PlayGroup::GetNames();
    while (!names.isEmpty())
    {
        listbox->addSelection(names.front());
        names.pop_front();
    }

    listbox->addSelection(tr("(Create new group)"), "__CREATE_NEW_GROUP__");

    listbox->setValue(lastValue);
}

DialogCode PlayGroupEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        open(listbox->getValue());

    return kDialogCodeRejected;
}

MythDialog* PlayGroupEditor::dialogWidget(MythMainWindow* parent,
                                          const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(doDelete()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(doDelete()));
    return dialog;
}
