#include "videosource.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythcontext.h"
#include "datadirect.h"

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
#include <iostream>

#ifdef USING_DVB
#include <linux/dvb/frontend.h>
#include "dvbdev.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#if defined(CONFIG_VIDEO4LINUX)
#include "videodev_myth.h"
#endif

QString VSSetting::whereClause(void) {
    return QString("sourceid = %1").arg(parent.getSourceID());
}

QString VSSetting::setClause(void) {
    return QString("sourceid = %1, %2 = '%3'")
        .arg(parent.getSourceID())
        .arg(getColumn())
        .arg(getValue());
}

QString CCSetting::whereClause(void) {
    return QString("cardid = %1").arg(parent.getCardID());
}

QString CCSetting::setClause(void) {
    return QString("cardid = %1, %2 = '%3'")
        .arg(parent.getCardID())
        .arg(getColumn())
        .arg(getValue());
}

QString DvbSatSetting::whereClause(void) {
    return QString("cardid = %1 AND diseqc_port = %2")
        .arg(parent.getCardID()).arg(satnum);
}

QString DvbSatSetting::setClause(void) {
    return QString("cardid = %1, diseqc_port = %2, %3 = '%4'")
        .arg(parent.getCardID())
        .arg(satnum)
        .arg(getColumn())
        .arg(getValue());
}

class XMLTVGrabber: public ComboBoxSetting, public VSSetting {
public:
    XMLTVGrabber(const VideoSource& parent): 
        VSSetting(parent, "xmltvgrabber") {
        setLabel(QObject::tr("XMLTV listings grabber"));
    };
};

FreqTableSelector::FreqTableSelector(const VideoSource& parent) 
                 : VSSetting(parent, "freqtable")
{
    setLabel(QObject::tr("Channel frequency table"));
    addSelection("default");
    addSelection("us-cable");
    addSelection("us-bcast");
    addSelection("us-cable-hrc");
    addSelection("japan-bcast");
    addSelection("japan-cable");
    addSelection("europe-west");
    addSelection("europe-east");
    addSelection("italy");
    addSelection("newzealand");
    addSelection("australia");
    addSelection("ireland");
    addSelection("france");
    addSelection("china-bcast");
    addSelection("southafrica");
    addSelection("argentina");
    addSelection("australia-optus");
    setHelpText(QObject::tr("Use default unless this source uses a "
                "different frequency table than the system wide table "
                "defined in the General settings."));
}

class DataDirectUserID: public LineEditSetting, public VSSetting {
public:
    DataDirectUserID(const VideoSource& parent):
        VSSetting(parent, "userid") {
        setLabel(QObject::tr("User ID"));
    };
};

class DataDirectPassword: public LineEditSetting, public VSSetting {
public:
    DataDirectPassword(const VideoSource& parent):
        VSSetting(parent, "password") {
        setLabel(QObject::tr("Password"));
    };
};

void DataDirectLineupSelector::fillSelections(const QString &uid,
                                              const QString &pwd) 
{
    if (uid.isEmpty() || pwd.isEmpty())
        return;

    qApp->processEvents();

    QString waitMsg = tr("Fetching lineups from DataDirect service...");
    VERBOSE(VB_GENERAL, waitMsg);
    MythProgressDialog pdlg(waitMsg, 2);

    clearSelections();

    DataDirectProcessor ddp;
    ddp.setUserID(uid);
    ddp.setPassword(pwd);

    pdlg.setProgress(1);

    if (!ddp.grabLineupsOnly())
    {
        VERBOSE(VB_IMPORTANT, "DDLS: fillSelections did not successfully load selections");
        return;
    }
    QValueList<DataDirectLineup> lineups = ddp.getLineups();

    QValueList<DataDirectLineup>::iterator it;
    for (it = lineups.begin(); it != lineups.end(); ++it)
        addSelection((*it).displayname, (*it).lineupid);

    pdlg.setProgress(2);
    pdlg.Close();
}

void DataDirect_config::load(QSqlDatabase* db) 
{
    VerticalConfigurationGroup::load(db);
    if ((userid->getValue() != lastloadeduserid) || 
        (password->getValue() != lastloadedpassword)) 
    {
        lineupselector->fillSelections(userid->getValue(), 
                                       password->getValue());
        lastloadeduserid = userid->getValue();
        lastloadedpassword = password->getValue();
    }
}

DataDirect_config::DataDirect_config(const VideoSource& _parent)
                 : parent(_parent) 
{
    setUseLabel(false);
    setUseFrame(false);

//    setLabel(QObject::tr("DataDirect configuration"));

    HorizontalConfigurationGroup *lp = new HorizontalConfigurationGroup(false,
                                                                        false);

    lp->addChild((userid = new DataDirectUserID(parent)));
    lp->addChild((password = new DataDirectPassword(parent)));

    addChild(lp);

    addChild((button = new DataDirectButton()));
    addChild((lineupselector = new DataDirectLineupSelector(parent)));
    connect(button, SIGNAL(pressed()),
            this, SLOT(fillDataDirectLineupSelector()));
}

void DataDirect_config::fillDataDirectLineupSelector(void)
{
    lineupselector->fillSelections(userid->getValue(), password->getValue());
}

void RegionSelector::fillSelections() {
    clearSelections();

    QString command = QString("tv_grab_uk --configure --list-regions");
    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        perror(command.ascii());
        return;
    }

    QFile f;
    f.open(IO_ReadOnly, fp);
    for(QString line ; f.readLine(line, 1024) > 0 ; ) {
        addSelection(line.stripWhiteSpace());
    }

    f.close();
    fclose(fp);
}

void ProviderSelector::fillSelections(const QString& location) {
    QString waitMsg = QString("Fetching providers for %1... Please be patient.")
                             .arg(location);
    VERBOSE(VB_GENERAL, waitMsg);
    
    MythProgressDialog pdlg(waitMsg, 2);

    clearSelections();

    // First let the final character show up...
    qApp->processEvents();    
    
    // Now show our progress dialog.
    pdlg.show();

    QString command = QString("%1 --configure --postalcode %2 --list-providers")
        .arg(grabber)
        .arg(location);

    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        pdlg.Close();
        VERBOSE(VB_GENERAL, "Failed to retrieve provider list");

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                            QObject::tr("Failed to retrieve provider list"), 
                            QObject::tr("You probably need to update XMLTV."));
        qApp->processEvents();

        perror(command.ascii());
        return;
    }

    // Update our progress
    pdlg.setProgress(1);

    QFile f;
    f.open(IO_ReadOnly, fp);
    for(QString line ; f.readLine(line, 1024) > 0 ; ) {
        QStringList fields = QStringList::split(":", line.stripWhiteSpace());
        addSelection(fields.last(), fields.first());
    }

    pdlg.setProgress( 2 );
    pdlg.Close();

    f.close();
    fclose(fp);
}

