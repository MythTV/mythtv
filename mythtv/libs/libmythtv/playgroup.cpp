#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include <qsqldatabase.h>
#include <qheader.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>
#include "playgroup.h"
#include "programinfo.h"

// A parameter associated with the profile itself
class PlayGroupSetting: public SimpleDBStorage,
                        virtual public Setting {
protected:
    PlayGroupSetting(const PlayGroup& _parent, QString _name):
        SimpleDBStorage("playgroup", _name),
        parent(_parent) {
        setName(_name);
    };

    virtual QString whereClause(MSqlBindings& bindings);

    const PlayGroup &parent;
};

QString PlayGroupSetting::whereClause(MSqlBindings& bindings)
{
    QString nameTag(":WHERENAME");
    QString query("name = " + nameTag);

    bindings.insert(nameTag, parent.getName());

    return query;
}

class TitleMatch: public LineEditSetting, public PlayGroupSetting {
public:
    TitleMatch(const PlayGroup& _parent):
        LineEditSetting(),
        PlayGroupSetting(_parent, "titlematch") {
        setLabel(QObject::tr("Title match (regex)"));
        setHelpText(QObject::tr("Automatically set new recording rules to "
                                "use this group if the title matches this "
                                "regular expression.  For example, "
                                "\"(News|CNN)\" would match any title in "
                                "which \"News\" or \"CNN\" appears."));
    };
};

class SkipAhead: public SpinBoxSetting, public PlayGroupSetting {
public:
    SkipAhead(const PlayGroup& _parent):
        SpinBoxSetting(0, 600, 5, true, "(" + QObject::tr("default") + ")"),
        PlayGroupSetting(_parent, "skipahead") {
        setLabel(QObject::tr("Skip ahead (seconds)"));
        setHelpText(QObject::tr("How many seconds to skip forward on a fast "
                                "forward."));
    };
};

class SkipBack: public SpinBoxSetting, public PlayGroupSetting {
public:
    SkipBack(const PlayGroup& _parent):
        SpinBoxSetting(0, 600, 5, true, "(" + QObject::tr("default") + ")"),
        PlayGroupSetting(_parent, "skipback") {
        setLabel(QObject::tr("Skip back (seconds)"));
        setHelpText(QObject::tr("How many seconds to skip backward on a "
                                "rewind."));
    };
};

class JumpMinutes: public SpinBoxSetting, public PlayGroupSetting {
public:
    JumpMinutes(const PlayGroup& _parent):
        SpinBoxSetting(0, 30, 10, true, "(" + QObject::tr("default") + ")"),
        PlayGroupSetting(_parent, "jump") {
        setLabel(QObject::tr("Jump amount (in minutes)"));
        setHelpText(QObject::tr("How many minutes to jump forward or backward "
                    "when the jump keys are pressed."));
    };
};

class TimeStretch: public SpinBoxSetting, public PlayGroupSetting {
public:
    TimeStretch(const PlayGroup& _parent):
        SpinBoxSetting(45, 200, 5, false, "(" + QObject::tr("default") + ")"),
        PlayGroupSetting(_parent, "timestretch") {
        setValue(45);
        setLabel(QObject::tr("Time stretch (speed x 100)"));
        setHelpText(QObject::tr("Initial playback speed with adjusted audio.  "
                                "Use 100 for normal speed, 50 for half speed "
                                "and 200 for double speed."));
    };

    virtual void load(void) {
        PlayGroupSetting::load();
        if (intValue() < 50 || intValue() > 200)
            setValue(45);
    };

    virtual void save(void) {
        if (intValue() < 50 || intValue() > 200)
            setValue(0);
        PlayGroupSetting::save();
    }
};

PlayGroup::PlayGroup(QString _name)
    : name(_name)
{
    ConfigurationGroup* cgroup = new VerticalConfigurationGroup(false);
    cgroup->setLabel(getName() + " " + tr("Group"));

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
        MythContext::DBError("PlayGroupEditor::load", query);
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
        MythContext::DBError("PlayGroupEditor::load", query);
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
                  "WHERE name = :TITLE OR "
                  "      name = :CATEGORY OR "
                  "      (titlematch <> '' AND "
                  "       :TITLE REGEXP titlematch) ");
    query.bindValue(":TITLE", pi->title);
    query.bindValue(":CATEGORY", pi->category);
    query.exec();

    if (!query.exec())
        MythContext::DBError("GetInitialName", query);
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
        MythContext::DBError("PlayGroup::GetSetting", query);
    else if (query.next())
        res = query.value(1).toInt();

    return res;
}

PlayGroupEditor::PlayGroupEditor(void)
    : lastValue("Default")
{
    setLabel(tr("Playback Groups"));
}

void PlayGroupEditor::open(QString name) 
{
    lastValue = name;
    bool created = false;

    if (name.isEmpty())
    {
        bool ok = MythPopupBox::showGetTextPopup(gContext->GetMainWindow(), 
            tr("Create New Playback Group"),
            tr("Enter group name or press SELECT to enter text via the "
               "On Screen Keyboard"), name);
        if (!ok)
            return;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO playgroup (name) VALUES (:NAME);");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythContext::DBError("PlayGroupEditor::open", query);
        else
            created = true;
    }

    PlayGroup group(name);
    if (group.exec() == QDialog::Accepted || !created)
        lastValue = name;
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM playgroup WHERE name = :NAME;");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythContext::DBError("PlayGroupEditor::open", query);
    }
};

void PlayGroupEditor::doDelete(void) 
{
    QString name = getValue();
    if (name.isEmpty() || name == "Default")
        return;

    QString message = tr("Delete playback group:") +
        QString("\n'%1'?").arg(name);

    int value = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                               "", message,
                                               tr("Yes, delete group"),
                                               tr("No, Don't delete group"), 2);

    if (value == 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM playgroup WHERE name = :NAME;");
        query.bindValue(":NAME", name);
        if (!query.exec())
            MythContext::DBError("PlayGroupEditor::doDelete", query);

        int lastIndex = getValueIndex(name);
        lastValue = "";
        load();
        setValue(lastIndex);
    }

    setFocus();
}

void PlayGroupEditor::load() {
    clearSelections();

    addSelection(tr("Default"), "Default");

    QStringList names = PlayGroup::GetNames();
    while (!names.isEmpty())
    {
        addSelection(names.front());
        names.pop_front();
    }

    addSelection(tr("(Create new group)"), "");

    setValue(lastValue);
}

int PlayGroupEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        open(getValue());

    return QDialog::Rejected;
}

MythDialog* PlayGroupEditor::dialogWidget(MythMainWindow* parent,
                                          const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(doDelete()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(doDelete()));
    return dialog;
}
