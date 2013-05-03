#include <iostream>

#include <QCursor>
#include <QLayout>

#include "recordingprofile.h"
#include "cardutil.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythwizard.h"

QString RecordingProfileStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString idTag(":WHEREID");
    QString query("id = " + idTag);

    bindings.insert(idTag, m_parent.getProfileNum());

    return query;
}

class CodecParamStorage : public SimpleDBStorage
{
  protected:
    CodecParamStorage(Setting *_setting,
                      const RecordingProfile &parentProfile,
                      QString name) :
        SimpleDBStorage(_setting, "codecparams", "value"),
        m_parent(parentProfile), codecname(name)
    {
        _setting->setName(name);
    }

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const RecordingProfile &m_parent;
    QString codecname;
};

QString CodecParamStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString profileTag(":SETPROFILE");
    QString nameTag(":SETNAME");
    QString valueTag(":SETVALUE");

    QString query("profile = " + profileTag + ", name = " + nameTag
            + ", value = " + valueTag);

    bindings.insert(profileTag, m_parent.getProfileNum());
    bindings.insert(nameTag, codecname);
    bindings.insert(valueTag, user->GetDBValue());

    return query;
}

QString CodecParamStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString profileTag(":WHEREPROFILE");
    QString nameTag(":WHERENAME");

    QString query("profile = " + profileTag + " AND name = " + nameTag);

    bindings.insert(profileTag, m_parent.getProfileNum());
    bindings.insert(nameTag, codecname);

    return query;
}

class AudioCodecName: public ComboBoxSetting, public RecordingProfileStorage
{
  public:
    AudioCodecName(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        RecordingProfileStorage(this, parent, "audiocodec")
    {
        setLabel(QObject::tr("Codec"));
    }
};

class MP3Quality : public SliderSetting, public CodecParamStorage
{
  public:
    MP3Quality(const RecordingProfile &parent) :
        SliderSetting(this, 1, 9, 1),
        CodecParamStorage(this, parent, "mp3quality")
    {
        setLabel(QObject::tr("MP3 quality"));
        setValue(7);
        setHelpText(QObject::tr("The higher the slider number, the lower the "
                    "quality of the audio. Better quality audio (lower "
                    "numbers) requires more CPU."));
    };
};

class BTTVVolume : public SliderSetting, public CodecParamStorage
{
  public:
    BTTVVolume(const RecordingProfile& parent) :
        SliderSetting(this, 0, 100, 1),
        CodecParamStorage(this, parent, "volume")
    {
       setLabel(QObject::tr("Volume (%)"));
       setValue(90);
       setHelpText(QObject::tr("Recording volume of the capture card."));
    };
};

class SampleRate : public ComboBoxSetting, public CodecParamStorage
{
  public:
    SampleRate(const RecordingProfile &parent, bool analog = true) :
        ComboBoxSetting(this), CodecParamStorage(this, parent, "samplerate")
    {
        setLabel(QObject::tr("Sampling rate"));
        setHelpText(QObject::tr("Sets the audio sampling rate for your DSP. "
                    "Ensure that you choose a sampling rate appropriate "
                    "for your device.  btaudio may only allow 32000."));

        rates.push_back(32000);
        rates.push_back(44100);
        rates.push_back(48000);

        allowed_rate[48000] = true;
        for (uint i = 0; analog && (i < rates.size()); i++)
            allowed_rate[rates[i]] = true;

    };

    void Load(void)
    {
        CodecParamStorage::Load();
        QString val = getValue();

        clearSelections();
        for (uint i = 0; i < rates.size(); i++)
        {
            if (allowed_rate[rates[i]])
                addSelection(QString::number(rates[i]));
        }

        int which = getValueIndex(val);
        setValue(max(which,0));

        if (allowed_rate.size() <= 1)
            setEnabled(false);
    }

    void addSelection(const QString &label,
                      QString        value  = QString::null,
                      bool           select = false)
    {
        QString val = value.isEmpty() ? label : value;
        uint rate = val.toUInt();
        if (allowed_rate[rate])
        {
            ComboBoxSetting::addSelection(label, value, select);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("SampleRate: ") +
                QString("Attempted to add a rate %1 Hz, which is "
                        "not in the list of allowed rates.").arg(rate));
        }
    }

    vector<uint>    rates;
    QMap<uint,bool> allowed_rate;
};

