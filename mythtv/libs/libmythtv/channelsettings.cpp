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
        setLabel("Channel Name");
    };
};

class Channum: public LineEditSetting, public CSetting {
public:
    Channum(const ChannelID& id):
        LineEditSetting(), CSetting(id, "channum") {
        setLabel("Channel Number");
    };
};

class Source: public ComboBoxSetting, public CSetting {
public:
    Source(const ChannelID& id):
        ComboBoxSetting(), CSetting(id, "sourceid") {
        setLabel("Video Source");
    };

    void load(QSqlDatabase* db) {
        fillSelections(db);
        CSetting::load(db);
    };

    void fillSelections(QSqlDatabase* db) {
        addSelection("[No Source Selected]", "0");

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
        setLabel("Callsign");
    };
};

class Rank: public SpinBoxSetting, public CSetting {
public:
    Rank(const ChannelID& id):
        SpinBoxSetting(0,256,1), CSetting(id, "rank") {
        setLabel("Rank");
    };
};

class Icon: public LineEditSetting, public CSetting {
public:
    Icon(const ChannelID& id):
        LineEditSetting(), CSetting(id, "icon") {
        setLabel("Icon");
    };
};

class VideoFilters: public LineEditSetting, public CSetting {
public:
    VideoFilters(const ChannelID& id):
        LineEditSetting(), CSetting(id, "videofilters") {
        setLabel("Video filters");
    };
};

class XmltvID: public LineEditSetting, public CSetting {
public:
    XmltvID(const ChannelID& id):
        LineEditSetting(), CSetting(id, "xmltvid") {
        setLabel("XMLTV ID");
    };
};

/*****************************************************************************
        Channel Options - Video 4 Linux
 *****************************************************************************/

class Contrast: public SliderSetting, public CSetting {
public:
    Contrast(const ChannelID& id):
        SliderSetting(0,32768,1), CSetting(id, "contrast") {
        setLabel("Contrast");
    };
};

class Freqid: public LineEditSetting, public CSetting {
public:
    Freqid(const ChannelID& id):
        LineEditSetting(), CSetting(id, "freqid") {
        setLabel("Frequency ID");
    };
};

class Finetune: public SliderSetting, public CSetting {
public:
    Finetune(const ChannelID& id):
        SliderSetting(0,32768,1), CSetting(id, "finetune") {
        setLabel("Finetune");
    };
};

class Brightness: public SliderSetting, public CSetting {
public:
    Brightness(const ChannelID& id):
        SliderSetting(0,32768,1), CSetting(id, "brightness") {
        setLabel("Brightness");
    };
};

class Colour: public SliderSetting, public CSetting {
public:
    Colour(const ChannelID& id):
        SliderSetting(0,32768,1), CSetting(id, "colour") {
        setLabel("Colour");
    };
};

class Hue: public SliderSetting, public CSetting {
public:
    Hue(const ChannelID& id):
        SliderSetting(0,32768,1), CSetting(id, "hue") {
        setLabel("Hue");
    };
};

/*****************************************************************************
        Channel Options - DVB
 *****************************************************************************/

class DvbFrequency: public LineEditSetting, public DvbSetting {
public:
    DvbFrequency(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "frequency") {
        setLabel("Frequency");
        setHelpText("[Common] Frequency (Option has no default)\n"
                    "The frequency for this channel in hz.");
    };
};

class DvbSymbolrate: public LineEditSetting, public DvbSetting {
public:
    DvbSymbolrate(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "symbolrate") {
        setLabel("Symbol Rate");
        setHelpText("[DVB-S/C] Symbol Rate (Option has no default)\n"
                    "???");
    };
};

class DvbSatellite: public ComboBoxSetting, DvbSetting {
public:
    DvbSatellite(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "satid") {
        setLabel("Satellite");
        setValue("0");
    };

    void load(QSqlDatabase* db) {
        fillSelections(db);
        DvbSetting::load(db);
    };

    void fillSelections(QSqlDatabase* db)
    {
        clearSelections();
        QSqlQuery query = db->exec(QString(
            "SELECT dvb_sat.name,dvb_sat.satid FROM dvb_sat,channel"
            " WHERE dvb_sat.sourceid=channel.sourceid AND channel.chanid='%1'")
            .arg(id.getValue()));
        if (!query.isActive())
            MythContext::DBError("DvbSatellite::fillSelections", query);

        if (query.numRowsAffected() > 0)
        {
            while (query.next())
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
        } else
            addSelection("[NO SATELLITES CONFIGURED]", "0");
    };
};

class DvbPolarity: public ComboBoxSetting, public DvbSetting {
public:
    DvbPolarity(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "polarity") {
        setLabel("Polarity");
        setHelpText("[DVB-S] Polarity (Option has no default)\n"
                    "???");
        addSelection("Horizontal", "h");
        addSelection("Vertical", "v");
        addSelection("Right Circular", "r");
        addSelection("Left Circular", "l");
    };
};

class DvbInversion: public ComboBoxSetting, public DvbSetting {
public:
    DvbInversion(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "inversion") {
        setLabel("Inversion");
        setHelpText("[Common] Inversion (Default: Auto):\n"
                    "Most cards can autodetect this now, so leave it at Auto"
                    " unless it won't work.");
        addSelection("Auto", "a");
        addSelection("On", "1");
        addSelection("Off", "0");
    };
};

