#include "config.h"
#include "videosource.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythcontext.h"
#include "datadirect.h"

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
#include <qmap.h>
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
#include <sys/stat.h>

#if defined(CONFIG_VIDEO4LINUX)
#include "videodev_myth.h"
#endif

const QString CardUtil::DVB = "DVB";

bool CardUtil::isCardPresent(QSqlDatabase *db, const QString &strType)
{
    QSqlQuery query = db->exec(QString("SELECT count(cardtype)"
                                       " FROM capturecard, cardinput"
                                       " WHERE cardinput.cardid = capturecard.cardid"
                                       " AND capturecard.cardtype=\"%1\""
                                       " AND capturecard.hostname = \"%2\"")
                                       .arg(strType).arg(gContext->GetHostName()));
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
    }

    return false;
}

enum CardUtil::DVB_TYPES CardUtil::cardDVBType(unsigned nVideoDev,
                                               QString &name)
{
    DVB_TYPES nRet = ERROR_OPEN;
#ifdef USING_DVB
    int fd_frontend = open(dvbdevice(DVB_DEV_FRONTEND, nVideoDev),
                           O_RDWR | O_NONBLOCK);
    if (fd_frontend >= 0)
    {
        struct dvb_frontend_info info;
        nRet = ERROR_PROBE;
        if (ioctl(fd_frontend, FE_GET_INFO, &info) >= 0)
        {
            name = info.name;
            switch(info.type)
            {
            case FE_QAM:
                nRet = QAM;
                break;
            case FE_QPSK:
                nRet = QPSK;
                break;
            case FE_OFDM:
                nRet = OFDM;
                break;
#if (DVB_API_VERSION_MINOR == 1)
            case FE_ATSC:
                nRet = ATSC;
                break;
#endif
            }
        }
        close(fd_frontend);
    }
#else
    (void)nVideoDev;
    (void)name;
#endif
    return nRet;
}

enum CardUtil::DVB_TYPES CardUtil::cardDVBType(unsigned nVideoDev)
{
    QString name;
    return CardUtil::cardDVBType(nVideoDev,name);
}

int CardUtil::videoDeviceFromCardID(QSqlDatabase *db, unsigned nCardID)
{
    int iRet = -1;
    QSqlQuery query = db->exec(QString("SELECT videodevice FROM capturecard "
                                       " WHERE capturecard.cardid=%1 ")
                                       .arg(nCardID));

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        iRet = query.value(0).toInt();
    }
    return iRet;
}

bool CardUtil::isDVB(QSqlDatabase *db, unsigned nCardID)
{
    bool fRet = false;
    QSqlQuery query = db->exec(QString("SELECT cardtype FROM capturecard "
                                       " WHERE capturecard.cardid=%1")
                                       .arg(nCardID));

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        if (query.value(0).toString() == "DVB")
           fRet = true;
    }
    return fRet;
}

enum CardUtil::DISEQC_TYPES CardUtil::diseqcType(QSqlDatabase *db,
                                                 unsigned nCardID)
{
    int iRet = 0;
    QSqlQuery query = db->exec(QString("SELECT dvb_diseqc_type "
                                       "FROM capturecard "
                                       " WHERE capturecard.cardid=%1 ")
                                       .arg(nCardID));

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        iRet = query.value(0).toInt();
    }
    return (DISEQC_TYPES)iRet;
}

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

    if (grabber == "tv_grab_de" || grabber == "tv_grab_se_swedb" || 
        grabber == "tv_grab_fi" || grabber == "tv_grab_es" ||
        grabber == "tv_grab_nl" || grabber == "tv_grab_jp" ||
        grabber == "tv_grab_no" || grabber == "tv_grab_pt")
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

    addTarget("tv_grab_se_swedb", new XMLTV_generic_config(parent, "tv_grab_se_swedb"));
    grabber->addSelection("Sweden (tv.swedb.se)","tv_grab_se_swedb");

    addTarget("tv_grab_no", new XMLTV_generic_config(parent, "tv_grab_no"));
    grabber->addSelection("Norway","tv_grab_no");

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

    addTarget("tv_grab_pt", new XMLTV_generic_config(parent, "tv_grab_pt"));
    grabber->addSelection("Portugal", "tv_grab_pt");
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

