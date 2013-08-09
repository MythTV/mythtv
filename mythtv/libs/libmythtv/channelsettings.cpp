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

class Name : public LineEditSetting, public ChannelDBStorage
{
  public:
    Name(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "name")
    {
        setLabel(QCoreApplication::translate("(Common)", "Channel Name"));
    }
};

class Channum : public LineEditSetting, public ChannelDBStorage
{
  public:
    Channum(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "channum")
    {
        setLabel(QCoreApplication::translate("(Common)", "Channel Number"));
    }
};

class Source : public ComboBoxSetting, public ChannelDBStorage
{
  public:
    Source(const ChannelID &id, uint _default_sourceid) :
        ComboBoxSetting(this), ChannelDBStorage(this, id, "sourceid"),
        default_sourceid(_default_sourceid)
    {
        setLabel(QCoreApplication::translate("(Common)", "Video Source"));
    }

    void Load(void)
    {
        fillSelections();
        ChannelDBStorage::Load();

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

class Callsign : public LineEditSetting, public ChannelDBStorage
{
  public:
    Callsign(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "callsign")
    {
        setLabel(QCoreApplication::translate("(Common)", "Callsign"));
    }
};

ChannelTVFormat::ChannelTVFormat(const ChannelID &id) :
    ComboBoxSetting(this), ChannelDBStorage(this, id, "tvformat")
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

class TimeOffset : public SpinBoxSetting, public ChannelDBStorage
{
  public:
    TimeOffset(const ChannelID &id) :
        SpinBoxSetting(this, -1440, 1440, 1),
        ChannelDBStorage(this, id, "tmoffset")
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

class Priority : public SpinBoxSetting, public ChannelDBStorage
{
  public:
    Priority(const ChannelID &id) :
        SpinBoxSetting(this, -99, 99, 1),
        ChannelDBStorage(this, id, "recpriority")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "Priority"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Number of priority points to be added to any recording on this "
            "channel during scheduling. Use a positive number as the priority "
            "if you want this to be a preferred channel, a negative one to "
            "deprecate this channel."));
    }
};

class Icon : public LineEditSetting, public ChannelDBStorage
{
  public:
    Icon(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "icon")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", "Icon"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Image file to use as the icon for this channel on various MythTV "
            "displays."));
    }
};

class VideoFilters : public LineEditSetting, public ChannelDBStorage
{
  public:
    VideoFilters(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "videofilters")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", 
                                             "Video filters"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Filters to be used when recording from this channel.  Not used "
            "with hardware encoding cards."));

    }
};


class OutputFilters : public LineEditSetting, public ChannelDBStorage
{
  public:
    OutputFilters(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "outputfilters")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                    "Playback filters"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Filters to be used when recordings from this channel are viewed. "
            "Start with a plus to append to the global playback filters."));
    }
};

class XmltvID : public ComboBoxSetting, public ChannelDBStorage
{
  public:
    XmltvID(const ChannelID &id, const QString &_sourceName) :
        ComboBoxSetting(this, true), ChannelDBStorage(this, id, "xmltvid"),
        sourceName(_sourceName)
    {
        setLabel(QCoreApplication::translate("(Common)", "XMLTV ID"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)", 
            "ID used by listing services to get an exact correspondance "
            "between a channel in your line-up and a channel in their "
            "database. Normally this is set automatically when "
            "'mythfilldatabase' is run."));
    }

    void Load(void)
    {
        fillSelections();
        ChannelDBStorage::Load();
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

class CommMethod : public ComboBoxSetting, public ChannelDBStorage
{
  public:
    CommMethod(const ChannelID &id) :
       ComboBoxSetting(this), ChannelDBStorage(this, id, "commmethod")
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

class Visible : public CheckBoxSetting, public ChannelDBStorage
{
  public:
    Visible(const ChannelID &id) :
        CheckBoxSetting(this), ChannelDBStorage(this, id, "visible")
    {
        setValue(true);

        setLabel(QCoreApplication::translate("(ChannelSettings)", "Visible"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)", 
            "If enabled, the channel will be visible in the EPG."));
    }
};

class OnAirGuide : public CheckBoxSetting, public ChannelDBStorage
{
  public:
    OnAirGuide(const ChannelID &id) :
        CheckBoxSetting(this), ChannelDBStorage(this, id, "useonairguide")
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

class Freqid : public LineEditSetting, public ChannelDBStorage
{
  public:
    Freqid(const ChannelID &id) :
        LineEditSetting(this), ChannelDBStorage(this, id, "freqid")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)",
                                             "Frequency or Channel"));
        setHelpText(QCoreApplication::translate("(ChannelSettings)",
            "Specify either the exact frequency (in kHz) or a valid channel "
            "for your 'TV Format'."));
    }
};