XMLTV_uk_config::XMLTV_uk_config(const VideoSource& _parent)
               : VerticalConfigurationGroup(false, false), parent(_parent) 
{
    setLabel(QObject::tr("tv_grab_uk configuration"));
    region = new RegionSelector();
    addChild(region);

    provider = new ProviderSelector("tv_grab_uk");
    addChild(provider);

    connect(region, SIGNAL(valueChanged(const QString&)),
            provider, SLOT(fillSelections(const QString&)));
}

void XMLTV_uk_config::save(QSqlDatabase* db) {
    (void)db;

    QString waitMsg(QObject::tr("Please wait while MythTV retrieves the "
                                "list of available channels\n.  You "
                                "might want to check the output as it\n"
                                "runs by switching to the terminal from "
                                "which you started\nthis program."));
    MythProgressDialog pdlg( waitMsg, 2 );
    VERBOSE(VB_GENERAL, QString("Please wait while MythTV retrieves the "
                                "list of available channels"));
    pdlg.show();

    QString filename = QString("%1/.mythtv/%2.xmltv")
        .arg(QDir::homeDirPath()).arg(parent.getSourceName());
    QString command = QString("tv_grab_uk --config-file '%1' --configure --retry-limit %2 --retry-delay %3 --postalcode %4 --provider %5 --auto-new-channels add")
        .arg(filename)
        .arg(2)
        .arg(30)
        .arg(region->getValue())
        .arg(provider->getValue());

    pdlg.setProgress(1);

    int ret = system(command);
    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  QObject::tr("Failed to retrieve channel "
                                              "information."),
                                  QObject::tr("MythTV was unable to retrieve "
                                              "channel information for your "
                                              "provider.\nPlease check the "
                                              "terminal window for more "
                                              "information"));
    }

    pdlg.setProgress( 2 );
    pdlg.Close();
}

XMLTV_generic_config::XMLTV_generic_config(const VideoSource& _parent, 
                                           QString _grabber)
                    : parent(_parent), grabber(_grabber) 
{
    setLabel(grabber);
    setValue(QObject::tr("Configuration will run in the terminal window"));
}

void XMLTV_generic_config::save(QSqlDatabase* db) {
    (void)db;

    QString waitMsg(QObject::tr("Please wait while MythTV retrieves the "
                                "list of available channels.\nYou "
                                "might want to check the output as it\n"
                                "runs by switching to the terminal from "
                                "which you started\nthis program."));
    MythProgressDialog pdlg( waitMsg, 2 );
    VERBOSE(VB_GENERAL, QString("Please wait while MythTV retrieves the "
                                "list of available channels"));
    pdlg.show();

    QString command;
    if (grabber == "tv_grab_de") {
        command = "tv_grab_de --configure";
    } else {
        QString filename = QString("%1/.mythtv/%2.xmltv")
            .arg(QDir::homeDirPath()).arg(parent.getSourceName());

        command = QString("%1 --config-file '%2' --configure")
            .arg(grabber).arg(filename);
    }

    pdlg.setProgress(1);

    int ret = system(command);
    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));

        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  QObject::tr("Failed to retrieve channel "
                                              "information."),
                                  QObject::tr("MythTV was unable to retrieve "
                                              "channel information for your "
                                              "provider.\nPlease check the "
                                              "terminal window for more "
                                              "information"));
    }

    if (grabber == "tv_grab_de" || grabber == "tv_grab_se" || 
        grabber == "tv_grab_fi" || grabber == "tv_grab_es" ||
        grabber == "tv_grab_nl" || grabber == "tv_grab_jp" ||
        grabber == "tv_grab_no")
    {
        cerr << "You _MUST_ run 'mythfilldatabase --manual the first time, "
             << "instead\n";
        cerr << "of just 'mythfilldatabase'.  Your grabber does not provide\n";
        cerr << "channel numbers, so you have to set them manually.\n";

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Warning."),
                                  QObject::tr("You MUST run 'mythfilldatabase "
                                              "--manual the first time,\n "
                                              "instead of just "
                                              "'mythfilldatabase'.\nYour "
                                              "grabber does not provide "
                                              "channel numbers, so you have to "
                                              "set them manually.") );
    }

    pdlg.setProgress( 2 );    
    pdlg.Close();
}

XMLTVConfig::XMLTVConfig(const VideoSource& parent) 
{
    setUseLabel(false);

    XMLTVGrabber* grabber = new XMLTVGrabber(parent);
    addChild(grabber);
    setTrigger(grabber);

    // only save settings for the selected grabber
    setSaveAll(false);

    addTarget("datadirect", new DataDirect_config(parent));
    grabber->addSelection("North America (DataDirect)", "datadirect");

    addTarget("tv_grab_de", new XMLTV_generic_config(parent, "tv_grab_de"));
    grabber->addSelection("Germany/Austria", "tv_grab_de");

    addTarget("tv_grab_se", new XMLTV_generic_config(parent, "tv_grab_se"));
    grabber->addSelection("Sweden","tv_grab_se");

    addTarget("tv_grab_no", new XMLTV_generic_config(parent, "tv_grab_no"));
    grabber->addSelection("Norway","tv_grab_no");

    addTarget("tv_grab_uk", new XMLTV_generic_config(parent, "tv_grab_uk"));
    grabber->addSelection("United Kingdom","tv_grab_uk");

    addTarget("tv_grab_uk_rt", new XMLTV_generic_config(parent, "tv_grab_uk_rt"));
    grabber->addSelection("United Kingdom (alternative)","tv_grab_uk_rt");

    addTarget("tv_grab_au", new XMLTV_generic_config(parent, "tv_grab_au"));
    grabber->addSelection("Australia", "tv_grab_au");

    addTarget("tv_grab_nz", new XMLTV_generic_config(parent, "tv_grab_nz"));
    grabber->addSelection("New Zealand", "tv_grab_nz");

    addTarget("tv_grab_fi", new XMLTV_generic_config(parent, "tv_grab_fi"));
    grabber->addSelection("Finland", "tv_grab_fi");

    addTarget("tv_grab_es", new XMLTV_generic_config(parent, "tv_grab_es"));
    grabber->addSelection("Spain", "tv_grab_es");

    addTarget("tv_grab_nl", new XMLTV_generic_config(parent, "tv_grab_nl"));
    grabber->addSelection("Holland", "tv_grab_nl");

    addTarget("tv_grab_dk", new XMLTV_generic_config(parent, "tv_grab_dk"));
    grabber->addSelection("Denmark", "tv_grab_dk");

    addTarget("tv_grab_fr", new XMLTV_generic_config(parent, "tv_grab_fr"));
    grabber->addSelection("France", "tv_grab_fr");

    addTarget("tv_grab_jp", new XMLTV_generic_config(parent, "tv_grab_jp"));
    grabber->addSelection("Japan", "tv_grab_jp");
}