bool VideoSourceEditor::cardTypesInclude(QSqlDatabase *db, 
                                         const int &sourceID, 
                                         const QString &thecardtype) 
{
    QString querystr = QString("SELECT count(cardtype)"
                               " FROM cardinput,capturecard "
                               " WHERE capturecard.cardid = cardinput.cardid "
                               " AND cardinput.sourceid=%1 "
                               " AND capturecard.cardtype=\"%2\"")
                               .arg(sourceID)
                               .arg(thecardtype);

    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
    }

    return false;
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
    VideoDevice(const CaptureCard& parent,
                uint minor_min=0, uint minor_max=UINT_MAX):
        PathSetting(true), CCSetting(parent, "videodevice")
    {
        setLabel(QObject::tr("Video device"));

        // /dev/v4l/video*
        QDir dev("/dev/v4l", "video*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev, minor_min, minor_max, false);

        // /dev/video*
        dev.setPath("/dev");
        fillSelectionsFromDir(dev, minor_min, minor_max, false);

        // /dev/dtv/video*
        dev.setPath("/dev/dtv");
        fillSelectionsFromDir(dev, minor_min, minor_max, false);

        // /dev/dtv*
        dev.setPath("/dev");
        dev.setNameFilter("dtv*");
        fillSelectionsFromDir(dev, minor_min, minor_max, false);

        VERBOSE(VB_IMPORTANT, "");
    };

    void fillSelectionsFromDir(const QDir& dir,
                               uint minor_min, uint minor_max,
                               bool allow_duplicates)
    {
        const QFileInfoList *il = dir.entryInfoList();
        if (!il)
            return;
        
        QFileInfoListIterator it( *il );
        QFileInfo *fi;
        
        for(; (fi = it.current()) != 0; ++it)
        {
            struct stat st;
            QString filepath = fi->absFilePath();
            int err = lstat(filepath, &st);
            if (0==err)
            {
                if (S_ISCHR(st.st_mode))
                {
                    uint minor_num = minor(st.st_rdev);
                    // this is a character device, if in minor range to list
                    if (minor_min<=minor_num && 
                        minor_max>=minor_num &&
                        (allow_duplicates ||
                         (minor_list.find(minor_num)==minor_list.end())))
                    {
                        addSelection(filepath);
                        minor_list[minor_num]=1;
                    }
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Could not stat file: %1").arg(filepath));
            }
        }
    }

    static QStringList probeInputs(QString device);
    static QStringList fillDVBInputs(int dvb_diseqc_type);
    static QValueList<DVBDiseqcInputList> fillDVBInputsDiseqc(int dvb_diseqc_type);
private:
    QMap<uint, uint> minor_list;
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
        setLabel(QObject::tr("Record in TS format instead of PS."));
        setHelpText(QObject::tr("Disables Transport Stream to Program Stream "
                    "conversion. TS recording results in slightly bigger files,"
                    " but reduces the risk for processing errors."));
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

class DVBDiseqcType: public ComboBoxSetting, public CCSetting {
public:
    DVBDiseqcType(const CaptureCard& parent):
        CCSetting(parent, "dvb_diseqc_type") {
        setLabel(QObject::tr("DiSEqC Input Type: (DVB-S)"));
        addSelection("Single LNB / Input","0");
        addSelection("Tone Switch aka Mini DiSEqC (2-Way)","1");
        addSelection("DiSEqC v1.0 Switch (2-Way)","2");
        addSelection("DiSEqC v1.1 Switch (2-Way)","3");
        addSelection("DiSEqC v1.0 Switch (4-Way)","4");
        addSelection("DiSEqC v1.1 Switch (4-Way)","5");
        addSelection("DiSEqC v1.2 Positioner","6");
        addSelection("DiSEqC v1.3 Positioner (Goto X)","7");
        setHelpText(QObject::tr("Select the input type for DVB-S cards. "
                    "Leave as Single LNB/Input for DVB-C or DVB-T. "
                    "The inputs are mapped from Input Connections option "
                    "on the main menu"));
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

class FirewireModel: public ComboBoxSetting, public CCSetting {
    public:
	FirewireModel(const CaptureCard& parent):
 	CCSetting(parent, "firewire_model") {
            setLabel(QObject::tr("Firewire Model"));
            addSelection(QObject::tr("Other"));
            setHelpText(QObject::tr("Firewire Model is for future use in case "
                                    "there is a need to model specific "
                                    "workarounds.")); 

        }
};

class FirewirePort: public LineEditSetting, public CCSetting {
    public:
	FirewirePort(const CaptureCard& parent):
 	CCSetting(parent, "firewire_port") {
            setValue("0");
            setLabel(QObject::tr("Firewire Port"));
            setHelpText(QObject::tr("Firewire port on your firewire card."));
        }
};

class FirewireNode: public LineEditSetting, public CCSetting {
    public:
        FirewireNode(const CaptureCard& parent):
        CCSetting(parent, "firewire_node") {
            setValue("2");
            setLabel(QObject::tr("Firewire Node"));
            setHelpText(QObject::tr("Firewire node is the remote device."));
        }
};

class FirewireSpeed: public ComboBoxSetting, public CCSetting {
    public:
	FirewireSpeed(const CaptureCard& parent):
 	CCSetting(parent, "firewire_speed") {
            setLabel(QObject::tr("Firewire Speed"));
            addSelection(QObject::tr("100Mbps"),"0");
            addSelection(QObject::tr("200Mbps"),"1");
            addSelection(QObject::tr("400Mbps"),"2");
        }
};

class FirewireInput: public ComboBoxSetting, public CCSetting {
    public:
	FirewireInput(const CaptureCard& parent):
 	CCSetting(parent, "defaultinput") {
            setLabel(QObject::tr("Default Input"));
            addSelection("MPEG2TS");
            setHelpText(QObject::tr("Only MPEG2TS is supported at this time."));
        }
};

class FirewireConfigurationGroup: public VerticalConfigurationGroup {
public:
    FirewireConfigurationGroup(CaptureCard& a_parent):
        parent(a_parent) {
        setUseLabel(false);
        addChild(new FirewireModel(parent));
        addChild(new FirewirePort(parent));
        addChild(new FirewireNode(parent));
        addChild(new FirewireSpeed(parent));
   	addChild(new FirewireInput(parent));
    };
private:
    CaptureCard& parent;
};

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

class MPEGConfigurationGroup: public VerticalConfigurationGroup
{
  public:
    MPEGConfigurationGroup(CaptureCard& a_parent):
        parent(a_parent)
    {
        setUseLabel(false);

        VideoDevice* device;
        TunerCardInput* input;

        addChild(device = new VideoDevice(parent, 0, 15));
        addChild(input = new TunerCardInput(parent));
        connect(device, SIGNAL(valueChanged(const QString&)),
                input, SLOT(fillSelections(const QString&)));
        input->fillSelections(device->getValue());
    };
  private:
    CaptureCard& parent;
};

class pcHDTVConfigurationGroup: public VerticalConfigurationGroup
{
  public:
    pcHDTVConfigurationGroup(CaptureCard& a_parent): 
        parent(a_parent)
    {
        setUseLabel(false);

        VideoDevice *atsc_device = new VideoDevice(parent, 32);
        TunerCardInput *atsc_input = new TunerCardInput(parent);
        addChild(atsc_device);
        addChild(atsc_input);
        connect(atsc_device, SIGNAL(valueChanged(const QString&)),
                atsc_input, SLOT(fillSelections(const QString&)));
        atsc_input->fillSelections(atsc_device->getValue());
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
    addTarget("HDTV", new pcHDTVConfigurationGroup(parent));
    addTarget("MPEG", new MPEGConfigurationGroup(parent));
    addTarget("FIREWIRE", new FirewireConfigurationGroup(parent));
}

void CaptureCardGroup::triggerChanged(const QString& value) 
{
    QString own = (value == "MJPEG") ? "V4L" : value;
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
    QString query = QString("SELECT cardtype, videodevice, cardid, firewire_port, firewire_node "
                            "FROM capturecard WHERE hostname = \"%1\";")
                            .arg(gContext->GetHostName());
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            // dont like doing this..
            if(result.value(0).toString() == "FIREWIRE") {
                     setting->addSelection("[ " + result.value(0).toString() + " Port: " +
                                  result.value(3).toString() + ", Node: " +
                                  result.value(4).toString() + "]",
                                  result.value(2).toString());
            } else {
                     setting->addSelection("[ " + result.value(0).toString() + " : " +
                                  result.value(1).toString() + " ]",
                                  result.value(2).toString());
            }
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
    setting->addSelection(QObject::tr("FireWire Input"),
                          "FIREWIRE");
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

class LNBLofSwitch: public LineEditSetting, public CISetting {
public:
    LNBLofSwitch(const CardInput& parent):
        CISetting(parent, "lnb_lof_switch") {
        setLabel(QObject::tr("LNB LOF Switch"));
        setValue("11700000");
        setHelpText(QObject::tr("This defines at what frequency (in Hz) "
                    "the LNB will do a switch from high to low setting, "
                    "and vice versa."));
    };
};

class LNBLofHi: public LineEditSetting, public CISetting {
public:
    LNBLofHi(const CardInput& parent):
        CISetting(parent, "lnb_lof_hi") {
        setLabel(QObject::tr("LNB LOF High"));
        setValue("10600000");
        setHelpText(QObject::tr("This defines the offset (in Hz) the "
                    "frequency coming from the lnb will be in high "
                    "setting."));
    };
};

class LNBLofLo: public LineEditSetting, public CISetting {
public:
    LNBLofLo(const CardInput& parent):
        CISetting(parent, "lnb_lof_lo") {
        setLabel(QObject::tr("LNB LOF Low"));
        setValue("9750000");
        setHelpText(QObject::tr("This defines the offset (in Hz) the "
                    "frequency coming from the lnb will be in low "
                    "setting."));
    };
};

class DiseqcPos: public LineEditSetting, public CISetting {
public:
    DiseqcPos(const CardInput& parent):
        CISetting(parent, "diseqc_pos") {
        setLabel(QObject::tr("DiSEqC Satellite Location"));
        setValue("0.0");
        setHelpText(QObject::tr("The longitude of the satellite "
                    "you are aiming at.  For western hemisphere use "
                    "a negative value.  Value is in decimal."));
//        setVisible(false);
    };
//    void fillSelections(const QString& pos) {
//        setValue(pos);
//    };
};


class DiseqcPort: public LabelSetting, public CISetting {
public:
    DiseqcPort(const CardInput& parent):
        CISetting(parent, "diseqc_port") {
        setVisible(false);
    };
    void fillSelections(const QString& port) {
        setValue(port);
    };
};


class FreeToAir: public CheckBoxSetting, public CISetting {
public:
    FreeToAir(const CardInput& parent):
        CISetting(parent, "freetoaironly")
    {
        setValue(true);
        setLabel(QObject::tr("Free to air channels only."));
        setHelpText(QObject::tr("If set, only free to air channels will be "
                    "used."));
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

class DVBLNBChooser: public ComboBoxSetting {
public:
    DVBLNBChooser()
    {
        setLabel("LNB Settings: (DVB-S)");
        addSelection("Universal - 2");
        addSelection("DBS");
        addSelection("Universal - 1");
        addSelection("Custom");
        setHelpText("Select the LNB Settings for DVB-S cards. "
                    "For DVB-C and DVB-T you don't need to set these values. ");
    };
    void save(QSqlDatabase* db) { (void)db; };
    void load(QSqlDatabase* db) { (void)db; };

private:
    QSqlDatabase* db;
};

class CardInput: public ConfigurationWizard {
public:
    CardInput(int DVB = 0) {
        addChild(id = new ID());

        ConfigurationGroup *group = new VerticalConfigurationGroup(false);
        group->setLabel(QObject::tr("Connect source to input"));
        group->addChild(cardid = new CardID(*this));
        group->addChild(inputname = new InputName(*this));
        group->addChild(sourceid = new SourceID(*this));
        group->addChild(new InputPreference(*this));
        if (!DVB)
        {
            group->addChild(new ExternalChannelCommand(*this));
            group->addChild(new PresetTuner(*this));
        }
        group->addChild(new StartingChannel(*this));
#ifdef USING_DVB
        if (DVB)
        {
           group->addChild(diseqcpos = new DiseqcPos(*this));
           group->addChild(diseqcport = new DiseqcPort(*this));
           group->addChild(lnblofswitch = new LNBLofSwitch(*this));
           group->addChild(lnblofhi = new LNBLofHi(*this));
           group->addChild(lnbloflo = new LNBLofLo(*this));
           group->addChild(new FreeToAir(*this));
        }
#endif
        addChild(group);
    };

    int getInputID(void) const { return id->intValue(); };

    void loadByID(QSqlDatabase* db, int id);
    void loadByInput(QSqlDatabase* db, int cardid, QString input);
    QString getSourceName(void) const { return sourceid->getSelectionLabel(); };

    void fillDiseqcSettingsInput(QString _pos, QString _port);

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

    ID* id;
    CardID* cardid;
    InputName* inputname;
    SourceID* sourceid;
    QSqlDatabase* db;
    DVBLNBChooser *lnbsettings;
    DiseqcPos* diseqcpos;
    DiseqcPort* diseqcport;
    LNBLofSwitch *lnblofswitch;
    LNBLofLo *lnbloflo;
    LNBLofHi *lnblofhi;
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

    if (CardUtil::isDVB(db,_cardid))
    {
        int iVideoDev = CardUtil::videoDeviceFromCardID(db,_cardid);
        CardUtil::DVB_TYPES dvbType;
        if ((iVideoDev >= 0) &&
           ((dvbType = CardUtil::cardDVBType(iVideoDev))>CardUtil::ERROR_PROBE))
        {
            if (dvbType == CardUtil::QPSK)
            {
                //Check for diseqc type
                diseqcpos->setVisible(true);
                lnblofswitch->setVisible(true);
                lnbloflo->setVisible(true);
                lnblofhi->setVisible(true);
                if (CardUtil::diseqcType(db,_cardid) == CardUtil::POSITIONER_X)
                    diseqcpos->setEnabled(true);
                else
                    diseqcpos->setEnabled(false);
            }
            else
            {
                diseqcpos->setVisible(false);
                lnblofswitch->setVisible(false);
                lnbloflo->setVisible(false);
                lnblofhi->setVisible(false);
            }
        }
    }
}

void CardInput::save(QSqlDatabase* db) {
    if (sourceid->getValue() == "0")
        // "None" is represented by the lack of a row
        db->exec(QString("DELETE FROM cardinput WHERE cardinputid = %1;").arg(getInputID()));
    else
        ConfigurationWizard::save(db);
}

void CardInput::fillDiseqcSettingsInput(QString _pos, QString _port) {
    if (_port != "")
        diseqcport->setValue(_port);
    if (_pos != "")
        diseqcpos->setValue(_pos);
}

int CISetting::getInputID(void) const {
    return parent.getInputID();
}

int CCSetting::getCardID(void) const {
    return parent.getCardID();
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
    connect(dialog, SIGNAL(editButtonPressed()), this, SLOT(edit()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(del()));
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
    connect(dialog, SIGNAL(editButtonPressed()), this, SLOT(edit()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(del()));
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

    QString thequery = QString("SELECT cardid, videodevice, cardtype, dvb_diseqc_type, "
                               "firewire_port, firewire_node "
                               "FROM capturecard WHERE hostname = \"%1\";")
                              .arg(gContext->GetHostName());

    QSqlQuery capturecards = db->exec(thequery);
    if (capturecards.isActive() && capturecards.numRowsAffected() > 0)
        while (capturecards.next()) {
            int cardid = capturecards.value(0).toInt();
            QString videodevice(capturecards.value(1).toString());

            QStringList inputs;
            if (capturecards.value(2).toString() == "DVB")
            {
                QValueList<DVBDiseqcInputList> dvbinput;
                dvbinput = VideoDevice::fillDVBInputsDiseqc(capturecards.value(3).toInt());

                QValueList<DVBDiseqcInputList>::iterator it;
                for (it = dvbinput.begin(); it != dvbinput.end(); ++it)
                {
                    // IS DVB Check for CardInput class
                    CardInput* cardinput = new CardInput(1);
                    cardinput->loadByInput(db, cardid, (*it).input);

                    cardinput->fillDiseqcSettingsInput((*it).position,(*it).port);

                    cardinputs.push_back(cardinput);
                    QString index = QString::number(cardinputs.size()-1);

                    QString label = QString("%1 (%2) -> %3")
                        .arg("[ " + capturecards.value(2).toString() +
                             " : " + capturecards.value(1).toString() +
                             " ]")
                        .arg((*it).input)
                        .arg(cardinput->getSourceName());
                    addSelection(label, index);
                }
            }
            else if(capturecards.value(2).toString() == "FIREWIRE")
            {
                inputs = QStringList("MPEG2TS");
                for (QStringList::iterator i = inputs.begin();
                     i != inputs.end(); ++i)
                { 
                    CardInput* cardinput = new CardInput();
                    cardinput->loadByInput(db, cardid, *i);   
                    cardinputs.push_back(cardinput);
                    QString index = QString::number(cardinputs.size()-1);

                    QString label;
                    label = QString("%1 (%2) -> %3")
                        .arg("[ " + capturecards.value(2).toString() +
                             " Port: " + capturecards.value(3).toString() +
                             ", Node: " + capturecards.value(4).toString() +
                             " ]")
                        .arg(*i)
                        .arg(cardinput->getSourceName());
                    addSelection(label, index);
                }
            }
            else
            {
                inputs = VideoDevice::probeInputs(videodevice);
                for (QStringList::iterator i = inputs.begin(); 
                     i != inputs.end(); ++i)
                {
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
}

CardInputEditor::~CardInputEditor() {
    while (cardinputs.size() > 0) {
        delete cardinputs.back();
        cardinputs.pop_back();
    }
}

QStringList VideoDevice::fillDVBInputs(int dvb_diseqc_type) {
    QValueList<DVBDiseqcInputList> dvbinput;
    QStringList inputs;

    dvbinput = fillDVBInputsDiseqc(dvb_diseqc_type);

    QValueList<DVBDiseqcInputList>::iterator it;
    for (it = dvbinput.begin(); it != dvbinput.end(); ++it)
        inputs += (*it).input;

    return inputs;
}

QValueList<DVBDiseqcInputList> VideoDevice::fillDVBInputsDiseqc(int dvb_diseqc_type) {
    QValueList<DVBDiseqcInputList> list;

    switch (dvb_diseqc_type)
    {
        case 1: case 2: case 3:
            list.append(DVBDiseqcInputList(QString("DiSEqC Switch Input 1"),
                                           QString("0"), QString("")));
            list.append(DVBDiseqcInputList(QString("DiSEqC Switch Input 2"),
                                           QString("1"), QString("")));
            break;
        case 4: case 5:
            list.append(DVBDiseqcInputList(QString("DiSEqC Switch Input 1"),
                                           QString("0"), QString("")));
            list.append(DVBDiseqcInputList(QString("DiSEqC Switch Input 2"),
                                           QString("1"), QString("")));
            list.append(DVBDiseqcInputList(QString("DiSEqC Switch Input 3"),
                                           QString("2"), QString("")));
            list.append(DVBDiseqcInputList(QString("DiSEqC Switch Input 4"),
                                           QString("3"), QString("")));
            break;
        case 6:
            for (int x=1;x<50;x++)
                list.append(DVBDiseqcInputList(QString("DiSEqC v1.2 Motor Position %1").arg(x),QString(""),QString("%1").arg(x)));
            break;
        case 7:
            for (int x=1;x<20;x++)
                list.append ( DVBDiseqcInputList(QString("DiSEqC v1.3 Input %1").arg(x),QString(""),QString("%1").arg(x)));
            break;
        default:
            list.append ( DVBDiseqcInputList(QString("DVBInput"),QString(""),QString("")));
    }

    return list;
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
    QString name;
    bool fEnable=false;
    switch (CardUtil::cardDVBType(cardNumber.toInt(),name))
    {
        case CardUtil::ERROR_OPEN:
            cardname->setValue(QString("Could not open card #%1!")
                                       .arg(cardNumber));
            cardtype->setValue(strerror(errno));
            break;
        case CardUtil::ERROR_PROBE:
            cardname->setValue(QString("Could not get card info for card #%1!")
                                      .arg(cardNumber));
            cardtype->setValue(strerror(errno));
            break;
        case CardUtil::QPSK:
            cardtype->setValue("DVB-S");
            cardname->setValue(name);
            fEnable = true;
            break;
        case CardUtil::QAM:
            cardtype->setValue("DVB-C");
            cardname->setValue(name);
            break;
        case CardUtil::OFDM:
            cardtype->setValue("DVB-T");
            cardname->setValue(name);
            break;
        case CardUtil::ATSC:
            cardtype->setValue("ATSC");
            cardname->setValue(name);
            break;

    }
    defaultinput->setEnabled(fEnable);
    diseqctype->setEnabled(fEnable);
#else
    (void)cardNumber;
    cardtype->setValue(QString("Recompile with DVB-Support!"));
#endif
}

void DVBDefaultInput::fillSelections(const QString& type) {
    clearSelections();

    QStringList inputs = VideoDevice::fillDVBInputs(type.toInt());

    for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);

}

void TunerCardInput::fillSelections(const QString& device) {
    clearSelections();

    if (device == QString::null || device == "")
        return;

    QStringList inputs = VideoDevice::probeInputs(device);

    for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);
}

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent):
                       parent(a_parent) {
    setUseLabel(false);

    advcfg = new TransButtonSetting();
    advcfg->setLabel(QObject::tr("Advanced Configuration"));

    DVBCardNum* cardnum = new DVBCardNum(parent);
    cardname = new DVBCardName();
    cardtype = new DVBCardType();

    defaultinput = new DVBDefaultInput(parent);
    diseqctype = new DVBDiseqcType(parent);

    addChild(cardnum);
    addChild(cardname);
    addChild(cardtype);

    addChild(new DVBAudioDevice(parent));
    addChild(new DVBVbiDevice(parent));
    addChild(diseqctype);
    addChild(defaultinput);
    addChild(advcfg);

    connect(cardnum, SIGNAL(valueChanged(const QString&)),
            this, SLOT(probeCard(const QString&)));
    connect(cardnum, SIGNAL(valueChanged(const QString&)),
            &parent, SLOT(setDvbCard(const QString&)));
    connect(diseqctype, SIGNAL(valueChanged(const QString&)),
            defaultinput, SLOT(fillSelections(const QString&)));
    connect(advcfg, SIGNAL(pressed()), &parent, SLOT(execDVBConfigMenu()));

    defaultinput->setEnabled(false);
    diseqctype->setEnabled(false);

    cardnum->setValue(0);

    defaultinput->fillSelections(diseqctype->getValue());
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

    MythPopupBox *popup = new MythPopupBox(gContext->GetMainWindow(),
                             tr("Advanced Configuration"));

    QButton *button;
    button = popup->addButton(tr("Recording Options"), this, 
                              SLOT(execAdvConfigWiz()));
    button->setFocus();
    button = popup->addButton(tr("Diseqc"), this, SLOT(execDiseqcWiz()));
    popup->ExecPopup();
    delete popup;
}

class DVBAdvConfigurationWizard: public ConfigurationWizard
{
public:
    DVBAdvConfigurationWizard(CaptureCard& parent)
    {
        VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);        rec->setLabel(QObject::tr("Recorder Options"));
        rec->setUseLabel(false);

        rec->addChild(new DVBSwFilter(parent));
        rec->addChild(new DVBRecordTS(parent));
        rec->addChild(new DVBNoSeqStart(parent));
        rec->addChild(new DVBOnDemand(parent));
        rec->addChild(new DVBPidBufferSize(parent));
        rec->addChild(new DVBBufferSize(parent));
        addChild(rec);
    }
};

static GlobalLineEdit *DiseqcLatitude()
{
    GlobalLineEdit *gc = new GlobalLineEdit("latitude");
    gc->setLabel("Latitude");
    gc->setHelpText(QObject::tr("The Latitude of your satellite dishes "
                    "location on the Earth.. "
                    " This is used with DiSEqC Motor Support.  Format 35.78"
                    " for 35.78 degrees North Longitude"));
    return gc;
};

static GlobalLineEdit *DiseqcLongitude()
{
    GlobalLineEdit *gc = new GlobalLineEdit("longitude");
    gc->setLabel("Longitude");
    gc->setHelpText(QObject::tr("The Longitude of your satellite dishes "
                    "location on the Earth.. "
                    " This is used with DiSEqC Motor Support.  Format -78.93"
                    " for 78.93 degrees West Longitude"));
    return gc;
};

class DVBDiseqcConfigurationWizard: public ConfigurationWizard
{
public:
    DVBDiseqcConfigurationWizard()
    {
        VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
        rec->setLabel(QObject::tr("Diseqc Options"));
        rec->setUseLabel(false);

        rec->addChild(DiseqcLatitude());
        rec->addChild(DiseqcLongitude());
        addChild(rec);
    }
};

void CaptureCard::execAdvConfigWiz()
{
    DVBAdvConfigurationWizard acw(*this);
    acw.exec(db);
}

void CaptureCard::execDiseqcWiz()
{
    DVBDiseqcConfigurationWizard diseqcWiz;
    diseqcWiz.exec(db);
}