class MPEG2audType : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2audType(const RecordingProfile &parent,
                 bool layer1, bool layer2, bool layer3) :
        ComboBoxSetting(this), CodecParamStorage(this, parent, "mpeg2audtype"),
        allow_layer1(layer1), allow_layer2(layer2), allow_layer3(layer3)
    {
        setLabel(QObject::tr("Type"));

        if (allow_layer1)
            addSelection("Layer I");
        if (allow_layer2)
            addSelection("Layer II");
        if (allow_layer3)
            addSelection("Layer III");

        uint allowed_cnt = 0;
        allowed_cnt += ((allow_layer1) ? 1 : 0);
        allowed_cnt += ((allow_layer2) ? 1 : 0);
        allowed_cnt += ((allow_layer3) ? 1 : 0);

        if (1 == allowed_cnt)
            setEnabled(false);

        setHelpText(QObject::tr("Sets the audio type"));
    }

    void Load(void)
    {
        CodecParamStorage::Load();
        QString val = getValue();

        if ((val == "Layer I") && !allow_layer1)
        {
            val = (allow_layer2) ? "Layer II" :
                ((allow_layer3) ? "Layer III" : val);
        }

        if ((val == "Layer II") && !allow_layer2)
        {
            val = (allow_layer3) ? "Layer III" :
                ((allow_layer1) ? "Layer I" : val);
        }

        if ((val == "Layer III") && !allow_layer3)
        {
            val = (allow_layer2) ? "Layer II" :
                ((allow_layer1) ? "Layer I" : val);
        }

        if (getValue() != val)
        {
            int which = getValueIndex(val);
            if (which >= 0)
                setValue(which);
        }
    }

  private:
    bool allow_layer1;
    bool allow_layer2;
    bool allow_layer3;
};

