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

    ConfigurationGroup* profile = new VerticalConfigurationGroup(false);
    profile->setLabel(tr("ProfileGroup"));
    profile->addChild(name = new Name(*this));
    CardInfo *cardInfo = new CardInfo(*this);
    profile->addChild(cardInfo);
    CardType::fillSelections(cardInfo);
    host = new HostName(*this);
    profile->addChild(host);
    host->fillSelections();
    addChild(profile);
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

void ProfileGroup::fillSelections(SelectSetting* setting)
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

            setting->addSelection(name, id);
        }
    }

    if (!tid.isEmpty())
        setting->addSelection(tr("Transcoders"), tid);
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

void ProfileGroupEditor::open(int id) {

    ProfileGroup* profilegroup = new ProfileGroup();

    bool isdefault = false;
    bool show_profiles = true;
    bool newgroup = false;
    int profileID;
    QString pgName;

    if (id != 0)
    {
        profilegroup->loadByID(id);
        pgName = profilegroup->getName();
        if (profilegroup->isDefault())
          isdefault = true;
    }
    else
    {
        pgName = tr("New Profile Group Name");
        profilegroup->setName(pgName);
        newgroup = true;
    }

    if (! isdefault)
    {
        if (profilegroup->exec(false) == QDialog::Accepted &&
            profilegroup->allowedGroupName())
        {
            profilegroup->Save();
            profileID = profilegroup->getProfileNum();
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
        else if (newgroup)
            show_profiles = false;
    }

    if (show_profiles)
    {
        pgName = profilegroup->getName();
        profileID = profilegroup->getProfileNum();
        RecordingProfileEditor editor(profileID, pgName);
        editor.exec();
    }
    delete profilegroup;
};

void ProfileGroupEditor::Load(void)
{
    listbox->clearSelections();
    ProfileGroup::addMissingDynamicProfiles();
    ProfileGroup::fillSelections(listbox);
    listbox->addSelection(tr("(Create new profile group)"), "0");
}

DialogCode ProfileGroupEditor::exec(void)
{
    DialogCode ret = kDialogCodeAccepted;
    redraw = true;

    while ((QDialog::Accepted == ret) || redraw)
    {
        redraw = false;

        Load();

        dialog = new ConfigurationDialogWidget(GetMythMainWindow(),
                                               "ProfileGroupEditor");

        connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(callDelete()));

        int   width = 0,    height = 0;
        float wmult = 0.0f, hmult  = 0.0f;
        GetMythUI()->GetScreenSettings(width, wmult, height, hmult);

        QVBoxLayout *layout = new QVBoxLayout(dialog);
        layout->setMargin((int)(20 * hmult));
        layout->addWidget(listbox->configWidget(NULL, dialog));

        dialog->Show();

        ret = dialog->exec();

        dialog->deleteLater();
        dialog = NULL;

        if (ret == QDialog::Accepted)
            open(listbox->getValue().toInt());
    }

    return kDialogCodeRejected;
}

void ProfileGroupEditor::callDelete(void)
{
    int id = listbox->getValue().toInt();

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT id FROM profilegroups WHERE "
                   "id = :PROFILE_ID AND is_default = 0;");

    result.bindValue(":PROFILE_ID", id);

    if (result.exec() && result.next())
    {
        QString message = tr("Delete profile group:\n'%1'?")
                              .arg(ProfileGroup::getName(id));

        DialogCode value = MythPopupBox::Show2ButtonPopup(
            GetMythMainWindow(),
            "", message,
            tr("Yes, delete group"),
            tr("No, Don't delete group"), kDialogCodeButton1);

        if (kDialogCodeButton0 == value)
        {
            result.prepare("DELETE codecparams FROM codecparams, "
                            "recordingprofiles WHERE "
                            "codecparams.profile = recordingprofiles.id "
                            "AND recordingprofiles.profilegroup = :PROFILE_ID;");
            result.bindValue(":PROFILE_ID", id);
            if (!result.exec())
                MythDB::DBError("ProfileGroupEditor::callDelete -- "
                                "delete codecparams", result);

            result.prepare("DELETE FROM recordingprofiles WHERE "
                           "profilegroup = :PROFILE_ID;");
            result.bindValue(":PROFILE_ID", id);
            if (!result.exec())
                MythDB::DBError("ProfileGroupEditor::callDelete -- "
                                "delete recordingprofiles", result);

            result.prepare("DELETE FROM profilegroups WHERE id = :PROFILE_ID;");
            result.bindValue(":PROFILE_ID", id);
            if (!result.exec())
                MythDB::DBError("ProfileGroupEditor::callDelete -- "
                                "delete profilegroups", result);

            redraw = true;

            if (dialog)
                dialog->done(QDialog::Rejected);
        }
    }

}