class DvbBandwidth: public ComboBoxSetting, public DvbSetting {
public:
    DvbBandwidth(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "bandwidth") {
        setLabel("Bandwidth");
        setHelpText("[DVB-C/T] Bandwidth (Default: Auto)\n"
                    "???");
        addSelection("Auto","a");
        addSelection("6 Mhz","6");
        addSelection("7 Mhz","7");
        addSelection("8 Mhz","8");
    };
};

class DvbModulationSetting: public ComboBoxSetting {
public:
    DvbModulationSetting() {
        addSelection("Auto","auto");
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
        setLabel("Modulation");
        setHelpText("[DVB-C] Modulation (Default: Auto)\n"
                    "???");
    };
};

class DvbConstellation: public DvbModulationSetting, public DvbSetting {
public:
    DvbConstellation(const ChannelID& id):
        DvbModulationSetting(), DvbSetting(id, "modulation") {
        setLabel("Constellation");
        setHelpText("[DVB-T] Constellation (Default: Auto)\n"
                    "???");
    };
};

class DvbFecSetting: public ComboBoxSetting {
public:
    DvbFecSetting(): ComboBoxSetting() {
        addSelection("Auto","auto");
        addSelection("None","none");
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
        setLabel("FEC");
        setHelpText("[DVB-S/C] Forward Error Correction (Default: Auto)\n"
                    "???");
    };
};

class DvbCoderateLP: public DvbFecSetting, public DvbSetting {
public:
    DvbCoderateLP(const ChannelID& id):
        DvbFecSetting(), DvbSetting(id, "lp_code_rate") {
        setLabel("LP Coderate");
        setHelpText("[DVB-T] Low Priority Code Rate (Default: Auto)\n"
                    "???");
    };
};

class DvbCoderateHP: public DvbFecSetting, public DvbSetting {
public:
    DvbCoderateHP(const ChannelID& id):
        DvbFecSetting(), DvbSetting(id, "fec") {
        setLabel("HP Coderate");
        setHelpText("[DVB-T] High Priority Code Rate (Default: Auto)\n"
                    "???");
    };
};

class DvbGuardInterval: public ComboBoxSetting, public DvbSetting {
public:
    DvbGuardInterval(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "guard_interval") {
        setLabel("Guard Interval");
        setHelpText("[DVB-T] Guard Interval (Default: Auto)\n"
                    "???");
        addSelection("Auto","auto");
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
        setLabel("Trans. Mode");
        setHelpText("[DVB-T] Transmission Mode (Default: Auto)\n"
                    "???");
        addSelection("Auto","a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class DvbHierarchy: public ComboBoxSetting, public DvbSetting {
public:
    DvbHierarchy(const ChannelID& id):
        ComboBoxSetting(), DvbSetting(id, "hierarchy") {
        setLabel("Hierarchy");
        setHelpText("[DVB-T] Hierarchy (Default: Auto)\n"
                    "???");
        addSelection("Auto","a");
        addSelection("None", "n");
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
        setLabel("Video");
    };
};

class DvbAudioPids: public DvbPidSetting {
public:
    DvbAudioPids(const ChannelID& id): DvbPidSetting(id, "a") {
        setLabel("Audio");
    };
};

class DvbTeletextPids: public DvbPidSetting {
public:
    DvbTeletextPids(const ChannelID& id): DvbPidSetting(id, "t") {
        setLabel("Teletext");
    };
};

class DvbSubtitlePids: public DvbPidSetting {
public:
    DvbSubtitlePids(const ChannelID& id): DvbPidSetting(id, "s") {
        setLabel("Subtitle");
    };
};

class DvbPcrPids: public DvbPidSetting {
public:
    DvbPcrPids(const ChannelID& id): DvbPidSetting(id, "p") {
        setLabel("Pcr");
    };
};

class DvbOtherPids: public DvbPidSetting {
public:
    DvbOtherPids(const ChannelID& id): DvbPidSetting(id, "o") {
        setLabel("Other");
    };
};

class DvbServiceID: public LineEditSetting, public DvbSetting {
public:
    DvbServiceID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "serviceid") {
        setLabel("Service ID");
    };
};

class DvbProviderID: public LineEditSetting, public DvbSetting {
public:
    DvbProviderID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "providerid") {
        setLabel("Provider ID");
    };
};

class DvbTransportID: public LineEditSetting, public DvbSetting {
public:
    DvbTransportID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "transportid") {
        setLabel("Transport ID");
    };
};

class DvbNetworkID: public LineEditSetting, public DvbSetting {
public:
    DvbNetworkID(const ChannelID& id):
        LineEditSetting(), DvbSetting(id, "networkid") {
        setLabel("Network ID");
    };
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID& id)
                    : VerticalConfigurationGroup(false,true) {
    setLabel("Channel Options - Common");
    setUseLabel(false);

    addChild(new Name(id));

    HorizontalConfigurationGroup* group1 = new HorizontalConfigurationGroup(false,true);

    VerticalConfigurationGroup* left = new VerticalConfigurationGroup(false,true);
    left->addChild(new Channum(id));
    left->addChild(new Callsign(id));
    group1->addChild(left);

    VerticalConfigurationGroup* right = new VerticalConfigurationGroup(false, true);
    right->addChild(new Source(id));
    right->addChild(new Rank(id));
    group1->addChild(right);

    addChild(group1);

    addChild(new Icon(id));
    addChild(new VideoFilters(id));
    addChild(new XmltvID(id));
};

ChannelOptionsV4L::ChannelOptionsV4L(const ChannelID& id)
                 : VerticalConfigurationGroup(false,true) {
    setLabel("Channel Options - Video 4 Linux");
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
    setLabel("Channel Options - DVB");
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
    setLabel("Channel Options - DVB Pids & IDs");
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