VideoSource::VideoSource() 
{
    // must be first
    addChild(id = new ID());

    ConfigurationGroup *group = new VerticalConfigurationGroup(false, false);
    group->setLabel(QObject::tr("Video source setup"));
    group->addChild(name = new Name(*this));
    group->addChild(new XMLTVConfig(*this));
    group->addChild(new FreqTableSelector(*this));
    addChild(group);
}

void VideoSource::fillSelections(QSqlDatabase* db,
                                 SelectSetting* setting) {
    QString query = QString("SELECT name, sourceid FROM videosource;");
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(0).toString(),
                                  result.value(1).toString());
}

void VideoSource::loadByID(QSqlDatabase* db, int sourceid) {
    id->setValue(sourceid);
    load(db);
}

class VideoDevice: public PathSetting, public CCSetting {
public:
    VideoDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "videodevice") {
        setLabel(QObject::tr("Video device"));
        QDir dev("/dev", "video*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/v4l");
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/dtv");
        fillSelectionsFromDir(dev);
    };

    static QStringList probeInputs(QString device);
};

class VbiDevice: public PathSetting, public CCSetting {
public:
    VbiDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "vbidevice") {
        setLabel(QObject::tr("VBI device"));
        QDir dev("/dev", "vbi*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/v4l");
        fillSelectionsFromDir(dev);
    };
};

class AudioDevice: public PathSetting, public CCSetting {
public:
    AudioDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "audiodevice") {
        setLabel(QObject::tr("Audio device"));
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
        addSelection(QObject::tr("(None)"), "/dev/null");
    };
};

class AudioRateLimit: public ComboBoxSetting, public CCSetting {
public:
    AudioRateLimit(const CaptureCard& parent):
        CCSetting(parent, "audioratelimit") {
        setLabel(QObject::tr("Audio sampling rate limit"));
        addSelection(QObject::tr("(None)"), "0");
        addSelection("32000");
        addSelection("44100");
        addSelection("48000");
    };
};

class SkipBtAudio: public CheckBoxSetting, public CCSetting {
public:
    SkipBtAudio(const CaptureCard& parent):
    CCSetting(parent, "skipbtaudio") {
        setLabel(QObject::tr("Do not adjust volume"));
        setHelpText(QObject::tr("Check this option for budget BT878 based "
                    "DVB-T cards such as the AverTV DVB-T that require the "
                    "audio volume left alone."));
   };
};

class DVBCardNum: public SpinBoxSetting, public CCSetting {
public:
    DVBCardNum(const CaptureCard& parent):
        SpinBoxSetting(0,3,1),
        CCSetting(parent, "videodevice") {
        setLabel(QObject::tr("DVB Card Number"));
        setHelpText(QObject::tr("When you change this setting, the text below "
                    "should change to the name and type of your card. If the "
                    "card cannot be opened, an error message will be "
                    "displayed."));
    };
};

class DVBCardType: public LabelSetting, public TransientStorage {
public:
    DVBCardType() {
        setLabel(QObject::tr("Card Type"));
    };
};

class DVBCardName: public LabelSetting, public TransientStorage {
public:
    DVBCardName() {
        setLabel(QObject::tr("Card Name"));
    };
};

class DVBSwFilter: public CheckBoxSetting, public CCSetting {
public:
    DVBSwFilter(const CaptureCard& parent):
        CCSetting(parent, "dvb_swfilter") {
        setLabel(QObject::tr("Do NOT use DVB driver for filtering."));
        setHelpText(QObject::tr("(BROKEN) This option is used to get around "
                    "filtering limitations on some DVB cards."));
    };
};

class DVBRecordTS: public CheckBoxSetting, public CCSetting {
public:
    DVBRecordTS(const CaptureCard& parent):
        CCSetting(parent, "dvb_recordts") {
        setLabel(QObject::tr("Record the TS, not PS."));
        setHelpText(QObject::tr("This will make the backend not perform "
                    "Transport Stream to Program Stream conversion."));
    };
};

class DVBNoSeqStart: public CheckBoxSetting, public CCSetting {
public:
    DVBNoSeqStart(const CaptureCard& parent):
        CCSetting(parent, "dvb_wait_for_seqstart") {
        setLabel(QObject::tr("Wait for SEQ start header."));
        setValue(true);
        setHelpText(QObject::tr("Normally the dvb-recording will drop packets "
                    "from the card untill a sequence start header is seen. "
                    "This option turns off this feature."));
    };
};

class DVBOnDemand: public CheckBoxSetting, public CCSetting {
public:
    DVBOnDemand(const CaptureCard& parent):
        CCSetting(parent, "dvb_on_demand") {
        setLabel(QObject::tr("Open DVB card on demand"));
        setValue(true);
        setHelpText(QObject::tr("This option makes the backend dvb-recorder "
                    "only open the card when it is actually in-use leaving "
                    "it free for other programs at other times."));
    };
};

class DVBPidBufferSize: public SpinBoxSetting, public CCSetting {
public:
    DVBPidBufferSize(const CaptureCard& parent):
        SpinBoxSetting(0, 180000, 188),
        CCSetting(parent, "dvb_dmx_buf_size") {
        setLabel(QObject::tr("Per PID driver buffer size"));
        setValue(188*50);
    };
};

class DVBBufferSize: public SpinBoxSetting, public CCSetting {
public:
    DVBBufferSize(const CaptureCard& parent):
        SpinBoxSetting(0, 188000, 188),
        CCSetting(parent, "dvb_pkt_buf_size") {
        setLabel(QObject::tr("Packet buffer"));
        setValue(188*100);
    };
};

class DVBSignalChannelOptions: public ChannelOptionsDVB {
public:
    DVBSignalChannelOptions(ChannelID& id): ChannelOptionsDVB(id) {};
    void load(QSqlDatabase* db) {
        if (id.intValue() != 0)
            ChannelOptionsDVB::load(db);
    };
    void save(QSqlDatabase* db) { (void)db; };
};

