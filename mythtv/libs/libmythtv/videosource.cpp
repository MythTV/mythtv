// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__OpenBSD__) || defined(_WIN32)
#include <sys/types.h>
#else
#include <sys/sysmacros.h>
#endif
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
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "v4l2util.h"
#include "mythnotification.h"
#include "mythterminal.h"

#ifdef USING_DVB
#include "dvbtypes.h"
#endif

#ifdef USING_VBOX
#include "vboxutils.h"
#endif

#ifdef USING_HDHOMERUN
#include "hdhomerun.h"
#endif

static const uint kDefaultMultirecCount = 2;

VideoSourceSelector::VideoSourceSelector(uint           _initial_sourceid,
                                         const QString &_card_types,
                                         bool           _must_have_mplexid) :
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
        "FROM capturecard, videosource";

    querystr += (must_have_mplexid) ? ", channel " : " ";

    querystr +=
        "WHERE capturecard.sourceid = videosource.sourceid AND "
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

    TransMythUIComboBoxSetting::Load();
}

class InstanceCount : public MythUISpinBoxSetting
{
  public:
    InstanceCount(const CardInput &parent, int _initValue) :
        MythUISpinBoxSetting(new CardInputDBStorage(this, parent, "reclimit"),
                             1, 10, 1)
    {
        setLabel(QObject::tr("Max recordings"));
        setValue(_initValue);
        setHelpText(
            QObject::tr(
                "Maximum number of simultaneous recordings MythTV will "
                "attempt using this device.  If set to a value other than "
                "1, MythTV can sometimes record multiple programs on "
                "the same multiplex or overlapping copies of the same "
                "program on a single channel."
                ));
    };
};

class SchedGroup : public MythUICheckBoxSetting
{
  public:
    explicit SchedGroup(const CardInput &parent) :
        MythUICheckBoxSetting(new CardInputDBStorage(this, parent, "schedgroup"))
    {
        setLabel(QObject::tr("Schedule as group"));
        setValue(false);
        setHelpText(
            QObject::tr(
                "Schedule all virtual inputs on this device as a group.  "
                "This is more efficient than scheduling each input "
                "individually.  Additional, virtual inputs will be "
                "automatically added as needed to fulfill the recording "
                "load."
                ));
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

class XMLTVGrabber : public MythUIComboBoxSetting
{
  public:
    explicit XMLTVGrabber(const VideoSource &parent) :
        MythUIComboBoxSetting(new VideoSourceDBStorage(this, parent,
                                                       "xmltvgrabber")),
        m_parent(parent)
    {
        setLabel(QObject::tr("Listings grabber"));
    };

    void Load(void)
    {
        addTargetedChild("schedulesdirect1",
                         new DataDirect_config(m_parent, DD_SCHEDULES_DIRECT,
                                               this));
        addTargetedChild("eitonly",   new EITOnly_config(m_parent, this));
        addTargetedChild("/bin/true", new NoGrabber_config(m_parent));

        addSelection(
            QObject::tr("North America (SchedulesDirect.org) (Internal)"),
            "schedulesdirect1");

        addSelection(
            QObject::tr("Transmitted guide only (EIT)"), "eitonly");

        addSelection(QObject::tr("No grabber"), "/bin/true");

        QString gname, d1, d2, d3;
        SourceUtil::GetListingsLoginData(m_parent.getSourceID(), gname, d1, d2, d3);

#ifdef _MSC_VER
#pragma message( "tv_find_grabbers is not supported yet on windows." )
        //-=>TODO:Screen doesn't show up if the call to MythSysemLegacy is executed
#else

        QString loc = "XMLTVGrabber::Load: ";
        QString loc_err = "XMLTVGrabber::Load, Error: ";

        QStringList name_list;
        QStringList prog_list;

        QStringList args;
        args += "baseline";

        MythSystemLegacy find_grabber_proc("tv_find_grabbers", args,
                                            kMSStdOut | kMSRunShell);
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

        MythUIComboBoxSetting::Load();
#endif
    }

    void Save(void)
    {
        MythUIComboBoxSetting::Save();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "UPDATE videosource "
            "SET userid=NULL, password=NULL "
            "WHERE xmltvgrabber NOT IN ( 'datadirect', 'technovera', "
            "                            'schedulesdirect1' )");
        if (!query.exec())
            MythDB::DBError("XMLTVGrabber::Save", query);
    }

    void LoadXMLTVGrabbers(QStringList name_list, QStringList prog_list)
    {
        if (name_list.size() != prog_list.size())
            return;

        QString selValue = getValue();
        int     selIndex = getValueIndex(selValue);
        setValue(0);

        for (uint i = 0; i < (uint) name_list.size(); i++)
        {
            addTargetedChild(prog_list[i],
                             new XMLTV_generic_config(m_parent, prog_list[i],
                                                      this));
            addSelection(name_list[i], prog_list[i]);
        }

        if (!selValue.isEmpty())
            selIndex = getValueIndex(selValue);
        if (selIndex >= 0)
            setValue(selIndex);
    }
private:
    const VideoSource &m_parent;
};

class CaptureCardSpinBoxSetting : public MythUISpinBoxSetting
{
  public:
    CaptureCardSpinBoxSetting(const CaptureCard &parent,
                              uint min_val, uint max_val, uint step,
                              const QString &setting) :
        MythUISpinBoxSetting(new CaptureCardDBStorage(this, parent, setting),
                             min_val, max_val, step)
    {
    }
};

class CaptureCardTextEditSetting : public MythUITextEditSetting
{
  public:
    CaptureCardTextEditSetting(const CaptureCard &parent,
                               const QString &setting) :
        MythUITextEditSetting(new CaptureCardDBStorage(this, parent, setting))
    {
    }
};

class DVBNetID : public MythUISpinBoxSetting
{
  public:
    DVBNetID(const VideoSource &parent, signed int value, signed int min_val) :
        MythUISpinBoxSetting(new VideoSourceDBStorage(this, parent, "dvb_nit_id"),
                             min_val, 0xffff, 1)
    {
       setLabel(QObject::tr("Network ID"));
       //: Network_ID is the name of an identifier in the DVB's Service
       //: Information standard specification.
       setHelpText(QObject::tr("If your provider has asked you to configure a "
                               "specific network identifier (Network_ID), "
                               "enter it here. Leave it at -1 otherwise."));
       setValue(value);
    };
};

FreqTableSelector::FreqTableSelector(const VideoSource &parent) :
    MythUIComboBoxSetting(new VideoSourceDBStorage(this, parent, "freqtable"))
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
    sourceid(_sourceid)
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

    loaded_freq_table.clear();

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

class UseEIT : public MythUICheckBoxSetting
{
  public:
    explicit UseEIT(const VideoSource &parent) :
        MythUICheckBoxSetting(new VideoSourceDBStorage(this, parent, "useeit"))
    {
        setLabel(QObject::tr("Perform EIT scan"));
        setHelpText(QObject::tr(
                        "If enabled, program guide data for channels on this "
                        "source will be updated with data provided by the "
                        "channels themselves 'Over-the-Air'."));
    }
};

class DataDirectUserID : public MythUITextEditSetting
{
  public:
    explicit DataDirectUserID(const VideoSource &parent) :
        MythUITextEditSetting(new VideoSourceDBStorage(this, parent, "userid"))
    {
        setLabel(QObject::tr("User ID"));
    }
};

class DataDirectPassword : public MythUITextEditSetting
{
  public:
    explicit DataDirectPassword(const VideoSource &parent) :
        MythUITextEditSetting(new VideoSourceDBStorage(this, parent, "password"))
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

    GetNotificationCenter()->Queue(MythBusyNotification(waitMsg,
                                                        tr("DataDirect")));
    clearSelections();

    if (!ddp.GrabLineupsOnly())
    {
        MythErrorNotification en(tr("Fetching of lineups failed"),
                                 tr("DataDirect"));
        GetNotificationCenter()->Queue(en);

        LOG(VB_GENERAL, LOG_ERR,
            "DDLS: fillSelections did not successfully load selections");
        return;
    }
    const DDLineupList lineups = ddp.GetLineups();

    DDLineupList::const_iterator it;
    for (it = lineups.begin(); it != lineups.end(); ++it)
        addSelection((*it).displayname, (*it).lineupid);

