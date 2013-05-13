// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QTextStream>
#include <QStringList>
#include <QCursor>
#include <QLayout>
#include <QFile>
#include <QMap>
#include <QDir>
#include <QDateTime>

// MythTV headers
#include "mythconfig.h"
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "mythcorecontext.h"
#include "videosource.h"
#include "datadirect.h"
#include "scanwizard.h"
#include "cardutil.h"
#include "sourceutil.h"
#include "channelinfo.h"
#include "channelutil.h"
#include "frequencies.h"
#include "diseqcsettings.h"
#include "firewiredevice.h"
#include "compat.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "libmythupnp/httprequest.h"    // for TestMimeType()
#include "mythsystem.h"
#include "exitcodes.h"

#ifdef USING_DVB
#include "dvbtypes.h"
#endif

#ifdef USING_V4L2
#include <linux/videodev2.h>
#endif

#ifdef USING_HDHOMERUN
#include "hdhomerun.h"
#endif

static const uint kDefaultMultirecCount = 2;

VideoSourceSelector::VideoSourceSelector(uint           _initial_sourceid,
                                         const QString &_card_types,
                                         bool           _must_have_mplexid) :
    ComboBoxSetting(this),
    initial_sourceid(_initial_sourceid),
    card_types(_card_types),
    must_have_mplexid(_must_have_mplexid)
{
    card_types.detach();
    setLabel(tr("Video Source"));
}

void VideoSourceSelector::Load(void)
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
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

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

class InstanceCount : public TransSpinBoxSetting
{
  public:
    InstanceCount(const CaptureCard &parent) : TransSpinBoxSetting(1, 5, 1)
    {
        setLabel(QObject::tr("Max recordings"));
        setHelpText(
            QObject::tr(
                "Maximum number of simultaneous recordings this device "
                "should make. Some digital transmitters transmit multiple "
                "programs on a multiplex, if this is set to a value greater "
                "than one MythTV can sometimes take advantage of this."));
        uint cnt = parent.GetInstanceCount();
        cnt = (!cnt) ? kDefaultMultirecCount : ((cnt < 1) ? 1 : cnt);
        setValue(cnt);
    };
};

QString VideoSourceDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString sourceidTag(":WHERESOURCEID");

    QString query("sourceid = " + sourceidTag);

    bindings.insert(sourceidTag, m_parent.getSourceID());

    return query;
}

QString VideoSourceDBStorage::GetSetClause(MSqlBindings& bindings) const
{
    QString sourceidTag(":SETSOURCEID");
    QString colTag(":SET" + GetColumnName().toUpper());

    QString query("sourceid = " + sourceidTag + ", " +
            GetColumnName() + " = " + colTag);

    bindings.insert(sourceidTag, m_parent.getSourceID());
    bindings.insert(colTag, user->GetDBValue());

    return query;
}

QString CaptureCardDBStorage::GetWhereClause(MSqlBindings& bindings) const
{
    QString cardidTag(":WHERECARDID");

    QString query("cardid = " + cardidTag);

    bindings.insert(cardidTag, m_parent.getCardID());

    return query;
}

QString CaptureCardDBStorage::GetSetClause(MSqlBindings& bindings) const
{
    QString cardidTag(":SETCARDID");
    QString colTag(":SET" + GetColumnName().toUpper());

    QString query("cardid = " + cardidTag + ", " +
            GetColumnName() + " = " + colTag);

    bindings.insert(cardidTag, m_parent.getCardID());
    bindings.insert(colTag, user->GetDBValue());

    return query;
}

class XMLTVGrabber : public ComboBoxSetting, public VideoSourceDBStorage
{
  public:
    XMLTVGrabber(const VideoSource &parent) :
        ComboBoxSetting(this),
        VideoSourceDBStorage(this, parent, "xmltvgrabber")
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

void TransFreqTableSelector::Load(void)
{
    int idx = getValueIndex(gCoreContext->GetSetting("FreqTable"));
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
        MythDB::DBError("TransFreqTableSelector::load", query);
        return;
    }

    loaded_freq_table = QString::null;

    if (query.next())
    {
        loaded_freq_table = query.value(0).toString();
        if (!loaded_freq_table.isEmpty() &&
            (loaded_freq_table.toLower() != "default"))
        {
            int idx = getValueIndex(loaded_freq_table);
            if (idx >= 0)
                setValue(idx);
        }
    }
}

void TransFreqTableSelector::Save(void)
{
    LOG(VB_GENERAL, LOG_INFO, "TransFreqTableSelector::Save(void)");

    if ((loaded_freq_table == getValue()) ||
        ((loaded_freq_table.toLower() == "default") &&
         (getValue() == gCoreContext->GetSetting("FreqTable"))))
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
        MythDB::DBError("TransFreqTableSelector::load", query);
        return;
    }
}

void TransFreqTableSelector::SetSourceID(uint _sourceid)
{
    sourceid = _sourceid;
    Load();
}

class UseEIT : public CheckBoxSetting, public VideoSourceDBStorage
{
  public:
    UseEIT(const VideoSource &parent) :
        CheckBoxSetting(this), VideoSourceDBStorage(this, parent, "useeit")
    {
        setLabel(QObject::tr("Perform EIT scan"));
        setHelpText(QObject::tr(
                        "If enabled, program guide data for channels on this "
                        "source will be updated with data provided by the "
                        "channels themselves 'Over-the-Air'."));
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
        LineEditSetting(this, true),
        VideoSourceDBStorage(this, parent, "password")
    {
        SetPasswordEcho(true);
        setLabel(QObject::tr("Password"));
    }
};

void DataDirectLineupSelector::fillSelections(const QString &uid,
                                              const QString &pwd,
                                              int _source)
{
    (void) uid;
    (void) pwd;
#ifdef USING_BACKEND
    if (uid.isEmpty() || pwd.isEmpty())
        return;

    qApp->processEvents();

    DataDirectProcessor ddp(_source, uid, pwd);
    QString waitMsg = tr("Fetching lineups from %1...")
        .arg(ddp.GetListingsProviderName());

    LOG(VB_GENERAL, LOG_INFO, waitMsg);
    MythProgressDialog *pdlg = new MythProgressDialog(waitMsg, 2);

    clearSelections();

    pdlg->setProgress(1);

    if (!ddp.GrabLineupsOnly())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DDLS: fillSelections did not successfully load selections");
        pdlg->deleteLater();
        return;
    }
    const DDLineupList lineups = ddp.GetLineups();

    DDLineupList::const_iterator it;
    for (it = lineups.begin(); it != lineups.end(); ++it)
        addSelection((*it).displayname, (*it).lineupid);

    pdlg->setProgress(2);
    pdlg->Close();
    pdlg->deleteLater();
#else // USING_BACKEND
    LOG(VB_GENERAL, LOG_ERR,
        "You must compile the backend to set up a DataDirect line-up");
#endif // USING_BACKEND
}

void DataDirect_config::Load()
{
    VerticalConfigurationGroup::Load();
    bool is_sd_userid = userid->getValue().contains('@') > 0;
    bool match = ((is_sd_userid  && (source == DD_SCHEDULES_DIRECT)) ||
                  (!is_sd_userid && (source == DD_ZAP2IT)));
    if (((userid->getValue() != lastloadeduserid) ||
         (password->getValue() != lastloadedpassword)) && match)
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

    HorizontalConfigurationGroup *up =
        new HorizontalConfigurationGroup(false, false, true, true);

    up->addChild(userid   = new DataDirectUserID(parent));
    addChild(up);

    HorizontalConfigurationGroup *lp =
        new HorizontalConfigurationGroup(false, false, true, true);

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
    QString filename = QString("%1/%2.xmltv")
        .arg(GetConfDir()).arg(parent.getSourceName());

    grabberArgs.push_back("--config-file");
    grabberArgs.push_back(filename);
    grabberArgs.push_back("--configure");

    addChild(new UseEIT(parent));

    TransButtonSetting *config = new TransButtonSetting();
    config->setLabel(tr("Configure"));
    config->setHelpText(tr("Run XMLTV configure command."));

    addChild(config);

    connect(config, SIGNAL(pressed()), SLOT(RunConfig()));
}

void XMLTV_generic_config::Save()
{
    VerticalConfigurationGroup::Save();
#if 0
    QString err_msg = QObject::tr(
        "You MUST run 'mythfilldatabase --manual' the first time,\n"
        "instead of just 'mythfilldatabase'.\nYour grabber does not provide "
        "channel numbers, so you have to set them manually.");

    if (is_grabber_external(grabber))
    {
        LOG(VB_GENERAL, LOG_ERR, err_msg);
        MythPopupBox::showOkPopup(
            GetMythMainWindow(), QObject::tr("Warning."), err_msg);
    }
#endif
}

