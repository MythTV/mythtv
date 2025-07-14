// C/C++ headers
#include <utility>

// Qt headers
#include <QCoreApplication>
#include <QFile>
#include <QWidget>

// MythTV headers
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h" // for COMM_DETECT*, GetPreferredSkipTypeCombinations()

#include "cardutil.h"
#include "channelsettings.h"
#include "channelutil.h"
#include "mpeg/mpegtables.h"

void ChannelID::Save()
{
    if (getValue().toInt() == 0) {
        setValue(findHighest());

        MSqlQuery query(MSqlQuery::InitCon());

        QString querystr = QString("SELECT %1 FROM %2 WHERE %3='%4'")
                         .arg(m_field, m_table, m_field, getValue());
        query.prepare(querystr);

        if (!query.exec() && !query.isActive())
            MythDB::DBError("ChannelID::save", query);

        if (query.size())
            return;

        querystr = QString("INSERT INTO %1 (%2) VALUES ('%3')")
                         .arg(m_table, m_field, getValue());
        query.prepare(querystr);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("ChannelID::save", query);

        if (query.numRowsAffected() != 1)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("ChannelID, Error: ") +
                    QString("Failed to insert into: %1").arg(m_table));
        }
    }
}

QString ChannelDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":WHERE" + m_id.getField().toUpper());
    QString query(m_id.getField() + " = " + fieldTag);

    bindings.insert(fieldTag, m_id.getValue());

    return query;
}

QString ChannelDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":SET" + m_id.getField().toUpper());
    QString nameTag = (":SET" + GetColumnName().toUpper());

    QString query(m_id.getField() + " = " + fieldTag + ", " +
                  GetColumnName() + " = " + nameTag);

    bindings.insert(fieldTag, m_id.getValue());
    bindings.insert(nameTag, m_user->GetDBValue());

    return query;
}

QString IPTVChannelDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":WHERE" + m_id.getField().toUpper());
    QString query(m_id.getField() + " = " + fieldTag);

    bindings.insert(fieldTag, m_id.getValue());

    return query;
}

QString IPTVChannelDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":SET" + m_id.getField().toUpper());
    QString nameTag = (":SET" + GetColumnName().toUpper());

    QString query(m_id.getField() + " = " + fieldTag + ", " +
                  GetColumnName() + " = " + nameTag);

    bindings.insert(fieldTag, m_id.getValue());
    bindings.insert(nameTag, m_user->GetDBValue());

    return query;
}

/*****************************************************************************
        Channel Options - Common
 *****************************************************************************/

class Name : public MythUITextEditSetting
{
  public:
    explicit Name(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "name"))
    {
        setLabel(QCoreApplication::translate("(Common)", "Channel Name"));
    }

    ~Name() override
    {
        delete GetStorage();
    }
};

class Channum : public MythUITextEditSetting
{
  public:
    explicit Channum(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "channum"))
    {
        setLabel(QCoreApplication::translate("(Common)", "Channel Number"));
        setHelpText(QCoreApplication::translate("(Common)",
        "This is the number by which the channel is known to MythTV."));
    }

    ~Channum() override
    {
        delete GetStorage();
    }
};

class Source : public MythUIComboBoxSetting
{
  public:
    Source(const ChannelID &id, uint _default_sourceid) :
        MythUIComboBoxSetting(new ChannelDBStorage(this, id, "sourceid")),
        m_defaultSourceId(_default_sourceid)
    {
        setLabel(QCoreApplication::translate("(Common)", "Video Source"));
        setHelpText(QCoreApplication::translate("(Common)",
        "It is NOT a good idea to change this value as it only changes "
        "the sourceid in table channel but not in dtv_multiplex. "
        "The sourceid in dtv_multiplex cannot and should not be changed."));
    }

    ~Source() override
    {
        delete GetStorage();
    }

    void Load(void) override // StandardSetting
    {
        fillSelections();
        StandardSetting::Load();

        if ((m_defaultSourceId != 0U) && (getValue().toUInt() == 0U))
        {
            uint which = m_sourceIdToIndex[m_defaultSourceId];
            if (which)
                setValue(which);
        }
    }

