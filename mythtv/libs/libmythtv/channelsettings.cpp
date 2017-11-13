// Qt headers
#include <QWidget>
#include <QFile>
#include <QCoreApplication>

// MythTV headers
#include "channelsettings.h"
#include "channelutil.h"
#include "programinfo.h" // for COMM_DETECT*, GetPreferredSkipTypeCombinations()
#include "mpegtables.h"
#include "mythdirs.h"
#include "cardutil.h"

using std::vector;

QString ChannelDBStorage::GetWhereClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":WHERE" + id.getField().toUpper());
    QString query(id.getField() + " = " + fieldTag);

    bindings.insert(fieldTag, id.getValue());

    return query;
}

QString ChannelDBStorage::GetSetClause(MSqlBindings &bindings) const
{
    QString fieldTag = (":SET" + id.getField().toUpper());
    QString nameTag = (":SET" + GetColumnName().toUpper());

    QString query(id.getField() + " = " + fieldTag + ", " +
                  GetColumnName() + " = " + nameTag);

    bindings.insert(fieldTag, id.getValue());
    bindings.insert(nameTag, user->GetDBValue());

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
};

class Source : public MythUIComboBoxSetting
{
  public:
    Source(const ChannelID &id, uint _default_sourceid) :
        MythUIComboBoxSetting(new ChannelDBStorage(this, id, "sourceid")),
        default_sourceid(_default_sourceid)
    {
        setLabel(QCoreApplication::translate("(Common)", "Video Source"));
    }

    void Load(void)
    {
        fillSelections();
        StandardSetting::Load();

        if (default_sourceid && !getValue().toUInt())
        {
            uint which = sourceid_to_index[default_sourceid];
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
                sourceid_to_index[query.value(1).toUInt()] = i;
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
            }
        }

        sourceid_to_index[0] = 0; // Not selected entry.
    }

  private:
    uint            default_sourceid;
    QMap<uint,uint> sourceid_to_index;
};

class Callsign : public MythUITextEditSetting
{
  public:
    explicit Callsign(const ChannelID &id) :
        MythUITextEditSetting(new ChannelDBStorage(this, id, "callsign"))
    {
        setLabel(QCoreApplication::translate("(Common)", "Callsign"));
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
    for (int i = 0; i < list.size(); i++)
        addSelection(list[i]);
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
            "channel are in a different time zone. (Works for DataDirect "
            "listings only.)"));
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
};

class XmltvID : public MythUIComboBoxSetting
{
  public:
    XmltvID(const ChannelID &id, const QString &_sourceName) :
        MythUIComboBoxSetting(new ChannelDBStorage(this, id, "xmltvid"), true),
        sourceName(_sourceName)
    {
        setLabel(QCoreApplication::translate("(Common)", "XMLTV ID"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "ID used by listing services to get an exact correspondence "
            "between a channel in your line-up and a channel in their "
            "database. Normally this is set automatically when "
            "'mythfilldatabase' is run."));
    }

    void Load(void)
    {
        fillSelections();
        StandardSetting::Load();
    }

    void fillSelections(void)
    {
        clearSelections();

        QString xmltvFile = GetConfDir() + '/' + sourceName + ".xmltv";

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

            for (int x = 0; x < idList.size(); x++)
                addSelection(idList.at(x), idList.at(x));
        }
    }

  private:
    QString sourceName;
};

class ServiceID : public MythUISpinBoxSetting
{
  public:
    explicit ServiceID(const ChannelID &id)
        : MythUISpinBoxSetting(new ChannelDBStorage(this, id, "serviceid"),
                               -1, UINT16_MAX, 1, true, "NULL")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "ServiceID"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
                "Service ID (Program Number) of desired channel within the transport stream. "
                "If there is only one channel, then setting this to anything will still find it."));
    }

    void Load(void)
    {
        StandardSetting::Load();

        if (getValue().isNull())
            setValue("-1");
    }

    QString getValue(void) const
    {
        if (StandardSetting::getValue().toInt() == -1)
            return QString();
        else
            return StandardSetting::getValue();
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

        deque<int> tmp = GetPreferredSkipTypeCombinations();
        tmp.push_front(COMM_DETECT_UNINIT);
        tmp.push_back(COMM_DETECT_COMMFREE);

        for (uint i = 0; i < tmp.size(); i++)
            addSelection(SkipTypeToString(tmp[i]), QString::number(tmp[i]));
    }
};

