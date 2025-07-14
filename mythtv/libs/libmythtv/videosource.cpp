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

// Qt headers
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLayout>
#include <QMap>
#include <QStringList>
#include <QTextStream>
#include <utility>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythui/mythnotification.h"
#include "libmythui/mythterminal.h"
#include "libmythupnp/httprequest.h"    // for TestMimeType()

#include "cardutil.h"
#include "channelinfo.h"
#include "channelutil.h"
#include "diseqcsettings.h"
#include "frequencies.h"
#include "recorders/firewiredevice.h"
#include "scanwizard.h"
#include "sourceutil.h"
#include "v4l2util.h"
#include "videosource.h"

#if CONFIG_DVB
#include "recorders/dvbtypes.h"
#endif

#if CONFIG_VBOX
#include "recorders/vboxutils.h"
#endif

#if CONFIG_HDHOMERUN
#include HDHOMERUN_HEADERFILE
#endif

VideoSourceSelector::VideoSourceSelector(uint    _initial_sourceid,
                                         QString _card_types,
                                         bool    _must_have_mplexid) :
    m_initialSourceId(_initial_sourceid),
    m_cardTypes(std::move(_card_types)),
    m_mustHaveMplexId(_must_have_mplexid)
{
    setLabel(tr("Video Source"));
    setHelpText(
        QObject::tr(
            "Select a video source that is connected to one "
            "or more capture cards. Default is the video source "
            "selected in the Channel Editor page."
            ));
}

void VideoSourceSelector::Load(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr =
        "SELECT DISTINCT videosource.name, videosource.sourceid "
        "FROM capturecard, videosource";

    querystr += (m_mustHaveMplexId) ? ", channel " : " ";

    querystr +=
        "WHERE capturecard.sourceid = videosource.sourceid AND "
        "      capturecard.hostname = :HOSTNAME ";

    if (!m_cardTypes.isEmpty())
    {
        querystr += QString(" AND capturecard.cardtype in %1 ")
            .arg(m_cardTypes);
    }

    if (m_mustHaveMplexId)
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

    uint sel = 0;
    uint cnt = 0;
    for (; query.next(); cnt++)
    {
        addSelection(query.value(0).toString(),
                     query.value(1).toString());

        sel = (query.value(1).toUInt() == m_initialSourceId) ? cnt : sel;
    }

    if (m_initialSourceId)
    {
        if (cnt)
            setValue(sel);
    }

    TransMythUIComboBoxSetting::Load();
}

VideoSourceShow::VideoSourceShow(uint    _initial_sourceid) :
    m_initialSourceId(_initial_sourceid)
{
    setLabel(tr("Video Source"));
    setHelpText(
        QObject::tr(
            "The video source that is "
            "selected in the Channel Editor page."
            ));
}

void VideoSourceShow::Load(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr =
        "SELECT DISTINCT videosource.name, videosource.sourceid "
        "FROM capturecard, videosource "
        "WHERE capturecard.sourceid = videosource.sourceid AND "
        "      capturecard.hostname = :HOSTNAME            AND "
        "      videosource.sourceid = :SOURCEID ";

    query.prepare(querystr);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    query.bindValue(":SOURCEID", m_initialSourceId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("VideoSourceShow::Load", query);
        return;
    }

    if (query.next())
    {
        setValue(query.value(0).toString());
    }
}

class InstanceCount : public MythUISpinBoxSetting
{
  public:
    explicit InstanceCount(const CardInput &parent) :
        MythUISpinBoxSetting(new CardInputDBStorage(this, parent, "reclimit"),
                             1, 10, 1)
    {
        setLabel(QObject::tr("Max recordings"));
        setValue(1);
        setHelpText(
            QObject::tr(
                "Maximum number of simultaneous recordings MythTV will "
                "attempt using this device.  If set to a value other than "
                "1, MythTV can sometimes record multiple programs on "
                "the same multiplex or overlapping copies of the same "
                "program on a single channel."
                ));
    };

    ~InstanceCount() override
    {
        delete GetStorage();
    }
};

class SchedGroup : public MythUICheckBoxSetting
{
  public:
    explicit SchedGroup(const CardInput &parent) :
        MythUICheckBoxSetting(new CardInputDBStorage(this, parent, "schedgroup"))
    {
        setLabel(QObject::tr("Schedule as group"));
        setValue(true);
        setHelpText(
            QObject::tr(
                "Schedule all virtual inputs on this device as a group.  "
                "This is more efficient than scheduling each input "
                "individually.  Additional, virtual inputs will be "
                "automatically added as needed to fulfill the recording "
                "load."
                ));
    };

    ~SchedGroup() override
    {
        delete GetStorage();
    }
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
    bindings.insert(colTag, m_user->GetDBValue());

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
    bindings.insert(colTag, m_user->GetDBValue());

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

    ~XMLTVGrabber() override
    {
        delete GetStorage();
    }

    void Load(void) override // StandardSetting
    {
        addTargetedChild("eitonly",   new EITOnly_config(m_parent, this));
        addTargetedChild("/bin/true", new NoGrabber_config(m_parent));

        addSelection(
            QObject::tr("Transmitted guide only (EIT)"), "eitonly");

        addSelection(QObject::tr("No grabber"), "/bin/true");

        QString gname;
        QString d1;
        QString d2;
        QString d3;
        SourceUtil::GetListingsLoginData(m_parent.getSourceID(), gname, d1, d2, d3);

#ifdef _MSC_VER
#pragma message( "tv_find_grabbers is not supported yet on windows." )
        //-=>TODO:Screen doesn't show up if the call to MythSysemLegacy is executed
#else

        QString loc = QString("XMLTVGrabber::Load(%1): ").arg(m_parent.getSourceName());

        QMutexLocker lock(&m_lock);
        if (m_nameList.isEmpty())
        {
            QStringList args;
            args += "baseline";

            MythSystemLegacy find_grabber_proc("tv_find_grabbers", args,
                                                kMSStdOut | kMSRunShell | kMSDontDisableDrawing);
            find_grabber_proc.Run(25s);
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
                        grabber_list.split("|", Qt::SkipEmptyParts);
                    QString grabber_name = grabber_split[1] + " (xmltv)";
                    QFileInfo grabber_file(grabber_split[0]);

                    m_nameList.push_back(grabber_name);
                    m_progList.push_back(grabber_file.fileName());
                    LOG(VB_GENERAL, LOG_DEBUG, "Found " + grabber_split[0]);
                }
                LOG(VB_GENERAL, LOG_INFO, loc + "Finished running tv_find_grabbers");
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, loc + "Failed to run tv_find_grabbers");
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, loc + "Loading results of tv_find_grabbers");
        }

        LoadXMLTVGrabbers(m_nameList, m_progList);
        MythUIComboBoxSetting::Load();
#endif
    }

    void Save(void) override // StandardSetting
    {
        MythUIComboBoxSetting::Save();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "UPDATE videosource "
            "SET userid=NULL, password=NULL "
            "WHERE xmltvgrabber NOT IN ( 'technovera' )");
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

private:
    static QMutex      m_lock;
    static QStringList m_nameList;
    static QStringList m_progList;
};

// Results of search for XMLTV grabbers
QMutex      XMLTVGrabber::m_lock;
QStringList XMLTVGrabber::m_nameList;
QStringList XMLTVGrabber::m_progList;

class CaptureCardSpinBoxSetting : public MythUISpinBoxSetting
{
  public:
    CaptureCardSpinBoxSetting(const CaptureCard &parent,
                              std::chrono::milliseconds min_val,
                              std::chrono::milliseconds max_val,
                              std::chrono::milliseconds step,
                              const QString &setting) :
        MythUISpinBoxSetting(new CaptureCardDBStorage(this, parent, setting),
                             min_val.count(), max_val.count(), step.count())
    {
    }

    ~CaptureCardSpinBoxSetting() override
    {
        delete GetStorage();
    }
    // Handles integer milliseconds (compiler converts seconds to milliseconds)
    void setValueMs (std::chrono::milliseconds newValue)
        { setValue(newValue.count()); }
    // Handle non-integer seconds
    template<typename T, typename = std::enable_if_t<!std::is_integral<T>()>>
    void setValueMs (std::chrono::duration<T> newSecs)
        { setValueMs(duration_cast<std::chrono::milliseconds>(newSecs)); }
};

class CaptureCardTextEditSetting : public MythUITextEditSetting
{
  public:
    CaptureCardTextEditSetting(const CaptureCard &parent,
                               const QString &setting) :
        MythUITextEditSetting(new CaptureCardDBStorage(this, parent, setting))
    {
    }

    ~CaptureCardTextEditSetting() override
    {
        delete GetStorage();
    }
};

class ScanFrequencyStart : public MythUITextEditSetting
{
  public:
    explicit ScanFrequencyStart(const VideoSource &parent) :
        MythUITextEditSetting(new VideoSourceDBStorage(this, parent, "scanfrequency"))
    {
       setLabel(QObject::tr("Scan Frequency"));
       setHelpText(QObject::tr("The frequency to start scanning this video source. "
                               "This is then default for 'Full Scan (Tuned)' channel scanning. "
                               "Frequency value in Hz for DVB-T/T2/C, in kHz for DVB-S/S2. "
                               "Leave at 0 if not known. "));
    };

    ~ScanFrequencyStart() override
    {
        delete GetStorage();
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

    ~DVBNetID() override
    {
        delete GetStorage();
    }
};

class BouquetID : public MythUISpinBoxSetting
{
  public:
    BouquetID(const VideoSource &parent, signed int value, signed int min_val) :
        MythUISpinBoxSetting(new VideoSourceDBStorage(this, parent, "bouquet_id"),
                             min_val, 0xffff, 1)
    {
       setLabel(QObject::tr("Bouquet ID"));
       setHelpText(QObject::tr("Bouquet ID for Freesat or Sky on satellite Astra-2 28.2E. "
                               "Leave this at 0 if you do not receive this satellite. "
                               "This is needed to get the Freesat and Sky channel numbers. "
                               "Value 272 selects Freesat bouquet 'England HD'. "
                               "See the MythTV Wiki https://www.mythtv.org/wiki/DVB_UK."));
       setValue(value);
    };

    ~BouquetID() override
    {
        delete GetStorage();
    }
};

class RegionID : public MythUISpinBoxSetting
{
  public:
    RegionID(const VideoSource &parent, signed int value, signed int min_val) :
        MythUISpinBoxSetting(new VideoSourceDBStorage(this, parent, "region_id"),
                             min_val, 100, 1)
    {
       setLabel(QObject::tr("Region ID"));
       setHelpText(QObject::tr("Region ID for Freesat or Sky on satellite Astra-2 28.2E. "
                               "Leave this at 0 you do not receive this satellite.  "
                               "This is needed to get the Freesat and Sky channel numbers. "
                               "Value 1 selects region London. "
                               "See the MythTV Wiki https://www.mythtv.org/wiki/DVB_UK."));
       setValue(value);
    };

    ~RegionID() override
    {
        delete GetStorage();
    }
};

class LCNOffset : public MythUISpinBoxSetting
{
  public:
    LCNOffset(const VideoSource &parent, signed int value, signed int min_val) :
        MythUISpinBoxSetting(new VideoSourceDBStorage(this, parent, "lcnoffset"),
                             min_val, 20000, 100)
    {
       setLabel(QObject::tr("Logical Channel Number Offset"));
       setHelpText(QObject::tr("The offset is added to each logical channel number found "
                               "during a scan of a DVB video source. This makes it possible "
                               "to give different video sources a non-overlapping range "
                               "of channel numbers. Leave at 0 if you have only one video source "
                               "or if the video sources do not have DVB logical channel numbers."));
       setValue(value);
    };

    ~LCNOffset() override
    {
        delete GetStorage();
    }
};

FreqTableSelector::FreqTableSelector(const VideoSource &parent) :
    MythUIComboBoxSetting(new VideoSourceDBStorage(this, parent, "freqtable"))
{
    setLabel(QObject::tr("Channel frequency table"));
    addSelection("default");

    for (const auto & chanlist : gChanLists)
        addSelection(chanlist.name);

    setHelpText(QObject::tr("Use default unless this source uses a "
                "different frequency table than the system wide table "
                "defined in the General settings."));
}

FreqTableSelector::~FreqTableSelector()
{
    delete GetStorage();
}

TransFreqTableSelector::TransFreqTableSelector(uint _sourceid) :
    m_sourceId(_sourceid)
{
    setLabel(QObject::tr("Channel frequency table"));

    for (const auto & chanlist : gChanLists)
        addSelection(chanlist.name);
}

void TransFreqTableSelector::Load(void)
{
    int idx1 = getValueIndex(gCoreContext->GetSetting("FreqTable"));
    if (idx1 >= 0)
        setValue(idx1);

    if (!m_sourceId)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT freqtable "
        "FROM videosource "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", m_sourceId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("TransFreqTableSelector::load", query);
        return;
    }

    m_loadedFreqTable.clear();

    if (query.next())
    {
        m_loadedFreqTable = query.value(0).toString();
        if (!m_loadedFreqTable.isEmpty() &&
            (m_loadedFreqTable.toLower() != "default"))
        {
            int idx2 = getValueIndex(m_loadedFreqTable);
            if (idx2 >= 0)
                setValue(idx2);
        }
    }
}