void XMLTV_generic_config::RunConfig(void)
{
    TerminalWizard *tw = new TerminalWizard(grabber, grabberArgs);
    tw->exec(false, true);
    delete tw;
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

void EITOnly_config::Save(void)
{
    // Force this value on
    useeit->setValue(true);
    useeit->Save();
}

NoGrabber_config::NoGrabber_config(const VideoSource& _parent) :
    VerticalConfigurationGroup(false, false, false, false)
{
    useeit = new UseEIT(_parent);
    useeit->setValue(false);
    useeit->setVisible(false);
    addChild(useeit);

    TransLabelSetting *label = new TransLabelSetting();
    label->setValue(QObject::tr("Do not configure a grabber"));
    addChild(label);
}

void NoGrabber_config::Save(void)
{
    useeit->setValue(false);
    useeit->Save();
}


XMLTVConfig::XMLTVConfig(const VideoSource &aparent) :
    TriggeredConfigurationGroup(false, true, false, false),
    parent(aparent), grabber(new XMLTVGrabber(parent))
{
    addChild(grabber);
    setTrigger(grabber);

    // only save settings for the selected grabber
    setSaveAll(false);

}

void XMLTVConfig::Load(void)
{
    addTarget("schedulesdirect1",
              new DataDirect_config(parent, DD_SCHEDULES_DIRECT));
    addTarget("eitonly",   new EITOnly_config(parent));
    addTarget("/bin/true", new NoGrabber_config(parent));

    grabber->addSelection(
        QObject::tr("North America (SchedulesDirect.org) (Internal)"),
        "schedulesdirect1");

    grabber->addSelection(
        QObject::tr("Transmitted guide only (EIT)"), "eitonly");

    grabber->addSelection(QObject::tr("No grabber"), "/bin/true");

    QString validValues;
    validValues += "schedulesdirect1";
    validValues += "eitonly";
    validValues += "/bin/true";

    QString gname, d1, d2, d3;
    SourceUtil::GetListingsLoginData(parent.getSourceID(), gname, d1, d2, d3);

    QString loc = "XMLTVConfig::Load: ";
    QString loc_err = "XMLTVConfig::Load, Error: ";

    QStringList name_list;
    QStringList prog_list;

    QStringList args;
    args += "baseline";

    MythSystem find_grabber_proc("tv_find_grabbers", args, 
                                 kMSStdOut | kMSBuffered | kMSRunShell);
    find_grabber_proc.Run(25);
    LOG(VB_GENERAL, LOG_INFO,
        loc + "Running 'tv_find_grabbers " + args.join(" ") + "'.");
    uint status = find_grabber_proc.Wait();

    if (status == GENERIC_EXIT_OK)
    {
        QTextStream ostream(find_grabber_proc.ReadAll());
        while (!ostream.atEnd())
        {
            QString grabber_list(ostream.readLine());
            QStringList grabber_split =
                grabber_list.split("|", QString::SkipEmptyParts);
            QString grabber_name = grabber_split[1] + " (xmltv)";
            QFileInfo grabber_file(grabber_split[0]);

            name_list.push_back(grabber_name);
            prog_list.push_back(grabber_file.fileName());
            LOG(VB_GENERAL, LOG_DEBUG, "Found " + grabber_split[0]);
        }
        LOG(VB_GENERAL, LOG_INFO, loc + "Finished running tv_find_grabbers");
    }
    else
        LOG(VB_GENERAL, LOG_ERR, loc + "Failed to run tv_find_grabbers");

    LoadXMLTVGrabbers(name_list, prog_list);

    TriggeredConfigurationGroup::Load();

}

void XMLTVConfig::LoadXMLTVGrabbers(
    QStringList name_list, QStringList prog_list)
{
    if (name_list.size() != prog_list.size())
        return;

    QString selValue = grabber->getValue();
    int     selIndex = grabber->getValueIndex(selValue);
    grabber->setValue(0);

    QString validValues;
    validValues += "schedulesdirect1";
    validValues += "eitonly";
    validValues += "/bin/true";

    for (uint i = 0; i < grabber->size(); i++)
    {
        if (!validValues.contains(grabber->GetValue(i)))
        {
            removeTarget(grabber->GetValue(i));
            i--;
        }
    }

    for (uint i = 0; i < (uint) name_list.size(); i++)
    {
        addTarget(prog_list[i],
                  new XMLTV_generic_config(parent, prog_list[i]));
        grabber->addSelection(name_list[i], prog_list[i]);
    }

    if (!selValue.isEmpty())
        selIndex = grabber->getValueIndex(selValue);
    if (selIndex >= 0)
        grabber->setValue(selIndex);

}

void XMLTVConfig::Save(void)
{
    TriggeredConfigurationGroup::Save();
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE videosource "
        "SET userid=NULL, password=NULL "
        "WHERE xmltvgrabber NOT IN ( 'datadirect', 'technovera', "
        "                            'schedulesdirect1' )");
    if (!query.exec())
        MythDB::DBError("XMLTVConfig::Save", query);
}

VideoSource::VideoSource()
{
    // must be first
    addChild(id = new ID());

    ConfigurationGroup *group = new VerticalConfigurationGroup(false, false);
    group->setLabel(QObject::tr("Video Source Setup"));
    group->addChild(name = new Name(*this));
    group->addChild(xmltv = new XMLTVConfig(*this));
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

    if (query.exec() && query.next())
    {
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
        dev.setNameFilters(QStringList("dtv*"));
        fillSelectionsFromDir(dev, minor_min, minor_max,
                              card, driver, false);
    };

    void fillSelectionsFromDir(const QDir &dir, bool absPath = true)
    {
        fillSelectionsFromDir(dir, 0, 255, QString::null, QString::null, false);
    }

    uint fillSelectionsFromDir(const QDir& dir,
                               uint minor_min, uint minor_max,
                               QString card, QString driver,
                               bool allow_duplicates)
    {
        uint cnt = 0;

        QFileInfoList il = dir.entryInfoList();
        QRegExp *driverExp = NULL;
        if (!driver.isEmpty())
            driverExp = new QRegExp(driver);

        for( QFileInfoList::iterator it  = il.begin();
                                     it != il.end();
                                   ++it )
        {
            QFileInfo &fi = *it;

            struct stat st;
            QString filepath = fi.absoluteFilePath();
            int err = lstat(filepath.toLocal8Bit().constData(), &st);

            if (err)
            {
                LOG(VB_GENERAL, LOG_ERR,
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
            QByteArray tmp = filepath.toLatin1();
            int videofd = open(tmp.constData(), O_RDWR);
            if (videofd >= 0)
            {
                QString cn, dn;
                if (CardUtil::GetV4LInfo(videofd, cn, dn) &&
                    (!driverExp     || (driverExp->exactMatch(dn)))  &&
                    (card.isEmpty() || (cn == card)))
                {
                    addSelection(filepath);
                    cnt++;
                }
                close(videofd);
            }

            // add to list of minors discovered to avoid duplicates
            minor_list[minor_num] = 1;
        }
        delete driverExp;

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
            if (!fillSelectionsFromDir(dev, card, driver) &&
                !getValue().isEmpty())
            {
                addSelection(getValue(),getValue(),true);
            }
        }
    }

    void fillSelectionsFromDir(const QDir &dir, bool absPath = true)
    {
        fillSelectionsFromDir(dir, QString::null, QString::null);
    }

    uint fillSelectionsFromDir(const QDir &dir, const QString &card,
                               const QString &driver)
    {
        QStringList devices;
        QFileInfoList il = dir.entryInfoList();
        for( QFileInfoList::iterator it  = il.begin();
                                     it != il.end();
                                   ++it )
        {
            QFileInfo &fi = *it;

            QString    device = fi.absoluteFilePath();
            QByteArray adevice = device.toLatin1();
            int vbifd = open(adevice.constData(), O_RDWR);
            if (vbifd < 0)
                continue;

            QString cn, dn;
            if (CardUtil::GetV4LInfo(vbifd, cn, dn)  &&
                (driver.isEmpty() || (dn == driver)) &&
                (card.isEmpty()   || (cn == card)))
            {
                devices.push_back(device);
            }

            close(vbifd);
        }

        QString sel = getValue();
        for (uint i = 0; i < (uint) devices.size(); i++)
            addSelection(devices[i], devices[i], devices[i] == sel);

        return (uint) devices.size();
    }
};

class FileDevice : public PathSetting, public CaptureCardDBStorage
{
  public:
    FileDevice(const CaptureCard &parent) :
        PathSetting(this, false),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setLabel(QObject::tr("File path"));
    };
};

class AudioDevice : public PathSetting, public CaptureCardDBStorage
{
  public:
    AudioDevice(const CaptureCard &parent) :
        PathSetting(this, false),
        CaptureCardDBStorage(this, parent, "audiodevice")
    {
        setLabel(QObject::tr("Audio device"));
#if USING_OSS
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
#endif
#if USING_ALSA
        addSelection("ALSA:default", "ALSA:default");
#endif
        addSelection(QObject::tr("(None)"), "NULL");
    };
};

class SignalTimeout : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    SignalTimeout(const CaptureCard &parent, uint value, uint min_val) :
        SpinBoxSetting(this, min_val, 60000, 250),
        CaptureCardDBStorage(this, parent, "signal_timeout")
    {
        setLabel(QObject::tr("Signal timeout (ms)"));
        setValue(value);
        setHelpText(QObject::tr(
                        "Maximum time (in milliseconds) MythTV waits for "
                        "a signal when scanning for channels."));
    };
};

class ChannelTimeout : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    ChannelTimeout(const CaptureCard &parent, uint value, uint min_val) :
        SpinBoxSetting(this, min_val, 65000, 250),
        CaptureCardDBStorage(this, parent, "channel_timeout")
    {
        setLabel(QObject::tr("Tuning timeout (ms)"));
        setValue(value);
        setHelpText(QObject::tr(
                        "Maximum time (in milliseconds) MythTV waits for "
                        "a channel lock when scanning for channels during setup, or for "
                        "issuing a warning in Live TV mode."));
    };
};

class AudioRateLimit : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    AudioRateLimit(const CaptureCard &parent) :
        ComboBoxSetting(this),
        CaptureCardDBStorage(this, parent, "audioratelimit")
    {
        setLabel(QObject::tr("Force audio sampling rate"));
        setHelpText(
            QObject::tr("If non-zero, override the audio sampling "
                        "rate in the recording profile when this card is "
                        "used.  Use this if your capture card does not "
                        "support all of the standard rates."));
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

class DVBCardNum : public PathSetting, public CaptureCardDBStorage
{
  public:
    DVBCardNum(const CaptureCard &parent) :
        PathSetting(this, true),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setLabel(QObject::tr("DVB device"));
        setHelpText(
            QObject::tr("When you change this setting, the text below "
                        "should change to the name and type of your card. "
                        "If the card cannot be opened, an error message "
                        "will be displayed."));
        fillSelections(QString::null);
    };

    /// \brief Adds all available cards to list
    /// If current is >= 0 it will be considered available even
    /// if no device exists for it in /dev/dvb/adapter*
    void fillSelections(const QString &current)
    {
        clearSelections();

        // Get devices from filesystem
        QStringList sdevs = CardUtil::ProbeVideoDevices("DVB");

        // Add current if needed
        if (!current.isEmpty() &&
            (find(sdevs.begin(), sdevs.end(), current) == sdevs.end()))
        {
            stable_sort(sdevs.begin(), sdevs.end());
        }

        QStringList db = CardUtil::GetVideoDevices("DVB");

        QMap<QString,bool> in_use;
        QString sel = current;
        for (uint i = 0; i < (uint)sdevs.size(); i++)
        {
            const QString dev = sdevs[i];
            in_use[sdevs[i]] = find(db.begin(), db.end(), dev) != db.end();
            if (sel.isEmpty() && !in_use[sdevs[i]])
                sel = dev;
        }

        if (sel.isEmpty() && sdevs.size())
            sel = sdevs[0];

        QString usestr = QString(" -- ");
        usestr += QObject::tr("Warning: already in use");

        for (uint i = 0; i < (uint)sdevs.size(); i++)
        {
            const QString dev = sdevs[i];
            QString desc = dev + (in_use[sdevs[i]] ? usestr : "");
            desc = (current == sdevs[i]) ? dev : desc;
            addSelection(desc, dev, dev == sel);
        }
    }

    virtual void Load(void)
    {
        clearSelections();
        addSelection(QString::null);

        CaptureCardDBStorage::Load();

        QString dev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, getValue());
        fillSelections(dev);
    }
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
            QObject::tr("If enabled, drop packets from the start of a DVB "
                        "recording until a sequence start header is seen."));
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
            QObject::tr("If enabled, only open the DVB card when required, "
                        "leaving it free for other programs at other times."));
    };
};

class DVBEITScan : public CheckBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBEITScan(const CaptureCard &parent) :
        CheckBoxSetting(this),
        CaptureCardDBStorage(this, parent, "dvb_eitscan")
    {
        setLabel(QObject::tr("Use DVB card for active EIT scan"));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, activate active scanning for "
                        "program data (EIT). When this option is enabled "
                        "the DVB card is constantly in-use."));
    };
};

