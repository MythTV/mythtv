#include "recordingprofile.h"
#include "cardutil.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/mythwizard.h"
#include <qsqldatabase.h>
#include <qheader.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>

#include "managedlist.h"

QString RecordingProfileParam::whereClause(MSqlBindings& bindings) {
    QString idTag(":WHEREID");
    QString query("id = " + idTag);

    bindings.insert(idTag, parentProfile.getProfileNum());

    return query;
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

    virtual QString setClause(MSqlBindings& bindings);
    virtual QString whereClause(MSqlBindings& bindings);
};

QString CodecParam::setClause(MSqlBindings& bindings) {
    QString profileTag(":SETPROFILE");
    QString nameTag(":SETNAME");
    QString valueTag(":SETVALUE");

    QString query("profile = " + profileTag + ", name = " + nameTag
            + ", value = " + valueTag);

    bindings.insert(profileTag, parentProfile.getProfileNum());
    bindings.insert(nameTag, getName());
    bindings.insert(valueTag, getValue());

    return query;
}

QString CodecParam::whereClause(MSqlBindings& bindings) {
    QString profileTag(":WHEREPROFILE");
    QString nameTag(":WHERENAME");

    QString query("profile = " + profileTag + " AND name = " + nameTag);

    bindings.insert(profileTag, parentProfile.getProfileNum());
    bindings.insert(nameTag, getName());

    return query;
}

class AudioCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    AudioCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "audiocodec") {
        setLabel(QObject::tr("Codec"));
    };
};

class MP3Quality: public CodecParam, public SliderSetting {
public:
    MP3Quality(const RecordingProfile& parent):
        CodecParam(parent, "mp3quality"),
        SliderSetting(1,9,1) {
        setLabel(QObject::tr("MP3 Quality"));
        setValue(7);
        setHelpText(QObject::tr("The higher the slider number, the lower the "
                    "quality of the audio.  Better quality audio (lower "
                    "numbers) requires more CPU."));
    };
};

class BTTVVolume: public CodecParam, public SliderSetting {
public:
    BTTVVolume(const RecordingProfile& parent):
       CodecParam(parent, "volume"),
       SliderSetting(0,100,1) {
       setLabel(QObject::tr("Volume (%)"));
       setValue(90);
       setHelpText(QObject::tr("Recording volume of the capture card"));
    };
};

class SampleRate: public CodecParam, public ComboBoxSetting {
public:
    SampleRate(const RecordingProfile& parent, bool analog = true):
        CodecParam(parent, "samplerate") {
        setLabel(QObject::tr("Sampling rate"));
        if (analog)
        {
            addSelection("32000");
            addSelection("44100");
            addSelection("48000");
        }
        else
        {
            addSelection("48000");
            //addSelection("44100");
            //addSelection("32000");
        }
        setHelpText(QObject::tr("Sets the audio sampling rate for your DSP. "
                    "Ensure that you choose a sampling rate appropriate "
                    "for your device.  btaudio may only allow 32000."));
    };
};

class MPEG2audType: public CodecParam, public ComboBoxSetting {
public:
   MPEG2audType(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audtype") {
        setLabel(QObject::tr("Type"));
        setHelpText(QObject::tr("Sets the audio type"));
    };
};

class MPEG2audBitrateL1: public CodecParam, public ComboBoxSetting {
public:
   MPEG2audBitrateL1(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audbitratel1") {
        setLabel(QObject::tr("Bitrate"));
        
        addSelection("32 kbps", "32");
        addSelection("64 kbps", "64");
        addSelection("96 kbps", "96");
        addSelection("128 kbps", "128");
        addSelection("160 kbps", "160");
        addSelection("192 kbps", "192");
        addSelection("224 kbps", "224");
        addSelection("256 kbps", "256");
        addSelection("288 kbps", "288");
        addSelection("320 kbps", "320");
        addSelection("352 kbps", "352");
        addSelection("384 kbps", "384");
        addSelection("416 kbps", "416");
        addSelection("448 kbps", "448");
        setValue(13);
        setHelpText(QObject::tr("Sets the audio bitrate"));
    };
};

