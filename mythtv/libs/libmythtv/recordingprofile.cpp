#include "recordingprofile.h"
#include "libmyth/mythcontext.h"
#include <qsqldatabase.h>
#include <qheader.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>

QString RecordingProfileParam::whereClause(void) {
  return QString("id = %1").arg(parentProfile.getProfileNum());
}

class CodecParam: public SimpleDBStorage,
                  public RecordingProfileSetting {
protected:
    CodecParam(const RecordingProfile& parentProfile,
                    QString name):
        SimpleDBStorage("codecparams", "value"),
        RecordingProfileSetting(parentProfile) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);
};

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

class AudioCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    AudioCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "audiocodec") {
        setLabel("Codec");
    };
};

class MP3Quality: public CodecParam, public SliderSetting {
public:
    MP3Quality(const RecordingProfile& parent):
        CodecParam(parent, "mp3quality"),
        SliderSetting(1,9,1) {
        setLabel("MP3 Quality");
        setValue(7);
	setHelpText("The higher the slider number, the lower the quality "
                    "of the audio.  Better quality audio (lower numbers) "
                    "requires more CPU.");
    };
};

class SampleRate: public CodecParam, public ComboBoxSetting {
public:
    SampleRate(const RecordingProfile& parent):
        CodecParam(parent, "samplerate") {
        setLabel("Sampling rate");
        addSelection("32000");
        addSelection("44100");
        addSelection("48000");
	setHelpText("Sets the audio sampling rate for your DSP. "
                    "Ensure that you choose a sampling rate appropriate "
                    "for your device.  btaudio may only allow 32000.");
    };
};

class AudioCompressionSettings: public VerticalConfigurationGroup,
                                public TriggeredConfigurationGroup {
public:
    AudioCompressionSettings(const RecordingProfile& parent)
           : VerticalConfigurationGroup(false)
    {
        setLabel("Audio Compression");
        setUseLabel(false);

        AudioCodecName* codecName = new AudioCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup();
        params->setLabel("MP3");
        params->addChild(new MP3Quality(parent));
        addTarget("MP3", params);
        codecName->addSelection("MP3");

        addTarget("Uncompressed", new VerticalConfigurationGroup());
        codecName->addSelection("Uncompressed");
    };
};

class VideoCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    VideoCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "videocodec") {
        setLabel("Codec");
    };
};

class RTjpegQuality: public CodecParam, public SliderSetting {
public:
    RTjpegQuality(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegquality"),
        SliderSetting(1,255,1) {
        setLabel("RTjpeg Quality");
        setValue(170);
	setHelpText("Higher is better quality.");
    };
};

class RTjpegLumaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegLumaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpeglumafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel("Luma filter");
        setValue(0);
	setHelpText("Lower is better.");
    };
};

class RTjpegChromaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegChromaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegchromafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel("Chroma filter");
        setValue(0);
	setHelpText("Lower is better.");
    };
};

class MPEG4bitrate: public CodecParam, public SliderSetting {
public:
    MPEG4bitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4bitrate"),
        SliderSetting(100,8000,100) {

        setLabel("Bitrate");
        setValue(2200);
	setHelpText("Bitrate in kilobits/second.  2200Kbps is "
                    "approximately 1 Gigabyte per hour.");
    };
};

class MPEG4ScaleBitrate: public CodecParam, public CheckBoxSetting {
public:
    MPEG4ScaleBitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4scalebitrate") {
        setLabel("Scale bitrate for frame size");
        setValue(true);
	setHelpText("If set, the MPEG4 bitrate will be used for "
                    "640x480.  If other resolutions are used, the "
                    "bitrate will be scaled appropriately.");
    };
};

class MPEG4MinQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MinQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4minquality"),
        SliderSetting(1,31,1) {

        setLabel("Minimum quality");
        setValue(15);
	setHelpText("Modifying the default may have severe consequences.");
    };
};

class MPEG4MaxQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MaxQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4maxquality"),
        SliderSetting(1,31,1) {

        setLabel("Maximum quality");
        setValue(2);
	setHelpText("Modifying the default may have severe consequences.");
    };
};

