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

VideoCompressionSettings::VideoCompressionSettings(const RecordingProfile& parent) {

    setName("Video Compression");

    VideoCodecName* codecName = new VideoCodecName(parent);
    addChild(codecName);
    setTrigger(codecName);

    ConfigurationGroup* allParams = new VerticalConfigurationGroup();
    allParams->setLabel("RTjpeg");

    ConfigurationGroup* params = new VerticalConfigurationGroup();
    params->setLabel("Parameters");
    params->addChild(new RTjpegQuality(parent));

    allParams->addChild(params);

    params = new VerticalConfigurationGroup();
    params->setLabel("Advanced parameters");
    params->addChild(new RTjpegLumaFilter(parent));
    params->addChild(new RTjpegChromaFilter(parent));

    allParams->addChild(params);

    addTarget("RTjpeg", allParams);
    codecName->addSelection("RTjpeg");

    allParams = new VerticalConfigurationGroup();
    allParams->setLabel("MPEG-4");

    params = new VerticalConfigurationGroup();
    params->setLabel("Parameters");
    params->addChild(new MPEG4bitrate(parent));

    allParams->addChild(params);

    params = new VerticalConfigurationGroup();
    params->setLabel("Advanced parameters");
    params->addChild(new MPEG4MaxQuality(parent));
    params->addChild(new MPEG4MinQuality(parent));
    params->addChild(new MPEG4QualDiff(parent));
    params->addChild(new MPEG4ScaleBitrate(parent));

    allParams->addChild(params);

    addTarget("MPEG-4", allParams);
    codecName->addSelection("MPEG-4");

    allParams = new VerticalConfigurationGroup();
    allParams->setLabel("Hardware MJPEG");

    params = new VerticalConfigurationGroup();
    params->addChild(new HardwareMJPEGQuality(parent));
    params->addChild(new HardwareMJPEGDecimation(parent));

    allParams->addChild(params);

    addTarget("Hardware MJPEG", allParams);
    codecName->addSelection("Hardware MJPEG");
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
    clearSelections();
    addSelection("(Create new profile)", "0");
    QSqlQuery query = db->exec("SELECT id, name FROM recordingprofiles");
    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            addSelection(query.value(1).toString(), query.value(0).toString());
}

int RecordingProfileEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}
