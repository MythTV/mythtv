// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <qapplication.h>
#include <qstringlist.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
#include <qmap.h>
#include <qdir.h>
#include <qprocess.h>

// MythTV headers
#include "mythconfig.h"
#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "videosource.h"
#include "datadirect.h"
#include "scanwizard.h"
#include "cardutil.h"
#include "sourceutil.h"
#include "channelutil.h"
#include "frequencies.h"
#include "diseqcsettings.h"
#include "firewiredevice.h"

#ifdef USING_DVB
#include "dvbtypes.h"
#endif

#ifdef USING_V4L
#include "videodev_myth.h"
#endif

VideoSourceSelector::VideoSourceSelector(uint           _initial_sourceid,
                                         const QString &_card_types,
                                         bool           _must_have_mplexid) :
    ComboBoxSetting(this),
    initial_sourceid(_initial_sourceid),
    card_types(QDeepCopy<QString>(_card_types)),
    must_have_mplexid(_must_have_mplexid)
{
    setLabel(tr("Video Source"));
}

void VideoSourceSelector::load(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    QString querystr =
        "SELECT DISTINCT videosource.name, videosource.sourceid "
        "FROM cardinput, videosource, capturecard";

    querystr += (must_have_mplexid) ? ", channel " : " ";

    querystr +=
        "WHERE cardinput.sourceid   = videosource.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid   AND "
        "      capturecard.hostname = :HOSTNAME ";

    if (!card_types.isEmpty())
    {
        querystr += QString(" AND capturecard.cardtype in %1 ")
            .arg(card_types);
    }

    if (must_have_mplexid)
    {
        querystr +=
            " AND channel.sourceid      = videosource.sourceid "
            " AND channel.mplexid      != 32767                "
            " AND channel.mplexid      != 0                    ";
    }

    query.prepare(querystr);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive() || query.size() <= 0)
        return;

    uint sel = 0, cnt = 0;
    for (; query.next(); cnt++)
    {
        addSelection(query.value(0).toString(),
                     query.value(1).toString());

        sel = (query.value(1).toUInt() == initial_sourceid) ? cnt : sel;
    }

    if (initial_sourceid)
    {
        if (cnt)
            setValue(sel);
        setEnabled(false);
    }
}

class RecorderOptions : public ConfigurationWizard
{
  public:
    RecorderOptions(CaptureCard& parent);
};

QString VideoSourceDBStorage::whereClause(MSqlBindings& bindings)
{
    QString sourceidTag(":WHERESOURCEID");
    
    QString query("sourceid = " + sourceidTag);

    bindings.insert(sourceidTag, parent.getSourceID());

    return query;
}

QString VideoSourceDBStorage::setClause(MSqlBindings& bindings)
{
    QString sourceidTag(":SETSOURCEID");
    QString colTag(":SET" + getColumn().upper());

    QString query("sourceid = " + sourceidTag + ", " + 
            getColumn() + " = " + colTag);

    bindings.insert(sourceidTag, parent.getSourceID());
    bindings.insert(colTag, setting->getValue());

    return query;
}

QString CaptureCardDBStorage::whereClause(MSqlBindings& bindings)
{
    QString cardidTag(":WHERECARDID");
    
    QString query("cardid = " + cardidTag);

    bindings.insert(cardidTag, parent.getCardID());

    return query;
}

QString CaptureCardDBStorage::setClause(MSqlBindings& bindings)
{
    QString cardidTag(":SETCARDID");
    QString colTag(":SET" + getColumn().upper());

    QString query("cardid = " + cardidTag + ", " +
            getColumn() + " = " + colTag);

    bindings.insert(cardidTag, parent.getCardID());
    bindings.insert(colTag, setting->getValue());

    return query;
}

class XMLTVGrabber : public ComboBoxSetting, public VideoSourceDBStorage
{
  public:
    XMLTVGrabber(const VideoSource &parent) :
        ComboBoxSetting(this), VideoSourceDBStorage(this, parent, "xmltvgrabber")
    {
        setLabel(QObject::tr("Listings grabber"));
    };
};

FreqTableSelector::FreqTableSelector(const VideoSource &parent) :
    ComboBoxSetting(this), VideoSourceDBStorage(this, parent, "freqtable")
{
    setLabel(QObject::tr("Channel frequency table"));
    addSelection("default");

    for (uint i = 0; chanlists[i].name; i++)
        addSelection(chanlists[i].name);

    setHelpText(QObject::tr("Use default unless this source uses a "
                "different frequency table than the system wide table "
                "defined in the General settings."));
}

TransFreqTableSelector::TransFreqTableSelector(uint _sourceid) :
    ComboBoxSetting(this), sourceid(_sourceid),
    loaded_freq_table(QString::null)
{
    setLabel(QObject::tr("Channel frequency table"));

    for (uint i = 0; chanlists[i].name; i++)
        addSelection(chanlists[i].name);
}

void TransFreqTableSelector::load(void)
{
    int idx = getValueIndex(gContext->GetSetting("FreqTable"));
    if (idx >= 0)
        setValue(idx);

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT freqtable "
        "FROM videosource "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("TransFreqTableSelector::load", query);
        return;
    }

    loaded_freq_table = QString::null;

    if (query.next())
    {
        loaded_freq_table = query.value(0).toString();
        if (!loaded_freq_table.isEmpty() &&
            (loaded_freq_table.lower() != "default"))
        {
            int idx = getValueIndex(loaded_freq_table);
            if (idx >= 0)
                setValue(idx);
        }
    }
}

void TransFreqTableSelector::save(void)
{
    VERBOSE(VB_IMPORTANT, "TransFreqTableSelector::save(void)");

    if ((loaded_freq_table == getValue()) ||
        ((loaded_freq_table.lower() == "default") &&
         (getValue() == gContext->GetSetting("FreqTable"))))
    {
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE videosource "
        "SET freqtable = :FREQTABLE "
        "WHERE sourceid = :SOURCEID");

    query.bindValue(":FREQTABLE", getValue());
    query.bindValue(":SOURCEID",  sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("TransFreqTableSelector::load", query);
        return;
    }
}

void TransFreqTableSelector::SetSourceID(uint _sourceid)
{
    sourceid = _sourceid;
    load();
}

class UseEIT : public CheckBoxSetting, public VideoSourceDBStorage
{
  public:
    UseEIT(const VideoSource &parent) :
        CheckBoxSetting(this), VideoSourceDBStorage(this, parent, "useeit")
    {
        setLabel(QObject::tr("Perform EIT Scan"));
        setHelpText(QObject::tr(
                        "If this is enabled the data in this source will be "
                        "updated with listing data provided by the channels "
                        "themselves 'over-the-air'."));
    }
};

class DataDirectUserID : public LineEditSetting, public VideoSourceDBStorage
{
  public:
    DataDirectUserID(const VideoSource &parent) :
        LineEditSetting(this), VideoSourceDBStorage(this, parent, "userid")
    {
        setLabel(QObject::tr("User ID"));
    }
};