class MPEG2audBitrateL2: public CodecParam, public ComboBoxSetting {
public:
   MPEG2audBitrateL2(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audbitratel2") {
        setLabel(QObject::tr("Bitrate"));
        
        addSelection("32 kbps", "32");
        addSelection("48 kbps", "48");
        addSelection("56 kbps", "56");
        addSelection("64 kbps", "64");
        addSelection("80 kbps", "80");
        addSelection("96 kbps", "96");
        addSelection("112 kbps", "112");
        addSelection("128 kbps", "128");
        addSelection("160 kbps", "160");
        addSelection("192 kbps", "192");
        addSelection("224 kbps", "224");
        addSelection("256 kbps", "256");
        addSelection("320 kbps", "320");
        addSelection("384 kbps", "384");
        setValue(13);
        setHelpText(QObject::tr("Sets the audio bitrate"));
    };
};

class MPEG2audVolume: public CodecParam, public SliderSetting {
public:
    MPEG2audVolume(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audvolume"),
        SliderSetting(0,100,1) {

        setLabel(QObject::tr("Volume (%)"));
        setValue(90);
        setHelpText(QObject::tr("Volume of the recording "));
    };
};

class MPEG2AudioBitrateSettings: public HorizontalConfigurationGroup,
                                 public TriggeredConfigurationGroup
{
  public:
    MPEG2AudioBitrateSettings(const RecordingProfile& parent) :
        ConfigurationGroup(false, true, true, true),
        HorizontalConfigurationGroup(false, true, true, true)
    {
        setLabel(QObject::tr("Bitrate Settings"));
        MPEG2audType* audType = new MPEG2audType(parent);
        addChild(audType);
        setTrigger(audType);

        ConfigurationGroup *audbr =
            new VerticalConfigurationGroup(false, true, true, true);
        audbr->addChild(new MPEG2audBitrateL1(parent));
        audbr->setLabel("Layer I");
        addTarget("Layer I", audbr);
        audType->addSelection("Layer I");

        audbr = new VerticalConfigurationGroup(false, true, true, true);
        audbr->addChild(new MPEG2audBitrateL2(parent));
        audbr->setLabel("Layer II");
        addTarget("Layer II", audbr);
        audType->addSelection("Layer II");
        audType->setValue(1);
    };
};

class MPEG2Language : public CodecParam, public ComboBoxSetting
{
  public:
    MPEG2Language(const RecordingProfile &parent)
        : CodecParam(parent, "mpeg2language")
    {
        setLabel(QObject::tr("SAP/Bilingual"));

        addSelection(QObject::tr("Main Language"), "0");
        addSelection(QObject::tr("SAP Language"),  "1");
        addSelection(QObject::tr("Dual"),          "2");

        setValue(0);
        setHelpText(QObject::tr(
                        "Chooses the language(s) to record when "
                        "two languages are broadcast. Only Layer II "
                        "supports the recording of two languages (Dual). ") +
                    // Delete this part of message once 0.1.10 is old...
                    QObject::tr(
                        "Version 0.1.10+ of ivtv driver is required for "
                        "this setting to have any effect."));
    };
};

class AudioCompressionSettings: public VerticalConfigurationGroup,
                                public TriggeredConfigurationGroup {
public:
    AudioCompressionSettings(const RecordingProfile& parent, QString profName) :
        ConfigurationGroup(false, true, false, false),
        VerticalConfigurationGroup(false, true, false, false)
    {
        QString labelName;
        if (profName.isNull())
            labelName = QString(QObject::tr("Audio Quality"));
        else
            labelName = profName + "->" + QObject::tr("Audio Quality");
        setName(labelName);

        codecName = new AudioCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup(false);
        params->setLabel("MP3");
        params->addChild(new SampleRate(parent));
        params->addChild(new MP3Quality(parent));
        params->addChild(new BTTVVolume(parent));
        addTarget("MP3", params);

        params = new VerticalConfigurationGroup(false, false, true, true);
        params->setLabel("MPEG-2 Hardware Encoder");
        params->addChild(new SampleRate(parent, false));
        params->addChild(new MPEG2AudioBitrateSettings(parent));
        params->addChild(new MPEG2Language(parent));
        params->addChild(new MPEG2audVolume(parent));
        addTarget("MPEG-2 Hardware Encoder", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("Uncompressed");
        params->addChild(new SampleRate(parent));
        params->addChild(new BTTVVolume(parent));
        addTarget("Uncompressed", params);
    };

    void selectCodecs(QString groupType)
    {
        if (!groupType.isNull())
        {
            if (groupType == "MPEG")
               codecName->addSelection("MPEG-2 Hardware Encoder");
            else
            {
                // V4L, TRANSCODE (and any undefined types)
                codecName->addSelection("MP3");
                codecName->addSelection("Uncompressed");
            }
        }
        else
        {
            codecName->addSelection("MP3");
            codecName->addSelection("Uncompressed");
            codecName->addSelection("MPEG-2 Hardware Encoder");
        }
    }
private:
    AudioCodecName* codecName;
};

class VideoCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    VideoCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "videocodec") {
        setLabel(QObject::tr("Codec"));
    };
};

