#include "videosource.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythcontext.h"

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
#include <iostream>

#ifdef USING_DVB
#include <linux/dvb/frontend.h>
#include "dvbdev.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "videodev_myth.h"

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

void RegionSelector::fillSelections() {
    clearSelections();

    QString command = QString("tv_grab_uk --configure --list-regions");
    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        perror(command.ascii());
        return;
    }

    QFile f;
    f.open(IO_ReadOnly, fp);
    for(QString line ; f.readLine(line, 1024) > 0 ; ) {
        addSelection(line.stripWhiteSpace());
    }

    f.close();
    fclose(fp);
}

void ProviderSelector::fillSelections(const QString& location) {
    clearSelections();

    QString command = QString("%1 --configure --postalcode %2 --list-providers")
        .arg(grabber)
        .arg(location);

    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        perror(command.ascii());
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
        .arg(QDir::homeDirPath()).arg(parent.getSourceName());
    QString command = QString("tv_grab_na --config-file '%1' --configure --retry-limit %2 --retry-delay %3 --postalcode %4 --provider %5 --auto-new-channels add")
        .arg(filename)
        .arg(2)
        .arg(30)
        .arg(postalcode->getValue().replace(QRegExp(" "), ""))
        .arg(provider->getValue());

    int ret = system(command);

    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));
    }
}

void XMLTV_uk_config::save(QSqlDatabase* db) {
    (void)db;

    QString filename = QString("%1/.mythtv/%2.xmltv")
        .arg(QDir::homeDirPath()).arg(parent.getSourceName());
    QString command = QString("tv_grab_uk --config-file '%1' --configure --retry-limit %2 --retry-delay %3 --postalcode %4 --provider %5 --auto-new-channels add")
        .arg(filename)
        .arg(2)
        .arg(30)
        .arg(region->getValue())
        .arg(provider->getValue());

    int ret = system(command);
    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));
    }
}

void XMLTV_generic_config::save(QSqlDatabase* db) {
    (void)db;

    QString command;
    if (grabber == "tv_grab_de") {
        command = "tv_grab_de --configure";
    } else {
        QString filename = QString("%1/.mythtv/%2.xmltv")
            .arg(QDir::homeDirPath()).arg(parent.getSourceName());

        command = QString("%1 --config-file '%2' --configure")
            .arg(grabber).arg(filename);
    }

    int ret = system(command);
    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));
    }

    if (grabber == "tv_grab_de" || grabber == "tv_grab_sn" || 
        grabber == "tv_grab_fi" || grabber == "tv_grab_es" ||
        grabber == "tv_grab_nl") 
    {
        cerr << "You _MUST_ run 'mythfilldatabase --manual the first time, "
             << "instead\n";
        cerr << "of just 'mythfilldatabase'.  Your grabber does not provide\n";
        cerr << "channel numbers, so you have to set them manually.\n";
    }

}

void VideoSource::fillSelections(QSqlDatabase* db,
                                 SelectSetting* setting) {
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
                                 SelectSetting* setting) {
    QString query = QString("SELECT cardtype, videodevice, cardid "
                            "FROM capturecard WHERE hostname = \"%1\";")
                            .arg(gContext->GetHostName());
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection("[ " + result.value(0).toString() + " : " +
                                  result.value(1).toString() + " ]",
                                  result.value(2).toString());
}

void CaptureCard::loadByID(QSqlDatabase* db, int cardid) {
    id->setValue(cardid);
    load(db);
}

CardType::CardType(const CaptureCard& parent)
        : CCSetting(parent, "cardtype") 
{
    setLabel("Card type");
    setHelpText("Change the cardtype to the appropriate type for the capture "
                "card you are configuring.");
    fillSelections(this);
}

void CardType::fillSelections(SelectSetting* setting)
{
    setting->addSelection("Standard V4L capture card", "V4L");
    setting->addSelection("MJPEG capture card (Matrox G200, DC10)", "MJPEG");
    setting->addSelection("MPEG-2 Encoder card (PVR-250, PVR-350)", "MPEG");
    setting->addSelection("pcHDTV ATSC capture card", "HDTV");
    setting->addSelection("Digital Video Broadcast card (DVB)", "DVB");
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
        cardid->setValue(QString::number(_cardid));
        inputname->setValue(_inputname);
    }
}

void CardInput::save(QSqlDatabase* db) {
    if (sourceid->getValue() == "0")
        // "None" is represented by the lack of a row
        db->exec(QString("DELETE FROM cardinput WHERE cardinputid = %1;").arg(getInputID()));
    else
        ConfigurationWizard::save(db);
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
    CaptureCard::fillSelections(db, this);
}

int VideoSourceEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit(getValue().toInt());

    return QDialog::Rejected;
}

void VideoSourceEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection("(New video source)", "0");
    VideoSource::fillSelections(db, this);
}

int CardInputEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit(getValue().toInt());

    return QDialog::Rejected;
}



void CardInputEditor::load(QSqlDatabase* db) {
    clearSelections();

    // We do this manually because we want custom labels.  If
    // SelectSetting provided a facility to edit the labels, we
    // could use CaptureCard::fillSelections

    QString thequery = QString("SELECT cardid, videodevice, cardtype "
                               "FROM capturecard WHERE hostname = \"%1\";")
                              .arg(gContext->GetHostName());

    QSqlQuery capturecards = db->exec(thequery);
    if (capturecards.isActive() && capturecards.numRowsAffected() > 0)
        while (capturecards.next()) {
            int cardid = capturecards.value(0).toInt();
            QString videodevice("[ " + capturecards.value(2).toString() + 
                                " : " + capturecards.value(1).toString() + 
                                " ]");

            QStringList inputs;
            if (capturecards.value(2).toString() != "DVB")
                inputs = VideoDevice::probeInputs(videodevice);
            else
                inputs = QStringList("DVBInput");

            for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i) {
                CardInput* cardinput = new CardInput();
                cardinput->loadByInput(db, cardid, *i);
                cardinputs.push_back(cardinput);
                QString index = QString::number(cardinputs.size()-1);

                QString label = QString("%1 (%2) -> %3")
                    .arg(videodevice)
                    .arg(*i)
                    .arg(cardinput->getSourceName());
                addSelection(label, index);
            }
        }
}

CardInputEditor::~CardInputEditor() {
    while (cardinputs.size() > 0) {
        delete cardinputs.back();
        cardinputs.pop_back();
    }
}

QStringList VideoDevice::probeInputs(QString device) {
    QStringList ret;

    int videofd = open(device.ascii(), O_RDWR);
    if (videofd < 0) {
        cerr << "Couldn't open " << device << " to probe its inputs.\n";
        return ret;
    }

    bool usingv4l2 = false;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
         usingv4l2 = false;
    else {
        if (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
            usingv4l2 = true;
    }

    if (usingv4l2) {
        struct v4l2_input vin;
        memset(&vin, 0, sizeof(vin));
        vin.index = 0;

        while (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0) {
            QString input((char *)vin.name);

            ret += input;

            vin.index++;
        }
    }
    else
    {
        struct video_capability vidcap;
        memset(&vidcap, 0, sizeof(vidcap));
        if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0) {
            perror("VIDIOCGCAP");
            vidcap.channels = 0;
        }

        for (int i = 0; i < vidcap.channels; i++) {
            struct video_channel test;
            memset(&test, 0, sizeof(test));
            test.channel = i;

            if (ioctl(videofd, VIDIOCGCHAN, &test) != 0) {
                perror("ioctl(VIDIOCGCHAN)");
                continue;
            }

            ret += test.name;
        }
    }

    if (ret.size() == 0)
        ret += "ERROR, No inputs found";

    close(videofd);
    return ret;
}

void DVBConfigurationGroup::probeCard(const QString& cardNumber)
{
#ifdef USING_DVB
    int fd = open(dvbdevice(DVB_DEV_FRONTEND, cardNumber.toInt()), O_RDWR);
    if (fd == -1)
    {
        cardname->setValue(QString("Could not open card #%1!")
                            .arg(cardNumber));
        cardtype->setValue(strerror(errno));
    }
    else
    {
        struct dvb_frontend_info info;

        if (ioctl(fd, FE_GET_INFO, &info) < 0)
        {
            cardname->setValue(QString("Could not get card info for card #%1!")
                                      .arg(cardNumber));
            cardtype->setValue(strerror(errno));
        }
        else
        {
            switch(info.type)
            {
                case FE_QPSK:   cardtype->setValue("DVB-S"); break;
                case FE_QAM:    cardtype->setValue("DVB-C"); break;
                case FE_OFDM:   cardtype->setValue("DVB-T"); break;
            }

            cardname->setValue(info.name);
        }
        close(fd);
    }
#else
    (void)cardNumber;
    cardtype->setValue(QString("Recompile with DVB-Support!"));
#endif
}

void TunerCardInput::fillSelections(const QString& device) {
    clearSelections();

    if (device == QString::null || device == "")
        return;

    QStringList inputs = VideoDevice::probeInputs(device);
    for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);
}