class DVBSatelliteConfigType: public ComboBoxSetting, public CCSetting {
public:
    DVBSatelliteConfigType(CaptureCard& parent):
        CCSetting(parent, "dvb_sat_type") {
        setLabel(QObject::tr("Type"));
        addSelection("Single LNB","0");
        addSelection("Tone Switch aka Mini DiSEqC (2-Way)","1");
        addSelection("DiSEq v1.0 Switch (2-Way)","2");
        addSelection("DiSEq v1.1 Switch (2-Way)","3");
        addSelection("DiSEq v1.0 Switch (4-Way)","4");
        addSelection("DiSEq v1.1 Switch (4-Way)","5");
//        addSelection("DiSEqC Positioner","6");
        setHelpText(QObject::tr("Select the type of satellite equipment you "
                    "have. Selecting 'Finish' on this screen will only save "
                    "the type, and not the individual satellite, move down to "
                    "the list to do this."));
    };
};

DVBSatelliteList::DVBSatelliteList(CaptureCard& _parent)
                : parent(_parent)
{
    setLabel(QObject::tr("Satellites"));
    db = NULL;
    satellites = 1;
    setHelpText(QObject::tr("Select the satellite you want to configure "
                "and press the 'menu' key, and edit the satellite, when "
                "you are done configuring, press 'OK' to leave this "
                "wizard."));
}

void DVBSatelliteList::load(QSqlDatabase* _db)
{
    db = _db;
    clearSelections();
    QSqlQuery query = db->exec(
        QString("SELECT name FROM dvb_sat WHERE cardid=%1")
        .arg(parent.getCardID()));
    if (!query.isActive())
        MythContext::DBError("DvbSatelliteList", query);
    for (int i=0; i<satellites; i++)
    {
        QString str = QString("%1. ").arg(i);
        if (i<query.numRowsAffected())
        {
            query.next();
            str += query.value(0).toString();
        }
        else
            str += "Unconfigured";
        addSelection(str);
    }
}

void DVBSatelliteList::fillSelections(const QString &v)
{
    satellites = 1;
    if (v.toInt() > 0)
        satellites = 2;
    if (v.toInt() > 3)
        satellites = 4;
    if (db)
        load(db);
}

class V4LConfigurationGroup: public VerticalConfigurationGroup {
public:
    V4LConfigurationGroup(CaptureCard& a_parent):
        parent(a_parent) {
        setUseLabel(false);

        VideoDevice* device;
        TunerCardInput* input;

        addChild(device = new VideoDevice(parent));
        addChild(new VbiDevice(parent));
        addChild(new AudioDevice(parent));

        HorizontalConfigurationGroup *ag;
        ag = new HorizontalConfigurationGroup(false, false);
        ag->addChild(new AudioRateLimit(parent));
        ag->addChild(new SkipBtAudio(parent));
        addChild(ag);

        addChild(input = new TunerCardInput(parent));

        connect(device, SIGNAL(valueChanged(const QString&)),
                input, SLOT(fillSelections(const QString&)));
        input->fillSelections(device->getValue());
    };
private:
    CaptureCard& parent;
};

CaptureCardGroup::CaptureCardGroup(CaptureCard& parent)
{
    setLabel(QObject::tr("Capture Card Setup"));

    CardType* cardtype = new CardType(parent);
    addChild(cardtype);
    setTrigger(cardtype);
    setSaveAll(false);

    addTarget("V4L", new V4LConfigurationGroup(parent));
    addTarget("DVB", new DVBConfigurationGroup(parent));
}

void CaptureCardGroup::triggerChanged(const QString& value) 
{
    QString own = value;
    if (own == "HDTV" || own == "MPEG" || own == "MJPEG")
        own = "V4L";
    TriggeredConfigurationGroup::triggerChanged(own);
}

CaptureCard::CaptureCard() 
{
    // must be first
    addChild(id = new ID());

    CaptureCardGroup *cardgroup = new CaptureCardGroup(*this);
    addChild(cardgroup);

    addChild(new Hostname(*this));
}

void CaptureCard::fillSelections(QSqlDatabase* db,
                                 SelectSetting* setting) {
    QString query = QString("SELECT cardtype, videodevice, cardid "
                            "FROM capturecard WHERE hostname = \"%1\";")
                            .arg(gContext->GetHostName());
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection("[ " + result.value(0).toString() + " : " +
                                  result.value(1).toString() + " ]",
                                  result.value(2).toString());
}

void CaptureCard::loadByID(QSqlDatabase* db, int cardid) {
    id->setValue(cardid);
    load(db);
}

CardType::CardType(const CaptureCard& parent)
        : CCSetting(parent, "cardtype") 
{
    setLabel(QObject::tr("Card type"));
    setHelpText(QObject::tr("Change the cardtype to the appropriate type for "
                "the capture card you are configuring."));
    fillSelections(this);
}

void CardType::fillSelections(SelectSetting* setting)
{
    setting->addSelection(QObject::tr("Standard V4L capture card"), "V4L");
    setting->addSelection(QObject::tr("MJPEG capture card (Matrox G200, DC10)"),
                          "MJPEG");
    setting->addSelection(QObject::tr("MPEG-2 Encoder card (PVR-250, PVR-350)"),
                          "MPEG");
    setting->addSelection(QObject::tr("pcHDTV capture card (HD-2000, HD-3000)"),
                          "HDTV");
    setting->addSelection(QObject::tr("Digital Video Broadcast card (DVB)"), 
                          "DVB");
}

class CardID: public SelectLabelSetting, public CISetting {
public:
    CardID(const CardInput& parent):
        CISetting(parent, "cardid") {
        setLabel(QObject::tr("Capture device"));
    };

    virtual void load(QSqlDatabase* db) {
        fillSelections(db);
        CISetting::load(db);
    };

    void fillSelections(QSqlDatabase* db) {
        CaptureCard::fillSelections(db, this);
    };
};

class SourceID: public ComboBoxSetting, public CISetting {
public:
    SourceID(const CardInput& parent):
        CISetting(parent, "sourceid") {
        setLabel(QObject::tr("Video source"));
        addSelection(QObject::tr("(None)"), "0");
    };

    virtual void load(QSqlDatabase* db) {
        fillSelections(db);
        CISetting::load(db);
    };

    void fillSelections(QSqlDatabase* db) {
        clearSelections();
        addSelection(QObject::tr("(None)"), "0");
        VideoSource::fillSelections(db, this);
    };
};

class InputName: public LabelSetting, public CISetting {
public:
    InputName(const CardInput& parent):
        CISetting(parent, "inputname") {
        setLabel(QObject::tr("Input"));
    };
};