class MPEG2audBitrateL1 : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2audBitrateL1(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2audbitratel1")
    {
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

class MPEG2audBitrateL2 : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2audBitrateL2(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2audbitratel2")
    {
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

class MPEG2audBitrateL3 : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2audBitrateL3(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2audbitratel3")
    {
        setLabel(QObject::tr("Bitrate"));

        addSelection("32 kbps", "32");
        addSelection("40 kbps", "40");
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
        setValue(10);
        setHelpText(QObject::tr("Sets the audio bitrate"));
    };
};

class MPEG2audVolume : public SliderSetting, public CodecParamStorage
{
  public:
    MPEG2audVolume(const RecordingProfile &parent) :
        SliderSetting(this, 0, 100, 1),
        CodecParamStorage(this, parent, "mpeg2audvolume")
    {

        setLabel(QObject::tr("Volume (%)"));
        setValue(90);
        setHelpText(QObject::tr("Volume of the recording "));
    };
};

class MPEG2AudioBitrateSettings : public TriggeredConfigurationGroup
{
  public:
    MPEG2AudioBitrateSettings(const RecordingProfile &parent,
                              bool layer1, bool layer2, bool layer3,
                              uint default_layer) :
        TriggeredConfigurationGroup(false, true, true, true)
    {
        const QString layers[3] = { "Layer I", "Layer II", "Layer III", };

        SetVertical(false);
        setLabel(QObject::tr("Bitrate Settings"));

        MPEG2audType *audType = new MPEG2audType(
            parent, layer1, layer2, layer3);

        addChild(audType);
        setTrigger(audType);

        addTarget(layers[0], new MPEG2audBitrateL1(parent));
        addTarget(layers[1], new MPEG2audBitrateL2(parent));
        addTarget(layers[2], new MPEG2audBitrateL3(parent));

        uint desired_layer = max(min(3U, default_layer), 1U) - 1;
        int which = audType->getValueIndex(layers[desired_layer]);
        if (which >= 0)
            audType->setValue(which);
    };
};

class MPEG2Language : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2Language(const RecordingProfile &parent) :
        ComboBoxSetting(this), CodecParamStorage(this, parent, "mpeg2language")
    {
        setLabel(QObject::tr("SAP/Bilingual"));

        addSelection(QObject::tr("Main Language"), "0");
        addSelection(QObject::tr("SAP Language"),  "1");
        addSelection(QObject::tr("Dual"),          "2");

        setValue(0);
        setHelpText(QObject::tr(
                        "Chooses the language(s) to record when "
                        "two languages are broadcast. Only Layer II "
                        "supports the recording of two languages (Dual)."
                        "Requires ivtv 0.4.0 or later."));
    };
};

class AudioCompressionSettings : public TriggeredConfigurationGroup
{
  public:
    AudioCompressionSettings(const RecordingProfile &parent, QString profName) :
        TriggeredConfigurationGroup(false, true, false, false)
    {
        setSaveAll(false);

        QString labelName;
        if (profName.isNull())
            labelName = QObject::tr("Audio Quality");
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
        params->addChild(new MPEG2AudioBitrateSettings(
                             parent, false, true, false, 2));
        params->addChild(new MPEG2Language(parent));
        params->addChild(new MPEG2audVolume(parent));
        addTarget("MPEG-2 Hardware Encoder", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("Uncompressed");
        params->addChild(new SampleRate(parent));
        params->addChild(new BTTVVolume(parent));
        addTarget("Uncompressed", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("AC3 Hardware Encoder");
        addTarget("AC3 Hardware Encoder", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("AAC Hardware Encoder");
        addTarget("AAC Hardware Encoder", params);

    };

    void selectCodecs(QString groupType)
    {
        if (!groupType.isNull())
        {
            if (groupType == "MPEG")
               codecName->addSelection("MPEG-2 Hardware Encoder");
            else if (groupType == "HDPVR")
            {
                codecName->addSelection("AC3 Hardware Encoder");
                codecName->addSelection("AAC Hardware Encoder");
            }
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

class VideoCodecName : public ComboBoxSetting, public RecordingProfileStorage
{
  public:
    VideoCodecName(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        RecordingProfileStorage(this, parent, "videocodec")
    {
        setLabel(QObject::tr("Codec"));
    }
};

class RTjpegQuality : public SliderSetting, public CodecParamStorage
{
  public:
    RTjpegQuality(const RecordingProfile &parent) :
        SliderSetting(this, 1, 255, 1),
        CodecParamStorage(this, parent, "rtjpegquality")
    {
        setLabel(QObject::tr("RTjpeg Quality"));
        setValue(170);
        setHelpText(QObject::tr("Higher is better quality."));
    };
};

class RTjpegLumaFilter : public SpinBoxSetting, public CodecParamStorage
{
  public:
    RTjpegLumaFilter(const RecordingProfile &parent) :
        SpinBoxSetting(this, 0, 31, 1),
        CodecParamStorage(this, parent, "rtjpeglumafilter")
    {
        setLabel(QObject::tr("Luma filter"));
        setValue(0);
        setHelpText(QObject::tr("Lower is better."));
    };
};

class RTjpegChromaFilter : public SpinBoxSetting, public CodecParamStorage
{
  public:
    RTjpegChromaFilter(const RecordingProfile &parent) :
        SpinBoxSetting(this, 0, 31, 1),
        CodecParamStorage(this, parent, "rtjpegchromafilter")
    {
        setLabel(QObject::tr("Chroma filter"));
        setValue(0);
        setHelpText(QObject::tr("Lower is better."));
    };
};

class MPEG4bitrate : public SliderSetting, public CodecParamStorage
{
  public:
    MPEG4bitrate(const RecordingProfile &parent) :
        SliderSetting(this, 100, 8000, 100),
        CodecParamStorage(this, parent, "mpeg4bitrate")
    {
        setLabel(QObject::tr("Bitrate (kb/s)"));
        setValue(2200);
        setHelpText(QObject::tr("Bitrate in kilobits/second. As a guide, "
                    "2200 kb/s is approximately 1 GB/hr."));
    };
};

class ScaleBitrate : public CheckBoxSetting, public CodecParamStorage
{
  public:
    ScaleBitrate(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "scalebitrate")
    {
        setLabel(QObject::tr("Scale bitrate for frame size"));
        setValue(true);
        setHelpText(QObject::tr("If set, the bitrate specified will be used "
                    "for 640x480. If other resolutions are used, the "
                    "bitrate will be scaled appropriately."));
    };
};

class MPEG4MinQuality : public SliderSetting, public CodecParamStorage
{
  public:
    MPEG4MinQuality(const RecordingProfile &parent) :
        SliderSetting(this, 1, 31, 1),
        CodecParamStorage(this, parent, "mpeg4minquality")
    {
        setLabel(QObject::tr("Minimum quality"));
        setValue(15);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4MaxQuality : public SliderSetting, public CodecParamStorage
{
  public:
    MPEG4MaxQuality(const RecordingProfile &parent) :
        SliderSetting(this, 1, 31, 1),
        CodecParamStorage(this, parent, "mpeg4maxquality")
    {
        setLabel(QObject::tr("Maximum quality"));
        setValue(2);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4QualDiff : public SliderSetting, public CodecParamStorage
{
  public:
    MPEG4QualDiff(const RecordingProfile &parent) :
        SliderSetting(this, 1, 31, 1),
        CodecParamStorage(this, parent, "mpeg4qualdiff")
    {

        setLabel(QObject::tr("Max quality difference between frames"));
        setValue(3);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4OptionIDCT : public CheckBoxSetting, public CodecParamStorage
{
  public:
    MPEG4OptionIDCT(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg4optionidct")
    {
        setLabel(QObject::tr("Enable interlaced DCT encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "interlaced DCT encoding. You may want this when encoding "
                    "interlaced video; however, this is experimental and may "
                    "cause damaged video."));
    };
};

class MPEG4OptionIME : public CheckBoxSetting, public CodecParamStorage
{
  public:
    MPEG4OptionIME(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg4optionime")
    {
        setLabel(QObject::tr("Enable interlaced motion estimation"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "interlaced motion estimation. You may want this when "
                    "encoding interlaced video; however, this is experimental "
                    "and may cause damaged video."));
    };
};

class MPEG4OptionVHQ : public CheckBoxSetting, public CodecParamStorage
{
  public:
    MPEG4OptionVHQ(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg4optionvhq")
    {
        setLabel(QObject::tr("Enable high-quality encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "'high-quality' encoding options. This requires much "
                    "more processing, but can result in better video."));
    };
};

class MPEG4Option4MV : public CheckBoxSetting, public CodecParamStorage
{
  public:
    MPEG4Option4MV(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg4option4mv")
    {
        setLabel(QObject::tr("Enable 4MV encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use '4MV' "
                    "motion-vector encoding.  This requires "
                    "much more processing, but can result in better "
                    "video. It is highly recommended that the HQ option is "
                    "enabled if 4MV is enabled."));
    };
};

class EncodingThreadCount : public SliderSetting, public CodecParamStorage
{
  public:
    EncodingThreadCount(const RecordingProfile &parent) :
        SliderSetting(this, 1, 8, 1),
        CodecParamStorage(this, parent, "encodingthreadcount")
    {

        setLabel(QObject::tr("Number of threads"));
        setValue(1);
        setHelpText(
            QObject::tr("Threads to use for software encoding.") + " " +
            QObject::tr("Set to a value less than or equal to the "
                        "number of processors on the backend that "
                        "will be doing the encoding."));
    };
};

class AverageBitrate : public SliderSetting, public CodecParamStorage
{
  public:
    AverageBitrate(const RecordingProfile &parent,
                   QString setting = "mpeg2bitrate",
                   uint min_br = 1000, uint max_br = 16000,
                   uint default_br = 4500, uint increment = 100,
                   QString label = QString::null) :
        SliderSetting(this, min_br, max_br, increment),
        CodecParamStorage(this, parent, setting)
    {
        if (label.isEmpty())
            label = QObject::tr("Avg. Bitrate (kb/s)");
        setLabel(label);
        setValue(default_br);
        setHelpText(QObject::tr(
                        "Average bitrate in kilobits/second. As a guide, "
                        "2200 kb/s is approximately 1 GB/hour."));
    };
};

class PeakBitrate : public SliderSetting, public CodecParamStorage
{
  public:
    PeakBitrate(const RecordingProfile &parent,
                QString setting = "mpeg2maxbitrate",
                uint min_br = 1000, uint max_br = 16000,
                uint default_br = 6000, uint increment = 100,
                QString label = QString::null) :
        SliderSetting(this, min_br, max_br, increment),
        CodecParamStorage(this, parent, setting)
    {
        if (label.isEmpty())
            label = QObject::tr("Max. Bitrate (kb/s)");
        setLabel(label);
        setValue(default_br);
        setHelpText(QObject::tr("Maximum bitrate in kilobits/second. "
                    "As a guide, 2200 kb/s is approximately 1 GB/hour."));
    };
};

class MPEG2streamType : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2streamType(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2streamtype")
    {
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

class MPEG2aspectRatio : public ComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2aspectRatio(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2aspectratio")
    {
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

class HardwareMJPEGQuality : public SliderSetting, public CodecParamStorage
{
  public:
    HardwareMJPEGQuality(const RecordingProfile &parent) :
        SliderSetting(this, 0, 100, 1),
        CodecParamStorage(this, parent, "hardwaremjpegquality")
    {
        setLabel(QObject::tr("Quality"));
        setValue(100);
    };
};

class HardwareMJPEGHDecimation : public ComboBoxSetting,
                                 public CodecParamStorage
{
  public:
    HardwareMJPEGHDecimation(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "hardwaremjpeghdecimation")
    {
        setLabel(QObject::tr("Horizontal Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class HardwareMJPEGVDecimation : public ComboBoxSetting,
                                 public CodecParamStorage
{
  public:
    HardwareMJPEGVDecimation(const RecordingProfile &parent) :
        ComboBoxSetting(this),
        CodecParamStorage(this, parent, "hardwaremjpegvdecimation") {
        setLabel(QObject::tr("Vertical Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class VideoCompressionSettings : public TriggeredConfigurationGroup
{
  public:
    VideoCompressionSettings(const RecordingProfile &parent, QString profName) :
        TriggeredConfigurationGroup(false, true, false, false)
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
        params->addChild(new ScaleBitrate(parent));

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

        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-2 Parameters"));
        params->addChild(new AverageBitrate(parent));
        params->addChild(new ScaleBitrate(parent));
        //params->addChild(new MPEG4MaxQuality(parent));
        //params->addChild(new MPEG4MinQuality(parent));
        //params->addChild(new MPEG4QualDiff(parent));
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
        params->addChild(new AverageBitrate(parent));
        params->addChild(new PeakBitrate(parent));

        addTarget("MPEG-2 Hardware Encoder", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-4 AVC Hardware Encoder"));
        ConfigurationGroup *h0 = new HorizontalConfigurationGroup(
            true, false, true, true);
        h0->setLabel(QObject::tr("Low Resolution"));
        h0->addChild(new AverageBitrate(parent, "low_mpeg4avgbitrate",
                                        1000, 13500, 4500, 500));
        h0->addChild(new PeakBitrate(parent, "low_mpeg4peakbitrate",
                                     1100, 20200, 6000, 500));
        params->addChild(h0);
        ConfigurationGroup *h1 = new HorizontalConfigurationGroup(
            true, false, true, true);
        h1->setLabel(QObject::tr("Medium Resolution"));
        h1->addChild(new AverageBitrate(parent, "medium_mpeg4avgbitrate",
                                        1000, 13500, 9000, 500));
        h1->addChild(new PeakBitrate(parent, "medium_mpeg4peakbitrate",
                                     1100, 20200, 11000, 500));
        params->addChild(h1);
        ConfigurationGroup *h2 = new HorizontalConfigurationGroup(
            true, false, true, true);
        h2->setLabel(QObject::tr("High Resolution"));
        h2->addChild(new AverageBitrate(parent, "high_mpeg4avgbitrate",
                                        1000, 13500, 13500, 500));
        h2->addChild(new PeakBitrate(parent, "high_mpeg4peakbitrate",
                                     1100, 20200, 20200, 500));
        params->addChild(h2);
        addTarget("MPEG-4 AVC Hardware Encoder", params);
    }

    void selectCodecs(QString groupType)
    {
        if (!groupType.isNull())
        {
            if (groupType == "HDPVR")
               codecName->addSelection("MPEG-4 AVC Hardware Encoder");
            else if (groupType == "MPEG")
               codecName->addSelection("MPEG-2 Hardware Encoder");
            else if (groupType == "MJPEG")
                codecName->addSelection("Hardware MJPEG");
            else if (groupType == "GO7007")
            {
                codecName->addSelection("MPEG-4");
                codecName->addSelection("MPEG-2");
            }
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

class AutoTranscode : public CheckBoxSetting, public CodecParamStorage
{
  public:
    AutoTranscode(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "autotranscode")
    {
        setLabel(QObject::tr("Enable auto-transcode after recording"));
        setValue(false);
        setHelpText(QObject::tr("Automatically transcode when a recording is "
                                "made using this profile and the recording's "
                                "schedule is configured to allow transcoding."));
    };
};

class TranscodeResize : public CheckBoxSetting, public CodecParamStorage
{
  public:
    TranscodeResize(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "transcoderesize")
    {
        setLabel(QObject::tr("Resize video while transcoding"));
        setValue(false);
        setHelpText(QObject::tr("Allows the transcoder to "
                                "resize the video during transcoding."));
    };
};

class TranscodeLossless : public CheckBoxSetting, public CodecParamStorage
{
  public:
    TranscodeLossless(const RecordingProfile &parent) :
        CheckBoxSetting(this),
        CodecParamStorage(this, parent, "transcodelossless")
    {
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

class RecordingType : public ComboBoxSetting, public CodecParamStorage
{
  public:
    RecordingType(const RecordingProfile &parent) :
        ComboBoxSetting(this), CodecParamStorage(this, parent, "recordingtype")
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

class RecordFullTSStream : public ComboBoxSetting, public CodecParamStorage
{
  public:
    RecordFullTSStream(const RecordingProfile &parent) :
        ComboBoxSetting(this), CodecParamStorage(this, parent, "recordmpts")
    {
        setLabel(QObject::tr("Record Full TS?"));

        QString msg = QObject::tr(
            "If set, extra files will be created for each recording with "
            "the name of the recording followed by '.ts' and a number. "
            "These extra files represent the full contents of the transport "
            "stream used to generate the recording.");
        setHelpText(msg);

        addSelection(QObject::tr("Yes"), "1");
        addSelection(QObject::tr("No"),  "0");
        setValue(1);
    };
};

class TranscodeFilters : public LineEditSetting, public CodecParamStorage
{
  public:
    TranscodeFilters(const RecordingProfile &parent) :
        LineEditSetting(this),
        CodecParamStorage(this, parent, "transcodefilters")
    {
        setLabel(QObject::tr("Custom filters"));
        setHelpText(QObject::tr("Filters used when transcoding with this "
                                "profile. This value must be blank to perform "
                                "lossless transcoding.  Format: "
                                "[[<filter>=<options>,]...]"
                                ));
    };
};

class ImageSize : public VerticalConfigurationGroup
{
  public:
    class Width : public SpinBoxSetting, public CodecParamStorage
    {
      public:
        Width(const RecordingProfile &parent,
              uint defaultwidth, uint maxwidth,
              bool transcoding = false) :
            SpinBoxSetting(this, transcoding ? 0 : 160,
                           maxwidth, 16, false,
                           transcoding ? QObject::tr("Auto") : QString()),
            CodecParamStorage(this, parent, "width")
        {
            setLabel(QObject::tr("Width"));
            setValue(defaultwidth);

            QString help = (transcoding) ?
                QObject::tr("If the width is set to 'Auto', the width "
                            "will be calculated based on the height and "
                            "the recording's physical aspect ratio.") :
                QObject::tr("Width to use for encoding. "
                            "Note: PVR-x50 cards may produce ghosting if "
                            "this is not set to 720 or 768 for NTSC and "
                            "PAL, respectively.");

            setHelpText(help);
        };
    };

    class Height: public SpinBoxSetting, public CodecParamStorage
    {
      public:
        Height(const RecordingProfile &parent,
               uint defaultheight, uint maxheight,
               bool transcoding = false):
            SpinBoxSetting(this, transcoding ? 0 : 160,
                           maxheight, 16, false,
                           transcoding ? QObject::tr("Auto") : QString()),
            CodecParamStorage(this, parent, "height")
        {
            setLabel(QObject::tr("Height"));
            setValue(defaultheight);

            QString help = (transcoding) ?
                QObject::tr("If the height is set to 'Auto', the height "
                            "will be calculated based on the width and "
                            "the recording's physical aspect ratio.") :
                QObject::tr("Height to use for encoding. "
                            "Note: PVR-x50 cards may produce ghosting if "
                            "this is not set to 480 or 576 for NTSC and "
                            "PAL, respectively.");

            setHelpText(help);
        };
    };

    ImageSize(const RecordingProfile &parent,
              QString tvFormat, QString profName) :
        VerticalConfigurationGroup(false, true, false, false)
    {
        ConfigurationGroup* imgSize = new HorizontalConfigurationGroup(false);
        QString labelName;
        if (profName.isNull())
            labelName = QObject::tr("Image size");
        else
            labelName = profName + "->" + QObject::tr("Image size");
        setLabel(labelName);

        QSize defaultsize(768, 576), maxsize(768, 576);
        bool transcoding = profName.left(11) == "Transcoders";
        bool ivtv = profName.left(20) == "IVTV MPEG-2 Encoders";

        if (transcoding)
        {
            maxsize     = QSize(1920, 1088);
            if (tvFormat.toLower() == "ntsc" || tvFormat.toLower() == "atsc")
                defaultsize = QSize(480, 480);
            else
                defaultsize = QSize(480, 576);
        }
        else if (tvFormat.toLower().left(4) == "ntsc")
        {
            maxsize     = QSize(720, 480);
            defaultsize = (ivtv) ? QSize(720, 480) : QSize(480, 480);
        }
        else if (tvFormat.toLower() == "atsc")
        {
            maxsize     = QSize(1920, 1088);
            defaultsize = QSize(1920, 1088);
        }
        else
        {
            maxsize     = QSize(768, 576);
            defaultsize = (ivtv) ? QSize(720, 576) : QSize(480, 576);
        }

        imgSize->addChild(new Width(parent, defaultsize.width(),
                                    maxsize.width(), transcoding));
        imgSize->addChild(new Height(parent, defaultsize.height(),
                                     maxsize.height(), transcoding));

        addChild(imgSize);
    };
};

typedef enum {
    RPPopup_OK = 0,
    RPPopup_CANCEL,
    RPPopup_DELETE
} RPPopupResult;

class RecordingProfilePopup
{
  public:
    static RPPopupResult showPopup(MythMainWindow *parent, QString title,
                                   QString message, QString& text)
    {
        MythPopupBox *popup = new MythPopupBox(
            parent, title.toLatin1().constData());
        popup->addLabel(message);

        MythLineEdit *textEdit = new MythLineEdit(popup, "chooseEdit");
        textEdit->setText(text);
        popup->addWidget(textEdit);

        popup->addButton(QObject::tr("OK"),     popup, SLOT(accept()));
        popup->addButton(QObject::tr("Cancel"), popup, SLOT(reject()));

        textEdit->setFocus();

        bool ok = (MythDialog::Accepted == popup->ExecPopup());
        if (ok)
            text = textEdit->text();

        popup->hide();
        popup->deleteLater();

        return (ok) ? RPPopup_OK : RPPopup_CANCEL;
    }
};

// id and name will be deleted by ConfigurationGroup's destructor
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
        labelName = QObject::tr("Profile");
    else
        labelName = profName + "->" + QObject::tr("Profile");
    profile->setLabel(labelName);
    profile->addChild(name);

    tr_filters = NULL;
    tr_lossless = NULL;
    tr_resize = NULL;

    if (!profName.isEmpty())
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
        "SELECT cardtype, profilegroups.name "
        "FROM profilegroups, recordingprofiles "
        "WHERE profilegroups.id     = recordingprofiles.profilegroup AND "
        "      recordingprofiles.id = :PROFILEID");
    result.bindValue(":PROFILEID", profileId);

    QString type;
    QString name;
    if (!result.exec())
    {
        MythDB::DBError("RecordingProfile::loadByID -- cardtype", result);
    }
    else if (result.next())
    {
        type = result.value(0).toString();
        name = result.value(1).toString();
    }

    CompleteLoad(profileId, type, name);
}

void RecordingProfile::FiltersChanged(const QString &val)
{
    if (!tr_filters || !tr_lossless)
      return;

    // If there are filters, we cannot do lossless transcoding
    if (!val.trimmed().isEmpty())
    {
       tr_lossless->setValue(false);
       tr_lossless->setEnabled(false);
    }
    else
    {
       tr_lossless->setEnabled(true);
    }
}

bool RecordingProfile::loadByType(const QString &name, const QString &cardtype)
{
    QString hostname = gCoreContext->GetHostName().toLower();
    uint profileId = 0;

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT recordingprofiles.id, profilegroups.hostname, "
        "       profilegroups.is_default "
        "FROM recordingprofiles, profilegroups "
        "WHERE profilegroups.id       = recordingprofiles.profilegroup AND "
        "      profilegroups.cardtype = :CARDTYPE                      AND "
        "      recordingprofiles.name = :NAME");
    result.bindValue(":CARDTYPE", cardtype);
    result.bindValue(":NAME", name);

    if (!result.exec())
    {
        MythDB::DBError("RecordingProfile::loadByType()", result);
        return false;
    }

    while (result.next())
    {
        if (result.value(1).toString().toLower() == hostname)
        {
            profileId = result.value(0).toUInt();
        }
        else if (result.value(2).toInt() == 1)
        {
            profileId = result.value(0).toUInt();
            break;
        }
    }

    if (profileId)
    {
        CompleteLoad(profileId, cardtype, name);
        return true;
    }

    return false;
}

bool RecordingProfile::loadByGroup(const QString &name, const QString &group)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT recordingprofiles.id, cardtype "
        "FROM recordingprofiles, profilegroups "
        "WHERE recordingprofiles.profilegroup = profilegroups.id AND "
        "      profilegroups.name             = :GROUPNAME       AND "
        "      recordingprofiles.name         = :NAME");
    result.bindValue(":GROUPNAME", group);
    result.bindValue(":NAME", name);

    if (!result.exec())
    {
        MythDB::DBError("RecordingProfile::loadByGroup()", result);
        return false;
    }

    if (result.next())
    {
        uint profileId = result.value(0).toUInt();
        QString type = result.value(1).toString();

        CompleteLoad(profileId, type, name);
        return true;
    }

    return false;
}

void RecordingProfile::CompleteLoad(int profileId, const QString &type,
                                    const QString &name)
{
    if (profileName.isEmpty())
        profileName = name;

    isEncoder = CardUtil::IsEncoder(type);

    if (isEncoder)
    {
        QString tvFormat = gCoreContext->GetSetting("TVFormat");
        if (type.toUpper() != "HDPVR")
            addChild(new ImageSize(*this, tvFormat, profileName));

        videoSettings = new VideoCompressionSettings(*this, profileName);
        addChild(videoSettings);

        audioSettings = new AudioCompressionSettings(*this, profileName);
        addChild(audioSettings);

        if (!profileName.isEmpty() && profileName.left(11) == "Transcoders")
        {
            connect(tr_resize,   SIGNAL(valueChanged   (bool)),
                    this,        SLOT(  ResizeTranscode(bool)));
            connect(tr_lossless, SIGNAL(valueChanged        (bool)),
                    this,        SLOT(  SetLosslessTranscode(bool)));
            connect(tr_filters,  SIGNAL(valueChanged(const QString&)),
                    this,        SLOT(FiltersChanged(const QString&)));
        }
    }
    else if (type.toUpper() == "DVB")
    {
        addChild(new RecordingType(*this));
    }
    else if (type.toUpper() == "ASI")
    {
        addChild(new RecordFullTSStream(*this));
    }

    id->setValue(profileId);
    Load();
}

void RecordingProfile::setCodecTypes()
{
    if (videoSettings)
        videoSettings->selectCodecs(groupType());
    if (audioSettings)
        audioSettings->selectCodecs(groupType());
}

DialogCode RecordingProfile::exec(void)
{
    MythDialog *dialog = dialogWidget(
        GetMythMainWindow(), "Recording Profile");

    dialog->Show();
    if (tr_lossless)
        SetLosslessTranscode(tr_lossless->boolValue());
    if (tr_resize)
        ResizeTranscode(tr_resize->boolValue());
    // Filters should be set last because it might disable lossless
    if (tr_filters)
        FiltersChanged(tr_filters->getValue());

    DialogCode ret = dialog->exec();

    dialog->deleteLater();

    return ret;
}

void RecordingProfileEditor::open(int id)
{
    if (id)
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
            profile->Save();
        delete profile;
    }
    else
    {
        QString profName;
        RPPopupResult result = RecordingProfilePopup::showPopup(
            GetMythMainWindow(),
            tr("Add Recording Profile"),
            tr("Enter the name of the new profile"), profName);
        if (result == RPPopup_CANCEL)
            return;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "INSERT INTO recordingprofiles "
                "(name, videocodec, audiocodec, profilegroup) "
              "VALUES "
                "(:NAME, :VIDEOCODEC, :AUDIOCODEC, :PROFILEGROUP);");
        query.bindValue(":NAME", profName);
        query.bindValue(":VIDEOCODEC", "MPEG-4");
        query.bindValue(":AUDIOCODEC", "MP3");
        query.bindValue(":PROFILEGROUP", group);
        if (!query.exec())
        {
            MythDB::DBError("RecordingProfileEditor::open", query);
        }
        else
        {
            query.prepare(
                "SELECT id "
                  "FROM recordingprofiles "
                  "WHERE name = :NAME AND profilegroup = :PROFILEGROUP;");
            query.bindValue(":NAME", profName);
            query.bindValue(":PROFILEGROUP", group);
            if (!query.exec())
            {
                MythDB::DBError("RecordingProfileEditor::open", query);
            }
            else
            {
                if (query.next())
                    open(query.value(0).toInt());
            }
        }
    }
}

RecordingProfileEditor::RecordingProfileEditor(int id, QString profName) :
    listbox(new ListBoxSetting(this)), group(id), labelName(profName)
{
    if (!labelName.isEmpty())
        listbox->setLabel(labelName);
    addChild(listbox);
}

void RecordingProfileEditor::Load(void)
{
    listbox->clearSelections();
    listbox->addSelection("(Create new profile)", "0");
    RecordingProfile::fillSelections(listbox, group);
}

DialogCode RecordingProfileEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        open(listbox->getValue().toInt());

    return kDialogCodeRejected;
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

    if (!result.exec())
    {
        MythDB::DBError("RecordingProfile::fillSelections 1", result);
        return;
    }
    else if (!result.next())
    {
        return;
    }

    if (group == RecordingProfile::TranscoderGroup && foldautodetect)
    {
        QString id = QString::number(RecordingProfile::TranscoderAutodetect);
        setting->addSelection(QObject::tr("Autodetect"), id);
    }

    do
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
    while (result.next());
}

QMap<int, QString> RecordingProfile::listProfiles(int group)
{
    QMap<int, QString> profiles;

    if (!group)
    {
        for (uint i = 0; !availProfiles[i].isEmpty(); i++)
            profiles[i] = availProfiles[i];
        return profiles;
    }

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT name, id "
        "FROM recordingprofiles "
        "WHERE profilegroup = :GROUP "
        "ORDER BY id");
    result.bindValue(":GROUP", group);

    if (!result.exec())
    {
        MythDB::DBError("RecordingProfile::fillSelections 2", result);
        return profiles;
    }
    else if (!result.next())
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "RecordingProfile::fillselections, Warning: "
            "Failed to locate recording id for recording group.");
        return profiles;
    }

    if (group == RecordingProfile::TranscoderGroup)
    {
        int id = RecordingProfile::TranscoderAutodetect;
        profiles[id] = QObject::tr("Transcode using Autodetect");
    }

    do
    {
        QString name = result.value(0).toString();
        int id = result.value(1).toInt();

        if (group == RecordingProfile::TranscoderGroup)
        {
            /* RTjpeg/MPEG4 and MPEG2 are used by "Autodetect". */
            if (name != "RTjpeg/MPEG4" && name != "MPEG2")
            {
                QString lbl = QObject::tr("Transcode using \"%1\"").arg(name);
                profiles[id] = lbl;
            }
            continue;
        }

        QString lbl = QObject::tr("Record using the \"%1\" profile").arg(name);
        profiles[id] = lbl;
    } while (result.next());

    return profiles;
}

QString RecordingProfile::groupType(void) const
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT profilegroups.cardtype "
        "FROM profilegroups, recordingprofiles "
        "WHERE profilegroups.id = recordingprofiles.profilegroup AND "
        "      recordingprofiles.id = :ID");
    result.bindValue(":ID", getProfileNum());

    if (!result.exec())
        MythDB::DBError("RecordingProfile::groupType", result);
    else if (result.next())
        return result.value(0).toString();

    return QString::null;
}

QString RecordingProfile::getName(int id)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "SELECT name "
        "FROM recordingprofiles "
        "WHERE id = :ID");

    result.bindValue(":ID", id);

    if (!result.exec())
        MythDB::DBError("RecordingProfile::getName", result);
    else if (result.next())
        return result.value(0).toString();

    return QString::null;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
