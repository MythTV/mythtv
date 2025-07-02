
#include "recordingprofile.h"

#include "cardutil.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "v4l2util.h"
#include <algorithm>
#include <utility>

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
    CodecParamStorage(StandardSetting *_setting,
                      const RecordingProfile &parentProfile,
                      const QString& name) :
        SimpleDBStorage(_setting, "codecparams", "value"),
        m_parent(parentProfile), m_codecName(name)
    {
        _setting->setName(name);
    }

    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

    const RecordingProfile &m_parent;
    QString m_codecName;
};

QString CodecParamStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString profileTag(":SETPROFILE");
    QString nameTag(":SETNAME");
    QString valueTag(":SETVALUE");

    QString query("profile = " + profileTag + ", name = " + nameTag
            + ", value = " + valueTag);

    bindings.insert(profileTag, m_parent.getProfileNum());
    bindings.insert(nameTag, m_codecName);
    bindings.insert(valueTag, m_user->GetDBValue());

    return query;
}

QString CodecParamStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString profileTag(":WHEREPROFILE");
    QString nameTag(":WHERENAME");

    QString query("profile = " + profileTag + " AND name = " + nameTag);

    bindings.insert(profileTag, m_parent.getProfileNum());
    bindings.insert(nameTag, m_codecName);

    return query;
}

class AudioCodecName: public MythUIComboBoxSetting
{
  public:
    explicit AudioCodecName(const RecordingProfile &parent) :
        MythUIComboBoxSetting(
            new RecordingProfileStorage(this, parent, "audiocodec"))
    {
        setLabel(QObject::tr("Codec"));
        setName("audiocodec");
    }
};

class MP3Quality : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit MP3Quality(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 1, 9, 1),
        CodecParamStorage(this, parent, "mp3quality")
    {
        setLabel(QObject::tr("MP3 quality"));
        setValue(7);
        setHelpText(QObject::tr("The higher the slider number, the lower the "
                    "quality of the audio. Better quality audio (lower "
                    "numbers) requires more CPU."));
    };
};

class BTTVVolume : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit BTTVVolume(const RecordingProfile& parent) :
        MythUISpinBoxSetting(this, 0, 100, 1),
        CodecParamStorage(this, parent, "volume")
    {
       setLabel(QObject::tr("Volume (%)"));
       setValue(90);
       setHelpText(QObject::tr("Recording volume of the capture card."));
    };
};

class SampleRate : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit SampleRate(const RecordingProfile &parent, bool analog = true) :
        MythUIComboBoxSetting(this), CodecParamStorage(this, parent, "samplerate")
    {
        setLabel(QObject::tr("Sampling rate"));
        setHelpText(QObject::tr("Sets the audio sampling rate for your DSP. "
                    "Ensure that you choose a sampling rate appropriate "
                    "for your device.  btaudio may only allow 32000."));

        m_rates.push_back(32000);
        m_rates.push_back(44100);
        m_rates.push_back(48000);

        m_allowedRate[48000] = true;
        for (uint i = 0; analog && (i < m_rates.size()); i++)
            m_allowedRate[m_rates[i]] = true;

    };

    void Load(void) override // StandardSetting
    {
        CodecParamStorage::Load();
        QString val = getValue();

        clearSelections();
        for (uint rate : m_rates)
        {
            if (m_allowedRate[rate])
                addSelection(QString::number(rate));
        }

        int which = getValueIndex(val);
        setValue(std::max(which,0));

        if (m_allowedRate.size() <= 1)
            setEnabled(false);
    }

    void addSelection(const QString &label,
                      const QString& value  = QString(),
                      bool           select = false)
    {
        QString val = value.isEmpty() ? label : value;
        uint rate = val.toUInt();
        if (m_allowedRate[rate])
        {
            MythUIComboBoxSetting::addSelection(label, value, select);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("SampleRate: ") +
                QString("Attempted to add a rate %1 Hz, which is "
                        "not in the list of allowed rates.").arg(rate));
        }
    }

    std::vector<uint> m_rates;
    QMap<uint,bool> m_allowedRate;
};

class MPEG2audType : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    MPEG2audType(const RecordingProfile &parent,
                 bool layer1, bool layer2, bool layer3) :
        MythUIComboBoxSetting(this), CodecParamStorage(this, parent, "mpeg2audtype"),
        m_allowLayer1(layer1), m_allowLayer2(layer2), m_allowLayer3(layer3)
    {
        setLabel(QObject::tr("Type"));

        if (m_allowLayer1)
            addSelection("Layer I");
        if (m_allowLayer2)
            addSelection("Layer II");
        if (m_allowLayer3)
            addSelection("Layer III");

        uint allowed_cnt = 0;
        allowed_cnt += ((m_allowLayer1) ? 1 : 0);
        allowed_cnt += ((m_allowLayer2) ? 1 : 0);
        allowed_cnt += ((m_allowLayer3) ? 1 : 0);

        if (1 == allowed_cnt)
            setEnabled(false);

        setHelpText(QObject::tr("Sets the audio type"));
    }

    void Load(void) override // StandardSetting
    {
        CodecParamStorage::Load();
        QString val = getValue();

        if ((val == "Layer I") && !m_allowLayer1)
        {
            if (m_allowLayer2)
                val = "Layer II";
            else if (m_allowLayer3)
                val = "Layer III";
        }

        if ((val == "Layer II") && !m_allowLayer2)
        {
            if (m_allowLayer3)
                val = "Layer III";
            else if (m_allowLayer1)
                val = "Layer I";
        }

        if ((val == "Layer III") && !m_allowLayer3)
        {
            if (m_allowLayer2)
                val = "Layer II";
            else if (m_allowLayer1)
                val = "Layer I";
        }

        if (getValue() != val)
        {
            int which = getValueIndex(val);
            if (which >= 0)
                setValue(which);
        }
    }

  private:
    bool m_allowLayer1;
    bool m_allowLayer2;
    bool m_allowLayer3;
};