class DVBTuningDelay : public SpinBoxSetting, public CaptureCardDBStorage
{
  public:
    DVBTuningDelay(const CaptureCard &parent) :
        SpinBoxSetting(this, 0, 2000, 25),
        CaptureCardDBStorage(this, parent, "dvb_tuning_delay")
    {
        setLabel(QObject::tr("DVB tuning delay (ms)"));
        setHelpText(
            QObject::tr("Some Linux DVB drivers, in particular for the "
                        "Hauppauge Nova-T, require that we slow down "
                        "the tuning process by specifying a delay "
                        "(in milliseconds)."));
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
    addSelection(QObject::tr("Motorola Generic"), "MOTO GENERIC");
    addSelection(QObject::tr("SA/Cisco Generic"), "SA GENERIC");
    addSelection("DCH-3200");
    addSelection("DCX-3200");
    addSelection("DCT-3412");
    addSelection("DCT-3416");
    addSelection("DCT-6200");
    addSelection("DCT-6212");
    addSelection("DCT-6216");
    addSelection("QIP-6200");
    addSelection("QIP-7100");
    addSelection("PACE-550");
    addSelection("PACE-779");
    addSelection("SA3250HD");
    addSelection("SA4200HD");
    addSelection("SA4250HDC");
    addSelection("SA8300HD");
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
    name = name.simplified();
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
        addChild(new EmptyAudioDevice(parent));
        addChild(new EmptyVBIDevice(parent));
        addChild(desc);
        addChild(model);

#ifdef USING_LINUX_FIREWIRE
        addChild(new FirewireConnection(parent));
        addChild(new FirewireSpeed(parent));
#endif // USING_LINUX_FIREWIRE

        addChild(new SignalTimeout(parent, 2000, 1000));
        addChild(new ChannelTimeout(parent, 9000, 1750));

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

// -----------------------
// HDHomeRun Configuration
// -----------------------

HDHomeRunIP::HDHomeRunIP()
{
    setLabel(QObject::tr("IP Address"));
    setEnabled(false);
    connect(this, SIGNAL(valueChanged( const QString&)),
            this, SLOT(  UpdateDevices(const QString&)));
    _oldValue="";
};

void HDHomeRunIP::setEnabled(bool e)
{
    TransLineEditSetting::setEnabled(e);
    if (e)
    {
        if (!_oldValue.isEmpty())
            setValue(_oldValue);
        emit NewIP(getValue());
    }
    else
    {
        _oldValue = getValue();
        _oldValue.detach();
    }
}

void HDHomeRunIP::UpdateDevices(const QString &v)
{
   if (isEnabled())
   {
#if 0
       LOG(VB_GENERAL, LOG_DEBUG, QString("Emitting NewIP(%1)").arg(v));
#endif
       emit NewIP(v);
   }
}

HDHomeRunTunerIndex::HDHomeRunTunerIndex()
{
    setLabel(QObject::tr("Tuner"));
    setEnabled(false);
    connect(this, SIGNAL(valueChanged( const QString&)),
            this, SLOT(  UpdateDevices(const QString&)));
    _oldValue = "";
};

void HDHomeRunTunerIndex::setEnabled(bool e)
{
    TransLineEditSetting::setEnabled(e);
    if (e) {
        if (!_oldValue.isEmpty())
            setValue(_oldValue);
        emit NewTuner(getValue());
    }
    else
    {
        _oldValue = getValue();
    }
}

void HDHomeRunTunerIndex::UpdateDevices(const QString &v)
{
   if (isEnabled())
   {
#if 0
       LOG(VB_GENERAL, LOG_DEBUG, QString("Emitting NewTuner(%1)").arg(v));
#endif
       emit NewTuner(v);
   }
}

HDHomeRunDeviceID::HDHomeRunDeviceID(const CaptureCard &parent) :
    LabelSetting(this),
    CaptureCardDBStorage(this, parent, "videodevice"),
    _ip(QString::null),
    _tuner(QString::null),
    _overridedeviceid(QString::null)
{
    setLabel(tr("Device ID"));
    setHelpText(tr("Device ID of HDHomeRun device"));
}

void HDHomeRunDeviceID::SetIP(const QString &ip)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Setting IP to %1").arg(ip));
#endif
    _ip = ip;
    setValue(QString("%1-%2").arg(_ip).arg(_tuner));
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Done Setting IP to %1").arg(ip));
#endif
}

void HDHomeRunDeviceID::SetTuner(const QString &tuner)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Setting Tuner to %1").arg(tuner));
#endif
    _tuner = tuner;
    setValue(QString("%1-%2").arg(_ip).arg(_tuner));
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Done Setting Tuner to %1").arg(tuner));
#endif
}

void HDHomeRunDeviceID::SetOverrideDeviceID(const QString &deviceid)
{
    _overridedeviceid = deviceid;
    setValue(deviceid);
}

void HDHomeRunDeviceID::Load(void)
{
    CaptureCardDBStorage::Load();
    if (!_overridedeviceid.isEmpty())
    {
        setValue(_overridedeviceid);
        _overridedeviceid = QString::null;
    }
}

HDHomeRunDeviceIDList::HDHomeRunDeviceIDList(
    HDHomeRunDeviceID   *deviceid,
    TransLabelSetting   *desc,
    HDHomeRunIP         *cardip,
    HDHomeRunTunerIndex *cardtuner,
    HDHomeRunDeviceList *devicelist) :
    _deviceid(deviceid),
    _desc(desc),
    _cardip(cardip),
    _cardtuner(cardtuner),
    _devicelist(devicelist)
{
    setLabel(QObject::tr("Available devices"));
    setHelpText(
        QObject::tr(
            "Device ID and Tuner Number of available HDHomeRun devices."));

    connect(this, SIGNAL(valueChanged( const QString&)),
            this, SLOT(  UpdateDevices(const QString&)));

    _oldValue = "";
};

/// \brief Adds all available device-tuner combinations to list
/// If current is >= 0 it will be considered available even
/// if no device exists for it on the network
void HDHomeRunDeviceIDList::fillSelections(const QString &cur)
{
    clearSelections();

    vector<QString> devs;
    QMap<QString, bool> in_use;

    QString current = cur;

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Filling List, current = '%1'")
            .arg(current));
#endif

    HDHomeRunDeviceList::iterator it = _devicelist->begin();
    for (; it != _devicelist->end(); ++it)
    {
        if ((*it).discovered)
        {
            devs.push_back(it.key());
            in_use[it.key()] = (*it).inuse;
        }
    }

    QString man_addr = HDHomeRunDeviceIDList::tr("Manually Enter IP Address");
    QString sel = man_addr;
    devs.push_back(sel);

    if (3 == devs.size() && current.startsWith("FFFFFFFF", Qt::CaseInsensitive))
    {
        current = sel = (current.endsWith("0")) ?
            *(devs.begin()) : *(++devs.begin());
    }
    else
    {
        vector<QString>::const_iterator it = devs.begin();
        for (; it != devs.end(); ++it)
            sel = (current == *it) ? *it : sel;
    }

    QString usestr = QString(" -- ");
    usestr += QObject::tr("Warning: already in use");

    for (uint i = 0; i < devs.size(); i++)
    {
        const QString dev = devs[i];
        QString desc = dev + (in_use[devs[i]] ? usestr : "");
        desc = (current == devs[i]) ? dev : desc;
        addSelection(desc, dev, dev == sel);
    }

    if (current != cur)
    {
        _deviceid->SetOverrideDeviceID(current);
    }
    else if (sel == man_addr && !current.isEmpty())
    {
        // Populate the proper values for IP address and tuner
        QStringList selection = current.split("-");

        _cardip->SetOldValue(selection.first());
        _cardtuner->SetOldValue(selection.last());

        _cardip->setValue(selection.first());
        _cardtuner->setValue(selection.last());
    }
}

void HDHomeRunDeviceIDList::Load(void)
{
    clearSelections();

    fillSelections(_deviceid->getValue());
}

void HDHomeRunDeviceIDList::UpdateDevices(const QString &v)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Got signal with %1").arg(v));
#endif
    if (v == HDHomeRunDeviceIDList::tr("Manually Enter IP Address"))
    {
        _cardip->setEnabled(true);
        _cardtuner->setEnabled(true);
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "Done");
#endif
    }
    else if (!v.isEmpty())
    {
        if (_oldValue == HDHomeRunDeviceIDList::tr("Manually Enter IP Address"))
        {
            _cardip->setEnabled(false);
            _cardtuner->setEnabled(false);
        }
        _deviceid->setValue(v);

        // Update _cardip and cardtuner
        _cardip->setValue((*_devicelist)[v].cardip);
        _cardtuner->setValue(QString("%1").arg((*_devicelist)[v].cardtuner));
        _desc->setValue((*_devicelist)[v].desc);
    }
    _oldValue = v;
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
        setHelpText(
            QObject::tr("URL of M3U containing RTSP/RTP/UDP channel URLs."));
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
        addChild(new EmptyAudioDevice(parent));
        addChild(new EmptyVBIDevice(parent));
    };

  private:
    CaptureCard &parent;
};

class ASIDevice : public ComboBoxSetting, public CaptureCardDBStorage
{
  public:
    ASIDevice(const CaptureCard &parent) :
        ComboBoxSetting(this, true),
        CaptureCardDBStorage(this, parent, "videodevice")
    {
        setLabel(QObject::tr("ASI device"));
        fillSelections(QString::null);
    };

    /// \brief Adds all available cards to list
    /// If current is >= 0 it will be considered available even
    /// if no device exists for it in /dev/dvb/adapter*
    void fillSelections(const QString &current)
    {
        clearSelections();

        // Get devices from filesystem
        QStringList sdevs = CardUtil::ProbeVideoDevices("ASI");

        // Add current if needed
        if (!current.isEmpty() &&
            (find(sdevs.begin(), sdevs.end(), current) == sdevs.end()))
        {
            stable_sort(sdevs.begin(), sdevs.end());
        }

        // Get devices from DB
        QStringList db = CardUtil::GetVideoDevices("ASI");

        // Figure out which physical devices are already in use
        // by another card defined in the DB, and select a device
        // for new configs (preferring non-conflicing devices).
        QMap<QString,bool> in_use;
        QString sel = current;
        for (uint i = 0; i < (uint)sdevs.size(); i++)
        {
            const QString dev = sdevs[i];
            in_use[sdevs[i]] = find(db.begin(), db.end(), dev) != db.end();
            if (sel.isEmpty() && !in_use[sdevs[i]])
                sel = dev;
        }

        // Unfortunately all devices are conflicted, select first device.
        if (sel.isEmpty() && sdevs.size())
            sel = sdevs[0];

        QString usestr = QString(" -- ");
        usestr += QObject::tr("Warning: already in use");

        // Add the devices to the UI
        bool found = false;
        for (uint i = 0; i < (uint)sdevs.size(); i++)
        {
            const QString dev = sdevs[i];
            QString desc = dev + (in_use[sdevs[i]] ? usestr : "");
            desc = (current == sdevs[i]) ? dev : desc;
            addSelection(desc, dev, dev == sel);
            found |= (dev == sel);
        }

        // If a configured device isn't on the list, add it with warning
        if (!found && !current.isEmpty())
        {
            QString desc = current + " -- " +
                QObject::tr("Warning: unable to open");
            addSelection(desc, current, true);
        }
    }