class ExternalChannelCommand: public LineEditSetting, public CISetting {
public:
    ExternalChannelCommand(const CardInput& parent):
        CISetting(parent,"externalcommand") {
        setLabel(QObject::tr("External channel change command"));
        setValue("");
        setHelpText(QObject::tr("If specified, this command will be run to "
                    "change the channel for inputs which have an external "
                    "tuner device such as a cable box. The first argument "
                    "will be the channel number."));
    };
};

class PresetTuner: public LineEditSetting, public CISetting {
public:
    PresetTuner(const CardInput& parent):
        CISetting(parent, "tunechan") {
        setLabel(QObject::tr("Preset tuner to channel"));
        setValue("");
        setHelpText(QObject::tr("Leave this blank unless you have an external "
                    "tuner that is connected to the tuner input of your card. "
                    "If so, you will need to specify the preset channel for "
                    "the signal (normally 3 or 4)."));
    };
};

class StartingChannel: public LineEditSetting, public CISetting {
public:
    StartingChannel(const CardInput& parent):
        CISetting(parent, "startchan") {
        setLabel(QObject::tr("Starting channel"));
        setValue("3");
        setHelpText(QObject::tr("LiveTV will change to the above channel when "
                    "the input is first selected."));
    };
};

class InputPreference: public SpinBoxSetting, public CISetting {
public:
    InputPreference(const CardInput& parent):
        SpinBoxSetting(-99,99,1),
        CISetting(parent, "preference") {
        setLabel(QObject::tr("Input preference"));
        setValue(0);
        setHelpText(QObject::tr("If the input preference is not equal for "
                    "all inputs, the scheduler may choose to record a show "
                    "at a later time so that it can record on an input with "
                    "a higher value."));
    };
};

class CardInput: public ConfigurationWizard {
public:
    CardInput() {
        addChild(id = new ID());

        ConfigurationGroup *group = new VerticalConfigurationGroup(false);
        group->setLabel(QObject::tr("Connect source to input"));
        group->addChild(cardid = new CardID(*this));
        group->addChild(inputname = new InputName(*this));
        group->addChild(sourceid = new SourceID(*this));
        group->addChild(new InputPreference(*this));
        group->addChild(new ExternalChannelCommand(*this));
        group->addChild(new PresetTuner(*this));
        group->addChild(new StartingChannel(*this));
        addChild(group);
    };

    int getInputID(void) const { return id->intValue(); };

    void loadByID(QSqlDatabase* db, int id);
    void loadByInput(QSqlDatabase* db, int cardid, QString input);
    QString getSourceName(void) const { return sourceid->getSelectionLabel(); };

    virtual void save(QSqlDatabase* db);

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("cardinput", "cardid") {
            setVisible(false);
            setName("CardInputID");
        };
        virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName = 0) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };

private:
    ID* id;
    CardID* cardid;
    InputName* inputname;
    SourceID* sourceid;
    QSqlDatabase* db;
};

QString CISetting::whereClause(void) {
    return QString("cardinputid = %1").arg(parent.getInputID());
}

QString CISetting::setClause(void) {
    return QString("cardinputid = %1, %2 = '%3'")
        .arg(parent.getInputID())
        .arg(getColumn())
        .arg(getValue());
}

void CardInput::loadByID(QSqlDatabase* db, int inputid) {
    id->setValue(inputid);
    load(db);
}

void CardInput::loadByInput(QSqlDatabase* db, int _cardid, QString _inputname) {
    QString query = QString("SELECT cardinputid FROM cardinput WHERE cardid = %1 AND inputname = '%2';")
        .arg(_cardid)
        .arg(_inputname);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        result.next();
        loadByID(db, result.value(0).toInt());
    } else {
        load(db); // new
        cardid->setValue(QString::number(_cardid));
        inputname->setValue(_inputname);
    }
}

void CardInput::save(QSqlDatabase* db) {
    if (sourceid->getValue() == "0")
        // "None" is represented by the lack of a row
        db->exec(QString("DELETE FROM cardinput WHERE cardinputid = %1;").arg(getInputID()));
    else
        ConfigurationWizard::save(db);
}

int CISetting::getInputID(void) const {
    return parent.getInputID();
}

int CCSetting::getCardID(void) const {
    return parent.getCardID();
}

int DvbSatSetting::getCardID(void) const {
    return parent.getCardID();
}

int DvbSatSetting::getSatNum(void) const {
    return satnum;
}

int CaptureCardEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void CaptureCardEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection(QObject::tr("(New capture card)"), "0");
    CaptureCard::fillSelections(db, this);
}

MythDialog* CaptureCardEditor::dialogWidget(MythMainWindow* parent,
                                            const char* widgetName) 
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(menu()));
    return dialog;
}

void CaptureCardEditor::menu(void)
{
    if (getValue().toInt() == 0) 
    {
        CaptureCard cc;
        cc.exec(db);
    } 
    else 
    {
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 tr("Capture Card Menu"),
                                                 tr("Edit.."),
                                                 tr("Delete.."), 1);
        if (val == 0)
            edit();
        else if (val == 1)
            del();
    }
}

void CaptureCardEditor::edit(void)
{
    CaptureCard cc;
    if (getValue().toInt() != 0)
        cc.loadByID(db,getValue().toInt());
    cc.exec(db);
}

void CaptureCardEditor::del(void)
{
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                          tr("Are you sure you want to delete "
                                             "this capture card?"),
                                             tr("Yes, delete capture card"),
                                             tr("No, don't"), 2);
    if (val == 0)
    {
        QSqlQuery query;

        query = db->exec(QString("DELETE FROM capturecard"
                                 " WHERE cardid='%1'").arg(getValue()));
        if (!query.isActive())
            MythContext::DBError("Deleting Capture Card", query);

        query = db->exec(QString("DELETE FROM cardinput"
                                 " WHERE cardid='%1'").arg(getValue()));
        if (!query.isActive())
            MythContext::DBError("Deleting Card Input", query);
        load(db);
    }
}

MythDialog* VideoSourceEditor::dialogWidget(MythMainWindow* parent,
                                            const char* widgetName) 
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(menu()));
    return dialog;
}

int VideoSourceEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void VideoSourceEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection(QObject::tr("(New video source)"), "0");
    VideoSource::fillSelections(db, this);
}

void VideoSourceEditor::menu()
{
    if (getValue().toInt() == 0) 
    {
        VideoSource vs;
        vs.exec(db);
    } 
    else 
    {
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 tr("Video Source Menu"),
                                                 tr("Edit.."),
                                                 tr("Delete.."), 1);

        if (val == 0)
            edit();
        else if (val == 1)
            del();
    }
}

void VideoSourceEditor::edit() 
{
    VideoSource vs;
    if (getValue().toInt() != 0)
        vs.loadByID(db,getValue().toInt());

    vs.exec(db);
}