class Visible : public MythUICheckBoxSetting
{
  public:
    explicit Visible(const ChannelID &id) :
        MythUICheckBoxSetting(new ChannelDBStorage(this, id, "visible"))
    {
        setValue(true);

        setLabel(QCoreApplication::translate("(ChannelSettings)", "Visible"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "If enabled, the channel will be visible in the EPG."));
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
            "Depending on the tuner type, specify either the exact "
            "frequency (in kHz) or a valid channel "
            "number that will be understood by your tuners."));
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
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID &id,
          uint default_sourceid, bool add_freqid)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Common"));
    addChild(new Name(id));

    Source *source = new Source(id, default_sourceid);
    source->Load();

    Channum *channum = new Channum(id);
    addChild(channum);
    if (add_freqid)
    {
        freqid = new Freqid(id);
        addChild(freqid);
    }
    else
        freqid = 0;
    addChild(new Callsign(id));



    addChild(new Visible(id));
    addChild(new ServiceID(id));

    addChild(source);
    addChild(new ChannelTVFormat(id));
    addChild(new Priority(id));

    addChild(onairguide = new OnAirGuide(id));
    addChild(xmltvID = new XmltvID(id, source->getValueLabel()));
    addChild(new TimeOffset(id));

    addChild(new CommMethod(id));
    addChild(new Icon(id));

    connect(onairguide, SIGNAL(valueChanged(     bool)),
            this,       SLOT(  onAirGuideChanged(bool)));
    connect(source,     SIGNAL(valueChanged( const QString&)),
            this,       SLOT(  sourceChanged(const QString&)));
};

void ChannelOptionsCommon::onAirGuideChanged(bool fValue)
{
    (void)fValue;
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
        supports_eit = (query.size()) ? false : true;
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
            uses_eit_only = (query.size()) ? true : false;
            while (query.next())
            {
                uses_eit_only &= (query.value(0).toString() == "eitonly");
            }
        }
    }

    onairguide->setEnabled(supports_eit);
    xmltvID->setEnabled(!uses_eit_only);
    xmltvID->Load();
}

ChannelOptionsFilters::ChannelOptionsFilters(const ChannelID& id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Filters"));

    addChild(new VideoFilters(id));
    addChild(new OutputFilters(id));
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
    cid(id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
        "Channel Options - Raw Transport Stream"));

    const uint mx = kMaxPIDs;
    pids.resize(mx);
    sids.resize(mx);
    pcrs.resize(mx);

    for (uint i = 0; i < mx; i++)
    {
        addChild((pids[i] = new TransTextEditSetting()));
        pids[i]->setLabel("PID");
        addChild((sids[i] = new TransMythUIComboBoxSetting()));
        sids[i]->setLabel("    StreamID");
        for (uint j = 0x101; j <= 0x1ff; j++)
        {
            QString desc = StreamID::GetDescription(j&0xff);
            if (!desc.isEmpty())
                sids[i]->addSelection(
                    QString("%1 (0x%2)")
                    .arg(desc).arg(j&0xff,2,16,QLatin1Char('0')),
                    QString::number(j), false);
        }
        for (uint j = 0x101; j <= 0x1ff; j++)
        {
            QString desc = StreamID::GetDescription(j&0xff);
            if (desc.isEmpty())
                sids[i]->addSelection(
                    QString("0x%1").arg(j&0xff,2,16,QLatin1Char('0')),
                    QString::number(j), false);
        }
/* we don't allow tables right now, PAT & PMT generated on the fly
        for (uint j = 0; j <= 0xff; j++)
        {
            QString desc = TableID::GetDescription(j);
            if (!desc.isEmpty())
            {
                sids[i]->addSelection(
                    QString("%1 (0x%2)").arg(j,0,16,QLatin1Char('0')),
                    QString::number(j),
                    false);
            }
        }
*/
        addChild((pcrs[i] = new TransMythUICheckBoxSetting()));
        pcrs[i]->setLabel("    Is PCR");
    }
};

void ChannelOptionsRawTS::Load(void)
{
    uint chanid = cid.getValue().toUInt();

    pid_cache_t pid_cache;
    if (!ChannelUtil::GetCachedPids(chanid, pid_cache))
        return;

    pid_cache_t::const_iterator it = pid_cache.begin();
    for (uint i = 0; i < kMaxPIDs && it != pid_cache.end(); )
    {
        if (!(it->IsPermanent()))
        {
            ++it;
            continue;
        }

        pids[i]->setValue(QString("0x%1")
                          .arg(it->GetPID(),2,16,QLatin1Char('0')));
        sids[i]->setValue(QString::number(it->GetComposite()&0x1ff));
        pcrs[i]->setValue(it->IsPCRPID());

        ++it;
        ++i;
    }
}

void ChannelOptionsRawTS::Save(void)
{
    uint chanid = cid.getValue().toUInt();

    pid_cache_t pid_cache;
    for (uint i = 0; i < kMaxPIDs; i++)
    {
        bool ok;
        uint pid = pids[i]->getValue().toUInt(&ok, 0);
        if (!ok || !sids[i]->getValue().toUInt())
            continue;

        pid_cache.push_back(
            pid_cache_item_t(
                pid, sids[i]->getValue().toUInt() | 0x10000 |
                (pcrs[i]->getValue().toUInt() ? 0x200 : 0x0)));
    }

    ChannelUtil::SaveCachedPids(chanid, pid_cache, true /* delete_all */);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
