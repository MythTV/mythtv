#include "recordingprofile.h"
#include <qsqldatabase.h>
#include <qheader.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>

QString RecordingProfileParam::whereClause(void) {
  return QString("id = %1").arg(parentProfile.getProfileNum());
}

QString CodecParam::setClause(void) {
  return QString("profile = %1, name = '%2', value = '%3'")
    .arg(parentProfile.getProfileNum())
    .arg(getName())
    .arg(getValue());
}

QString CodecParam::whereClause(void) {
  return QString("profile = %1 AND name = '%2'")
    .arg(parentProfile.getProfileNum()).arg(getName());
}

void RecordingProfile::loadByID(QSqlDatabase* db, int profileId) {
    id->setValue(profileId);
    load(db);
}

void RecordingProfile::loadByName(QSqlDatabase* db, QString name) {
    QString query = QString("SELECT id FROM recordingprofiles WHERE name = '%1'").arg(name);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        result.next();
        loadByID(db, result.value(0).toInt());
    } else {
        cout << "Profile not found: " << name << endl;
    }
}

void RecordingProfileEditor::load(QSqlDatabase* db) {
    addSelection("(Create new profile)", "0");
    QSqlQuery query = db->exec("SELECT id, name FROM recordingprofiles");
    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            addSelection(query.value(1).toString(), query.value(0).toString());
}

int RecordingProfileEditor::exec(QSqlDatabase* db) {
    if (ConfigurationDialog::exec(db) == QDialog::Accepted) {
        open(getValue().toInt());
        return QDialog::Accepted;
    }

    return QDialog::Rejected;
}