class DataDirectPassword : public LineEditSetting, public VideoSourceDBStorage
{
  public:
    DataDirectPassword(const VideoSource &parent) :
        LineEditSetting(this), VideoSourceDBStorage(this, parent, "password")
    {
        setLabel(QObject::tr("Password"));
    }
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

    DataDirectProcessor ddp(_source, uid, pwd);
    QString waitMsg = tr("Fetching lineups from %1...")
        .arg(ddp.GetListingsProviderName());
        
    VERBOSE(VB_GENERAL, waitMsg);
    MythProgressDialog pdlg(waitMsg, 2);

    clearSelections();

    pdlg.setProgress(1);

    if (!ddp.GrabLineupsOnly())
    {
        VERBOSE(VB_IMPORTANT, "DDLS: fillSelections "
                "did not successfully load selections");
        return;
    }
    const DDLineupList lineups = ddp.GetLineups();

    DDLineupList::const_iterator it;
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

DataDirect_config::DataDirect_config(const VideoSource& _parent, int _source) :
    VerticalConfigurationGroup(false, false, false, false),
    parent(_parent) 
{
    source = _source;

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
                                           QString _grabber) :
    VerticalConfigurationGroup(false, false, false, false),
    parent(_parent), grabber(_grabber) 
{
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

    if (grabber != "datadirect" && grabber != "eitonly" && 
        grabber != "/bin/true")
    {
        VERBOSE(VB_IMPORTANT, "\n" << err_msg);
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), QObject::tr("Warning."), err_msg);
    }

    pdlg.setProgress( 2 );    
    pdlg.Close();
}

EITOnly_config::EITOnly_config(const VideoSource& _parent) :
    VerticalConfigurationGroup(false, false, true, true)
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

NoGrabber_config::NoGrabber_config(const VideoSource& _parent) :
    VerticalConfigurationGroup(false, false, false, false)
{
    useeit = new UseEIT(_parent);
    useeit->setValue(false);
    useeit->setVisible(false);
    addChild(useeit);
}

void NoGrabber_config::save()
{
    useeit->setValue(false);
    useeit->save();
}

XMLTVConfig::XMLTVConfig(const VideoSource &parent) :
    TriggeredConfigurationGroup(false, true, false, false)
{
    XMLTVGrabber* grabber = new XMLTVGrabber(parent);
    addChild(grabber);
    setTrigger(grabber);

    // only save settings for the selected grabber
    setSaveAll(false);

    addTarget("datadirect", new DataDirect_config(parent));
    grabber->addSelection("North America (DataDirect) (Internal)", "datadirect");

    addTarget("eitonly", new EITOnly_config(parent));
    grabber->addSelection("Transmitted guide only (EIT)", "eitonly");

    QProcess find_grabber_proc( QString("tv_find_grabbers"), this );
    find_grabber_proc.addArgument("baseline");
    find_grabber_proc.addArgument("manualconfig");
    if ( find_grabber_proc.start() ) {

        int i=0;
        // Assume it shouldn't take more than 25 seconds
        // Broken versions of QT cause QProcess::start
        // and QProcess::isRunning to return true even
        // when the executable doesn't exist
        while (find_grabber_proc.isRunning() && i < 250)
        {
            usleep(100000);
            ++i;
        }

        if (find_grabber_proc.normalExit())
        {
            while (find_grabber_proc.canReadLineStdout())
            {
                QString grabber_list = find_grabber_proc.readLineStdout();
                QStringList grabber_split = QStringList::split("|",
                    grabber_list);
                QString grabber_name = grabber_split[1] + " (xmltv)";
                QFileInfo grabber_file(grabber_split[0]);
                addTarget(grabber_file.fileName(),
                    new XMLTV_generic_config(parent, grabber_file.fileName()));
                grabber->addSelection(grabber_name, grabber_file.fileName());
            }
        }
        else {
            VERBOSE(VB_IMPORTANT, "tv_find_grabbers exited early or we timed out waiting");
        }
    }
    else {
        VERBOSE(VB_IMPORTANT, "Failed to run tv_find_grabbers");
    }

    addTarget("/bin/true", new NoGrabber_config(parent));
    grabber->addSelection("No grabber", "/bin/true");
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

class VideoDevice : public PathSetting, public CaptureCardDBStorage
{
  public:
    VideoDevice(const CaptureCard &parent,
                uint    minor_min = 0,
                uint    minor_max = UINT_MAX,
                QString card      = QString::null,
                QString driver    = QString::null) :
        PathSetting(this, true),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setLabel(QObject::tr("Video device"));

        // /dev/v4l/video*
        QDir dev("/dev/v4l", "video*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev, minor_min, minor_max,
                              card, driver, false);

        // /dev/video*
        dev.setPath("/dev");
        fillSelectionsFromDir(dev, minor_min, minor_max,
                              card, driver, false);

        // /dev/dtv/video*
        dev.setPath("/dev/dtv");
        fillSelectionsFromDir(dev, minor_min, minor_max,
                              card, driver, false);

        // /dev/dtv*
        dev.setPath("/dev");
        dev.setNameFilter("dtv*");
        fillSelectionsFromDir(dev, minor_min, minor_max,
                              card, driver, false);
    };

    uint fillSelectionsFromDir(const QDir& dir,
                               uint minor_min, uint minor_max,
                               QString card, QString driver,
                               bool allow_duplicates)
    {
        uint cnt = 0;
        const QFileInfoList *il = dir.entryInfoList();
        if (!il)
            return cnt;
        
        QFileInfoListIterator it( *il );
        QFileInfo *fi;

        for (; (fi = it.current()) != 0; ++it)
        {
            struct stat st;
            QString filepath = fi->absFilePath();
            int err = lstat(filepath, &st);

            if (0 != err)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Could not stat file: %1").arg(filepath));
                continue;
            }

            // is this is a character device?
            if (!S_ISCHR(st.st_mode))
                continue;

            // is this device is in our minor range?
            uint minor_num = minor(st.st_rdev);
            if (minor_min > minor_num || minor_max < minor_num)
                continue;

            // ignore duplicates if allow_duplicates not set
            if (!allow_duplicates && minor_list[minor_num])
                continue;

            // if the driver returns any info add this device to our list
            int videofd = open(filepath.ascii(), O_RDWR);
            if (videofd >= 0)
            {
                QString cn, dn;
                if (CardUtil::GetV4LInfo(videofd, cn, dn) &&
                    (driver.isEmpty() || (dn == driver))  &&
                    (card.isEmpty()   || (cn == card)))
                {
                    addSelection(filepath);
                    cnt++;
                }
                close(videofd);
            }

            // add to list of minors discovered to avoid duplicates
            minor_list[minor_num] = 1;
        }

        return cnt;
    }

  private:
    QMap<uint, uint> minor_list;
};

class VBIDevice : public PathSetting, public CaptureCardDBStorage
{
  public:
    VBIDevice(const CaptureCard &parent) :
        PathSetting(this, true),
        CaptureCardDBStorage(this, parent, "vbidevice")
    {
        setLabel(QObject::tr("VBI device"));
        setFilter(QString::null, QString::null);
    };

