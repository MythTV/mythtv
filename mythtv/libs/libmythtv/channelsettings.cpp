#include "channelsettings.h"

QString CSetting::whereClause(void) {
    return QString("%1=%2").arg(id.getField()).arg(id.getValue());
}

QString CSetting::setClause(void) {
    return QString("%1=%2, %3='%4'").arg(id.getField()).arg(id.getValue())
                   .arg(getName()).arg(getValue());
}

/*****************************************************************************
        Channel Options - Common
 *****************************************************************************/

class Name: public LineEditSetting, public CSetting {
public:
    Name(const ChannelID& id):
        LineEditSetting(), CSetting(id, "name") {
        setLabel(QObject::tr("Channel Name"));
    };
};

class Channum: public LineEditSetting, public CSetting {
public:
    Channum(const ChannelID& id):
        LineEditSetting(), CSetting(id, "channum") {
        setLabel(QObject::tr("Channel Number"));
    };
};

class Source: public ComboBoxSetting, public CSetting {
public:
    Source(const ChannelID& id):
        ComboBoxSetting(), CSetting(id, "sourceid") {
        setLabel(QObject::tr("Video Source"));
    };

    void load() {
        fillSelections();
        CSetting::load();
    };

    void fillSelections() 
    {
        addSelection(QObject::tr("[Not Selected]"), "0");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT name, sourceid FROM videosource");
        
        if (query.exec() && query.isActive() && query.size() > 0)
            while(query.next())
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
    };
};

class Callsign: public LineEditSetting, public CSetting {
public:
    Callsign(const ChannelID& id):
        LineEditSetting(), CSetting(id, "callsign") {
        setLabel(QObject::tr("Callsign"));
    };
};

class ChannelTVFormat: public ComboBoxSetting, public CSetting {
public:
    ChannelTVFormat(const ChannelID& id):
       ComboBoxSetting(), CSetting(id, "tvformat") {
       setLabel(QObject::tr("TV Format"));
       setHelpText(QObject::tr("If this channel uses a format other than TV "
                   "Format in the General Backend Setup screen, set it here."));
       addSelection("Default");
       addSelection("NTSC");
       addSelection("ATSC");
       addSelection("PAL");
       addSelection("SECAM");
       addSelection("PAL-NC");
       addSelection("PAL-M");
       addSelection("PAL-N");
       addSelection("NTSC-JP");
    };
};

class Rank: public SpinBoxSetting, public CSetting {
public:
    Rank(const ChannelID& id):
        SpinBoxSetting(-99,99,1), CSetting(id, "rank") {
        setLabel(QObject::tr("Rank"));
    };
};

class Icon: public LineEditSetting, public CSetting {
public:
    Icon(const ChannelID& id):
        LineEditSetting(), CSetting(id, "icon") {
        setLabel(QObject::tr("Icon"));
    };
};

class VideoFilters: public LineEditSetting, public CSetting {
public:
    VideoFilters(const ChannelID& id):
        LineEditSetting(), CSetting(id, "videofilters") {
        setLabel(QObject::tr("Video filters"));
        setHelpText(QObject::tr("Filters to be used when recording "
                                "from this channel.  Not used with "
                                "hardware encoding cards."));

    };
};


class OutputFilters: public LineEditSetting, public CSetting {
public:
    OutputFilters(const ChannelID& id):
        LineEditSetting(), CSetting(id, "outputfilters") {
        setLabel(QObject::tr("Playback filters"));
        setHelpText(QObject::tr("Filters to be used when recordings "
                                "from this channel are viewed.  "
                                "Start with a plus to append to the "
                                "global playback filters."));
    };
};


class XmltvID: public LineEditSetting, public CSetting {
public:
    XmltvID(const ChannelID& id):
        LineEditSetting(), CSetting(id, "xmltvid") {
        setLabel(QObject::tr("XMLTV ID"));
    };
};

class CommFree: public CheckBoxSetting, public CSetting {
public:
    CommFree(const ChannelID& id):
        CheckBoxSetting(), CSetting(id, "commfree") {
        setLabel(QObject::tr("Commercial Free"));
        setHelpText(QObject::tr("If set automatic commercial flagging will "
                "be skipped for this channel.  Useful for "
                "premium channels like HBO."));
    };
};

class Visible: public CheckBoxSetting, public CSetting {
public:
    Visible(const ChannelID& id):
        CheckBoxSetting(), CSetting(id, "visible") {
        setValue(true);
        setLabel(QObject::tr("Visible"));
        setHelpText(QObject::tr("If set, the channel will be visible in the "
                    "EPG."));
    };
};

class OnAirGuide: public CheckBoxSetting, public CSetting {
public:
    OnAirGuide(const ChannelID& id):
        CheckBoxSetting(), CSetting(id, "useonairguide") {
        setLabel(QObject::tr("Use on air guide"));
        setHelpText(QObject::tr("If set the guide information will be taken "
                    "from the On Air Channel guide."));
    };
};

