// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

// C++ headers
#include <iostream>

// Qt headers
#include <qapplication.h>
#include <qstringlist.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
#include <qmap.h>
#include <qdir.h>

// MythTV headers
#include "mythconfig.h"
#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "videosource.h"
#include "datadirect.h"
#include "scanwizard.h"
#include "util.h"
#include "cardutil.h"
#include "sourceutil.h"

#ifdef USING_DVB
#include <linux/dvb/frontend.h>
#include "dvbdev.h"
#endif

#if defined(CONFIG_VIDEO4LINUX)
#include "videodev_myth.h"
#endif

class RecorderOptions: public ConfigurationWizard
{
  public:
    RecorderOptions(CaptureCard& parent);
};

class DVBDiSEqCConfigurationWizard: public ConfigurationWizard
{
  public:
    DVBDiSEqCConfigurationWizard(CaptureCard &parent);
};

QString VSSetting::whereClause(void)
{
    return QString("sourceid = %1").arg(parent.getSourceID());
}

QString VSSetting::setClause(void)
{
    return QString("sourceid = %1, %2 = '%3'")
        .arg(parent.getSourceID())
        .arg(getColumn())
        .arg(getValue());
}

QString CCSetting::whereClause(void)
{
    return QString("cardid = %1").arg(parent.getCardID());
}

QString CCSetting::setClause(void)
{
    return QString("cardid = %1, %2 = '%3'")
        .arg(parent.getCardID())
        .arg(getColumn())
        .arg(getValue());
}

class XMLTVGrabber: public ComboBoxSetting, public VSSetting
{
  public:
    XMLTVGrabber(const VideoSource& parent)
      : VSSetting(parent, "xmltvgrabber")
    {
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

class UseEIT : public CheckBoxSetting, public VSSetting
{
  public:
    UseEIT(const VideoSource& parent) : VSSetting(parent, "useeit")
    {
        setLabel(QObject::tr("Perform EIT Scan"));
        setHelpText(QObject::tr(
                        "If this is enabled the data in this source will be "
                        "updated with listing data provided by the channels "
                        "themselves 'over-the-air'."));
    }
};

class DataDirectUserID: public LineEditSetting, public VSSetting
{
  public:
    DataDirectUserID(const VideoSource& parent)
      : VSSetting(parent, "userid")
    {
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
                                              const QString &pwd,
                                              int _source) 
{
    (void) uid;
    (void) pwd;
    (void) _source;
#ifdef USING_BACKEND
    if (uid.isEmpty() || pwd.isEmpty())
        return;

    qApp->processEvents();

    QString waitMsg;
    if (_source == DD_ZAP2IT)
    
        waitMsg = tr("Fetching lineups from DataDirect service...");
    else
        waitMsg = tr("Fetching lineups from the Myth Plus service...");
        
    VERBOSE(VB_GENERAL, waitMsg);
    MythProgressDialog pdlg(waitMsg, 2);

    clearSelections();

    DataDirectProcessor ddp(_source);
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
#else // USING_BACKEND
    VERBOSE(VB_IMPORTANT, "You must compile the backend "
            "to set up a DataDirect line-up");
#endif // USING_BACKEND
}

void DataDirect_config::load() 
{
    VerticalConfigurationGroup::load();
    if ((userid->getValue() != lastloadeduserid) || 
        (password->getValue() != lastloadedpassword)) 
    {
        lineupselector->fillSelections(userid->getValue(), 
                                       password->getValue(),
                                       source);
        lastloadeduserid = userid->getValue();
        lastloadedpassword = password->getValue();
    }
}

DataDirect_config::DataDirect_config(const VideoSource& _parent, int _source)
  : parent(_parent) 
{
    source = _source;
    setUseLabel(false);
    setUseFrame(false);

    HorizontalConfigurationGroup *lp =
        new HorizontalConfigurationGroup(false, false, true, true);

    lp->addChild(userid   = new DataDirectUserID(parent));
    lp->addChild(password = new DataDirectPassword(parent));
    lp->addChild(button   = new DataDirectButton());
    addChild(lp);

    addChild(lineupselector = new DataDirectLineupSelector(parent));
    addChild(new UseEIT(parent));

    connect(button, SIGNAL(pressed()),
            this,   SLOT(fillDataDirectLineupSelector()));
}

void DataDirect_config::fillDataDirectLineupSelector(void)
{
    lineupselector->fillSelections(
        userid->getValue(), password->getValue(), source);
}

XMLTV_generic_config::XMLTV_generic_config(const VideoSource& _parent, 
                                           QString _grabber)
                    : parent(_parent), grabber(_grabber) 
{
    setUseLabel(false);
    setUseFrame(false);

    TransLabelSetting *label = new TransLabelSetting();
    label->setLabel(grabber);
    label->setValue(
        QObject::tr("Configuration will run in the terminal window"));
    addChild(label);
    addChild(new UseEIT(parent));
}

void XMLTV_generic_config::save()
{
    VerticalConfigurationGroup::save();
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
    QString filename = QString("%1/%2.xmltv")
        .arg(MythContext::GetConfDir()).arg(parent.getSourceName());

    command = QString("%1 --config-file '%2' --configure")
        .arg(grabber).arg(filename);

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

    QString err_msg = QObject::tr(
        "You MUST run 'mythfilldatabase --manual the first time,\n "
        "instead of just 'mythfilldatabase'.\nYour grabber does not provide "
        "channel numbers, so you have to set them manually.");

    if (grabber == "tv_grab_de_tvtoday" || grabber == "tv_grab_se_swedb" || 
        grabber == "tv_grab_fi" || grabber == "tv_grab_es" ||
        grabber == "tv_grab_nl" || grabber == "tv_grab_jp" ||
        grabber == "tv_grab_no" || grabber == "tv_grab_pt" ||
        grabber == "tv_grab_ee" || grabber == "tv_grab_be_tvb" ||
        grabber == "tv_grab_be_tlm" || grabber == "tv_grab_is" ||
        grabber == "tv_grab_br" || grabber == "tv_grab_cz")
        
    {
        VERBOSE(VB_IMPORTANT, "\n" << err_msg);
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), QObject::tr("Warning."), err_msg);
    }

    pdlg.setProgress( 2 );    
    pdlg.Close();
}

EITOnly_config::EITOnly_config(const VideoSource& _parent)
    : VerticalConfigurationGroup(false, false, true, true)
{
    useeit = new UseEIT(_parent);
    useeit->setValue(true);
    useeit->setVisible(false);
    addChild(useeit);

    TransLabelSetting *label;
    label=new TransLabelSetting();
    label->setValue(QObject::tr("Use only the transmitted guide data."));
    addChild(label);
    label=new TransLabelSetting();
    label->setValue(
        QObject::tr("This will usually only work with ATSC or DVB channels,"));
    addChild(label);
    label=new TransLabelSetting();
    label->setValue(
        QObject::tr("and generally provides data only for the next few days."));
    addChild(label);
}

void EITOnly_config::save()
{
    // Force this value on
    useeit->setValue(true);
    useeit->save();
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
    
#ifdef USING_DVB_EIT
    addTarget("eitonly", new EITOnly_config(parent));
    grabber->addSelection("Transmitted guide only (EIT)", "eitonly");
#endif

    addTarget("tv_grab_de_tvtoday", new XMLTV_generic_config(parent, "tv_grab_de_tvtoday"));
    grabber->addSelection("Germany (tvtoday)", "tv_grab_de_tvtoday");

    addTarget("tv_grab_se_swedb", new XMLTV_generic_config(parent, "tv_grab_se_swedb"));
    grabber->addSelection("Sweden (tv.swedb.se)","tv_grab_se_swedb");

    addTarget("tv_grab_no", new XMLTV_generic_config(parent, "tv_grab_no"));
    grabber->addSelection("Norway","tv_grab_no");

    addTarget("tv_grab_uk_rt", new XMLTV_generic_config(parent, "tv_grab_uk_rt"));
    grabber->addSelection("United Kingdom (alternative)","tv_grab_uk_rt");

    addTarget("tv_grab_au", new XMLTV_generic_config(parent, "tv_grab_au"));
    grabber->addSelection("Australia", "tv_grab_au");

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

    addTarget("tv_grab_ee", new XMLTV_generic_config(parent, "tv_grab_ee"));
    grabber->addSelection("Estonia", "tv_grab_ee");

    addTarget("tv_grab_be_tvb", new XMLTV_generic_config(parent, "tv_grab_be_tvb"));
    grabber->addSelection("Belgium (Dutch)", "tv_grab_be_tvb");

    addTarget("tv_grab_be_tlm", new XMLTV_generic_config(parent, "tv_grab_be_tlm"));
    grabber->addSelection("Belgium (French)", "tv_grab_be_tlm");

    addTarget("tv_grab_is", new XMLTV_generic_config(parent, "tv_grab_is"));
    grabber->addSelection("Iceland", "tv_grab_is");

    addTarget("tv_grab_br", new XMLTV_generic_config(parent, "tv_grab_br"));
    grabber->addSelection("Brazil", "tv_grab_br");

    addTarget("tv_grab_cz", new XMLTV_generic_config(parent, "tv_grab_cz"));
    grabber->addSelection("Czech Republic", "tv_grab_cz");
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

bool VideoSourceEditor::cardTypesInclude(const int &sourceID, 
                                         const QString &thecardtype) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(cardtype)"
                  " FROM cardinput,capturecard "
                  " WHERE capturecard.cardid = cardinput.cardid "
                  " AND cardinput.sourceid= :SOURCEID "
                  " AND capturecard.cardtype= :CARDTYPE ;");
    query.bindValue(":SOURCEID", sourceID);
    query.bindValue(":CARDTYPE", thecardtype);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
    }

    return false;
}

