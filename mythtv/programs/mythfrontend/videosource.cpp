#include "videosource.h"
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev.h>

QString VSSetting::whereClause(void) {
    return QString("sourceid = %1").arg(parent.getSourceID());
}

QString VSSetting::setClause(void) {
    return QString("sourceid = %1, %2 = '%3'")
        .arg(parent.getSourceID())
        .arg(getColumn())
        .arg(getValue());
}

QString CCSetting::whereClause(void) {
    return QString("cardid = %1").arg(parent.getCardID());
}

QString CCSetting::setClause(void) {
    return QString("cardid = %1, %2 = '%3'")
        .arg(parent.getCardID())
        .arg(getColumn())
        .arg(getValue());
}

QString CISetting::whereClause(void) {
    return QString("cardinputid = %1").arg(parent.getInputID());
}

QString CISetting::setClause(void) {
    return QString("cardinputid = %1, %2 = '%3'")
        .arg(parent.getInputID())
        .arg(getColumn())
        .arg(getValue());
}

void VideoSource::fillSelections(QSqlDatabase* db,
                                 StringSelectSetting* setting) {
    QString query = QString("SELECT sourceid, name FROM videosource");
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(1).toString(),
                                  result.value(0).toString());
}

void CaptureCard::fillSelections(QSqlDatabase* db,
                                 StringSelectSetting* setting) {
    QString query = QString("SELECT cardid, videodevice FROM capturecard");
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(1).toString(),
                                  result.value(0).toString());
}

void CardInput::fillSelections(QSqlDatabase* db) {
    selector->fillSelections(db);
    sourceid->fillSelections(db);
}

int CISetting::getInputID(void) const {
    return parent.getInputID();
}

int CCSetting::getCardID(void) const {
    return parent.getCardID();
}

void VideoInputSelector::fillSelections(QSqlDatabase* db) {
    cardid->fillSelections(db);
}


void VideoInputSelector::probeInputs(const QString& videoDevice, QString cardID) {
    int videofd = open(videoDevice.ascii(), O_RDWR);

    if (videofd <= 0) {
        cout << "Couldn't open " << videoDevice << " to probe its inputs.\n";
        return;
    }

    InputName* name = new InputName(parent);
    addTrigger(cardID, name);

    struct video_capability vidcap;
    memset(&vidcap, 0, sizeof(vidcap));
    if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0) {
        perror("ioctl");
        return;
    }
    
    for (int i = 0; i < vidcap.channels; i++) {
        struct video_channel test;
        memset(&test, 0, sizeof(test));
        test.channel = i;

        if (ioctl(videofd, VIDIOCGCHAN, &test) != 0) {
            perror("ioctl");
            continue;
        }

        name->addSelection(test.name);
    }
}