class Finetune : public SliderSetting, public ChannelDBStorage
{
  public:
    Finetune(const ChannelID& id)
        : SliderSetting(this, -300, 300, 1),
        ChannelDBStorage(this, id, "finetune")
    {
        setLabel(QCoreApplication::translate("(ChannelSettings)", 
                                             "Finetune (kHz)"));

        setHelpText(QCoreApplication::translate("(ChannelSettings)", 
            "Value to be added to your desired frequency (in kHz) for "
            "'fine tuning'."));
    }
};

class Contrast : public SliderSetting, public ChannelDBStorage
{
  public:
    Contrast(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655),
        ChannelDBStorage(this, id, "contrast")
    {
        setLabel(QCoreApplication::translate("(Common)", "Contrast"));
    }
};

class Brightness : public SliderSetting, public ChannelDBStorage
{
  public:
    Brightness(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655),
        ChannelDBStorage(this, id, "brightness")
    {
        setLabel(QCoreApplication::translate("(Common)", "Brightness"));
    }
};

class Colour : public SliderSetting, public ChannelDBStorage
{
  public:
    Colour(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655),
        ChannelDBStorage(this, id, "colour")
    {
        setLabel(QCoreApplication::translate("(Common)", "Color"));
    }
};

class Hue : public SliderSetting, public ChannelDBStorage
{
  public:
    Hue(const ChannelID &id) :
        SliderSetting(this, 0, 65535, 655), ChannelDBStorage(this, id, "hue")
    {
        setLabel(QCoreApplication::translate("(Common)", "Hue"));
    }
};

ChannelOptionsCommon::ChannelOptionsCommon(const ChannelID &id,
                                           uint default_sourceid) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Common"));
    setUseLabel(false);

    addChild(new Name(id));

    Source *source = new Source(id, default_sourceid);
    source->Load();

    HorizontalConfigurationGroup *group1 =
        new HorizontalConfigurationGroup(false,false,true,true);
    VerticalConfigurationGroup *bottomhoz =
        new VerticalConfigurationGroup(false, true);
    VerticalConfigurationGroup *left =
        new VerticalConfigurationGroup(false, true);
    VerticalConfigurationGroup *right =
        new VerticalConfigurationGroup(false, true);


    left->addChild(new Channum(id));
    left->addChild(new Callsign(id));
    left->addChild(new Visible(id));

    right->addChild(source);
    right->addChild(new ChannelTVFormat(id));
    right->addChild(new Priority(id));

    group1->addChild(left);
    group1->addChild(right);

    bottomhoz->addChild(onairguide = new OnAirGuide(id));
    bottomhoz->addChild(xmltvID = new XmltvID(id, source->getSelectionLabel()));
    bottomhoz->addChild(new TimeOffset(id));

    addChild(group1);
    addChild(new CommMethod(id));
    addChild(new Icon(id));
    addChild(bottomhoz);

    connect(onairguide, SIGNAL(valueChanged(     bool)),
            this,       SLOT(  onAirGuideChanged(bool)));
    connect(source,     SIGNAL(valueChanged( const QString&)),
            this,       SLOT(  sourceChanged(const QString&)));
};

void ChannelOptionsCommon::Load(void)
{
    VerticalConfigurationGroup::Load();
}

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
                  "FROM capturecard, videosource, cardinput "
                  "WHERE cardinput.sourceid   = videosource.sourceid AND "
                  "      cardinput.cardid     = capturecard.cardid   AND "
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

ChannelOptionsFilters::ChannelOptionsFilters(const ChannelID& id) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Filters"));
    setUseLabel(false);

    addChild(new VideoFilters(id));
    addChild(new OutputFilters(id));
}

ChannelOptionsV4L::ChannelOptionsV4L(const ChannelID& id) :
    VerticalConfigurationGroup(false, true, false, false)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
                                         "Channel Options - Video4Linux"));
    setUseLabel(false);

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
    VerticalConfigurationGroup(false, true, false, false), cid(id)
{
    setLabel(QCoreApplication::translate("(ChannelSettings)",
        "Channel Options - Raw Transport Stream"));

    setUseLabel(false);

    const uint mx = kMaxPIDs;
    pids.resize(mx);
    sids.resize(mx);
    pcrs.resize(mx);

    for (uint i = 0; i < mx; i++)
    {
        HorizontalConfigurationGroup *row =
            new HorizontalConfigurationGroup(false, false, true, true);
        TransLabelSetting *label0 = new TransLabelSetting();
        label0->setLabel("    PID");
        TransLabelSetting *label1 = new TransLabelSetting();
        label1->setLabel("    StreamID");
        TransLabelSetting *label2 = new TransLabelSetting();
        label2->setLabel("    Is PCR");
        row->addChild(label0);
        row->addChild((pids[i] = new TransLineEditSetting()));
        row->addChild(label1);
        row->addChild((sids[i] = new TransComboBoxSetting()));
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
        row->addChild(label2);
        row->addChild((pcrs[i] = new TransCheckBoxSetting()));
        addChild(row);
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