    void fillSelections(void)
    {
        addSelection(QCoreApplication::translate("(ChannelSettings)",
                                                 "[Not Selected]"), "0");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT name, sourceid "
                      "FROM videosource "
                      "ORDER BY sourceid");

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("Source::fillSelections", query);
        }
        else
        {
            for (uint i = 1; query.next(); i++)
            {
                m_sourceIdToIndex[query.value(1).toUInt()] = i;
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
            }
        }

        m_sourceIdToIndex[0] = 0; // Not selected entry.
    }

  private:
    uint            m_defaultSourceId;
    QMap<uint,uint> m_sourceIdToIndex;
};

class Callsign : public MythUITextEditSetting
{
  public:
    explicit Callsign(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "callsign"))
    {
        setLabel(QCoreApplication::translate("(Common)", "Callsign"));
    }

    ~Callsign() override
    {
        delete GetStorage();
    }
};

ChannelTVFormat::ChannelTVFormat(const ChannelID &id) :
    MythUIComboBoxSetting(new ChannelDBStorage(this, id, "tvformat"))
{
    setLabel(QCoreApplication::translate("(ChannelSettings)", "TV Format"));
    setHelpText(QCoreApplication::translate("(ChannelSettings)",
        "If this channel uses a format other than TV Format in the General "
        "Backend Setup screen, set it here."));

    addSelection(QCoreApplication::translate("(Common)", "Default"), "Default");

    QStringList list = GetFormats();
    for (const QString& format : std::as_const(list))
        addSelection(format);
}

ChannelTVFormat::~ChannelTVFormat()
{
    delete GetStorage();
}

QStringList ChannelTVFormat::GetFormats(void)
{
    QStringList list;

    list.push_back("NTSC");
    list.push_back("NTSC-JP");
    list.push_back("PAL");
    list.push_back("PAL-60");
    list.push_back("PAL-BG");
    list.push_back("PAL-DK");
    list.push_back("PAL-D");
    list.push_back("PAL-I");
    list.push_back("PAL-M");
    list.push_back("PAL-N");
    list.push_back("PAL-NC");
    list.push_back("SECAM");
    list.push_back("SECAM-D");
    list.push_back("SECAM-DK");

    return list;
}

class TimeOffset : public MythUISpinBoxSetting
{
  public:
    explicit TimeOffset(const ChannelID &id) :
        MythUISpinBoxSetting(new ChannelDBStorage(this, id, "tmoffset"),
                             -1440, 1440, 1)
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "DataDirect Time Offset"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Offset (in minutes) to apply to the program guide data during "
            "import.  This can be used when the listings for a particular "
            "channel are in a different time zone."));
    }

    ~TimeOffset() override
    {
        delete GetStorage();
    }
};

class Priority : public MythUISpinBoxSetting
{
  public:
    explicit Priority(const ChannelID &id) :
        MythUISpinBoxSetting(new ChannelDBStorage(this, id, "recpriority"),
                             -99, 99, 1)
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "Priority"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Number of priority points to be added to any recording on this "
            "channel during scheduling. Use a positive number as the priority "
            "if you want this to be a preferred channel, a negative one to "
            "depreciate this channel."));
    }

    ~Priority() override
    {
        delete GetStorage();
    }
};

class Icon : public MythUITextEditSetting
{
  public:
    explicit Icon(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "icon"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "Icon"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Image file to use as the icon for this channel on various MythTV "
            "displays."));
    }

    ~Icon() override
    {
        delete GetStorage();
    }
};

class VideoFilters : public MythUITextEditSetting
{
  public:
    explicit VideoFilters(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "videofilters"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "Video filters"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Filters to be used when recording from this channel.  Not used "
            "with hardware encoding cards."));

    }

    ~VideoFilters() override
    {
        delete GetStorage();
    }
};


