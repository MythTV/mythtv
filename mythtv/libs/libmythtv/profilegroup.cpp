#include "recordingprofile.h"
#include "videosource.h"
#include "profilegroup.h"
#include "libmyth/mythcontext.h"
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
        .arg(getValue());
}

void ProfileGroup::HostName::fillSelections(QSqlDatabase *db)
{
    QStringList hostnames;
    ProfileGroup::getHostNames(db, &hostnames);
    for(QStringList::Iterator it = hostnames.begin();
                 it != hostnames.end(); it++)
        this->addSelection(*it);
}

ProfileGroup::ProfileGroup(QSqlDatabase *_db)
{
    // This must be first because it is needed to load/save the other settings
    addChild(id = new ID());
    db = _db;
    addChild(is_default = new Is_default(*this));

    ConfigurationGroup* profile = new VerticalConfigurationGroup(false);
    profile->setLabel("ProfileGroup");
    profile->addChild(name = new Name(*this));
    CardInfo *cardInfo = new CardInfo(*this);
    profile->addChild(cardInfo);
    CardType::fillSelections(cardInfo);
    host = new HostName(*this);
    profile->addChild(host);
    host->fillSelections(db);
    addChild(profile);
};

void ProfileGroup::loadByID(int profileId) {
    id->setValue(profileId);
    load(db);
}

void ProfileGroup::fillSelections(QSqlDatabase* db, SelectSetting* setting) {
    QStringList cardtypes;
    QString transcodeID;
    QSqlQuery result = db->exec("SELECT DISTINCT cardtype FROM capturecard;");
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            cardtypes.append(result.value(0).toString());
    result = db->exec("SELECT name,id,hostname,is_default,cardtype "
                      "FROM profilegroups;");
    if (result.isActive() && result.numRowsAffected() > 0)
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
            QString value = result.value(0).toString();
            if (result.value(2).toString() != NULL &&
                result.value(2).toString() != "")
                value += QString(" (%1)").arg(result.value(2).toString());
            setting->addSelection(value, result.value(1).toString());
        }
    if (! transcodeID.isNull())
        setting->addSelection("Transcoders", transcodeID);
}

QString ProfileGroup::getName(QSqlDatabase *db, int group)
{
    QString query = QString("SELECT name from profilegroups WHERE id = %1")
                            .arg(group);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0)
    {
        result.next();
        return result.value(0).toString();
    }
    return NULL;
}

bool ProfileGroup::allowedGroupName(void)
{
    QString query = QString("SELECT DISTINCT id FROM profilegroups WHERE "
                            "name = '%1' AND hostname = '%2';")
                            .arg(getName()).arg(host->getValue());
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0)
        return false;
    return true;
}

void ProfileGroup::getHostNames(QSqlDatabase* db, QStringList *hostnames)
{
    hostnames->clear();
    QString query = QString("SELECT DISTINCT hostname from capturecard");
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            hostnames->append(result.value(0).toString());
}

void ProfileGroupEditor::open(int id) {

    ProfileGroup* profilegroup = new ProfileGroup(db);

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
        pgName = QString("New Profile Group Name");
        profilegroup->setName(pgName);
        newgroup = true;
    }
    if (! isdefault)
    {
        if (profilegroup->exec(db, false) == QDialog::Accepted &&
            profilegroup->allowedGroupName())
        {
            profilegroup->save(db);
            profileID = profilegroup->getProfileNum();
            QValueList <int> found;
            QString query = QString("SELECT name FROM recordingprofiles WHERE "
                                    "profilegroup = %1").arg(profileID);
            QSqlQuery result = db->exec(query);
            if (result.isActive() && result.numRowsAffected() > 0)
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
                    if(i == *j)
                        skip = true;
                if (! skip)
                {
                    query = QString("INSERT INTO recordingprofiles SET "
                                    "name = '%1', profilegroup = %2")
                                    .arg(availProfiles[i]).arg(profileID);
                    QSqlQuery result = db->exec(query);
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
        RecordingProfileEditor editor(db,profileID,pgName);
        editor.exec(db);
    }
    delete profilegroup;
};

void ProfileGroupEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection(QObject::tr("(Create new profile group)"), "0");
    ProfileGroup::fillSelections(db, this);
}

int ProfileGroupEditor::exec(QSqlDatabase* db) {
    int ret;
    do {
    redraw = false;
    load(db);
    dialog = new ConfigurationDialogWidget(gContext->GetMainWindow());
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(callDelete()));
    int width = 0, height = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(width, wmult, height, hmult);

    QVBoxLayout* layout = new QVBoxLayout(dialog, (int)(20 * hmult));
    layout->addWidget(configWidget(NULL, dialog));

    dialog->setCursor(QCursor(Qt::ArrowCursor));
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
    QString query = QString("SELECT id FROM profilegroups WHERE "
                            "id = %1 AND is_default = 0;").arg(id);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0)
    {
        result.next();
        QString message = QString("Delete profile group:\n'%1'?")
                                  .arg(ProfileGroup::getName(db, id));
        Myth2ButtonDialog *dlg = new Myth2ButtonDialog(
                                       gContext->GetMainWindow(),
                                       "", message, "Yes, delete group",
                                       "No, Don't delete group", 2);
        int value = dlg->exec();
        delete dlg;
        if (value == 1)
        {
            query = QString("DELETE codecparams FROM codecparams, "
                            "recordingprofiles WHERE " 
                            "codecparams.profile = recordingprofiles.id "
                            "AND recordingprofiles.profilegroup = %1").arg(id);
            db->exec(query);
            query = QString("DELETE FROM recordingprofiles WHERE "
                            "profilegroup = %1").arg(id);
            db->exec(query);
            query = QString("DELETE FROM profilegroups WHERE id = %1;").arg(id);
            db->exec(query);
            redraw = true;
            dialog->done(QDialog::Rejected);
        }
    }
}