void TransFreqTableSelector::Save(void)
{
    LOG(VB_GENERAL, LOG_INFO, "TransFreqTableSelector::Save(void)");

    if ((m_loadedFreqTable == getValue()) ||
        ((m_loadedFreqTable.toLower() == "default") &&
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
    query.bindValue(":SOURCEID",  m_sourceId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("TransFreqTableSelector::load", query);
        return;
    }
}

void TransFreqTableSelector::SetSourceID(uint sourceid)
{
    m_sourceId = sourceid;
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

    ~UseEIT() override
    {
        delete GetStorage();
    }
};

XMLTV_generic_config::XMLTV_generic_config(const VideoSource& _parent,
                                           const QString& _grabber,
                                           StandardSetting *_setting) :
    m_parent(_parent), m_grabber(_grabber)
{
    setVisible(false);

    QString filename = QString("%1/%2.xmltv")
        .arg(GetConfDir(), m_parent.getSourceName());

    m_grabberArgs.push_back("--config-file");
    m_grabberArgs.push_back(filename);
    m_grabberArgs.push_back("--configure");

    _setting->addTargetedChild(_grabber, new UseEIT(m_parent));

    auto *config = new ButtonStandardSetting(tr("Configure"));
    config->setHelpText(tr("Run XMLTV configure command."));

    _setting->addTargetedChild(_grabber, config);

    connect(config, &ButtonStandardSetting::clicked, this, &XMLTV_generic_config::RunConfig);
}

void XMLTV_generic_config::Save()
{
    GroupSetting::Save();
#if 0
    QString err_msg = QObject::tr(
        "You MUST run 'mythfilldatabase --manual' the first time,\n"
        "instead of just 'mythfilldatabase'.\nYour grabber does not provide "
        "channel numbers, so you have to set them manually.");

    if (is_grabber_external(m_grabber))
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
        new MythTerminal(mainStack, m_grabber, m_grabberArgs);

    if (ssd->Create())
        mainStack->AddScreen(ssd);
    else
        delete ssd;
}

EITOnly_config::EITOnly_config(const VideoSource& _parent, StandardSetting *_setting)
  : m_useEit(new UseEIT(_parent))
{
    setVisible(false);

    m_useEit->setValue(true);
    m_useEit->setVisible(false);
    addChild(m_useEit);

    auto *label=new TransTextEditSetting();
    label->setValue(QObject::tr("Use only the transmitted guide data."));
    label->setHelpText(
        QObject::tr("This will usually only work with ATSC or DVB channels, "
                    "and generally provides data only for the next few days."));
    _setting->addTargetedChild("eitonly", label);
}

void EITOnly_config::Save(void)
{
    // Force this value on
    m_useEit->setValue(true);
    m_useEit->Save();
}

NoGrabber_config::NoGrabber_config(const VideoSource& _parent)
  : m_useEit(new UseEIT(_parent))
{
    m_useEit->setValue(false);
    m_useEit->setVisible(false);
    addChild(m_useEit);

    auto *label = new TransTextEditSetting();
    label->setValue(QObject::tr("Do not configure a grabber"));
    addTargetedChild("/bin/true", label);
}

void NoGrabber_config::Save(void)
{
    m_useEit->setValue(false);
    m_useEit->Save();
}

VideoSource::VideoSource()
    // must be first
  : m_id(new ID())
{
    addChild(m_id = new ID());

    setLabel(QObject::tr("Video Source Setup"));
    addChild(m_name = new Name(*this));
    addChild(new XMLTVGrabber(*this));
    addChild(new FreqTableSelector(*this));
    addChild(new ScanFrequencyStart(*this));
    addChild(new DVBNetID(*this, -1, -1));
    addChild(new BouquetID(*this, 0, 0));
    addChild(new RegionID(*this, 0, 0));
    addChild(new LCNOffset(*this, 0, 0));
}

bool VideoSource::canDelete(void)
{
    return true;
}

void VideoSource::deleteEntry(void)
{
    SourceUtil::DeleteSource(getSourceID());
}

bool VideoSourceEditor::cardTypesInclude(int sourceID,
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
            auto* source = new VideoSource();
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
    m_id->setValue(sourceid);
}

class VideoDevice : public CaptureCardComboBoxSetting
{
  public:
    explicit VideoDevice(const CaptureCard &parent,
                uint    minor_min = 0,
                uint    minor_max = UINT_MAX,
                const QString& card      = QString(),
                const QRegularExpression& driver = QRegularExpression()) :
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
    void fillSelectionsFromDir(const QDir &dir,
                               [[maybe_unused]] bool absPath = true)
    {
        fillSelectionsFromDir(dir, 0, 255, QString(), QRegularExpression(), false);
    }

    uint fillSelectionsFromDir(const QDir& dir,
                               uint minor_min, uint minor_max,
                               const QString& card, const QRegularExpression& driver,
                               bool allow_duplicates)
    {
        uint cnt = 0;
        QFileInfoList entries = dir.entryInfoList();
        for (const auto & fi : std::as_const(entries))
        {
            struct stat st {};
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
            if (!allow_duplicates && m_minorList[minor_num])
                continue;

            // if the driver returns any info add this device to our list
            QByteArray tmp = filepath.toLatin1();
            int videofd = open(tmp.constData(), O_RDWR);
            if (videofd >= 0)
            {
                QString card_name;
                QString driver_name;
                if (CardUtil::GetV4LInfo(videofd, card_name, driver_name))
                {
                    auto match = driver.match(driver_name);
                    if ((!driver.pattern().isEmpty() || match.hasMatch()) &&
                        (card.isEmpty() || (card_name == card)))
                    {
                        addSelection(filepath);
                        cnt++;
                    }
                }
                close(videofd);
            }

            // add to list of minors discovered to avoid duplicates
            m_minorList[minor_num] = 1;
        }

        return cnt;
    }

    QString Driver(void) const { return m_driverName; }
    QString Card(void) const { return m_cardName; }

  private:
    QMap<uint, uint> m_minorList;
    QString          m_cardName;
    QString          m_driverName;
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
        clearSelections();
        QDir dev("/dev/v4l", "vbi*", QDir::Name, QDir::System);
        uint count = fillSelectionsFromDir(dev, card, driver);
        if (count == 0)
        {
            dev.setPath("/dev");
            count = fillSelectionsFromDir(dev, card, driver);
            if ((count == 0U) && !getValue().isEmpty())
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
    void fillSelectionsFromDir(const QDir &dir,
                               [[maybe_unused]] bool absPath = true)
    {
        fillSelectionsFromDir(dir, QString(), QString());
    }

    uint fillSelectionsFromDir(const QDir &dir, const QString &card,
                               const QString &driver)
    {
        QStringList devices;
        QFileInfoList entries = dir.entryInfoList();
        for (const auto & fi : std::as_const(entries))
        {
            QString    device = fi.absoluteFilePath();
            QByteArray adevice = device.toLatin1();
            int vbifd = open(adevice.constData(), O_RDWR);
            if (vbifd < 0)
                continue;

            QString cn;
            QString dn;
            if (CardUtil::GetV4LInfo(vbifd, cn, dn)  &&
                (driver.isEmpty() || (dn == driver)) &&
                (card.isEmpty()   || (cn == card)))
            {
                devices.push_back(device);
            }

            close(vbifd);
        }

        QString sel = getValue();
        for (const QString& device : std::as_const(devices))
            addSelection(device, device, device == sel);

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

    ~CommandPath() override
    {
        delete GetStorage();
    }
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

    ~FileDevice() override
    {
        delete GetStorage();
    }
};

class AudioDevice : public CaptureCardComboBoxSetting
{
  public:
    explicit AudioDevice(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, true /* mustexist false */,
                                   "audiodevice")
    {
        setLabel(QObject::tr("Audio device"));
#if CONFIG_AUDIO_OSS
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
#endif
#if CONFIG_AUDIO_ALSA
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
    // Handles integer milliseconds (compiler converts seconds to milliseconds)
    SignalTimeout(const CaptureCard &parent, std::chrono::milliseconds value,
                  std::chrono::milliseconds min_val) :
        CaptureCardSpinBoxSetting(parent, min_val, 60s, 250ms, "signal_timeout")
    {
        setLabel(QObject::tr("Signal timeout (ms)"));
        setValueMs(value);
        setHelpText(QObject::tr(
                        "Maximum time (in milliseconds) MythTV waits for "
                        "a signal when scanning for channels."));
    };
    // Handle non-integer seconds
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>> >
    SignalTimeout(const CaptureCard &parent, std::chrono::milliseconds value, std::chrono::duration<T> min_secs) :
        SignalTimeout(parent, value, duration_cast<std::chrono::milliseconds>(min_secs)) {};
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>> >
    SignalTimeout(const CaptureCard &parent, std::chrono::duration<T> value, std::chrono::duration<T> min_secs) :
        SignalTimeout(parent,
                      duration_cast<std::chrono::milliseconds>(value),
                      duration_cast<std::chrono::milliseconds>(min_secs)) {};
};

class ChannelTimeout : public CaptureCardSpinBoxSetting
{
  public:
    // Handles integer milliseconds (compiler converts seconds to milliseconds)
    ChannelTimeout(const CaptureCard &parent, std::chrono::milliseconds value,
                   std::chrono::milliseconds min_val) :
        CaptureCardSpinBoxSetting(parent, min_val, 65s, 250ms, "channel_timeout")
    {
        setLabel(QObject::tr("Tuning timeout (ms)"));
        setValueMs(value);
        setHelpText(QObject::tr(
                        "Maximum time (in milliseconds) MythTV waits for "
                        "a channel lock.  For recordings, if this time is "
                        "exceeded, the recording will be marked as failed."));
    };
    // Handle non-integer seconds
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>> >
    ChannelTimeout(const CaptureCard &parent, std::chrono::milliseconds value, std::chrono::duration<T> min_secs) :
        ChannelTimeout(parent, value, duration_cast<std::chrono::milliseconds>(min_secs)) {};
    template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>> >
    ChannelTimeout(const CaptureCard &parent, std::chrono::duration<T> value, std::chrono::duration<T> min_secs) :
        ChannelTimeout(parent, value, duration_cast<std::chrono::milliseconds>(min_secs)) {};
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

    ~SkipBtAudio() override
    {
        delete GetStorage();
    }
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
            (std::find(sdevs.begin(), sdevs.end(), current) == sdevs.end()))
        {
            std::stable_sort(sdevs.begin(), sdevs.end());
        }

        QStringList db = CardUtil::GetVideoDevices("DVB");

        QMap<QString,bool> in_use;
        QString sel = current;
        for (const QString& dev : std::as_const(sdevs))
        {
            in_use[dev] = std::find(db.begin(), db.end(), dev) != db.end();
            if (sel.isEmpty() && !in_use[dev])
                sel = dev;
        }

        if (sel.isEmpty() && !sdevs.empty())
            sel = sdevs[0];

        QString usestr = QString(" -- ");
        usestr += QObject::tr("Warning: already in use");

        for (const QString& dev : std::as_const(sdevs))
        {
            QString desc = dev + (in_use[dev] ? usestr : "");
            desc = (current == dev) ? dev : desc;
            addSelection(desc, dev, dev == sel);
        }
    }

    void Load(void) override // StandardSetting
    {
        clearSelections();
        addSelection(QString());

        StandardSetting::Load();

        QString dev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, getValue());
        fillSelections(dev);
    }
};

// Use capturecard/inputname to store the delivery system selection of the card
class DVBCardType : public CaptureCardComboBoxSetting
{
  public:
    explicit DVBCardType(const CaptureCard &parent) :
        CaptureCardComboBoxSetting(parent, false, "inputname")
    {
        setLabel(QObject::tr("Delivery system"));
        setHelpText(
            QObject::tr("If your card supports more than one delivery system "
                        "then you can select here the one that you want to use."));
    };
};

class DVBCardName : public GroupSetting
{
  public:
    DVBCardName()
    {
        setLabel(QObject::tr("Frontend ID"));
        setHelpText(
            QObject::tr("Identification string reported by the card. "
                        "If the message \"Could not get card info...\" appears "
                        "the card can be in use by another program."));
    };
};

class DVBNoSeqStart : public MythUICheckBoxSetting
{
  public:
    explicit DVBNoSeqStart(const CaptureCard &parent) :
        MythUICheckBoxSetting(
            new CaptureCardDBStorage(this, parent, "dvb_wait_for_seqstart"))
    {
        setLabel(QObject::tr("Wait for SEQ start header"));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, drop packets from the start of a DVB "
                        "recording until a sequence start header is seen."));
    };

    ~DVBNoSeqStart() override
    {
        delete GetStorage();
    }
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

    ~DVBOnDemand() override
    {
        delete GetStorage();
    }
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
                        "the DVB card is constantly in use."));
    };

    ~DVBEITScan() override
    {
        delete GetStorage();
    }
};