class MPEG2audBitrateL1 : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2audBitrateL1(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this),
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

class MPEG2audBitrateL2 : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2audBitrateL2(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this),
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

class MPEG2audBitrateL3 : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2audBitrateL3(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this),
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

class MPEG2audVolume : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2audVolume(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 0, 100, 1),
        CodecParamStorage(this, parent, "mpeg2audvolume")
    {

        setLabel(QObject::tr("Volume (%)"));
        setValue(90);
        setHelpText(QObject::tr("Volume of the recording "));
    };
};

class MPEG2AudioBitrateSettings : public GroupSetting
{
  public:
    MPEG2AudioBitrateSettings(const RecordingProfile &parent,
                              bool layer1, bool layer2, bool layer3,
                              uint default_layer)
    {
        const std::array<const QString,3> layers { "Layer I", "Layer II", "Layer III", };

        setLabel(QObject::tr("Bitrate Settings"));

        auto *audType = new MPEG2audType(parent, layer1, layer2, layer3);

        addChild(audType);

        addTargetedChild(layers[0], new MPEG2audBitrateL1(parent));
        addTargetedChild(layers[1], new MPEG2audBitrateL2(parent));
        addTargetedChild(layers[2], new MPEG2audBitrateL3(parent));

        uint desired_layer = std::clamp(default_layer, 1U, 3U) - 1;
        int which = audType->getValueIndex(layers[desired_layer]);
        if (which >= 0)
            audType->setValue(which);
    };
};

class MPEG2Language : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2Language(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this), CodecParamStorage(this, parent, "mpeg2language")
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

class BitrateMode : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit BitrateMode(const RecordingProfile& parent,
                const QString& setting = "mpeg2bitratemode") :
        MythUIComboBoxSetting(this),
        CodecParamStorage(this, parent, setting)
    {
        setLabel(QObject::tr("Bitrate Mode"));

        addSelection("Variable Bitrate", "0");
        addSelection("Constant Bitrate", "1");
        setValue(0);
        setHelpText(QObject::tr("Bitrate mode"));
    }
};

class AudioCompressionSettings : public GroupSetting
{
  public:
    AudioCompressionSettings(const RecordingProfile &parentProfile,
                             [[maybe_unused]] V4L2util* v4l2) :
        m_parent(parentProfile),
        m_codecName(new AudioCodecName(m_parent))
    {
        setName(QObject::tr("Audio Quality"));

        addChild(m_codecName);

        QString label("MP3");
        m_codecName->addTargetedChild(label, new SampleRate(m_parent));
        m_codecName->addTargetedChild(label, new MP3Quality(m_parent));
        m_codecName->addTargetedChild(label, new BTTVVolume(m_parent));

        label = "MPEG-2 Hardware Encoder";
        m_codecName->addTargetedChild(label, new SampleRate(m_parent, false));
        m_codecName->addTargetedChild(label,
                                      new MPEG2AudioBitrateSettings
                                      (m_parent, false, true, false, 2));
        m_codecName->addTargetedChild(label, new MPEG2Language(m_parent));
        m_codecName->addTargetedChild(label, new MPEG2audVolume(m_parent));

        label = "Uncompressed";
        m_codecName->addTargetedChild(label, new SampleRate(m_parent));
        m_codecName->addTargetedChild(label, new BTTVVolume(m_parent));

        m_codecName->addTargetedChild("AC3 Hardware Encoder",
                                      new GroupSetting());

        m_codecName->addTargetedChild("AAC Hardware Encoder",
                                      new GroupSetting());

#if CONFIG_V4L2
        if (v4l2)
        {
            // Dynamically create user options based on the
            // capabilties the driver reports.

            DriverOption::Options options;

            if (v4l2->GetOptions(options))
            {
                /* StandardSetting cannot handle multiple 'targets' pointing
                 * to the same setting configuration, so we need to do
                 * this in two passes. */

                for (const auto & option : std::as_const(options))
                {
                    if (option.m_category == DriverOption::AUDIO_ENCODING)
                    {
                        for (const auto & Imenu : std::as_const(option.m_menu))
                        {
                            if (!Imenu.isEmpty())
                                m_v4l2codecs << "V4L2:" + Imenu;
                        }
                    }
                }

                for (auto Icodec = m_v4l2codecs.begin(); Icodec < m_v4l2codecs.end(); ++Icodec)
                {
                    for (const auto & option : std::as_const(options))
                    {
                        if (option.m_category == DriverOption::AUDIO_BITRATE_MODE)
                        {
                            m_codecName->addTargetedChild(*Icodec,
                                 new BitrateMode(m_parent, "audbitratemode"));
                        }
                        else if (option.m_category ==
                                 DriverOption::AUDIO_SAMPLERATE)
                        {
                            m_codecName->addTargetedChild(*Icodec,
                                             new SampleRate(m_parent, false));
                        }
                        else if (option.m_category ==
                                 DriverOption::AUDIO_LANGUAGE)
                        {
                            m_codecName->addTargetedChild(*Icodec,
                                             new MPEG2Language(m_parent));
                        }
                        else if (option.m_category == DriverOption::AUDIO_BITRATE)
                        {
                            bool layer1 = false;
                            bool layer2 = false;
                            bool layer3 = false;

                            for (const auto & Imenu : std::as_const(option.m_menu))
                            {
                                if (Imenu.indexOf("Layer III") >= 0)
                                    layer3 = true;
                                else if (Imenu.indexOf("Layer II") >= 0)
                                    layer2 = true;
                                else if (Imenu.indexOf("Layer I") >= 0)
                                    layer1 = true;
                            }

                            if (layer1 || layer2 || layer3)
                            {
                                m_codecName->addTargetedChild(*Icodec,
                                       new MPEG2AudioBitrateSettings(m_parent,
                                                                     layer1,
                                                                     layer2,
                                                                     layer3, 2));
                            }
                        }
                        else if (option.m_category == DriverOption::VOLUME)
                        {
                            m_codecName->addTargetedChild(*Icodec,
                                                new MPEG2audVolume(m_parent));
                        }
                    }
                }
            }
        }
#endif //  CONFIG_V4L2
    }