void VideoSource::fillSelections(SelectSetting* setting) 
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT name, sourceid FROM videosource;");

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next())
        {
            setting->addSelection(result.value(0).toString(),
                                  result.value(1).toString());
        }
    }
}

void VideoSource::loadByID(int sourceid) 
{
    id->setValue(sourceid);
    load();
}

class VideoDevice: public PathSetting, public CCSetting
{
  public:
    VideoDevice(const CaptureCard& parent,
                uint minor_min=0, uint minor_max=UINT_MAX)
      : PathSetting(true), CCSetting(parent, "videodevice")
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

  private:
    QMap<uint, uint> minor_list;
};

class VbiDevice: public PathSetting, public CCSetting
{
  public:
    VbiDevice(const CaptureCard& parent)
      : PathSetting(true), CCSetting(parent, "vbidevice")
    {
        setLabel(QObject::tr("VBI device"));
        QDir dev("/dev", "vbi*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/v4l");
        fillSelectionsFromDir(dev);
    };
};

class AudioDevice: public PathSetting, public CCSetting
{
  public:
    AudioDevice(const CaptureCard& parent)
      : PathSetting(true), CCSetting(parent, "audiodevice")
    {
        setLabel(QObject::tr("Audio device"));
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
        addSelection(QObject::tr("(None)"), "/dev/null");
    };
};

class SignalTimeout: public SpinBoxSetting, public CCSetting
{
  public:
    SignalTimeout(const CaptureCard& parent, int start_val)
      : SpinBoxSetting(start_val,60000,250),
        CCSetting(parent, "signal_timeout")
    {
        setLabel(QObject::tr("Signal Timeout (msec)"));
        setHelpText(QObject::tr(
                        "Maximum time MythTV waits for any signal when "
                        "scanning for channels."));
    };
};

class ChannelTimeout: public SpinBoxSetting, public CCSetting
{
  public:
    ChannelTimeout(const CaptureCard& parent, int start_val)
      : SpinBoxSetting(start_val,65000,250),
        CCSetting(parent, "channel_timeout")
    {
        setLabel(QObject::tr("Tuning Timeout (msec)"));
        setHelpText(QObject::tr(
                        "Maximum time MythTV waits for a channel lock "
                        "when scanning for channels. Or, for issuing "
                        "a warning in LiveTV mode."));
    };
};

class AudioRateLimit: public ComboBoxSetting, public CCSetting
{
  public:
    AudioRateLimit(const CaptureCard& parent)
      : CCSetting(parent, "audioratelimit")
    {
        setLabel(QObject::tr("Audio sampling rate limit"));
        addSelection(QObject::tr("(None)"), "0");
        addSelection("32000");
        addSelection("44100");
        addSelection("48000");
    };
};

class SkipBtAudio: public CheckBoxSetting, public CCSetting
{
  public:
    SkipBtAudio(const CaptureCard& parent)
      : CCSetting(parent, "skipbtaudio")
    {
        setLabel(QObject::tr("Do not adjust volume"));
        setHelpText(
            QObject::tr("Check this option for budget BT878 based "
                        "DVB-T cards such as the AverTV DVB-T that "
                        "require the audio volume left alone."));
   };
};

