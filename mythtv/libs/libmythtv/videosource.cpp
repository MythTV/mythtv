#include "videosource.h"
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
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

void ProviderSelector::fillSelections(const QString& zipcode) {
    if (zipcode.length() < 5)
        return;

    clearSelections();

    QString command = QString("tv_grab_na --configure --postalcode %1 --list-providers")
        .arg(zipcode);
    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        perror("open");
        return;
    }

    QFile f;
    f.open(IO_ReadOnly, fp);
    for(QString line ; f.readLine(line, 1024) > 0 ; ) {
        QStringList fields = QStringList::split(":", line.stripWhiteSpace());
        addSelection(fields.last(), fields.first());
    }

    f.close();
    fclose(fp);
}

void XMLTV_na_config::save(QSqlDatabase* db) {
    (void)db;

    QString filename = QString("%1/.mythtv/%2.xmltv")
        .arg(getenv("HOME")).arg(parent.getSourceName());
    QString command = QString("tv_grab_na --config-file %1 --configure --retry-limit %2 --retry-delay %3 --postalcode %4 --provider %5 --auto-new-channels add")
        .arg(filename)
        .arg(2)
        .arg(30)
        .arg(postalcode->getValue())
        .arg(provider->getValue());

    int ret = system(command);
    if (ret != 0)
        cout << command << endl << "exited with status " << ret << endl;
}

void XMLTV_generic_config::save(QSqlDatabase* db) {
    (void)db;

    QString command;
    if (grabber == "tv_grab_de") {
        command = "tv_grab_de --configure";
    } else {
        QString filename = QString("%1/.mythtv/%2.xmltv")
            .arg(getenv("HOME")).arg(parent.getSourceName());

        command = QString("%1 --config-file %2 --configure")
            .arg(grabber).arg(filename);
    }

    int ret = system(command);
    if (ret != 0)
        cout << command << endl << "exited with status " << ret << endl;

    if (grabber == "tv_grab_uk" || grabber == "tv_grab_de" ||
        grabber == "tv_grab_sn") {
        cout << "You _MUST_ run 'mythfilldatabase --manual the first time, "
             << "instead\n";
        cout << "of just 'mythfilldatabase'.  Your grabber does not provide\n";
        cout << "channel numbers, so you have to set them manually.\n";
    }

}

void VideoSource::fillSelections(QSqlDatabase* db,
                                 StringSelectSetting* setting) {
    QString query = QString("SELECT name, sourceid FROM videosource;");
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(0).toString(),
                                  result.value(1).toString());
}

void VideoSource::loadByID(QSqlDatabase* db, int sourceid) {
    id->setValue(sourceid);
    load(db);
}

void CaptureCard::fillSelections(QSqlDatabase* db,
                                 StringSelectSetting* setting) {
    QString query = QString("SELECT videodevice, cardid FROM capturecard;");
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(0).toString(),
                                  result.value(1).toString());
}

void CaptureCard::loadByID(QSqlDatabase* db, int cardid) {
    id->setValue(cardid);
    load(db);
}

void CardInput::loadByID(QSqlDatabase* db, int inputid) {
    id->setValue(inputid);
    load(db);
}

void CardInput::loadByInput(QSqlDatabase* db, int _cardid, QString _inputname) {
    QString query = QString("SELECT cardinputid FROM cardinput WHERE cardid = %1 AND inputname = '%2';")
        .arg(_cardid)
        .arg(_inputname);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        result.next();
        loadByID(db, result.value(0).toInt());
    } else {
        load(db); // new
        cardid->setValue(QString("%1").arg(_cardid));
        inputname->setValue(_inputname);
    }
}

void CardInput::save(QSqlDatabase* db) {
    if (sourceid->getValue() == "0")
        // "None" is represented by the lack of a row
        db->exec(QString("DELETE FROM cardinput WHERE cardinputid = %1").arg(getInputID()));
    else
        VerticalConfigurationGroup::save(db);
}

int CISetting::getInputID(void) const {
    return parent.getInputID();
}

int CCSetting::getCardID(void) const {
    return parent.getCardID();
}

int CaptureCardEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit(getValue().toInt());

    return QDialog::Rejected;
}

void CaptureCardEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection("(New capture card)", "0");
    QSqlQuery query = db->exec("SELECT videodevice, cardid FROM capturecard;");
    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            addSelection(query.value(0).toString(), query.value(1).toString());
}

int VideoSourceEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit(getValue().toInt());

    return QDialog::Rejected;
}

void VideoSourceEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection("(New video source)", "0");
    QSqlQuery query = db->exec("SELECT name, sourceid FROM videosource;");
    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            addSelection(query.value(0).toString(), query.value(1).toString());

}

int CardInputEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit(getValue().toInt());

    return QDialog::Rejected;
}

void CardInputEditor::load(QSqlDatabase* db) {
    clearSelections();
    QSqlQuery capturecards = db->exec("SELECT cardid,videodevice FROM capturecard;");
    if (capturecards.isActive() && capturecards.numRowsAffected() > 0)
        while (capturecards.next()) {
            int cardid = capturecards.value(0).toInt();
            QString videodevice(capturecards.value(1).toString());

            int videofd = open(videodevice.ascii(), O_RDWR);
            if (videofd < 0) {
                cout << "Couldn't open " << videodevice << " to probe its inputs.\n";
                continue;
            }

            struct video_capability vidcap;
            memset(&vidcap, 0, sizeof(vidcap));
            if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0) {
                perror("ioctl");
                close(videofd);
                continue;
            }

            for (int i = 0; i < vidcap.channels; i++) {
                struct video_channel test;
                memset(&test, 0, sizeof(test));
                test.channel = i;

                if (ioctl(videofd, VIDIOCGCHAN, &test) != 0) {
                    perror("ioctl");
                    continue;
                }

                QString input(test.name);
                CardInput* cardinput = new CardInput(m_context);
                cardinput->loadByInput(db, cardid, input);
                cardinputs.push_back(cardinput);
                QString index = QString("%1").arg(cardinputs.size()-1);

                QString label = QString("%1 (%2) -> %3")
                    .arg(videodevice)
                    .arg(input)
                    .arg(cardinput->getSourceName());
                addSelection(label, index);
            }

            close(videofd);
        }
}

CardInputEditor::~CardInputEditor() {
    while (cardinputs.size() > 0) {
        delete cardinputs.back();
        cardinputs.pop_back();
    }
}
