#include <QWidget>

#include "channelsettings.h"
#include "cardutil.h"
#include "channelutil.h"
#include "programinfo.h" // for COMM_DETECT*, GetPreferredSkipTypeCombinations()

QString ChannelDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":WHERE" + id.getField().toUpper());
    QString query(id.getField() + " = " + fieldTag);

    bindings.insert(fieldTag, id.getValue());

    return query;
}

QString ChannelDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":SET" + id.getField().toUpper());
    QString nameTag = (":SET" + GetColumnName().toUpper());

    QString query(id.getField() + " = " + fieldTag + ", " +
                  GetColumnName() + " = " + nameTag);

    bindings.insert(fieldTag, id.getValue());
    bindings.insert(nameTag, user->GetDBValue());

    return query;
}

/*****************************************************************************
        Channel Options - Common
 *****************************************************************************/

class Name : public LineEditSetting, public ChannelDBStorage
{
  public:
    Name(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "name")
    {
        setLabel(QObject::tr("Channel Name"));
    }
};

class Channum : public LineEditSetting, public ChannelDBStorage
{
  public:
    Channum(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "channum")
    {
        setLabel(QObject::tr("Channel Number"));
    }
};

class Source : public ComboBoxSetting, public ChannelDBStorage
{
  public:
    Source(const ChannelID &id, uint _default_sourceid) :
        ComboBoxSetting(this), ChannelDBStorage(this, id, "sourceid"),
        default_sourceid(_default_sourceid)
    {
        setLabel(QObject::tr("Video Source"));
    }

    void Load(void)
    {
        fillSelections();
        ChannelDBStorage::Load();

        if (default_sourceid && !getValue().toUInt())
        {
            uint which = sourceid_to_index[default_sourceid];
            if (which)
                setValue(which);
        }
    }

    void fillSelections(void)
    {
        addSelection(QObject::tr("[Not Selected]"), "0");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT name, sourceid "
                      "FROM videosource "
                      "ORDER BY sourceid");

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("Source::fillSelections", query);
        }
        else
        {
            for (uint i = 1; query.next(); i++)
            {
                sourceid_to_index[query.value(1).toUInt()] = i;
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
            }
        }

        sourceid_to_index[0] = 0; // Not selected entry.
    }

  private:
    uint            default_sourceid;
    QMap<uint,uint> sourceid_to_index;
};

class Callsign : public LineEditSetting, public ChannelDBStorage
{
  public:
    Callsign(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "callsign")
    {
        setLabel(QObject::tr("Callsign"));
    }
};

ChannelTVFormat::ChannelTVFormat(const ChannelID &id) :
    ComboBoxSetting(this), ChannelDBStorage(this, id, "tvformat")
{
    setLabel(QObject::tr("TV Format"));
    setHelpText(
        QObject::tr(
            "If this channel uses a format other than TV "
            "Format in the General Backend Setup screen, set it here."));

    addSelection(QObject::tr("Default"), "Default");

    QStringList list = GetFormats();
    for (int i = 0; i < list.size(); i++)
        addSelection(list[i]);
}

QStringList ChannelTVFormat::GetFormats(void)
{
    QStringList list;

    list.push_back("NTSC");
    list.push_back("NTSC-JP");
    list.push_back("PAL");
    list.push_back("PAL-60");
    list.push_back("PAL-BG");
    list.push_back("PAL-DK");
    list.push_back("PAL-D");
    list.push_back("PAL-I");
    list.push_back("PAL-M");
    list.push_back("PAL-N");
    list.push_back("PAL-NC");
    list.push_back("SECAM");
    list.push_back("SECAM-D");
    list.push_back("SECAM-DK");

    return list;
}

class TimeOffset : public SpinBoxSetting, public ChannelDBStorage
{
  public:
    TimeOffset(const ChannelID &id) :
        SpinBoxSetting(this, -1440, 1440, 1),
        ChannelDBStorage(this, id, "tmoffset")
    {
        setLabel(QObject::tr("DataDirect") + " " + QObject::tr("Time Offset"));
        setHelpText(QObject::tr("Offset (in minutes) to apply to the program "
                    "guide data during import.  This can be used when the "
                    "listings for a particular channel are in a different "
                    "time zone.") + " " +
                    QObject::tr("(Works for DataDirect listings only.)"));
    }
};