    void setFilter(const QString &card, const QString &driver)
    {
        clearSelections();
        QDir dev("/dev/v4l", "vbi*", QDir::Name, QDir::System);
        if (!fillSelectionsFromDir(dev, card, driver))
        {
            dev.setPath("/dev");
            fillSelectionsFromDir(dev, card, driver);
        }
    }

    uint fillSelectionsFromDir(const QDir &dir, const QString &card,
                               const QString &driver)
    {
        uint cnt = 0;
        const QFileInfoList *il = dir.entryInfoList();
        if (!il)
            return cnt;

        QFileInfoListIterator it(*il);
        QFileInfo *fi;

        for (; (fi = it.current()) != 0; ++it)
        {
            QString device = fi->absFilePath();
            int vbifd = open(device.ascii(), O_RDWR);
            if (vbifd < 0)
                continue;

            QString cn, dn;
            if (CardUtil::GetV4LInfo(vbifd, cn, dn)  &&
                (driver.isEmpty() || (dn == driver)) &&
                (card.isEmpty()   || (cn == card)))
            {
                addSelection(device);
                cnt++;
            }

            close(vbifd);
        }

        return cnt;
    }
};

class AudioDevice : public PathSetting, public CaptureCardDBStorage
{
  public:
    AudioDevice(const CaptureCard &parent) :
        PathSetting(this, true),
        CaptureCardDBStorage(this, parent, "audiodevice")
    {
        setLabel(QObject::tr("Audio device"));
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
        addSelection(QObject::tr("(None)"), "/dev/null");
    };
};

class SignalTimeout : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    SignalTimeout(const CaptureCard &parent, uint value, uint min_val) :
        SpinBoxSetting(this, min_val, 60000, 250),
        CaptureCardDBStorage(this, parent, "signal_timeout")
    {
        setLabel(QObject::tr("Signal Timeout (msec)"));
        setValue(value);
        setHelpText(QObject::tr(
                        "Maximum time MythTV waits for any signal when "
                        "scanning for channels."));
    };
};

class ChannelTimeout : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    ChannelTimeout(const CaptureCard &parent, uint value, uint min_val) :
        SpinBoxSetting(this, min_val, 65000, 250),
        CaptureCardDBStorage(this, parent, "channel_timeout")
    {
        setLabel(QObject::tr("Tuning Timeout (msec)"));
        setValue(value);
        setHelpText(QObject::tr(
                        "Maximum time MythTV waits for a channel lock "
                        "when scanning for channels. Or, for issuing "
                        "a warning in LiveTV mode."));
    };
};

class AudioRateLimit : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    AudioRateLimit(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "audioratelimit")
    {
        setLabel(QObject::tr("Audio sampling rate limit"));
        addSelection(QObject::tr("(None)"), "0");
        addSelection("32000");
        addSelection("44100");
        addSelection("48000");
    };
};

class SkipBtAudio : public CheckBoxSetting, public CaptureCardDBStorage
{
  public:
    SkipBtAudio(const CaptureCard &parent) :
        CheckBoxSetting(this),
        CaptureCardDBStorage(this, parent, "skipbtaudio")
    {
        setLabel(QObject::tr("Do not adjust volume"));
        setHelpText(
            QObject::tr("Enable this option for budget BT878 based "
                        "DVB-T cards such as the AverTV DVB-T which "
                        "require the audio volume to be left alone."));
   };
};

class DVBInput : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBInput(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "defaultinput")
    {
        setLabel(QObject::tr("Default Input"));
        fillSelections(false);
    }

    void fillSelections(bool diseqc)
    {
        clearSelections();
        addSelection((diseqc) ? "DVBInput #1" : "DVBInput");
    }
};

class DVBCardNum : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBCardNum(const CaptureCard &parent) :
        SpinBoxSetting(this, 0, 7, 1),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setLabel(QObject::tr("DVB Card Number"));
        setHelpText(
            QObject::tr("When you change this setting, the text below "
                        "should change to the name and type of your card. "
                        "If the card cannot be opened, an error message "
                        "will be displayed."));
    };
};

class DVBCardType : public TransLabelSetting
{
  public:
    DVBCardType()
    {
        setLabel(QObject::tr("Subtype"));
    };
};

class DVBCardName : public TransLabelSetting
{
  public:
    DVBCardName()
    {
        setLabel(QObject::tr("Frontend ID"));
    };
};

class DVBNoSeqStart : public CheckBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBNoSeqStart(const CaptureCard &parent) :
        CheckBoxSetting(this),
        CaptureCardDBStorage(this, parent, "dvb_wait_for_seqstart")
    {
        setLabel(QObject::tr("Wait for SEQ start header."));
        setValue(true);
        setHelpText(
            QObject::tr("Make the dvb-recording drop packets from "
                        "the card until a sequence start header is seen."));
    };
};

class DVBOnDemand : public CheckBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBOnDemand(const CaptureCard &parent) :
        CheckBoxSetting(this),
        CaptureCardDBStorage(this, parent, "dvb_on_demand")
    {
        setLabel(QObject::tr("Open DVB card on demand"));
        setValue(true);
        setHelpText(
            QObject::tr("This option makes the backend dvb-recorder "
                        "only open the card when it is actually in-use, leaving "
                        "it free for other programs at other times."));
    };
};

class DVBTuningDelay : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBTuningDelay(const CaptureCard &parent) :
        SpinBoxSetting(this, 0, 2000, 25),
        CaptureCardDBStorage(this, parent, "dvb_tuning_delay")
    {
        setLabel(QObject::tr("DVB Tuning Delay (msec)"));
        setHelpText(
            QObject::tr("Some Linux DVB drivers, in particular for the "
                        "Hauppauge Nova-T, require that we slow down "
                        "the tuning process."));
    };
};

class FirewireGUID : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    FirewireGUID(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setLabel(QObject::tr("GUID"));
#ifdef USING_FIREWIRE
        vector<AVCInfo> list = FirewireDevice::GetSTBList();
        for (uint i = 0; i < list.size(); i++)
        {
            QString guid = list[i].GetGUIDString();
            guid_to_avcinfo[guid] = list[i];
            addSelection(guid);
        }
#endif // USING_FIREWIRE
    }

    AVCInfo GetAVCInfo(const QString &guid) const
        { return guid_to_avcinfo[guid]; }

  private:
    QMap<QString,AVCInfo> guid_to_avcinfo;
};

FirewireModel::FirewireModel(const CaptureCard  &parent,
                             const FirewireGUID *_guid) :
    ComboBoxSetting(this),
    CaptureCardDBStorage(this, parent, "firewire_model"),
    guid(_guid)
{
    setLabel(QObject::tr("Cable box model"));
    addSelection(QObject::tr("Generic"), "GENERIC");
    addSelection("DCT-6200");
    addSelection("DCT-6212");
    addSelection("DCT-6216");
    addSelection("SA3250HD");
    addSelection("SA4200HD");
    QString help = QObject::tr(
        "Choose the model that most closely resembles your set top box. "
        "Depending on firmware revision SA4200HD may work better for a "
        "SA3250HD box.");
    setHelpText(help);
}