class MPEG4QualDiff: public CodecParam, public SliderSetting {
public:
    MPEG4QualDiff(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4qualdiff"),
        SliderSetting(1,31,1) {

        setLabel("Max quality difference between frames");
        setValue(3);
        setHelpText("Modifying the default may have severe consequences.");
    };
};

class MPEG4OptionVHQ: public CodecParam, public CheckBoxSetting {
public:
    MPEG4OptionVHQ(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4optionvhq") {
        setLabel("Enable high-quality encoding");
        setValue(false);
        setHelpText("If set, the MPEG4 encoder will use 'high-quality' "
                    "encoding options.  This requires much more "
                    "processing, but can result in better video.");
    };
};

class MPEG4Option4MV: public CodecParam, public CheckBoxSetting {
public:
    MPEG4Option4MV(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4option4mv") {
        setLabel("Enable 4MV encoding");
        setValue(false);
        setHelpText("If set, the MPEG4 encoder will use '4MV' "
                    "motion-vector encoding.  This requires "
                    "much more processing, but can result in better "
                    "video. It is highly recommended that the HQ option is "
                    "enabled if 4MV is enabled.");
    };
};

class HardwareMJPEGQuality: public CodecParam, public SliderSetting {
public:
    HardwareMJPEGQuality(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpegquality"),
        SliderSetting(0, 100, 1) {
        setLabel("Quality");
        setValue(100);
    };
};

class HardwareMJPEGHDecimation: public CodecParam, public ComboBoxSetting {
public:
    HardwareMJPEGHDecimation(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpeghdecimation") {
        setLabel("Horizontal Decimation");
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class HardwareMJPEGVDecimation: public CodecParam, public ComboBoxSetting {
public:
    HardwareMJPEGVDecimation(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpegvdecimation") {
        setLabel("Vertical Decimation");
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class VideoCompressionSettings: public VerticalConfigurationGroup,
                                public TriggeredConfigurationGroup {
public:
    VideoCompressionSettings(const RecordingProfile& parent, QString profName)
             : VerticalConfigurationGroup(false),
               TriggeredConfigurationGroup(false)
    {
        QString labelName;
        if (profName.isNull())
            labelName = "Video Compression";
        else
            labelName = profName + "->Video Compression";
        setName(labelName);
        setUseLabel(false);

        codecName = new VideoCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup();
        params->setLabel("RTjpeg Parameters");
        params->addChild(new RTjpegQuality(parent));
        params->addChild(new RTjpegLumaFilter(parent));
        params->addChild(new RTjpegChromaFilter(parent));

        addTarget("RTjpeg", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("MPEG-4 Parameters");
        params->addChild(new MPEG4bitrate(parent));
        params->addChild(new MPEG4MaxQuality(parent));
        params->addChild(new MPEG4MinQuality(parent));
        params->addChild(new MPEG4QualDiff(parent));
        params->addChild(new MPEG4ScaleBitrate(parent));
        params->addChild(new MPEG4OptionVHQ(parent));
        params->addChild(new MPEG4Option4MV(parent));

        addTarget("MPEG-4", params);

        params = new VerticalConfigurationGroup();
        params->setLabel("Hardware MJPEG Parameters");
        params->addChild(new HardwareMJPEGQuality(parent));
        params->addChild(new HardwareMJPEGHDecimation(parent));
        params->addChild(new HardwareMJPEGVDecimation(parent));
 
        addTarget("Hardware MJPEG", params);
    }

    void selectCodecs(QString groupType)
    {
        if(!groupType.isNull())
        {
//            if(groupType == "MPEG")
//               codecName->addSelection("Hardware MPEG2");
            if(groupType == "MJPEG")
                codecName->addSelection("Hardware MJPEG");
            else
            {
                // V4L, TRANSCODE (and any undefined types)
                codecName->addSelection("RTjpeg");
                codecName->addSelection("MPEG-4");
            }
        }
        else
        {
            codecName->addSelection("RTjpeg");
            codecName->addSelection("MPEG-4");
            codecName->addSelection("Hardware MJPEG");
        }
    }

private:
    VideoCodecName* codecName;
};

class ImageSize: public HorizontalConfigurationGroup {
public:
    class Width: public SpinBoxSetting, public CodecParam {
    public:
        Width(const RecordingProfile& parent, int maxwidth=704):
            SpinBoxSetting(160,maxwidth,16),
            CodecParam(parent, "width") {
            setLabel("Width");
            setValue(480);
        };
    };

    class Height: public SpinBoxSetting, public CodecParam {
    public:
        Height(const RecordingProfile& parent, int maxheight=576):
            SpinBoxSetting(160,maxheight,16),
            CodecParam(parent, "height") {
            setLabel("Height");
            setValue(480);
        };
    };

    ImageSize(const RecordingProfile& parent, QString tvFormat,
              QString profName) 
         : HorizontalConfigurationGroup(false) 
    {
        QString labelName;
        if (profName.isNull())
            labelName = "Image size";
        else
            labelName = profName + "->Image size";
        setLabel(labelName);

        setUseLabel(false);

        QString fullsize, halfsize;
        int maxwidth, maxheight;
        if (tvFormat.lower() == "ntsc" || tvFormat.lower() == "ntsc-jp") {
            maxwidth = 720;
            maxheight = 480;

        } else {
            maxwidth = 768;
            maxheight = 576;
        }

        addChild(new Width(parent,maxwidth));
        addChild(new Height(parent,maxheight));
    };
};

RecordingProfile::RecordingProfile(QString profName)
{
    // This must be first because it is needed to load/save the other settings
    addChild(id = new ID());

    ConfigurationGroup* profile = new VerticalConfigurationGroup(false);
    QString labelName;
    if (profName.isNull())
        labelName = QString("Profile");
    else
        labelName = profName + "->Profile";
    profile->setLabel(labelName);
    profile->addChild(name = new Name(*this));
    addChild(profile);

    QString tvFormat = gContext->GetSetting("TVFormat");
    addChild(new ImageSize(*this, tvFormat, profName));
    vc = new VideoCompressionSettings(*this, profName);
    addChild(vc);

    ConfigurationGroup* audioquality = new VerticalConfigurationGroup(false);
    if (profName.isNull())
        labelName = QString("Audio Quality");
    else
        labelName = profName + "->Audio Quality";
    audioquality->setLabel(labelName);
    audioquality->addChild(new SampleRate(*this));
    audioquality->addChild(new AudioCompressionSettings(*this));
    addChild(audioquality);
};

void RecordingProfile::loadByID(QSqlDatabase* db, int profileId) {
    id->setValue(profileId);
    load(db);
}

bool RecordingProfile::loadByCard(QSqlDatabase* db, QString name, int cardid) {
    QString hostname = gContext->GetHostName();
    int id = 0;
    QString query = QString("SELECT recordingprofiles.id, "
                   "profilegroups.hostname, profilegroups.is_default FROM "
                   "recordingprofiles,profilegroups,capturecard WHERE "
                   "profilegroups.id = recordingprofiles.profilegroup "
                   "AND capturecard.cardid = %1 "
                   "AND capturecard.cardtype = profilegroups.cardtype "
                   "AND recordingprofiles.name = '%2';").arg(cardid).arg(name);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        while (result.next())
        {
            if(result.value(1).toString() == hostname)
            {
                id = result.value(0).toInt();
                break;
            }
            else if(result.value(2).toInt() == 1)
                id = result.value(0).toInt();
        }
    }
    if (id)
    {
        loadByID(db, id);
        return true;
    }
    else
        return false;
}

bool RecordingProfile::loadByGroup(QSqlDatabase* db, QString name, QString group) {
    QString query = QString("SELECT recordingprofiles.id FROM "
                 "recordingprofiles, profilegroups WHERE "
                 "recordingprofiles.profilegroup = profilegroups.id AND "
                 "profilegroups.name = '%1' AND recordingprofiles.name = '%2';")
                 .arg(group).arg(name);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        result.next();
        loadByID(db, result.value(0).toInt());
        return true;
    } else {
        return false;
    }
}

void RecordingProfile::setCodecTypes(QSqlDatabase *db)
{
    vc->selectCodecs(groupType(db));
}

void RecordingProfileEditor::open(int id) {
    QString profName = RecordingProfile::getName(db, id);
    if (profName.isNull())
        profName = labelName;
    else
        profName = labelName + "->" + profName;
    RecordingProfile* profile = new RecordingProfile(profName);

    profile->loadByID(db,id);
    profile->setCodecTypes(db);

    if (profile->exec(db) == QDialog::Accepted)
        profile->save(db);
    delete profile;
};

RecordingProfileEditor::RecordingProfileEditor(QSqlDatabase* _db,
                          int id, QString profName)
{
    labelName = profName;
    db = _db;
    group = id;
    if(! labelName.isNull())
        this->setLabel(labelName);
}

void RecordingProfileEditor::load(QSqlDatabase* db) {
    clearSelections();
    //addSelection("(Create new profile)", "0");
    RecordingProfile::fillSelections(db, this, group);
}

int RecordingProfileEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void RecordingProfile::fillSelections(QSqlDatabase* db, SelectSetting* setting,
                                      int group) {
    if (group == 0)
    {
       for(int i = 0; availProfiles[i][0] != 0; i++)
           setting->addSelection(availProfiles[i],QString::number(i));
    }
    else
    {
        QString query = QString("SELECT name, id FROM recordingprofiles "
                                "WHERE profilegroup = %1;").arg(group);
       QSqlQuery result = db->exec(query);
        if (result.isActive() && result.numRowsAffected() > 0)
            while (result.next())
                setting->addSelection(result.value(0).toString(),
                                      result.value(1).toString());
    }
}

QString RecordingProfile::groupType(QSqlDatabase *db)
{
    if (db != NULL)
    {
        QString query = QString("SELECT profilegroups.cardtype FROM "
                            "profilegroups, recordingprofiles WHERE "
                            "profilegroups.id = recordingprofiles.profilegroup "
                            "AND recordingprofiles.id = %1;")
                            .arg(getProfileNum());
        QSqlQuery result = db->exec(query);
        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            return (result.value(0).toString());
        }
    }
    return NULL;
}

QString RecordingProfile::getName(QSqlDatabase *db, int id)
{
    if (db != NULL)
    {
        QString query = QString("SELECT name FROM recordingprofiles WHERE "
                                "id = %1;").arg(id);
        QSqlQuery result = db->exec(query);
        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            return (result.value(0).toString());
        }
    }
    return NULL;
}