class DVBCardNum: public SpinBoxSetting, public CCSetting
{
  public:
    DVBCardNum(const CaptureCard& parent)
      : SpinBoxSetting(0,7,1), CCSetting(parent, "videodevice")
    {
        setLabel(QObject::tr("DVB Card Number"));
        setHelpText(
            QObject::tr("When you change this setting, the text below "
                        "should change to the name and type of your card. "
                        "If the card cannot be opened, an error message "
                        "will be displayed."));
    };
};

class DVBCardType: public LabelSetting, public TransientStorage
{
  public:
    DVBCardType()
    {
        setLabel(QObject::tr("Subtype"));
    };
};

class DVBCardName: public LabelSetting, public TransientStorage
{
  public:
    DVBCardName()
    {
        setLabel(QObject::tr("Frontend ID"));
    };
};

class DVBNoSeqStart: public CheckBoxSetting, public CCSetting
{
  public:
    DVBNoSeqStart(const CaptureCard& parent)
      : CCSetting(parent, "dvb_wait_for_seqstart")
    {
        setLabel(QObject::tr("Wait for SEQ start header."));
        setValue(true);
        setHelpText(
            QObject::tr("Normally the dvb-recording will drop packets "
                        "from the card until a sequence start header is seen. "
                        "This option turns off this feature."));
    };
};

class DVBOnDemand: public CheckBoxSetting, public CCSetting
{
  public:
    DVBOnDemand(const CaptureCard& parent)
      : CCSetting(parent, "dvb_on_demand")
    {
        setLabel(QObject::tr("Open DVB card on demand"));
        setValue(true);
        setHelpText(
            QObject::tr("This option makes the backend dvb-recorder "
                        "only open the card when it is actually in-use leaving "
                        "it free for other programs at other times."));
    };
};

class DVBDiSEqCType: public ComboBoxSetting, public CCSetting
{
  public:
    DVBDiSEqCType(const CaptureCard& parent)
      : CCSetting(parent, "dvb_diseqc_type")
    {
        setLabel(QObject::tr("DiSEqC Input Type: (DVB-S)"));
        addSelection(QObject::tr("Single LNB / Input"),
                     QString::number(DISEQC_SINGLE));
        addSelection(QObject::tr("Tone Switch aka Mini DiSEqC (2-Way)"),
                     QString::number(DISEQC_MINI_2));
        addSelection(QObject::tr("DiSEqC v1.0 Switch (2-Way)"),
                     QString::number(DISEQC_SWITCH_2_1_0));
        addSelection(QObject::tr("DiSEqC v1.1 Switch (2-Way)"),
                     QString::number(DISEQC_SWITCH_2_1_1));
        addSelection(QObject::tr("DiSEqC v1.0 Switch (4-Way)"),
                     QString::number(DISEQC_SWITCH_4_1_0));
        addSelection(QObject::tr("DiSEqC v1.1 Switch (4-Way)"),
                     QString::number(DISEQC_SWITCH_4_1_1));
        addSelection(QObject::tr("DiSEqC v1.2 Positioner"),
                     QString::number(DISEQC_POSITIONER_1_2));
        addSelection(QObject::tr("DiSEqC v1.3 Positioner (Goto X)"),
                     QString::number(DISEQC_POSITIONER_X));
        addSelection(QObject::tr("DiSEqC v1.1 or 2.1 (10-way method2)"),
                     QString::number(DISEQC_POSITIONER_1_2_SWITCH_2));
        addSelection(QObject::tr("SW21 Switch (2-Way)"),
                     QString::number(DISEQC_SW21));
        addSelection(QObject::tr("SW64 Switch (3-Way)"),
                     QString::number(DISEQC_SW64));
        setHelpText(QObject::tr("Select the input type for DVB-S cards. "
                    "Leave as Single LNB/Input for DVB-C or DVB-T. "
                    "The inputs are mapped from Input Connections option "
                    "on the main menu"));
    };
};

class FirewireModel: public ComboBoxSetting, public CCSetting
{
  public:
    FirewireModel(const CaptureCard& parent)
      : CCSetting(parent, "firewire_model")
    {
        setLabel(QObject::tr("Cable box model"));
        addSelection(QObject::tr("Other"));
        addSelection("DCT-6200");
        addSelection("SA3250HD");
        setHelpText(QObject::tr("Firewire Model is for future use in case "
                                "there is a need to model specific "
                                "workarounds.")); 
    }
};

class FirewireConnection: public ComboBoxSetting, public CCSetting
{
      public:
       FirewireConnection(const CaptureCard& parent):
       CCSetting(parent, "firewire_connection") {
            setLabel(QObject::tr("Connection Type"));
            addSelection(QObject::tr("Point to Point"),"0");
            addSelection(QObject::tr("Broadcast"),"1");
        }
};

class FirewirePort: public LineEditSetting, public CCSetting
{
  public:
    FirewirePort(const CaptureCard& parent)
      : CCSetting(parent, "firewire_port")
    {
        setValue("0");
        setLabel(QObject::tr("IEEE-1394 Port"));
        setHelpText(QObject::tr("Firewire port on your firewire card."));
    }
};

class FirewireNode: public LineEditSetting, public CCSetting
{
  public:
    FirewireNode(const CaptureCard& parent)
      : CCSetting(parent, "firewire_node")
    {
        setValue("2");
        setLabel(QObject::tr("Node"));
        setHelpText(QObject::tr("Firewire node is the remote device."));
    }
};

class FirewireSpeed: public ComboBoxSetting, public CCSetting
{
  public:
    FirewireSpeed(const CaptureCard& parent)
      : CCSetting(parent, "firewire_speed")
    {
        setLabel(QObject::tr("Speed"));
        addSelection(QObject::tr("100Mbps"),"0");
        addSelection(QObject::tr("200Mbps"),"1");
        addSelection(QObject::tr("400Mbps"),"2");
    }
};

class FirewireInput: public ComboBoxSetting, public CCSetting
{
  public:
    FirewireInput(const CaptureCard& parent)
      : CCSetting(parent, "defaultinput")
    {
        setLabel(QObject::tr("Default Input"));
        addSelection("MPEG2TS");
        setHelpText(QObject::tr("Only MPEG2TS is supported at this time."));
    }
};

