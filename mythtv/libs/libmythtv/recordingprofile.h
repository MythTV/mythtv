#ifndef RECORDINGPROFILE_H
#define RECORDINGPROFILE_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"
#include <qlistview.h>
#include <qdialog.h>
#include <vector>

class RecordingProfile;

// Any setting associated with a recording profile
class RecordingProfileSetting: virtual public Setting {
protected:
    RecordingProfileSetting(const RecordingProfile& _parent):
        parentProfile(_parent) {};
    const RecordingProfile& parentProfile;
};

// A parameter associated with the profile itself
class RecordingProfileParam: public SimpleDBStorage,
                             public RecordingProfileSetting {
protected:
    RecordingProfileParam(const RecordingProfile& parentProfile, QString name):
        SimpleDBStorage("recordingprofiles", name),
        RecordingProfileSetting(parentProfile) {
        setName(name);
    };

    virtual QString whereClause(void);
};

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

class AudioCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    AudioCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "audiocodec") {
    };
};

// Manages selection of a codec and its associated settings
class CompressionSettings: public VerticalConfigurationGroup {
    Q_OBJECT
protected:
    CompressionSettings(StringSelectSetting* _codecName) {
        codecName = _codecName;
        codecSettingStack = new StackedConfigurationGroup();

        addChild(codecName);
        addChild(codecSettingStack);

        // Switch the stack when the codec changes
        connect(codecName, SIGNAL(valueChanged(const QString&)),
                this, SLOT(codecChanged(const QString&)));
    };

    void addCodec(QString codec, Configurable* settings) {
        codecSettingStack->addChild(settings);
        codecSettingMap[codec] = settings;
        codecName->addSelection(codec);
    };

protected slots:
    void codecChanged(const QString& str) {
        codecSettingStack->raise(codecSettingMap[str]);
    };

private:
    StackedConfigurationGroup* codecSettingStack;
    StringSelectSetting* codecName;

    // For raising codec settings to the top
    typedef map<QString,Configurable*> CodecSettingMap;
    CodecSettingMap codecSettingMap;
};

class MP3Quality: public CodecParam, public SliderSetting {
public:
    MP3Quality(const RecordingProfile& parent):
        CodecParam(parent, "mp3quality"),
        SliderSetting(1,9,1) {
        setLabel("MP3 Quality");
        setValue(7);
    };
};


class SampleRate: public CodecParam, public SpinBoxSetting {
public:
    SampleRate(const RecordingProfile& parent):
        CodecParam(parent, "samplerate"),
        SpinBoxSetting(32000,48000,100) {
        setLabel("Sampling rate");
        setValue(32000);
    };
};

class AudioCompressionSettings: public CompressionSettings {
    Q_OBJECT
public:
    AudioCompressionSettings(const RecordingProfile& parent):
        CompressionSettings(new AudioCodecName(parent)) {

        setLabel("Audio Compression");

        ConfigurationGroup* params = new VerticalConfigurationGroup();
        params->setLabel("MP3");
        params->addChild(new MP3Quality(parent));
        addCodec("MP3", params);

        addCodec("Uncompressed", new VerticalConfigurationGroup());
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
    };
};

class RTjpegLumaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegLumaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpeglumafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel("Luma filter");
        setValue(0);
    };
};

class RTjpegChromaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegChromaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegchromafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel("Chroma filter");
        setValue(0);
    };
};

class MPEG4bitrate: public CodecParam, public SliderSetting {
public:
    MPEG4bitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4bitrate"),
        SliderSetting(100,8000,100) {

        setLabel("Quality");
        setValue(2200);
    };
};

class MPEG4ScaleBitrate: public CodecParam, public CheckBoxSetting {
public:
    MPEG4ScaleBitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4scalebitrate") {
        setLabel("Scale bitrate for frame size");
        setValue(true);
    };
};

class MPEG4MinQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MinQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4minquality"),
        SliderSetting(1,31,1) {

        setLabel("Min quality");
        setValue(15);
    };
};

class MPEG4MaxQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MaxQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4maxquality"),
        SliderSetting(1,31,1) {

        setLabel("Max quality");
        setValue(2);
    };
};