class OutputFilters : public MythUITextEditSetting
{
  public:
    explicit OutputFilters(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "outputfilters"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                    "Playback filters"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Filters to be used when recordings from this channel are viewed. "
            "Start with a plus to append to the global playback filters."));
    }

    ~OutputFilters() override
    {
        delete GetStorage();
    }
};

class XmltvID : public MythUIComboBoxSetting
{
  public:
    XmltvID(const ChannelID &id, QString _sourceName) :
        MythUIComboBoxSetting(new ChannelDBStorage(this, id, "xmltvid"), true),
        m_sourceName(std::move(_sourceName))
    {
        setLabel(QCoreApplication::translate("(Common)", "XMLTV ID"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "ID used by listing services to get an exact correspondence "
            "between a channel in your line-up and a channel in their "
            "database. Normally this is set automatically when "
            "'mythfilldatabase' is run."));
    }

    ~XmltvID() override
    {
        delete GetStorage();
    }

    void Load(void) override // StandardSetting
    {
        fillSelections();
        StandardSetting::Load();
    }

    void fillSelections(void)
    {
        clearSelections();

        QString xmltvFile = GetConfDir() + '/' + m_sourceName + ".xmltv";

        if (QFile::exists(xmltvFile))
        {
            QFile file(xmltvFile);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                return;

            QStringList idList;

            while (!file.atEnd())
            {
                QByteArray line = file.readLine();

                if (line.startsWith("channel="))
                {
                    QString id = line.mid(8, -1).trimmed();
                    idList.append(id);
                }
            }

            idList.sort();

            for (const QString& idName : std::as_const(idList))
                addSelection(idName, idName);
        }
    }

  private:
    QString m_sourceName;
};

class ServiceID : public MythUISpinBoxSetting
{
  public:
    explicit ServiceID(const ChannelID &id)
        : MythUISpinBoxSetting(new ChannelDBStorage(this, id, "serviceid"),
                               -1, UINT16_MAX, 1, 1, "NULL")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "Service ID"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
                "Service ID (Program Number) of desired channel within the transport stream. "
                "If there is only one channel, then setting this to anything will still find it."));
    }

    ~ServiceID() override
    {
        delete GetStorage();
    }

    void Load(void) override // StandardSetting
    {
        StandardSetting::Load();

        if (getValue().isNull())
            setValue("-1");
    }

    QString getValue(void) const override // StandardSetting
    {
        if (StandardSetting::getValue().toInt() == -1)
            return {};
        return StandardSetting::getValue();
    }
};

// Transport ID in Channel Options
class TransportID_CO : public GroupSetting
{
  public:
    TransportID_CO(void)
    {
        setLabel(QObject::tr("Transport ID"));
        setHelpText(
            QObject::tr("The transport stream ID (tid) can be used to identify "
                "the transport of this channel in the Transport Editor."));
    }
};

// Frequency in Channel Options
class Frequency_CO : public GroupSetting
{
  public:
    Frequency_CO(void)
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(
            QObject::tr("Frequency of the transport of this channel in Hz for "
                "DVB-T/T2/C or in kHz plus polarization H or V for DVB-S/S2."));
    }
};

class CommMethod : public MythUIComboBoxSetting
{
  public:
    explicit CommMethod(const ChannelID &id) :
       MythUIComboBoxSetting(new ChannelDBStorage(this, id, "commmethod"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "Commercial Detection Method"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Changes the method of commercial detection used for recordings on "
            "this channel or skips detection by marking the channel as "
            "Commercial Free."));

        std::deque<int> tmp = GetPreferredSkipTypeCombinations();
        tmp.push_front(COMM_DETECT_UNINIT);
        tmp.push_back(COMM_DETECT_COMMFREE);

        for (int pref : tmp)
            addSelection(SkipTypeToString(pref), QString::number(pref));
    }

    ~CommMethod() override
    {
        delete GetStorage();
    }
};

