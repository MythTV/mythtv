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
    if (result.isActive() && result.numRowsAffected() > 0)
        loadByID(db, result.value(0).toInt());
    else
        cout << "Profile not found: " << name << endl;
}

RecordingProfileBrowser::RecordingProfileBrowser(MythContext* context,
                                                 QWidget* parent,
                                                 QSqlDatabase* _db)
                       : MyDialog(context, parent), db(_db) 
{

    QVBoxLayout* vbox = new QVBoxLayout(this, (int)(15 * wmult));

    listview = new MyListView(this);
    vbox->addWidget(listview);

    listview->addColumn("Profile");
    listview->setAllColumnsShowFocus(TRUE);

    idMap[new QListViewItem(listview, "(Create new profile)")] = 0;

    QSqlQuery query = db->exec("SELECT id, name FROM recordingprofiles");

    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next()) {
            QListViewItem* item = new QListViewItem(listview,
                                                    query.value(1).toString());
            idMap[item] = query.value(0).toInt();
            listview->insertItem(item);
        }

    connect(listview, SIGNAL(clicked(QListViewItem*)),
            this, SLOT(select(QListViewItem*)));
    connect(listview, SIGNAL(returnPressed(QListViewItem*)),
            this, SLOT(select(QListViewItem*)));
    connect(listview, SIGNAL(spacePressed(QListViewItem*)),
            this, SLOT(select(QListViewItem*)));
}