class MPEG4QualDiff: public CodecParam, public SliderSetting {
public:
    MPEG4QualDiff(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4qualdiff"),
        SliderSetting(1,31,1) {

        setLabel("Max quality difference");
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

class HardwareMJPEGDecimation: public CodecParam, public SliderSetting {
public:
    HardwareMJPEGDecimation(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpegdecimation"),
        SliderSetting(1, 2, 1) {
        setLabel("Decimation");
        setValue(2);
    };
};

class VideoCompressionSettings: public CompressionSettings {
    Q_OBJECT
public:
    VideoCompressionSettings(const RecordingProfile& parent):
        CompressionSettings(new VideoCodecName(parent)) {

        setName("Video Compression");

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

        addCodec("RTjpeg", allParams);

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

        allParams->addChild(params);

        addCodec("MPEG-4", allParams);

        allParams = new VerticalConfigurationGroup();
        allParams->setLabel("Hardware MJPEG");

        params = new VerticalConfigurationGroup();
        params->addChild(new HardwareMJPEGQuality(parent));
        params->addChild(new HardwareMJPEGDecimation(parent));

        allParams->addChild(params);

        addCodec("Hardware MJPEG", allParams);
    };
};

class ImageSize: public VerticalConfigurationGroup {
public:
    class Width: public SpinBoxSetting, public CodecParam {
    public:
        Width(const RecordingProfile& parent, int maxwidth=704):
            SpinBoxSetting(120,maxwidth,4),
            CodecParam(parent, "width") {
            setLabel("Width");
            setValue(maxwidth);
        };
    };

    class Height: public SpinBoxSetting, public CodecParam {
    public:
        Height(const RecordingProfile& parent, int maxheight=576):
            SpinBoxSetting(120,maxheight,4),
            CodecParam(parent, "height") {
            setLabel("Height");
            setValue(maxheight);
        };
    };

    ImageSize(const RecordingProfile& parent, QString tvFormat) {
        setLabel("Image size");

        QString fullsize, halfsize;
        int maxwidth, maxheight;
        if (tvFormat.lower() == "ntsc" || tvFormat.lower() == "ntsc-jp") {
            maxwidth = 640;
            maxheight = 480;
            
        } else {
            maxwidth = 704;
            maxheight = 576;
        }

        addChild(new Width(parent,maxwidth));
        addChild(new Height(parent,maxheight));
    };
};

class RecordingProfile: public ConfigurationWizard {
protected:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("recordingprofiles", "id") {
            setVisible(false);
        };

        // Should never be called because this setting is not visible
        virtual QWidget* configWidget(QWidget* parent = NULL,
                                      const char* widgetName = NULL) {
            (void)parent; (void)widgetName;
            return NULL;
        };
    };

    class Name: public ComboBoxSetting, public RecordingProfileParam {
    public:
        Name(const RecordingProfile& parent):
            ComboBoxSetting(true),
            RecordingProfileParam(parent, "name") {

            setLabel("Profile name");
            addSelection("Custom");
            addSelection("Movies");
        };
    };
public:
    RecordingProfile() {
        // This must be first because it is needed to load/save the other settings
        addChild(id = new ID());

        ConfigurationGroup* profile = new VerticalConfigurationGroup();
        profile->setLabel("Profile");
        profile->addChild(name = new Name(*this));
        addChild(profile);

        ConfigurationGroup* videoquality = new VerticalConfigurationGroup();
        videoquality->setLabel("Video Quality");

        videoquality->addChild(new ImageSize(*this,"ntsc")); // xxx
        videoquality->addChild(new VideoCompressionSettings(*this));
        addChild(videoquality);

        ConfigurationGroup* audioquality = new VerticalConfigurationGroup();
        audioquality->setLabel("Audio Quality");
        audioquality->addChild(new SampleRate(*this));
        audioquality->addChild(new AudioCompressionSettings(*this));
        addChild(audioquality);
    };

    virtual void loadByID(QSqlDatabase* db, int id);

    int getProfileNum(void) const {
        return id->intValue();
    };

    QString getName(void) const { return name->getValue(); };
    const ImageSize& getImageSize(void) const { return *imageSize; };
    
private:
    ID* id;
    Name* name;
    ImageSize* imageSize;
};

class RecordingProfileBrowser: public QDialog {
    Q_OBJECT
public:
    RecordingProfileBrowser(MythContext* context, QWidget* parent, QSqlDatabase* db);

signals:
    void selected(int id);

protected slots:
    void select(QListViewItem* item) {
        emit selected(idMap[item]);
    };

protected:
    QListView* listview;
    map<QListViewItem*,int> idMap;
    QSqlDatabase* db;
};

class RecordingProfileEditor: public RecordingProfileBrowser {
    Q_OBJECT
public:
    RecordingProfileEditor(MythContext* context, QWidget* parent, QSqlDatabase* db):
        RecordingProfileBrowser(context, parent, db) {

            connect(this, SIGNAL(selected(int)),
                    this, SLOT(open(int)));
    };

protected slots:
    void open(int id) {
        RecordingProfile* profile = new RecordingProfile();

        if (id != 0)
            profile->loadByID(db,id);

        if (profile->dialogWidget(this)->exec() == QDialog::Accepted)
            profile->save(db);
        delete profile;
    };
};

#endif