class DVBTuningDelay : public CaptureCardSpinBoxSetting
{
  public:
    explicit DVBTuningDelay(const CaptureCard &parent) :
        CaptureCardSpinBoxSetting(parent, 0ms, 2s, 25ms, "dvb_tuning_delay")
    {
        setLabel(QObject::tr("DVB tuning delay (ms)"));
        setValueMs(0ms);
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
#if CONFIG_FIREWIRE
        std::vector<AVCInfo> list = FirewireDevice::GetSTBList();
        for (auto & i : list)
        {
            QString guid = i.GetGUIDString();
            m_guidToAvcInfo[guid] = i;
            addSelection(guid);
        }
#endif // CONFIG_FIREWIRE
    }

    AVCInfo GetAVCInfo(const QString &guid) const
        { return m_guidToAvcInfo[guid]; }

  private:
    QMap<QString,AVCInfo> m_guidToAvcInfo;
};

FirewireModel::FirewireModel(const CaptureCard  &parent,
                             const FirewireGUID *_guid) :
    CaptureCardComboBoxSetting(parent, false, "firewire_model"),
    m_guid(_guid)
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

void FirewireModel::SetGUID([[maybe_unused]] const QString &_guid)
{
#if CONFIG_FIREWIRE
    AVCInfo info = m_guid->GetAVCInfo(_guid);
    QString model = FirewireDevice::GetModelName(info.m_vendorid, info.m_modelid);
    setValue(std::max(getValueIndex(model), 0));
#endif // CONFIG_FIREWIRE
}

void FirewireDesc::SetGUID([[maybe_unused]] const QString &_guid)
{
    setLabel(tr("Description"));

#if CONFIG_FIREWIRE
    QString name = m_guid->GetAVCInfo(_guid).m_product_name;
    name.replace("Scientific-Atlanta", "SA");
    name.replace(", Inc.", "");
    name.replace("Explorer(R)", "");
    name = name.simplified();
    setValue((name.isEmpty()) ? "" : name);
#endif // CONFIG_FIREWIRE
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

    ~FirewireConnection() override
    {
        delete GetStorage();
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

    ~FirewireSpeed() override
    {
        delete GetStorage();
    }
};

#if CONFIG_FIREWIRE
static void FirewireConfigurationGroup(CaptureCard& parent, CardType& cardtype)
{
    auto *dev(new FirewireGUID(parent));
    auto *desc(new FirewireDesc(dev));
    auto *model(new FirewireModel(parent, dev));
    cardtype.addTargetedChild("FIREWIRE", dev);
    cardtype.addTargetedChild("FIREWIRE", new EmptyAudioDevice(parent));
    cardtype.addTargetedChild("FIREWIRE", new EmptyVBIDevice(parent));
    cardtype.addTargetedChild("FIREWIRE", desc);
    cardtype.addTargetedChild("FIREWIRE", model);

#if CONFIG_FIREWIRE_LINUX
    cardtype.addTargetedChild("FIREWIRE", new FirewireConnection(parent));
    cardtype.addTargetedChild("FIREWIRE", new FirewireSpeed(parent));
#endif // CONFIG_FIREWIRE_LINUX

    cardtype.addTargetedChild("FIREWIRE", new SignalTimeout(parent, 2s, 1s));
    cardtype.addTargetedChild("FIREWIRE", new ChannelTimeout(parent, 9s, 1.75s));

    model->SetGUID(dev->getValue());
    desc->SetGUID(dev->getValue());
    QObject::connect(dev,   qOverload<const QString&>(&StandardSetting::valueChanged),
                     model, &FirewireModel::SetGUID);
    QObject::connect(dev,   qOverload<const QString&>(&StandardSetting::valueChanged),
                     desc,  &FirewireDesc::SetGUID);
}
#endif

#if CONFIG_HDHOMERUN

// -----------------------
// HDHomeRun Configuration
// -----------------------

HDHomeRunDeviceID::HDHomeRunDeviceID(const CaptureCard &parent,
                                     HDHomeRunConfigurationGroup &_group) :
    MythUITextEditSetting(
        new CaptureCardDBStorage(this, parent, "videodevice")),
    m_group(_group)
{
    setVisible(false);
};

HDHomeRunDeviceID::~HDHomeRunDeviceID()
{
    delete GetStorage();
}

void HDHomeRunDeviceID::Load(void)
{
    MythUITextEditSetting::Load();
    m_group.SetDeviceCheckBoxes(getValue());
}

void HDHomeRunDeviceID::Save(void)
{
    setValue(m_group.GetDeviceCheckBoxes());
    MythUITextEditSetting::Save();
}

class HDHomeRunEITScan : public MythUICheckBoxSetting
{
  public:
    explicit HDHomeRunEITScan(const CaptureCard &parent) :
        MythUICheckBoxSetting(
            new CaptureCardDBStorage(this, parent, "dvb_eitscan"))
    {
        setLabel(QObject::tr("Use HDHomeRun for active EIT scan"));
        setValue(true);
        setHelpText(
            QObject::tr("If enabled, activate active scanning for "
                        "program data (EIT). When this option is enabled "
                        "the HDHomeRun is constantly in use."));
    };

    ~HDHomeRunEITScan() override
    {
        delete GetStorage();
    }
};


class UseHDHomeRunDevice : public TransMythUICheckBoxSetting
{
  public:
    explicit UseHDHomeRunDevice(QString &deviceid, QString &model,
                                QString &ipaddr)
    {
        setLabel(QObject::tr("Use HDHomeRun %1 (%2 %3)")
                 .arg(deviceid, model, ipaddr));
        setValue(false);
        setHelpText(
            QObject::tr("If enabled, use tuners from this HDHomeRun "
                        "device."));
    };
};

HDHomeRunConfigurationGroup::HDHomeRunConfigurationGroup
        (CaptureCard& a_parent, CardType &a_cardtype) :
    m_parent(a_parent),
    m_deviceId(new HDHomeRunDeviceID(a_parent, *this))
{
    setVisible(false);

    // Fill Device list
    FillDeviceList();

    QMap<QString, HDHomeRunDevice>::iterator dit;
    for (dit = m_deviceList.begin(); dit != m_deviceList.end(); ++dit)
    {
        HDHomeRunDevice &dev = *dit;
        dev.m_checkbox = new UseHDHomeRunDevice(
            dev.m_deviceId, dev.m_model, dev.m_cardIp);
        a_cardtype.addTargetedChild("HDHOMERUN", dev.m_checkbox);
    }
    a_cardtype.addTargetedChild("HDHOMERUN", new EmptyAudioDevice(m_parent));
    a_cardtype.addTargetedChild("HDHOMERUN", new EmptyVBIDevice(m_parent));
    a_cardtype.addTargetedChild("HDHOMERUN", m_deviceId);

    auto *buttonRecOpt = new GroupSetting();
    buttonRecOpt->setLabel(tr("Recording Options"));
    buttonRecOpt->addChild(new SignalTimeout(m_parent, 3s, 0.25s));
    buttonRecOpt->addChild(new ChannelTimeout(m_parent, 6s, 1.75s));
    buttonRecOpt->addChild(new HDHomeRunEITScan(m_parent));
    a_cardtype.addTargetedChild("HDHOMERUN", buttonRecOpt);
};

void HDHomeRunConfigurationGroup::FillDeviceList(void)
{
    m_deviceList.clear();

    // Find physical devices first
    // ProbeVideoDevices returns "deviceid ip" pairs
    QStringList devs = CardUtil::ProbeVideoDevices("HDHOMERUN");

    for (const auto & dev : std::as_const(devs))
    {
        QStringList devinfo = dev.split(" ");
        const QString& devid = devinfo.at(0);
        const QString& devip = devinfo.at(1);
        const QString& model = devinfo.at(2);

        HDHomeRunDevice tmpdevice;
        tmpdevice.m_model    = model;
        tmpdevice.m_cardIp   = devip;
        tmpdevice.m_deviceId = devid;
        // Fully specify object.  Checkboxes will be added later when
        // the configuration group is created.
        tmpdevice.m_checkbox = nullptr;
        m_deviceList[tmpdevice.m_deviceId] = tmpdevice;
    }

#if 0
    // Debug dump of cards
    QMap<QString, HDHomeRunDevice>::iterator debugit;
    for (debugit = m_deviceList.begin(); debugit != m_deviceList.end(); ++debugit)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("%1: %2 %3")
            .arg(debugit.key()).arg((*debugit).model)
            .arg((*debugit).cardip));
    }
#endif
}

void HDHomeRunConfigurationGroup::SetDeviceCheckBoxes(const QString& devices)
{
    QStringList devstrs = devices.split(",");
    for (const QString& devstr : std::as_const(devstrs))
    {
        // Get the HDHomeRun device ID using libhdhomerun.  We need to
        // do it this way because legacy configurations could use an
        // IP address and a tuner nubmer.
        QByteArray ba = devstr.toUtf8();
        hdhomerun_device_t *device = hdhomerun_device_create_from_str(
            ba.data(), nullptr);
        if (!device)
            continue;
        QString devid = QString("%1").arg(
            hdhomerun_device_get_device_id(device), 8, 16).toUpper();
        hdhomerun_device_destroy(device);

        // If we know about this device, set its checkbox to on.
        QMap<QString, HDHomeRunDevice>::iterator dit;
        dit = m_deviceList.find(devid);
        if (dit != m_deviceList.end())
            (*dit).m_checkbox->setValue(true);
    }
}

QString HDHomeRunConfigurationGroup::GetDeviceCheckBoxes(void)
{
    // Return a string listing each HDHomeRun device with its checbox
    // turned on.
    QStringList devstrs;
    QMap<QString, HDHomeRunDevice>::iterator dit;
    for (dit = m_deviceList.begin(); dit != m_deviceList.end(); ++dit)
    {
        if ((*dit).m_checkbox->boolValue())
            devstrs << (*dit).m_deviceId;
    }
    QString devices = devstrs.join(",");
    return devices;
}

#endif

// -----------------------
// VBOX Configuration
// -----------------------

VBoxIP::VBoxIP()
{
    setLabel(QObject::tr("IP Address"));
    setHelpText(QObject::tr("Device IP or ID of a VBox device. eg. '192.168.1.100' or 'vbox_3718'"));
    VBoxIP::setEnabled(false);
    connect(this, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &VBoxIP::UpdateDevices);
};

void VBoxIP::setEnabled(bool e)
{
    MythUITextEditSetting::setEnabled(e);
    if (e)
    {
        if (!m_oldValue.isEmpty())
            setValue(m_oldValue);
        emit NewIP(getValue());
    }
    else
    {
        m_oldValue = getValue();
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
    VBoxTunerIndex::setEnabled(false);
    connect(this, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &VBoxTunerIndex::UpdateDevices);
};

void VBoxTunerIndex::setEnabled(bool e)
{
    MythUITextEditSetting::setEnabled(e);
    if (e) {
        if (!m_oldValue.isEmpty())
            setValue(m_oldValue);
        emit NewTuner(getValue());
    }
    else
    {
        m_oldValue = getValue();
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
    setReadOnly(true);
}

VBoxDeviceID::~VBoxDeviceID()
{
    delete GetStorage();
}

void VBoxDeviceID::SetIP(const QString &ip)
{
    m_ip = ip;
    setValue(QString("%1-%2").arg(m_ip, m_tuner));
}

void VBoxDeviceID::SetTuner(const QString &tuner)
{
    m_tuner = tuner;
    setValue(QString("%1-%2").arg(m_ip, m_tuner));
}

void VBoxDeviceID::SetOverrideDeviceID(const QString &deviceid)
{
    m_overrideDeviceId = deviceid;
    setValue(deviceid);
}

void VBoxDeviceID::Load(void)
{
    GetStorage()->Load();
    if (!m_overrideDeviceId.isEmpty())
    {
        setValue(m_overrideDeviceId);
        m_overrideDeviceId.clear();
    }
}

VBoxDeviceIDList::VBoxDeviceIDList(
    VBoxDeviceID        *deviceid,
    StandardSetting     *desc,
    VBoxIP              *cardip,
    VBoxTunerIndex      *cardtuner,
    VBoxDeviceList      *devicelist,
    const CaptureCard &parent) :
    m_deviceId(deviceid),
    m_desc(desc),
    m_cardIp(cardip),
    m_cardTuner(cardtuner),
    m_deviceList(devicelist),
    m_parent(parent)
{
    setLabel(QObject::tr("Available devices"));
    setHelpText(
        QObject::tr(
            "Device IP or ID, tuner number and tuner type of available VBox devices."));

    connect(this, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &VBoxDeviceIDList::UpdateDevices);
};

/// \brief Adds all available device-tuner combinations to list
void VBoxDeviceIDList::fillSelections(const QString &cur)
{
    clearSelections();

    std::vector<QString> devs;
    QMap<QString, bool> in_use;

    const QString& current = cur;

    for (auto it = m_deviceList->begin(); it != m_deviceList->end(); ++it)
    {
        devs.push_back(it.key());
        in_use[it.key()] = (*it).m_inUse;
    }

    QString man_addr = VBoxDeviceIDList::tr("Manually Enter IP Address");
    QString sel = man_addr;
    devs.push_back(sel);

    for (const auto & dev : devs)
        sel = (current == dev) ? dev : sel;

    QString usestr = QString(" -- ");
    usestr += QObject::tr("Warning: already in use");

    for (const auto & dev : devs)
    {
        QString desc = dev + (in_use[dev] ? usestr : "");
        addSelection(desc, dev, dev == sel);
    }

    if (current != cur)
    {
        m_deviceId->SetOverrideDeviceID(current);
    }
    else if (sel == man_addr && !current.isEmpty())
    {
        // Populate the proper values for IP address and tuner
        QStringList selection = current.split("-");

        m_cardIp->SetOldValue(selection.first());
        m_cardTuner->SetOldValue(selection.last());

        m_cardIp->setValue(selection.first());
        m_cardTuner->setValue(selection.last());
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
        m_cardIp->setEnabled(true);
        m_cardTuner->setEnabled(true);
    }
    else if (!v.isEmpty())
    {
        if (m_oldValue == VBoxDeviceIDList::tr("Manually Enter IP Address"))
        {
            m_cardIp->setEnabled(false);
            m_cardTuner->setEnabled(false);
        }
        m_deviceId->setValue(v);

        // Update _cardip and _cardtuner
        m_cardIp->setValue((*m_deviceList)[v].m_cardIp);
        m_cardTuner->setValue(QString("%1").arg((*m_deviceList)[v].m_tunerNo));
        m_desc->setValue((*m_deviceList)[v].m_desc);
    }
    m_oldValue = v;
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
            QObject::tr("URL of M3U file containing RTSP/RTP/UDP/HTTP channel URLs,"
            " example for HDHomeRun: http://<ipv4>/lineup.m3u and for Freebox:"
            " http://mafreebox.freebox.fr/freeboxtv/playlist.m3u."
            ));
    }
};