class Priority : public SpinBoxSetting, public ChannelDBStorage
{
  public:
    Priority(const ChannelID &id) :
        SpinBoxSetting(this, -99, 99, 1),
        ChannelDBStorage(this, id, "recpriority")
    {
        setLabel(QObject::tr("Priority"));
        setHelpText(
            QObject::tr("Number of priority points to be added to any "
                        "recording on this channel during scheduling.")+" "+
            QObject::tr("Use a positive number as the priority if you "
                        "want this to be a preferred channel, a "
                        "negative one to deprecate this channel."));
    }
};

class Icon : public LineEditSetting, public ChannelDBStorage
{
  public:
    Icon(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "icon")
    {
        setLabel(QObject::tr("Icon"));
        setHelpText(QObject::tr("Image file to use as the icon for this "
                                "channel on various MythTV displays."));
    }
};

class VideoFilters : public LineEditSetting, public ChannelDBStorage
{
  public:
    VideoFilters(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "videofilters")
    {
        setLabel(QObject::tr("Video filters"));
        setHelpText(QObject::tr("Filters to be used when recording "
                                "from this channel.  Not used with "
                                "hardware encoding cards."));

    }
};


class OutputFilters : public LineEditSetting, public ChannelDBStorage
{
  public:
    OutputFilters(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "outputfilters")
    {
        setLabel(QObject::tr("Playback filters"));
        setHelpText(QObject::tr("Filters to be used when recordings "
                                "from this channel are viewed.  "
                                "Start with a plus to append to the "
                                "global playback filters."));
    }
};


class XmltvID : public LineEditSetting, public ChannelDBStorage
{
  public:
    XmltvID(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "xmltvid")
    {
        setLabel(QObject::tr("XMLTV ID"));
        setHelpText(QObject::tr(
                        "ID used by listing services to get an exact "
                        "correspondance between a channel in your line-up "
                        "and a channel in their database. Normally this is "
                        "set automatically when 'mythfilldatabase' is run."));
    }
};

class CommMethod : public ComboBoxSetting, public ChannelDBStorage
{
  public:
    CommMethod(const ChannelID &id) :
       ComboBoxSetting(this), ChannelDBStorage(this, id, "commmethod")
    {
        setLabel(QObject::tr("Commercial Detection Method"));
        setHelpText(QObject::tr("Changes the method of "
               "commercial detection used for recordings on this channel or "
               "skips detection by marking the channel as Commercial Free."));

        deque<int> tmp = GetPreferredSkipTypeCombinations();
        tmp.push_front(COMM_DETECT_UNINIT);
        tmp.push_back(COMM_DETECT_COMMFREE);

        for (uint i = 0; i < tmp.size(); i++)
            addSelection(SkipTypeToString(tmp[i]), QString::number(tmp[i]));
    }
};

class Visible : public CheckBoxSetting, public ChannelDBStorage
{
  public:
    Visible(const ChannelID &id) :
        CheckBoxSetting(this), ChannelDBStorage(this, id, "visible")
    {
        setValue(true);
        setLabel(QObject::tr("Visible"));
        setHelpText(QObject::tr("If set, the channel will be visible in the "
                    "EPG."));
    }
};

class OnAirGuide : public CheckBoxSetting, public ChannelDBStorage
{
  public:
    OnAirGuide(const ChannelID &id) :
        CheckBoxSetting(this), ChannelDBStorage(this, id, "useonairguide")
    {
        setLabel(QObject::tr("Use on air guide"));
        setHelpText(QObject::tr("If set the guide information will be taken "
                    "from the On Air Channel guide."));
    }
};

/*****************************************************************************
        Channel Options - Video 4 Linux
 *****************************************************************************/

class Freqid : public LineEditSetting, public ChannelDBStorage
{
  public:
    Freqid(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "freqid")
    {
        setLabel(QObject::tr("Frequency")+" "+QObject::tr("or")+" "+
                 QObject::tr("Channel"));
        setHelpText(QObject::tr(
                        "Specify either the exact frequency in kHz or "
                        "a valid channel for your 'TV Format'."));
    }
};

class Finetune : public SliderSetting, public ChannelDBStorage
{
  public:
    Finetune(const ChannelID& id)
        : SliderSetting(this, -300, 300, 1),
        ChannelDBStorage(this, id, "finetune")
    {
        setLabel(QObject::tr("Finetune")+" (kHz)");
        setHelpText(QObject::tr("Value to be added to your desired frequency "
                                "in kHz, for 'fine tuning'."));
    }
};

