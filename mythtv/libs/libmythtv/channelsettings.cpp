#include "channelsettings.h"

QString CSetting::whereClause(void) {
    return QString("%1=%2").arg(id.getField()).arg(id.getValue());
}

QString CSetting::setClause(void) {
    return QString("%1=%2, %3='%4'").arg(id.getField()).arg(id.getValue())
                   .arg(getName()).arg(getValue());
}

QString DvbSetting::whereClause(void) {
    return QString("%1=%2").arg(id.getField()).arg(id.getValue());
}

QString DvbSetting::setClause(void) {
    return QString("%1=%2, %3='%4'").arg(id.getField()).arg(id.getValue())
                   .arg(getName()).arg(getValue());
}

void DvbPidSetting::load(QSqlDatabase* db) {
    QString value;
    QSqlQuery query = db->exec(QString("SELECT pid FROM %1 WHERE chanid='%2' AND type='%3'")
                                       .arg(getTable()).arg(id.getValue()).arg(getColumn()));
    if (query.isActive() && query.numRowsAffected() > 0)
        while(query.next()) {
            value += query.value(0).toString();
            if (!((query.at()+1) == query.numRowsAffected()))
                value += ",";
        }
    setValue(value);
    setUnchanged();
}

void DvbPidSetting::save(QSqlDatabase* db) {
    if (!isChanged())
        return;
    QStringList list = QStringList::split(",", getValue());
    QStringList::iterator iter = list.begin();
    QString querystr = QString("DELETE FROM %1 WHERE chanid='%2' AND type='%3'")
        .arg(getTable()).arg(id.getValue()).arg(getColumn());
    QSqlQuery query = db->exec(querystr);
    while(iter != list.end())
    {
        querystr = QString("INSERT INTO %1 SET pid='%2', chanid='%3', type='%4'")
            .arg(getTable()).arg((*iter).toInt()).arg(id.getValue()).arg(getColumn());
        query = db->exec(querystr);
        if (!query.isActive())
            MythContext::DBError("DvbPidSetting Insert", querystr);
        iter++;
    }
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

    void load(QSqlDatabase* db) {
        fillSelections(db);
        CSetting::load(db);
    };

    void fillSelections(QSqlDatabase* db) {
        addSelection(QObject::tr("[Not Selected]"), "0");

        QSqlQuery query = db->exec(QString(
            "SELECT name, sourceid FROM videosource"));
        if (query.isActive() && query.numRowsAffected() > 0)
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

/*****************************************************************************
        Channel Options - DVB
 *****************************************************************************/

class DvbFrequency: public LineEditSetting, public DvbSetting {
public:
    DvbFrequency(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "frequency") {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("[Common] Frequency (Option has no default)\n"
                    "The frequency for this channel in hz."));
    };
};

class DvbSymbolrate: public LineEditSetting, public DvbSetting {
public:
    DvbSymbolrate(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "symbolrate") {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(QObject::tr("[DVB-S/C] Symbol Rate (Option has no default)"
                    "\n???"));
    };
};

class DvbSatellite: public ComboBoxSetting, DvbSetting {
public:
    DvbSatellite(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "satid") {
        setLabel(QObject::tr("Satellite"));
    };

    void load(QSqlDatabase* db) {
        fillSelections(db);
        DvbSetting::load(db);
    };

    void fillSelections(QSqlDatabase* db)
    {
        clearSelections();
        QSqlQuery query = db->exec(QString(
            "SELECT dvb_sat.name,dvb_sat.satid FROM dvb_sat,channel,cardinput"
            " WHERE dvb_sat.cardid=cardinput.cardid"
            " AND cardinput.sourceid=channel.sourceid"
            " AND channel.chanid='%1'")
            .arg(id.getValue()));
        if (!query.isActive())
            MythContext::DBError("DvbSatellite::fillSelections", query);

        if (query.numRowsAffected() > 0)
        {
            while (query.next())
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
        } else
            addSelection(QObject::tr("[NONE CONFIGURED]"), "0");
    };
};

class DvbPolarity: public ComboBoxSetting, public DvbSetting {
public:
    DvbPolarity(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "polarity") {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("[DVB-S] Polarity (Option has no default)\n"
                    "???"));
        addSelection(QObject::tr("Horizontal"), "h");
        addSelection(QObject::tr("Vertical"), "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"), "l");
    };
};

class DvbInversion: public ComboBoxSetting, public DvbSetting {
public:
    DvbInversion(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "inversion") {
        setLabel(QObject::tr("Inversion"));
        setHelpText(QObject::tr("[Common] Inversion (Default: Auto):\n"
                    "Most cards can autodetect this now, so leave it at Auto"
                    " unless it won't work."));
        addSelection(QObject::tr("Auto"), "a");
        addSelection(QObject::tr("On"), "1");
        addSelection(QObject::tr("Off"), "0");
    };
};