    void selectCodecs(const QString & groupType)
    {
        if (!groupType.isNull())
        {
            if (groupType == "MPEG")
               m_codecName->addSelection("MPEG-2 Hardware Encoder");
            else if (groupType == "HDPVR")
            {
                m_codecName->addSelection("AC3 Hardware Encoder");
                m_codecName->addSelection("AAC Hardware Encoder");
            }
            else if (groupType.startsWith("V4L2:"))
            {
                for (const auto & codec : std::as_const(m_v4l2codecs))
                    m_codecName->addSelection(codec);
            }
            else
            {
                // V4L, TRANSCODE (and any undefined types)
                m_codecName->addSelection("MP3");
                m_codecName->addSelection("Uncompressed");
            }
        }
        else
        {
            m_codecName->addSelection("MP3");
            m_codecName->addSelection("Uncompressed");
            m_codecName->addSelection("MPEG-2 Hardware Encoder");
        }
    }
private:
    const RecordingProfile& m_parent;
    AudioCodecName*         m_codecName;
    QStringList             m_v4l2codecs;
};

class VideoCodecName : public MythUIComboBoxSetting
{
  public:
    explicit VideoCodecName(const RecordingProfile &parent) :
        MythUIComboBoxSetting(
            new RecordingProfileStorage(this, parent, "videocodec"))
    {
        setLabel(QObject::tr("Codec"));
        setName("videocodec");
    }
};

class RTjpegQuality : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit RTjpegQuality(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 1, 255, 1),
        CodecParamStorage(this, parent, "rtjpegquality")
    {
        setLabel(QObject::tr("RTjpeg Quality"));
        setValue(170);
        setHelpText(QObject::tr("Higher is better quality."));
    };
};

class RTjpegLumaFilter : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit RTjpegLumaFilter(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 0, 31, 1),
        CodecParamStorage(this, parent, "rtjpeglumafilter")
    {
        setLabel(QObject::tr("Luma filter"));
        setValue(0);
        setHelpText(QObject::tr("Lower is better."));
    };
};

class RTjpegChromaFilter : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit RTjpegChromaFilter(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 0, 31, 1),
        CodecParamStorage(this, parent, "rtjpegchromafilter")
    {
        setLabel(QObject::tr("Chroma filter"));
        setValue(0);
        setHelpText(QObject::tr("Lower is better."));
    };
};

class MPEG4bitrate : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4bitrate(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 100, 8000, 100),
        CodecParamStorage(this, parent, "mpeg4bitrate")
    {
        setLabel(QObject::tr("Bitrate (kb/s)"));
        setValue(2200);
        setHelpText(QObject::tr("Bitrate in kilobits/second. As a guide, "
                    "2200 kb/s is approximately 1 GB/hr."));
    };
};

class ScaleBitrate : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit ScaleBitrate(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
        CodecParamStorage(this, parent, "scalebitrate")
    {
        setLabel(QObject::tr("Scale bitrate for frame size"));
        setValue(true);
        setHelpText(QObject::tr("If set, the bitrate specified will be used "
                    "for 640x480. If other resolutions are used, the "
                    "bitrate will be scaled appropriately."));
    };
};