    virtual void Load(void)
    {
        clearSelections();
        addSelection(QString::null);
        CaptureCardDBStorage::Load();
        fillSelections(getValue());
    }
};

ASIConfigurationGroup::ASIConfigurationGroup(CaptureCard& a_parent):
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    device(new ASIDevice(parent)),
    cardinfo(new TransLabelSetting()),
    instances(new InstanceCount(parent))
{
    addChild(device);
    addChild(new EmptyAudioDevice(parent));
    addChild(new EmptyVBIDevice(parent));
    addChild(cardinfo);
    addChild(instances);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));
    connect(instances, SIGNAL(valueChanged(int)),
            &parent,   SLOT(  SetInstanceCount(int)));

    probeCard(device->getValue());
};

void ASIConfigurationGroup::probeCard(const QString &device)
{
#ifdef USING_ASI
    if (device.isEmpty())
    {
        cardinfo->setValue("");
        return;
    }

    if (parent.getCardID() && parent.GetRawCardType() != "ASI")
    {
        cardinfo->setValue("");
        return;
    }

    QString error;
    int device_num = CardUtil::GetASIDeviceNumber(device, &error);
    if (device_num < 0)
    {
        cardinfo->setValue(tr("Not a valid DVEO ASI card"));
        LOG(VB_GENERAL, LOG_WARNING,
            "ASIConfigurationGroup::probeCard(), Warning: " + error);
        return;
    }
    cardinfo->setValue(tr("Valid DVEO ASI card"));
#else
    cardinfo->setValue(QString("Not compiled with ASI support"));
#endif
}

ImportConfigurationGroup::ImportConfigurationGroup(CaptureCard& a_parent):
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    info(new TransLabelSetting()), size(new TransLabelSetting())
{
    FileDevice *device = new FileDevice(parent);
    device->setHelpText(tr("A local file used to simulate a recording."
                           " Leave empty to use MythEvents to trigger an"
                           " external program to import recording files."));
    addChild(device);

    addChild(new EmptyAudioDevice(parent));
    addChild(new EmptyVBIDevice(parent));

    info->setLabel(tr("File info"));
    addChild(info);

    size->setLabel(tr("File size"));
    addChild(size);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
};

void ImportConfigurationGroup::probeCard(const QString &device)
{
    QString   ci, cs;
    QFileInfo fileInfo(device);

    // For convenience, ImportRecorder allows both formats:
    if (device.toLower().startsWith("file:"))
        fileInfo.setFile(device.mid(5));

    if (fileInfo.exists())
    {
        if (fileInfo.isReadable() && (fileInfo.isFile()))
        {
            ci = HTTPRequest::TestMimeType(fileInfo.absoluteFilePath());
            cs = tr("%1 MB").arg(fileInfo.size() / 1024 / 1024);
        }
        else
            ci = tr("File not readable");
    }
    else
    {
        ci = tr("File %1 does not exist").arg(device);
    }

    info->setValue(ci);
    size->setValue(cs);
}

class HDHomeRunExtra : public ConfigurationWizard
{
  public:
    HDHomeRunExtra(HDHomeRunConfigurationGroup &parent);
    uint GetInstanceCount(void) const
    {
        return (uint) count->intValue();
    }

  private:
    InstanceCount *count;
};

HDHomeRunExtra::HDHomeRunExtra(HDHomeRunConfigurationGroup &parent) :
    count(new InstanceCount(parent.parent))
{
    VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
    rec->setLabel(QObject::tr("Recorder Options"));
    rec->setUseLabel(false);

    rec->addChild(new SignalTimeout(parent.parent, 1000, 250));
    rec->addChild(new ChannelTimeout(parent.parent, 3000, 1750));
    rec->addChild(count);

    addChild(rec);
}

HDHomeRunConfigurationGroup::HDHomeRunConfigurationGroup
        (CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent)
{
    setUseLabel(false);

    // Fill Device list
    FillDeviceList();

    deviceid     = new HDHomeRunDeviceID(parent);
    desc         = new TransLabelSetting();
    desc->setLabel(tr("Description"));
    cardip       = new HDHomeRunIP();
    cardtuner    = new HDHomeRunTunerIndex();
    deviceidlist = new HDHomeRunDeviceIDList(
        deviceid, desc, cardip, cardtuner, &devicelist);

    addChild(deviceidlist);
    addChild(new EmptyAudioDevice(parent));
    addChild(new EmptyVBIDevice(parent));
    addChild(deviceid);
    addChild(desc);
    addChild(cardip);
    addChild(cardtuner);

    TransButtonSetting *buttonRecOpt = new TransButtonSetting();
    buttonRecOpt->setLabel(tr("Recording Options"));
    addChild(buttonRecOpt);

    connect(buttonRecOpt, SIGNAL(pressed()),
            this,         SLOT(  HDHomeRunExtraPanel()));

    connect(cardip,    SIGNAL(NewIP(const QString&)),
            deviceid,  SLOT(  SetIP(const QString&)));
    connect(cardtuner, SIGNAL(NewTuner(const QString&)),
            deviceid,  SLOT(  SetTuner(const QString&)));
};

void HDHomeRunConfigurationGroup::FillDeviceList(void)
{
    devicelist.clear();

    // Find physical devices first
    // ProbeVideoDevices returns "deviceid ip" pairs
    QStringList devs = CardUtil::ProbeVideoDevices("HDHOMERUN");

    QStringList::const_iterator it;

    for (it = devs.begin(); it != devs.end(); ++it)
    {
        QString dev = *it;
        QStringList devinfo = dev.split(" ");
        QString devid = devinfo.at(0);
        QString devip = devinfo.at(1);
        QString devtuner = devinfo.at(2);

        HDHomeRunDevice tmpdevice;
        tmpdevice.deviceid   = devid;
        tmpdevice.desc       = CardUtil::GetHDHRdesc(devid);
        tmpdevice.cardip     = devip;
        tmpdevice.inuse      = false;
        tmpdevice.discovered = true;
        tmpdevice.cardtuner = devtuner;
        tmpdevice.mythdeviceid =
            tmpdevice.deviceid + "-" + tmpdevice.cardtuner;
        devicelist[tmpdevice.mythdeviceid] = tmpdevice;
    }
    uint found_device_count = devicelist.size();

    // Now find configured devices

    // returns "xxxxxxxx-n" or "ip.ip.ip.ip-n" values
    QStringList db = CardUtil::GetVideoDevices("HDHOMERUN");

    for (it = db.begin(); it != db.end(); ++it)
    {
        QMap<QString, HDHomeRunDevice>::iterator dit;

        dit = devicelist.find(*it);

        if (dit == devicelist.end())
        {
            if ((*it).toUpper() == "FFFFFFFF-0" && 2 == found_device_count)
                dit = devicelist.begin();

            if ((*it).toUpper() == "FFFFFFFF-1" && 2 == found_device_count)
            {
                dit = devicelist.begin();
                ++dit;
            }
        }

        if (dit != devicelist.end())
        {
            (*dit).inuse = true;
            continue;
        }

        HDHomeRunDevice tmpdevice;
        tmpdevice.mythdeviceid = *it;
        tmpdevice.inuse        = true;
        tmpdevice.discovered   = false;

        if (ProbeCard(tmpdevice))
            devicelist[tmpdevice.mythdeviceid] = tmpdevice;
    }

#if 0
    // Debug dump of cards
    QMap<QString, HDHomeRunDevice>::iterator debugit;
    for (debugit = devicelist.begin(); debugit != devicelist.end(); ++debugit)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("%1: %2 %3 %4 %5 %6 %7")
                .arg(debugit.key())
                .arg((*debugit).mythdeviceid)
                .arg((*debugit).deviceid)
                .arg((*debugit).cardip)
                .arg((*debugit).cardtuner)
                .arg((*debugit).inuse)
                .arg((*debugit).discovered));
    }
#endif
}

bool HDHomeRunConfigurationGroup::ProbeCard(HDHomeRunDevice &tmpdevice)
{
#ifdef USING_HDHOMERUN
    hdhomerun_device_t *thisdevice =
        hdhomerun_device_create_from_str(
            tmpdevice.mythdeviceid.toLocal8Bit().constData(), NULL);

    if (thisdevice)
    {
        uint device_id = hdhomerun_device_get_device_id(thisdevice);
        uint device_ip = hdhomerun_device_get_device_ip(thisdevice);
        uint tuner     = hdhomerun_device_get_tuner(thisdevice);
        hdhomerun_device_destroy(thisdevice);

        if (device_id == 0)
            tmpdevice.deviceid = "NOTFOUND";
        else
        {
            tmpdevice.deviceid = QString("%1").arg(device_id, 8, 16);
            tmpdevice.desc     = CardUtil::GetHDHRdesc(tmpdevice.deviceid);
        }

        tmpdevice.deviceid = tmpdevice.deviceid.toUpper();

        tmpdevice.cardip = QString("%1.%2.%3.%4")
            .arg((device_ip>>24) & 0xFF).arg((device_ip>>16) & 0xFF)
            .arg((device_ip>> 8) & 0xFF).arg((device_ip>> 0) & 0xFF);

        tmpdevice.cardtuner = QString("%1").arg(tuner);
        return true;
    }
#endif // USING_HDHOMERUN
    return false;
}

void HDHomeRunConfigurationGroup::HDHomeRunExtraPanel(void)
{
    parent.reload(); // ensure card id is valid

    HDHomeRunExtra acw(*this);
    acw.exec();
    parent.SetInstanceCount(acw.GetInstanceCount());
}

// -----------------------
// Ceton Configuration
// -----------------------

CetonSetting::CetonSetting(const char* label, const char* helptext)
{
    setLabel(QObject::tr(label));
    setHelpText(tr(helptext));
    connect(this, SIGNAL(valueChanged( const QString&)),
            this, SLOT(  UpdateDevices(const QString&)));
}

void CetonSetting::UpdateDevices(const QString &v)
{
    if (isEnabled())
        emit NewValue(v);
}

void CetonSetting::LoadValue(const QString &value)
{
    setValue(value);
}

CetonDeviceID::CetonDeviceID(const CaptureCard &parent) :
    LabelSetting(this),
    CaptureCardDBStorage(this, parent, "videodevice"),
    _ip(), _card(), _tuner()
{
    setLabel(tr("Device ID"));
    setHelpText(tr("Device ID of Ceton device"));
}

void CetonDeviceID::SetIP(const QString &ip)
{
    QString regexp = "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){4}$";
    if (QRegExp(regexp).exactMatch(ip + "."))
    {
        _ip = ip;
        setValue(QString("%1-RTP.%3").arg(_ip).arg(_tuner));
    }
}

void CetonDeviceID::SetTuner(const QString &tuner)
{
    if (QRegExp("^\\d$").exactMatch(tuner))
    {
        _tuner = tuner;
        setValue(QString("%1-RTP.%2").arg(_ip).arg(_tuner));
    }
}