    MythCheckNotification n(tr("Fetching of lineups complete"),
                            tr("DataDirect"));
    GetNotificationCenter()->Queue(n);
#else // USING_BACKEND
    LOG(VB_GENERAL, LOG_ERR,
        "You must compile the backend to set up a DataDirect line-up");
#endif // USING_BACKEND
}

void DataDirect_config::Load()
{
    GroupSetting::Load();
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

DataDirect_config::DataDirect_config(const VideoSource& _parent, int _source, StandardSetting *_setting) :
    parent(_parent)
{
    setVisible(false);

    source = _source;

    _setting->addTargetedChild("schedulesdirect1", userid   = new DataDirectUserID(parent));

    _setting->addTargetedChild("schedulesdirect1", password = new DataDirectPassword(parent));
    _setting->addTargetedChild("schedulesdirect1", button   = new DataDirectButton());

    _setting->addTargetedChild("schedulesdirect1", lineupselector = new DataDirectLineupSelector(parent));
    _setting->addTargetedChild("schedulesdirect1", new UseEIT(parent));

    connect(button, SIGNAL(clicked()),
            this,   SLOT(fillDataDirectLineupSelector()));
}

void DataDirect_config::fillDataDirectLineupSelector(void)
{
    lineupselector->fillSelections(
        userid->getValue(), password->getValue(), source);
}

XMLTV_generic_config::XMLTV_generic_config(const VideoSource& _parent,
                                           QString _grabber,
                                           StandardSetting *_setting) :
    parent(_parent), grabber(_grabber)
{
    setVisible(false);

    QString filename = QString("%1/%2.xmltv")
        .arg(GetConfDir()).arg(parent.getSourceName());

    grabberArgs.push_back("--config-file");
    grabberArgs.push_back(filename);
    grabberArgs.push_back("--configure");

    _setting->addTargetedChild(_grabber, new UseEIT(parent));

    ButtonStandardSetting *config = new ButtonStandardSetting(tr("Configure"));
    config->setHelpText(tr("Run XMLTV configure command."));

    _setting->addTargetedChild(_grabber, config);

    connect(config, SIGNAL(clicked()), SLOT(RunConfig()));
}

void XMLTV_generic_config::Save()
{
    GroupSetting::Save();
#if 0
    QString err_msg = QObject::tr(
        "You MUST run 'mythfilldatabase --manual' the first time,\n"
        "instead of just 'mythfilldatabase'.\nYour grabber does not provide "
        "channel numbers, so you have to set them manually.");

    if (is_grabber_external(grabber))
    {
        LOG(VB_GENERAL, LOG_ERR, err_msg);
        ShowOkPopup(err_msg);
    }
#endif
}

void XMLTV_generic_config::RunConfig(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    MythScreenType *ssd =
        new MythTerminal(mainStack, grabber, grabberArgs);

    if (ssd->Create())
        mainStack->AddScreen(ssd);
    else
        delete ssd;
}

EITOnly_config::EITOnly_config(const VideoSource& _parent, StandardSetting *_setting)
{
    setVisible(false);

    useeit = new UseEIT(_parent);
    useeit->setValue(true);
    useeit->setVisible(false);
    addChild(useeit);

    TransTextEditSetting *label;
    label=new TransTextEditSetting();
    label->setValue(QObject::tr("Use only the transmitted guide data."));
    label->setHelpText(
        QObject::tr("This will usually only work with ATSC or DVB channels, "
                    "and generally provides data only for the next few days."));
    _setting->addTargetedChild("eitonly", label);
}

void EITOnly_config::Save(void)
{
    // Force this value on
    useeit->setValue(true);
    useeit->Save();
}

NoGrabber_config::NoGrabber_config(const VideoSource& _parent)
{
    useeit = new UseEIT(_parent);
    useeit->setValue(false);
    useeit->setVisible(false);
    addChild(useeit);

    TransTextEditSetting *label = new TransTextEditSetting();
    label->setValue(QObject::tr("Do not configure a grabber"));
    addTargetedChild("/bin/true", label);
}

void NoGrabber_config::Save(void)
{
    useeit->setValue(false);
    useeit->Save();
}

VideoSource::VideoSource()
{
    // must be first
    id = new ID();
    addChild(id = new ID());

    setLabel(QObject::tr("Video Source Setup"));
    addChild(name = new Name(*this));
    addChild(new XMLTVGrabber(*this));
    addChild(new FreqTableSelector(*this));
    addChild(new DVBNetID(*this, -1, -1));
}

bool VideoSource::canDelete(void)
{
    return true;
}

void VideoSource::deleteEntry(void)
{
    SourceUtil::DeleteSource(getSourceID());
}

bool VideoSourceEditor::cardTypesInclude(const int &sourceID,
                                         const QString &thecardtype)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(cardtype)"
                  " FROM capturecard "
                  " WHERE capturecard.sourceid = :SOURCEID "
                  " AND capturecard.cardtype = :CARDTYPE ;");
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

void VideoSource::fillSelections(GroupSetting* setting)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT name, sourceid FROM videosource;");

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next())
        {
            VideoSource* source = new VideoSource();
            source->setLabel(result.value(0).toString());
            source->loadByID(result.value(1).toInt());
            setting->addChild(source);
        }
    }
}

void VideoSource::fillSelections(MythUIComboBoxSetting* setting)
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

class VideoDevice : public CaptureCardComboBoxSetting
{
  public:
    VideoDevice(const CaptureCard &parent,
                uint    minor_min = 0,
                uint    minor_max = UINT_MAX,
                QString card      = QString(),
                QString driver    = QString()) :
        CaptureCardComboBoxSetting(parent, true, "videodevice")
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

    /**
     *  \param dir      The directory to open and search for devices.
     *  \param absPath  Ignored. The function always uses absolute paths.
     */
    void fillSelectionsFromDir(const QDir &dir, bool absPath = true)
    {
        // Needed to make both compiler and doxygen happy.
        (void) absPath;

        fillSelectionsFromDir(dir, 0, 255, QString(), QString(), false);
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
                QString card_name, driver_name;
                if (CardUtil::GetV4LInfo(videofd, card_name, driver_name) &&
                    (!driverExp     || (driverExp->exactMatch(driver_name)))  &&
                    (card.isEmpty() || (card_name == card)))
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

    QString Driver(void) const { return driver_name; }
    QString Card(void) const { return card_name; }

  private:
    QMap<uint, uint> minor_list;
    QString card_name;
    QString driver_name;
};

class VBIDevice : public CaptureCardComboBoxSetting
{
  public:
    explicit VBIDevice(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, true /*, mustexist true */,
                                   "vbidevice")
    {
        setLabel(QObject::tr("VBI device"));
        setFilter(QString(), QString());
        setHelpText(QObject::tr("Device to read VBI (captions) from."));
    };

    uint setFilter(const QString &card, const QString &driver)
    {
        uint count = 0;
        clearSelections();
        QDir dev("/dev/v4l", "vbi*", QDir::Name, QDir::System);
        if (!(count = fillSelectionsFromDir(dev, card, driver)))
        {
            dev.setPath("/dev");
            if (!(count = fillSelectionsFromDir(dev, card, driver)) &&
                !getValue().isEmpty())
            {
                addSelection(getValue(),getValue(),true);
            }
        }

        return count;
    }

    /**
     *  \param dir      The directory to open and search for devices.
     *  \param absPath  Ignored. The function always uses absolute paths.
     */
    void fillSelectionsFromDir(const QDir &dir, bool absPath = true)
    {
        // Needed to make both compiler and doxygen happy.
        (void) absPath;

        fillSelectionsFromDir(dir, QString(), QString());
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

class CommandPath : public MythUITextEditSetting
{
  public:
    explicit CommandPath(const CaptureCard &parent) :
        MythUITextEditSetting(new CaptureCardDBStorage(this, parent,
                                                       "videodevice"))
    {
        setLabel(QObject::tr(""));
        setValue("");
        setHelpText(QObject::tr("Specify the command to run, with any "
                                "needed arguments."));
    };
};

class FileDevice : public MythUIFileBrowserSetting
{
  public:
    explicit FileDevice(const CaptureCard &parent) :
        MythUIFileBrowserSetting(
            new CaptureCardDBStorage(this, parent, "videodevice")
            /* mustexist, false */)
    {
        setLabel(QObject::tr("File path"));
    };
};

class AudioDevice : public CaptureCardComboBoxSetting
{
  public:
    explicit AudioDevice(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, true /* mustexist false */,
                                   "audiodevice")
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
        setHelpText(QObject::tr("Device to read audio from, "
                                "if audio is separate from the video."));
    };
};

class SignalTimeout : public CaptureCardSpinBoxSetting
{
  public:
    SignalTimeout(const CaptureCard &parent, uint value, uint min_val) :
        CaptureCardSpinBoxSetting(parent, min_val, 60000, 250, "signal_timeout")
    {
        setLabel(QObject::tr("Signal timeout (ms)"));
        setValue(QString::number(value));
        setHelpText(QObject::tr(
                        "Maximum time (in milliseconds) MythTV waits for "
                        "a signal when scanning for channels."));
    };
};

class ChannelTimeout : public CaptureCardSpinBoxSetting
{
  public:
    ChannelTimeout(const CaptureCard &parent, uint value, uint min_val) :
        CaptureCardSpinBoxSetting(parent, min_val, 65000, 250,
                                  "channel_timeout")
    {
        setLabel(QObject::tr("Tuning timeout (ms)"));
        setValue(value);
        setHelpText(QObject::tr(
                        "Maximum time (in milliseconds) MythTV waits for "
                        "a channel lock.  For recordings, if this time is "
                        "exceeded, the recording will be marked as failed."));
    };
};

class AudioRateLimit : public CaptureCardComboBoxSetting
{
  public:
    explicit AudioRateLimit(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, false, "audioratelimit")
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

class SkipBtAudio : public MythUICheckBoxSetting
{
  public:
    explicit SkipBtAudio(const CaptureCard &parent) :
        MythUICheckBoxSetting(new CaptureCardDBStorage(this, parent,
                                                       "skipbtaudio"))
    {
        setLabel(QObject::tr("Do not adjust volume"));
        setHelpText(
            QObject::tr("Enable this option for budget BT878 based "
                        "DVB-T cards such as the AverTV DVB-T which "
                        "require the audio volume to be left alone."));
   };
};

class DVBCardNum : public CaptureCardComboBoxSetting
{
  public:
    explicit DVBCardNum(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, true, "videodevice")
    {
        setLabel(QObject::tr("DVB device"));
        setHelpText(
            QObject::tr("When you change this setting, the text below "
                        "should change to the name and type of your card. "
                        "If the card cannot be opened, an error message "
                        "will be displayed."));
        fillSelections(QString());
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
        addSelection(QString());

        StandardSetting::Load();

        QString dev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, getValue());
        fillSelections(dev);
    }
};

class DVBCardType : public GroupSetting
{
  public:
    DVBCardType()
    {
        setLabel(QObject::tr("Subtype"));
        setEnabled(false);
    };
};

class DVBCardName : public GroupSetting
{
  public:
    DVBCardName()
    {
        setLabel(QObject::tr("Frontend ID"));
        setEnabled(false);
    };
};

class DVBNoSeqStart : public MythUICheckBoxSetting
{
  public:
    explicit DVBNoSeqStart(const CaptureCard &parent) :
        MythUICheckBoxSetting(
            new CaptureCardDBStorage(this, parent, "dvb_wait_for_seqstart"))
    {
        setLabel(QObject::tr("Wait for SEQ start header."));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, drop packets from the start of a DVB "
                        "recording until a sequence start header is seen."));
    };
};