class FirewireConfigurationGroup: public VerticalConfigurationGroup
{
  public:
    FirewireConfigurationGroup(CaptureCard& a_parent):
        parent(a_parent) {
        setUseLabel(false);
        HorizontalConfigurationGroup *hg0 =
            new HorizontalConfigurationGroup(false, false, true, true);
        hg0->addChild(new FirewireModel(parent));
        hg0->addChild(new FirewireConnection(parent));
        addChild(hg0);
        HorizontalConfigurationGroup *hg1 =
            new HorizontalConfigurationGroup(false, false, true, true);
        hg1->addChild(new FirewirePort(parent));
        hg1->addChild(new FirewireNode(parent));
        hg1->addChild(new FirewireSpeed(parent));
        addChild(hg1);
   	addChild(new FirewireInput(parent));
    };
  private:
    CaptureCard& parent;
};

class DBOX2Port: public LineEditSetting, public CCSetting {
    public:
      DBOX2Port(const CaptureCard& parent):
      CCSetting(parent, "dbox2_port") {
            setValue("31338");
            setLabel(QObject::tr("DBOX2 Streaming Port"));
            setHelpText(QObject::tr("DBOX2 streaming port on your DBOX2."));
        }
};

class DBOX2HttpPort: public LineEditSetting, public CCSetting {
  public:
      DBOX2HttpPort(const CaptureCard& parent):
      CCSetting(parent, "dbox2_httpport") {
            setValue("80");
            setLabel(QObject::tr("DBOX2 HTTP Port"));
            setHelpText(QObject::tr("DBOX2 http port on your DBOX2."));
        }
};
class DBOX2Host: public LineEditSetting, public CCSetting {
   public:
       DBOX2Host(const CaptureCard& parent):
       CCSetting(parent, "dbox2_host") {
           setValue("dbox");
           setLabel(QObject::tr("DBOX2 Host IP"));
           setHelpText(QObject::tr("DBOX2 Host IP is the remote device."));
       }
};

class DBOX2ConfigurationGroup: public VerticalConfigurationGroup {
public:
   DBOX2ConfigurationGroup(CaptureCard& a_parent):
       parent(a_parent) {
       setUseLabel(false);
       addChild(new DBOX2Port(parent));
       addChild(new DBOX2HttpPort(parent));
       addChild(new DBOX2Host(parent));
   };
  private:
     CaptureCard& parent;
 };




class V4LConfigurationGroup: public VerticalConfigurationGroup
{
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

        VideoDevice *atsc_device = new VideoDevice(parent, 0, 64);
        TunerCardInput *atsc_input = new TunerCardInput(parent);
        SignalTimeout *signal_timeout = new SignalTimeout(parent, 500);
        ChannelTimeout *channel_timeout = new ChannelTimeout(parent, 2000);
        addChild(atsc_device);
        addChild(signal_timeout);
        addChild(channel_timeout);
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
    addTarget("DBOX2", new DBOX2ConfigurationGroup(parent));
}

void CaptureCardGroup::triggerChanged(const QString& value) 
{
    QString own = (value == "MJPEG" || value == "GO7007") ? "V4L" : value;
    TriggeredConfigurationGroup::triggerChanged(own);
}

CaptureCard::CaptureCard(bool use_card_group) 
{
    // must be first
    addChild(id = new ID());
    addChild(parentid = new ParentID(*this));
    if (use_card_group)
        addChild(new CaptureCardGroup(*this));
    addChild(new Hostname(*this));
}

void CaptureCard::setParentID(int id)
{
    parentid->setValue(QString::number(id));
}

void CaptureCard::fillSelections(SelectSetting* setting) 
{
    CaptureCard::fillSelections(setting, false);
}

void CaptureCard::fillSelections(SelectSetting* setting, bool no_children) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT cardtype, videodevice, cardid, "
        "       firewire_port, firewire_node, "
        "       dbox2_port, dbox2_host, dbox2_httpport "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME";
    if (no_children)
        qstr += " AND parentid='0'";

    query.prepare(qstr);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        { 
            // dont like doing this..
            if (query.value(0).toString() == "FIREWIRE") 
            {
                setting->addSelection(
                      "[ " + query.value(0).toString() + " Port: " +
                             query.value(3).toString() + ", Node: " +
                             query.value(4).toString() + "]",
                             query.value(2).toString());
            } 
            else if (query.value(0).toString() == "DBOX2")
            {
                setting->addSelection(
                    "[ " + query.value(0).toString() + " " + 
                    "Host IP: " + query.value(6).toString() +  ", " +
                    "Streaming-Port: " + query.value(5).toString() + ", " +
                    "Http-Port: " + query.value(7).toString() + 
                    "] ", query.value(2).toString());
	    }
            else 
            {
                setting->addSelection(
                       "[ " + query.value(0).toString() + " : " +
                              query.value(1).toString() + " ]",
                              query.value(2).toString());
            }
        }
    }   
}

void CaptureCard::loadByID(int cardid) 
{
    id->setValue(cardid);
    load();
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
    setting->addSelection(
        QObject::tr("Analog V4L capture card"), "V4L");
    setting->addSelection(
        QObject::tr("MJPEG capture card (Matrox G200, DC10)"), "MJPEG");
    setting->addSelection(
        QObject::tr("MPEG-2 encoder card (PVR-x50, PVR-500)"), "MPEG");
    setting->addSelection(
        QObject::tr("DVB DTV capture card (v3.x)"), "DVB");
    setting->addSelection(
        QObject::tr("pcHDTV DTV capture card (w/V4L drivers)"), "HDTV");
    setting->addSelection(
        QObject::tr("FireWire cable box"), "FIREWIRE");
    setting->addSelection(
        QObject::tr("USB MPEG-4 encoder box (Plextor ConvertX, etc)"),
        "GO7007");
    setting->addSelection(
        QObject::tr("DBox2 TCP/IP cable box"), "DBOX2");
}

class CardID: public SelectLabelSetting, public CISetting {
  public:
    CardID(const CardInput& parent):
        CISetting(parent, "cardid") {
        setLabel(QObject::tr("Capture device"));
    };

    virtual void load() {
        fillSelections();
        CISetting::load();
    };

    void fillSelections() {
        CaptureCard::fillSelections(this);
    };
};

class ChildID: public CISetting
{
  public:
    ChildID(const CardInput &parent) : CISetting(parent, "childcardid")
    {
        setValue("0");
        setVisible(false);
    }
};

