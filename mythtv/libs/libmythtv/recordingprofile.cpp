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

RecordingProfileBrowser::RecordingProfileBrowser(MythContext* context,
                                                 QWidget* parent,
                                                 QSqlDatabase* _db):
    QDialog(parent), db(_db) {

    m_context = context;

    int screenwidth, screenheight;
    float wmult, hmult;
    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    context->ThemeWidget(this);

    setFont(QFont("Arial", (int)(context->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));


    QVBoxLayout* vbox = new QVBoxLayout(this, (int)(15 * wmult));

    listview = new QListView(this);
    vbox->addWidget(listview);

    listview->addColumn("Profile");
    listview->setAllColumnsShowFocus(TRUE);

    listview->viewport()->setPalette(palette());
    listview->horizontalScrollBar()->setPalette(palette());
    listview->verticalScrollBar()->setPalette(palette());
    listview->header()->setPalette(palette());
    listview->header()->setFont(font());

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