class DVBOnDemand : public MythUICheckBoxSetting
{
  public:
    explicit DVBOnDemand(const CaptureCard &parent) :
        MythUICheckBoxSetting(
            new CaptureCardDBStorage(this, parent, "dvb_on_demand"))
    {
        setLabel(QObject::tr("Open DVB card on demand"));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, only open the DVB card when required, "
                        "leaving it free for other programs at other times."));
    };
};

class DVBEITScan : public MythUICheckBoxSetting
{
  public:
    explicit DVBEITScan(const CaptureCard &parent) :
        MythUICheckBoxSetting(
            new CaptureCardDBStorage(this, parent, "dvb_eitscan"))
    {
        setLabel(QObject::tr("Use DVB card for active EIT scan"));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, activate active scanning for "
                        "program data (EIT). When this option is enabled "
                        "the DVB card is constantly in-use."));
    };
};

class DVBTuningDelay : public CaptureCardSpinBoxSetting
{
  public:
    explicit DVBTuningDelay(const CaptureCard &parent) :
        CaptureCardSpinBoxSetting(parent, 0, 2000, 25, "dvb_tuning_delay")
    {
        setValue("0");
        setLabel(QObject::tr("DVB tuning delay (ms)"));
        setValue(true);
        setHelpText(
            QObject::tr("Some Linux DVB drivers, in particular for the "
                        "Hauppauge Nova-T, require that we slow down "
                        "the tuning process by specifying a delay "
                        "(in milliseconds)."));
    };
};

class FirewireGUID : public CaptureCardComboBoxSetting
{
  public:
    explicit FirewireGUID(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, false, "videodevice")
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
    CaptureCardComboBoxSetting(parent, false, "firewire_model"),
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

class FirewireConnection : public MythUIComboBoxSetting
{
  public:
    explicit FirewireConnection(const CaptureCard &parent) :
        MythUIComboBoxSetting(new CaptureCardDBStorage(this, parent,
                                                       "firewire_connection"))
    {
        setLabel(QObject::tr("Connection Type"));
        addSelection(QObject::tr("Point to Point"),"0");
        addSelection(QObject::tr("Broadcast"),"1");
    }
};

class FirewireSpeed : public MythUIComboBoxSetting
{
  public:
    explicit FirewireSpeed(const CaptureCard &parent) :
        MythUIComboBoxSetting(new CaptureCardDBStorage(this, parent,
                                                       "firewire_speed"))
    {
        setLabel(QObject::tr("Speed"));
        addSelection(QObject::tr("100Mbps"),"0");
        addSelection(QObject::tr("200Mbps"),"1");
        addSelection(QObject::tr("400Mbps"),"2");
        addSelection(QObject::tr("800Mbps"),"3");
    }
};

#ifdef USING_FIREWIRE
static void FirewireConfigurationGroup(CaptureCard& parent, CardType& cardtype)
{
    FirewireGUID  *dev(new FirewireGUID(parent));
    FirewireDesc  *desc(new FirewireDesc(dev));
    FirewireModel *model(new FirewireModel(parent, dev));
    cardtype.addTargetedChild("FIREWIRE", dev);
    cardtype.addTargetedChild("FIREWIRE", new EmptyAudioDevice(parent));
    cardtype.addTargetedChild("FIREWIRE", new EmptyVBIDevice(parent));
    cardtype.addTargetedChild("FIREWIRE", desc);
    cardtype.addTargetedChild("FIREWIRE", model);

#ifdef USING_LINUX_FIREWIRE
    cardtype.addTargetedChild("FIREWIRE", new FirewireConnection(parent));
    cardtype.addTargetedChild("FIREWIRE", new FirewireSpeed(parent));
#endif // USING_LINUX_FIREWIRE

    cardtype.addTargetedChild("FIREWIRE", new SignalTimeout(parent, 2000, 1000));
    cardtype.addTargetedChild("FIREWIRE", new ChannelTimeout(parent, 9000, 1750));

    model->SetGUID(dev->getValue());
    desc->SetGUID(dev->getValue());
    QObject::connect(dev,   SIGNAL(valueChanged(const QString&)),
                     model, SLOT(  SetGUID(     const QString&)));
    QObject::connect(dev,   SIGNAL(valueChanged(const QString&)),
                     desc,  SLOT(  SetGUID(     const QString&)));
}
#endif

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
    MythUITextEditSetting::setEnabled(e);
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
    MythUITextEditSetting::setEnabled(e);
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
    MythUITextEditSetting(new CaptureCardDBStorage(this, parent, "videodevice"))
{
    setLabel(tr("Device ID"));
    setHelpText(tr("Device ID of HDHomeRun device"));
    setEnabled(false);
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
    GetStorage()->Load();
    if (!_overridedeviceid.isEmpty())
    {
        setValue(_overridedeviceid);
        _overridedeviceid.clear();
    }
}

HDHomeRunDeviceIDList::HDHomeRunDeviceIDList(
    HDHomeRunDeviceID   *deviceid,
    StandardSetting     *desc,
    HDHomeRunIP         *cardip,
    HDHomeRunTunerIndex *cardtuner,
    HDHomeRunDeviceList *devicelist,
    const CaptureCard &parent) :
    _deviceid(deviceid),
    _desc(desc),
    _cardip(cardip),
    _cardtuner(cardtuner),
    _devicelist(devicelist),
    m_parent(parent)
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

    int cardid = m_parent.getCardID();
    QString device = CardUtil::GetVideoDevice(cardid);
    fillSelections(device);
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

// -----------------------
// VBOX Configuration
// -----------------------

VBoxIP::VBoxIP()
{
    setLabel(QObject::tr("IP Address"));
    setHelpText(QObject::tr("Device IP or ID of a VBox device. eg. '192.168.1.100' or 'vbox_3718'"));
    setEnabled(false);
    connect(this, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateDevices(const QString&)));
    _oldValue="";
};

void VBoxIP::setEnabled(bool e)
{
    MythUITextEditSetting::setEnabled(e);
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

void VBoxIP::UpdateDevices(const QString &v)
{
   if (isEnabled())
       emit NewIP(v);
}

VBoxTunerIndex::VBoxTunerIndex()
{
    setLabel(QObject::tr("Tuner"));
    setHelpText(QObject::tr("Number and type of the tuner to use. eg '1-DVBT/T2'."));
    setEnabled(false);
    connect(this, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateDevices(const QString&)));
    _oldValue = "";
};

void VBoxTunerIndex::setEnabled(bool e)
{
    MythUITextEditSetting::setEnabled(e);
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

void VBoxTunerIndex::UpdateDevices(const QString &v)
{
   if (isEnabled())
       emit NewTuner(v);
}

VBoxDeviceID::VBoxDeviceID(const CaptureCard &parent) :
    MythUITextEditSetting(new CaptureCardDBStorage(this, parent, "videodevice"))
{
    setLabel(tr("Device ID"));
    setHelpText(tr("Device ID of VBox device"));
    setEnabled(false);
}

void VBoxDeviceID::SetIP(const QString &ip)
{
    _ip = ip;
    setValue(QString("%1-%2").arg(_ip).arg(_tuner));
}

void VBoxDeviceID::SetTuner(const QString &tuner)
{
    _tuner = tuner;
    setValue(QString("%1-%2").arg(_ip).arg(_tuner));
}

void VBoxDeviceID::SetOverrideDeviceID(const QString &deviceid)
{
    _overridedeviceid = deviceid;
    setValue(deviceid);
}

void VBoxDeviceID::Load(void)
{
    GetStorage()->Load();
    if (!_overridedeviceid.isEmpty())
    {
        setValue(_overridedeviceid);
        _overridedeviceid.clear();
    }
}

VBoxDeviceIDList::VBoxDeviceIDList(
    VBoxDeviceID        *deviceid,
    StandardSetting     *desc,
    VBoxIP              *cardip,
    VBoxTunerIndex      *cardtuner,
    VBoxDeviceList      *devicelist,
    const CaptureCard &parent) :
    _deviceid(deviceid),
    _desc(desc),
    _cardip(cardip),
    _cardtuner(cardtuner),
    _devicelist(devicelist),
    m_parent(parent)
{
    setLabel(QObject::tr("Available devices"));
    setHelpText(
        QObject::tr(
            "Device IP or ID, tuner number and tuner type of available VBox devices."));

    connect(this, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateDevices(const QString&)));

    _oldValue = "";
};