class InputDisplayName: public LineEditSetting, public CISetting
{
  public:
    InputDisplayName(const CardInput& parent):
        CISetting(parent, "displayname")
    {
        setLabel(QObject::tr("Display Name (optional)"));
        setHelpText(QObject::tr(
                        "This name is displayed on screen when live TV begins "
                        "and when changing the selected input or card. If you "
                        "use this, make sure the information is unique for "
                        "each input."));
    };
};

class SourceID: public ComboBoxSetting, public CISetting {
  public:
    SourceID(const CardInput& parent):
        CISetting(parent, "sourceid") {
        setLabel(QObject::tr("Video source"));
        addSelection(QObject::tr("(None)"), "0");
    };

    virtual void load() {
        fillSelections();
        CISetting::load();
    };

    void fillSelections() {
        clearSelections();
        addSelection(QObject::tr("(None)"), "0");
        VideoSource::fillSelections(this);
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

class DiSEqCPos: public LineEditSetting, public CISetting
{
  public:
    DiSEqCPos(const CardInput& parent)
        : CISetting(parent, "diseqc_pos")
    {
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


class DiSEqCPort: public LabelSetting, public CISetting
{
  public:
    DiSEqCPort(const CardInput& parent)
        : CISetting(parent, "diseqc_port")
    {
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

class RadioServices: public CheckBoxSetting, public CISetting {
public:
    RadioServices(const CardInput& parent):
        CISetting(parent, "radioservices")
    {
        setValue(true);
        setLabel(QObject::tr("Radio channels."));
        setHelpText(QObject::tr("If set, radio channels will also be included.")); 
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

void StartingChannel::SetSourceID(const QString &sourceid)
{
    //VERBOSE(VB_IMPORTANT, "StartingChannel::SetSourceID("<<sourceid<<")");
    clearSelections();
    if (sourceid.isEmpty() || !sourceid.toUInt())
        return;

    // Get the existing starting channel
    QString startChan = QString::null;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT startchan "
        "FROM cardinput "
        "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", getInputID());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("SetSourceID -- get start chan", query);
    else if (query.next())
        startChan = query.value(0).toString();

    // Get the existing channels on the connected source
    query.prepare(
        "SELECT channum "
        "FROM channel "
        "WHERE sourceid = :SOURCEID "
        "ORDER BY atscsrcid, channum");
    query.bindValue(":SOURCEID", sourceid.toUInt());

    QString nnsc = startChan.isEmpty() ? "" : startChan;
    if (!query.exec() || !query.isActive())
    {
        addSelection(tr("DB Error, see console"), nnsc);
        MythContext::DBError("SetSourceID -- get channels", query);
    }
    else if (query.size() > 0)
    {
        // If there are channels add them, and
        // highlight the old start channel
        while (query.next())
        {
            const QString channum = query.value(0).toString();
            addSelection(channum, channum, channum == startChan);
        }
    }
    else
    {
        addSelection(tr("Please add channels to this source"), nnsc);
    }
}

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
        setHelpText(
            QObject::tr("Select the LNB Settings for DVB-S cards.") + " " +
            QObject::tr("For DVB-C and DVB-T you don't need to "
                        "set these values."));
    };
    void save() {};
    void load() {};

private:
};

class DishNetEIT: public CheckBoxSetting, public CISetting
{
  public:
    DishNetEIT(const CardInput& parent)
        : CISetting(parent, "dishnet_eit")
    {
        setLabel(QObject::tr("Use DishNet Long-term EIT Data"));
        setValue(false);
        setHelpText(
            QObject::tr(
                "If you point your satellite dish toward DishNet's birds, "
                "you may wish to enable this feature. For best results, "
                "enable general EIT collection as well."));
    };
};

CardInput::CardInput(bool isDVBcard)
{
    addChild(id = new ID());

    ConfigurationGroup *group =
        new VerticalConfigurationGroup(false, false, true, true);

    group->setLabel(QObject::tr("Connect source to input"));

    HorizontalConfigurationGroup *ci;
    ci = new HorizontalConfigurationGroup(false, false);
    ci->addChild(cardid = new CardID(*this));
    ci->addChild(inputname = new InputName(*this));
    group->addChild(ci);
    group->addChild(new InputDisplayName(*this));
    group->addChild(sourceid = new SourceID(*this));
    if (!isDVBcard)
    {
        group->addChild(new ExternalChannelCommand(*this));
        group->addChild(new PresetTuner(*this));
    }

    TransButtonSetting *scan = new TransButtonSetting();
    scan->setLabel(tr("Scan for channels"));
    scan->setHelpText(
        tr("Use channel scanner to find channels for this input."));

    TransButtonSetting *srcfetch = new TransButtonSetting();
    srcfetch->setLabel(tr("Fetch channels from listings source"));
    srcfetch->setHelpText(
        tr("This uses the listings data source to "
           "provide the channels for this input.") + " " +
        tr("This can take a long time to run."));

    ConfigurationGroup *sgrp =
        new HorizontalConfigurationGroup(false, false, true, true);
    sgrp->addChild(scan);
    sgrp->addChild(srcfetch);
    group->addChild(sgrp);

    startchan = new StartingChannel(*this);
    group->addChild(startchan);
    group->addChild(new InputPreference(*this));

    addChild(group);

#ifdef USING_DVB
    if (isDVBcard)
    {
        ConfigurationGroup *dvbgroup =
          new VerticalConfigurationGroup(false, false, true, true);
   
        dvbgroup->addChild(diseqcpos    = new DiSEqCPos(*this));
        dvbgroup->addChild(diseqcport   = new DiSEqCPort(*this));
        dvbgroup->addChild(lnblofswitch = new LNBLofSwitch(*this));
        dvbgroup->addChild(lnblofhi = new LNBLofHi(*this));
        dvbgroup->addChild(lnbloflo = new LNBLofLo(*this));

        dvbgroup->addChild(new FreeToAir(*this));
        dvbgroup->addChild(new RadioServices(*this));
        dvbgroup->addChild(new DishNetEIT(*this));
        addChild(dvbgroup);
    }
#endif

    childid = new ChildID(*this);
    addChild(childid);

    setName("CardInput");
    connect(scan,     SIGNAL(pressed()), SLOT(channelScanner()));
    connect(srcfetch, SIGNAL(pressed()), SLOT(sourceFetch()));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            startchan,SLOT(  SetSourceID (const QString&)));
}

QString CardInput::getSourceName(void) const
{
    return sourceid->getSelectionLabel();
}

void CardInput::SetChildCardID(uint ccid)
{
    childid->setValue(QString::number(ccid));
}

void CardInput::channelScanner(void)
{
    uint srcid = sourceid->getValue().toUInt();

#ifdef USING_BACKEND
    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    save(); // save info for scanner.

    ScanWizard scanwizard(srcid);
    scanwizard.exec(false,true);

    if (SourceUtil::GetChannelCount(srcid))
        startchan->SetSourceID(QString::number(srcid));        
    if (num_channels_before)
    {
        startchan->load();
        startchan->save();
    }
#else
    VERBOSE(VB_IMPORTANT, "You must compile the backend "
            "to be able to scan for channels");
#endif
    
}

void CardInput::sourceFetch(void)
{
    uint srcid = sourceid->getValue().toUInt();

    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    myth_system("mythfilldatabase --refresh-today --dont-refresh-tomorrow");

    if (SourceUtil::GetChannelCount(srcid))
        startchan->SetSourceID(QString::number(srcid));        
    if (num_channels_before)
    {
        startchan->load();
        startchan->save();
    }
}

QString CISetting::whereClause(void) 
{
    return QString("cardinputid = %1").arg(parent.getInputID());
}

QString CISetting::setClause(void) 
{
    return QString("cardinputid = %1, %2 = '%3'")
        .arg(parent.getInputID())
        .arg(getColumn())
        .arg(getValue());
}

void CardInput::loadByID(int inputid) 
{
    id->setValue(inputid);
    load();
}

void CardInput::loadByInput(int _cardid, QString _inputname) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardinputid FROM cardinput "
                  "WHERE cardid = :CARDID AND inputname = :INPUTNAME");
    query.bindValue(":CARDID", _cardid);
    query.bindValue(":INPUTNAME", _inputname);

    if (query.exec() && query.isActive() && query.size() > 0) 
    {
        query.next();
        loadByID(query.value(0).toInt());
    } 
    else 
    {
        load(); // new
        cardid->setValue(QString::number(_cardid));
        inputname->setValue(_inputname);
    }

    if (CardUtil::IsDVB(_cardid, _inputname))
    {
        QString subtype = CardUtil::ProbeSubTypeName(_cardid, _inputname);
        CardUtil::CARD_TYPES dvbType = CardUtil::toCardType(subtype);
        
        if ("QPSK" == subtype)
        {
            //Check for DiSEqC type
            diseqcpos->setVisible(true);
            lnblofswitch->setVisible(true);
            lnbloflo->setVisible(true);
            lnblofhi->setVisible(true);

            DISEQC_TYPES dt = CardUtil::GetDISEqCType(_cardid);
            bool pos = (dt == DISEQC_POSITIONER_X);
            diseqcpos->setEnabled(pos);
        }
        else if (dvbType > CardUtil::ERROR_PROBE)
        {
            diseqcpos->setVisible(false);
            lnblofswitch->setVisible(false);
            lnbloflo->setVisible(false);
            lnblofhi->setVisible(false);
        }
    }
}

void CardInput::save() 
{

    if (sourceid->getValue() == "0")
    {
        // "None" is represented by the lack of a row
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM cardinput WHERE cardinputid = :INPUTID");
        query.bindValue(":INPUTID", getInputID());
        query.exec();
    }
    else
    {
        ConfigurationWizard::save();
    }
}

void CardInput::fillDiseqcSettingsInput(QString _pos, QString _port) 
{
    if (_port != "")
        diseqcport->setValue(_port);
    if (_pos != "")
        diseqcpos->setValue(_pos);
}

int CISetting::getInputID(void) const 
{
    return parent.getInputID();
}

int CCSetting::getCardID(void) const 
{
    return parent.getCardID();
}

int CaptureCardEditor::exec() 
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void CaptureCardEditor::load() 
{
    clearSelections();
    addSelection(QObject::tr("(New capture card)"), "0");
    addSelection(QObject::tr("(Delete all capture cards)"), "-1");
    CaptureCard::fillSelections(this, true);
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
        cc.exec();
    } 
    else 
    {
        int val = MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(),
            "",
            tr("Capture Card Menu"),
            tr("Edit.."),
            tr("Delete.."),
            1);

        if (val == 0)
            edit();
        else if (val == 1)
            del();
    }
}