class MPEG4MinQuality : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4MinQuality(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 1, 31, 1),
        CodecParamStorage(this, parent, "mpeg4minquality")
    {
        setLabel(QObject::tr("Minimum quality"));
        setValue(15);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4MaxQuality : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4MaxQuality(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 1, 31, 1),
        CodecParamStorage(this, parent, "mpeg4maxquality")
    {
        setLabel(QObject::tr("Maximum quality"));
        setValue(2);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4QualDiff : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4QualDiff(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 1, 31, 1),
        CodecParamStorage(this, parent, "mpeg4qualdiff")
    {

        setLabel(QObject::tr("Max quality difference between frames"));
        setValue(3);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4OptionIDCT : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4OptionIDCT(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
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

class MPEG4OptionIME : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4OptionIME(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
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

class MPEG4OptionVHQ : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4OptionVHQ(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg4optionvhq")
    {
        setLabel(QObject::tr("Enable high-quality encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "'high-quality' encoding options. This requires much "
                    "more processing, but can result in better video."));
    };
};

class MPEG4Option4MV : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG4Option4MV(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
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

class EncodingThreadCount : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit EncodingThreadCount(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 1, 8, 1),
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

class AverageBitrate : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit AverageBitrate(const RecordingProfile &parent,
                   const QString& setting = "mpeg2bitrate",
                   uint min_br = 1000, uint max_br = 16000,
                   uint default_br = 4500, uint increment = 100,
                   QString label = QString()) :
        MythUISpinBoxSetting(this, min_br, max_br, increment),
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

class PeakBitrate : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit PeakBitrate(const RecordingProfile &parent,
                const QString& setting = "mpeg2maxbitrate",
                uint min_br = 1000, uint max_br = 16000,
                uint default_br = 6000, uint increment = 100,
                QString label = QString()) :
        MythUISpinBoxSetting(this, min_br, max_br, increment),
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

class MPEG2streamType : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2streamType(const RecordingProfile &parent,
                    uint minopt = 0, uint maxopt = 8, uint defopt = 0) :
        MythUIComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2streamtype")
    {
        maxopt = std::min<uint>(maxopt, 8);

        setLabel(QObject::tr("Stream Type"));

        const std::array<const QString,9> options { "MPEG-2 PS", "MPEG-2 TS",
                                     "MPEG-1 VCD", "PES AV",
                                     "PES V", "PES A",
                                     "DVD", "DVD-Special 1", "DVD-Special 2" };

        for (uint idx = minopt; idx <= maxopt; ++idx)
            addSelection(options[idx]);

        setValue(defopt - minopt);
        setHelpText(QObject::tr("Sets the type of stream generated by "
                    "your PVR."));
    };
};

class MPEG2aspectRatio : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit MPEG2aspectRatio(const RecordingProfile &parent,
                     uint minopt = 0, uint maxopt = 8, uint defopt = 0) :
        MythUIComboBoxSetting(this),
        CodecParamStorage(this, parent, "mpeg2aspectratio")
    {
        maxopt = std::min<uint>(maxopt, 3);

        setLabel(QObject::tr("Aspect Ratio"));

        const std::array<const QString,4> options { QObject::tr("Square"), "4:3",
                                     "16:9", "2.21:1" };

        for (uint idx = minopt; idx <= maxopt; ++idx)
            addSelection(options[idx]);

        setValue(defopt);
        setHelpText(QObject::tr("Sets the aspect ratio of stream generated "
                    "by your PVR."));
    };
};

class HardwareMJPEGQuality : public MythUISpinBoxSetting, public CodecParamStorage
{
  public:
    explicit HardwareMJPEGQuality(const RecordingProfile &parent) :
        MythUISpinBoxSetting(this, 0, 100, 1),
        CodecParamStorage(this, parent, "hardwaremjpegquality")
    {
        setLabel(QObject::tr("Quality"));
        setValue("100");
    };
};

class HardwareMJPEGHDecimation : public MythUIComboBoxSetting,
                                 public CodecParamStorage
{
  public:
    explicit HardwareMJPEGHDecimation(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this),
        CodecParamStorage(this, parent, "hardwaremjpeghdecimation")
    {
        setLabel(QObject::tr("Horizontal Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class HardwareMJPEGVDecimation : public MythUIComboBoxSetting,
                                 public CodecParamStorage
{
  public:
    explicit HardwareMJPEGVDecimation(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this),
        CodecParamStorage(this, parent, "hardwaremjpegvdecimation") {
        setLabel(QObject::tr("Vertical Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class VideoCompressionSettings : public GroupSetting
{
  public:
    VideoCompressionSettings(const RecordingProfile &parentProfile,
                             [[maybe_unused]] V4L2util* v4l2) :
        m_parent(parentProfile),
        m_codecName(new VideoCodecName(m_parent))
    {
        setName(QObject::tr("Video Compression"));

        addChild(m_codecName);

        QString label("RTjpeg");
        m_codecName->addTargetedChild(label, new RTjpegQuality(m_parent));
        m_codecName->addTargetedChild(label, new RTjpegLumaFilter(m_parent));
        m_codecName->addTargetedChild(label, new RTjpegChromaFilter(m_parent));

        label = "MPEG-4";
        m_codecName->addTargetedChild(label, new MPEG4bitrate(m_parent));
        m_codecName->addTargetedChild(label, new MPEG4MaxQuality(m_parent));
        m_codecName->addTargetedChild(label, new MPEG4MinQuality(m_parent));
        m_codecName->addTargetedChild(label, new MPEG4QualDiff(m_parent));
        m_codecName->addTargetedChild(label, new ScaleBitrate(m_parent));

        m_codecName->addTargetedChild(label, new MPEG4OptionVHQ(m_parent));
        m_codecName->addTargetedChild(label, new MPEG4Option4MV(m_parent));

        m_codecName->addTargetedChild(label, new MPEG4OptionIDCT(m_parent));
        m_codecName->addTargetedChild(label, new MPEG4OptionIME(m_parent));
#ifdef USING_FFMPEG_THREADS
        m_codecName->addTargetedChild(label, new EncodingThreadCount(m_parent));
#endif

        label = "MPEG-2";
        m_codecName->addTargetedChild(label, new AverageBitrate(m_parent));
        m_codecName->addTargetedChild(label, new ScaleBitrate(m_parent));
        //m_codecName->addTargetedChild(label, new MPEG4MaxQuality(m_parent));
        //m_codecName->addTargetedChild(label, new MPEG4MinQuality(m_parent));
        //m_codecName->addTargetedChild(label, new MPEG4QualDiff(m_parent));
        //m_codecName->addTargetedChild(label, new MPEG4OptionVHQ(m_parent));
        //m_codecName->addTargetedChild(label, new MPEG4Option4MV(m_parent));
#ifdef USING_FFMPEG_THREADS
        addTargetedChild(label, new EncodingThreadCount(m_parent));
#endif

        label = "Hardware MJPEG";
        m_codecName->addTargetedChild(label, new HardwareMJPEGQuality(m_parent));
        m_codecName->addTargetedChild(label, new HardwareMJPEGHDecimation(m_parent));
        m_codecName->addTargetedChild(label, new HardwareMJPEGVDecimation(m_parent));

        label = "MPEG-2 Hardware Encoder";
        m_codecName->addTargetedChild(label, new MPEG2streamType(m_parent));
        m_codecName->addTargetedChild(label, new MPEG2aspectRatio(m_parent));
        m_codecName->addTargetedChild(label, new AverageBitrate(m_parent));
        m_codecName->addTargetedChild(label, new PeakBitrate(m_parent));

        label = "MPEG-4 AVC Hardware Encoder";
        auto *h0 = new GroupSetting();
        h0->setLabel(QObject::tr("Low Resolution"));
        h0->addChild(new AverageBitrate(m_parent, "low_mpeg4avgbitrate",
                                        1000, 13500, 4500, 500));
        h0->addChild(new PeakBitrate(m_parent, "low_mpeg4peakbitrate",
                                     1100, 20200, 6000, 500));
        m_codecName->addTargetedChild(label, h0);

        auto *h1 = new GroupSetting();
        h1->setLabel(QObject::tr("Medium Resolution"));
        h1->addChild(new AverageBitrate(m_parent, "medium_mpeg4avgbitrate",
                                        1000, 13500, 9000, 500));
        h1->addChild(new PeakBitrate(m_parent, "medium_mpeg4peakbitrate",
                                     1100, 20200, 11000, 500));
        m_codecName->addTargetedChild(label, h1);

        auto *h2 = new GroupSetting();
        h2->setLabel(QObject::tr("High Resolution"));
        h2->addChild(new AverageBitrate(m_parent, "high_mpeg4avgbitrate",
                                            1000, 13500, 13500, 500));
        h2->addChild(new PeakBitrate(m_parent, "high_mpeg4peakbitrate",
                                         1100, 20200, 20200, 500));
        m_codecName->addTargetedChild(label, h2);

#if CONFIG_V4L2
        if (v4l2)
        {
            DriverOption::Options options;

            if (v4l2->GetOptions(options))
            {
                /* StandardSetting cannot handle multiple 'targets' pointing
                 * to the same setting configuration, so we need to do
                 * this in two passes. */

                for (const auto & option : std::as_const(options))
                {
                    if (option.m_category == DriverOption::VIDEO_ENCODING)
                    {
                        for (const auto & Imenu : std::as_const(option.m_menu))
                        {
                            if (!Imenu.isEmpty())
                                m_v4l2codecs << "V4L2:" + Imenu;
                        }
                    }
                }

                for (auto Icodec = m_v4l2codecs.begin(); Icodec < m_v4l2codecs.end(); ++Icodec)
                {
                    auto* bit_low    = new GroupSetting();
                    auto* bit_medium = new GroupSetting();
                    auto* bit_high   = new GroupSetting();
                    bool dynamic_res = !v4l2->UserAdjustableResolution();

                    for (auto & option : options)
                    {
                        if (option.m_category == DriverOption::STREAM_TYPE)
                        {
                            m_codecName->addTargetedChild(*Icodec,
                                             new MPEG2streamType(m_parent,
                                                     option.m_minimum,
                                                     option.m_maximum,
                                                     option.m_defaultValue));
                        }
                        else if (option.m_category == DriverOption::VIDEO_ASPECT)
                        {
                            m_codecName->addTargetedChild(*Icodec,
                                             new MPEG2aspectRatio(m_parent,
                                             option.m_minimum,
                                             option.m_maximum,
                                             option.m_defaultValue));
                        }
                        else if (option.m_category ==
                                 DriverOption::VIDEO_BITRATE_MODE)
                        {
                            if (dynamic_res)
                            {
                                bit_low->addChild(new BitrateMode(m_parent,
                                             "low_mpegbitratemode"));
                                bit_medium->addChild(new BitrateMode(m_parent,
                                             "medium_mpegbitratemode"));
                                bit_high->addChild(new BitrateMode(m_parent,
                                             "medium_mpegbitratemode"));
                            }
                            else
                            {
                                bit_low->addChild(new BitrateMode(m_parent));
                            }
                        }
                        else if (option.m_category == DriverOption::VIDEO_BITRATE)
                        {
                            if (dynamic_res)
                            {
                                bit_low->setLabel(QObject::tr("Low Resolution"));
                                bit_low->addChild(new AverageBitrate(m_parent,
                                             "low_mpegavgbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));

                                bit_medium->setLabel(QObject::
                                                     tr("Medium Resolution"));
                                bit_medium->addChild(new AverageBitrate(m_parent,
                                             "medium_mpegavgbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));

                                bit_high->setLabel(QObject::
                                                   tr("High Resolution"));
                                bit_high->addChild(new AverageBitrate(m_parent,
                                             "high_mpegavgbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));
                            }
                            else
                            {
                                bit_low->setLabel(QObject::tr("Bitrate"));
                                bit_low->addChild(new AverageBitrate(m_parent,
                                             "mpeg2bitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));
                            }
                        }
                        else if (option.m_category ==
                                 DriverOption::VIDEO_BITRATE_PEAK)
                        {
                            if (dynamic_res)
                            {
                                bit_low->addChild(new PeakBitrate(m_parent,
                                             "low_mpegpeakbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));
                                bit_medium->addChild(new PeakBitrate(m_parent,
                                             "medium_mpegpeakbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));
                                bit_high->addChild(new PeakBitrate(m_parent,
                                             "high_mpegpeakbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));
                            }
                            else
                            {
                                bit_low->addChild(new PeakBitrate(m_parent,
                                             "mpeg2maxbitrate",
                                             option.m_minimum / 1000,
                                             option.m_maximum / 1000,
                                             option.m_defaultValue / 1000,
                                             option.m_step / 1000));
                            }
                        }
                    }

                    m_codecName->addTargetedChild(*Icodec, bit_low);
                    if (dynamic_res)
                    {
                        m_codecName->addTargetedChild(*Icodec, bit_medium);
                        m_codecName->addTargetedChild(*Icodec, bit_high);
                    }
                    else
                    {
                        // These are only referenced when dynamic_res is true.
                        delete bit_medium;
                        delete bit_high;
                    }
                }
            }
        }
#endif // CONFIG_V4L2
    }

    void selectCodecs(const QString& groupType)
    {
        if (!groupType.isNull())
        {
            if (groupType == "HDPVR")
               m_codecName->addSelection("MPEG-4 AVC Hardware Encoder");
            else if (groupType.startsWith("V4L2:"))
            {
                for (const auto & codec : std::as_const(m_v4l2codecs))
                    m_codecName->addSelection(codec);
            }
            else if (groupType == "MPEG")
            {
               m_codecName->addSelection("MPEG-2 Hardware Encoder");
            }
            else if (groupType == "MJPEG")
            {
                m_codecName->addSelection("Hardware MJPEG");
            }
            else if (groupType == "GO7007")
            {
                m_codecName->addSelection("MPEG-4");
                m_codecName->addSelection("MPEG-2");
            }
            else
            {
                // V4L, TRANSCODE (and any undefined types)
                m_codecName->addSelection("RTjpeg");
                m_codecName->addSelection("MPEG-4");
            }
        }
        else
        {
            m_codecName->addSelection("RTjpeg");
            m_codecName->addSelection("MPEG-4");
            m_codecName->addSelection("Hardware MJPEG");
            m_codecName->addSelection("MPEG-2 Hardware Encoder");
        }
    }

private:
    const RecordingProfile& m_parent;
    VideoCodecName*         m_codecName;
    QStringList             m_v4l2codecs;
};

class AutoTranscode : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit AutoTranscode(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
        CodecParamStorage(this, parent, "autotranscode")
    {
        setLabel(QObject::tr("Enable auto-transcode after recording"));
        setValue(false);
        setHelpText(QObject::tr("Automatically transcode when a recording is "
                                "made using this profile and the recording's "
                                "schedule is configured to allow transcoding."));
    };
};

class TranscodeResize : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit TranscodeResize(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
        CodecParamStorage(this, parent, "transcoderesize")
    {
        setLabel(QObject::tr("Resize video while transcoding"));
        setValue(false);
        setHelpText(QObject::tr("Allows the transcoder to "
                                "resize the video during transcoding."));
    };
};

class TranscodeLossless : public MythUICheckBoxSetting, public CodecParamStorage
{
  public:
    explicit TranscodeLossless(const RecordingProfile &parent) :
        MythUICheckBoxSetting(this),
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

class RecordingTypeStream : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit RecordingTypeStream(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this), CodecParamStorage(this, parent, "recordingtype")
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

class RecordFullTSStream : public MythUIComboBoxSetting, public CodecParamStorage
{
  public:
    explicit RecordFullTSStream(const RecordingProfile &parent) :
        MythUIComboBoxSetting(this), CodecParamStorage(this, parent, "recordmpts")
    {
        setLabel(QObject::tr("Record Full TS?"));

        QString msg = QObject::tr(
            "If set, extra files will be created for each recording with "
            "the name of the recording followed by '.ts.raw'. "
            "These extra files represent the full contents of the transport "
            "stream used to generate the recording. (For debugging purposes)");
        setHelpText(msg);

        addSelection(QObject::tr("Yes"), "1");
        addSelection(QObject::tr("No"),  "0");
        setValue(1);
    };
};

class TranscodeFilters : public MythUITextEditSetting, public CodecParamStorage
{
  public:
    explicit TranscodeFilters(const RecordingProfile &parent) :
        MythUITextEditSetting(this),
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

class ImageSize : public GroupSetting
{
  public:
    class Width : public MythUISpinBoxSetting, public CodecParamStorage
    {
      public:
        Width(const RecordingProfile &parent,
              uint defaultwidth, uint maxwidth,
              bool transcoding = false) :
            MythUISpinBoxSetting(this, transcoding ? 0 : 160,
                           maxwidth, 16, 0,
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

    class Height: public MythUISpinBoxSetting, public CodecParamStorage
    {
      public:
        Height(const RecordingProfile &parent,
               uint defaultheight, uint maxheight,
               bool transcoding = false):
            MythUISpinBoxSetting(this, transcoding ? 0 : 160,
                           maxheight, 16, 0,
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
              const QString& tvFormat, const QString& profName)
    {
        setLabel(QObject::tr("Image size"));

        QSize defaultsize(768, 576);
        QSize maxsize(768, 576);
        bool transcoding = profName.startsWith("Transcoders");
        bool ivtv = profName.startsWith("IVTV MPEG-2 Encoders");

        if (transcoding)
        {
            maxsize     = QSize(1920, 1088);
            if (tvFormat.toLower() == "ntsc" || tvFormat.toLower() == "atsc")
                defaultsize = QSize(480, 480);
            else
                defaultsize = QSize(480, 576);
        }
        else if (tvFormat.startsWith("ntsc", Qt::CaseInsensitive))
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

        addChild(new Width(parent, defaultsize.width(),
                                    maxsize.width(), transcoding));
        addChild(new Height(parent, defaultsize.height(),
                                     maxsize.height(), transcoding));
    };
};

// id and name will be deleted by ConfigurationGroup's destructor
RecordingProfile::RecordingProfile(const QString& profName)
    : m_id(new ID()),
      m_name(new Name(*this)),
      m_profileName(profName)
{
    // This must be first because it is needed to load/save the other settings
    addChild(m_id);

    setLabel(profName);
    addChild(m_name);

    if (!profName.isEmpty())
    {
        if (profName.startsWith("Transcoders"))
        {
            m_trFilters  = new TranscodeFilters(*this);
            m_trLossless = new TranscodeLossless(*this);
            m_trResize   = new TranscodeResize(*this);
            addChild(m_trFilters);
            addChild(m_trLossless);
            addChild(m_trResize);
        }
        else
        {
            addChild(new AutoTranscode(*this));
        }
    }
    else
    {
        m_trFilters  = new TranscodeFilters(*this);
        m_trLossless = new TranscodeLossless(*this);
        m_trResize   = new TranscodeResize(*this);
        addChild(m_trFilters);
        addChild(m_trLossless);
        addChild(m_trResize);
        addChild(new AutoTranscode(*this));
    }
};

RecordingProfile::~RecordingProfile(void)
{
#if CONFIG_V4L2
    delete m_v4l2util;
    m_v4l2util = nullptr;
#endif
}

void RecordingProfile::ResizeTranscode(const QString & /*val*/)
{
    if (m_imageSize)
        m_imageSize->setEnabled(m_trResize->boolValue());
}

void RecordingProfile::SetLosslessTranscode(const QString & /*val*/)
{
    bool lossless = m_trLossless->boolValue();
    bool show_size = (lossless) ? false : m_trResize->boolValue();
    if (m_imageSize)
        m_imageSize->setEnabled(show_size);
    m_videoSettings->setEnabled(! lossless);
    m_audioSettings->setEnabled(! lossless);
    m_trResize->setEnabled(! lossless);
    m_trFilters->setEnabled(! lossless);
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
    // If there are filters, we cannot do lossless transcoding
    if (!val.trimmed().isEmpty())
    {
       m_trLossless->setValue(false);
       m_trLossless->setEnabled(false);
    }
    else
    {
       m_trLossless->setEnabled(true);
    }
}

bool RecordingProfile::loadByType(const QString &name, const QString &card,
                                  [[maybe_unused]] const QString &videodev)
{
    QString hostname = gCoreContext->GetHostName().toLower();
    QString cardtype = card;
    uint profileId = 0;

#if CONFIG_V4L2
    if (cardtype == "V4L2ENC")
    {
        m_v4l2util = new V4L2util(videodev);
        if (m_v4l2util->IsOpen())
            cardtype = m_v4l2util->ProfileName();
    }
#endif

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
    if (m_profileName.isEmpty())
        m_profileName = name;

    m_isEncoder = CardUtil::IsEncoder(type);

    if (m_isEncoder)
    {
#if CONFIG_V4L2
        if (type.startsWith("V4L2:"))
        {
            QStringList devices = CardUtil::GetVideoDevices("V4L2ENC");
            if (!devices.isEmpty())
            {
                for (const auto & device : std::as_const(devices))
                {
                    delete m_v4l2util;
                    m_v4l2util = new V4L2util(device);
                    if (m_v4l2util->IsOpen() &&
                        m_v4l2util->DriverName() == type.mid(5))
                        break;
                    delete m_v4l2util;
                    m_v4l2util = nullptr;
                }
            }
        }
#endif

        QString tvFormat = gCoreContext->GetSetting("TVFormat");
        // TODO: When mpegrecorder is removed, don't check for "HDPVR' anymore...
        if (type != "HDPVR" &&
            (!m_v4l2util
#if CONFIG_V4L2
             || m_v4l2util->UserAdjustableResolution()
#endif
            ))
        {
            addChild(new ImageSize(*this, tvFormat, m_profileName));
        }
        m_videoSettings = new VideoCompressionSettings(*this,
                                                       m_v4l2util);
        addChild(m_videoSettings);

        m_audioSettings = new AudioCompressionSettings(*this,
                                                       m_v4l2util);
        addChild(m_audioSettings);

        if (!m_profileName.isEmpty() && m_profileName.startsWith("Transcoders"))
        {
            connect(m_trResize,   qOverload<const QString&>(&StandardSetting::valueChanged),
                    this,         &RecordingProfile::ResizeTranscode);
            connect(m_trLossless, qOverload<const QString&>(&StandardSetting::valueChanged),
                    this,         &RecordingProfile::SetLosslessTranscode);
            connect(m_trFilters,  qOverload<const QString&>(&StandardSetting::valueChanged),
                    this,         &RecordingProfile::FiltersChanged);
        }
    }

    // Cards that can receive additional streams such as EIT and MHEG
    if (CardUtil::IsEITCapable(type))
    {
        addChild(new RecordingTypeStream(*this));
    }

    if (CardUtil::IsTunerSharingCapable(type))
    {
        addChild(new RecordFullTSStream(*this));
    }

    m_id->setValue(profileId);
    Load();
}

void RecordingProfile::setCodecTypes()
{
    if (m_videoSettings)
        m_videoSettings->selectCodecs(groupType());
    if (m_audioSettings)
        m_audioSettings->selectCodecs(groupType());
}

RecordingProfileEditor::RecordingProfileEditor(int id, QString profName) :
    m_group(id), m_labelName(std::move(profName))
{
    if (!m_labelName.isEmpty())
        setLabel(m_labelName);
}

void RecordingProfileEditor::Load(void)
{
    clearSettings();
    auto *newProfile = new ButtonStandardSetting(tr("(Create new profile)"));
    connect(newProfile, &ButtonStandardSetting::clicked, this, &RecordingProfileEditor::ShowNewProfileDialog);
    addChild(newProfile);
    RecordingProfile::fillSelections(this, m_group);
    StandardSetting::Load();
}

void RecordingProfileEditor::ShowNewProfileDialog() const
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *settingdialog = new MythTextInputDialog(popupStack,
                                tr("Enter the name of the new profile"));

    if (settingdialog->Create())
    {
        connect(settingdialog, &MythTextInputDialog::haveResult,
                this, &RecordingProfileEditor::CreateNewProfile);
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
}

void RecordingProfileEditor::CreateNewProfile(const QString& profName)
{
   MSqlQuery query(MSqlQuery::InitCon());
   query.prepare(
       "INSERT INTO recordingprofiles "
           "(name, videocodec, audiocodec, profilegroup) "
         "VALUES "
           "(:NAME, :VIDEOCODEC, :AUDIOCODEC, :PROFILEGROUP);");
   query.bindValue(":NAME", profName);
   query.bindValue(":VIDEOCODEC", "MPEG-4");
   query.bindValue(":AUDIOCODEC", "MP3");
   query.bindValue(":PROFILEGROUP", m_group);
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
       query.bindValue(":PROFILEGROUP", m_group);
       if (!query.exec())
       {
           MythDB::DBError("RecordingProfileEditor::open", query);
       }
       else
       {
           if (query.next())
           {
               auto* profile = new RecordingProfile(profName);

               profile->loadByID(query.value(0).toInt());
               profile->setCodecTypes();
               addChild(profile);
               emit settingsChanged(this);
           }
       }
   }
}

void RecordingProfile::fillSelections(GroupSetting *setting, int group,
                                      bool foldautodetect)
{
    if (!group)
    {
        for (const auto & name : kAvailProfiles)
        {
            auto *profile = new GroupSetting();
            profile->setLabel(name);
            setting->addChild(profile);
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

    if (!result.exec())
    {
        MythDB::DBError("RecordingProfile::fillSelections 1", result);
        return;
    }

    while (result.next())
    {
        if ((result.at() == 0) && foldautodetect &&
            (group == RecordingProfile::TranscoderGroup))
        {
            auto *autodetect = new GroupSetting();
            autodetect->setLabel(QObject::tr("Autodetect"));
            setting->addChild(autodetect);
        }

        QString name = result.value(0).toString();
        QString id   = result.value(1).toString();

        if ((group == RecordingProfile::TranscoderGroup) &&
            (name == "RTjpeg/MPEG4" || name == "MPEG2") &&
            !foldautodetect) {
            name = QObject::tr("Autodetect from %1").arg(name);
        }

        auto *profile = new RecordingProfile(name);
        profile->loadByID(id.toInt());
        profile->setCodecTypes();
        setting->addChild(profile);
    }
}

QMap< int, QString > RecordingProfile::GetProfiles(RecProfileGroup group)
{
    QMap<int, QString> profiles;

    if (!group)
    {
        for (uint i = 0; !kAvailProfiles[i].isEmpty(); i++)
            profiles[i] = kAvailProfiles[i];
        return profiles;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name, id "
        "FROM recordingprofiles "
        "WHERE profilegroup = :GROUPID "
        "ORDER BY id");
    query.bindValue(":GROUPID", group);

    if (!query.exec())
    {
        MythDB::DBError("RecordingProfile::GetProfileMap()", query);
        return profiles;
    }

    while (query.next())
    {
        if ((query.at() == 0) && (group == RecordingProfile::TranscoderGroup))
        {
            int id = RecordingProfile::kTranscoderAutodetect;
            profiles[id] = QObject::tr("Transcode using Autodetect");
        }

        QString name = query.value(0).toString();
        int id = query.value(1).toInt();

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
    }

    if (query.at() == QSql::BeforeFirstRow)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "RecordingProfile::fillselections, Warning: "
            "Failed to locate recording id for recording group.");
    }

    return profiles;
}

QMap< int, QString > RecordingProfile::GetTranscodingProfiles()
{
    return GetProfiles(RecordingProfile::TranscoderGroup);
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

    return {};
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

    return {};
}

bool RecordingProfile::canDelete(void)
{
    return true;
}

void RecordingProfile::deleteEntry(void)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(
        "DELETE "
        "FROM recordingprofiles "
        "WHERE id = :ID");

    result.bindValue(":ID", m_id->getValue());

    if (!result.exec())
        MythDB::DBError("RecordingProfile::deleteEntry", result);

}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