static void IPTVConfigurationGroup(CaptureCard& parent, CardType& cardType)
{
    cardType.addTargetedChild("FREEBOX", new IPTVHost(parent));
    cardType.addTargetedChild("FREEBOX", new ChannelTimeout(parent, 30s, 1.75s));
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
    /// if no device exists for it in /dev/asi*
    void fillSelections(const QString &current)
    {
        clearSelections();

        // Get devices from filesystem
        QStringList sdevs = CardUtil::ProbeVideoDevices("ASI");

        // Add current if needed
        if (!current.isEmpty() &&
            (std::find(sdevs.begin(), sdevs.end(), current) == sdevs.end()))
        {
            std::stable_sort(sdevs.begin(), sdevs.end());
        }

        // Get devices from DB
        QStringList db = CardUtil::GetVideoDevices("ASI");

        // Figure out which physical devices are already in use
        // by another card defined in the DB, and select a device
        // for new configs (preferring non-conflicing devices).
        QMap<QString,bool> in_use;
        QString sel = current;
        for (const QString& dev : std::as_const(sdevs))
        {
            in_use[dev] = std::find(db.begin(), db.end(), dev) != db.end();
            if (sel.isEmpty() && !in_use[dev])
                sel = dev;
        }

        // Unfortunately all devices are conflicted, select first device.
        if (sel.isEmpty() && !sdevs.empty())
            sel = sdevs[0];

        QString usestr = QString(" -- ");
        usestr += QObject::tr("Warning: already in use");

        // Add the devices to the UI
        bool found = false;
        for (const QString& dev : std::as_const(sdevs))
        {
            QString desc = dev + (in_use[dev] ? usestr : "");
            desc = (current == dev) ? dev : desc;
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

    void Load(void) override // StandardSetting
    {
        clearSelections();
        addSelection(QString());
        GetStorage()->Load();
        fillSelections(getValue());
    }
};

ASIConfigurationGroup::ASIConfigurationGroup(CaptureCard& a_parent,
                                             CardType &cardType):
    m_parent(a_parent),
    m_device(new ASIDevice(m_parent)),
    m_cardInfo(new TransTextEditSetting())
{
    setVisible(false);
    m_cardInfo->setLabel(tr("Status"));
    m_cardInfo->setEnabled(false);

    cardType.addTargetedChild("ASI", m_device);
    cardType.addTargetedChild("ASI", new EmptyAudioDevice(m_parent));
    cardType.addTargetedChild("ASI", new EmptyVBIDevice(m_parent));
    cardType.addTargetedChild("ASI", m_cardInfo);

    connect(m_device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,     &ASIConfigurationGroup::probeCard);

    probeCard(m_device->getValue());
};

void ASIConfigurationGroup::probeCard([[maybe_unused]] const QString &device)
{
#if CONFIG_ASI
    if (device.isEmpty())
    {
        m_cardInfo->setValue("");
        return;
    }

    if ((m_parent.getCardID() != 0) && m_parent.GetRawCardType() != "ASI")
    {
        m_cardInfo->setValue("");
        return;
    }

    QString error;
    int device_num = CardUtil::GetASIDeviceNumber(device, &error);
    if (device_num < 0)
    {
        m_cardInfo->setValue(tr("Not a valid DVEO ASI card"));
        LOG(VB_GENERAL, LOG_WARNING,
            "ASIConfigurationGroup::probeCard(), Warning: " + error);
        return;
    }
    m_cardInfo->setValue(tr("Valid DVEO ASI card"));
#else
    m_cardInfo->setValue(QString("Not compiled with ASI support"));
#endif
}

ImportConfigurationGroup::ImportConfigurationGroup(CaptureCard& a_parent,
                                                   CardType& a_cardtype):
    m_parent(a_parent),
    m_info(new GroupSetting()), m_size(new GroupSetting())
{
    setVisible(false);
    auto *device = new FileDevice(m_parent);
    device->setHelpText(tr("A local file used to simulate a recording."
                           " Leave empty to use MythEvents to trigger an"
                           " external program to import recording files."));
    a_cardtype.addTargetedChild("IMPORT", device);

    a_cardtype.addTargetedChild("IMPORT", new EmptyAudioDevice(m_parent));
    a_cardtype.addTargetedChild("IMPORT", new EmptyVBIDevice(m_parent));

    m_info->setLabel(tr("File info"));
    a_cardtype.addTargetedChild("IMPORT", m_info);

    m_size->setLabel(tr("File size"));
    a_cardtype.addTargetedChild("IMPORT", m_size);

    connect(device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   &ImportConfigurationGroup::probeCard);

    probeCard(device->getValue());
};

void ImportConfigurationGroup::probeCard(const QString &device)
{
    QString   ci;
    QString   cs;
    QFileInfo fileInfo(device);

    // For convenience, ImportRecorder allows both formats:
    if (device.startsWith("file:", Qt::CaseInsensitive))
        fileInfo.setFile(device.mid(5));

    if (fileInfo.exists())
    {
        if (fileInfo.isReadable() && (fileInfo.isFile()))
        {
            ci = HTTPRequest::TestMimeType(fileInfo.absoluteFilePath());
            cs = tr("%1 MB").arg(fileInfo.size() / 1024 / 1024);
        }
        else
        {
            ci = tr("File not readable");
        }
    }
    else
    {
        ci = tr("File %1 does not exist").arg(device);
    }

    m_info->setValue(ci);
    m_size->setValue(cs);
}

// -----------------------
// VBox Configuration
// -----------------------

VBoxConfigurationGroup::VBoxConfigurationGroup
        (CaptureCard& a_parent, CardType& a_cardtype) :
    m_parent(a_parent),
    m_desc(new GroupSetting()),
    m_deviceId(new VBoxDeviceID(a_parent)),
    m_cardIp(new VBoxIP()),
    m_cardTuner(new VBoxTunerIndex())
{
    setVisible(false);

    // Fill Device list
    FillDeviceList();

    m_desc->setLabel(tr("Description"));
    m_deviceIdList = new VBoxDeviceIDList(
        m_deviceId, m_desc, m_cardIp, m_cardTuner, &m_deviceList, m_parent);

    a_cardtype.addTargetedChild("VBOX", m_deviceIdList);
    a_cardtype.addTargetedChild("VBOX", new EmptyAudioDevice(m_parent));
    a_cardtype.addTargetedChild("VBOX", new EmptyVBIDevice(m_parent));
    a_cardtype.addTargetedChild("VBOX", m_deviceId);
    a_cardtype.addTargetedChild("VBOX", m_desc);
    a_cardtype.addTargetedChild("VBOX", m_cardIp);
    a_cardtype.addTargetedChild("VBOX", m_cardTuner);
    a_cardtype.addTargetedChild("VBOX", new SignalTimeout(m_parent, 7s, 1s));
    a_cardtype.addTargetedChild("VBOX", new ChannelTimeout(m_parent, 10s, 1.75s));

    connect(m_cardIp,    &VBoxIP::NewIP,
            m_deviceId,  &VBoxDeviceID::SetIP);
    connect(m_cardTuner, &VBoxTunerIndex::NewTuner,
            m_deviceId,  &VBoxDeviceID::SetTuner);
};

void VBoxConfigurationGroup::FillDeviceList(void)
{
    m_deviceList.clear();

    // Find physical devices first
    // ProbeVideoDevices returns "deviceid ip tunerno tunertype"
    QStringList devs = CardUtil::ProbeVideoDevices("VBOX");

    for (const auto & dev : std::as_const(devs))
    {
        QStringList devinfo = dev.split(" ");
        const QString& id = devinfo.at(0);
        const QString& ip = devinfo.at(1);
        const QString& tunerNo = devinfo.at(2);
        const QString& tunerType = devinfo.at(3);

        VBoxDevice tmpdevice;
        tmpdevice.m_deviceId   = id;
        tmpdevice.m_desc       = CardUtil::GetVBoxdesc(id, ip, tunerNo, tunerType);
        tmpdevice.m_cardIp     = ip;
        tmpdevice.m_inUse      = false;
        tmpdevice.m_discovered = true;
        tmpdevice.m_tunerNo  = tunerNo;
        tmpdevice.m_tunerType  = tunerType;
        tmpdevice.m_mythDeviceId = id + "-" + tunerNo + "-" + tunerType;
        m_deviceList[tmpdevice.m_mythDeviceId] = tmpdevice;
    }

    // Now find configured devices

    // returns "ip.ip.ip.ip-n-type" or deviceid-n-type values
    QStringList db = CardUtil::GetVideoDevices("VBOX");

    for (const auto & dev : std::as_const(db))
    {
        QMap<QString, VBoxDevice>::iterator dit;
        dit = m_deviceList.find(dev);

        if (dit != m_deviceList.end())
            (*dit).m_inUse = true;
    }
}

// -----------------------
// Ceton Configuration
// -----------------------
#if CONFIG_CETON
CetonSetting::CetonSetting(QString label, const QString& helptext)
{
    setLabel(std::move(label));
    setHelpText(helptext);
    connect(this, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &CetonSetting::UpdateDevices);
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
    m_parent(parent)
{
    setLabel(tr("Device ID"));
    setHelpText(tr("Device ID of Ceton device"));
}

CetonDeviceID::~CetonDeviceID()
{
    delete GetStorage();
}

void CetonDeviceID::SetIP(const QString &ip)
{
    static const QRegularExpression ipV4Regex
        { "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){4}$" };
    auto match = ipV4Regex.match(ip + ".");
    if (match.hasMatch())
    {
        m_ip = ip;
        setValue(QString("%1-RTP.%3").arg(m_ip, m_tuner));
    }
}

void CetonDeviceID::SetTuner(const QString &tuner)
{
    static const QRegularExpression oneDigit { "^\\d$" };
    auto match = oneDigit.match(tuner);
    if (match.hasMatch())
    {
        m_tuner = tuner;
        setValue(QString("%1-RTP.%2").arg(m_ip, m_tuner));
    }
}

void CetonDeviceID::Load(void)
{
    GetStorage()->Load();
    UpdateValues();
}

void CetonDeviceID::UpdateValues(void)
{
    static const QRegularExpression newstyle { R"(^([0-9.]+)-(\d|RTP)\.(\d)$)" };
    auto match = newstyle.match(getValue());
    if (match.hasMatch())
    {
        emit LoadedIP(match.captured(1));
        emit LoadedTuner(match.captured(3));
    }
}

void CetonSetting::CetonConfigurationGroup(CaptureCard& parent, CardType& cardtype)
{
    auto *deviceid = new CetonDeviceID(parent);
    auto *desc = new GroupSetting();
    desc->setLabel(tr("CetonConfigurationGroup", "Description"));
    auto *ip = new CetonSetting(tr("IP Address"),
        tr("IP Address of the Ceton device (192.168.200.1 by default)"));
    auto *tuner = new CetonSetting(tr("Tuner"),
        tr("Number of the tuner on the Ceton device (first tuner is number 0)"));

    cardtype.addTargetedChild("CETON", ip);
    cardtype.addTargetedChild("CETON", tuner);
    cardtype.addTargetedChild("CETON", deviceid);
    cardtype.addTargetedChild("CETON", desc);
    cardtype.addTargetedChild("CETON", new SignalTimeout(parent, 1s, 0.25s));
    cardtype.addTargetedChild("CETON", new ChannelTimeout(parent, 3s, 1.75s));

    QObject::connect(ip,       &CetonSetting::NewValue,
                     deviceid, &CetonDeviceID::SetIP);
    QObject::connect(tuner,    &CetonSetting::NewValue,
                     deviceid, &CetonDeviceID::SetTuner);

    QObject::connect(deviceid, &CetonDeviceID::LoadedIP,
                     ip,       &CetonSetting::LoadValue);
    QObject::connect(deviceid, &CetonDeviceID::LoadedTuner,
                     tuner,    &CetonSetting::LoadValue);
}
#endif

// Override database schema default, set schedgroup false
class SchedGroupFalse : public MythUICheckBoxSetting
{
  public:
    explicit SchedGroupFalse(const CaptureCard &parent) :
        MythUICheckBoxSetting(new CaptureCardDBStorage(this, parent,
                                                       "schedgroup"))
    {
        setValue(false);
        setVisible(false);
    };

    ~SchedGroupFalse() override
    {
        delete GetStorage();
    }
};

V4LConfigurationGroup::V4LConfigurationGroup(CaptureCard& parent,
                                             CardType& cardtype,
                                             const QString &inputtype) :
    m_parent(parent),
    m_cardInfo(new GroupSetting()),
    m_vbiDev(new VBIDevice(m_parent))
{
    setVisible(false);
    QRegularExpression drv { "^(?!ivtv|hdpvr|(saa7164(.*))).*$" };
    auto *device = new VideoDevice(m_parent, 0, 15, QString(), drv);

    m_cardInfo->setLabel(tr("Probed info"));
    m_cardInfo->setReadOnly(true);

    cardtype.addTargetedChild(inputtype, device);
    cardtype.addTargetedChild(inputtype, m_cardInfo);
    cardtype.addTargetedChild(inputtype, m_vbiDev);
    cardtype.addTargetedChild(inputtype, new AudioDevice(m_parent));
    cardtype.addTargetedChild(inputtype, new AudioRateLimit(m_parent));
    cardtype.addTargetedChild(inputtype, new SkipBtAudio(m_parent));

    // Override database schema default, set schedgroup false
    cardtype.addTargetedChild(inputtype, new SchedGroupFalse(m_parent));

    connect(device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   &V4LConfigurationGroup::probeCard);

    probeCard(device->getValue());
};

void V4LConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open");
    QString ci = cn;
    QString dn;

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

    m_cardInfo->setValue(ci);
    m_vbiDev->setFilter(cn, dn);
}

MPEGConfigurationGroup::MPEGConfigurationGroup(CaptureCard &parent,
                                               CardType &cardtype) :
    m_parent(parent),
    m_vbiDevice(new VBIDevice(parent)),
    m_cardInfo(new GroupSetting())
{
    setVisible(false);
    QRegularExpression drv { "^(ivtv|(saa7164(.*)))$" };
    m_device    = new VideoDevice(m_parent, 0, 15, QString(), drv);
    m_vbiDevice->setVisible(false);

    m_cardInfo->setLabel(tr("Probed info"));
    m_cardInfo->setReadOnly(true);

    cardtype.addTargetedChild("MPEG", m_device);
    cardtype.addTargetedChild("MPEG", m_vbiDevice);
    cardtype.addTargetedChild("MPEG", m_cardInfo);
    cardtype.addTargetedChild("MPEG", new ChannelTimeout(m_parent, 12s, 2s));

    // Override database schema default, set schedgroup false
    cardtype.addTargetedChild("MPEG", new SchedGroupFalse(m_parent));

    connect(m_device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,     &MPEGConfigurationGroup::probeCard);

    probeCard(m_device->getValue());
}

void MPEGConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open");
    QString ci = cn;
    QString dn;

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

    m_cardInfo->setValue(ci);
    m_vbiDevice->setVisible(dn!="ivtv");
    m_vbiDevice->setFilter(cn, dn);
}

DemoConfigurationGroup::DemoConfigurationGroup(CaptureCard &a_parent,
                                               CardType &a_cardtype) :
    m_parent(a_parent),
    m_info(new GroupSetting()), m_size(new GroupSetting())
{
    setVisible(false);
    auto *device = new FileDevice(m_parent);
    device->setHelpText(tr("A local MPEG file used to simulate a recording."));

    a_cardtype.addTargetedChild("DEMO", device);

    a_cardtype.addTargetedChild("DEMO", new EmptyAudioDevice(m_parent));
    a_cardtype.addTargetedChild("DEMO", new EmptyVBIDevice(m_parent));

    m_info->setLabel(tr("File info"));
    a_cardtype.addTargetedChild("DEMO", m_info);

    m_size->setLabel(tr("File size"));
    a_cardtype.addTargetedChild("DEMO", m_size);

    connect(device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   &DemoConfigurationGroup::probeCard);

    probeCard(device->getValue());
}

void DemoConfigurationGroup::probeCard(const QString &device)
{
    QString   ci;
    QString   cs;
    QFileInfo fileInfo(device);
    if (fileInfo.exists())
    {
        if (fileInfo.isReadable() && (fileInfo.isFile()))
        {
            ci = HTTPRequest::TestMimeType(fileInfo.absoluteFilePath());
            cs = tr("%1 MB").arg(fileInfo.size() / 1024 / 1024);
        }
        else
        {
            ci = tr("File not readable");
        }
    }
    else
    {
        ci = tr("File does not exist");
    }

    m_info->setValue(ci);
    m_size->setValue(cs);
}

#if !defined( _WIN32 )
ExternalConfigurationGroup::ExternalConfigurationGroup(CaptureCard &a_parent,
                                                       CardType &a_cardtype) :
    m_parent(a_parent),
    m_info(new GroupSetting())
{
    setVisible(false);
    auto *device = new CommandPath(m_parent);
    device->setLabel(tr("Command path"));
    device->setHelpText(tr("A 'black box' application controlled via stdin, status on "
                           "stderr and TransportStream read from stdout.\n"
                           "Use absolute path or path relative to the current directory."));
    a_cardtype.addTargetedChild("EXTERNAL", device);

    m_info->setLabel(tr("File info"));
    a_cardtype.addTargetedChild("EXTERNAL", m_info);

    a_cardtype.addTargetedChild("EXTERNAL",
                                new ChannelTimeout(m_parent, 20s, 1.75s));

    connect(device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   &ExternalConfigurationGroup::probeApp);

    probeApp(device->getValue());
}

void ExternalConfigurationGroup::probeApp(const QString & path)
{
    int idx1 = path.startsWith("file:", Qt::CaseInsensitive) ? 5 : 0;
    int idx2 = path.indexOf(' ', idx1);

    QString   ci;
    QFileInfo fileInfo(path.mid(idx1, idx2 - idx1));

    if (fileInfo.exists())
    {
        ci = tr("File '%1' is valid.").arg(fileInfo.absoluteFilePath());
        if (!fileInfo.isReadable() || !fileInfo.isFile())
            ci = tr("WARNING: File '%1' is not readable.")
                 .arg(fileInfo.absoluteFilePath());
        if (!fileInfo.isExecutable())
            ci = tr("WARNING: File '%1' is not executable.")
                 .arg(fileInfo.absoluteFilePath());
    }
    else
    {
        ci = tr("WARNING: File '%1' does not exist.")
             .arg(fileInfo.absoluteFilePath());
    }

    m_info->setValue(ci);
    m_info->setHelpText(ci);
}
#endif // !defined( _WIN32 )

HDPVRConfigurationGroup::HDPVRConfigurationGroup(CaptureCard &a_parent,
                                                 CardType &a_cardtype) :
    m_parent(a_parent), m_cardInfo(new GroupSetting()),
    m_audioInput(new TunerCardAudioInput(m_parent, QString(), "HDPVR"))
{
    setVisible(false);

    auto *device = new VideoDevice(m_parent, 0, 15, QString(),
                                   QRegularExpression("^hdpvr$"));

    m_cardInfo->setLabel(tr("Probed info"));
    m_cardInfo->setReadOnly(true);

    a_cardtype.addTargetedChild("HDPVR", device);
    a_cardtype.addTargetedChild("HDPVR", new EmptyAudioDevice(m_parent));
    a_cardtype.addTargetedChild("HDPVR", new EmptyVBIDevice(m_parent));
    a_cardtype.addTargetedChild("HDPVR", m_cardInfo);
    a_cardtype.addTargetedChild("HDPVR", m_audioInput);
    a_cardtype.addTargetedChild("HDPVR", new ChannelTimeout(m_parent, 15s, 2s));

    // Override database schema default, set schedgroup false
    a_cardtype.addTargetedChild("HDPVR", new SchedGroupFalse(m_parent));

    connect(device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   &HDPVRConfigurationGroup::probeCard);

    probeCard(device->getValue());
}

void HDPVRConfigurationGroup::probeCard(const QString &device)
{
    QString cn = tr("Failed to open");
    QString ci = cn;
    QString dn;

    int videofd = open(device.toLocal8Bit().constData(), O_RDWR);
    if (videofd >= 0)
    {
        if (!CardUtil::GetV4LInfo(videofd, cn, dn))
            ci = cn = tr("Failed to probe");
        else if (!dn.isEmpty())
            ci = cn + "  [" + dn + "]";
        close(videofd);
        m_audioInput->fillSelections(device);
    }

    m_cardInfo->setValue(ci);
}

V4L2encGroup::V4L2encGroup(CaptureCard &parent, CardType& cardtype) :
    m_parent(parent),
    m_cardInfo(new GroupSetting())
{
    setVisible(false);

    m_device = new VideoDevice(m_parent, 0, 15);

    setLabel(QObject::tr("V4L2 encoder devices (multirec capable)"));

    m_cardInfo->setLabel(tr("Probed info"));
    m_cardInfo->setReadOnly(true);

    cardtype.addTargetedChild("V4L2ENC", m_device);
    cardtype.addTargetedChild("V4L2ENC", m_cardInfo);

    // Override database schema default, set schedgroup false
    cardtype.addTargetedChild("V4L2ENC", new SchedGroupFalse(m_parent));

    connect(m_device, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,     &V4L2encGroup::probeCard);

    const QString &device_name = m_device->getValue();
    if (!device_name.isEmpty())
        probeCard(device_name);
}

void V4L2encGroup::probeCard([[maybe_unused]] const QString &device_name)
{
#if CONFIG_V4L2
    QString    card_name = tr("Failed to open");
    QString    card_info = card_name;
    V4L2util   v4l2(device_name);

    if (!v4l2.IsOpen())
    {
        m_driverName = tr("Failed to probe");
        return;
    }
    m_driverName = v4l2.DriverName();
    card_name = v4l2.CardName();

    if (!m_driverName.isEmpty())
        card_info = card_name + "  [" + m_driverName + "]";

    m_cardInfo->setValue(card_info);

    if (m_device->getSubSettings()->empty())
    {
        auto* audioinput = new TunerCardAudioInput(m_parent, QString(), "V4L2");
        if (audioinput->fillSelections(device_name) > 1)
        {
            audioinput->setName("AudioInput");
            m_device->addTargetedChild(m_driverName, audioinput);
        }
        else
        {
            delete audioinput;
        }

        if (v4l2.HasSlicedVBI())
        {
            auto* vbidev = new VBIDevice(m_parent);
            if (vbidev->setFilter(card_name, m_driverName) > 0)
            {
                vbidev->setName("VBIDevice");
                m_device->addTargetedChild(m_driverName, vbidev);
            }
            else
            {
                delete vbidev;
            }
        }

        m_device->addTargetedChild(m_driverName, new EmptyVBIDevice(m_parent));
        m_device->addTargetedChild(m_driverName,
                                   new ChannelTimeout(m_parent, 15s, 2s));
    }
#endif // CONFIG_V4L2
}

CaptureCardGroup::CaptureCardGroup(CaptureCard &parent)
{
    setLabel(QObject::tr("Capture Card Setup"));

    auto* cardtype = new CardType(parent);
    parent.addChild(cardtype);

#if CONFIG_DVB
    cardtype->addTargetedChild("DVB",
                               new DVBConfigurationGroup(parent, *cardtype));
#endif // CONFIG_DVB

#if CONFIG_V4L2
    cardtype->addTargetedChild("HDPVR",
                               new HDPVRConfigurationGroup(parent, *cardtype));
#endif // CONFIG_V4L2

#if CONFIG_HDHOMERUN
    cardtype->addTargetedChild("HDHOMERUN",
                               new HDHomeRunConfigurationGroup(parent, *cardtype));
#endif // CONFIG_HDHOMERUN

#if CONFIG_VBOX
    cardtype->addTargetedChild("VBOX",
                               new VBoxConfigurationGroup(parent, *cardtype));
#endif // CONFIG_VBOX

#if CONFIG_SATIP
    cardtype->addTargetedChild("SATIP",
                               new SatIPConfigurationGroup(parent, *cardtype));
#endif // CONFIG_SATIP

#if CONFIG_FIREWIRE
    FirewireConfigurationGroup(parent, *cardtype);
#endif // CONFIG_FIREWIRE

#if CONFIG_CETON
    CetonSetting::CetonConfigurationGroup(parent, *cardtype);
#endif // CONFIG_CETON

#if CONFIG_IPTV
    IPTVConfigurationGroup(parent, *cardtype);
#endif // CONFIG_IPTV

#if CONFIG_V4L2
    cardtype->addTargetedChild("V4L2ENC", new V4L2encGroup(parent, *cardtype));
    cardtype->addTargetedChild("V4L",
                               new V4LConfigurationGroup(parent, *cardtype, "V4L"));
    cardtype->addTargetedChild("MJPEG",
                               new V4LConfigurationGroup(parent, *cardtype, "MJPEG"));
    cardtype->addTargetedChild("GO7007",
                               new V4LConfigurationGroup(parent, *cardtype, "GO7007"));
    cardtype->addTargetedChild("MPEG",
                               new MPEGConfigurationGroup(parent, *cardtype));
#endif // CONFIG_V4L2

#if CONFIG_ASI
    cardtype->addTargetedChild("ASI",
                               new ASIConfigurationGroup(parent, *cardtype));
#endif // CONFIG_ASI

    // for testing without any actual tuner hardware:
    cardtype->addTargetedChild("IMPORT",
                               new ImportConfigurationGroup(parent, *cardtype));
    cardtype->addTargetedChild("DEMO",
                               new DemoConfigurationGroup(parent, *cardtype));
#if !defined( _WIN32 )
    cardtype->addTargetedChild("EXTERNAL",
                               new ExternalConfigurationGroup(parent,
                                                              *cardtype));
#endif
}

CaptureCard::CaptureCard(bool use_card_group)
    : m_id(new ID)
{
    addChild(m_id);
    if (use_card_group)
        CaptureCardGroup(*this);
    addChild(new Hostname(*this));
}

QString CaptureCard::GetRawCardType(void) const
{
    int cardid = getCardID();
    if (cardid <= 0)
        return {};
    return CardUtil::GetRawInputType(cardid);
}

void CaptureCard::fillSelections(GroupSetting *setting)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT cardid, videodevice, cardtype, displayname "
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
        QString displayname = query.value(3).toString();

        QString label = QString("%1 (%2)")
            .arg(CardUtil::GetDeviceLabel(cardtype, videodevice), displayname);

        auto *card = new CaptureCard();
        card->loadByID(cardid);
        card->setLabel(label);
        setting->addChild(card);
    }
}