void CetonDeviceID::Load(void)
{
    CaptureCardDBStorage::Load();
    UpdateValues();
}

void CetonDeviceID::UpdateValues(void)
{
    QRegExp newstyle("^([0-9.]+)-(\\d|RTP)\\.(\\d)$");
    if (newstyle.exactMatch(getValue()))
    {
        emit LoadedIP(newstyle.cap(1));
        emit LoadedTuner(newstyle.cap(3));
    }
}

CetonConfigurationGroup::CetonConfigurationGroup(CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent)
{
    setUseLabel(false);

    deviceid     = new CetonDeviceID(parent);
    desc         = new TransLabelSetting();
    desc->setLabel(tr("Description"));
    ip    = new CetonSetting(
        "IP Address",
        "IP Address of the Ceton device (192.168.200.1 by default)");
    tuner = new CetonSetting(
        "Tuner",
        "Number of the tuner on the Ceton device (first tuner is number 0)");

    addChild(ip);
    addChild(tuner);
    addChild(deviceid);
    addChild(desc);

    connect(ip,       SIGNAL(NewValue(const QString&)),
            deviceid, SLOT(  SetIP(const QString&)));
    connect(tuner,    SIGNAL(NewValue(const QString&)),
            deviceid, SLOT(  SetTuner(const QString&)));

    connect(deviceid, SIGNAL(LoadedIP(const QString&)),
            ip,       SLOT(  LoadValue(const QString&)));
    connect(deviceid, SIGNAL(LoadedTuner(const QString&)),
            tuner,    SLOT(  LoadValue(const QString&)));

};

V4LConfigurationGroup::V4LConfigurationGroup(CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    cardinfo(new TransLabelSetting()),  vbidev(new VBIDevice(parent))
{
    QString drv = "(?!ivtv|hdpvr|(saa7164(.*)))";
    VideoDevice *device = new VideoDevice(parent, 0, 15, QString::null, drv);
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

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
};

void V4LConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn = QString::null;

    QByteArray adevice = device.toLatin1();
    int videofd = open(adevice.constData(), O_RDWR);
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
}


MPEGConfigurationGroup::MPEGConfigurationGroup(CaptureCard &a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    device(NULL), vbidevice(NULL),
    cardinfo(new TransLabelSetting())
{
    QString drv = "ivtv|(saa7164(.*))";
    device    = new VideoDevice(parent, 0, 15, QString::null, drv);
    vbidevice = new VBIDevice(parent);
    vbidevice->setVisible(false);

    cardinfo->setLabel(tr("Probed info"));

    addChild(device);
    addChild(vbidevice);
    addChild(cardinfo);
    addChild(new ChannelTimeout(parent, 12000, 2000));

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void MPEGConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn = QString::null;

    QByteArray adevice = device.toLatin1();
    int videofd = open(adevice.constData(), O_RDWR);
    if (videofd >= 0)
    {
        if (!CardUtil::GetV4LInfo(videofd, cn, dn))
            ci = cn = tr("Failed to probe");
        else if (!dn.isEmpty())
            ci = cn + "  [" + dn + "]";
        close(videofd);
    }

    cardinfo->setValue(ci);
    vbidevice->setVisible(dn!="ivtv");
    vbidevice->setFilter(cn, dn);
}

DemoConfigurationGroup::DemoConfigurationGroup(CaptureCard &a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    info(new TransLabelSetting()), size(new TransLabelSetting())
{
    FileDevice *device = new FileDevice(parent);
    device->setHelpText(tr("A local MPEG file used to simulate a recording."
                           " Must be entered as file:/path/movie.mpg"));
    device->addSelection("file:/");
    addChild(device);

    addChild(new EmptyAudioDevice(parent));
    addChild(new EmptyVBIDevice(parent));

    info->setLabel(tr("File info"));
    addChild(info);

    size->setLabel(tr("File size"));
    addChild(size);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void DemoConfigurationGroup::probeCard(const QString &device)
{
    if (!device.startsWith("file:", Qt::CaseInsensitive))
    {
        info->setValue("");
        size->setValue("");
        return;
    }


    QString   ci, cs;
    QFileInfo fileInfo(device.mid(5));
    if (fileInfo.exists())
    {
        if (fileInfo.isReadable() && (fileInfo.isFile()))
        {
            ci = HTTPRequest::TestMimeType(fileInfo.absoluteFilePath());
            cs = tr("%1 MB").arg(fileInfo.size() / 1024 / 1024);
        }
        else
            ci = tr("File not readable");
    }
    else
    {
        ci = tr("File does not exist");
    }

    info->setValue(ci);
    size->setValue(cs);
}

HDPVRConfigurationGroup::HDPVRConfigurationGroup(CaptureCard &a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent), cardinfo(new TransLabelSetting()),
    audioinput(new TunerCardAudioInput(parent, QString::null, "HDPVR"))
{
    VideoDevice *device =
        new VideoDevice(parent, 0, 15, QString::null, "hdpvr");

    cardinfo->setLabel(tr("Probed info"));

    addChild(device);
    addChild(new EmptyAudioDevice(parent));
    addChild(new EmptyVBIDevice(parent));
    addChild(cardinfo);
    addChild(audioinput);
    addChild(new ChannelTimeout(parent, 12000, 2000));

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void HDPVRConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn = QString::null;

    int videofd = open(device.toLocal8Bit().constData(), O_RDWR);
    if (videofd >= 0)
    {
        if (!CardUtil::GetV4LInfo(videofd, cn, dn))
            ci = cn = tr("Failed to probe");
        else if (!dn.isEmpty())
            ci = cn + "  [" + dn + "]";
        close(videofd);
    }

    cardinfo->setValue(ci);
    audioinput->fillSelections(device);
}

CaptureCardGroup::CaptureCardGroup(CaptureCard &parent) :
    TriggeredConfigurationGroup(true, true, false, false)
{
    setLabel(QObject::tr("Capture Card Setup"));

    CardType* cardtype = new CardType(parent);
    addChild(cardtype);

    setTrigger(cardtype);
    setSaveAll(false);
    
#ifdef USING_DVB
    addTarget("DVB",       new DVBConfigurationGroup(parent));
#endif // USING_DVB

#ifdef USING_V4L2
# ifdef USING_HDPVR
    addTarget("HDPVR",     new HDPVRConfigurationGroup(parent));
# endif // USING_HDPVR
#endif // USING_V4L2

#ifdef USING_HDHOMERUN
    addTarget("HDHOMERUN", new HDHomeRunConfigurationGroup(parent));
#endif // USING_HDHOMERUN

#ifdef USING_FIREWIRE
    addTarget("FIREWIRE",  new FirewireConfigurationGroup(parent));
#endif // USING_FIREWIRE

#ifdef USING_CETON
    addTarget("CETON",     new CetonConfigurationGroup(parent));
#endif // USING_CETON

#ifdef USING_IPTV
    addTarget("FREEBOX",   new IPTVConfigurationGroup(parent));
#endif // USING_IPTV
    
#ifdef USING_V4L2
    addTarget("V4L",       new V4LConfigurationGroup(parent));
# ifdef USING_IVTV
    addTarget("MPEG",      new MPEGConfigurationGroup(parent));
# endif // USING_IVTV
#endif // USING_V4L2
    
#ifdef USING_ASI
    addTarget("ASI",       new ASIConfigurationGroup(parent));
#endif // USING_ASI

    // for testing without any actual tuner hardware:
    addTarget("IMPORT",    new ImportConfigurationGroup(parent));
    addTarget("DEMO",      new DemoConfigurationGroup(parent));
}

void CaptureCardGroup::triggerChanged(const QString& value)
{
    QString own = (value == "MJPEG" || value == "GO7007") ? "V4L" : value;
    TriggeredConfigurationGroup::triggerChanged(own);
}

CaptureCard::CaptureCard(bool use_card_group)
    : id(new ID), instance_count(0)
{
    addChild(id);
    if (use_card_group)
        addChild(new CaptureCardGroup(*this));
    addChild(new Hostname(*this));
}

QString CaptureCard::GetRawCardType(void) const
{
    int cardid = getCardID();
    if (cardid <= 0)
        return QString::null;
    return CardUtil::GetRawCardType(cardid);
}

void CaptureCard::fillSelections(SelectSetting *setting)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT cardid, videodevice, cardtype "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME "
        "ORDER BY cardid";

    query.prepare(qstr);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec())
    {
        MythDB::DBError("CaptureCard::fillSelections", query);
        return;
    }

    QMap<QString, uint> device_refs;
    while (query.next())
    {
        uint    cardid      = query.value(0).toUInt();
        QString videodevice = query.value(1).toString();
        QString cardtype    = query.value(2).toString();

        bool sharable = CardUtil::IsTunerSharingCapable(cardtype.toUpper());

        if (sharable && (1 != ++device_refs[videodevice]))
            continue;

        QString label = CardUtil::GetDeviceLabel(cardtype, videodevice);
        setting->addSelection(label, QString::number(cardid));
    }
}

void CaptureCard::loadByID(int cardid)
{
    id->setValue(cardid);
    Load();

    // Update instance count for cloned cards.
    uint new_cnt = 0;
    if (cardid > 0)
    {
        QString type = CardUtil::GetRawCardType(cardid);
        if (CardUtil::IsTunerSharingCapable(type))
        {
            QString dev = CardUtil::GetVideoDevice(cardid);
            vector<uint> cardids = CardUtil::GetCardIDs(dev, type);
            new_cnt = cardids.size();
        }
    }
    instance_count = new_cnt;
}

void CaptureCard::Save(void)
{
    uint init_cardid = getCardID();

    ////////

    ConfigurationWizard::Save();

    ////////

    uint cardid = getCardID();
    QString type = CardUtil::GetRawCardType(cardid);
    if (!CardUtil::IsTunerSharingCapable(type))
        return;

    QString init_dev = CardUtil::GetVideoDevice(cardid);
    if (init_dev.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Cannot clone card #%1 with empty videodevice")
                .arg(cardid));
        return;
    }
    vector<uint> cardids = CardUtil::GetCardIDs(init_dev, type);

    if (!instance_count)
    {
        instance_count = (init_cardid) ?
            max((size_t)1, cardids.size()) : kDefaultMultirecCount;
    }
    uint cloneCount = instance_count - 1;

    if (!init_cardid)
        init_cardid = cardid;

    // Delete old clone cards as required.
    for (uint i = cardids.size() - 1; (i > cloneCount) && !cardids.empty(); i--)
    {
        CardUtil::DeleteCard(cardids.back());
        cardids.pop_back();
    }

    // Make sure clones & original all share an input group
    if (cloneCount && !CardUtil::CreateInputGroupIfNeeded(cardid))
        return;

    // Clone this config to existing clone cards.
    for (uint i = 0; i < cardids.size(); i++)
    {
        if (cardids[i] != init_cardid)
            CardUtil::CloneCard(init_cardid, cardids[i]);
    }

    // Create new clone cards as required.
    for (uint i = cardids.size(); i < cloneCount + 1; i++)
    {
        CardUtil::CloneCard(init_cardid, 0);
    }
}