void VideoSourceEditor::del() 
{
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                          tr("Are you sure you want to delete "
                                             "this video source?"),
                                             tr("Yes, delete video source"),
                                             tr("No, don't"), 2);

    if (val == 0)
    {
        QSqlQuery query = db->exec(QString("DELETE FROM videosource"
                                           " WHERE sourceid='%1'")
                                           .arg(getValue()));
        if (!query.isActive())
            MythContext::DBError("Deleting VideoSource", query);
        load(db);
    }
}

int CardInputEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        cardinputs[getValue().toInt()]->exec(db);

    return QDialog::Rejected;
}



void CardInputEditor::load(QSqlDatabase* db) {
    clearSelections();

    // We do this manually because we want custom labels.  If
    // SelectSetting provided a facility to edit the labels, we
    // could use CaptureCard::fillSelections

    QString thequery = QString("SELECT cardid, videodevice, cardtype "
                               "FROM capturecard WHERE hostname = \"%1\";")
                              .arg(gContext->GetHostName());

    QSqlQuery capturecards = db->exec(thequery);
    if (capturecards.isActive() && capturecards.numRowsAffected() > 0)
        while (capturecards.next()) {
            int cardid = capturecards.value(0).toInt();
            QString videodevice(capturecards.value(1).toString());

            QStringList inputs;
            if (capturecards.value(2).toString() != "DVB")
                inputs = VideoDevice::probeInputs(videodevice);
            else
                inputs = QStringList("DVBInput");

            for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i) {
                CardInput* cardinput = new CardInput();
                cardinput->loadByInput(db, cardid, *i);
                cardinputs.push_back(cardinput);
                QString index = QString::number(cardinputs.size()-1);

                QString label = QString("%1 (%2) -> %3")
                    .arg("[ " + capturecards.value(2).toString() + 
                         " : " + capturecards.value(1).toString() + 
                         " ]")
                    .arg(*i)
                    .arg(cardinput->getSourceName());
                addSelection(label, index);
            }
        }
}

CardInputEditor::~CardInputEditor() {
    while (cardinputs.size() > 0) {
        delete cardinputs.back();
        cardinputs.pop_back();
    }
}

QStringList VideoDevice::probeInputs(QString device) {
    QStringList ret;

#if defined(CONFIG_VIDEO4LINUX)
    int videofd = open(device.ascii(), O_RDWR);
    if (videofd < 0) {
        cerr << "Couldn't open " << device << " to probe its inputs.\n";
        return ret;
    }

    bool usingv4l2 = false;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
         usingv4l2 = false;
    else {
        if (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
            usingv4l2 = true;
    }

    if (usingv4l2) {
        struct v4l2_input vin;
        memset(&vin, 0, sizeof(vin));
        vin.index = 0;

        while (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0) {
            QString input((char *)vin.name);

            ret += input;

            vin.index++;
        }
    }
    else
    {
        struct video_capability vidcap;
        memset(&vidcap, 0, sizeof(vidcap));
        if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0) {
            perror("VIDIOCGCAP");
            vidcap.channels = 0;
        }

        for (int i = 0; i < vidcap.channels; i++) {
            struct video_channel test;
            memset(&test, 0, sizeof(test));
            test.channel = i;

            if (ioctl(videofd, VIDIOCGCHAN, &test) != 0) {
                perror("ioctl(VIDIOCGCHAN)");
                continue;
            }

            ret += test.name;
        }
    }

    if (ret.size() == 0)
        ret += QObject::tr("ERROR, No inputs found");

    close(videofd);

#else
    ret += QObject::tr("ERROR, V4L support unavailable on this OS");
#endif

    return ret;
}

void DVBConfigurationGroup::probeCard(const QString& cardNumber)
{
#ifdef USING_DVB
    int fd = open(dvbdevice(DVB_DEV_FRONTEND, cardNumber.toInt()), O_RDWR);
    if (fd == -1)
    {
        cardname->setValue(QString("Could not open card #%1!")
                            .arg(cardNumber));
        cardtype->setValue(strerror(errno));
    }
    else
    {
        struct dvb_frontend_info info;

        if (ioctl(fd, FE_GET_INFO, &info) < 0)
        {
            cardname->setValue(QString("Could not get card info for card #%1!")
                                      .arg(cardNumber));
            cardtype->setValue(strerror(errno));
        }
        else
        {
            switch(info.type)
            {
                case FE_QPSK:   cardtype->setValue("DVB-S"); break;
                case FE_QAM:    cardtype->setValue("DVB-C"); break;
                case FE_OFDM:   cardtype->setValue("DVB-T"); break;
            }

            cardname->setValue(info.name);
        }
        close(fd);
    }
#else
    (void)cardNumber;
    cardtype->setValue(QString("Recompile with DVB-Support!"));
#endif
}

void TunerCardInput::fillSelections(const QString& device) {
    clearSelections();

    if (device == QString::null || device == "")
        return;

    QStringList inputs = VideoDevice::probeInputs(device);
    for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);
}

class DVBSignalMeter: public ProgressSetting, public TransientStorage {
public:
    DVBSignalMeter(int steps): ProgressSetting(steps), TransientStorage() {};
};

class DVBVideoSource: public ComboBoxSetting {
public:
    DVBVideoSource() {
        setLabel("VideoSource");
    };

    void save(QSqlDatabase* db) { (void)db; };
    void load(QSqlDatabase* _db) {
        db = _db;
        fillSelections();
    };

    void fillSelections() {
        clearSelections();
        QSqlQuery query;
        query = db->exec(QString("SELECT videosource.name,videosource.sourceid "
                                 "FROM videosource,capturecard,cardinput "
                                 "WHERE capturecard.cardtype='DVB' "
                                 "AND videosource.sourceid=cardinput.sourceid "
                                 "AND cardinput.cardid=capturecard.cardid"));
        if (!query.isActive())
            MythContext::DBError("VideoSource", query);

        if (query.numRowsAffected() > 0) {
            addSelection(QObject::tr("[All VideoSources]"), "ALL");
            while(query.next())
                addSelection(query.value(0).toString(),
                              query.value(1).toString());
        } else
            addSelection(QObject::tr("[No VideoSources Defined]"), "0");
    };

private:
    QSqlDatabase* db;
};