class RTjpegQuality: public CodecParam, public SliderSetting {
public:
    RTjpegQuality(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegquality"),
        SliderSetting(1,255,1) {
        setLabel(QObject::tr("RTjpeg Quality"));
        setValue(170);
        setHelpText(QObject::tr("Higher is better quality."));
    };
};

class RTjpegLumaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegLumaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpeglumafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel(QObject::tr("Luma filter"));
        setValue(0);
        setHelpText(QObject::tr("Lower is better."));
    };
};

class RTjpegChromaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegChromaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegchromafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel(QObject::tr("Chroma filter"));
        setValue(0);
        setHelpText(QObject::tr("Lower is better."));
    };
};

class MPEG4bitrate: public CodecParam, public SliderSetting {
public:
    MPEG4bitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4bitrate"),
        SliderSetting(100,8000,100) {

        setLabel(QObject::tr("Bitrate"));
        setValue(2200);
        setHelpText(QObject::tr("Bitrate in kilobits/second.  2200Kbps is "
                    "approximately 1 Gigabyte per hour."));
    };
};

class MPEG4ScaleBitrate: public CodecParam, public CheckBoxSetting {
public:
    MPEG4ScaleBitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4scalebitrate") {
        setLabel(QObject::tr("Scale bitrate for frame size"));
        setValue(true);
        setHelpText(QObject::tr("If set, the MPEG4 bitrate will be used for "
                    "640x480.  If other resolutions are used, the "
                    "bitrate will be scaled appropriately."));
    };
};