void CaptureCardEditor::edit(void)
{
    const int cardid = getValue().toInt();
    if (-1 == cardid)
    {
        int val = MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(), "",
            tr("Are you sure you want to delete "
               "ALL capture cards?"),
            tr("Yes, delete capture cards"),
            tr("No, don't"), 2);

        if (0 == val)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.exec("TRUNCATE TABLE capturecard;");
            query.exec("TRUNCATE TABLE cardinput;");
            load();
        }
    }
    else
    {
        CaptureCard cc;
        if (cardid)
            cc.loadByID(cardid);
        cc.exec();
    }
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
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("DELETE FROM capturecard "
                      "WHERE cardid   = :CARDID OR "
                      "      parentid = :CARDID");
        query.bindValue(":CARDID", getValue());
        if (!query.exec() || !query.isActive())
            MythContext::DBError("Deleting Capture Card", query);

        query.prepare("DELETE FROM cardinput WHERE cardid = :CARDID");
        query.bindValue(":CARDID", getValue());
        if (!query.exec() || !query.isActive())
            MythContext::DBError("Deleting Card Input", query);
        
        load();
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

int VideoSourceEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void VideoSourceEditor::load() {
    clearSelections();
    addSelection(QObject::tr("(New video source)"), "0");
    addSelection(QObject::tr("(Delete all video sources)"), "-1");
    VideoSource::fillSelections(this);
}

void VideoSourceEditor::menu()
{
    if (getValue().toInt() == 0) 
    {
        VideoSource vs;
        vs.exec();
    } 
    else 
    {
        int val = MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(),
            "",
            tr("Video Source Menu"),
            tr("Edit.."),
            tr("Delete.."),
            1);

        if (val == 0)
            edit();
        else if (val == 1)
            del();
    }
}