void CaptureCard::reload(void)
{
    if (getCardID() == 0)
    {
        Save();
        Load();
    }
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
#ifdef USING_DVB
    setting->addSelection(
        QObject::tr("DVB-T/S/C, ATSC or ISDB-T tuner card"), "DVB");
#endif // USING_DVB

#ifdef USING_V4L2
# ifdef USING_HDPVR
    setting->addSelection(
        QObject::tr("HD-PVR H.264 encoder"), "HDPVR");
# endif // USING_HDPVR
#endif // USING_V4L2

#ifdef USING_HDHOMERUN
    setting->addSelection(
        QObject::tr("HDHomeRun networked tuner"), "HDHOMERUN");
#endif // USING_HDHOMERUN
    
#ifdef USING_FIREWIRE
    setting->addSelection(
        QObject::tr("FireWire cable box"), "FIREWIRE");
#endif // USING_FIREWIRE

#ifdef USING_CETON
    setting->addSelection(
        QObject::tr("Ceton Cablecard tuner"), "CETON");
#endif // USING_CETON

#ifdef USING_IPTV
    setting->addSelection(QObject::tr("IPTV recorder"), "FREEBOX");
#endif // USING_IPTV

#ifdef USING_V4L2
# ifdef USING_IVTV
    setting->addSelection(
        QObject::tr("Analog to MPEG-2 encoder card (PVR-150/250/350, etc)"), "MPEG");
# endif // USING_IVTV
    setting->addSelection(
        QObject::tr("Analog to MJPEG encoder card (Matrox G200, DC10, etc)"), "MJPEG");
    setting->addSelection(
        QObject::tr("Analog to MPEG-4 encoder (Plextor ConvertX USB, etc)"),
        "GO7007");
    setting->addSelection(
        QObject::tr("Analog capture card"), "V4L");
#endif // USING_V4L2

#ifdef USING_ASI
    setting->addSelection(QObject::tr("DVEO ASI recorder"), "ASI");
#endif

    setting->addSelection(QObject::tr("Import test recorder"), "IMPORT");
    setting->addSelection(QObject::tr("Demo test recorder"),   "DEMO");
}

class CardID : public SelectLabelSetting, public CardInputDBStorage
{
  public:
    CardID(const CardInput &parent) :
        SelectLabelSetting(this), CardInputDBStorage(this, parent, "cardid")
    {
        setLabel(QObject::tr("Capture device"));
    };

    virtual void Load(void)
    {
        fillSelections();
        CardInputDBStorage::Load();
    };

    void fillSelections() {
        CaptureCard::fillSelections(this);
    };
};

class InputDisplayName : public LineEditSetting, public CardInputDBStorage
{
  public:
    InputDisplayName(const CardInput &parent) :
        LineEditSetting(this),
        CardInputDBStorage(this, parent, "displayname")
    {
        setLabel(QObject::tr("Display name (optional)"));
        setHelpText(QObject::tr(
                        "This name is displayed on screen when Live TV begins "
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

    virtual void Load(void)
    {
        fillSelections();
        CardInputDBStorage::Load();
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

class InputGroup : public TransComboBoxSetting
{
  public:
    InputGroup(const CardInput &parent, uint group_num) :
        TransComboBoxSetting(false), cardinput(parent),
        groupnum(group_num), groupid(0)
    {
        setLabel(QObject::tr("Input group") +
                 QString(" %1").arg(groupnum + 1));
        setHelpText(QObject::tr(
                        "Leave as 'Generic' unless this input is shared with "
                        "another device. Only one of the inputs in an input "
                        "group will be allowed to record at any given time."));
    }

    virtual void Load(void);

    virtual void Save(void)
    {
        uint inputid     = cardinput.getInputID();
        uint new_groupid = getValue().toUInt();

        if (groupid)
            CardUtil::UnlinkInputGroup(inputid, groupid);

        if (new_groupid)
        {
            if (CardUtil::UnlinkInputGroup(inputid, new_groupid))
                CardUtil::LinkInputGroup(inputid, new_groupid);
        }
    }

    virtual void Save(QString /*destination*/) { Save(); }

  private:
    const CardInput &cardinput;
    uint             groupnum;
    uint             groupid;
};

void InputGroup::Load(void)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("InputGroup::Load() %1 %2")
            .arg(groupnum).arg(cardinput.getInputID()));
#endif

    uint             inputid = cardinput.getInputID();
    QMap<uint, uint> grpcnt;
    vector<QString>  names;
    vector<uint>     grpid;
    vector<uint>     selected_groupids;

    names.push_back(QObject::tr("Generic"));
    grpid.push_back(0);
    grpcnt[0]++;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid, inputgroupid, inputgroupname "
        "FROM inputgroup "
        "ORDER BY inputgroupid, cardinputid, inputgroupname");

    if (!query.exec())
    {
        MythDB::DBError("InputGroup::Load()", query);
    }
    else
    {
        while (query.next())
        {
            uint groupid = query.value(1).toUInt();
            if (inputid && (query.value(0).toUInt() == inputid))
                selected_groupids.push_back(groupid);

            grpcnt[groupid]++;

            if (grpcnt[groupid] == 1)
            {
                names.push_back(query.value(2).toString());
                grpid.push_back(groupid);
            }
        }
    }

    // makes sure we select something
    groupid = 0;
    if (groupnum < selected_groupids.size())
        groupid = selected_groupids[groupnum];

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Group num: %1 id: %2")
            .arg(groupnum).arg(groupid));
    {
        QString msg;
        for (uint i = 0; i < selected_groupids.size(); i++)
            msg += QString("%1 ").arg(selected_groupids[i]);
        LOG(VB_GENERAL, LOG_DEBUG, msg);
    }
#endif

    // add selections to combobox
    clearSelections();
    uint index = 0;
    for (uint i = 0; i < names.size(); i++)
    {
        bool sel = (groupid == grpid[i]);
        index = (sel) ? i : index;

#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("grpid %1, name '%2', i %3, s %4")
                .arg(grpid[i]).arg(names[i]) .arg(index).arg(sel ? "T" : "F"));
#endif

        addSelection(names[i], QString::number(grpid[i]), sel);
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Group index: %1").arg(index));
#endif

    if (!names.empty())
        setValue(index);
}

class QuickTune : public ComboBoxSetting, public CardInputDBStorage
{
  public:
    QuickTune(const CardInput &parent) :
        ComboBoxSetting(this), CardInputDBStorage(this, parent, "quicktune")
    {
        setLabel(QObject::tr("Use quick tuning"));
        addSelection(QObject::tr("Never"),        "0", true);
        addSelection(QObject::tr("Live TV only"), "1", false);
        addSelection(QObject::tr("Always"),       "2", false);
        setHelpText(QObject::tr(
                        "If enabled, MythTV will tune using only the "
                        "MPEG program number. The program numbers "
                        "change more often than DVB or ATSC tuning "
                        "parameters, so this is slightly less reliable. "
                        "This will also inhibit EIT gathering during "
                        "Live TV and recording."));
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
    clearSelections();
    if (sourceid.isEmpty() || !sourceid.toUInt())
        return;

    // Get the existing starting channel
    QString startChan = CardUtil::GetStartingChannel(getInputID());

    ChannelInfoList channels = ChannelUtil::GetAllChannels(sourceid.toUInt());

    if (channels.empty())
    {
        addSelection(tr("Please add channels to this source"),
                     startChan.isEmpty() ? "" : startChan);
        return;
    }

    // If there are channels sort them, then add theme
    // (selecting the old start channel if it is there).
    QString order = gCoreContext->GetSetting("ChannelOrdering", "channum");
    ChannelUtil::SortChannels(channels, order);
    bool has_visible = false;
    for (uint i = 0; i < channels.size() && !has_visible; i++)
        has_visible |= channels[i].visible;

    for (uint i = 0; i < channels.size(); i++)
    {
        const QString channum = channels[i].channum;
        bool sel = channum == startChan;
        if (!has_visible || channels[i].visible || sel)
        {
            addSelection(channum, channum, sel);
        }
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

class ScheduleOrder : public SpinBoxSetting, public CardInputDBStorage
{
  public:
    ScheduleOrder(const CardInput &parent, int _value) :
        SpinBoxSetting(this, 0, 99, 1),
        CardInputDBStorage(this, parent, "schedorder")
    {
        setLabel(QObject::tr("Schedule order"));
        setValue(_value);
        setHelpText(QObject::tr("If priorities and other factors are equal "
                                "the scheduler will choose the available "
                                "input with the lowest, non-zero value.  "
                                "Setting this value to zero will make the "
                                "input unavailable to the scheduler."));
    };
};

class LiveTVOrder : public SpinBoxSetting, public CardInputDBStorage
{
  public:
    LiveTVOrder(const CardInput &parent, int _value) :
        SpinBoxSetting(this, 0, 99, 1),
        CardInputDBStorage(this, parent, "livetvorder")
    {
        setLabel(QObject::tr("Live TV order"));
        setValue(_value);
        setHelpText(QObject::tr("When entering Live TV, the available, local "
                                "input with the lowest, non-zero value will "
                                "be used.  If no local inputs are available, "
                                "the available, remote input with the lowest, "
                                "non-zero value will be used.  "
                                "Setting this value to zero will make the "
                                "input unavailable to live TV."));
    };
};

class DishNetEIT : public CheckBoxSetting, public CardInputDBStorage
{
  public:
    DishNetEIT(const CardInput &parent) :
        CheckBoxSetting(this),
        CardInputDBStorage(this, parent, "dishnet_eit")
    {
        setLabel(QObject::tr("Use DishNet long-term EIT data"));
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
    inputname(new InputName(*this)),
    sourceid(new SourceID(*this)),
    startchan(new StartingChannel(*this)),
    scan(new TransButtonSetting()),
    srcfetch(new TransButtonSetting()),
    externalInputSettings(new DiSEqCDevSettings()),
    inputgrp0(new InputGroup(*this, 0)),
    inputgrp1(new InputGroup(*this, 1))
{
    addChild(id);

    if (CardUtil::IsInNeedOfExternalInputConf(_cardid))
    {
        addChild(new DTVDeviceConfigGroup(*externalInputSettings,
                                          _cardid, isNewInput));
    }

    ConfigurationGroup *basic =
        new VerticalConfigurationGroup(false, false, true, true);

    basic->setLabel(QObject::tr("Connect source to input"));

    basic->addChild(cardid);
    basic->addChild(inputname);
    basic->addChild(new InputDisplayName(*this));
    basic->addChild(sourceid);

    if (!isDTVcard)
    {
        basic->addChild(new ExternalChannelCommand(*this));
        basic->addChild(new PresetTuner(*this));
    }
    else
    {
        ConfigurationGroup *chgroup =
            new HorizontalConfigurationGroup(false, false, true, true);
        chgroup->addChild(new QuickTune(*this));
        if (isDVBcard)
            chgroup->addChild(new DishNetEIT(*this));
        basic->addChild(chgroup);
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
    basic->addChild(sgrp);

    basic->addChild(startchan);

    addChild(basic);

    ConfigurationGroup *interact =
        new VerticalConfigurationGroup(false, false, true, true);

    interact->setLabel(QObject::tr("Interactions between inputs"));
    interact->addChild(new InputPriority(*this));
    interact->addChild(new ScheduleOrder(*this, _cardid));
    interact->addChild(new LiveTVOrder(*this, _cardid));

    TransButtonSetting *ingrpbtn = new TransButtonSetting("newgroup");
    ingrpbtn->setLabel(QObject::tr("Create a New Input Group"));
    ingrpbtn->setHelpText(
        QObject::tr("Input groups are only needed when two or more cards "
                    "share the same resource such as a FireWire card and "
                    "an analog card input controlling the same set top box."));
    interact->addChild(ingrpbtn);
    interact->addChild(inputgrp0);
    interact->addChild(inputgrp1);

    addChild(interact);

    setObjectName("CardInput");
    SetSourceID("-1");

    connect(scan,     SIGNAL(pressed()), SLOT(channelScanner()));
    connect(srcfetch, SIGNAL(pressed()), SLOT(sourceFetch()));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            startchan,SLOT(  SetSourceID (const QString&)));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            this,     SLOT(  SetSourceID (const QString&)));
    connect(ingrpbtn, SIGNAL(pressed(QString)),
            this,     SLOT(  CreateNewInputGroup()));
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
    uint cid = cardid->getValue().toUInt();
    QString raw_card_type = CardUtil::GetRawCardType(cid);
    bool enable = (sourceid.toInt() > 0);
    scan->setEnabled(enable && !raw_card_type.isEmpty() &&
                     !CardUtil::IsUnscanable(raw_card_type));
    srcfetch->setEnabled(enable);
}

QString CardInput::getSourceName(void) const
{
    return sourceid->getSelectionLabel();
}

void CardInput::CreateNewInputGroup(void)
{
    QString new_name = QString::null;
    QString tmp_name = QString::null;

    inputgrp0->Save();
    inputgrp1->Save();

    while (true)
    {
        tmp_name = "";
        bool ok = MythPopupBox::showGetTextPopup(
            GetMythMainWindow(), tr("Create Input Group"),
            tr("Enter new group name"), tmp_name);

        new_name = tmp_name;

        if (!ok)
            return;

        if (new_name.isEmpty())
        {
            MythPopupBox::showOkPopup(
                GetMythMainWindow(), tr("Error"),
                tr("Sorry, this Input Group name cannot be blank."));
            continue;
        }

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT inputgroupname "
            "FROM inputgroup "
            "WHERE inputgroupname = :GROUPNAME");
        query.bindValue(":GROUPNAME", new_name);

        if (!query.exec())
        {
            MythDB::DBError("CreateNewInputGroup 1", query);
            return;
        }

        if (query.next())
        {
            MythPopupBox::showOkPopup(
                GetMythMainWindow(), tr("Error"),
                tr("Sorry, this Input Group name is already in use."));
            continue;
        }

        break;
    }

    uint inputgroupid = CardUtil::CreateInputGroup(new_name);

    inputgrp0->Load();
    inputgrp1->Load();

    if (!inputgrp0->getValue().toUInt())
    {
        inputgrp0->setValue(
            inputgrp0->getValueIndex(QString::number(inputgroupid)));
    }
    else
    {
        inputgrp1->setValue(
            inputgrp1->getValueIndex(QString::number(inputgroupid)));
    }
}

void CardInput::channelScanner(void)
{
    uint srcid = sourceid->getValue().toUInt();
    uint crdid = cardid->getValue().toUInt();
    QString in = inputname->getValue();

#ifdef USING_BACKEND
    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    Save(); // save info for scanner.

    QString cardtype = CardUtil::GetRawCardType(crdid);
    if (CardUtil::IsUnscanable(cardtype))
    {
        LOG(VB_GENERAL, LOG_ERR, 
            QString("Sorry, %1 cards do not yet support scanning.")
                .arg(cardtype));
        return;
    }

    ScanWizard *scanwizard = new ScanWizard(srcid, crdid, in);
    scanwizard->exec(false, true);
    scanwizard->deleteLater();

    if (SourceUtil::GetChannelCount(srcid))
        startchan->SetSourceID(QString::number(srcid));
    if (num_channels_before)
    {
        startchan->Load();
        startchan->Save();
    }
#else
    LOG(VB_GENERAL, LOG_ERR, "You must compile the backend "
                             "to be able to scan for channels");
#endif
}

void CardInput::sourceFetch(void)
{
    uint srcid = sourceid->getValue().toUInt();
    uint crdid = cardid->getValue().toUInt();

    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    if (crdid && srcid)
    {
        Save(); // save info for fetch..

        QString cardtype = CardUtil::GetRawCardType(crdid);

        if (!CardUtil::IsCableCardPresent(crdid, cardtype) &&
            !CardUtil::IsUnscanable(cardtype) &&
            !CardUtil::IsEncoder(cardtype)    &&
            !num_channels_before)
        {
            LOG(VB_GENERAL, LOG_ERR, "Skipping channel fetch, you need to "
                                     "scan for channels first.");
            return;
        }

        SourceUtil::UpdateChannelsFromListings(srcid, cardtype);
    }

    if (SourceUtil::GetChannelCount(srcid))
        startchan->SetSourceID(QString::number(srcid));
    if (num_channels_before)
    {
        startchan->Load();
        startchan->Save();
    }
}

QString CardInputDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString cardinputidTag(":WHERECARDINPUTID");

    QString query("cardinputid = " + cardinputidTag);

    bindings.insert(cardinputidTag, m_parent.getInputID());

    return query;
}

QString CardInputDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString cardinputidTag(":SETCARDINPUTID");
    QString colTag(":SET" + GetColumnName().toUpper());

    QString query("cardinputid = " + cardinputidTag + ", " +
            GetColumnName() + " = " + colTag);

    bindings.insert(cardinputidTag, m_parent.getInputID());
    bindings.insert(colTag, user->GetDBValue());

    return query;
}

void CardInput::loadByID(int inputid)
{
    id->setValue(inputid);
    externalInputSettings->Load(inputid);
    ConfigurationWizard::Load();
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
        Load();
        cardid->setValue(QString::number(_cardid));
        inputname->setValue(_inputname);
    }
}

void CardInput::Save(void)
{

    if (sourceid->getValue() == "0")
    {
        // "None" is represented by the lack of a row
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM cardinput WHERE cardinputid = :INPUTID");
        query.bindValue(":INPUTID", getInputID());
        if (!query.exec())
            MythDB::DBError("CardInput::Save", query);
    }
    else
    {
        ConfigurationWizard::Save();
        externalInputSettings->Store(getInputID());
    }

    // Handle any cloning we may need to do
    uint src_cardid = cardid->getValue().toUInt();
    QString type = CardUtil::GetRawCardType(src_cardid);
    if (CardUtil::IsTunerSharingCapable(type))
    {
        vector<uint> clones = CardUtil::GetCloneCardIDs(src_cardid);
        if (clones.size() && CardUtil::CreateInputGroupIfNeeded(src_cardid))
        {
            for (uint i = 0; i < clones.size(); i++)
                CardUtil::CloneCard(src_cardid, clones[i]);
        }
    }

    // Delete any orphaned inputs
    CardUtil::DeleteOrphanInputs();
    // Delete any unused input groups
    CardUtil::UnlinkInputGroup(0,0);
}

int CardInputDBStorage::getInputID(void) const
{
    return m_parent.getInputID();
}

int CaptureCardDBStorage::getCardID(void) const
{
    return m_parent.getCardID();
}

CaptureCardEditor::CaptureCardEditor() : listbox(new ListBoxSetting(this))
{
    listbox->setLabel(tr("Capture cards"));
    addChild(listbox);
}

DialogCode CaptureCardEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        edit();