class Visible : public MythUIComboBoxSetting
{
  public:
    explicit Visible(const ChannelID &id) :
        MythUIComboBoxSetting(new ChannelDBStorage(this, id, "visible"))
    {
        setValue(kChannelVisible);

        setLabel(QCoreApplication::translate("(ChannelSettings)", "Visible"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "If set to Always Visible or Visible, the channel will be visible in the "
            "EPG.  Set to Always Visible or Never Visible to prevent MythTV and other "
            "utilities from automatically managing the value for this "
            "channel."));

        addSelection(QCoreApplication::translate("(Common)", "Always Visible"),
                     QString::number(kChannelAlwaysVisible));
        addSelection(QCoreApplication::translate("(Common)", "Visible"),
                     QString::number(kChannelVisible));
        addSelection(QCoreApplication::translate("(Common)", "Not Visible"),
                     QString::number(kChannelNotVisible));
        addSelection(QCoreApplication::translate("(Common)", "Never Visible"),
                     QString::number(kChannelNeverVisible));
    }

    ~Visible() override
    {
        delete GetStorage();
    }
};

class OnAirGuide : public MythUICheckBoxSetting
{
  public:
    explicit OnAirGuide(const ChannelID &id) :
        MythUICheckBoxSetting(new ChannelDBStorage(this, id, "useonairguide"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "Use on air guide"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "If enabled, guide information for this channel will be updated "
            "using 'Over-the-Air' program listings."));
    }

    ~OnAirGuide() override
    {
        delete GetStorage();
    }
};

/*****************************************************************************
        Channel Options - IPTV
 *****************************************************************************/
class ChannelURL : public MythUITextEditSetting
{
  public:
    explicit ChannelURL(const ChannelID &id) :
        MythUITextEditSetting(new IPTVChannelDBStorage(this, id, "url"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "URL"));
        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "URL for streaming of this channel. Used by the IPTV "
            "capture card and obtained with an \"M3U Import\" or "
            "with a \"HDHomeRun Channel Import\" loading of an XML file."));
    }

    ~ChannelURL() override
    {
        delete GetStorage();
    }
};

/*****************************************************************************
        Channel Options - Video 4 Linux
 *****************************************************************************/

class Freqid : public MythUITextEditSetting
{
  public:
    explicit Freqid(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "freqid"))
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "Freq/Channel"));
        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "N.B. This setting is only used for analog channels. "
            "Depending on the tuner type, specify either the exact "
            "frequency (in kHz) or a valid channel "
            "number that will be understood by your tuners."));
    }

    ~Freqid() override
    {
        delete GetStorage();
    }
};

class Finetune : public MythUISpinBoxSetting
{
  public:
    explicit Finetune(const ChannelID& id)
        : MythUISpinBoxSetting(new ChannelDBStorage(this, id, "finetune"),
                               -300, 300, 1)
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "Finetune (kHz)"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Value to be added to your desired frequency (in kHz) for "
            "'fine tuning'."));

        setValue("0");
    }

    ~Finetune() override
    {
        delete GetStorage();
    }
};

class Contrast : public MythUISpinBoxSetting
{
  public:
    explicit Contrast(const ChannelID &id) :
        MythUISpinBoxSetting(new ChannelDBStorage(this, id, "contrast"),
                             0, 65535, 655)
    {
        setLabel(QCoreApplication::translate("(Common)", "Contrast"));
    }

    ~Contrast() override
    {
        delete GetStorage();
    }
};

class Brightness : public MythUISpinBoxSetting
{
  public:
    explicit Brightness(const ChannelID &id) :
        MythUISpinBoxSetting(new ChannelDBStorage(this, id, "brightness"),
                             0, 65535, 655)
    {
        setLabel(QCoreApplication::translate("(Common)", "Brightness"));
    }

    ~Brightness() override
    {
        delete GetStorage();
    }
};

class Colour : public MythUISpinBoxSetting
{
  public:
    explicit Colour(const ChannelID &id) :
        MythUISpinBoxSetting(new ChannelDBStorage(this, id, "colour"),
                             0, 65535, 655)
    {
        setLabel(QCoreApplication::translate("(Common)", "Color"));
    }

