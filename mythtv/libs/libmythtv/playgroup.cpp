// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythdb.h"
#include "playgroup.h"
#include "programinfo.h"
#include "standardsettings.h"

// A parameter associated with the profile itself
class PlayGroupDBStorage : public SimpleDBStorage
{
  public:
    PlayGroupDBStorage(StandardSetting *_setting,
                       const PlayGroupConfig &_parent,
                       QString          _name) :
        SimpleDBStorage(_setting, "playgroup", _name), parent(_parent)
    {
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

class TitleMatch : public MythUITextEditSetting
{
  public:
    explicit TitleMatch(const PlayGroupConfig& _parent):
        MythUITextEditSetting(new PlayGroupDBStorage(this, _parent, "titlematch"))
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

class SkipAhead : public MythUISpinBoxSetting
{
  public:
    explicit SkipAhead(const PlayGroupConfig& _parent):
        MythUISpinBoxSetting(new PlayGroupDBStorage(this, _parent, "skipahead"),
                             0, 600, 5, true, PlayGroupConfig::tr("(default)"))

    {
        setLabel(PlayGroupConfig::tr("Skip ahead (seconds)"));
        setHelpText(PlayGroupConfig::tr("How many seconds to skip forward on "
                                        "a fast forward."));
    };
};

class SkipBack : public MythUISpinBoxSetting
{
  public:
    explicit SkipBack(const PlayGroupConfig& _parent):
        MythUISpinBoxSetting(new PlayGroupDBStorage(this, _parent, "skipback"),
                             0, 600, 5, true, PlayGroupConfig::tr("(default)"))
    {
        setLabel(PlayGroupConfig::tr("Skip back (seconds)"));
        setHelpText(PlayGroupConfig::tr("How many seconds to skip backward on "
                                        "a rewind."));
    };
};

class JumpMinutes : public MythUISpinBoxSetting
{
  public:
    explicit JumpMinutes(const PlayGroupConfig& _parent):
        MythUISpinBoxSetting(new PlayGroupDBStorage(this, _parent, "jump"),
                             0, 30, 10, true, PlayGroupConfig::tr("(default)"))
    {
        setLabel(PlayGroupConfig::tr("Jump amount (minutes)"));
        setHelpText(PlayGroupConfig::tr("How many minutes to jump forward or "
                                        "backward when the jump keys are "
                                        "pressed."));
    };
};

class TimeStretch : public MythUISpinBoxSetting
{
  public:
    explicit TimeStretch(const PlayGroupConfig& _parent):
        MythUISpinBoxSetting(new PlayGroupDBStorage(this, _parent, "timestretch"),
                             45, 200, 5, false,
                             PlayGroupConfig::tr("(default)"))
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
        StandardSetting::Load();
        if (intValue() < 50 || intValue() > 200)
            setValue(45);
    }

    virtual void Save(void)
    {
        if (intValue() < 50 || intValue() > 200)
            setValue(0);
        StandardSetting::Save();
    }
};

PlayGroupConfig::PlayGroupConfig(const QString &/*label*/, const QString &name,
                                 bool isNew)
    : m_isNew(isNew)
{
    setName(name);

    //: %1 is the name of the playgroup
    setLabel(tr("%1 Group", "Play Group").arg(getName()));

    addChild(m_titleMatch = new TitleMatch(*this));
    addChild(m_skipAhead = new SkipAhead(*this));
    addChild(m_skipBack = new SkipBack(*this));
    addChild(m_jumpMinutes = new JumpMinutes(*this));
    addChild(m_timeStrech = new TimeStretch(*this));

    // Ensure new entries are saved on exit
    if (isNew)
        setChanged(true);
}

void PlayGroupConfig::updateButton(MythUIButtonListItem *item)
{
    GroupSetting::updateButton(item);
    item->SetText("", "value");
}

void PlayGroupConfig::Save()
{
    if (m_isNew)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("INSERT playgroup "
                        "(name, titlematch, skipahead, skipback, jump, timestretch) "
                        "VALUES "
                        "(:NEWNAME, :TITLEMATCH, :SKIPAHEAD, :SKIPBACK, :JUMP, :TIMESTRETCH);");

        query.bindValue(":NEWNAME",     getName());
        query.bindValue(":TITLEMATCH",  m_titleMatch->getValue());
        query.bindValue(":SKIPAHEAD",   m_skipAhead->intValue());
        query.bindValue(":SKIPBACK",    m_skipBack->intValue());
        query.bindValue(":JUMP",        m_jumpMinutes->intValue());
        query.bindValue(":TIMESTRETCH", m_timeStrech->intValue());

        if (!query.exec())
            MythDB::DBError("PlayGroupConfig::Save", query);
    }
    else
        GroupSetting::Save();
}

bool PlayGroupConfig::canDelete(void)
{
    return (getName() != "Default");
}

void PlayGroupConfig::deleteEntry(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM playgroup "
                    "WHERE name = :NAME ;");
    query.bindValue(":NAME", getName());

    if (!query.exec())
        MythDB::DBError("PlayGroupConfig::deleteEntry", query);
}

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


PlayGroupEditor::PlayGroupEditor()
    : m_addGroupButton(NULL)
{
    setLabel(tr("Playback Groups"));
    m_addGroupButton = new ButtonStandardSetting(tr("Create New Playback Group"));
    addChild(m_addGroupButton);
    connect(m_addGroupButton, SIGNAL(clicked()),
            this, SLOT(CreateNewPlayBackGroup()));
}

void PlayGroupEditor::CreateNewPlayBackGroup()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythTextInputDialog *settingdialog =
        new MythTextInputDialog(popupStack, tr("Enter new group name"));

    if (settingdialog->Create())
    {
        connect(settingdialog, SIGNAL(haveResult(QString)),
                SLOT(CreateNewPlayBackGroupSlot(const QString&)));
        popupStack->AddScreen(settingdialog);
    }
    else
        delete settingdialog;
}

void PlayGroupEditor::CreateNewPlayBackGroupSlot(const QString& name)
{
    if (name.isEmpty())
    {
        ShowOkPopup(tr("Sorry, this Playback Group name cannot be blank."));
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name "
                  "FROM playgroup "
                  "WHERE name = :NAME");
    query.bindValue(":NAME", name);

    if (!query.exec())
    {
        MythDB::DBError("CreateNewPlayBackGroup", query);
        return;
    }

    if (query.next())
    {
        ShowOkPopup(tr("Sorry, this Playback Group name is already in use."));
        return;
    }

    addChild(new PlayGroupConfig(name, name, true));

    emit settingsChanged(NULL);
}

void PlayGroupEditor::Load()
{
    addChild(new PlayGroupConfig(tr("Default"), "Default"));

    QStringList names = PlayGroup::GetNames();
    while (!names.isEmpty())
    {
        addChild(new PlayGroupConfig(names.front(), names.front()));
        names.pop_front();
    }

    //Load all the groups
    GroupSetting::Load();

    //TODO select the new one or the edited one
    emit settingsChanged(NULL);
}