class MPEG4MinQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MinQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4minquality"),
        SliderSetting(1,31,1) {

        setLabel(QObject::tr("Minimum quality"));
        setValue(15);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4MaxQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MaxQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4maxquality"),
        SliderSetting(0,31,1) {

        setLabel(QObject::tr("Maximum quality"));
        setValue(2);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4QualDiff: public CodecParam, public SliderSetting {
public:
    MPEG4QualDiff(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4qualdiff"),
        SliderSetting(1,31,1) {

        setLabel(QObject::tr("Max quality difference between frames"));
        setValue(3);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4OptionIDCT: public CodecParam, public CheckBoxSetting {
public:
    MPEG4OptionIDCT(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4optionidct") {
        setLabel(QObject::tr("Enable interlaced DCT encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "interlaced DCT encoding. You may want this when encoding "
                    "interlaced video, however, this is experimental and may "
                    "cause damaged video."));
    };
};

class MPEG4OptionIME: public CodecParam, public CheckBoxSetting {
public:
    MPEG4OptionIME(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4optionime") {
        setLabel(QObject::tr("Enable interlaced motion estimation"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "interlaced motion estimation. You may want this when "
                    "encoding interlaced video, however, this is experimental "
                    "and may cause damaged video."));
    };
};

class MPEG4OptionVHQ: public CodecParam, public CheckBoxSetting {
public:
    MPEG4OptionVHQ(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4optionvhq") {
        setLabel(QObject::tr("Enable high-quality encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "'high-quality' encoding options.  This requires much "
                    "more processing, but can result in better video."));
    };
};

class MPEG4Option4MV: public CodecParam, public CheckBoxSetting {
public:
    MPEG4Option4MV(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4option4mv") {
        setLabel(QObject::tr("Enable 4MV encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use '4MV' "
                    "motion-vector encoding.  This requires "
                    "much more processing, but can result in better "
                    "video. It is highly recommended that the HQ option is "
                    "enabled if 4MV is enabled."));
    };
};

class EncodingThreadCount: public CodecParam, public SliderSetting {
public:
    EncodingThreadCount(const RecordingProfile& parent):
        CodecParam(parent, "encodingthreadcount"),
        SliderSetting(1,8,1) {

        setLabel(QObject::tr("Number of threads"));
        setValue(1);
        setHelpText(
            QObject::tr("Threads to use for software encoding.") + " " +
            QObject::tr("Set to a value less than or equal to the "
                        "number of processors on the backend that "
                        "will be doing the encoding."));
    };
};

class MPEG2bitrate: public CodecParam, public SliderSetting {
public:
    MPEG2bitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2bitrate"),
        SliderSetting(1000,16000,100) {

        setLabel(QObject::tr("Bitrate"));
        setValue(4500);
        setHelpText(QObject::tr("Bitrate in kilobits/second.  2200Kbps is "
                    "approximately 1 Gigabyte per hour."));
    };
};

class MPEG2maxBitrate: public CodecParam, public SliderSetting {
public:
    MPEG2maxBitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2maxbitrate"),
        SliderSetting(1000,16000,100) {

        setLabel(QObject::tr("Max. Bitrate"));
        setValue(6000);
        setHelpText(QObject::tr("Maximum Bitrate in kilobits/second.  "
                    "2200Kbps is approximately 1 Gigabyte per hour."));
    };
};

class MPEG2streamType: public CodecParam, public ComboBoxSetting {
public:
    MPEG2streamType(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2streamtype") {
        setLabel(QObject::tr("Stream Type"));
        
        addSelection("MPEG-2 PS");
        addSelection("MPEG-2 TS");
        addSelection("MPEG-1 VCD");
        addSelection("PES AV");
        addSelection("PES V");
        addSelection("PES A");
        addSelection("DVD");
        addSelection("DVD-Special 1");
        addSelection("DVD-Special 2");
        setValue(0);
        setHelpText(QObject::tr("Sets the type of stream generated by "
                    "your PVR."));
    };
};

class MPEG2aspectRatio: public CodecParam, public ComboBoxSetting {
public:
    MPEG2aspectRatio(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2aspectratio") {
        setLabel(QObject::tr("Aspect Ratio"));
        
        addSelection(QObject::tr("Square"), "Square");
        addSelection("4:3");
        addSelection("16:9");
        addSelection("2.21:1");
        setValue(1);
        setHelpText(QObject::tr("Sets the aspect ratio of stream generated "
                    "by your PVR."));
    };
};

class HardwareMJPEGQuality: public CodecParam, public SliderSetting {
public:
    HardwareMJPEGQuality(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpegquality"),
        SliderSetting(0, 100, 1) {
        setLabel(QObject::tr("Quality"));
        setValue(100);
    };
};

class HardwareMJPEGHDecimation: public CodecParam, public ComboBoxSetting {
public:
    HardwareMJPEGHDecimation(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpeghdecimation") {
        setLabel(QObject::tr("Horizontal Decimation"));
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
        setLabel(QObject::tr("Vertical Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class VideoCompressionSettings: public VerticalConfigurationGroup,
                                public TriggeredConfigurationGroup {
public:
    VideoCompressionSettings(const RecordingProfile& parent, QString profName) :
        ConfigurationGroup(false, true, false, false),
        VerticalConfigurationGroup(false, true, false, false),
        TriggeredConfigurationGroup(false)
    {
        QString labelName;
        if (profName.isNull())
            labelName = QObject::tr("Video Compression");
        else
            labelName = profName + "->" + QObject::tr("Video Compression");
        setName(labelName);

        codecName = new VideoCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup();
        params->setLabel(QObject::tr("RTjpeg Parameters"));
        params->addChild(new RTjpegQuality(parent));
        params->addChild(new RTjpegLumaFilter(parent));
        params->addChild(new RTjpegChromaFilter(parent));

        addTarget("RTjpeg", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-4 Parameters"));
        params->addChild(new MPEG4bitrate(parent));
        params->addChild(new MPEG4MaxQuality(parent));
        params->addChild(new MPEG4MinQuality(parent));
        params->addChild(new MPEG4QualDiff(parent));
        params->addChild(new MPEG4ScaleBitrate(parent));

        HorizontalConfigurationGroup *hq;
        hq = new HorizontalConfigurationGroup(false, false);
        hq->addChild(new MPEG4OptionVHQ(parent));
        hq->addChild(new MPEG4Option4MV(parent));
        params->addChild(hq);

        HorizontalConfigurationGroup *inter;
        inter = new HorizontalConfigurationGroup(false, false);
        inter->addChild(new MPEG4OptionIDCT(parent));
        inter->addChild(new MPEG4OptionIME(parent));
        params->addChild(inter);
#ifdef USING_FFMPEG_THREADS
        params->addChild(new EncodingThreadCount(parent));
#endif
        addTarget("MPEG-4", params);

        // We'll eventually want to add MPEG2 software encoding params here
        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-2 Parameters"));
        //params->addChild(new MPEG4bitrate(parent));
        //params->addChild(new MPEG4MaxQuality(parent));
        //params->addChild(new MPEG4MinQuality(parent));
        //params->addChild(new MPEG4QualDiff(parent));
        //params->addChild(new MPEG4ScaleBitrate(parent));
        //params->addChild(new MPEG4OptionVHQ(parent));
        //params->addChild(new MPEG4Option4MV(parent));
#ifdef USING_FFMPEG_THREADS
        params->addChild(new EncodingThreadCount(parent));
#endif
        addTarget("MPEG-2", params);

        params = new VerticalConfigurationGroup();
        params->setLabel(QObject::tr("Hardware MJPEG Parameters"));
        params->addChild(new HardwareMJPEGQuality(parent));
        params->addChild(new HardwareMJPEGHDecimation(parent));
        params->addChild(new HardwareMJPEGVDecimation(parent));
 
        addTarget("Hardware MJPEG", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-2 Hardware Encoder"));
        params->addChild(new MPEG2streamType(parent));
        params->addChild(new MPEG2aspectRatio(parent));
        params->addChild(new MPEG2bitrate(parent));
        params->addChild(new MPEG2maxBitrate(parent));

        addTarget("MPEG-2 Hardware Encoder", params);
    }

    void selectCodecs(QString groupType)
    {
        if (!groupType.isNull())
        {
            if (groupType == "MPEG")
               codecName->addSelection("MPEG-2 Hardware Encoder");
            else if (groupType == "MJPEG")
                codecName->addSelection("Hardware MJPEG");
            else if (groupType == "GO7007")
                codecName->addSelection("MPEG-4");
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
            codecName->addSelection("MPEG-2 Hardware Encoder");
        }
    }

private:
    VideoCodecName* codecName;
};

class AutoTranscode: public CodecParam, public CheckBoxSetting {
public:
    AutoTranscode(const RecordingProfile& parent):
        CodecParam(parent, "autotranscode") {
        setLabel(QObject::tr("Enable auto-transcode after recording"));
        setValue(false);
        setHelpText(QObject::tr("Automatically transcode when a recording is "
                                "made using this profile and the recording's "
                                "schedule is configurd to allow transcoding."));
    };
};

class TranscodeResize: public CodecParam, public CheckBoxSetting {
public:
    TranscodeResize(const RecordingProfile& parent):
        CodecParam(parent, "transcoderesize") {
        setLabel(QObject::tr("Resize Video while transcoding"));
        setValue(false);
        setHelpText(QObject::tr("Allows the transcoder to "
                                "resize the video during transcoding."));
    };
};

class TranscodeLossless: public CodecParam, public CheckBoxSetting {
public:
    TranscodeLossless(const RecordingProfile& parent):
        CodecParam(parent, "transcodelossless") {
        setLabel(QObject::tr("Lossless transcoding"));
        setValue(false);
        setHelpText(QObject::tr("Only reencode where absolutely needed "
                                "(normally only around cutpoints).  Otherwise "
                                "keep audio and video formats identical to "
                                "the source.  This should result in the "
                                "highest quality, but won't save as much "
                                "space."));
    };
};

class RecordingType : public CodecParam, public ComboBoxSetting
{
  public:
    RecordingType(const RecordingProfile& parent) :
        CodecParam(parent, "recordingtype")
    {
        setLabel(QObject::tr("Recording Type"));

        QString msg = QObject::tr(
            "This option allows you to filter out unwanted streams. "
            "'Normal' will record all relevant streams including "
            "interactive television data. 'TV Only' will record only "
            "audio, video and subtitle streams. ");
        setHelpText(msg);

        addSelection(QObject::tr("Normal"),     "all");
        addSelection(QObject::tr("TV Only"),    "tv");
        addSelection(QObject::tr("Audio Only"), "audio");
        setValue(0);
    };
};


class TranscodeFilters: public CodecParam, public LineEditSetting {
public:
    TranscodeFilters(const RecordingProfile& parent):
        CodecParam(parent, "transcodefilters"),
        LineEditSetting() {
        setLabel(QObject::tr("Custom Filters"));
        setHelpText(QObject::tr("Filters used when transcoding with this "
                                "profile. This value must be blank to perform "
                                "lossless transcoding.  Format: "
                                "[[<filter>=<options>,]...]"
                                ));
    };
};

class ImageSize: public VerticalConfigurationGroup {
public:
    class Width: public SpinBoxSetting, public CodecParam {
    public:
        Width(const RecordingProfile& parent, int maxwidth=704,
              bool transcoding = false):
            SpinBoxSetting(transcoding ? 0 : 160,
                           maxwidth, 16, false,
                           transcoding ? QObject::tr("Auto") : ""),
            CodecParam(parent, "width") {
            setLabel(QObject::tr("Width"));
            setValue(480);
            if (transcoding)
                setHelpText(QObject::tr("If the width is set to 'Auto', "
                            "the width will be calculated based on the height "
                            "and the recording's physical aspect ratio."));
        };
    };

    class Height: public SpinBoxSetting, public CodecParam {
    public:
        Height(const RecordingProfile& parent, int maxheight=576,
               bool transcoding = false):
            SpinBoxSetting(transcoding ? 0 : 160,
                           maxheight, 16, false,
                           transcoding ? QObject::tr("Auto") : ""),
            CodecParam(parent, "height") {
            setLabel(QObject::tr("Height"));
            setValue(480);
            if (transcoding)
                setHelpText(QObject::tr("If the height is set to 'Auto', "
                            "the height will be calculated based on the width "
                            "and the recording's physical aspect ratio."));
        };
    };

    ImageSize(const RecordingProfile& parent, QString tvFormat,
              QString profName) :
        ConfigurationGroup(false, true, false, false),
        VerticalConfigurationGroup(false, true, false, false)
    {
        ConfigurationGroup* imgSize = new HorizontalConfigurationGroup(false);
        QString labelName;
        if (profName.isNull())
            labelName = QObject::tr("Image size");
        else
            labelName = profName + "->" + QObject::tr("Image size");
        setLabel(labelName);

        QString fullsize, halfsize;
        int maxwidth, maxheight;
        bool transcoding = false;
        if (profName.left(11) == "Transcoders") {
            maxwidth = 1920;
            maxheight = 1088;
            transcoding = true;
        } else if ((tvFormat.lower() == "ntsc") ||
                   (tvFormat.lower() == "ntsc-jp")) {
            maxwidth = 720;
            maxheight = 480;
        } else if (tvFormat.lower() == "atsc") {
            maxwidth = 1920;
            maxheight = 1088;
        } else {
            maxwidth = 768;
            maxheight = 576;
        }

        imgSize->addChild(new Width(parent, maxwidth, transcoding));
        imgSize->addChild(new Height(parent, maxheight, transcoding));
        addChild(imgSize);
    };
};

RecordingProfile::RecordingProfile(QString profName)
    : id(new ID()),        name(new Name(*this)),
      imageSize(NULL),     videoSettings(NULL),
      audioSettings(NULL), profileName(profName),
      isEncoder(true)
{
    // This must be first because it is needed to load/save the other settings
    addChild(id);

    ConfigurationGroup* profile = new VerticalConfigurationGroup(false);
    QString labelName;
    if (profName.isNull())
        labelName = QString(QObject::tr("Profile"));
    else
        labelName = profName + "->" + QObject::tr("Profile");
    profile->setLabel(labelName);
    profile->addChild(name);

    tr_filters = NULL;
    tr_lossless = NULL;
    tr_resize = NULL;

    if (profName != NULL)
    {
        if (profName.left(11) == "Transcoders")
        {
            tr_filters = new TranscodeFilters(*this);
            tr_lossless = new TranscodeLossless(*this);
            tr_resize = new TranscodeResize(*this);
            profile->addChild(tr_filters);
            profile->addChild(tr_lossless);
            profile->addChild(tr_resize);
        }
        else
            profile->addChild(new AutoTranscode(*this));
    }
    else
    {
        tr_filters = new TranscodeFilters(*this);
        tr_lossless = new TranscodeLossless(*this);
        tr_resize = new TranscodeResize(*this);
        profile->addChild(tr_filters);
        profile->addChild(tr_lossless);
        profile->addChild(tr_resize);
        profile->addChild(new AutoTranscode(*this));
    }

    addChild(profile);
};

void RecordingProfile::ResizeTranscode(bool resize)
{
    MythWizard *wizard = (MythWizard *)dialog;
    if (!wizard)
        return;
    //page '1' is the Image Size page
    QWidget *size_page = wizard->page(1);
    wizard->setAppropriate(size_page, resize);
}

void RecordingProfile::SetLosslessTranscode(bool lossless)
{
    MythWizard *wizard = (MythWizard *)dialog;
    if (!wizard)
        return;

    bool show_size = (lossless) ? false : tr_resize->boolValue();
    wizard->setAppropriate(wizard->page(1), show_size);
    wizard->setAppropriate(wizard->page(2), ! lossless);
    wizard->setAppropriate(wizard->page(3), ! lossless);
    tr_resize->setEnabled(! lossless);
    wizard->setNextEnabled(wizard->page(0), ! lossless);
    wizard->setFinishEnabled(wizard->page(0), lossless);
    
    if (tr_filters)
        tr_filters->setEnabled(!lossless);
}

void RecordingProfile::loadByID(int profileId) 
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT cardtype "
        "FROM profilegroups, recordingprofiles "
        "WHERE profilegroups.id     = recordingprofiles.profilegroup AND "
        "      recordingprofiles.id = :PROFILEID");
    result.bindValue(":PROFILEID", profileId);

    QString type = "";
    if (!result.exec() || !result.isActive())
        MythContext::DBError("RecordingProfile::loadByID -- cardtype", result);
    else if (result.next())
    {
        type = result.value(0).toString();
        isEncoder = CardUtil::IsEncoder(type);
    }

    if (isEncoder)
    {
        QString tvFormat = gContext->GetSetting("TVFormat");
        addChild(new ImageSize(*this, tvFormat, profileName));

        videoSettings = new VideoCompressionSettings(*this, profileName);
        addChild(videoSettings);

        audioSettings = new AudioCompressionSettings(*this, profileName);
        addChild(audioSettings);

        if (profileName && profileName.left(11) == "Transcoders")
        {
            connect(tr_resize, SIGNAL(valueChanged   (bool)),
                    this,      SLOT(  ResizeTranscode(bool)));
            connect(tr_lossless, SIGNAL(valueChanged        (bool)),
                    this,      SLOT(  SetLosslessTranscode(bool)));
            connect(tr_filters, SIGNAL(valueChanged(const QString &)),
                    this,      SLOT(FiltersChanged(const QString &)));
        }
    }
    else if (type.upper() == "DVB")
    {
        addChild(new RecordingType(*this));
    }

    id->setValue(profileId);
    load();
}

void RecordingProfile::FiltersChanged(const QString &val)
{
    if (!tr_filters || !tr_lossless)
      return;
   
    // If there are filters, we can not do lossless transcoding 
    if (val.stripWhiteSpace().length() > 0) {
       tr_lossless->setValue(false);
       tr_lossless->setEnabled(false);
    } else {
       tr_lossless->setEnabled(true);
    }
}

bool RecordingProfile::loadByType(QString name, QString cardtype) 
{
    QString hostname = gContext->GetHostName();
    int recid = 0;

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT recordingprofiles.id, profilegroups.hostname, "
        "       profilegroups.is_default "
        "FROM recordingprofiles, profilegroups, capturecard "
        "WHERE profilegroups.id       = recordingprofiles.profilegroup AND "
        "      profilegroups.cardtype = :CARDTYPE                      AND "
        "      recordingprofiles.name = :NAME");
    result.bindValue(":CARDTYPE", cardtype);
    result.bindValue(":NAME", name);

    if (!result.exec() || !result.isActive())
    {
        MythContext::DBError("RecordingProfile::loadByType()", result);
        return false;
    }

    while (result.next())
    {
        if (result.value(1).toString() == hostname)
        {
            recid = result.value(0).toInt();
            break;
        }
        else if (result.value(2).toInt() == 1)
            recid = result.value(0).toInt();
    }
    if (recid)
    {
        loadByID(recid);
        return true;
    }
    return false;
}

bool RecordingProfile::loadByGroup(QString name, QString group) 
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT recordingprofiles.id "
        "FROM recordingprofiles, profilegroups "
        "WHERE recordingprofiles.profilegroup = profilegroups.id AND "
        "      profilegroups.name             = :GROUPNAME       AND "
        "      recordingprofiles.name         = :NAME");
    result.bindValue(":GROUPNAME", group);
    result.bindValue(":NAME", name);

    if (result.exec() && result.isActive() && result.next())
    {
        loadByID(result.value(0).toInt());
        return true;
    }
    return false;
}

void RecordingProfile::setCodecTypes()
{
    if (videoSettings)
        videoSettings->selectCodecs(groupType());
    if (audioSettings)
        audioSettings->selectCodecs(groupType());
}

int RecordingProfile::exec()
{
    MythDialog* dialog = dialogWidget(gContext->GetMainWindow());

    dialog->Show();
    if (tr_lossless)
        SetLosslessTranscode(tr_lossless->boolValue());
    if (tr_resize)
        ResizeTranscode(tr_resize->boolValue());
    // Filters should be set last because it might disable lossless
    if (tr_filters)
        FiltersChanged(tr_filters->getValue());
    
    int ret = dialog->exec();

    delete dialog;

    return ret;
}

void RecordingProfileEditor::open(int id) 
{
    QString profName = RecordingProfile::getName(id);
    if (profName.isNull())
        profName = labelName;
    else
        profName = labelName + "->" + profName;
    RecordingProfile* profile = new RecordingProfile(profName);

    profile->loadByID(id);
    profile->setCodecTypes();

    if (profile->exec() == QDialog::Accepted)
        profile->save();
    delete profile;
};

RecordingProfileEditor::RecordingProfileEditor(int id, QString profName)
{
    labelName = profName;
    group = id;
    if (! labelName.isNull())
        this->setLabel(labelName);
}

void RecordingProfileEditor::load() {
    clearSelections();
    //addSelection("(Create new profile)", "0");
    RecordingProfile::fillSelections(this, group);
}

int RecordingProfileEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void RecordingProfile::fillSelections(SelectSetting *setting, int group,
                                      bool foldautodetect)
{
    if (!group)
    {
       for (uint i = 0; !availProfiles[i].isEmpty(); i++)
           setting->addSelection(availProfiles[i], availProfiles[i]);
       return;
    }

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT name, id "
        "FROM recordingprofiles "
        "WHERE profilegroup = :GROUP "
        "ORDER BY id");
    result.bindValue(":GROUP", group);

    if (!result.exec() || !result.isActive())
    {
        MythContext::DBError("RecordingProfile::fillSelections 1", result);
        return;
    }
    else if (!result.size())
        return;

    if (group == RecordingProfile::TranscoderGroup && foldautodetect)
    {
        QString id = QString::number(RecordingProfile::TranscoderAutodetect);
        setting->addSelection(QObject::tr("Autodetect"), id);
    }

    while (result.next())
    {
        QString name = result.value(0).toString();
        QString id   = result.value(1).toString();

        if (group == RecordingProfile::TranscoderGroup)
        {
            if (name == "RTjpeg/MPEG4" || name == "MPEG2")
            {
                if (!foldautodetect)
                {
                    setting->addSelection(
                        QObject::tr("Autodetect from %1").arg(name), id);
                }
            }
            else
            {
                setting->addSelection(name, id);
            }
            continue;
        }

        setting->addSelection(name, id);
    }
}

void RecordingProfile::fillSelections(SelectManagedListItem *setting,
                                      int group)
{
    if (!group)
    {
        for (uint i = 0; !availProfiles[i].isEmpty(); i++)
        {
            QString lbl = QObject::tr("Record using the \"%1\" profile")
                .arg(availProfiles[i]);
            setting->addSelection(lbl, availProfiles[i], false);
        }
        return;
    }

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT name, id "
        "FROM recordingprofiles "
        "WHERE profilegroup = :GROUP "
        "ORDER BY id");
    result.bindValue(":GROUP", group);

    if (!result.exec() || !result.isActive())
    {
        MythContext::DBError("RecordingProfile::fillSelections 2", result);
        return;
    }
    else if (!result.size())
        return;

    if (group == RecordingProfile::TranscoderGroup)
    {
        QString id = QString::number(RecordingProfile::TranscoderAutodetect);
        setting->addSelection(QObject::tr("Transcode using Autodetect"), id);
    }

    while (result.next())
    {
        QString name = result.value(0).toString();
        QString id   = result.value(1).toString();

        if (group == RecordingProfile::TranscoderGroup)
        {
            /* RTjpeg/MPEG4 and MPEG2 are used by "Autodetect". */
            if (name != "RTjpeg/MPEG4" && name != "MPEG2")
            {
                QString lbl = QObject::tr("Transcode using \"%1\"").arg(name);
                setting->addSelection(lbl, id, false);
            }
            continue;
        }

        QString lbl = QObject::tr("Record using the \"%1\" profile").arg(name);
        setting->addSelection(lbl, result.value(1).toString(), false);
    }
}

QString RecordingProfile::groupType(void) const
{
    MSqlQuery result(MSqlQuery::InitCon());
    QString querystr = QString("SELECT profilegroups.cardtype FROM "
                        "profilegroups, recordingprofiles WHERE "
                        "profilegroups.id = recordingprofiles.profilegroup "
                        "AND recordingprofiles.id = %1;")
                        .arg(getProfileNum());
    result.prepare(querystr);

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        result.next();
        return (result.value(0).toString());
    }

    return NULL;
}

QString RecordingProfile::getName(int id)
{
    MSqlQuery result(MSqlQuery::InitCon());
    QString querystr = QString("SELECT name FROM recordingprofiles WHERE "
                                "id = %1;").arg(id);
    result.prepare(querystr);

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        result.next();
        return (result.value(0).toString());
    }

    return NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