    ~Colour() override
    {
        delete GetStorage();
    }
};

class Hue : public MythUISpinBoxSetting
{
  public:
    explicit Hue(const ChannelID &id) :
        MythUISpinBoxSetting(new ChannelDBStorage(this, id, "hue"),
                             0, 65535, 655)
    {
        setLabel(QCoreApplication::translate("(Common)", "Hue"));
    }

    ~Hue() override
    {
        delete GetStorage();
    }
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID &id,
          uint default_sourceid, bool add_freqid)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Common"));
    addChild(new Name(id));

    auto *source = new Source(id, default_sourceid);

    auto *channum = new Channum(id);
    addChild(channum);
    if (add_freqid)
    {
        m_freqId = new Freqid(id);
        addChild(m_freqId);
    }
    else
    {
        m_freqId = nullptr;
    }
    addChild(new Callsign(id));



    addChild(new Visible(id));
    addChild(new ServiceID(id));

    addChild(m_transportId = new TransportID_CO());
    addChild(m_frequency = new Frequency_CO());

    addChild(source);
    addChild(new ChannelTVFormat(id));
    addChild(new Priority(id));

    addChild(m_onAirGuide = new OnAirGuide(id));
    addChild(m_xmltvID = new XmltvID(id, source->getValueLabel()));
    addChild(new TimeOffset(id));

    addChild(new CommMethod(id));
    addChild(new Icon(id));

    connect(m_onAirGuide, &MythUICheckBoxSetting::valueChanged,
            this,         &ChannelOptionsCommon::onAirGuideChanged);
    connect(source,       qOverload<const QString&>(&StandardSetting::valueChanged),
            this,         &ChannelOptionsCommon::sourceChanged);

    // Transport stream ID and frequency from dtv_multiplex
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT transportid, frequency, polarity, mod_sys FROM dtv_multiplex "
        "JOIN channel ON channel.mplexid = dtv_multiplex.mplexid "
        "WHERE channel.chanid = :CHANID");

    query.bindValue(":CHANID", id.getValue().toUInt());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ChannelOptionsCommon::ChannelOptionsCommon", query);
    }
    else if (query.next())
    {
        QString frequency = query.value(1).toString();
        DTVModulationSystem modSys;
        modSys.Parse(query.value(3).toString());
        if ((modSys == DTVModulationSystem::kModulationSystem_DVBS) ||
            (modSys == DTVModulationSystem::kModulationSystem_DVBS2))
        {
            QString polarization = query.value(2).toString().toUpper();
            frequency.append(polarization);
        }
        m_transportId->setValue(query.value(0).toString());
        m_frequency->setValue(frequency);
    }
};

void ChannelOptionsCommon::onAirGuideChanged([[maybe_unused]] bool fValue)
{
}

void ChannelOptionsCommon::sourceChanged(const QString& sourceid)
{
    bool supports_eit  = true;
    bool uses_eit_only = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard, videosource "
                  "WHERE capturecard.sourceid   = videosource.sourceid AND "
                  "      videosource.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("sourceChanged -- supports eit", query);
    else
    {
        supports_eit = (query.size() == 0);
        while (query.next())
        {
            supports_eit |= CardUtil::IsEITCapable(
                query.value(0).toString().toUpper());
        }

        query.prepare("SELECT xmltvgrabber "
                      "FROM videosource "
                      "WHERE sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", sourceid);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("sourceChanged -- eit only", query);
        else
        {
            uses_eit_only = (query.size() != 0);
            while (query.next())
            {
                uses_eit_only &= (query.value(0).toString() == "eitonly");
            }
        }
    }

    m_onAirGuide->setEnabled(supports_eit);
    m_xmltvID->setEnabled(!uses_eit_only);
    m_xmltvID->Load();
}

ChannelOptionsFilters::ChannelOptionsFilters(const ChannelID& id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Filters"));

    addChild(new VideoFilters(id));
    addChild(new OutputFilters(id));
}

