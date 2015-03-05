#include "recordingprofile.h"
#include "videosource.h"
#include "profilegroup.h"
#include "mythdb.h"
#include "mythuihelper.h"
#include "cardutil.h"

QString ProfileGroupStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString idTag(":WHEREID");
    QString query("id = " + idTag);

    bindings.insert(idTag, m_parent.getProfileNum());

    return query;
}

QString ProfileGroupStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString idTag(":SETID");
    QString colTag(":SET" + GetColumnName().toUpper());

    QString query("id = " + idTag + ", " +
            GetColumnName() + " = " + colTag);

    bindings.insert(idTag, m_parent.getProfileNum());
    bindings.insert(colTag, user->GetDBValue());

    return query;
}

void ProfileGroup::HostName::fillSelections()
{
    QStringList hostnames;
    ProfileGroup::getHostNames(&hostnames);
    for(QStringList::Iterator it = hostnames.begin();
                 it != hostnames.end(); it++)
        this->addSelection(*it);
}

ProfileGroup::ProfileGroup()
{
    // This must be first because it is needed to load/save the other settings
    addChild(id = new ID());
    addChild(is_default = new Is_default(*this));

    setLabel(tr("ProfileGroup"));
    addChild(name = new Name(*this));
    CardInfo *cardInfo = new CardInfo(*this);
    addChild(cardInfo);
    CardType::fillSelections(cardInfo);
    host = new HostName(*this);
    addChild(host);
    host->fillSelections();
};

void ProfileGroup::loadByID(int profileId) {
    id->setValue(profileId);
    Load();
}

bool ProfileGroup::addMissingDynamicProfiles(void)
{
    // We need separate profiles for different V4L2 card types
    QStringList existing;
    MSqlQuery   query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT cardtype FROM profilegroups");

    if (!query.exec())
    {
        MythDB::DBError("ProfileGroup::createMissingDynamicProfiles", query);
        return false;
    }

    while (query.next())
        existing.push_back(query.value(0).toString());


    const uint num_profiles = 4;
    const QString profile_names[num_profiles] = { "Default", "Live TV",
                                                  "High Quality",
                                                  "Low Quality" };

    CardUtil::InputTypes cardtypes = CardUtil::GetInputTypes();

    CardUtil::InputTypes::iterator Itype = cardtypes.begin();
    for ( ; Itype != cardtypes.end(); ++Itype)
    {
        if (Itype.key().startsWith("V4L2:") && existing.indexOf(Itype.key()) == -1)
        {
            // Add dynamic profile group
            query.prepare("INSERT INTO profilegroups SET name = "
                          ":CARDNAME, cardtype = :CARDTYPE, is_default = 1;");
            query.bindValue(":CARDTYPE", Itype.key());
            query.bindValue(":CARDNAME", Itype.value());
            if (!query.exec())
            {
                MythDB::DBError("Unable to insert V4L2 profilegroup.", query);
                return false;
            }

            // get the id of the new profile group
            int groupid = query.lastInsertId().toInt();

            for (uint idx = 0 ; idx < num_profiles; ++idx)
            {
                // insert the recording profiles
                query.prepare("INSERT INTO recordingprofiles SET name = "
                              ":NAME, profilegroup = :GROUPID;");
                query.bindValue(":NAME", profile_names[idx]);
                query.bindValue(":GROUPID", groupid);
                if (!query.exec())
                {
                    MythDB::DBError("Unable to insert 'Default' "
                                    "recordingprofile.", query);
                    return false;
                }
            }
        }
    }

    return true;
}

void ProfileGroup::fillSelections(GroupSetting* setting)
{
    CardUtil::InputTypes cardtypes = CardUtil::GetInputTypes();
    QString     tid       = QString::null;

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT name, id, hostname, is_default, cardtype "
        "FROM profilegroups");

    if (!result.exec())
    {
        MythDB::DBError("ProfileGroup::fillSelections", result);
        return;
    }

    while (result.next())
    {
        QString name       = result.value(0).toString();
        QString id         = result.value(1).toString();
        QString hostname   = result.value(2).toString();
        bool    is_default = (bool) result.value(3).toInt();
        QString cardtype   = result.value(4).toString();

        // Only show default profiles that match installed cards
        // Workaround for #12481 in fixes/0.27
        bool have_cardtype = cardtypes.contains(cardtype);
        if (is_default && (cardtype == "TRANSCODE") && !have_cardtype)
        {
            tid = id;
        }
        else if (have_cardtype)
        {
            if (!hostname.isEmpty())
                name += QString(" (%1)").arg(result.value(2).toString());

            if (is_default)
            {
                setting->addChild(new RecordingProfileEditor(id.toInt(), name));
            }
            else
            {
                ProfileGroup *profileGroup = new ProfileGroup();
                profileGroup->loadByID(id.toInt());
                profileGroup->setLabel(name);
                profileGroup->addChild(
                    new RecordingProfileEditor(id.toInt(), tr("Profiles")));
                setting->addChild(profileGroup);
            }
        }
    }

    if (!tid.isEmpty())
    {
        setting->addChild(new RecordingProfileEditor(tid.toInt(),
                                                     tr("Transcoders")));
    }
}