    return kDialogCodeRejected;
}

void CaptureCardEditor::Load(void)
{
    listbox->clearSelections();
    listbox->addSelection(QObject::tr("(New capture card)"), "0");
    listbox->addSelection(QObject::tr("(Delete all capture cards on %1)")
                          .arg(gCoreContext->GetHostName()), "-1");
    listbox->addSelection(QObject::tr("(Delete all capture cards)"), "-2");
    CaptureCard::fillSelections(listbox);
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
        DialogCode val = MythPopupBox::Show2ButtonPopup(
            GetMythMainWindow(),
            "",
            tr("Capture Card Menu"),
            tr("Edit..."),
            tr("Delete..."),
            kDialogCodeButton0);

        if (kDialogCodeButton0 == val)
            edit();
        else if (kDialogCodeButton1 == val)
            del();
    }
}

void CaptureCardEditor::edit(void)
{
    const int cardid = listbox->getValue().toInt();
    if (-1 == cardid)
    {
        DialogCode val = MythPopupBox::Show2ButtonPopup(
            GetMythMainWindow(), "",
            tr("Are you sure you want to delete "
               "ALL capture cards on %1?").arg(gCoreContext->GetHostName()),
            tr("Yes, delete capture cards"),
            tr("No, don't"), kDialogCodeButton1);

        if (kDialogCodeButton0 == val)
        {
            MSqlQuery cards(MSqlQuery::InitCon());

            cards.prepare(
                "SELECT cardid "
                "FROM capturecard "
                "WHERE hostname = :HOSTNAME");
            cards.bindValue(":HOSTNAME", gCoreContext->GetHostName());

            if (!cards.exec() || !cards.isActive())
            {
                MythPopupBox::showOkPopup(
                    GetMythMainWindow(),
                    tr("Error getting list of cards for this host"),
                    tr("Unable to delete capturecards for %1")
                    .arg(gCoreContext->GetHostName()));

                MythDB::DBError("Selecting cardids for deletion", cards);
                return;
            }

            while (cards.next())
                CardUtil::DeleteCard(cards.value(0).toUInt());
        }
    }
    else if (-2 == cardid)
    {
        DialogCode val = MythPopupBox::Show2ButtonPopup(
            GetMythMainWindow(), "",
            tr("Are you sure you want to delete "
               "ALL capture cards?"),
            tr("Yes, delete capture cards"),
            tr("No, don't"), kDialogCodeButton1);

        if (kDialogCodeButton0 == val)
        {
            CardUtil::DeleteAllCards();
            Load();
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
    DialogCode val = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(), "",
        tr("Are you sure you want to delete this capture card?"),
        tr("Yes, delete capture card"),
        tr("No, don't"), kDialogCodeButton1);

    if (kDialogCodeButton0 == val)
    {
        CardUtil::DeleteCard(listbox->getValue().toUInt());
        Load();
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

DialogCode VideoSourceEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
        edit();

    return kDialogCodeRejected;
}

void VideoSourceEditor::Load(void)
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
        DialogCode val = MythPopupBox::Show2ButtonPopup(
            GetMythMainWindow(),
            "",
            tr("Video Source Menu"),
            tr("Edit..."),
            tr("Delete..."),
            kDialogCodeButton0);

        if (kDialogCodeButton0 == val)
            edit();
        else if (kDialogCodeButton1 == val)
            del();
    }
}