void FirewireModel::SetGUID(const QString &_guid)
{
    (void) _guid;

#ifdef USING_FIREWIRE
    AVCInfo info = guid->GetAVCInfo(_guid);
    QString model = FirewireDevice::GetModelName(info.vendorid, info.modelid);
    setValue(max(getValueIndex(model), 0));
#endif // USING_FIREWIRE
}

void FirewireDesc::SetGUID(const QString &_guid)
{
    (void) _guid;

    setLabel(tr("Description"));

#ifdef USING_FIREWIRE
    QString name = guid->GetAVCInfo(_guid).product_name;
    name.replace("Scientific-Atlanta", "SA");
    name.replace(", Inc.", "");
    name.replace("Explorer(R)", "");
    name = name.simplifyWhiteSpace();
    setValue((name.isEmpty()) ? "" : name);
#endif // USING_FIREWIRE
}

class FirewireConnection : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    FirewireConnection(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "firewire_connection")
    {
        setLabel(QObject::tr("Connection Type"));
        addSelection(QObject::tr("Point to Point"),"0");
        addSelection(QObject::tr("Broadcast"),"1");
    }
};

class FirewireSpeed : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    FirewireSpeed(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "firewire_speed")
    {
        setLabel(QObject::tr("Speed"));
        addSelection(QObject::tr("100Mbps"),"0");
        addSelection(QObject::tr("200Mbps"),"1");
        addSelection(QObject::tr("400Mbps"),"2");
        addSelection(QObject::tr("800Mbps"),"3");
    }
};

class FirewireConfigurationGroup : public VerticalConfigurationGroup
{
  public:
    FirewireConfigurationGroup(CaptureCard& a_parent) :
        VerticalConfigurationGroup(false, true, false, false),
        parent(a_parent),
        dev(new FirewireGUID(parent)),
        desc(new FirewireDesc(dev)),
        model(new FirewireModel(parent, dev))
    {
        addChild(dev);
        addChild(desc);
        addChild(model);

#ifdef USING_LINUX_FIREWIRE
        addChild(new FirewireConnection(parent));
        addChild(new FirewireSpeed(parent));
#endif // USING_LINUX_FIREWIRE

        addChild(new SignalTimeout(parent, 2000, 1000));
        addChild(new ChannelTimeout(parent, 9000, 1750));
        addChild(new SingleCardInput(parent));

        model->SetGUID(dev->getValue());
        desc->SetGUID(dev->getValue());
        connect(dev,   SIGNAL(valueChanged(const QString&)),
                model, SLOT(  SetGUID(     const QString&)));
        connect(dev,   SIGNAL(valueChanged(const QString&)),
                desc,  SLOT(  SetGUID(     const QString&)));
    };

  private:
    CaptureCard   &parent;
    FirewireGUID  *dev;
    FirewireDesc  *desc;
    FirewireModel *model;
};

class DBOX2Port : public LineEditSetting, public CaptureCardDBStorage
{
  public:
    DBOX2Port(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "dbox2_port")
    {
        setValue("31338");
        setLabel(QObject::tr("DBOX2 Streaming Port"));
        setHelpText(QObject::tr("DBOX2 streaming port on your DBOX2."));
    }
};

class DBOX2HttpPort : public LineEditSetting, public CaptureCardDBStorage
{
  public:
    DBOX2HttpPort(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "dbox2_httpport")
    {
        setValue("80");
        setLabel(QObject::tr("DBOX2 HTTP Port"));
        setHelpText(QObject::tr("DBOX2 http port on your DBOX2."));
    }
};

class DBOX2Host : public LineEditSetting, public CaptureCardDBStorage
{
  public:
    DBOX2Host(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "dbox2_host")
    {
        setValue("dbox");
        setLabel(QObject::tr("DBOX2 Host IP"));
        setHelpText(QObject::tr("DBOX2 Host IP is the remote device."));
    }
};

class DBOX2ConfigurationGroup : public VerticalConfigurationGroup
{
  public:
    DBOX2ConfigurationGroup(CaptureCard& a_parent):
        VerticalConfigurationGroup(false, true, false, false),
        parent(a_parent)
    {
        addChild(new DBOX2Port(parent));
        addChild(new DBOX2HttpPort(parent));
        addChild(new DBOX2Host(parent));
        addChild(new SingleCardInput(parent));
    };
  private:
    CaptureCard &parent;
 };

class HDHomeRunDeviceID : public LineEditSetting, public CaptureCardDBStorage
{
  public:
    HDHomeRunDeviceID(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setValue("FFFFFFFF");
        setLabel(QObject::tr("Device ID"));
        setHelpText(QObject::tr("IP address or Device ID from the bottom of "
                                "the HDHomeRun.  You may use "
                                "'FFFFFFFF' if there is only one unit "
                                "on your your network."));
    }
};

class IPTVHost : public LineEditSetting, public CaptureCardDBStorage
{
  public:
    IPTVHost(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setValue("http://mafreebox.freebox.fr/freeboxtv/playlist.m3u");
        setLabel(QObject::tr("M3U URL"));
        setHelpText(QObject::tr("URL of M3U containing IPTV channel URLs."));
    }
};

class IPTVConfigurationGroup : public VerticalConfigurationGroup
{
  public:
    IPTVConfigurationGroup(CaptureCard& a_parent):
       VerticalConfigurationGroup(false, true, false, false),
       parent(a_parent)
    {
        setUseLabel(false);
        addChild(new IPTVHost(parent));
        addChild(new ChannelTimeout(parent, 3000, 1750));
        addChild(new SingleCardInput(parent));
    };

  private:
    CaptureCard &parent;
};

class HDHomeRunTunerIndex : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    HDHomeRunTunerIndex(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "dbox2_port")
    {
        setLabel(QObject::tr("Tuner"));
        addSelection("0");
        addSelection("1");
    }
};

class HDHomeRunConfigurationGroup : public VerticalConfigurationGroup
{
  public:
    HDHomeRunConfigurationGroup(CaptureCard& a_parent) :
        VerticalConfigurationGroup(false, true, false, false),
        parent(a_parent)
    {
        setUseLabel(false);
        addChild(new HDHomeRunDeviceID(parent));
        addChild(new HDHomeRunTunerIndex(parent));
        addChild(new SignalTimeout(parent, 1000, 250));
        addChild(new ChannelTimeout(parent, 3000, 1750));
        addChild(new SingleCardInput(parent));
    };

  private:
    CaptureCard &parent;
};