void DVBChannels::fillSelections(const QString& videoSource) 
{
    if (db == NULL)
        return;
    clearSelections();
    QSqlQuery query;
    QString thequery = "SELECT name,chanid FROM channel";
    if (videoSource != "All")
        thequery += QString(" WHERE sourceid='%1'").arg(videoSource);
    query = db->exec(thequery);
    if (!query.isActive()) {
        MythContext::DBError("Selecting Testchannels", query);
        return;
    }

    if (query.numRowsAffected() > 0) {
        while (query.next())
            addSelection(query.value(0).toString(),
                         query.value(1).toString());
    } else
        addSelection(QObject::tr("[No Channels Defined]"), "0");
}

class DVBLoadTuneButton: public ButtonSetting, public TransientStorage {
public:
    DVBLoadTuneButton() {
        setLabel(QObject::tr("Load & Tune"));
        setHelpText(QObject::tr("Will load the selected channel above into "
                    "the previous screen, and try to tune it. If it fails to "
                    "tune the channel, press back and check the settings."));
    };
};

class DVBTuneOnlyButton: public ButtonSetting, public TransientStorage {
public:
    DVBTuneOnlyButton() {
        setLabel(QObject::tr("Tune Only"));
        setHelpText(QObject::tr("Will ONLY try to tune the previous screen, "
                    "not alter it. If it fails to tune, press back and check "
                    "the settings."));
    };
};

#ifndef USING_DVB
DVBCardVerificationWizard::DVBCardVerificationWizard(int cardNum)
{
    (void)cardNum;
}

DVBCardVerificationWizard::~DVBCardVerificationWizard()
{
}

void DVBCardVerificationWizard::tuneConfigscreen()
{
}

void DVBCardVerificationWizard::tunePredefined()
{
}

#else

DVBCardVerificationWizard::DVBCardVerificationWizard(int cardNum) {
    chan = new DVBChannel(cardNum);

    cid.setValue(0);
    addChild(dvbopts = new DVBSignalChannelOptions(cid));

    VerticalConfigurationGroup* testGroup = new VerticalConfigurationGroup(false,true);
    testGroup->setLabel(QObject::tr("Card Verification Wizard (DVB#") + QString("%1)").arg(cardNum));
    testGroup->setUseLabel(false);

    DVBVideoSource* source;

    DVBStatusLabel* sl = new DVBStatusLabel();
    DVBSignalMeter* sn = new DVBSignalMeter(65535);
    DVBSignalMeter* ss = new DVBSignalMeter(65535);
    testGroup->addChild(sl);
    testGroup->addChild(sn);
    testGroup->addChild(ss);

    HorizontalConfigurationGroup* lblGroup = new HorizontalConfigurationGroup(false,true);
    DVBInfoLabel* ber = new DVBInfoLabel(QObject::tr("Bit Error Rate"));
    DVBInfoLabel* un  = new DVBInfoLabel(QObject::tr("Uncorrected Blocks"));
    lblGroup->addChild(ber);
    lblGroup->addChild(un);
    testGroup->addChild(lblGroup);

    HorizontalConfigurationGroup* chanGroup = new HorizontalConfigurationGroup(false,true);
    chanGroup->addChild(source = new DVBVideoSource());
    chanGroup->addChild(channels = new DVBChannels());
    testGroup->addChild(chanGroup);

    HorizontalConfigurationGroup* btnGroup = new HorizontalConfigurationGroup(false,true);
    DVBLoadTuneButton* loadtune = new DVBLoadTuneButton();
    DVBTuneOnlyButton* tuneonly = new DVBTuneOnlyButton();
    btnGroup->addChild(loadtune);
    btnGroup->addChild(tuneonly);
    testGroup->addChild(btnGroup);

    sn->setLabel(QObject::tr("Signal/Noise"));
    ss->setLabel(QObject::tr("Signal Strength"));

    addChild(testGroup);

    connect(source, SIGNAL(valueChanged(const QString&)),
            channels, SLOT(fillSelections(const QString&)));

    connect(chan, SIGNAL(StatusSignalToNoise(int)), sn, SLOT(setValue(int)));
    connect(chan, SIGNAL(StatusSignalStrength(int)), ss, SLOT(setValue(int)));
    connect(chan, SIGNAL(StatusBitErrorRate(int)), ber, SLOT(set(int)));
    connect(chan, SIGNAL(StatusUncorrectedBlocks(int)), un, SLOT(set(int)));
    connect(chan, SIGNAL(Status(QString)), sl, SLOT(set(QString)));

    connect(loadtune, SIGNAL(pressed()), this, SLOT(tunePredefined()));
    connect(tuneonly, SIGNAL(pressed()), this, SLOT(tuneConfigscreen()));
}

DVBCardVerificationWizard::~DVBCardVerificationWizard()
{
    delete chan;
}

void DVBCardVerificationWizard::tuneConfigscreen() {
    dvb_channel_t chan_opts;

    if (!chan->Open())
    {
        setLabel(QObject::tr("FAILED TO OPEN CARD, CHECK CONSOLE!!"));
        return;
    }

    bool parseOK = true;
    switch (chan->GetCardType())
    {
        case FE_QPSK:
            parseOK = chan->ParseQPSK(
                dvbopts->getFrequency(), dvbopts->getInversion(),
                dvbopts->getSymbolrate(), dvbopts->getFec(),
                dvbopts->getPolarity(),
                //TODO: Static Diseqc options.
                QString("0"), QString("0"),
                //TODO: Static LNB options.
                QString("11700000"), QString("10600000"),
                QString("9750000"), chan_opts.tuning);
            break;
        case FE_QAM:
            parseOK = chan->ParseQAM(
                dvbopts->getFrequency(), dvbopts->getInversion(),
                dvbopts->getSymbolrate(), dvbopts->getFec(),
                dvbopts->getModulation(), chan_opts.tuning);
            break;
        case FE_OFDM:
            parseOK = chan->ParseOFDM(
                dvbopts->getFrequency(), dvbopts->getInversion(),
                dvbopts->getBandwidth(), dvbopts->getCodeRateHP(),
                dvbopts->getCodeRateLP(), dvbopts->getConstellation(),
                dvbopts->getTransmissionMode(), dvbopts->getGuardInterval(),
                dvbopts->getHierarchy(), chan_opts.tuning);
            break;
    }

    chan->Tune(chan_opts,true);
}