class Contrast : public SliderSetting, public ChannelDBStorage
{
  public:
    Contrast(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655),
        ChannelDBStorage(this, id, "contrast")
    {
        setLabel(QObject::tr("Contrast"));
    }
};

class Brightness : public SliderSetting, public ChannelDBStorage
{
  public:
    Brightness(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655),
        ChannelDBStorage(this, id, "brightness")
    {
        setLabel(QObject::tr("Brightness"));
    }
};

class Colour : public SliderSetting, public ChannelDBStorage
{
  public:
    Colour(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655),
        ChannelDBStorage(this, id, "colour")
    {
        setLabel(QObject::tr("Color"));
    }
};

class Hue : public SliderSetting, public ChannelDBStorage
{
  public:
    Hue(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655), ChannelDBStorage(this, id, "hue")
    {
        setLabel(QObject::tr("Hue"));
    }
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID &id,
                                           uint default_sourceid) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Channel Options - Common"));
    setUseLabel(false);

    addChild(new Name(id));

    Source *source = new Source(id, default_sourceid);

    HorizontalConfigurationGroup *group1 =
        new HorizontalConfigurationGroup(false,false,true,true);
    HorizontalConfigurationGroup *bottomhoz =
        new HorizontalConfigurationGroup(false, true);
    VerticalConfigurationGroup *left =
        new VerticalConfigurationGroup(false, true);
    VerticalConfigurationGroup *right =
        new VerticalConfigurationGroup(false, true);


    left->addChild(new Channum(id));
    left->addChild(new Callsign(id));
    left->addChild(new Visible(id));

    right->addChild(source);
    right->addChild(new ChannelTVFormat(id));
    right->addChild(new Priority(id));

    group1->addChild(left);
    group1->addChild(right);

    bottomhoz->addChild(onairguide = new OnAirGuide(id));
    bottomhoz->addChild(xmltvID = new XmltvID(id));
    bottomhoz->addChild(new TimeOffset(id));

    addChild(group1);
    addChild(new CommMethod(id));
    addChild(new Icon(id));
    addChild(bottomhoz);

    connect(onairguide, SIGNAL(valueChanged(     bool)),
            this,       SLOT(  onAirGuideChanged(bool)));
    connect(source,     SIGNAL(valueChanged( const QString&)),
            this,       SLOT(  sourceChanged(const QString&)));
};

void ChannelOptionsCommon::Load(void)
{
    VerticalConfigurationGroup::Load();
}

void ChannelOptionsCommon::onAirGuideChanged(bool fValue)
{
    (void)fValue;
}

void ChannelOptionsCommon::sourceChanged(const QString& sourceid)
{
    bool supports_eit  = true;
    bool uses_eit_only = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard, videosource, cardinput "
                  "WHERE cardinput.sourceid   = videosource.sourceid AND "
                  "      cardinput.cardid     = capturecard.cardid   AND "
                  "      videosource.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("sourceChanged -- supports eit", query);
    else
    {
        supports_eit = (query.size()) ? false : true;
        while (query.next())
        {
            supports_eit |= CardUtil::IsEITCapable(
                query.value(0).toString().toUpper());
        }

        query.prepare("SELECT xmltvgrabber "
                      "FROM videosource "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", sourceid);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("sourceChanged -- eit only", query);
        else
        {
            uses_eit_only = (query.size()) ? true : false;
            while (query.next())
            {
                uses_eit_only &= (query.value(0).toString() == "eitonly");
            }
        }
    }

    onairguide->setEnabled(supports_eit);
    xmltvID->setEnabled(!uses_eit_only);
}

ChannelOptionsFilters::ChannelOptionsFilters(const ChannelID& id) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Channel Options - Filters"));
    setUseLabel(false);

    addChild(new VideoFilters(id));
    addChild(new OutputFilters(id));
}

ChannelOptionsV4L::ChannelOptionsV4L(const ChannelID& id) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Channel Options - Video 4 Linux"));
    setUseLabel(false);

    addChild(new Freqid(id));
    addChild(new Finetune(id));
    addChild(new Contrast(id));
    addChild(new Brightness(id));
    addChild(new Colour(id));
    addChild(new Hue(id));
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