/// \brief Adds all available device-tuner combinations to list
void VBoxDeviceIDList::fillSelections(const QString &cur)
{
    clearSelections();

    vector<QString> devs;
    QMap<QString, bool> in_use;

    QString current = cur;

    VBoxDeviceList::iterator it = _devicelist->begin();
    for (; it != _devicelist->end(); ++it)
    {
        devs.push_back(it.key());
        in_use[it.key()] = (*it).inuse;
    }

    QString man_addr = VBoxDeviceIDList::tr("Manually Enter IP Address");
    QString sel = man_addr;
    devs.push_back(sel);

    vector<QString>::const_iterator it2 = devs.begin();
    for (; it2 != devs.end(); ++it2)
        sel = (current == *it2) ? *it2 : sel;

    QString usestr = QString(" -- ");
    usestr += QObject::tr("Warning: already in use");

    for (uint i = 0; i < devs.size(); i++)
    {
        const QString dev = devs[i];
        QString desc = dev + (in_use[devs[i]] ? usestr : "");
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

void VBoxDeviceIDList::Load(void)
{
    clearSelections();

    int cardid = m_parent.getCardID();
    QString device = CardUtil::GetVideoDevice(cardid);
    fillSelections(device);
}

void VBoxDeviceIDList::UpdateDevices(const QString &v)
{
    if (v == VBoxDeviceIDList::tr("Manually Enter IP Address"))
    {
        _cardip->setEnabled(true);
        _cardtuner->setEnabled(true);
    }
    else if (!v.isEmpty())
    {
        if (_oldValue == VBoxDeviceIDList::tr("Manually Enter IP Address"))
        {
            _cardip->setEnabled(false);
            _cardtuner->setEnabled(false);
        }
        _deviceid->setValue(v);

        // Update _cardip and _cardtuner
        _cardip->setValue((*_devicelist)[v].cardip);
        _cardtuner->setValue(QString("%1").arg((*_devicelist)[v].tunerno));
        _desc->setValue((*_devicelist)[v].desc);
    }
    _oldValue = v;
};

// -----------------------
// IPTV Configuration
// -----------------------

class IPTVHost : public CaptureCardTextEditSetting
{
  public:
    explicit IPTVHost(const CaptureCard &parent) :
        CaptureCardTextEditSetting(parent, "videodevice")
    {
        setValue("http://mafreebox.freebox.fr/freeboxtv/playlist.m3u");
        setLabel(QObject::tr("M3U URL"));
        setHelpText(
            QObject::tr("URL of M3U containing RTSP/RTP/UDP channel URLs."));
    }
};

static void IPTVConfigurationGroup(CaptureCard& parent, CardType& cardType)
{
    cardType.addTargetedChild("FREEBOX", new IPTVHost(parent));
    cardType.addTargetedChild("FREEBOX", new ChannelTimeout(parent, 30000, 1750));
    cardType.addTargetedChild("FREEBOX", new EmptyAudioDevice(parent));
    cardType.addTargetedChild("FREEBOX", new EmptyVBIDevice(parent));
}

class ASIDevice : public CaptureCardComboBoxSetting
{
  public:
    explicit ASIDevice(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, true, "videodevice")
    {
        setLabel(QObject::tr("ASI device"));
        fillSelections(QString());
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
        addSelection(QString());
        GetStorage()->Load();
        fillSelections(getValue());
    }
};

ASIConfigurationGroup::ASIConfigurationGroup(CaptureCard& a_parent,
                                             CardType &cardType):
    parent(a_parent),
    device(new ASIDevice(parent)),
    cardinfo(new TransTextEditSetting())
{
    setVisible(false);
    cardinfo->setEnabled(false);
    cardType.addChild(device);
    cardType.addChild(new EmptyAudioDevice(parent));
    cardType.addChild(new EmptyVBIDevice(parent));
    cardType.addChild(cardinfo);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

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
    Q_UNUSED(device);
    cardinfo->setValue(QString("Not compiled with ASI support"));
#endif
}

ImportConfigurationGroup::ImportConfigurationGroup(CaptureCard& a_parent,
                                                   CardType& a_cardtype):
    parent(a_parent),
    info(new TransTextEditSetting()), size(new TransTextEditSetting())
{
    setVisible(false);
    FileDevice *device = new FileDevice(parent);
    device->setHelpText(tr("A local file used to simulate a recording."
                           " Leave empty to use MythEvents to trigger an"
                           " external program to import recording files."));
    a_cardtype.addTargetedChild("IMPORT", device);

    a_cardtype.addTargetedChild("IMPORT", new EmptyAudioDevice(parent));
    a_cardtype.addTargetedChild("IMPORT", new EmptyVBIDevice(parent));

    info->setLabel(tr("File info"));
    info->setEnabled(false);
    a_cardtype.addTargetedChild("IMPORT", info);

    size->setLabel(tr("File size"));
    size->setEnabled(false);
    a_cardtype.addTargetedChild("IMPORT", size);

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

class HDHomeRunEITScan : public MythUICheckBoxSetting
{
  public:
    explicit HDHomeRunEITScan(const CaptureCard &parent) :
        MythUICheckBoxSetting(
            new CaptureCardDBStorage(this, parent, "dvb_eitscan"))
    {
        setLabel(QObject::tr("Use HD HomeRun for active EIT scan"));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, activate active scanning for "
                        "program data (EIT). When this option is enabled "
                        "the HD HomeRun is constantly in-use."));
    };
};


HDHomeRunConfigurationGroup::HDHomeRunConfigurationGroup
        (CaptureCard& a_parent, CardType &a_cardtype) :
    parent(a_parent)
{
    setVisible(false);

    // Fill Device list
    FillDeviceList();

    deviceid     = new HDHomeRunDeviceID(parent);
    desc         = new GroupSetting();
    desc->setLabel(tr("Description"));
    cardip       = new HDHomeRunIP();
    cardtuner    = new HDHomeRunTunerIndex();
    deviceidlist = new HDHomeRunDeviceIDList(
        deviceid, desc, cardip, cardtuner, &devicelist, parent);

    a_cardtype.addTargetedChild("HDHOMERUN", deviceidlist);
    a_cardtype.addTargetedChild("HDHOMERUN", new EmptyAudioDevice(parent));
    a_cardtype.addTargetedChild("HDHOMERUN", new EmptyVBIDevice(parent));
    a_cardtype.addTargetedChild("HDHOMERUN", deviceid);
    a_cardtype.addTargetedChild("HDHOMERUN", desc);
    a_cardtype.addTargetedChild("HDHOMERUN", cardip);
    a_cardtype.addTargetedChild("HDHOMERUN", cardtuner);

    GroupSetting *buttonRecOpt = new GroupSetting();
    buttonRecOpt->setLabel(tr("Recording Options"));
    buttonRecOpt->addChild(new SignalTimeout(parent, 1000, 250));
    buttonRecOpt->addChild(new ChannelTimeout(parent, 3000, 1750));
    buttonRecOpt->addChild(new HDHomeRunEITScan(parent));
    a_cardtype.addTargetedChild("HDHOMERUN", buttonRecOpt);

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

// -----------------------
// VBox Configuration
// -----------------------

VBoxConfigurationGroup::VBoxConfigurationGroup
        (CaptureCard& a_parent, CardType& a_cardtype) :
    parent(a_parent)
{
    setVisible(false);

    // Fill Device list
    FillDeviceList();

    deviceid     = new VBoxDeviceID(parent);
    desc         = new GroupSetting();
    desc->setLabel(tr("Description"));
    cardip       = new VBoxIP();
    cardtuner    = new VBoxTunerIndex();
    deviceidlist = new VBoxDeviceIDList(
        deviceid, desc, cardip, cardtuner, &devicelist, parent);

    a_cardtype.addTargetedChild("VBOX", deviceidlist);
    a_cardtype.addTargetedChild("VBOX", new EmptyAudioDevice(parent));
    a_cardtype.addTargetedChild("VBOX", new EmptyVBIDevice(parent));
    a_cardtype.addTargetedChild("VBOX", deviceid);
    a_cardtype.addTargetedChild("VBOX", desc);
    a_cardtype.addTargetedChild("VBOX", cardip);
    a_cardtype.addTargetedChild("VBOX", cardtuner);
    a_cardtype.addTargetedChild("VBOX", new SignalTimeout(parent, 7000, 1000));
    a_cardtype.addTargetedChild("VBOX", new ChannelTimeout(parent, 10000, 1750));
//    TransButtonSetting *buttonRecOpt = new TransButtonSetting();
//    buttonRecOpt->setLabel(tr("Recording Options"));
//    addChild(buttonRecOpt);

//    connect(buttonRecOpt, SIGNAL(pressed()),
//            this,         SLOT(  VBoxExtraPanel()));

    connect(cardip,    SIGNAL(NewIP(const QString&)),
            deviceid,  SLOT(  SetIP(const QString&)));
    connect(cardtuner, SIGNAL(NewTuner(const QString&)),
            deviceid,  SLOT(  SetTuner(const QString&)));
};

void VBoxConfigurationGroup::FillDeviceList(void)
{
    devicelist.clear();

    // Find physical devices first
    // ProbeVideoDevices returns "deviceid ip tunerno tunertype"
    QStringList devs = CardUtil::ProbeVideoDevices("VBOX");

    QStringList::const_iterator it;

    for (it = devs.begin(); it != devs.end(); ++it)
    {
        QString dev = *it;
        QStringList devinfo = dev.split(" ");
        QString id = devinfo.at(0);
        QString ip = devinfo.at(1);
        QString tunerNo = devinfo.at(2);
        QString tunerType = devinfo.at(3);

        VBoxDevice tmpdevice;
        tmpdevice.deviceid   = id;
        tmpdevice.desc       = CardUtil::GetVBoxdesc(id, ip, tunerNo, tunerType);
        tmpdevice.cardip     = ip;
        tmpdevice.inuse      = false;
        tmpdevice.discovered = true;
        tmpdevice.tunerno  = tunerNo;
        tmpdevice.tunertype  = tunerType;
        tmpdevice.mythdeviceid = id + "-" + tunerNo + "-" + tunerType;
        devicelist[tmpdevice.mythdeviceid] = tmpdevice;
    }

    // Now find configured devices

    // returns "ip.ip.ip.ip-n-type" or deviceid-n-type values
    QStringList db = CardUtil::GetVideoDevices("VBOX");

    for (it = db.begin(); it != db.end(); ++it)
    {
        QMap<QString, VBoxDevice>::iterator dit;
        dit = devicelist.find(*it);

        if (dit != devicelist.end())
            (*dit).inuse = true;
    }
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
    MythUITextEditSetting(new CaptureCardDBStorage(this, parent, "videodevice")),
    _ip(), _card(), _tuner(), _parent(parent)
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
    GetStorage()->Load();
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

static void CetonConfigurationGroup(CaptureCard& parent, CardType& cardtype)
{
    CetonDeviceID *deviceid = new CetonDeviceID(parent);
    GroupSetting *desc = new GroupSetting();
    desc->setLabel(QCoreApplication::translate("CetonConfigurationGroup",
                                               "Description"));
    CetonSetting *ip = new CetonSetting(
        "IP Address",
        "IP Address of the Ceton device (192.168.200.1 by default)");
    CetonSetting *tuner = new CetonSetting(
        "Tuner",
        "Number of the tuner on the Ceton device (first tuner is number 0)");

    cardtype.addTargetedChild("CETON", ip);
    cardtype.addTargetedChild("CETON", tuner);
    cardtype.addTargetedChild("CETON", deviceid);
    cardtype.addTargetedChild("CETON", desc);
    cardtype.addTargetedChild("CETON", new SignalTimeout(parent, 1000, 250));
    cardtype.addTargetedChild("CETON", new ChannelTimeout(parent, 3000, 1750));

    QObject::connect(ip,       SIGNAL(NewValue(const QString&)),
                     deviceid, SLOT(  SetIP(const QString&)));
    QObject::connect(tuner,    SIGNAL(NewValue(const QString&)),
                     deviceid, SLOT(  SetTuner(const QString&)));

    QObject::connect(deviceid, SIGNAL(LoadedIP(const QString&)),
                     ip,       SLOT(  LoadValue(const QString&)));
    QObject::connect(deviceid, SIGNAL(LoadedTuner(const QString&)),
                     tuner,    SLOT(  LoadValue(const QString&)));
}

V4LConfigurationGroup::V4LConfigurationGroup(CaptureCard& a_parent,
                                             CardType& a_cardtype) :
    parent(a_parent),
    cardinfo(new TransTextEditSetting()),  vbidev(new VBIDevice(parent))
{
    setVisible(false);
    QString drv = "(?!ivtv|hdpvr|(saa7164(.*))).*";
    VideoDevice *device = new VideoDevice(parent, 0, 15, QString(), drv);

    cardinfo->setLabel(tr("Probed info"));
    cardinfo->setEnabled(false);

    a_cardtype.addTargetedChild("V4L", device);
    a_cardtype.addTargetedChild("V4L", cardinfo);
    a_cardtype.addTargetedChild("V4L", vbidev);
    a_cardtype.addTargetedChild("V4L", new AudioDevice(parent));
    a_cardtype.addTargetedChild("V4L", new AudioRateLimit(parent));
    a_cardtype.addTargetedChild("V4L", new SkipBtAudio(parent));

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
};

void V4LConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn;

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

MPEGConfigurationGroup::MPEGConfigurationGroup(CaptureCard &a_parent,
                                               CardType &a_cardtype) :
    parent(a_parent),
    device(NULL), vbidevice(NULL),
    cardinfo(new TransTextEditSetting())
{
    setVisible(false);
    QString drv = "ivtv|(saa7164(.*))";
    device    = new VideoDevice(parent, 0, 15, QString(), drv);
    vbidevice = new VBIDevice(parent);
    vbidevice->setVisible(false);

    cardinfo->setLabel(tr("Probed info"));
    cardinfo->setEnabled(false);

    a_cardtype.addTargetedChild("MPEG", device);
    a_cardtype.addTargetedChild("MPEG", vbidevice);
    a_cardtype.addTargetedChild("MPEG", cardinfo);
    a_cardtype.addTargetedChild("MPEG", new ChannelTimeout(parent, 12000, 2000));

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void MPEGConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn;

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

DemoConfigurationGroup::DemoConfigurationGroup(CaptureCard &a_parent,
                                               CardType &a_cardtype) :
    parent(a_parent),
    info(new TransTextEditSetting()), size(new TransTextEditSetting())
{
    setVisible(false);
    FileDevice *device = new FileDevice(parent);
    device->setHelpText(tr("A local MPEG file used to simulate a recording."));

    a_cardtype.addTargetedChild("DEMO", device);

    a_cardtype.addTargetedChild("DEMO", new EmptyAudioDevice(parent));
    a_cardtype.addTargetedChild("DEMO", new EmptyVBIDevice(parent));

    info->setLabel(tr("File info"));
    info->setEnabled(false);
    a_cardtype.addTargetedChild("DEMO", info);

    size->setLabel(tr("File size"));
    size->setEnabled(false);
    a_cardtype.addTargetedChild("DEMO", size);

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void DemoConfigurationGroup::probeCard(const QString &device)
{
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

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
ExternalConfigurationGroup::ExternalConfigurationGroup(CaptureCard &a_parent,
                                                       CardType &a_cardtype) :
    parent(a_parent),
    info(new TransTextEditSetting())
{
    setVisible(false);
    CommandPath *device = new CommandPath(parent);
    device->setLabel(tr("Command path"));
    device->setHelpText(tr("A 'black box' application controlled via "
                           "stdin, status on stderr and TransportStream "
                           "read from stdout"));
    a_cardtype.addTargetedChild("EXTERNAL", device);

    info->setLabel(tr("File info"));
    info->setEnabled(false);
    a_cardtype.addTargetedChild("EXTERNAL", info);

    a_cardtype.addTargetedChild("EXTERNAL",
                                new ChannelTimeout(parent, 20000, 1750));

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeApp(   const QString&)));

    probeApp(device->getValue());
}

void ExternalConfigurationGroup::probeApp(const QString & path)
{
    int idx1 = path.toLower().startsWith("file:") ? 5 : 0;
    int idx2 = path.indexOf(' ', idx1);

    QString   ci, cs;
    QFileInfo fileInfo(path.mid(idx1, idx2 - idx1));

    if (fileInfo.exists())
    {
        ci = tr("'%1' is valid.").arg(fileInfo.absoluteFilePath());
        if (!fileInfo.isReadable() || !fileInfo.isFile())
            ci = tr("WARNING: '%1' is not readable.")
                 .arg(fileInfo.absoluteFilePath());
        if (!fileInfo.isExecutable())
            ci = tr("WARNING: '%1' is not executable.")
                 .arg(fileInfo.absoluteFilePath());
    }
    else
    {
        ci = tr("WARNING: '%1' does not exist.")
             .arg(fileInfo.absoluteFilePath());
    }

    info->setValue(ci);
}
#endif // !defined( USING_MINGW ) && !defined( _MSC_VER )

HDPVRConfigurationGroup::HDPVRConfigurationGroup(CaptureCard &a_parent,
                                                 CardType &a_cardtype) :
    parent(a_parent), cardinfo(new GroupSetting()),
    audioinput(new TunerCardAudioInput(parent, QString(), "HDPVR")),
    vbidevice(NULL)
{
    setVisible(false);

    VideoDevice *device =
        new VideoDevice(parent, 0, 15, QString(), "hdpvr");

    cardinfo->setLabel(tr("Probed info"));
    cardinfo->setEnabled(false);

    a_cardtype.addTargetedChild("HDPVR", device);
    a_cardtype.addTargetedChild("HDPVR", new EmptyAudioDevice(parent));
    a_cardtype.addTargetedChild("HDPVR", new EmptyVBIDevice(parent));
    a_cardtype.addTargetedChild("HDPVR", cardinfo);
    a_cardtype.addTargetedChild("HDPVR", audioinput);
    a_cardtype.addTargetedChild("HDPVR", new ChannelTimeout(parent, 15000, 2000));

    connect(device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(device->getValue());
}

void HDPVRConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open"), ci = cn, dn;

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

V4L2encGroup::V4L2encGroup(CaptureCard &parent, CardType& cardtype) :
    m_parent(parent),
    m_cardinfo(new TransTextEditSetting())
{
    setLabel(QObject::tr("V4L2 encoder devices (multirec capable)"));
    m_device = new VideoDevice(m_parent, 0, 15);

    cardtype.addTargetedChild("V4L2ENC", m_device);
    m_cardinfo->setLabel(tr("Probed info"));
    cardtype.addTargetedChild("V4L2ENC", m_cardinfo);

    setVisible(false);

    connect(m_device, SIGNAL(valueChanged(const QString&)),
            this,   SLOT(  probeCard(   const QString&)));

    probeCard(m_device->getValue());
}

void V4L2encGroup::probeCard(const QString &device_name)
{
#ifdef USING_V4L2
    QString    card_name = tr("Failed to open");
    QString    card_info = card_name;
    V4L2util   v4l2(device_name);

    if (!v4l2.IsOpen())
    {
        m_DriverName = tr("Failed to probe");
        return;
    }
    m_DriverName = v4l2.DriverName();
    card_name = v4l2.CardName();

    if (!m_DriverName.isEmpty())
        card_info = card_name + "  [" + m_DriverName + "]";

    m_cardinfo->setValue(card_info);

    if (m_device->getSubSettings()->size() == 0)
    {
        TunerCardAudioInput* audioinput =
            new TunerCardAudioInput(m_parent, QString(), "V4L2");
        if (audioinput->fillSelections(device_name) > 1)
        {
            audioinput->setName("AudioInput");
            m_device->addTargetedChild(m_DriverName, audioinput);
        }
        else
            delete audioinput;

        if (v4l2.HasSlicedVBI())
        {
            VBIDevice* vbidev = new VBIDevice(m_parent);
            if (vbidev->setFilter(card_name, m_DriverName) > 0)
            {
                vbidev->setName("VBIDevice");
                m_device->addTargetedChild(m_DriverName, vbidev);
            }
            else
                delete vbidev;
        }

        m_device->addTargetedChild(m_DriverName, new EmptyVBIDevice(m_parent));
        m_device->addTargetedChild(m_DriverName,
                                   new ChannelTimeout(m_parent, 15000, 2000));
    }
#else
    Q_UNUSED(device_name);
#endif // USING_V4L2
}

CaptureCardGroup::CaptureCardGroup(CaptureCard &parent)
{
    setLabel(QObject::tr("Capture Card Setup"));

    CardType* cardtype = new CardType(parent);
    parent.addChild(cardtype);

#ifdef USING_DVB
    cardtype->addTargetedChild("DVB",
                               new DVBConfigurationGroup(parent, *cardtype));
#endif // USING_DVB

#ifdef USING_V4L2
# ifdef USING_HDPVR
    cardtype->addTargetedChild("HDPVR",
                               new HDPVRConfigurationGroup(parent, *cardtype));
# endif // USING_HDPVR
#endif // USING_V4L2

#ifdef USING_HDHOMERUN
    cardtype->addTargetedChild("HDHOMERUN",
                               new HDHomeRunConfigurationGroup(parent, *cardtype));
#endif // USING_HDHOMERUN

#ifdef USING_VBOX
    cardtype->addTargetedChild("VBOX",
                               new VBoxConfigurationGroup(parent, *cardtype));
#endif // USING_VBOX

#ifdef USING_FIREWIRE
    FirewireConfigurationGroup(parent, *cardtype);
#endif // USING_FIREWIRE

#ifdef USING_CETON
    CetonConfigurationGroup(parent, *cardtype);
#endif // USING_CETON

#ifdef USING_IPTV
    IPTVConfigurationGroup(parent, *cardtype);
#endif // USING_IPTV

#ifdef USING_V4L2
    cardtype->addTargetedChild("V4L2ENC", new V4L2encGroup(parent, *cardtype));
    cardtype->addTargetedChild("V4L",
                               new V4LConfigurationGroup(parent, *cardtype));
    cardtype->addTargetedChild("MJPEG",
                               new V4LConfigurationGroup(parent, *cardtype));
    cardtype->addTargetedChild("GO7007",
                               new V4LConfigurationGroup(parent, *cardtype));
# ifdef USING_IVTV
    cardtype->addTargetedChild("MPEG",
                               new MPEGConfigurationGroup(parent, *cardtype));
# endif // USING_IVTV
#endif // USING_V4L2

#ifdef USING_ASI
    cardtype->addTargetedChild("ASI",
                               new ASIConfigurationGroup(parent, *cardtype));
#endif // USING_ASI

    // for testing without any actual tuner hardware:
    cardtype->addTargetedChild("IMPORT",
                               new ImportConfigurationGroup(parent, *cardtype));
    cardtype->addTargetedChild("DEMO",
                               new DemoConfigurationGroup(parent, *cardtype));
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    cardtype->addTargetedChild("EXTERNAL",
                               new ExternalConfigurationGroup(parent,
                                                              *cardtype));
#endif
}

CaptureCard::CaptureCard(bool use_card_group)
    : id(new ID)
{
    addChild(id);
    if (use_card_group)
        CaptureCardGroup(*this);
    addChild(new Hostname(*this));
}

QString CaptureCard::GetRawCardType(void) const
{
    int cardid = getCardID();
    if (cardid <= 0)
        return QString();
    return CardUtil::GetRawInputType(cardid);
}

void CaptureCard::fillSelections(GroupSetting *setting)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT cardid, videodevice, cardtype "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME AND parentid = 0 "
        "ORDER BY cardid";

    query.prepare(qstr);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec())
    {
        MythDB::DBError("CaptureCard::fillSelections", query);
        return;
    }

    CardUtil::ClearVideoDeviceCache();

    while (query.next())
    {
        uint    cardid      = query.value(0).toUInt();
        QString videodevice = query.value(1).toString();
        QString cardtype    = query.value(2).toString();

        QString label = CardUtil::GetDeviceLabel(cardtype, videodevice);
        CaptureCard *card = new CaptureCard();
        card->loadByID(cardid);
        card->setLabel(label);
        setting->addChild(card);
    }
}

void CaptureCard::loadByID(int cardid)
{
    id->setValue(cardid);
    Load();
}

bool CaptureCard::canDelete(void)
{
    return true;
}

void CaptureCard::deleteEntry(void)
{
    CardUtil::DeleteInput(getCardID());
}


void CaptureCard::Save(void)
{
    uint init_cardid = getCardID();
    QString init_type = CardUtil::GetRawInputType(init_cardid);
    QString init_dev = CardUtil::GetVideoDevice(init_cardid);
    QString init_input = CardUtil::GetInputName(init_cardid);

    ////////

    GroupSetting::Save();

    ////////

    uint cardid = getCardID();
    QString type = CardUtil::GetRawInputType(cardid);
    QString dev = CardUtil::GetVideoDevice(cardid);

    if (dev != init_dev)
    {
        if (!init_dev.isEmpty())
        {
            uint init_groupid = CardUtil::GetDeviceInputGroup(init_cardid);
            CardUtil::UnlinkInputGroup(init_cardid, init_groupid);
        }
        if (!dev.isEmpty())
        {
            uint groupid =
                CardUtil::CreateDeviceInputGroup(cardid, type,
                                                 gCoreContext->GetHostName(), dev);
            CardUtil::LinkInputGroup(cardid, groupid);
            CardUtil::UnlinkInputGroup(0, groupid);
        }
    }

    // Handle any cloning we may need to do
    if (CardUtil::IsTunerSharingCapable(type))
    {
        vector<uint> clones = CardUtil::GetChildInputIDs(cardid);
        for (uint i = 0; i < clones.size(); i++)
            CardUtil::CloneCard(cardid, clones[i]);
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
    CaptureCardComboBoxSetting(parent, false, "cardtype")
{
    setLabel(QObject::tr("Card type"));
    setHelpText(QObject::tr("Change the cardtype to the appropriate type for "
                "the capture card you are configuring."));
    fillSelections(this);
}

void CardType::fillSelections(MythUIComboBoxSetting* setting)
{
#ifdef USING_DVB
    setting->addSelection(
        QObject::tr("DVB-T/S/C, ATSC or ISDB-T tuner card"), "DVB");
#endif // USING_DVB

#ifdef USING_V4L2
    setting->addSelection(
        QObject::tr("V4L2 encoder"), "V4L2ENC");
#ifdef USING_HDPVR
    setting->addSelection(
        QObject::tr("HD-PVR H.264 encoder"), "HDPVR");
# endif // USING_HDPVR
#endif // USING_V4L2

#ifdef USING_HDHOMERUN
    setting->addSelection(
        QObject::tr("HDHomeRun networked tuner"), "HDHOMERUN");
#endif // USING_HDHOMERUN

#ifdef USING_VBOX
    setting->addSelection(
        QObject::tr("V@Box TV Gateway networked tuner"), "VBOX");
#endif // USING_VBOX

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
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    setting->addSelection(QObject::tr("External (black box) recorder"),
                          "EXTERNAL");
#endif
}

class InputName : public MythUIComboBoxSetting
{
  public:
    explicit InputName(const CardInput &parent) :
        MythUIComboBoxSetting(new CardInputDBStorage(this, parent, "inputname"))
    {
        setLabel(QObject::tr("Input name"));
    };

    virtual void Load(void)
    {
        fillSelections();
        MythUIComboBoxSetting::Load();
    };

    void fillSelections() {
        clearSelections();
        addSelection(QObject::tr("(None)"), "None");
        uint cardid = static_cast<CardInputDBStorage*>(GetStorage())->getInputID();
        QString type = CardUtil::GetRawInputType(cardid);
        QString device = CardUtil::GetVideoDevice(cardid);
        QStringList inputs;
        CardUtil::GetDeviceInputNames(device, type, inputs);
        while (!inputs.isEmpty())
        {
            addSelection(inputs.front());
            inputs.pop_front();
        }
    };
};

class InputDisplayName : public MythUITextEditSetting
{
  public:
    explicit InputDisplayName(const CardInput &parent) :
        MythUITextEditSetting(new CardInputDBStorage(this, parent, "displayname"))
    {
        setLabel(QObject::tr("Display name (optional)"));
        setHelpText(QObject::tr(
                        "This name is displayed on screen when Live TV begins "
                        "and when changing the selected input or card. If you "
                        "use this, make sure the information is unique for "
                        "each input."));
    };
};

class CardInputComboBoxSetting : public MythUIComboBoxSetting
{
  public:
    CardInputComboBoxSetting(const CardInput &parent, const QString &setting) :
        MythUIComboBoxSetting(new CardInputDBStorage(this, parent, setting))
    {
    }
};

class SourceID : public CardInputComboBoxSetting
{
  public:
    explicit SourceID(const CardInput &parent) :
        CardInputComboBoxSetting(parent, "sourceid")
    {
        setLabel(QObject::tr("Video source"));
        addSelection(QObject::tr("(None)"), "0");
    };

    virtual void Load(void)
    {
        fillSelections();
        CardInputComboBoxSetting::Load();
    };

    void fillSelections() {
        clearSelections();
        addSelection(QObject::tr("(None)"), "0");
        VideoSource::fillSelections(this);
    };
};

class InputGroup : public TransMythUIComboBoxSetting
{
  public:
    InputGroup(const CardInput &parent, uint group_num) :
        TransMythUIComboBoxSetting(), cardinput(parent),
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
        "WHERE inputgroupname LIKE 'user:%' "
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
                names.push_back(query.value(2).toString().mid(5, -1));
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

    TransMythUIComboBoxSetting::Load();
}

class QuickTune : public CardInputComboBoxSetting
{
  public:
    explicit QuickTune(const CardInput &parent) :
        CardInputComboBoxSetting(parent, "quicktune")
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

class ExternalChannelCommand : public MythUITextEditSetting
{
  public:
    explicit ExternalChannelCommand(const CardInput &parent) :
        MythUITextEditSetting(new CardInputDBStorage(this, parent, "externalcommand"))
    {
        setLabel(QObject::tr("External channel change command"));
        setValue("");
        setHelpText(QObject::tr("If specified, this command will be run to "
                    "change the channel for inputs which have an external "
                    "tuner device such as a cable box. The first argument "
                    "will be the channel number."));
    };
};

class PresetTuner : public MythUITextEditSetting
{
  public:
    explicit PresetTuner(const CardInput &parent) :
        MythUITextEditSetting(new CardInputDBStorage(this, parent, "tunechan"))
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
    int inputId = static_cast<CardInputDBStorage*>(GetStorage())->getInputID();
    QString startChan = CardUtil::GetStartingChannel(inputId);

    ChannelInfoList channels = ChannelUtil::GetAllChannels(sourceid.toUInt());

    if (channels.empty())
    {
        addSelection(tr("Please add channels to this source"),
                     startChan.isEmpty() ? "0" : startChan);
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

class InputPriority : public MythUISpinBoxSetting
{
  public:
    explicit InputPriority(const CardInput &parent) :
        MythUISpinBoxSetting(new CardInputDBStorage(this, parent, "recpriority"),
                             -99, 99, 1)
    {
        setLabel(QObject::tr("Input priority"));
        setValue(0);
        setHelpText(QObject::tr("If the input priority is not equal for "
                    "all inputs, the scheduler may choose to record a show "
                    "at a later time so that it can record on an input with "
                    "a higher value."));
    };
};

class ScheduleOrder : public MythUISpinBoxSetting
{
  public:
    ScheduleOrder(const CardInput &parent, int _value) :
        MythUISpinBoxSetting(new CardInputDBStorage(this, parent, "schedorder"),
                             0, 99, 1)
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

class LiveTVOrder : public MythUISpinBoxSetting
{
  public:
    LiveTVOrder(const CardInput &parent, int _value) :
        MythUISpinBoxSetting(new CardInputDBStorage(this, parent, "livetvorder"),
                             0, 99, 1)
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

class DishNetEIT : public MythUICheckBoxSetting
{
  public:
    explicit DishNetEIT(const CardInput &parent) :
        MythUICheckBoxSetting(new CardInputDBStorage(this, parent,
                                                     "dishnet_eit"))
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

CardInput::CardInput(const QString & cardtype, const QString & device,
                     int _cardid) :
    id(new ID()),
    inputname(new InputName(*this)),
    sourceid(new SourceID(*this)),
    startchan(new StartingChannel(*this)),
    scan(new ButtonStandardSetting(tr("Scan for channels"))),
    srcfetch(new ButtonStandardSetting(tr("Fetch channels from listings source"))),
    externalInputSettings(new DiSEqCDevSettings()),
    inputgrp0(new InputGroup(*this, 0)),
    inputgrp1(new InputGroup(*this, 1)),
    instancecount(NULL),
    schedgroup(NULL)
{
    addChild(id);

    if (CardUtil::IsInNeedOfExternalInputConf(_cardid))
    {
        addChild(new DTVDeviceConfigGroup(*externalInputSettings,
                                          _cardid, true));
    }

    addChild(inputname);
    addChild(new InputDisplayName(*this));
    addChild(sourceid);

    if (CardUtil::IsEncoder(cardtype) || CardUtil::IsUnscanable(cardtype))
    {
        addChild(new ExternalChannelCommand(*this));
        if (CardUtil::HasTuner(cardtype, device))
            addChild(new PresetTuner(*this));
    }
    else
    {
        addChild(new QuickTune(*this));
        if ("DVB" == cardtype)
            addChild(new DishNetEIT(*this));
    }

    scan->setHelpText(
        tr("Use channel scanner to find channels for this input."));

    srcfetch->setHelpText(
        tr("This uses the listings data source to "
           "provide the channels for this input.") + " " +
        tr("This can take a long time to run."));

    addChild(scan);
    addChild(srcfetch);

    addChild(startchan);

    GroupSetting *interact = new GroupSetting();

    interact->setLabel(QObject::tr("Interactions between inputs"));
    if (CardUtil::IsTunerSharingCapable(cardtype))
    {
        instancecount = new InstanceCount(*this, kDefaultMultirecCount);
        interact->addChild(instancecount);
        schedgroup = new SchedGroup(*this);
        interact->addChild(schedgroup);
    }
    interact->addChild(new InputPriority(*this));
    interact->addChild(new ScheduleOrder(*this, _cardid));
    interact->addChild(new LiveTVOrder(*this, _cardid));

    ButtonStandardSetting *ingrpbtn =
        new ButtonStandardSetting(QObject::tr("Create a New Input Group"));
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

    connect(scan,     SIGNAL(clicked()), SLOT(channelScanner()));
    connect(srcfetch, SIGNAL(clicked()), SLOT(sourceFetch()));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            startchan,SLOT(  SetSourceID (const QString&)));
    connect(sourceid, SIGNAL(valueChanged(const QString&)),
            this,     SLOT(  SetSourceID (const QString&)));
    connect(ingrpbtn, SIGNAL(clicked()),
            this,     SLOT(  CreateNewInputGroup()));
    if (schedgroup)
        connect(instancecount, SIGNAL(valueChanged(const QString &)),
                this,     SLOT(UpdateSchedGroup(const QString &)));
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
    uint cid = id->getValue().toUInt();
    QString raw_card_type = CardUtil::GetRawInputType(cid);
    bool enable = (sourceid.toInt() > 0);
    scan->setEnabled(enable && !raw_card_type.isEmpty() &&
                     !CardUtil::IsUnscanable(raw_card_type));
    srcfetch->setEnabled(enable);
}

void CardInput::UpdateSchedGroup(const QString &val)
{
    int value = val.toInt();
    if (value <= 1)
        schedgroup->setValue(false);
    schedgroup->setEnabled(value > 1);
}

QString CardInput::getSourceName(void) const
{
    return sourceid->getValueLabel();
}

void CardInput::CreateNewInputGroup(void)
{
    inputgrp0->Save();
    inputgrp1->Save();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythTextInputDialog *settingdialog =
        new MythTextInputDialog(popupStack, tr("Enter new group name"));

    if (settingdialog->Create())
    {
        connect(settingdialog, SIGNAL(haveResult(QString)),
                SLOT(CreateNewInputGroupSlot(const QString&)));
        popupStack->AddScreen(settingdialog);
    }
    else
        delete settingdialog;
}

void CardInput::CreateNewInputGroupSlot(const QString& name)
{
    if (name.isEmpty())
    {
        ShowOkPopup(tr("Sorry, this Input Group name cannot be blank."));
        return;
    }

    QString new_name = QString("user:") + name;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT inputgroupname "
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
        ShowOkPopup(tr("Sorry, this Input Group name is already in use."));
        return;
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
    uint crdid = id->getValue().toUInt();
    QString in = inputname->getValue();

#ifdef USING_BACKEND
    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    Save(); // save info for scanner.

    QString cardtype = CardUtil::GetRawInputType(crdid);
    if (CardUtil::IsUnscanable(cardtype))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Sorry, %1 cards do not yet support scanning.")
                .arg(cardtype));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    StandardSettingDialog *ssd =
        new StandardSettingDialog(mainStack, "generalsettings",
                                  new ScanWizard(srcid, crdid, in));

    if (ssd->Create())
    {
        connect(ssd, &StandardSettingDialog::Exiting,
                [=]()
                {
                    if (SourceUtil::GetChannelCount(srcid))
                        startchan->SetSourceID(QString::number(srcid));
                    if (num_channels_before)
                    {
                        startchan->Load();
                        startchan->Save();
                    }
                });
        mainStack->AddScreen(ssd);
    }
    else
        delete ssd;

#else
    LOG(VB_GENERAL, LOG_ERR, "You must compile the backend "
                             "to be able to scan for channels");
#endif
}

void CardInput::sourceFetch(void)
{
    uint srcid = sourceid->getValue().toUInt();
    uint crdid = id->getValue().toUInt();

    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    if (crdid && srcid)
    {
        Save(); // save info for fetch..

        QString cardtype = CardUtil::GetRawInputType(crdid);

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
    QString cardinputidTag(":WHERECARDID");

    QString query("cardid = " + cardinputidTag);

    bindings.insert(cardinputidTag, m_parent.getInputID());

    return query;
}

QString CardInputDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString cardinputidTag(":SETCARDID");
    QString colTag(":SET" + GetColumnName().toUpper());

    QString query("cardid = " + cardinputidTag + ", " +
            GetColumnName() + " = " + colTag);

    bindings.insert(cardinputidTag, m_parent.getInputID());
    bindings.insert(colTag, user->GetDBValue());

    return query;
}

void CardInput::loadByID(int inputid)
{
    id->setValue(inputid);
    externalInputSettings->Load(inputid);
    GroupSetting::Load();
}

void CardInput::loadByInput(int _cardid, QString _inputname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardid FROM capturecard "
                  "WHERE cardid = :CARDID AND inputname = :INPUTNAME");
    query.bindValue(":CARDID", _cardid);
    query.bindValue(":INPUTNAME", _inputname);

    if (query.exec() && query.isActive() && query.next())
    {
        loadByID(query.value(0).toInt());
    }
}

void CardInput::Save(void)
{
    uint cardid = id->getValue().toUInt();
    QString init_input = CardUtil::GetInputName(cardid);
    GroupSetting::Save();
    externalInputSettings->Store(getInputID());

    uint icount = 1;
    if (instancecount)
        icount = instancecount->getValue().toUInt();
    vector<uint> cardids = CardUtil::GetChildInputIDs(cardid);

    // Delete old clone cards as required.
    for (uint i = cardids.size() + 1;
         (i > icount) && !cardids.empty(); --i)
    {
        CardUtil::DeleteInput(cardids.back());
        cardids.pop_back();
    }

    // Clone this config to existing clone cards.
    for (uint i = 0; i < cardids.size(); ++i)
    {
        CardUtil::CloneCard(cardid, cardids[i]);
    }

    // Create new clone cards as required.
    for (uint i = cardids.size() + 1; i < icount; i++)
    {
        CardUtil::CloneCard(cardid, 0);
    }

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

void CaptureCardButton::edit(MythScreenType *)
{
    emit Clicked(m_value);
}

void CaptureCardEditor::AddSelection(const QString &label, const char *slot)
{
    ButtonStandardSetting *button = new ButtonStandardSetting(label);
    connect(button, SIGNAL(clicked()), slot);
    addChild(button);
}

void CaptureCardEditor::ShowDeleteAllCaptureCardsDialogOnHost()
{
    ShowOkPopup(
        tr("Are you sure you want to delete "
           "ALL capture cards on %1?").arg(gCoreContext->GetHostName()),
        this,
        SLOT(DeleteAllCaptureCardsOnHost(bool)),
        true);
}

void CaptureCardEditor::ShowDeleteAllCaptureCardsDialog()
{
    ShowOkPopup(
        tr("Are you sure you want to delete "
           "ALL capture cards?"),
        this,
        SLOT(DeleteAllCaptureCards(bool)),
        true);
}

void CaptureCardEditor::AddNewCard()
{
    CaptureCard *card = new CaptureCard();
    card->setLabel(tr("New capture card"));
    card->Load();
    addChild(card);
    emit settingsChanged(this);
}

void CaptureCardEditor::DeleteAllCaptureCards(bool doDelete)
{
    if (!doDelete)
        return;

    CardUtil::DeleteAllInputs();
    Load();
    emit settingsChanged(this);
}

void CaptureCardEditor::DeleteAllCaptureCardsOnHost(bool doDelete)
{
    if (!doDelete)
        return;

    MSqlQuery cards(MSqlQuery::InitCon());

    cards.prepare(
        "SELECT cardid "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME");
    cards.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!cards.exec() || !cards.isActive())
    {
        ShowOkPopup(
            tr("Error getting list of cards for this host. "
               "Unable to delete capturecards for %1")
            .arg(gCoreContext->GetHostName()));

        MythDB::DBError("Selecting cardids for deletion", cards);
        return;
    }

    while (cards.next())
        CardUtil::DeleteInput(cards.value(0).toUInt());

    Load();
    emit settingsChanged(this);
}

CaptureCardEditor::CaptureCardEditor()
{
    setLabel(tr("Capture cards"));
}

void CaptureCardEditor::Load(void)
{
    clearSettings();
    AddSelection(QObject::tr("(New capture card)"), SLOT(AddNewCard()));
    AddSelection(QObject::tr("(Delete all capture cards on %1)")
                 .arg(gCoreContext->GetHostName()),
                 SLOT(ShowDeleteAllCaptureCardsDialogOnHost()));
    AddSelection(QObject::tr("(Delete all capture cards)"),
                 SLOT(ShowDeleteAllCaptureCardsDialog()));
    CaptureCard::fillSelections(this);
}

VideoSourceEditor::VideoSourceEditor()
{
    setLabel(tr("Video sources"));
}

void VideoSourceEditor::Load(void)
{
    clearSettings();
    AddSelection(QObject::tr("(New video source)"), SLOT(NewSource()));
    AddSelection(QObject::tr("(Delete all video sources)"),
                 SLOT(ShowDeleteAllSourcesDialog()));
    VideoSource::fillSelections(this);
    GroupSetting::Load();
}

void VideoSourceEditor::AddSelection(const QString &label, const char* slot)
{
    ButtonStandardSetting *button = new ButtonStandardSetting(label);
    connect(button, SIGNAL(clicked()), slot);
    addChild(button);
}

void VideoSourceEditor::ShowDeleteAllSourcesDialog(void)
{
    ShowOkPopup(
       tr("Are you sure you want to delete "
          "ALL video sources?"),
       this,
       SLOT(DeleteAllSources(bool)),
       true);
}

void VideoSourceEditor::DeleteAllSources(bool doDelete)
{
    if (!doDelete)
        return;

    SourceUtil::DeleteAllSources();
    Load();
    emit settingsChanged(this);
}

void VideoSourceEditor::NewSource(void)
{
    VideoSource *source = new VideoSource();
    source->setLabel(tr("New video source"));
    source->Load();
    addChild(source);
    emit settingsChanged(this);
}

CardInputEditor::CardInputEditor()
{
    setLabel(tr("Input connections"));
}

void CardInputEditor::Load(void)
{
    cardinputs.clear();
    clearSettings();

    // We do this manually because we want custom labels.  If
    // SelectSetting provided a facility to edit the labels, we
    // could use CaptureCard::fillSelections

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, videodevice, cardtype, inputname "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME "
        "      AND parentid = 0 "
        "ORDER BY cardid");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec())
    {
        MythDB::DBError("CardInputEditor::load", query);
        return;
    }

    while (query.next())
    {
        uint    cardid      = query.value(0).toUInt();
        QString videodevice = query.value(1).toString();
        QString cardtype    = query.value(2).toString();
        QString inputname   = query.value(3).toString();

        CardInput *cardinput = new CardInput(cardtype, videodevice,
                                             cardid);
        cardinput->loadByID(cardid);
        QString inputlabel = QString("%1 (%2) -> %3")
            .arg(CardUtil::GetDeviceLabel(cardtype, videodevice))
            .arg(inputname).arg(cardinput->getSourceName());
        cardinputs.push_back(cardinput);
        cardinput->setLabel(inputlabel);
        addChild(cardinput);
    }

    GroupSetting::Load();
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

void DVBConfigurationGroup::reloadDiseqcTree(const QString &videodevice)
{
    if (diseqc_tree)
        diseqc_tree->Load(videodevice);
}

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

    switch (CardUtil::toInputType(subtype))
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
        case CardUtil::DVBT2:
            cardtype->setValue("DVB-T2");
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
    CaptureCardComboBoxSetting(parent, false, "audiodevice"),
    last_device(dev), last_cardtype(type)
{
    setLabel(QObject::tr("Audio input"));
    setHelpText(QObject::tr("If there is more than one audio input, "
                            "select which one to use."));
    int cardid = parent.getCardID();
    if (cardid <= 0)
        return;

    last_cardtype = CardUtil::GetRawInputType(cardid);
    last_device   = CardUtil::GetAudioDevice(cardid);
}

int TunerCardAudioInput::fillSelections(const QString &device)
{
    clearSelections();

    if (device.isEmpty())
        return 0;

    last_device = device;
    QStringList inputs =
        CardUtil::ProbeAudioInputs(device, last_cardtype);

    for (uint i = 0; i < (uint)inputs.size(); i++)
    {
        addSelection(inputs[i], QString::number(i),
                     last_device == QString::number(i));
    }
    return inputs.size();
}

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent,
                                             CardType& cardType) :
    parent(a_parent),
    diseqc_tree(new DiSEqCDevTree())
{
    setVisible(false);

    cardnum  = new DVBCardNum(parent);
    cardname = new DVBCardName();
    cardtype = new DVBCardType();

    signal_timeout = new SignalTimeout(parent, 500, 250);
    channel_timeout = new ChannelTimeout(parent, 3000, 1750);

    cardType.addTargetedChild("DVB", cardnum);

    cardType.addTargetedChild("DVB", cardname);
    cardType.addTargetedChild("DVB", cardtype);

    cardType.addTargetedChild("DVB", signal_timeout);
    cardType.addTargetedChild("DVB", channel_timeout);

    cardType.addTargetedChild("DVB", new EmptyAudioDevice(parent));
    cardType.addTargetedChild("DVB", new EmptyVBIDevice(parent));

    cardType.addTargetedChild("DVB", new DVBNoSeqStart(parent));
    cardType.addTargetedChild("DVB", new DVBOnDemand(parent));
    cardType.addTargetedChild("DVB", new DVBEITScan(parent));

    diseqc_btn = new DeviceTree(*diseqc_tree);
    diseqc_btn->setLabel(tr("DiSEqC (Switch, LNB, and Rotor Configuration)"));
    diseqc_btn->setHelpText(tr("Input and satellite settings."));
    diseqc_btn->setVisible(false);

    tuning_delay = new DVBTuningDelay(parent);
    cardType.addTargetedChild("DVB", tuning_delay);
    cardType.addTargetedChild("DVB", diseqc_btn);
    tuning_delay->setVisible(false);

    connect(cardnum,      SIGNAL(valueChanged(const QString&)),
            this,         SLOT(  probeCard   (const QString&)));
    connect(cardnum,      SIGNAL(valueChanged(const QString&)),
            this,         SLOT(  reloadDiseqcTree(const QString&)));
}

DVBConfigurationGroup::~DVBConfigurationGroup()
{
    if (diseqc_tree)
    {
        delete diseqc_tree;
        diseqc_tree = NULL;
    }
}

void DVBConfigurationGroup::Load(void)
{
    reloadDiseqcTree(cardnum->getValue());
    diseqc_btn->Load();
    GroupSetting::Load();
    if (cardtype->getValue() == "DVB-S" ||
        cardtype->getValue() == "DVB-S2" ||
        DiSEqCDevTree::Exists(parent.getCardID()))
    {
        diseqc_btn->setVisible(true);
    }
}

void DVBConfigurationGroup::Save(void)
{
    GroupSetting::Save();
    diseqc_tree->Store(parent.getCardID(), cardnum->getValue());
    DiSEqCDev trees;
    trees.InvalidateTrees();
}
