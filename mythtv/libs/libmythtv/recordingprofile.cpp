#include "recordingprofile.h"
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
        setLabel("Video Codec");
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
	setHelpText("Lower is better.");
    };
};

class MPEG4MaxQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MaxQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4maxquality"),
        SliderSetting(1,31,1) {

        setLabel("Maximum quality");
        setValue(2);
	setHelpText("Lower is better.");
    };
};

class MPEG4QualDiff: public CodecParam, public SliderSetting {
public:
    MPEG4QualDiff(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4qualdiff"),
        SliderSetting(1,31,1) {

        setLabel("Max quality difference between frames");
        setValue(3);
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
    VideoCompressionSettings(const RecordingProfile& parent)
             : VerticalConfigurationGroup(false),
               TriggeredConfigurationGroup(false)
    {
        setName("Video Compression");
        setUseLabel(false);

        VideoCodecName* codecName = new VideoCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup();
        params->setLabel("RTjpeg Parameters");
        params->addChild(new RTjpegQuality(parent));
        params->addChild(new RTjpegLumaFilter(parent));
        params->addChild(new RTjpegChromaFilter(parent));

        addTarget("RTjpeg", params);
        codecName->addSelection("RTjpeg");

        params = new VerticalConfigurationGroup();
        params->setLabel("MPEG-4 Parameters");
        params->addChild(new MPEG4bitrate(parent));
        params->addChild(new MPEG4MaxQuality(parent));
        params->addChild(new MPEG4MinQuality(parent));
        params->addChild(new MPEG4QualDiff(parent));
        params->addChild(new MPEG4ScaleBitrate(parent));

        addTarget("MPEG-4", params);
        codecName->addSelection("MPEG-4");

        params = new VerticalConfigurationGroup();
        params->setLabel("Hardware MJPEG Parameters");
        params->addChild(new HardwareMJPEGQuality(parent));
        params->addChild(new HardwareMJPEGHDecimation(parent));
        params->addChild(new HardwareMJPEGVDecimation(parent));
 
        addTarget("Hardware MJPEG", params);
        codecName->addSelection("Hardware MJPEG");
    }
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

    ImageSize(const RecordingProfile& parent, QString tvFormat) 
         : HorizontalConfigurationGroup(false) 
    {
        setLabel("Image size");
        setUseLabel(false);

        QString fullsize, halfsize;
        int maxwidth, maxheight;
        if (tvFormat.lower() == "ntsc" || tvFormat.lower() == "ntsc-jp") {
            maxwidth = 720;
            maxheight = 480;

        } else {
            maxwidth = 704;
            maxheight = 576;
        }

        addChild(new Width(parent,maxwidth));
        addChild(new Height(parent,maxheight));
    };
};

RecordingProfile::RecordingProfile()
{
    // This must be first because it is needed to load/save the other settings
    addChild(id = new ID());

    ConfigurationGroup* profile = new VerticalConfigurationGroup(false);
    profile->setLabel("Profile");
    profile->addChild(name = new Name(*this));
    addChild(profile);

    addChild(new ImageSize(*this, "ntsc"));

    addChild(new VideoCompressionSettings(*this));

    ConfigurationGroup* audioquality = new VerticalConfigurationGroup(false);
    audioquality->setLabel("Audio Quality");
    audioquality->addChild(new SampleRate(*this));
    audioquality->addChild(new AudioCompressionSettings(*this));
    addChild(audioquality);
};

void RecordingProfile::loadByID(QSqlDatabase* db, int profileId) {
    id->setValue(profileId);
    load(db);
}

void RecordingProfile::loadByName(QSqlDatabase* db, QString name) {
    QString query = QString("SELECT id FROM recordingprofiles WHERE name = '%1';").arg(name);
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
    RecordingProfile::fillSelections(db, this);
}

int RecordingProfileEditor::exec(MythContext* context, QSqlDatabase* db) {
    m_context = context;
    while (ConfigurationDialog::exec(context, db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void RecordingProfile::fillSelections(QSqlDatabase* db, SelectSetting* setting) {
    QSqlQuery result = db->exec("SELECT name, id FROM recordingprofiles;");
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(0).toString(), result.value(1).toString());
}