void VideoSourceEditor::edit() 
{
    const int sourceid = getValue().toInt();
    if (-1 == sourceid)
    {
        int val = MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(), "",
            tr("Are you sure you want to delete "
               "ALL video sources?"),
            tr("Yes, delete video sources"),
            tr("No, don't"), 2);

        if (0 == val)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.exec("TRUNCATE TABLE channel;");
            query.exec("TRUNCATE TABLE program;");
            query.exec("TRUNCATE TABLE videosource;");
            query.exec("TRUNCATE TABLE credits;");
            query.exec("TRUNCATE TABLE programrating;");
            query.exec("TRUNCATE TABLE programgenres;");
            query.exec("TRUNCATE TABLE dtv_multiplex;");
            query.exec("TRUNCATE TABLE cardinput;");
            load();
        }
    }
    else
    {
        VideoSource vs;
        if (sourceid)
            vs.loadByID(sourceid);
        vs.exec();
    }
}

void VideoSourceEditor::del() 
{
    int val = MythPopupBox::show2ButtonPopup(
        gContext->GetMainWindow(), "",
        tr("Are you sure you want to delete "
           "this video source?"),
        tr("Yes, delete video source"),
        tr("No, don't"),
        2);

    if (val == 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM videosource "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", getValue());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Deleting VideoSource", query);

        load();
    }
}

int CardInputEditor::exec() 
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        cardinputs[getValue().toInt()]->exec();

    return QDialog::Rejected;
}

void CardInputEditor::load() 
{
    clearSelections();

    // We do this manually because we want custom labels.  If
    // SelectSetting provided a facility to edit the labels, we
    // could use CaptureCard::fillSelections

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, videodevice, cardtype "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME AND "
        "      parentid = '0'");
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive() || !query.size())
        return;

    uint j = 0;
    while (query.next())
    {
        int     cardid      = query.value(0).toInt();
        QString videodevice = query.value(1).toString();
        QString cardtype    = query.value(2).toString();

        QStringList        inputLabels;
        vector<CardInput*> cardInputs;

        CardUtil::GetCardInputs(cardid, videodevice, cardtype,
                                inputLabels, cardInputs);

        for (uint i = 0; i < inputLabels.size(); i++, j++)
        {
            cardinputs.push_back(cardInputs[i]);
            addSelection(inputLabels[i], QString::number(j));
        }
    }
}

CardInputEditor::~CardInputEditor()
{
    // CardInput instances are deleted by Qt, no need to do that here
}

#ifdef USING_DVB
static QString remove_chaff(const QString &name)
{
    // Trim off some of the chaff.
    QString short_name = name;
    if (short_name.left(14) == "LG Electronics")
        short_name = short_name.right(short_name.length() - 15);
    if (short_name.left(4) == "Oren")
        short_name = short_name.right(short_name.length() - 5);
    if (short_name.right(8) == "Frontend")
        short_name = short_name.left(short_name.length() - 9);
    if (short_name.right(7) == "VSB/QAM")
        short_name = short_name.left(short_name.length() - 8);
    if (short_name.right(3) == "VSB")
        short_name = short_name.left(short_name.length() - 4);

    // It would be infinitely better if DVB allowed us to query
    // the vendor ID. But instead we have to guess based on the
    // demodulator name. This means cards like the Air2PC HD5000
    // and DViCO Fusion HDTV cards are not identified correctly.
    if (short_name.left(7).lower() == "or51211")
        short_name = "pcHDTV HD-2000";
    else if (short_name.left(7).lower() == "or51132")
        short_name = "pcHDTV HD-3000";
    else if (short_name.left(7).lower() == "bcm3510")
        short_name = "Air2PC v1";
    else if (short_name.left(7).lower() == "nxt2002")
        short_name = "Air2PC v2";
    else if (short_name.left(8).lower() == "lgdt3302")
        short_name = "DViCO HDTV3";
    else if (short_name.left(8).lower() == "lgdt3303")
        short_name = "DViCO v2 or Air2PC v3";

    return short_name;
}
#endif // USING_DVB

void DVBConfigurationGroup::probeCard(const QString &videodevice)
{
    (void) videodevice;

#ifdef USING_DVB
    uint dvbdev = videodevice.toUInt();
    QString frontend_name = CardUtil::ProbeDVBFrontendName(dvbdev);
    QString subtype       = CardUtil::ProbeDVBType(dvbdev);

    QString err_open  = tr("Could not open card #%1").arg(dvbdev);
    QString err_other = tr("Could not get card info for card #%1").arg(dvbdev);

    switch (CardUtil::toCardType(subtype))
    {
        case CardUtil::ERROR_OPEN:
            cardname->setValue(err_open);
            cardtype->setValue(strerror(errno));
            break;
        case CardUtil::ERROR_UNKNOWN:
            cardname->setValue(err_other);
            cardtype->setValue("Unknown error");
            break;
        case CardUtil::ERROR_PROBE:
            cardname->setValue(err_other);
            cardtype->setValue(strerror(errno));
            break;
        case CardUtil::QPSK:
            cardtype->setValue("DVB-S");
            cardname->setValue(frontend_name);
            signal_timeout->setValue(60000);
            channel_timeout->setValue(62500);
            break;
        case CardUtil::QAM:
            cardtype->setValue("DVB-C");
            cardname->setValue(frontend_name);
            signal_timeout->setValue(500);
            channel_timeout->setValue(3000);
            break;
        case CardUtil::OFDM:
            cardtype->setValue("DVB-T");
            cardname->setValue(frontend_name);
            signal_timeout->setValue(500);
            channel_timeout->setValue(3000);
            if (frontend_name.lower().find("usb") >= 0)
            {
                signal_timeout->setValue(40000);
                channel_timeout->setValue(42500);
            }
            break;
        case CardUtil::ATSC:
        {
            QString short_name = remove_chaff(frontend_name);
            cardtype->setValue("ATSC");
            cardname->setValue(short_name);
            signal_timeout->setValue(500);
            channel_timeout->setValue(3000);

            if (frontend_name.lower().find("usb") < 0)
            {
                buttonAnalog->setVisible(
                    short_name.left(6).lower() == "pchdtv" ||
                    short_name.left(5).lower() == "dvico");
            }
        }
        break;
        default:
            break;
    }
#else
    cardtype->setValue(QString("Recompile with DVB-Support!"));
#endif
}

TunerCardInput::TunerCardInput(const CaptureCard &parent)
    : CCSetting(parent, "defaultinput"),
      last_device(QString::null),
      last_cardtype(QString::null),
      last_diseqct(-1)
{
    setLabel(QObject::tr("Default input"));
    int cardid = parent.getCardID();
    if (cardid <= 0)
        return;

    last_cardtype = CardUtil::GetRawCardType(cardid, 0);
    last_device   = CardUtil::GetVideoDevice(cardid, 0);
}