class DvbBandwidth: public ComboBoxSetting, public DvbSetting {
public:
    DvbBandwidth(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "bandwidth") {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("[DVB-C/T] Bandwidth (Default: Auto)\n"
                    "???"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("6 Mhz"),"6");
        addSelection(QObject::tr("7 Mhz"),"7");
        addSelection(QObject::tr("8 Mhz"),"8");
    };
};

class DvbModulationSetting: public ComboBoxSetting {
public:
    DvbModulationSetting() {
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("QPSK","qpsk");
        addSelection("QAM 16","qam_16");
        addSelection("QAM 32","qam_32");
        addSelection("QAM 64","qam_64");
        addSelection("QAM 128","qam_128");
        addSelection("QAM 256","qam_256");
    };
};

class DvbModulation: public DvbModulationSetting, public DvbSetting {
public:
    DvbModulation(const ChannelID& id):
        DvbModulationSetting(), DvbSetting(id, "modulation") {
        setLabel(QObject::tr("Modulation"));
        setHelpText(QObject::tr("[DVB-C] Modulation (Default: Auto)\n"
                    "???"));
    };
};

class DvbConstellation: public DvbModulationSetting, public DvbSetting {
public:
    DvbConstellation(const ChannelID& id):
        DvbModulationSetting(), DvbSetting(id, "modulation") {
        setLabel(QObject::tr("Constellation"));
        setHelpText(QObject::tr("[DVB-T] Constellation (Default: Auto)\n"
                    "???"));
    };
};

class DvbFecSetting: public ComboBoxSetting {
public:
    DvbFecSetting(): ComboBoxSetting() {
        addSelection(QObject::tr("Auto"),"auto");
        addSelection(QObject::tr("None"),"none");
        addSelection("1/2");
        addSelection("2/3");
        addSelection("3/4");
        addSelection("4/5");
        addSelection("5/6");
        addSelection("6/7");
        addSelection("7/8");
        addSelection("8/9");
    };
};

class DvbFec: public DvbFecSetting, public DvbSetting {
public:
    DvbFec(const ChannelID& id):
        DvbFecSetting(), DvbSetting(id, "fec") {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr("[DVB-S/C] Forward Error Correction "
                    "(Default: Auto)\n???"));
    };
};

class DvbCoderateLP: public DvbFecSetting, public DvbSetting {
public:
    DvbCoderateLP(const ChannelID& id):
        DvbFecSetting(), DvbSetting(id, "lp_code_rate") {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("[DVB-T] Low Priority Code Rate "
                    "(Default: Auto)\n???"));
    };
};

class DvbCoderateHP: public DvbFecSetting, public DvbSetting {
public:
    DvbCoderateHP(const ChannelID& id):
        DvbFecSetting(), DvbSetting(id, "fec") {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("[DVB-T] High Priority Code Rate "
                    "(Default: Auto)\n???"));
    };
};