V4LConfigurationGroup::V4LConfigurationGroup(CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    cardinfo(new TransLabelSetting()),  vbidev(new VBIDevice(parent)),
    input(new TunerCardInput(parent))
{
    VideoDevice *device = new VideoDevice(parent);
    HorizontalConfigurationGroup *audgrp =
        new HorizontalConfigurationGroup(false, false, true, true);

    cardinfo->setLabel(tr("Probed info"));
    audgrp->addChild(new AudioRateLimit(parent));
    audgrp->addChild(new SkipBtAudio(parent));

    addChild(device);
    addChild(cardinfo);
    addChild(vbidev);
    addChild(new AudioDevice(parent));
    addChild(audgrp);
    addChild(input);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
};

void V4LConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn = QString::null;

    int videofd = open(device.ascii(), O_RDWR);
    if (videofd >= 0)
    {
        if (!CardUtil::GetV4LInfo(videofd, cn, dn))
            ci = cn = tr("Failed to probe");
        else if (!dn.isEmpty())
            ci = cn + "  [" + dn + "]";
        close(videofd);
    }

    cardinfo->setValue(ci);
    vbidev->setFilter(cn, dn);
    input->fillSelections(device);
}


MPEGConfigurationGroup::MPEGConfigurationGroup(CaptureCard &a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent), cardinfo(new TransLabelSetting()),
    input(new TunerCardInput(parent))
{
    VideoDevice *device =
        new VideoDevice(parent, 0, 15, QString::null, "ivtv");

    cardinfo->setLabel(tr("Probed info"));

    addChild(device);
    addChild(cardinfo);
    addChild(input);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void MPEGConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn = QString::null;

    int videofd = open(device.ascii(), O_RDWR);
    if (videofd >= 0)
    {
        if (!CardUtil::GetV4LInfo(videofd, cn, dn))
            ci = cn = tr("Failed to probe");
        else if (!dn.isEmpty())
            ci = cn + "  [" + dn + "]";
        close(videofd);
    }

    cardinfo->setValue(ci);
    input->fillSelections(device);
}

pcHDTVConfigurationGroup::pcHDTVConfigurationGroup(CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent), cardinfo(new TransLabelSetting()),
    input(new TunerCardInput(parent))
{
    VideoDevice    *atsc_device     = new VideoDevice(parent, 0, 64);
    SignalTimeout  *signal_timeout  = new SignalTimeout(parent, 500, 250);
    ChannelTimeout *channel_timeout = new ChannelTimeout(parent, 2000, 1750);

    addChild(atsc_device);
    addChild(cardinfo);
    addChild(signal_timeout);
    addChild(channel_timeout);
    addChild(input);

    connect(atsc_device, SIGNAL(valueChanged(const QString&)),
            this,        SLOT(  probeCard(   const QString&)));

    probeCard(atsc_device->getValue());
}

void pcHDTVConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn = QString::null;

    int videofd = open(device.ascii(), O_RDWR);
    if (videofd >= 0)
    {
        if (!CardUtil::GetV4LInfo(videofd, cn, dn))
            ci = cn = tr("Failed to probe");
        else if (!dn.isEmpty())
            ci = cn + "  [" + dn + "]";
        close(videofd);
    }

    cardinfo->setValue(ci);
    input->fillSelections(device);
}

CaptureCardGroup::CaptureCardGroup(CaptureCard &parent) :
    TriggeredConfigurationGroup(true, true, false, false)
{
    setLabel(QObject::tr("Capture Card Setup"));

    CardType* cardtype = new CardType(parent);
    addChild(cardtype);

    setTrigger(cardtype);
    setSaveAll(false);
    
#ifdef USING_V4L
    addTarget("V4L",       new V4LConfigurationGroup(parent));
    addTarget("HDTV",      new pcHDTVConfigurationGroup(parent));
# ifdef USING_IVTV
    addTarget("MPEG",      new MPEGConfigurationGroup(parent));
# endif // USING_IVTV
#endif // USING_V4L

#ifdef USING_DVB
    addTarget("DVB",       new DVBConfigurationGroup(parent));
#endif // USING_DVB

#ifdef USING_FIREWIRE
    addTarget("FIREWIRE",  new FirewireConfigurationGroup(parent));
#endif // USING_FIREWIRE

#ifdef USING_DBOX2
    addTarget("DBOX2",     new DBOX2ConfigurationGroup(parent));
#endif // USING_DBOX2

#ifdef USING_HDHOMERUN
    addTarget("HDHOMERUN", new HDHomeRunConfigurationGroup(parent));
#endif // USING_HDHOMERUN

#ifdef USING_IPTV
    addTarget("FREEBOX",   new IPTVConfigurationGroup(parent));
#endif // USING_IPTV
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
        "SELECT cardid, cardtype, videodevice "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME";
    if (no_children)
        qstr += " AND parentid='0'";

    query.prepare(qstr);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("CaptureCard::fillSelections", query);
        return;
    }
    if (query.size() == 0)
        return;

    while (query.next())
    {
        QString label = CardUtil::GetDeviceLabel(
            query.value(0).toUInt(),
            query.value(1).toString(),
            query.value(2).toString());

        setting->addSelection(label, query.value(0).toString());
    }
}

void CaptureCard::loadByID(int cardid) 
{
    id->setValue(cardid);
    load();
}

CardType::CardType(const CaptureCard &parent) :
    ComboBoxSetting(this),
    CaptureCardDBStorage(this, parent, "cardtype") 
{
    setLabel(QObject::tr("Card type"));
    setHelpText(QObject::tr("Change the cardtype to the appropriate type for "
                "the capture card you are configuring."));
    fillSelections(this);
}

void CardType::fillSelections(SelectSetting* setting)
{
#ifdef USING_V4L
    setting->addSelection(
        QObject::tr("Analog V4L capture card"), "V4L");
    setting->addSelection(
        QObject::tr("MJPEG capture card (Matrox G200, DC10)"), "MJPEG");
# ifdef USING_IVTV
    setting->addSelection(
        QObject::tr("MPEG-2 encoder card (PVR-x50, PVR-500)"), "MPEG");
# endif // USING_IVTV
#endif // USING_V4L

#ifdef USING_DVB
    setting->addSelection(
        QObject::tr("DVB DTV capture card (v3.x)"), "DVB");
#endif // USING_DVB

#ifdef USING_V4L
    setting->addSelection(
        QObject::tr("pcHDTV DTV capture card (w/V4L drivers)"), "HDTV");
#endif // USING_V4L

#ifdef USING_FIREWIRE
    setting->addSelection(
        QObject::tr("FireWire cable box"), "FIREWIRE");
#endif // USING_FIREWIRE

#ifdef USING_V4L
    setting->addSelection(
        QObject::tr("USB MPEG-4 encoder box (Plextor ConvertX, etc)"),
        "GO7007");
#endif // USING_V4L

#ifdef USING_DBOX2
    setting->addSelection(
        QObject::tr("DBox2 TCP/IP cable box"), "DBOX2");
#endif // USING_DBOX2

#ifdef USING_HDHOMERUN
    setting->addSelection(
        QObject::tr("HDHomeRun DTV tuner box"), "HDHOMERUN");
#endif // USING_HDHOMERUN

#ifdef USING_IPTV
    setting->addSelection(QObject::tr("Network Recorder"), "FREEBOX");
#endif // USING_IPTV
}

class CardID : public SelectLabelSetting, public CardInputDBStorage
{
  public:
    CardID(const CardInput &parent) :
        SelectLabelSetting(this), CardInputDBStorage(this, parent, "cardid")
    {
        setLabel(QObject::tr("Capture device"));
    };