void TunerCardInput::fillSelections(const QString& device)
{
    clearSelections();

    if (device.isEmpty())
        return;

    last_device = device;
    QStringList inputs =
        CardUtil::probeInputs(device, last_cardtype, last_diseqct);

    for (QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);
}

void TunerCardInput::diseqcType(const QString &diseqcType)
{
    bool ok;
    int tmp = diseqcType.toInt(&ok);
    if (ok)
    {
        last_diseqct = tmp;
        fillSelections(last_device);
    }
}

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent)
    : parent(a_parent)
{
    setUseLabel(false);

    DVBCardNum* cardnum = new DVBCardNum(parent);
    cardname = new DVBCardName();
    cardtype = new DVBCardType();

    signal_timeout = new SignalTimeout(parent, 500);
    channel_timeout = new ChannelTimeout(parent, 3000);

    addChild(cardnum);

    HorizontalConfigurationGroup *hg0 = 
        new HorizontalConfigurationGroup(false, false, true, true);
    hg0->addChild(cardname);
    hg0->addChild(cardtype);
    addChild(hg0);

    addChild(signal_timeout);
    addChild(channel_timeout);

    addChild(new DVBAudioDevice(parent));
    addChild(new DVBVbiDevice(parent));

    TransButtonSetting *buttonDiSEqC = new TransButtonSetting();
    buttonDiSEqC->setLabel(tr("DiSEqC"));
    buttonDiSEqC->setHelpText(tr("Input and satellite settings."));

    buttonAnalog = new TransButtonSetting();
    buttonAnalog->setLabel(tr("Analog Options"));
    buttonAnalog->setVisible(false);
    buttonAnalog->setHelpText(
        tr("Analog child card settings."
           "\nWARNING: Do not press button if you are "
           "using an Air2PC HD-5000 card!!!! "
           "This card does not support analog tuning, "
           "but the DVB drivers do not yet allow us to "
           "detect this problem."));

    TransButtonSetting *buttonRecOpt = new TransButtonSetting();
    buttonRecOpt->setLabel(tr("Recording Options"));    
    buttonDiSEqC->setHelpText(tr("Various additional settings."));

    HorizontalConfigurationGroup *advcfg = 
        new HorizontalConfigurationGroup(false, false, true, true);
    advcfg->addChild(buttonDiSEqC);
    advcfg->addChild(buttonAnalog);
    advcfg->addChild(buttonRecOpt);
    addChild(advcfg);

    DVBDiSEqCType  *diseqctype   = new DVBDiSEqCType(parent);
    addChild(diseqctype);
    diseqctype->setVisible(false);

    TunerCardInput *defaultinput = new TunerCardInput(parent);
    addChild(defaultinput);
    defaultinput->setVisible(false);

    connect(cardnum,      SIGNAL(valueChanged(const QString&)),
            this,         SLOT(  probeCard   (const QString&)));
    connect(buttonDiSEqC, SIGNAL(pressed()),
            &parent,      SLOT(  DiSEqCPanel()));
    connect(buttonAnalog, SIGNAL(pressed()),
            &parent,      SLOT(  analogPanel()));
    connect(buttonRecOpt, SIGNAL(pressed()),
            &parent,      SLOT(  recorderOptionsPanel()));
    connect(diseqctype,   SIGNAL(valueChanged(const QString&)),
            defaultinput, SLOT(  diseqcType  (const QString&)));

    cardnum->setValue(0);
    defaultinput->diseqcType(diseqctype->getValue());
}

void CaptureCard::reload(void)
{
    if (getCardID() == 0)
    {
        save();
        load();
    }
}

void CaptureCard::analogPanel()
{
    reload();

    uint    cardid       = getCardID();
    uint    child_cardid = CardUtil::GetChildCardID(cardid);
    QString devname = "Unknown";
    QString dev = CardUtil::GetVideoDevice(cardid, 0);
    if (!dev.isEmpty())
        devname = QString("[ DVB : %1 ]").arg(devname);

    CaptureCard *card = new CaptureCard(false);
    card->addChild(new V4LConfigurationGroup(*card));
    if (child_cardid)
        card->loadByID(child_cardid);
    else
        card->setParentID(cardid);
    card->setLabel(tr("Analog Options for ") + devname);
    card->exec();
    delete card;
}

void CaptureCard::recorderOptionsPanel()
{
    reload();

    RecorderOptions acw(*this);
    acw.exec();
}

void CaptureCard::DiSEqCPanel()
{
    reload();

    DVBDiSEqCConfigurationWizard diseqcWiz(*this);
    diseqcWiz.exec();
    load();
}

RecorderOptions::RecorderOptions(CaptureCard& parent)
{
    VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
    rec->setLabel(QObject::tr("Recorder Options"));
    rec->setUseLabel(false);

    rec->addChild(new DVBNoSeqStart(parent));
    rec->addChild(new DVBOnDemand(parent));
    addChild(rec);
}

static GlobalLineEdit *DiSEqCLatitude()
{
    GlobalLineEdit *gc = new GlobalLineEdit("latitude");
    gc->setLabel("Latitude");
    gc->setHelpText(
        QObject::tr("The Cartesian latitude for your location.") + " " +
        QObject::tr("Use negative numbers for southern "
                    "and western coordinates."));
    return gc;
}

static GlobalLineEdit *DiSEqCLongitude()
{
    GlobalLineEdit *gc = new GlobalLineEdit("longitude");
    gc->setLabel("Longitude");
    gc->setHelpText(
        QObject::tr("The Cartesian longitude for your location.") + " " +
        QObject::tr("Use negative numbers for southern "
                    "and western coordinates."));
    return gc;
}

DVBDiSEqCConfigurationWizard::DVBDiSEqCConfigurationWizard(CaptureCard &parent)
{
    VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
    rec->setLabel(QObject::tr("DiSEqC Options"));
    rec->setUseLabel(false);

    DVBDiSEqCType  *diseqctype   = new DVBDiSEqCType(parent);
    TunerCardInput *defaultinput = new TunerCardInput(parent);

    rec->addChild(diseqctype);
    rec->addChild(defaultinput);
    rec->addChild(DiSEqCLatitude());
    rec->addChild(DiSEqCLongitude());
    addChild(rec);

    connect(diseqctype,   SIGNAL(valueChanged(const QString&)),
            defaultinput, SLOT(  diseqcType(  const QString&)));

    defaultinput->diseqcType(diseqctype->getValue());
}