void VideoSourceEditor::edit(void)
{
    const int sourceid = listbox->getValue().toInt();
    if (-1 == sourceid)
    {
        DialogCode val = MythPopupBox::Show2ButtonPopup(
            GetMythMainWindow(), "",
            tr("Are you sure you want to delete "
               "ALL video sources?"),
            tr("Yes, delete video sources"),
            tr("No, don't"), kDialogCodeButton1);

        if (kDialogCodeButton0 == val)
        {
            SourceUtil::DeleteAllSources();
            Load();
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
    DialogCode val = MythPopupBox::Show2ButtonPopup(
        GetMythMainWindow(), "",
        tr("Are you sure you want to delete "
           "this video source?"),
        tr("Yes, delete video source"),
        tr("No, don't"),
        kDialogCodeButton1);

    if (kDialogCodeButton0 == val)
    {
        SourceUtil::DeleteSource(listbox->getValue().toUInt());
        Load();
    }
}

CardInputEditor::CardInputEditor() : listbox(new ListBoxSetting(this))
{
    listbox->setLabel(tr("Input connections"));
    addChild(listbox);
}

DialogCode CardInputEditor::exec(void)
{
    while (ConfigurationDialog::exec() == kDialogCodeAccepted)
    {
        if (!listbox)
            return kDialogCodeRejected;

        if (cardinputs.empty())
            return kDialogCodeRejected;

        int val = listbox->getValue().toInt();

        if (cardinputs[val])
            cardinputs[val]->exec();
    }

    return kDialogCodeRejected;
}

void CardInputEditor::Load(void)
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
        "WHERE hostname = :HOSTNAME "
        "ORDER BY cardid");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec())
    {
        MythDB::DBError("CardInputEditor::load", query);
        return;
    }

    uint j = 0;
    QMap<QString, uint> device_refs;
    while (query.next())
    {
        uint    cardid      = query.value(0).toUInt();
        QString videodevice = query.value(1).toString();
        QString cardtype    = query.value(2).toString();

        bool sharable = CardUtil::IsTunerSharingCapable(cardtype.toUpper());

        if (sharable && (1 != ++device_refs[videodevice]))
            continue;

        QStringList        inputLabels;
        vector<CardInput*> cardInputs;

        CardUtil::GetCardInputs(cardid, videodevice, cardtype,
                                inputLabels, cardInputs);

        for (int i = 0; i < inputLabels.size(); i++, j++)
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
    if (short_name.startsWith("LG Electronics"))
        short_name = short_name.right(short_name.length() - 15);
    if (short_name.startsWith("Oren"))
        short_name = short_name.right(short_name.length() - 5);
    if (short_name.startsWith("Nextwave"))
        short_name = short_name.right(short_name.length() - 9);
    if (short_name.startsWith("frontend", Qt::CaseInsensitive))
        short_name = short_name.left(short_name.length() - 9);
    if (short_name.endsWith("VSB/QAM"))
        short_name = short_name.left(short_name.length() - 8);
    if (short_name.endsWith("VSB"))
        short_name = short_name.left(short_name.length() - 4);
    if (short_name.endsWith("DVB-T"))
        short_name = short_name.left(short_name.length() - 6);

    // It would be infinitely better if DVB allowed us to query
    // the vendor ID. But instead we have to guess based on the
    // demodulator name. This means cards like the Air2PC HD5000
    // and DViCO Fusion HDTV cards are not identified correctly.
    short_name = short_name.simplified();
    if (short_name.startsWith("or51211", Qt::CaseInsensitive))
        short_name = "pcHDTV HD-2000";
    else if (short_name.startsWith("or51132", Qt::CaseInsensitive))
        short_name = "pcHDTV HD-3000";
    else if (short_name.startsWith("bcm3510", Qt::CaseInsensitive))
        short_name = "Air2PC v1";
    else if (short_name.startsWith("nxt2002", Qt::CaseInsensitive))
        short_name = "Air2PC v2";
    else if (short_name.startsWith("nxt200x", Qt::CaseInsensitive))
        short_name = "Air2PC v2";
    else if (short_name.startsWith("lgdt3302", Qt::CaseInsensitive))
        short_name = "DViCO HDTV3";
    else if (short_name.startsWith("lgdt3303", Qt::CaseInsensitive))
        short_name = "DViCO v2 or Air2PC v3 or pcHDTV HD-5500";

    return short_name;
}
#endif // USING_DVB

void DVBConfigurationGroup::probeCard(const QString &videodevice)
{
    if (videodevice.isEmpty())
    {
        cardname->setValue("");
        cardtype->setValue("");
        return;
    }

    if (parent.getCardID() && parent.GetRawCardType() != "DVB")
    {
        cardname->setValue("");
        cardtype->setValue("");
        return;
    }

#ifdef USING_DVB
    QString frontend_name = CardUtil::ProbeDVBFrontendName(videodevice);
    QString subtype = CardUtil::ProbeDVBType(videodevice);

    QString err_open  = tr("Could not open card %1").arg(videodevice);
    QString err_other = tr("Could not get card info for card %1").arg(videodevice);

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
            signal_timeout->setValue(7000);
            channel_timeout->setValue(10000);
            break;
        case CardUtil::DVBS2:
            cardtype->setValue("DVB-S2");
            cardname->setValue(frontend_name);
            signal_timeout->setValue(7000);
            channel_timeout->setValue(10000);
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
            signal_timeout->setValue(1000);
            channel_timeout->setValue(3000);
            if (frontend_name.toLower().indexOf("usb") >= 0)
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

#if 0 // frontends on hybrid DVB-T/Analog cards
            QString short_name = remove_chaff(frontend_name);
            buttonAnalog->setVisible(
                short_name.startsWith("zarlink zl10353",
                                      Qt::CaseInsensitive) ||
                short_name.startsWith("wintv hvr 900 m/r: 65008/a1c0",
                                      Qt::CaseInsensitive) ||
                short_name.startsWith("philips tda10046h",
                                      Qt::CaseInsensitive));
#endif
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
            if (frontend_name == "Nextwave NXT200X VSB/QAM frontend")
            {
                signal_timeout->setValue(3000);
                channel_timeout->setValue(5500);
            }

#if 0 // frontends on hybrid DVB-T/Analog cards
            if (frontend_name.toLower().indexOf("usb") < 0)
            {
                buttonAnalog->setVisible(
                    short_name.startsWith("pchdtv", Qt::CaseInsensitive) ||
                    short_name.startsWith("dvico", Qt::CaseInsensitive) ||
                    short_name.startsWith("nextwave", Qt::CaseInsensitive));
            }
#endif
        }
        break;
        default:
            break;
    }
#else
    cardtype->setValue(QString("Recompile with DVB-Support!"));
#endif
}

TunerCardAudioInput::TunerCardAudioInput(const CaptureCard &parent,
                                         QString dev, QString type) :
    ComboBoxSetting(this), CaptureCardDBStorage(this, parent, "audiodevice"),
    last_device(dev), last_cardtype(type)
{
    setLabel(QObject::tr("Audio input"));
    int cardid = parent.getCardID();
    if (cardid <= 0)
        return;

    last_cardtype = CardUtil::GetRawCardType(cardid);
    last_device   = CardUtil::GetAudioDevice(cardid);
}

void TunerCardAudioInput::fillSelections(const QString &device)
{
    clearSelections();

    if (device.isEmpty())
        return;

    last_device = device;
    QStringList inputs =
        CardUtil::ProbeAudioInputs(device, last_cardtype);

    for (uint i = 0; i < (uint)inputs.size(); i++)
    {
        addSelection(inputs[i], QString::number(i),
                     last_device == QString::number(i));
    }
}

class DVBExtra : public ConfigurationWizard
{
  public:
    DVBExtra(DVBConfigurationGroup &parent);
    uint GetInstanceCount(void) const
    {
        return (uint) count->intValue();
    }

  private:
    InstanceCount *count;
};

DVBExtra::DVBExtra(DVBConfigurationGroup &parent)
    : count(new InstanceCount(parent.parent))
{
    VerticalConfigurationGroup* rec = new VerticalConfigurationGroup(false);
    rec->setLabel(QObject::tr("Recorder Options"));
    rec->setUseLabel(false);

    rec->addChild(count);
    rec->addChild(new DVBNoSeqStart(parent.parent));
    rec->addChild(new DVBOnDemand(parent.parent));
    rec->addChild(new DVBEITScan(parent.parent));
    rec->addChild(new DVBTuningDelay(parent.parent));

    addChild(rec);
}

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent) :
    VerticalConfigurationGroup(false, true, false, false),
    parent(a_parent),
    diseqc_tree(new DiSEqCDevTree())
{
    cardnum  = new DVBCardNum(parent);
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

    addChild(new EmptyAudioDevice(parent));
    addChild(new EmptyVBIDevice(parent));

    TransButtonSetting *buttonRecOpt = new TransButtonSetting();
    buttonRecOpt->setLabel(tr("Recording Options"));

    HorizontalConfigurationGroup *advcfg =
        new HorizontalConfigurationGroup(false, false, true, true);
    advcfg->addChild(buttonRecOpt);
    addChild(advcfg);

    diseqc_btn = new TransButtonSetting();
    diseqc_btn->setLabel(tr("DiSEqC (Switch, LNB, and Rotor Configuration)"));
    diseqc_btn->setHelpText(tr("Input and satellite settings."));

    HorizontalConfigurationGroup *diseqc_cfg =
        new HorizontalConfigurationGroup(false, false, true, true);
    diseqc_cfg->addChild(diseqc_btn);
    diseqc_btn->setVisible(false);
    addChild(diseqc_cfg);

    tuning_delay = new DVBTuningDelay(parent);
    addChild(tuning_delay);
    tuning_delay->setVisible(false);

    connect(cardnum,      SIGNAL(valueChanged(const QString&)),
            this,         SLOT(  probeCard   (const QString&)));
    connect(diseqc_btn,   SIGNAL(pressed()),
            this,         SLOT(  DiSEqCPanel()));
    connect(buttonRecOpt, SIGNAL(pressed()),
            this,         SLOT(  DVBExtraPanel()));
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
}

void DVBConfigurationGroup::Load(void)
{
    VerticalConfigurationGroup::Load();
    diseqc_tree->Load(parent.getCardID());
    if (cardtype->getValue() == "DVB-S" ||
        cardtype->getValue() == "DVB-S2" ||
        DiSEqCDevTree::Exists(parent.getCardID()))
    {
        diseqc_btn->setVisible(true);
    }
}

void DVBConfigurationGroup::Save(void)
{
    VerticalConfigurationGroup::Save();
    diseqc_tree->Store(parent.getCardID());
    DiSEqCDev trees;
    trees.InvalidateTrees();
}

void DVBConfigurationGroup::DVBExtraPanel(void)
{
    parent.reload(); // ensure card id is valid

    DVBExtra acw(*this);
    acw.exec();
    parent.SetInstanceCount(acw.GetInstanceCount());
}