    virtual void load() {
        fillSelections();
        CardInputDBStorage::load();
    };

    void fillSelections() {
        CaptureCard::fillSelections(this);
    };
};

class ChildID : public LabelSetting, public CardInputDBStorage
{
  public:
    ChildID(const CardInput &parent) :
        LabelSetting(this), CardInputDBStorage(this, parent, "childcardid")
    {
        setValue("0");
        setVisible(false);
    }
};

class InputDisplayName : public LineEditSetting, public CardInputDBStorage
{
  public:
    InputDisplayName(const CardInput &parent) :
        LineEditSetting(this),
        CardInputDBStorage(this, parent, "displayname")
    {
        setLabel(QObject::tr("Display Name (optional)"));
        setHelpText(QObject::tr(
                        "This name is displayed on screen when live TV begins "
                        "and when changing the selected input or card. If you "
                        "use this, make sure the information is unique for "
                        "each input."));
    };
};

class SourceID : public ComboBoxSetting, public CardInputDBStorage
{
  public:
    SourceID(const CardInput &parent) :
        ComboBoxSetting(this), CardInputDBStorage(this, parent, "sourceid")
    {
        setLabel(QObject::tr("Video source"));
        addSelection(QObject::tr("(None)"), "0");
    };

    virtual void load() {
        fillSelections();
        CardInputDBStorage::load();
    };

    void fillSelections() {
        clearSelections();
        addSelection(QObject::tr("(None)"), "0");
        VideoSource::fillSelections(this);
    };
};

class InputName : public LabelSetting, public CardInputDBStorage
{
  public:
    InputName(const CardInput &parent) :
        LabelSetting(this), CardInputDBStorage(this, parent, "inputname")
    {
        setLabel(QObject::tr("Input"));
    };
};

class FreeToAir : public CheckBoxSetting, public CardInputDBStorage
{
  public:
    FreeToAir(const CardInput &parent) :
        CheckBoxSetting(this),
        CardInputDBStorage(this, parent, "freetoaironly")
    {
        setValue(true);
        setLabel(QObject::tr("Unencrypted channels only"));
        setHelpText(QObject::tr(
                        "If set, only unencrypted channels will be tuned to "
                        "by MythTV or not be ignored by the MythTV channel "
                        "scanner."));
    };
};

class RadioServices : public CheckBoxSetting, public CardInputDBStorage
{
  public:
    RadioServices(const CardInput &parent) :
        CheckBoxSetting(this), 
        CardInputDBStorage(this, parent, "radioservices")
    {
        setValue(true);
        setLabel(QObject::tr("Allow audio only channels"));
        setHelpText(QObject::tr(
                        "If set, audio only channels will not be ignored "
                        "by the MythTV channel scanner.")); 
    };
};

class QuickTune : public ComboBoxSetting, public CardInputDBStorage
{
  public:
    QuickTune(const CardInput &parent) :
        ComboBoxSetting(this), CardInputDBStorage(this, parent, "quicktune")
    {
        setLabel(QObject::tr("Use quick tuning"));
        addSelection(QObject::tr("Never"),        "0", true);
        //addSelection(QObject::tr("Live TV only"), "1", true);
        addSelection(QObject::tr("Always"),       "2", false);
        setHelpText(QObject::tr(
                        "If enabled MythTV will tune using only the "
                        "MPEG program number. The program numbers "
                        "change more often than DVB or ATSC tuning "
                        "parameters, so this is slightly less reliable."));
    };
};

class ExternalChannelCommand :
    public LineEditSetting, public CardInputDBStorage
{
  public:
    ExternalChannelCommand(const CardInput &parent) :
        LineEditSetting(this),
        CardInputDBStorage(this, parent, "externalcommand")
    {
        setLabel(QObject::tr("External channel change command"));
        setValue("");
        setHelpText(QObject::tr("If specified, this command will be run to "
                    "change the channel for inputs which have an external "
                    "tuner device such as a cable box. The first argument "
                    "will be the channel number."));
    };
};

class PresetTuner : public LineEditSetting, public CardInputDBStorage
{
  public:
    PresetTuner(const CardInput &parent) :
        LineEditSetting(this),
        CardInputDBStorage(this, parent, "tunechan")
    {
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

    DBChanList channels = ChannelUtil::GetChannels(sourceid.toUInt(), false);

    if (channels.empty())
    {
        addSelection(tr("Please add channels to this source"),
                     startChan.isEmpty() ? "" : startChan);
        return;
    }

    // If there are channels sort them, then add them 
    // (selecting the old start channel if it is there).
    QString order = gContext->GetSetting("ChannelOrdering", "channum");
    ChannelUtil::SortChannels(channels, order);
    for (uint i = 0; i < channels.size(); i++)
    {
        const QString channum = channels[i].channum;
        addSelection(channum, channum, channum == startChan);
    }
}

class InputPriority : public SpinBoxSetting, public CardInputDBStorage
{
  public:
    InputPriority(const CardInput &parent) :
        SpinBoxSetting(this, -99, 99, 1),
        CardInputDBStorage(this, parent, "recpriority")
    {
        setLabel(QObject::tr("Input priority"));
        setValue(0);
        setHelpText(QObject::tr("If the input priority is not equal for "
                    "all inputs, the scheduler may choose to record a show "
                    "at a later time so that it can record on an input with "
                    "a higher value."));
    };
};

class DishNetEIT : public CheckBoxSetting, public CardInputDBStorage
{
  public:
    DishNetEIT(const CardInput &parent) :
        CheckBoxSetting(this), 
        CardInputDBStorage(this, parent, "dishnet_eit")
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

CardInput::CardInput(bool isDTVcard,  bool isDVBcard,
                     bool isNewInput, int _cardid) :
    id(new ID()),
    cardid(new CardID(*this)),
    childid(),
    inputname(new InputName(*this)),
    sourceid(new SourceID(*this)),
    startchan(new StartingChannel(*this)),
    scan(new TransButtonSetting()),
    srcfetch(new TransButtonSetting()),
    externalInputSettings(new DiSEqCDevSettings())
{
    addChild(id);

    if (CardUtil::IsInNeedOfExternalInputConf(_cardid))
    {
        addChild(new DTVDeviceConfigGroup(*externalInputSettings,
                                          _cardid, isNewInput));
    }

    ConfigurationGroup *group =
        new VerticalConfigurationGroup(false, false, true, true);

    group->setLabel(QObject::tr("Connect source to input"));

    HorizontalConfigurationGroup *ci;
    ci = new HorizontalConfigurationGroup(false, false);
    ci->addChild(cardid);
    ci->addChild(inputname);
    group->addChild(ci);
    group->addChild(new InputDisplayName(*this));
    group->addChild(sourceid);

    if (!isDTVcard)
    {
        group->addChild(new ExternalChannelCommand(*this));
        group->addChild(new PresetTuner(*this));
    }

    if (isDTVcard)
    {
        // we place this in a group just so the margins match the DVB ones.
        ConfigurationGroup *chgroup = 
            new VerticalConfigurationGroup(false, false, true, true);
        chgroup->addChild(new QuickTune(*this));
        chgroup->addChild(new FreeToAir(*this));
        group->addChild(chgroup);
    }

    if (isDVBcard)
    {
        ConfigurationGroup *chgroup = 
            new HorizontalConfigurationGroup(false, false, true, true);
        chgroup->addChild(new RadioServices(*this));
        chgroup->addChild(new DishNetEIT(*this));
        group->addChild(chgroup);
    }

    scan->setLabel(tr("Scan for channels"));
    scan->setHelpText(
        tr("Use channel scanner to find channels for this input."));

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

    group->addChild(startchan);
    group->addChild(new InputPriority(*this));

    addChild(group);

    childid = new ChildID(*this);
    addChild(childid);

    setName("CardInput");
    SetSourceID("-1");

    connect(scan,     SIGNAL(pressed()), SLOT(channelScanner()));
    connect(srcfetch, SIGNAL(pressed()), SLOT(sourceFetch()));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            startchan,SLOT(  SetSourceID (const QString&)));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            this,     SLOT(  SetSourceID (const QString&)));
}