class DvbGuardInterval: public ComboBoxSetting, public DvbSetting {
public:
    DvbGuardInterval(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "guard_interval") {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("[DVB-T] Guard Interval (Default: Auto)\n"
                    "???"));
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class DvbTransmissionMode: public ComboBoxSetting, public DvbSetting {
public:
    DvbTransmissionMode(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "transmission_mode") {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("[DVB-T] Transmission Mode (Default: Auto)\n"
                    "???"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class DvbHierarchy: public ComboBoxSetting, public DvbSetting {
public:
    DvbHierarchy(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "hierarchy") {
        setLabel(QObject::tr("Hierarchy"));
        setHelpText(QObject::tr("[DVB-T] Hierarchy (Default: Auto)\n"
                    "???"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("None"), "n");
        addSelection("1");
        addSelection("2");
        addSelection("4");
    };
};
/*****************************************************************************
        Channel Options - DVB Pids & IDs
 *****************************************************************************/

class DvbVideoPids: public DvbPidSetting {
public:
    DvbVideoPids(const ChannelID& id): DvbPidSetting(id, "v") {
        setLabel(QObject::tr("Video"));
    };
};

class DvbAudioPids: public DvbPidSetting {
public:
    DvbAudioPids(const ChannelID& id): DvbPidSetting(id, "a") {
        setLabel(QObject::tr("Audio"));
    };
};

class DvbTeletextPids: public DvbPidSetting {
public:
    DvbTeletextPids(const ChannelID& id): DvbPidSetting(id, "t") {
        setLabel(QObject::tr("Teletext"));
    };
};

class DvbSubtitlePids: public DvbPidSetting {
public:
    DvbSubtitlePids(const ChannelID& id): DvbPidSetting(id, "s") {
        setLabel(QObject::tr("Subtitle"));
    };
};

class DvbPcrPids: public DvbPidSetting {
public:
    DvbPcrPids(const ChannelID& id): DvbPidSetting(id, "p") {
        setLabel(QObject::tr("Pcr"));
    };
};

class DvbOtherPids: public DvbPidSetting {
public:
    DvbOtherPids(const ChannelID& id): DvbPidSetting(id, "o") {
        setLabel(QObject::tr("Other"));
    };
};

class DvbServiceID: public LineEditSetting, public DvbSetting {
public:
    DvbServiceID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "serviceid") {
        setLabel(QObject::tr("Service ID"));
    };
};

class DvbProviderID: public LineEditSetting, public DvbSetting {
public:
    DvbProviderID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "providerid") {
        setLabel(QObject::tr("Provider ID"));
    };
};

class DvbTransportID: public LineEditSetting, public DvbSetting {
public:
    DvbTransportID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "transportid") {
        setLabel(QObject::tr("Transport ID"));
    };
};

class DvbNetworkID: public LineEditSetting, public DvbSetting {
public:
    DvbNetworkID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "networkid") {
        setLabel(QObject::tr("Network ID"));
    };
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID& id)
                    : VerticalConfigurationGroup(false,true) {
    setLabel(QObject::tr("Channel Options - Common"));
    setUseLabel(false);

    addChild(new Name(id));

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
    right->addChild(new Source(id));
    right->addChild(new ChannelTVFormat(id));
    right->addChild(new Rank(id));
    group1->addChild(right);

    addChild(group1);

    addChild(new Icon(id));
    addChild(new VideoFilters(id));
    addChild(new OutputFilters(id));
    addChild(new XmltvID(id));
};

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

ChannelOptionsDVB::ChannelOptionsDVB(const ChannelID& id)
                 : HorizontalConfigurationGroup(false,true), id(id) 
{
    setLabel(QObject::tr("Channel Options - DVB"));
    setUseLabel(false);

    VerticalConfigurationGroup* left = new VerticalConfigurationGroup(false,true);
    left->addChild(frequency = new DvbFrequency(id));
    left->addChild(symbolrate = new DvbSymbolrate(id));
    left->addChild(satellite = new DvbSatellite(id));
    left->addChild(polarity = new DvbPolarity(id));
    left->addChild(fec = new DvbFec(id));
    left->addChild(modulation = new DvbModulation(id));
    left->addChild(inversion = new DvbInversion(id));
    addChild(left);

    VerticalConfigurationGroup* right = new VerticalConfigurationGroup(false,true);
    right->addChild(bandwidth = new DvbBandwidth(id));
    right->addChild(constellation = new DvbConstellation(id));
    right->addChild(coderate_lp = new DvbCoderateLP(id));
    right->addChild(coderate_hp = new DvbCoderateHP(id));
    right->addChild(trans_mode = new DvbTransmissionMode(id));
    right->addChild(guard_interval = new DvbGuardInterval(id));
    right->addChild(hierarchy = new DvbHierarchy(id));
    addChild(right);
};

QString ChannelOptionsDVB::getFrequency() { return frequency->getValue(); };
QString ChannelOptionsDVB::getSymbolrate() { return symbolrate->getValue(); };
QString ChannelOptionsDVB::getPolarity() { return polarity->getValue(); };
QString ChannelOptionsDVB::getFec() { return fec->getValue(); };
QString ChannelOptionsDVB::getModulation() { return modulation->getValue(); };
QString ChannelOptionsDVB::getInversion() { return inversion->getValue(); };
 
QString ChannelOptionsDVB::getBandwidth() { return bandwidth->getValue(); };
QString ChannelOptionsDVB::getConstellation() { return constellation->getValue(); };
QString ChannelOptionsDVB::getCodeRateLP() { return coderate_lp->getValue(); };
QString ChannelOptionsDVB::getCodeRateHP() { return coderate_hp->getValue(); };
QString ChannelOptionsDVB::getTransmissionMode() { return trans_mode->getValue(); };
QString ChannelOptionsDVB::getGuardInterval() { return guard_interval->getValue(); };
QString ChannelOptionsDVB::getHierarchy() { return hierarchy->getValue(); };


ChannelOptionsDVBPids::ChannelOptionsDVBPids(const ChannelID& id)
                     : HorizontalConfigurationGroup(false,true) {
    setLabel(QObject::tr("Channel Options - DVB Pids & IDs"));
    setUseLabel(false);

    VerticalConfigurationGroup* left = new VerticalConfigurationGroup(false,true);
    left->addChild(new DvbVideoPids(id));
    left->addChild(new DvbAudioPids(id));
    left->addChild(new DvbTeletextPids(id));
    left->addChild(new DvbSubtitlePids(id));
    left->addChild(new DvbPcrPids(id));
    left->addChild(new DvbOtherPids(id));
    addChild(left);

    VerticalConfigurationGroup* right = new VerticalConfigurationGroup(false,true);
    right->addChild(new DvbServiceID(id));
    right->addChild(new DvbProviderID(id));
    right->addChild(new DvbTransportID(id));
    right->addChild(new DvbNetworkID(id));
    addChild(right);
};