void CaptureCard::loadByID(int cardid)
{
    m_id->setValue(cardid);
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
    QString init_dev = CardUtil::GetVideoDevice(init_cardid);

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
        std::vector<uint> clones = CardUtil::GetChildInputIDs(cardid);
        for (uint clone : clones)
            CardUtil::CloneCard(cardid, clone);
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
#if CONFIG_DVB
    setting->addSelection(
        QObject::tr("DVB-T/S/C, ATSC or ISDB-T tuner card"), "DVB");
#endif // CONFIG_DVB

#if CONFIG_V4L2
    setting->addSelection(
        QObject::tr("V4L2 encoder"), "V4L2ENC");
    setting->addSelection(
        QObject::tr("HD-PVR H.264 encoder"), "HDPVR");
#endif // CONFIG_V4L2

#if CONFIG_HDHOMERUN
    setting->addSelection(
        QObject::tr("HDHomeRun networked tuner"), "HDHOMERUN");
#endif // CONFIG_HDHOMERUN

#if CONFIG_SATIP
    setting->addSelection(
        QObject::tr("Sat>IP networked tuner"), "SATIP");
#endif // CONFIG_SATIP

#if CONFIG_VBOX
    setting->addSelection(
        QObject::tr("V@Box TV Gateway networked tuner"), "VBOX");
#endif // CONFIG_VBOX

#if CONFIG_FIREWIRE
    setting->addSelection(
        QObject::tr("FireWire cable box"), "FIREWIRE");
#endif // CONFIG_FIREWIRE

#if CONFIG_CETON
    setting->addSelection(
        QObject::tr("Ceton Cablecard tuner"), "CETON");
#endif // CONFIG_CETON

#if CONFIG_IPTV
    setting->addSelection(QObject::tr("IPTV recorder"), "FREEBOX");
#endif // CONFIG_IPTV

#if CONFIG_V4L2
    setting->addSelection(
        QObject::tr("Analog to MPEG-2 encoder card (PVR-150/250/350, etc)"), "MPEG");
    setting->addSelection(
        QObject::tr("Analog to MJPEG encoder card (Matrox G200, DC10, etc)"), "MJPEG");
    setting->addSelection(
        QObject::tr("Analog to MPEG-4 encoder (Plextor ConvertX USB, etc)"),
        "GO7007");
    setting->addSelection(
        QObject::tr("Analog capture card"), "V4L");
#endif // CONFIG_V4L2

#if CONFIG_ASI
    setting->addSelection(QObject::tr("DVEO ASI recorder"), "ASI");
#endif

    setting->addSelection(QObject::tr("Import test recorder"), "IMPORT");
    setting->addSelection(QObject::tr("Demo test recorder"),   "DEMO");
#if !defined( _WIN32 )
    setting->addSelection(QObject::tr("External (black box) recorder"),
                          "EXTERNAL");
#endif
}