CardInput::~CardInput()
{
    if (externalInputSettings)
    {
        delete externalInputSettings;
        externalInputSettings = NULL;
    }
}

void CardInput::SetSourceID(const QString &sourceid)
{
    bool enable = (sourceid.toInt() > 0);
    scan->setEnabled(enable);
    srcfetch->setEnabled(enable);
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
    uint crdid = cardid->getValue().toUInt();
    QString in = inputname->getValue();

#ifdef USING_BACKEND
    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    save(); // save info for scanner.

    QString cardtype = CardUtil::GetRawCardType(crdid, in);
    if (CardUtil::IsUnscanable(cardtype))
    {
        VERBOSE(VB_IMPORTANT, QString("Sorry, %1 cards do not "
                                      "yet support scanning.").arg(cardtype));
        return;
    }

    ScanWizard *scanwizard = new ScanWizard(srcid, crdid, in);
    scanwizard->exec(false, true);
    scanwizard->deleteLater();

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
    uint child = childid->getValue().toUInt();
    uint crdid = (child) ? child : cardid->getValue().toUInt();

    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    if (cardid && srcid)
    {
        QString cardtype = CardUtil::GetRawCardType(crdid, 0);

        if (!CardUtil::IsUnscanable(cardtype) &&
            !CardUtil::IsEncoder(cardtype)    &&
            !num_channels_before)
        {
            VERBOSE(VB_IMPORTANT, "Skipping channel fetch, you need to "
                    "scan for channels first.");
            return;
        }

        SourceUtil::UpdateChannelsFromListings(srcid, cardtype);
    }

    if (SourceUtil::GetChannelCount(srcid))
        startchan->SetSourceID(QString::number(srcid));        
    if (num_channels_before)
    {
        startchan->load();
        startchan->save();
    }
}

QString CardInputDBStorage::whereClause(MSqlBindings& bindings) 
{
    QString cardinputidTag(":WHERECARDINPUTID");
    
    QString query("cardinputid = " + cardinputidTag);

    bindings.insert(cardinputidTag, parent.getInputID());

    return query;
}

QString CardInputDBStorage::setClause(MSqlBindings& bindings) 
{
    QString cardinputidTag(":SETCARDINPUTID");
    QString colTag(":SET" + getColumn().upper());

    QString query("cardinputid = " + cardinputidTag + ", " + 
            getColumn() + " = " + colTag);

    bindings.insert(cardinputidTag, parent.getInputID());
    bindings.insert(colTag, setting->getValue());

    return query;
}

void CardInput::loadByID(int inputid) 
{
    id->setValue(inputid);
    externalInputSettings->Load(inputid);
    ConfigurationWizard::load();
}

void CardInput::loadByInput(int _cardid, QString _inputname) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardinputid FROM cardinput "
                  "WHERE cardid = :CARDID AND inputname = :INPUTNAME");
    query.bindValue(":CARDID", _cardid);
    query.bindValue(":INPUTNAME", _inputname);

    if (query.exec() && query.isActive() && query.next())
    {
        loadByID(query.value(0).toInt());
    } 
    else 
    { // create new input connection
        load();
        cardid->setValue(QString::number(_cardid));
        inputname->setValue(_inputname);
    }
}

void CardInput::save(void)
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
        externalInputSettings->Store(getInputID());
    }
}

int CardInputDBStorage::getInputID(void) const 
{
    return parent.getInputID();
}

int CaptureCardDBStorage::getCardID(void) const 
{
    return parent.getCardID();
}

CaptureCardEditor::CaptureCardEditor() : listbox(new ListBoxSetting(this))
{
    listbox->setLabel(tr("Capture cards"));
    addChild(listbox);
}

int CaptureCardEditor::exec(void)
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void CaptureCardEditor::load(void)
{
    listbox->clearSelections();
    listbox->addSelection(QObject::tr("(New capture card)"), "0");
    listbox->addSelection(QObject::tr("(Delete all capture cards on %1)")
                          .arg(gContext->GetHostName()), "-1");
    listbox->addSelection(QObject::tr("(Delete all capture cards)"), "-2");
    CaptureCard::fillSelections(listbox, true);
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
    if (!listbox->getValue().toInt())
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
    const int cardid = listbox->getValue().toInt();
    if (-1 == cardid)
    {
        int val = MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(), "",
            tr("Are you sure you want to delete "
               "ALL capture cards on %1?").arg(gContext->GetHostName()),
            tr("Yes, delete capture cards"),
            tr("No, don't"), 2);

        if (0 == val)
        {
            MSqlQuery cards(MSqlQuery::InitCon());

            cards.prepare(
                "SELECT cardid "
                "FROM capturecard "
                "WHERE hostname = :HOSTNAME AND "
                "      parentid = '0'");
            cards.bindValue(":HOSTNAME", gContext->GetHostName());

            if (!cards.exec() || !cards.isActive())
            {
                MythPopupBox::showOkPopup(
                    gContext->GetMainWindow(),
                    tr("Error getting list of cards for this host"),
                    tr("Unable to delete capturecards for %1")
                    .arg(gContext->GetHostName()));

                MythContext::DBError("Selecting cardids for deletion", cards);
                return;
            }

            while (cards.next())
                CardUtil::DeleteCard(cards.value(0).toUInt());
        }
    }
    else if (-2 == cardid)
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
    int val = MythPopupBox::show2ButtonPopup(
        gContext->GetMainWindow(), "",
        tr("Are you sure you want to delete this capture card?"),
        tr("Yes, delete capture card"),
        tr("No, don't"), 2);

    if (val == 0)
    {
        CardUtil::DeleteCard(listbox->getValue().toUInt());
        load();
    }
}