void DVBCardVerificationWizard::tunePredefined() {
    cid.setValue(channels->getValue().toInt());
    dvbopts->load(db);
    tuneConfigscreen();
}
#endif

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent):
                       parent(a_parent) {
    setUseLabel(false);

    advcfg = new TransButtonSetting();
    advcfg->setLabel(QObject::tr("Advanced Configuration"));

    DVBCardNum* cardnum = new DVBCardNum(parent);
    cardname = new DVBCardName();
    cardtype = new DVBCardType();

    addChild(cardnum);
    addChild(cardname);
    addChild(cardtype);
    addChild(new DVBAudioDevice(parent));
    addChild(new DVBVbiDevice(parent));
    addChild(new DVBDefaultInput(parent));
    addChild(advcfg);

    connect(cardnum, SIGNAL(valueChanged(const QString&)),
            this, SLOT(probeCard(const QString&)));
    connect(cardnum, SIGNAL(valueChanged(const QString&)),
            &parent, SLOT(setDvbCard(const QString&)));
    connect(advcfg, SIGNAL(pressed()), &parent, SLOT(execDVBConfigMenu()));
    cardnum->setValue(0);
}

void CaptureCard::execDVBConfigMenu() {
    if (getCardID() == 0)
    {
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                    tr("You have to save the current card before configuring it"
                       ", would you like to do this now?"),
                    tr("Yes, save now"), tr("No, don't"), 2);

        if (val != 0)
            return;
        save(db);
        load(db);
    }

    DVBAdvancedConfigMenu acm(*this);
    acm.exec(db);
}

DVBSatelliteWizard::DVBSatelliteWizard(CaptureCard& _parent)
                  : parent(_parent) 
{
    VerticalConfigurationGroup* g = new VerticalConfigurationGroup(false);
    g->setLabel(QObject::tr("Satellite Configuration"));
    g->setUseLabel(false);
    DVBSatelliteConfigType* type = new DVBSatelliteConfigType(parent);
    list = new DVBSatelliteList(parent);
    connect(type, SIGNAL(valueChanged(const QString&)),
            list, SLOT(fillSelections(const QString&)));
    connect(list, SIGNAL(menuButtonPressed(int)),
            this, SLOT(editSat(int)));
    g->addChild(type);
    g->addChild(list);
    addChild(g);
}

class SatName: public LineEditSetting, public DvbSatSetting {
public:
    SatName(const CaptureCard& parent, int satnum):
        DvbSatSetting(parent, satnum, "name") {
        setLabel(QObject::tr("Satellite Name"));
        setValue("Unnamed");
        setHelpText(QObject::tr("A textual representation of this "
                    "satellite or cluster of satellites."));
    };
};

class SatPos: public LineEditSetting, public DvbSatSetting {
public:
    SatPos(const CaptureCard& parent, int satnum):
        DvbSatSetting(parent, satnum, "pos") {
        setLabel(QObject::tr("Satellite Position"));
        setValue("");
        setHelpText(QObject::tr("A textual representation of which "
                    "position the satellite is located at ('1W')"));
    };
};

class LofSwitch: public LineEditSetting, public DvbSatSetting {
public:
    LofSwitch(const CaptureCard& parent, int satnum):
        DvbSatSetting(parent, satnum, "lnb_lof_switch") {
        setLabel(QObject::tr("LNB LOF Switch"));
        setValue("11700000");
        setHelpText(QObject::tr("This defines at what frequency (in hz) "
                    "the LNB will do a switch from high to low setting, "
                    "and vice versa."));
    };
};

class LofHigh: public LineEditSetting, public DvbSatSetting {
public:
    LofHigh(const CaptureCard& parent, int satnum):
        DvbSatSetting(parent, satnum, "lnb_lof_hi") {
        setLabel(QObject::tr("LNB LOF High"));
        setValue("10600000");
        setHelpText(QObject::tr("This defines the offset (in hz) the "
                    "frequency coming from the lnb will be in high "
                    "setting."));
    };
};

class LofLow: public LineEditSetting, public DvbSatSetting {
public:
    LofLow(const CaptureCard& parent, int satnum):
        DvbSatSetting(parent, satnum, "lnb_lof_lo") {
        setLabel(QObject::tr("LNB LOF Low"));
        setValue("9750000");
        setHelpText(QObject::tr("This defines the offset (in hz) the "
                    "frequency coming from the lnb will be in low "
                    "setting."));
    };
};

class SatEditor: public ConfigurationWizard {
public:
    SatEditor(CaptureCard& _parent, int satnum):
        parent(_parent) {
        VerticalConfigurationGroup* g = new VerticalConfigurationGroup(false);
        g->addChild(new SatName(parent, satnum));
        g->addChild(new SatPos(parent, satnum));
        g->addChild(new LofSwitch(parent, satnum));
        g->addChild(new LofLow(parent, satnum));
        g->addChild(new LofHigh(parent, satnum));
        addChild(g);
    };
protected:
    CaptureCard& parent;
};

void DVBSatelliteWizard::editSat(int satnum)
{
    SatEditor ed(parent, satnum);
    ed.exec(db);
    list->load(db);
}

class DVBAdvConfigurationWizard: public ConfigurationWizard {
public:
    DVBAdvConfigurationWizard(CaptureCard& parent) {
        VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
        rec->setLabel(QObject::tr("Recorder Options"));
        rec->setUseLabel(false);

        rec->addChild(new DVBSwFilter(parent));
        rec->addChild(new DVBRecordTS(parent));
        rec->addChild(new DVBNoSeqStart(parent));
        rec->addChild(new DVBOnDemand(parent));
        rec->addChild(new DVBPidBufferSize(parent));
        rec->addChild(new DVBBufferSize(parent));
        addChild(rec);
    };
};

DVBAdvancedConfigMenu::DVBAdvancedConfigMenu(CaptureCard& a_parent)
                     : parent(a_parent) 
{
    setLabel(QObject::tr("Configuration Options"));
    TransButtonSetting* advcfg = new TransButtonSetting();
    TransButtonSetting* verify = new TransButtonSetting();
    TransButtonSetting* satellite = new TransButtonSetting();

    advcfg->setLabel(QObject::tr("Advanced Configuration"));
    verify->setLabel(QObject::tr("Card Verification Wizard"));
    satellite->setLabel(QObject::tr("Satellite Configuration"));

    addChild(advcfg);
    addChild(verify);
    addChild(satellite);

    connect(advcfg, SIGNAL(pressed()), this, SLOT(execACW()));
    connect(verify, SIGNAL(pressed()), this, SLOT(execCVW()));
    connect(satellite, SIGNAL(pressed()), this, SLOT(execSAT()));
}

void DVBAdvancedConfigMenu::execCVW() {
    DVBCardVerificationWizard cvw(parent.getDvbCard().toInt());
    cvw.exec(db);
}

void DVBAdvancedConfigMenu::execACW() {
    DVBAdvConfigurationWizard acw(parent);
    acw.exec(db);
}

void DVBAdvancedConfigMenu::execSAT() {
    DVBSatelliteWizard sw(parent);
    sw.exec(db);
}