QString ProfileGroup::getName(int group)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT name from profilegroups WHERE id = :PROFILE_ID;");
    result.bindValue(":PROFILE_ID", group);
    if (result.exec() && result.next())
    {
        return result.value(0).toString();
    }

    return NULL;
}

bool ProfileGroup::allowedGroupName(void)
{
    MSqlQuery result(MSqlQuery::InitCon());
    QString querystr = QString("SELECT DISTINCT id FROM profilegroups WHERE "
                            "name = '%1' AND hostname = '%2';")
                            .arg(getName()).arg(host->getValue());
    result.prepare(querystr);

    if (result.exec() && result.next())
        return false;
    return true;
}

void ProfileGroup::getHostNames(QStringList *hostnames)
{
    hostnames->clear();

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("SELECT DISTINCT hostname from capturecard");

    if (result.exec())
    {
        while (result.next())
            hostnames->append(result.value(0).toString());
    }
}

void ProfileGroupEditor::Load(void)
{
    clearSettings();
    ProfileGroup::addMissingDynamicProfiles();
    ButtonStandardSetting *newProfile =
        new ButtonStandardSetting(tr("(Create new profile group)"));
    connect(newProfile, SIGNAL(clicked()), SLOT(ShowNewProfileDialog()));
    addChild(newProfile);
    ProfileGroup::fillSelections(this);
    StandardSetting::Load();
}

void ProfileGroupEditor::ShowNewProfileDialog(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythTextInputDialog *settingdialog =
        new MythTextInputDialog(popupStack, tr("New Profile Group Name"));

    if (settingdialog->Create())
    {
        connect(settingdialog, SIGNAL(haveResult(QString)),
                SLOT(createNewProfile(QString)));
        popupStack->AddScreen(settingdialog);
    }
    else
        delete settingdialog;
}

void ProfileGroupEditor::CreateNewProfile(QString name)
{
    ProfileGroup* profilegroup = new ProfileGroup();

    profilegroup->setName(name);
    if (profilegroup->allowedGroupName())
    {
        profilegroup->setLabel(name);
        if (!profilegroup->isDefault())
        {
            profilegroup->Save();
            int profileID = profilegroup->getProfileNum();
            vector<int> found;

            MSqlQuery result(MSqlQuery::InitCon());
            result.prepare("SELECT name FROM recordingprofiles WHERE "
                           "profilegroup = :PROFILE_ID");

            result.bindValue(":PROFILE_ID", profileID);

            if (result.exec())
            {
                while (result.next())
                {
                    for (int i = 0; availProfiles[i] != ""; i++)
                      if (result.value(0).toString() == availProfiles[i])
                          found.push_back(i);
                }
            }

            for(int i = 0; availProfiles[i] != ""; i++)
            {
                bool skip = false;

                for (vector<int>::iterator j = found.begin();
                     j != found.end(); ++j)
                {
                    if (i == *j)
                        skip = true;
                }
                if (! skip)
                {
                    result.prepare("INSERT INTO recordingprofiles "
                                   "(name, profilegroup) VALUES (:NAME, :PROFID);");
                    result.bindValue(":NAME", availProfiles[i]);
                    result.bindValue(":PROFID", profileID);
                    if (!result.exec())
                        MythDB::DBError("ProfileGroup::getHostNames", result);
                }
            }
        }

        addChild(profilegroup);
        emit settingsChanged(this);
    }
    else
        delete profilegroup;
}

bool ProfileGroup::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;

    if (GetMythMainWindow()->TranslateKeyPress("Global", e, actions))
        return true;

    foreach (const QString &action, actions)
    {
        if (action == "DELETE")
        {
            CallDelete();
            return true;
        }
    }

    return false;
}

void ProfileGroup::CallDelete(void)
{
    if (!isDefault())
    {
        int id = getProfileNum();

        QString message = tr("Delete profile group:\n'%1'?").arg(getName());

        MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");

        MythDialogBox* menu =
            new MythDialogBox(message, popupStack, "deletemenu");

        if (menu->Create())
        {
            menu->SetReturnEvent(this, "deletemenu");

            auto deleteSlot = [id]() {
                MSqlQuery query(MSqlQuery::InitCon());
                query.prepare("DELETE codecparams FROM codecparams, "
                              "recordingprofiles WHERE "
                              "codecparams.profile = recordingprofiles.id "
                              "AND recordingprofiles.profilegroup = :PROFILE_ID;");
                query.bindValue(":PROFILE_ID", id);
                if (!query.exec())
                    MythDB::DBError("ProfileGroupEditor::CallDelete -- "
                                    "delete codecparams", query);

                query.prepare("DELETE FROM recordingprofiles WHERE "
                              "profilegroup = :PROFILE_ID;");
                query.bindValue(":PROFILE_ID", id);
                if (!query.exec())
                    MythDB::DBError("ProfileGroupEditor::CallDelete -- "
                                    "delete recordingprofiles", query);

                query.prepare("DELETE FROM profilegroups WHERE id = :PROFILE_ID;");
                query.bindValue(":PROFILE_ID", id);
                if (!query.exec())
                    MythDB::DBError("ProfileGroupEditor::CallDelete -- "
                                    "delete profilegroups", query);

            };

            menu->AddButton(tr("No, Don't delete group"));
            menu->AddButton(tr("Yes, delete group"), deleteSlot);
            popupStack->AddScreen(menu);
        }
        else
            delete menu;
    }
}