/*****************************************************************************
        Channel Options - Video 4 Linux
 *****************************************************************************/

class Freqid: public LineEditSetting, public CSetting {
public:
    Freqid(const ChannelID& id):
        LineEditSetting(), CSetting(id, "freqid") {
        setLabel(QObject::tr("Frequency ID"));
    };
};

class Finetune: public SliderSetting, public CSetting {
public:
    Finetune(const ChannelID& id):
        SliderSetting(-200,200,1), CSetting(id, "finetune") {
        setLabel(QObject::tr("Finetune"));
    };
};

class Contrast: public SliderSetting, public CSetting {
public:
    Contrast(const ChannelID& id):
        SliderSetting(0,65535,655), CSetting(id, "contrast") {
        setLabel(QObject::tr("Contrast"));
    };
};

class Brightness: public SliderSetting, public CSetting {
public:
    Brightness(const ChannelID& id):
        SliderSetting(0,65535,655), CSetting(id, "brightness") {
        setLabel(QObject::tr("Brightness"));
    };
};

class Colour: public SliderSetting, public CSetting {
public:
    Colour(const ChannelID& id):
        SliderSetting(0,65535,655), CSetting(id, "colour") {
        setLabel(QObject::tr("Color"));
    };
};

class Hue: public SliderSetting, public CSetting {
public:
    Hue(const ChannelID& id):
        SliderSetting(0,65535,655), CSetting(id, "hue") {
        setLabel(QObject::tr("Hue"));
    };
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID& id)
                    : VerticalConfigurationGroup(false,true)
{
    setLabel(QObject::tr("Channel Options - Common"));
    setUseLabel(false);

    addChild(new Name(id));

    Source *source;

    HorizontalConfigurationGroup* group1 = new HorizontalConfigurationGroup(false,true);

    VerticalConfigurationGroup* left = new VerticalConfigurationGroup(false,true);
    left->addChild(new Channum(id));
    left->addChild(new Callsign(id));

    HorizontalConfigurationGroup *lefthoz = new HorizontalConfigurationGroup(false,true);

    lefthoz->addChild(new Visible(id));
    lefthoz->addChild(new CommFree(id));
    left->addChild(lefthoz);
    group1->addChild(left);

    VerticalConfigurationGroup* right = new VerticalConfigurationGroup(false, true);
    right->addChild(source = new Source(id));
    right->addChild(new ChannelTVFormat(id));
    right->addChild(new Rank(id));
    group1->addChild(right);

    addChild(group1);

    addChild(new Icon(id));
    addChild(new VideoFilters(id));
    addChild(new OutputFilters(id));

#ifdef USING_DVB_EIT
    HorizontalConfigurationGroup *bottomhoz = new HorizontalConfigurationGroup(false,true);
    bottomhoz->addChild(onairguide = new OnAirGuide(id));
    bottomhoz->addChild(xmltvID = new XmltvID(id));
    addChild(bottomhoz);

    connect(onairguide,SIGNAL(valueChanged(bool)),this,SLOT(onAirGuideChanged(bool)));
    connect(source,SIGNAL(valueChanged(const QString&)),this,SLOT(sourceChanged(const QString&)));
#else
    addChild(new XmltvID(id));
#endif
};

void ChannelOptionsCommon::load()
{
    VerticalConfigurationGroup::load();
}

void ChannelOptionsCommon::onAirGuideChanged(bool fValue)
{
    (void)fValue;
#ifdef USING_DVB
    xmltvID->setEnabled(!fValue);
#endif
}

void ChannelOptionsCommon::sourceChanged(const QString& str)
{
    (void)str;
#ifdef USING_DVB

    bool fDVB =false;

    MSqlQuery query(MSqlQuery::InitCon());

    QString queryStr = QString("SELECT count(cardtype) "
       "FROM capturecard,videosource,cardinput "
       "WHERE cardinput.sourceid=videosource.sourceid "
       "AND cardinput.cardid=capturecard.cardid "
       "AND capturecard.cardtype in (\"DVB\") "
       "AND videosource.sourceid=\"%1\" "
       "AND capturecard.hostname=\"%2\"").arg(str).arg(gContext->GetHostName());

    query.prepare(queryStr);
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        if (query.value(0).toInt())
           fDVB = true;
    }
    if (fDVB)
    {
       onairguide->setEnabled(true);
       xmltvID->setEnabled(!onairguide->boolValue());
    }
    else
    {
       onairguide->setEnabled(false);
       xmltvID->setEnabled(true);
    }
#endif
}

ChannelOptionsV4L::ChannelOptionsV4L(const ChannelID& id)
                 : VerticalConfigurationGroup(false,true) {
    setLabel(QObject::tr("Channel Options - Video 4 Linux"));
    setUseLabel(false);

    addChild(new Freqid(id));
    addChild(new Finetune(id));
    addChild(new Contrast(id));
    addChild(new Brightness(id));
    addChild(new Colour(id));
    addChild(new Hue(id));
};