VideoSourceEditor::VideoSourceEditor() : listbox(new ListBoxSetting(this))
{
    listbox->setLabel(tr("Video sources"));
    addChild(listbox);
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

void VideoSourceEditor::load(void)
{
    listbox->clearSelections();
    listbox->addSelection(QObject::tr("(New video source)"), "0");
    listbox->addSelection(QObject::tr("(Delete all video sources)"), "-1");
    VideoSource::fillSelections(listbox);
}

void VideoSourceEditor::menu(void)
{
    if (!listbox->getValue().toInt())
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

void VideoSourceEditor::edit(void)
{
    const int sourceid = listbox->getValue().toInt();
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

        // Delete the channels associated with the source
        query.prepare("DELETE FROM channel "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", listbox->getValue());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Deleting Channels", query);
            return;
        }

        // Delete the inputs associated with the source
        query.prepare("DELETE FROM cardinput "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", listbox->getValue());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Deleting cardinputs", query);
            return;
        }

        // Delete the source itself
        query.prepare("DELETE FROM videosource "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", listbox->getValue());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Deleting VideoSource", query);

        load();
    }
}

CardInputEditor::CardInputEditor() : listbox(new ListBoxSetting(this))
{
    listbox->setLabel(tr("Input connections"));
    addChild(listbox);
}

int CardInputEditor::exec(void)
{
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        cardinputs[listbox->getValue().toInt()]->exec();

    return QDialog::Rejected;
}

void CardInputEditor::load() 
{
    cardinputs.clear();
    listbox->clearSelections();

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
            listbox->addSelection(inputLabels[i], QString::number(j));
        }
    }
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
    if (short_name.right(3) == "DVB-T")
        short_name = short_name.left(short_name.length() - 6);

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
        short_name = "DViCO v2 or Air2PC v3 or pcHDTV HD-5500";

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
            signal_timeout->setValue(1000);
            channel_timeout->setValue(3000);
            break;
        case CardUtil::OFDM:
        {
            cardtype->setValue("DVB-T");
            cardname->setValue(frontend_name);
            signal_timeout->setValue(500);
            channel_timeout->setValue(3000);
            if (frontend_name.lower().find("usb") >= 0)
            {
                signal_timeout->setValue(40000);
                channel_timeout->setValue(42500);
            }

            // slow down tuning for buggy drivers
            if ((frontend_name == "DiBcom 3000P/M-C DVB-T") ||
                (frontend_name ==
                 "TerraTec/qanu USB2.0 Highspeed DVB-T Receiver"))
            {
                tuning_delay->setValue(200);
            }

            QString short_name = remove_chaff(frontend_name);
            buttonAnalog->setVisible(
                short_name.left(15).lower() == "zarlink zl10353" ||
                short_name.lower() == "wintv hvr 900 m/r: 65008/a1c0");
        }
        break;
        case CardUtil::ATSC:
        {
            QString short_name = remove_chaff(frontend_name);
            cardtype->setValue("ATSC");
            cardname->setValue(short_name);
            signal_timeout->setValue(500);
            channel_timeout->setValue(3000);

            // According to #1779 and #1935 the AverMedia 180 needs
            // a 3000 ms signal timeout, at least for QAM tuning.
            if (frontend_name = "Nextwave NXT200X VSB/QAM frontend")
            {
                signal_timeout->setValue(3000);
                channel_timeout->setValue(5500);
            }

            if (frontend_name.lower().find("usb") < 0)
            {
                buttonAnalog->setVisible(
                    short_name.left(6).lower() == "pchdtv" ||
                    short_name.left(5).lower() == "dvico" ||
                    short_name.left(8).lower() == "nextwave");
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

TunerCardInput::TunerCardInput(const CaptureCard &parent,
                               QString dev, QString type) :
    ComboBoxSetting(this), CaptureCardDBStorage(this, parent, "defaultinput"),
    last_device(dev), last_cardtype(type), last_diseqct(-1)
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
        CardUtil::probeInputs(device, last_cardtype);

    for (QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);
}

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    diseqc_tree(new DiSEqCDevTree())
{
    DVBCardNum* cardnum = new DVBCardNum(parent);
    cardname = new DVBCardName();
    cardtype = new DVBCardType();

    signal_timeout = new SignalTimeout(parent, 500, 250);
    channel_timeout = new ChannelTimeout(parent, 3000, 1750);

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
           "using an Air2PC HD-5000 or Avermedia A180 card. "
           "These cards do not support analog tuning, "
           "but the DVB drivers do not yet allow us to "
           "detect this problem."));

    TransButtonSetting *buttonRecOpt = new TransButtonSetting();
    buttonRecOpt->setLabel(tr("Recording Options"));    

    HorizontalConfigurationGroup *advcfg = 
        new HorizontalConfigurationGroup(false, false, true, true);
    advcfg->addChild(buttonDiSEqC);
    advcfg->addChild(buttonAnalog);
    advcfg->addChild(buttonRecOpt);
    addChild(advcfg);

    defaultinput = new DVBInput(parent);
    addChild(defaultinput);
    defaultinput->setVisible(false);

    tuning_delay = new DVBTuningDelay(parent);
    addChild(tuning_delay);
    tuning_delay->setVisible(false);

    connect(cardnum,      SIGNAL(valueChanged(const QString&)),
            this,         SLOT(  probeCard   (const QString&)));
    connect(buttonDiSEqC, SIGNAL(pressed()),
            this,         SLOT(  DiSEqCPanel()));
    connect(buttonAnalog, SIGNAL(pressed()),
            &parent,      SLOT(  analogPanel()));
    connect(buttonRecOpt, SIGNAL(pressed()),
            &parent,      SLOT(  recorderOptionsPanel()));

    cardnum->setValue(0);
}

DVBConfigurationGroup::~DVBConfigurationGroup()
{
    if (diseqc_tree)
    {
        delete diseqc_tree;
        diseqc_tree = NULL;
    }
}

void DVBConfigurationGroup::DiSEqCPanel()
{
    parent.reload(); // ensure card id is valid

    DTVDeviceTreeWizard diseqcWiz(*diseqc_tree);
    diseqcWiz.exec();
    defaultinput->fillSelections(diseqc_tree->IsInNeedOfConf());
}

void DVBConfigurationGroup::load()
{
    VerticalConfigurationGroup::load();
    diseqc_tree->Load(parent.getCardID());
    defaultinput->fillSelections(diseqc_tree->IsInNeedOfConf());
}

void DVBConfigurationGroup::save()
{
    VerticalConfigurationGroup::save();
    diseqc_tree->Store(parent.getCardID());
    DiSEqCDev trees;
    trees.InvalidateTrees();
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
/* TODO FIXME
    card->setLabel(tr("Analog Options for") + " " + devname);
*/
    card->exec();
    delete card;
}

void CaptureCard::recorderOptionsPanel()
{
    reload();

    RecorderOptions acw(*this);
    acw.exec();
}

RecorderOptions::RecorderOptions(CaptureCard &parent)
{
    VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
    rec->setLabel(QObject::tr("Recorder Options"));
    rec->setUseLabel(false);

    rec->addChild(new DVBNoSeqStart(parent));
    rec->addChild(new DVBOnDemand(parent));
    rec->addChild(new DVBTuningDelay(parent));

    addChild(rec);
}
