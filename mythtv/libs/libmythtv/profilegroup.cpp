#include "recordingprofile.h"
#include "videosource.h"
#include "profilegroup.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include <qsqldatabase.h>
#include <qheader.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>
#include <qaccel.h>

QString ProfileGroupParam::whereClause(void) {
    return QString("id = %1").arg(parent.getProfileNum());
}

QString ProfileGroupParam::setClause(void) {
    return QString("id = %1, %2 = '%3'")
        .arg(parent.getProfileNum())
        .arg(getColumn())
        .arg(getValue().utf8());
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
    profile->setLabel(QObject::tr("ProfileGroup"));
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
    load();
}

void ProfileGroup::fillSelections(SelectSetting* setting) {
    QStringList cardtypes;
    QString transcodeID;

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("SELECT DISTINCT cardtype FROM capturecard;");

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next())
        {
            cardtypes.append(result.value(0).toString());
        }
    }

    result.prepare("SELECT name,id,hostname,is_default,cardtype "
                      "FROM profilegroups;");

    if (result.exec() && result.isActive() && result.size() > 0)
        while (result.next())
        {
            // Only show default profiles that match installed cards
            if (result.value(3).toInt())
            {
               bool match = false;
               for(QStringList::Iterator it = cardtypes.begin();
                         it != cardtypes.end(); it++)
                   if (result.value(4).toString() == *it)
                       match = true;
                   else if (result.value(4).toString() == "TRANSCODE")
                       transcodeID = result.value(1).toString();
               if (! match)
                   continue;
            }
            QString value = QString::fromUtf8(result.value(0).toString());
            if (result.value(2).toString() != NULL &&
                result.value(2).toString() != "")
                value += QString(" (%1)").arg(result.value(2).toString());
            setting->addSelection(value, result.value(1).toString());
        }
    if (! transcodeID.isNull())
        setting->addSelection(QObject::tr("Transcoders"), transcodeID);
}

QString ProfileGroup::getName(int group)
{
    MSqlQuery result(MSqlQuery::InitCon());
    QString querystr = QString("SELECT name from profilegroups WHERE id = %1")
                            .arg(group);
    result.prepare(querystr);

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        result.next();
        return QString::fromUtf8(result.value(0).toString());
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

    if (result.exec() && result.isActive() && result.size() > 0)
        return false;
    return true;
}

void ProfileGroup::getHostNames(QStringList *hostnames)
{
    hostnames->clear();

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("SELECT DISTINCT hostname from capturecard");

    if (result.exec() && result.isActive() && result.size() > 0)
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
        pgName = QString(QObject::tr("New Profile Group Name"));
        profilegroup->setName(pgName);
        newgroup = true;
    }

    if (! isdefault)
    {
        if (profilegroup->exec(false) == QDialog::Accepted &&
            profilegroup->allowedGroupName())
        {
            profilegroup->save();
            profileID = profilegroup->getProfileNum();
            QValueList <int> found;
            
            MSqlQuery result(MSqlQuery::InitCon());
            QString querystr = QString("SELECT name FROM recordingprofiles WHERE "
                                    "profilegroup = %1").arg(profileID);
            result.prepare(querystr);

            if (result.exec() && result.isActive() && result.size() > 0)
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
                for (QValueList <int>::Iterator j = found.begin();
                       j != found.end(); j++)
                    if (i == *j)
                        skip = true;
                if (! skip)
                {
                    
                    querystr = QString("INSERT INTO recordingprofiles SET "
                                    "name = '%1', profilegroup = %2")
                                    .arg(availProfiles[i]).arg(profileID);
                    result.prepare(querystr);
                    result.exec();
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

void ProfileGroupEditor::load() 
{
    clearSelections();
    ProfileGroup::fillSelections(this);
    addSelection(QObject::tr("(Create new profile group)"), "0");
}

int ProfileGroupEditor::exec() 
{
    int ret;
    do {
    redraw = false;
    load();
    dialog = new ConfigurationDialogWidget(gContext->GetMainWindow());
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(callDelete()));
    int width = 0, height = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(width, wmult, height, hmult);

    QVBoxLayout* layout = new QVBoxLayout(dialog, (int)(20 * hmult));
    layout->addWidget(configWidget(NULL, dialog));

    dialog->Show();

    ret = dialog->exec();

    delete dialog;
    if (ret == QDialog::Accepted)
        open(getValue().toInt());
    } while (ret == QDialog::Accepted || redraw);

    return QDialog::Rejected;
}

void ProfileGroupEditor::callDelete(void)
{
    int id = getValue().toInt();
    
    MSqlQuery result(MSqlQuery::InitCon());
    QString querystr = QString("SELECT id FROM profilegroups WHERE "
                            "id = %1 AND is_default = 0;").arg(id);
    result.prepare(querystr);
    
    if (result.exec() && result.isActive() && result.size() > 0)
    {
        result.next();
        QString message = QObject::tr("Delete profile group:") + 
                          QString("\n'%1'?").arg(ProfileGroup::getName(id));

        int value = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                   "", message, 
                                     QObject::tr("Yes, delete group"),
                                     QObject::tr("No, Don't delete group"), 2);

        if (value == 0)
        {
            querystr = QString("DELETE codecparams FROM codecparams, "
                            "recordingprofiles WHERE " 
                            "codecparams.profile = recordingprofiles.id "
                            "AND recordingprofiles.profilegroup = %1").arg(id);
            result.prepare(querystr);
            result.exec();

            querystr = QString("DELETE FROM recordingprofiles WHERE "
                            "profilegroup = %1").arg(id);
            result.prepare(querystr);
            result.exec();

            querystr = QString("DELETE FROM profilegroups WHERE id = %1;").arg(id);
            result.prepare(querystr);
            result.exec();

            redraw = true;
            dialog->done(QDialog::Rejected);
        }
    }

}