CaptureCard::Hostname::Hostname(const CaptureCard &parent) :
    StandardSetting(new CaptureCardDBStorage(this, parent, "hostname"))
{
    setVisible(false);
    setValue(gCoreContext->GetHostName());
}

CaptureCard::Hostname::~Hostname()
{
    delete GetStorage();
}

class InputName : public MythUIComboBoxSetting
{
  public:
    explicit InputName(const CardInput &parent) :
        MythUIComboBoxSetting(new CardInputDBStorage(this, parent, "inputname"))
    {
        setLabel(QObject::tr("Input name"));
    };

    ~InputName() override
    {
        delete GetStorage();
    }

    void Load(void) override // StandardSetting
    {
        fillSelections();
        MythUIComboBoxSetting::Load();
    };

    void fillSelections() {
        clearSelections();
        addSelection(QObject::tr("(None)"), "None");
        auto *storage = dynamic_cast<CardInputDBStorage*>(GetStorage());
        if (storage == nullptr)
            return;
        uint cardid = storage->getInputID();
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

class DeliverySystem : public GroupSetting
{
  public:
    DeliverySystem()
    {
        setLabel(QObject::tr("Delivery system"));
        setHelpText(QObject::tr(
                        "This shows the delivery system (modulation), for instance DVB-T2, "
                        "that you have selected when you configured the capture card. "
                        "This must be the same as the modulation used by the video source. "));
    };
};

class InputDisplayName : public MythUITextEditSetting
{
    Q_DECLARE_TR_FUNCTIONS(InputDisplayName);

  public:
    explicit InputDisplayName(const CardInput &parent) :
        MythUITextEditSetting(new CardInputDBStorage(this, parent, "displayname")), m_parent(parent)
    {
        setLabel(QObject::tr("Display name"));
        setHelpText(QObject::tr(
                        "This name is displayed on screen when Live TV begins "
                        "and in various other places.  Make sure the last two "
                        "characters are unique for each input or use a "
                        "slash ('/') to designate the unique portion."));
    };

    ~InputDisplayName() override
    {
        delete GetStorage();
    }
    void Load(void) override {
        MythUITextEditSetting::Load();
        if (getValue().isEmpty())
            setValue(tr("Input %1").arg(m_parent.getInputID()));
    }
  private:
    const CardInput &m_parent;
};

class CardInputComboBoxSetting : public MythUIComboBoxSetting
{
  public:
    CardInputComboBoxSetting(const CardInput &parent, const QString &setting) :
        MythUIComboBoxSetting(new CardInputDBStorage(this, parent, setting))
    {
    }

    ~CardInputComboBoxSetting() override
    {
        delete GetStorage();
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

    void Load(void) override // StandardSetting
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
        m_cardInput(parent),
        m_groupNum(group_num)
    {
        setLabel(QObject::tr("Input group") +
                 QString(" %1").arg(m_groupNum + 1));
        setHelpText(QObject::tr(
                        "Leave as 'Generic' unless this input is shared with "
                        "another device. Only one of the inputs in an input "
                        "group will be allowed to record at any given time."));
    }

    void Load(void) override; // StandardSetting

    void Save(void) override // StandardSetting
    {
        uint inputid     = m_cardInput.getInputID();
        uint new_groupid = getValue().toUInt();

        if (m_groupId && (m_groupId != new_groupid))
            CardUtil::UnlinkInputGroup(inputid, m_groupId);

        if (new_groupid)
            CardUtil::LinkInputGroup(inputid, new_groupid);
    }

    virtual void Save(const QString& /*destination*/) { Save(); }

  private:
    const CardInput &m_cardInput;
    uint             m_groupNum;
    uint             m_groupId   {0};
};

void InputGroup::Load(void)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("InputGroup::Load() %1 %2")
            .arg(m_groupNum).arg(m_cardInput.getInputID()));
#endif

    uint             inputid = m_cardInput.getInputID();
    QMap<uint, uint> grpcnt;
    std::vector<QString>  names;
    std::vector<uint>     grpid;
    std::vector<uint>     selected_groupids;

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
            if ((inputid != 0U) && (query.value(0).toUInt() == inputid))
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
    m_groupId = 0;
    if (m_groupNum < selected_groupids.size())
        m_groupId = selected_groupids[m_groupNum];

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("Group num: %1 id: %2")
            .arg(m_groupNum).arg(m_groupId));
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
    for (size_t i = 0; i < names.size(); i++)
    {
        bool sel = (m_groupId == grpid[i]);
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

    ~ExternalChannelCommand() override
    {
        delete GetStorage();
    }
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

    ~PresetTuner() override
    {
        delete GetStorage();
    }
};

void StartingChannel::SetSourceID(const QString &sourceid)
{
    clearSelections();
    if (sourceid.isEmpty() || !sourceid.toUInt())
        return;

    // Get the existing starting channel
    auto *storage = dynamic_cast<CardInputDBStorage*>(GetStorage());
    if (storage == nullptr)
        return;
    int inputId = storage->getInputID();
    QString startChan = CardUtil::GetStartChannel(inputId);

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
    for (size_t i = 0; i < channels.size() && !has_visible; i++)
        has_visible |= channels[i].m_visible;

    for (auto & channel : channels)
    {
        const QString channum = channel.m_chanNum;
        bool sel = channum == startChan;
        if (!has_visible || channel.m_visible || sel)
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

    ~InputPriority() override
    {
        delete GetStorage();
    }
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

    ~ScheduleOrder() override
    {
        delete GetStorage();
    }
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

    ~LiveTVOrder() override
    {
        delete GetStorage();
    }
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

    ~DishNetEIT() override
    {
        delete GetStorage();
    }
};

CardInput::CardInput(const QString & cardtype, const QString & device,
                     int _cardid) :
    m_id(new ID()),
    m_inputName(new InputName(*this)),
    m_sourceId(new SourceID(*this)),
    m_startChan(new StartingChannel(*this)),
    m_scan(new ButtonStandardSetting(tr("Scan for channels"))),
    m_srcFetch(new ButtonStandardSetting(tr("Fetch channels from listings source"))),
    m_externalInputSettings(new DiSEqCDevSettings()),
    m_inputGrp0(new InputGroup(*this, 0)),
    m_inputGrp1(new InputGroup(*this, 1))
{
    addChild(m_id);

    if (CardUtil::IsInNeedOfExternalInputConf(_cardid))
    {
        addChild(new DTVDeviceConfigGroup(*m_externalInputSettings,
                                          _cardid, true));
    }

    // Delivery system for DVB, input name for other,
    // same field capturecard/inputname for both
    if ("DVB" == cardtype)
    {
        auto *ds = new DeliverySystem();
        ds->setValue(CardUtil::GetDeliverySystemFromDB(_cardid));
        addChild(ds);
    }
    else if (CardUtil::IsV4L(cardtype))
    {
        addChild(m_inputName);
    }
    addChild(new InputDisplayName(*this));
    addChild(m_sourceId);

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

    m_scan->setHelpText(
        tr("Use channel scanner to find channels for this input."));

    m_srcFetch->setHelpText(
        tr("This uses the listings data source to "
           "provide the channels for this input.") + " " +
        tr("This can take a long time to run."));

    addChild(m_scan);
    addChild(m_srcFetch);

    addChild(m_startChan);

    auto *interact = new GroupSetting();

    interact->setLabel(QObject::tr("Interactions between inputs"));
    if (CardUtil::IsTunerSharingCapable(cardtype))
    {
        m_instanceCount = new InstanceCount(*this);
        interact->addChild(m_instanceCount);
        m_schedGroup = new SchedGroup(*this);
        interact->addChild(m_schedGroup);
    }
    interact->addChild(new InputPriority(*this));
    interact->addChild(new ScheduleOrder(*this, _cardid));
    interact->addChild(new LiveTVOrder(*this, _cardid));

    auto *ingrpbtn =
        new ButtonStandardSetting(QObject::tr("Create a New Input Group"));
    ingrpbtn->setHelpText(
        QObject::tr("Input groups are only needed when two or more cards "
                    "share the same resource such as a FireWire card and "
                    "an analog card input controlling the same set top box."));
    interact->addChild(ingrpbtn);
    interact->addChild(m_inputGrp0);
    interact->addChild(m_inputGrp1);

    addChild(interact);

    setObjectName("CardInput");
    SetSourceID("-1");

    connect(m_scan,     &ButtonStandardSetting::clicked, this, &CardInput::channelScanner);
    connect(m_srcFetch, &ButtonStandardSetting::clicked, this, &CardInput::sourceFetch);
    connect(m_sourceId, qOverload<const QString&>(&StandardSetting::valueChanged),
            m_startChan,&StartingChannel::SetSourceID);
    connect(m_sourceId, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,       &CardInput::SetSourceID);
    connect(ingrpbtn,   &ButtonStandardSetting::clicked,
            this,       &CardInput::CreateNewInputGroup);
}

CardInput::~CardInput()
{
    if (m_externalInputSettings)
    {
        delete m_externalInputSettings;
        m_externalInputSettings = nullptr;
    }
}

void CardInput::SetSourceID(const QString &sourceid)
{
    uint cid = m_id->getValue().toUInt();
    QString raw_card_type = CardUtil::GetRawInputType(cid);
    bool enable = (sourceid.toInt() > 0);
    m_scan->setEnabled(enable && !raw_card_type.isEmpty() &&
                     !CardUtil::IsUnscanable(raw_card_type));
    m_srcFetch->setEnabled(enable);
}

QString CardInput::getSourceName(void) const
{
    return m_sourceId->getValueLabel();
}

void CardInput::CreateNewInputGroup(void)
{
    m_inputGrp0->Save();
    m_inputGrp1->Save();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *settingdialog =
        new MythTextInputDialog(popupStack, tr("Enter new group name"));

    if (settingdialog->Create())
    {
        connect(settingdialog, &MythTextInputDialog::haveResult,
                this, &CardInput::CreateNewInputGroupSlot);
        popupStack->AddScreen(settingdialog);
    }
    else
    {
        delete settingdialog;
    }
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

    m_inputGrp0->Load();
    m_inputGrp1->Load();

    if (m_inputGrp0->getValue().toUInt() == 0U)
    {
        m_inputGrp0->setValue(
            m_inputGrp0->getValueIndex(QString::number(inputgroupid)));
    }
    else
    {
        m_inputGrp1->setValue(
            m_inputGrp1->getValueIndex(QString::number(inputgroupid)));
    }
}

void CardInput::channelScanner(void)
{
    uint srcid = m_sourceId->getValue().toUInt();
    uint crdid = m_id->getValue().toUInt();
    QString in = m_inputName->getValue();

#if CONFIG_BACKEND
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
    auto *ssd = new StandardSettingDialog(mainStack, "generalsettings",
                                          new ScanWizard(srcid, crdid, in));

    if (ssd->Create())
    {
        connect(ssd, &StandardSettingDialog::Exiting, this,
                [srcid, this, num_channels_before]()
                {
                    if (SourceUtil::GetChannelCount(srcid))
                        m_startChan->SetSourceID(QString::number(srcid));
                    if (num_channels_before)
                    {
                        m_startChan->Load();
                        m_startChan->Save();
                    }
                });
        mainStack->AddScreen(ssd);
    }
    else
    {
        delete ssd;
    }

#else
    LOG(VB_GENERAL, LOG_ERR, "You must compile the backend "
                             "to be able to scan for channels");
#endif
}

void CardInput::sourceFetch(void)
{
    uint srcid = m_sourceId->getValue().toUInt();
    uint crdid = m_id->getValue().toUInt();

    uint num_channels_before = SourceUtil::GetChannelCount(srcid);

    if (crdid && srcid)
    {
        Save(); // save info for fetch..

        QString cardtype = CardUtil::GetRawInputType(crdid);

        if (!CardUtil::IsCableCardPresent(crdid, cardtype) &&
            !CardUtil::IsUnscanable(cardtype) &&
            !CardUtil::IsEncoder(cardtype)    &&
            cardtype != "HDHOMERUN"           &&
            !num_channels_before)
        {
            LOG(VB_GENERAL, LOG_ERR, "Skipping channel fetch, you need to "
                                     "scan for channels first.");
            return;
        }

        SourceUtil::UpdateChannelsFromListings(srcid, cardtype);
    }

    if (SourceUtil::GetChannelCount(srcid))
        m_startChan->SetSourceID(QString::number(srcid));
    if (num_channels_before)
    {
        m_startChan->Load();
        m_startChan->Save();
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
    bindings.insert(colTag, m_user->GetDBValue());

    return query;
}

void CardInput::loadByID(int inputid)
{
    m_id->setValue(inputid);
    m_externalInputSettings->Load(inputid);
    GroupSetting::Load();
}

void CardInput::loadByInput(int _cardid, const QString& _inputname)
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
    uint cardid = m_id->getValue().toUInt();
    GroupSetting::Save();
    m_externalInputSettings->Store(getInputID());

    uint icount = 1;
    if (m_instanceCount)
        icount = m_instanceCount->getValue().toUInt();

    CardUtil::InputSetMaxRecordings(cardid, icount);
}

int CardInputDBStorage::getInputID(void) const
{
    return m_parent.getInputID();
}

int CaptureCardDBStorage::getCardID(void) const
{
    return m_parent.getCardID();
}

void CaptureCardButton::edit(MythScreenType * /*screen*/)
{
    emit Clicked(m_value);
}

void CaptureCardEditor::AddSelection(const QString &label, const CCESlot slot)
{
    auto *button = new ButtonStandardSetting(label);
    connect(button, &ButtonStandardSetting::clicked, this, slot);
    addChild(button);
}

void CaptureCardEditor::AddSelection(const QString &label, const CCESlotConst slot)
{
    auto *button = new ButtonStandardSetting(label);
    connect(button, &ButtonStandardSetting::clicked, this, slot);
    addChild(button);
}

void CaptureCardEditor::ShowDeleteAllCaptureCardsDialogOnHost() const
{
    ShowOkPopup(
        tr("Are you sure you want to delete "
           "ALL capture cards on %1?").arg(gCoreContext->GetHostName()),
        this, &CaptureCardEditor::DeleteAllCaptureCardsOnHost,
        true);
}

void CaptureCardEditor::ShowDeleteAllCaptureCardsDialog() const
{
    ShowOkPopup(
        tr("Are you sure you want to delete "
           "ALL capture cards?"),
        this, &CaptureCardEditor::DeleteAllCaptureCards,
        true);
}

void CaptureCardEditor::AddNewCard()
{
    auto *card = new CaptureCard();
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
    AddSelection(QObject::tr("(New capture card)"), &CaptureCardEditor::AddNewCard);
    AddSelection(QObject::tr("(Delete all capture cards on %1)")
                 .arg(gCoreContext->GetHostName()),
                 &CaptureCardEditor::ShowDeleteAllCaptureCardsDialogOnHost);
    AddSelection(QObject::tr("(Delete all capture cards)"),
                 &CaptureCardEditor::ShowDeleteAllCaptureCardsDialog);
    CaptureCard::fillSelections(this);
}

VideoSourceEditor::VideoSourceEditor()
{
    setLabel(tr("Video sources"));
}

void VideoSourceEditor::Load(void)
{
    clearSettings();
    AddSelection(QObject::tr("(New video source)"), &VideoSourceEditor::NewSource);
    AddSelection(QObject::tr("(Delete all video sources)"),
                 &VideoSourceEditor::ShowDeleteAllSourcesDialog);
    VideoSource::fillSelections(this);
    GroupSetting::Load();
}

void VideoSourceEditor::AddSelection(const QString &label, const VSESlot slot)
{
    auto *button = new ButtonStandardSetting(label);
    connect(button, &ButtonStandardSetting::clicked, this, slot);
    addChild(button);
}

void VideoSourceEditor::AddSelection(const QString &label, const VSESlotConst slot)
{
    auto *button = new ButtonStandardSetting(label);
    connect(button, &ButtonStandardSetting::clicked, this, slot);
    addChild(button);
}

void VideoSourceEditor::ShowDeleteAllSourcesDialog(void) const
{
    ShowOkPopup(
       tr("Are you sure you want to delete "
          "ALL video sources?"),
       this, &VideoSourceEditor::DeleteAllSources,
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
    auto *source = new VideoSource();
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
    m_cardInputs.clear();
    clearSettings();

    // We do this manually because we want custom labels.  If
    // SelectSetting provided a facility to edit the labels, we
    // could use CaptureCard::fillSelections

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, videodevice, cardtype, displayname "
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
        QString displayname = query.value(3).toString();

        auto *cardinput = new CardInput(cardtype, videodevice, cardid);
        cardinput->loadByID(cardid);
        QString inputlabel = QString("%1 (%2) -> %3")
            .arg(CardUtil::GetDeviceLabel(cardtype, videodevice),
                 displayname, cardinput->getSourceName());
        m_cardInputs.push_back(cardinput);
        cardinput->setLabel(inputlabel);
        addChild(cardinput);
    }

    GroupSetting::Load();
}

#if CONFIG_DVB
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
    else if (short_name.startsWith("nxt2002", Qt::CaseInsensitive) ||
             short_name.startsWith("nxt200x", Qt::CaseInsensitive))
        short_name = "Air2PC v2";
    else if (short_name.startsWith("lgdt3302", Qt::CaseInsensitive))
        short_name = "DViCO HDTV3";
    else if (short_name.startsWith("lgdt3303", Qt::CaseInsensitive))
        short_name = "DViCO v2 or Air2PC v3 or pcHDTV HD-5500";

    return short_name;
}
#endif // CONFIG_DVB

void DVBConfigurationGroup::reloadDiseqcTree(const QString &videodevice)
{
    if (m_diseqcTree)
        m_diseqcTree->Load(videodevice);

    if (m_cardType->getValue() == "DVB-S" ||
        m_cardType->getValue() == "DVB-S2" )
    {
        m_diseqcBtn->setVisible(true);
    }
    else
    {
        m_diseqcBtn->setVisible(false);
    }
    emit getParent()->settingsChanged(this);
}

void DVBConfigurationGroup::probeCard(const QString &videodevice)
{
    if (videodevice.isEmpty())
    {
        m_cardName->setValue("");
        m_cardType->setValue("");
        return;
    }

    if ((m_parent.getCardID() != 0) && m_parent.GetRawCardType() != "DVB")
    {
        m_cardName->setValue("");
        m_cardType->setValue("");
        return;
    }

#if CONFIG_DVB
    QString frontend_name = CardUtil::ProbeDVBFrontendName(videodevice);
    QString subtype = CardUtil::ProbeDVBType(videodevice);

    QString err_open  = tr("Could not open card %1").arg(videodevice);
    QString err_other = tr("Could not get card info for card %1").arg(videodevice);

    switch (CardUtil::toInputType(subtype))
    {
        case CardUtil::INPUT_TYPES::ERROR_OPEN:
            m_cardName->setValue(err_open);
            m_cardType->setValue(strerror(errno));
            break;
        case CardUtil::INPUT_TYPES::ERROR_UNKNOWN:
            m_cardName->setValue(err_other);
            m_cardType->setValue("Unknown error");
            break;
        case CardUtil::INPUT_TYPES::ERROR_PROBE:
            m_cardName->setValue(err_other);
            m_cardType->setValue(strerror(errno));
            break;
        case CardUtil::INPUT_TYPES::QPSK:
            m_cardType->setValue("DVB-S");
            m_cardName->setValue(frontend_name);
            m_signalTimeout->setValueMs(7s);
            m_channelTimeout->setValueMs(10s);
            break;
        case CardUtil::INPUT_TYPES::DVBS2:
            m_cardType->setValue("DVB-S2");
            m_cardName->setValue(frontend_name);
            m_signalTimeout->setValueMs(7s);
            m_channelTimeout->setValueMs(10s);
            break;
        case CardUtil::INPUT_TYPES::QAM:
            m_cardType->setValue("DVB-C");
            m_cardName->setValue(frontend_name);
            m_signalTimeout->setValueMs(3s);
            m_channelTimeout->setValueMs(6s);
            break;
        case CardUtil::INPUT_TYPES::DVBT2:
            m_cardType->setValue("DVB-T2");
            m_cardName->setValue(frontend_name);
            m_signalTimeout->setValueMs(3s);
            m_channelTimeout->setValueMs(6s);
            break;
        case CardUtil::INPUT_TYPES::OFDM:
        {
            m_cardType->setValue("DVB-T");
            m_cardName->setValue(frontend_name);
            m_signalTimeout->setValueMs(3s);
            m_channelTimeout->setValueMs(6s);
            if (frontend_name.toLower().indexOf("usb") >= 0)
            {
                m_signalTimeout->setValueMs(40s);
                m_channelTimeout->setValueMs(42.5s);
            }

            // slow down tuning for buggy drivers
            if ((frontend_name == "DiBcom 3000P/M-C DVB-T") ||
                (frontend_name ==
                 "TerraTec/qanu USB2.0 Highspeed DVB-T Receiver"))
            {
                m_tuningDelay->setValueMs(200ms);
            }

#if 0 // frontends on hybrid DVB-T/Analog cards
            QString short_name = remove_chaff(frontend_name);
            m_buttonAnalog->setVisible(
                short_name.startsWith("zarlink zl10353",
                                      Qt::CaseInsensitive) ||
                short_name.startsWith("wintv hvr 900 m/r: 65008/a1c0",
                                      Qt::CaseInsensitive) ||
                short_name.startsWith("philips tda10046h",
                                      Qt::CaseInsensitive));
#endif
        }
        break;
        case CardUtil::INPUT_TYPES::ATSC:
        {
            QString short_name = remove_chaff(frontend_name);
            m_cardType->setValue("ATSC");
            m_cardName->setValue(short_name);
            m_signalTimeout->setValueMs(2s);
            m_channelTimeout->setValueMs(4s);

            // According to #1779 and #1935 the AverMedia 180 needs
            // a 3000 ms signal timeout, at least for QAM tuning.
            if (frontend_name == "Nextwave NXT200X VSB/QAM frontend")
            {
                m_signalTimeout->setValueMs(3s);
                m_channelTimeout->setValueMs(5.5s);
            }

#if 0 // frontends on hybrid DVB-T/Analog cards
            if (frontend_name.toLower().indexOf("usb") < 0)
            {
                m_buttonAnalog->setVisible(
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

    // Create selection list of all delivery systems of this card
    {
        m_cardType->clearSelections();
        QStringList delsyslist = CardUtil::ProbeDeliverySystems(videodevice);
        for (const auto & item : std::as_const(delsyslist))
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("DVBCardType: add deliverysystem:%1")
                .arg(item));

            m_cardType->addSelection(item, item);
        }

        // Default value, used if not already defined in capturecard/inputname
        QString delsys = CardUtil::ProbeDefaultDeliverySystem(videodevice);
        if (!delsys.isEmpty())
        {
            m_cardType->setValue(delsys);
        }
    }
#
#else
    m_cardType->setValue(QString("Recompile with DVB-Support!"));
#endif
}

TunerCardAudioInput::TunerCardAudioInput(const CaptureCard &parent,
                                         QString dev, QString type) :
    CaptureCardComboBoxSetting(parent, false, "audiodevice"),
    m_lastDevice(std::move(dev)), m_lastCardType(std::move(type))
{
    setLabel(QObject::tr("Audio input"));
    setHelpText(QObject::tr("If there is more than one audio input, "
                            "select which one to use."));
    int cardid = parent.getCardID();
    if (cardid <= 0)
        return;

    m_lastCardType = CardUtil::GetRawInputType(cardid);
    m_lastDevice   = CardUtil::GetAudioDevice(cardid);
}

int TunerCardAudioInput::fillSelections(const QString &device)
{
    clearSelections();

    if (device.isEmpty())
        return 0;

    m_lastDevice = device;
    QStringList inputs =
        CardUtil::ProbeAudioInputs(device, m_lastCardType);

    for (uint i = 0; i < (uint)inputs.size(); i++)
    {
        addSelection(inputs[i], QString::number(i),
                     m_lastDevice == QString::number(i));
    }
    return inputs.size();
}

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent,
                                             CardType& cardType) :
    m_parent(a_parent),
    m_cardNum(new DVBCardNum(a_parent)),
    m_cardName(new DVBCardName()),
    m_cardType(new DVBCardType(a_parent)),
    m_signalTimeout(new SignalTimeout(a_parent, 0.5s, 0.25s)),
    m_tuningDelay(new DVBTuningDelay(a_parent)),
    m_diseqcTree(new DiSEqCDevTree()),
    m_diseqcBtn(new DeviceTree(*m_diseqcTree))
{
    setVisible(false);

    m_channelTimeout = new ChannelTimeout(m_parent, 3s, 1.75s);

    cardType.addTargetedChild("DVB", m_cardNum);

    cardType.addTargetedChild("DVB", m_cardName);
    cardType.addTargetedChild("DVB", m_cardType);

    cardType.addTargetedChild("DVB", m_signalTimeout);
    cardType.addTargetedChild("DVB", m_channelTimeout);

    cardType.addTargetedChild("DVB", new EmptyAudioDevice(m_parent));
    cardType.addTargetedChild("DVB", new EmptyVBIDevice(m_parent));

    cardType.addTargetedChild("DVB", new DVBNoSeqStart(m_parent));
    cardType.addTargetedChild("DVB", new DVBOnDemand(m_parent));
    cardType.addTargetedChild("DVB", new DVBEITScan(m_parent));

    m_diseqcBtn->setLabel(tr("DiSEqC (Switch, LNB and Rotor Configuration)"));
    m_diseqcBtn->setHelpText(tr("Input and satellite settings."));

    cardType.addTargetedChild("DVB", m_tuningDelay);
    cardType.addTargetedChild("DVB", m_diseqcBtn);
    m_tuningDelay->setVisible(false);

    connect(m_cardNum,    qOverload<const QString&>(&StandardSetting::valueChanged),
            this,         &DVBConfigurationGroup::probeCard);
    connect(m_cardNum,    qOverload<const QString&>(&StandardSetting::valueChanged),
            this,         &DVBConfigurationGroup::reloadDiseqcTree);
}

DVBConfigurationGroup::~DVBConfigurationGroup()
{
    if (m_diseqcTree)
    {
        delete m_diseqcTree;
        m_diseqcTree = nullptr;
    }
}

void DVBConfigurationGroup::Load(void)
{
    reloadDiseqcTree(m_cardNum->getValue());
    m_diseqcBtn->Load();
    GroupSetting::Load();
    if (m_cardType->getValue() == "DVB-S" ||
        m_cardType->getValue() == "DVB-S2" ||
        DiSEqCDevTree::Exists(m_parent.getCardID()))
    {
        m_diseqcBtn->setVisible(true);
    }
}

void DVBConfigurationGroup::Save(void)
{
    GroupSetting::Save();
    m_diseqcTree->Store(m_parent.getCardID(), m_cardNum->getValue());
    DiSEqCDev::InvalidateTrees();
}

// -----------------------
// SAT>IP configuration
// -----------------------
#if CONFIG_SATIP

class DiSEqCPosition : public MythUISpinBoxSetting
{
  public:
    explicit DiSEqCPosition(const CaptureCard &parent, int value, int min_val) :
        MythUISpinBoxSetting(new CaptureCardDBStorage(this, parent, "dvb_diseqc_type"),
                             min_val, 0xff, 1)
    {
       setLabel(QObject::tr("DiSEqC position"));
       setHelpText(QObject::tr("Position of the LNB on the DiSEqC switch. "
                               "Leave at 1 if there is no DiSEqC switch "
                               "and the LNB is directly connected to the SatIP server. "
                               "This value is used as signal source (attribute src) in "
                               "the SatIP tune command."));
       setValue(value);
    };

    ~DiSEqCPosition() override
    {
        delete GetStorage();
    }
};

SatIPConfigurationGroup::SatIPConfigurationGroup
        (CaptureCard& a_parent, CardType &a_cardtype) :
    m_parent(a_parent),
    m_deviceId(new SatIPDeviceID(a_parent))
{
    setVisible(false);

    FillDeviceList();

    m_friendlyName = new SatIPDeviceAttribute(tr("Friendly name"), tr("Friendly name of the Sat>IP server"));
    m_tunerType    = new SatIPDeviceAttribute(tr("Tuner type"),    tr("Type of the selected tuner"));
    m_tunerIndex   = new SatIPDeviceAttribute(tr("Tuner index"),   tr("Index of the tuner on the Sat>IP server"));

    m_deviceIdList = new SatIPDeviceIDList(
        m_deviceId, m_friendlyName, m_tunerType, m_tunerIndex, &m_deviceList, m_parent);

    a_cardtype.addTargetedChild("SATIP", m_deviceIdList);
    a_cardtype.addTargetedChild("SATIP", m_friendlyName);
    a_cardtype.addTargetedChild("SATIP", m_tunerType);
    a_cardtype.addTargetedChild("SATIP", m_tunerIndex);
    a_cardtype.addTargetedChild("SATIP", m_deviceId);
    a_cardtype.addTargetedChild("SATIP", new SignalTimeout(m_parent, 7s, 1s));
    a_cardtype.addTargetedChild("SATIP", new ChannelTimeout(m_parent, 10s, 2s));
    a_cardtype.addTargetedChild("SATIP", new DVBEITScan(m_parent));
    a_cardtype.addTargetedChild("SATIP", new DiSEqCPosition(m_parent, 1, 1));

    connect(m_deviceIdList, &SatIPDeviceIDList::NewTuner,
            m_deviceId,     &SatIPDeviceID::SetTuner);
};

void SatIPConfigurationGroup::FillDeviceList(void)
{
    m_deviceList.clear();

    // Find devices on the network
    // Returns each devices as "deviceid friendlyname ip tunerno tunertype"
    QStringList devs = CardUtil::ProbeVideoDevices("SATIP");

    for (const auto & dev : std::as_const(devs))
    {
        QStringList devparts = dev.split(" ");
        const QString& id = devparts.value(0);
        const QString& name = devparts.value(1);
        const QString& ip = devparts.value(2);
        const QString& tunerno = devparts.value(3);
        const QString& tunertype = devparts.value(4);

        SatIPDevice device;
        device.m_deviceId = id;
        device.m_cardIP = ip;
        device.m_inUse = false;
        device.m_friendlyName = name;
        device.m_tunerNo = tunerno;
        device.m_tunerType = tunertype;
        device.m_mythDeviceId = QString("%1:%2:%3").arg(id, tunertype, tunerno);

        QString friendlyIdentifier = QString("%1, %2, Tuner #%3").arg(name, tunertype, tunerno);

        m_deviceList[device.m_mythDeviceId] = device;

        LOG(VB_CHANNEL, LOG_DEBUG, QString("SatIP: Add %1 '%2' '%3'")
            .arg(device.m_mythDeviceId, device.m_friendlyName, friendlyIdentifier));
    }

    // Now find configured devices
    // Returns each devices as "deviceid friendlyname ip tunerno tunertype"
    QStringList db = CardUtil::GetVideoDevices("SATIP");
    for (const auto& dev : std::as_const(db))
    {
        auto dit = m_deviceList.find(dev);
        if (dit != m_deviceList.end())
        {
            (*dit).m_inUse = true;
        }
    }
};

SatIPDeviceIDList::SatIPDeviceIDList(
    SatIPDeviceID        *deviceId,
    SatIPDeviceAttribute *friendlyName,
    SatIPDeviceAttribute *tunerType,
    SatIPDeviceAttribute *tunerIndex,
    SatIPDeviceList      *deviceList,
    const CaptureCard    &parent) :
    m_deviceId(deviceId),
    m_friendlyName(friendlyName),
    m_tunerType(tunerType),
    m_tunerIndex(tunerIndex),
    m_deviceList(deviceList),
    m_parent(parent)
{
    setLabel(tr("Available devices"));
    setHelpText(tr("Device IP or ID, tuner number and tuner type of available Sat>IP device"));

    connect(this, qOverload<const QString&>(&StandardSetting::valueChanged),
            this, &SatIPDeviceIDList::UpdateDevices);
};

void SatIPDeviceIDList::Load(void)
{
    clearSelections();

    int cardid = m_parent.getCardID();
    QString device = CardUtil::GetVideoDevice(cardid);

    fillSelections(device);
};

void SatIPDeviceIDList::UpdateDevices(const QString &v)
{
    SatIPDevice dev = (*m_deviceList)[v];
    m_deviceId->setValue(dev.m_mythDeviceId);
    m_friendlyName->setValue(dev.m_friendlyName);
    m_tunerType->setValue(dev.m_tunerType);
    m_tunerIndex->setValue(dev.m_tunerNo);
};

void SatIPDeviceIDList::fillSelections(const QString &cur)
{
    clearSelections();

    std::vector<QString> names;
    std::vector<QString> devs;
    QMap<QString, bool> in_use;

    const QString& current = cur;
    QString sel;

    SatIPDeviceList::iterator it = m_deviceList->begin();
    for(; it != m_deviceList->end(); ++it)
    {
        QString friendlyIdentifier = QString("%1, %2, Tuner #%3")
            .arg((*it).m_friendlyName, (*it).m_tunerType, (*it).m_tunerNo);
        names.push_back(friendlyIdentifier);

        devs.push_back(it.key());
        in_use[it.key()] = (*it).m_inUse;
    }

    for (const auto& it2s : devs)
    {
        sel = (current == it2s) ? it2s : sel;
    }

    QString usestr = QString(" -- ");
    usestr += tr("Warning: already in use");

    for (uint i = 0; i < devs.size(); ++i)
    {
        const QString& dev = devs[i];
        const QString& name = names[i];
        bool dev_in_use = (dev == sel) ? false : in_use[devs[i]];
        QString desc = name + (dev_in_use ? usestr : "");
        addSelection(desc, dev, dev == sel);
    }
};

SatIPDeviceID::SatIPDeviceID(const CaptureCard &parent) :
    MythUITextEditSetting(new CaptureCardDBStorage(this, parent, "videodevice")),
    m_parent(parent)
{
    setLabel(tr("Device ID"));
    setHelpText(tr("Device ID of the Sat>IP tuner"));
    setEnabled(true);
    setReadOnly(true);
};

SatIPDeviceID::~SatIPDeviceID()
{
    delete GetStorage();
}

void SatIPDeviceID::Load(void)
{
    MythUITextEditSetting::Load();
};

void SatIPDeviceID::SetTuner(const QString &tuner)
{
    setValue(tuner);
};

SatIPDeviceAttribute::SatIPDeviceAttribute(const QString& label, const QString& helptext)
{
    setLabel(label);
    setHelpText(helptext);
};
#endif // CONFIG_SATIP