ChannelOptionsIPTV::ChannelOptionsIPTV(const ChannelID& id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - IPTV"));

    addChild(new Name(id));
    addChild(new Channum(id));
    addChild(new ChannelURL(id));
}

ChannelOptionsV4L::ChannelOptionsV4L(const ChannelID& id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Video4Linux"));

    addChild(new Freqid(id));
    addChild(new Finetune(id));
    addChild(new Contrast(id));
    addChild(new Brightness(id));
    addChild(new Colour(id));
    addChild(new Hue(id));
};

/*****************************************************************************
        Channel Options - Video 4 Linux
 *****************************************************************************/

ChannelOptionsRawTS::ChannelOptionsRawTS(const ChannelID &id) :
    m_cid(id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
        "Channel Options - Raw Transport Stream"));

    const uint mx = kMaxPIDs;
    m_pids.resize(mx);
    m_sids.resize(mx);
    m_pcrs.resize(mx);

    for (uint i = 0; i < mx; i++)
    {
        addChild((m_pids[i] = new TransTextEditSetting()));
        m_pids[i]->setLabel("PID");
        addChild((m_sids[i] = new TransMythUIComboBoxSetting()));
        m_sids[i]->setLabel("    StreamID");
        for (uint j = 0x101; j <= 0x1ff; j++)
        {
            QString desc = StreamID::GetDescription(j&0xff);
            if (!desc.isEmpty())
            {
                m_sids[i]->addSelection(
                    QString("%1 (0x%2)")
                    .arg(desc).arg(j&0xff,2,16,QLatin1Char('0')),
                    QString::number(j), false);
            }
        }
        for (uint j = 0x101; j <= 0x1ff; j++)
        {
            QString desc = StreamID::GetDescription(j&0xff);
            if (desc.isEmpty())
            {
                m_sids[i]->addSelection(
                    QString("0x%1").arg(j&0xff,2,16,QLatin1Char('0')),
                    QString::number(j), false);
            }
        }
/* we don't allow tables right now, PAT & PMT generated on the fly
        for (uint j = 0; j <= 0xff; j++)
        {
            QString desc = TableID::GetDescription(j);
            if (!desc.isEmpty())
            {
                m_sids[i]->addSelection(
                    QString("%1 (0x%2)").arg(j,0,16,QLatin1Char('0')),
                    QString::number(j),
                    false);
            }
        }
*/
        addChild((m_pcrs[i] = new TransMythUICheckBoxSetting()));
        m_pcrs[i]->setLabel("    Is PCR");
    }
};

void ChannelOptionsRawTS::Load(void)
{
    uint chanid = m_cid.getValue().toUInt();

    pid_cache_t pid_cache;
    if (!ChannelUtil::GetCachedPids(chanid, pid_cache))
        return;

    auto it = pid_cache.begin();
    for (uint i = 0; i < kMaxPIDs && it != pid_cache.end(); )
    {
        if (!(it->IsPermanent()))
        {
            ++it;
            continue;
        }

        m_pids[i]->setValue(QString("0x%1")
                          .arg(it->GetPID(),2,16,QLatin1Char('0')));
        m_sids[i]->setValue(QString::number(it->GetComposite()&0x1ff));
        m_pcrs[i]->setValue(it->IsPCRPID());

        ++it;
        ++i;
    }
}

void ChannelOptionsRawTS::Save(void)
{
    uint chanid = m_cid.getValue().toUInt();

    pid_cache_t pid_cache;
    for (uint i = 0; i < kMaxPIDs; i++)
    {
        bool ok = false;
        uint pid = m_pids[i]->getValue().toUInt(&ok, 0);
        if (!ok || (m_sids[i]->getValue().toUInt() == 0U))
            continue;

        pid_cache.emplace_back(
                pid, m_sids[i]->getValue().toUInt() | 0x10000 |
                (m_pcrs[i]->getValue().toUInt() ? 0x200 : 0x0));
    }

    ChannelUtil::SaveCachedPids(chanid, pid_cache, true /* delete_all */);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
