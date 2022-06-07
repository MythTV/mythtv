// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "atsctables.h"
#include "atscdescriptors.h"

QString MasterGuideTable::TableClassString(uint i) const
{
    static const std::array<const QString,11> kTts {
        QString("UNKNOWN"),
        QString("Terrestrial VCT with current()"),
        QString("Terrestrial VCT with !current()"),
        QString("Cable VCT with current()"),
        QString("Cable VCT with !current()"),
        QString("Channel ETT"),
        QString("DCCSCT"),
        QString("EIT + 0x100"),
        QString("Event ETT + 0x200"),
        QString("DCCT + 0x1400"),
        QString("RTT + 0x300")
    };
    int tt = TableClass(i) + 1;
    return kTts[tt];
}

int MasterGuideTable::TableClass(uint i) const
{
    const int tt = TableType(i);
    if (tt < 6)
    {
        return tt;
    }
    if (tt < 0x300)
    {
        if (tt < 0x200) return TableClass::EIT;
        return TableClass::ETTe;
    }
    if (tt >= 0x1400 && tt < 0x1500)
        return TableClass::DCCT;
    if (tt < 0x400)
        return TableClass::RRT;
    return TableClass::UNKNOWN;
}

void MasterGuideTable::Parse(void) const
{
    m_ptrs.clear();
    m_ptrs.push_back(const_cast<unsigned char*>(psipdata()) + 3);
    for (uint i = 0; i < TableCount(); i++)
        m_ptrs.push_back(m_ptrs[i] + 11 + TableDescriptorsLength(i));
}


void VirtualChannelTable::Parse(void) const
{
    m_ptrs.clear();
    m_ptrs.push_back(const_cast<unsigned char*>(psipdata()) + 2);
    for (uint i = 0; i < ChannelCount(); i++)
        m_ptrs.push_back(m_ptrs[i] + 32 + DescriptorsLength(i));
}

void EventInformationTable::Parse(void) const
{
    m_ptrs.clear();
    m_ptrs.push_back(const_cast<unsigned char*>(psipdata()) + 2);
    for (uint i = 0; i < EventCount(); i++)
        m_ptrs.push_back(m_ptrs[i] + 12 + TitleLength(i) + DescriptorsLength(i));
}

QString MasterGuideTable::toString(void) const
{
    QString str;
    str.append(QString("Master Guide Section\n%1"
                       "      table_count(%2)\n")
               .arg(PSIPTable::toString())
               .arg(TableCount()));

    if (m_ptrs.size() < TableCount())
        LOG(VB_GENERAL, LOG_ERR, "MasterGuideTable::toString(): Table count mismatch");

    for (uint i = 0; i < TableCount() && i < m_ptrs.size(); i++)
    {
        str.append(QString("  Table #%1 ").arg(i, 2, 10));
        str.append(QString("pid(0x%1) ver(%2) ")
                   .arg(TablePID(i), 0, 16).arg(TableVersion(i), 2, 10));
        str.append(QString("size(%1) desc_len(%2) type: %4 %3 \n")
                   .arg(TableDescriptorsBytes(i), 4, 10)
                   .arg(TableDescriptorsLength(i))
                   .arg(TableClassString(i)).arg(TableType(i)));
        if (0 != TableDescriptorsLength(i))
        {
            std::vector<const unsigned char*> desc =
                MPEGDescriptor::Parse(TableDescriptors(i),
                                      TableDescriptorsLength(i));
            for (auto & d : desc)
                str.append(QString("  %1\n")
                           .arg(MPEGDescriptor(d).toString()));
        }
    }
    if (0 != GlobalDescriptorsLength())
    {
        str.append(QString("  global descriptors length(%1) ")
                   .arg(GlobalDescriptorsLength()));
        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(GlobalDescriptors(),
                                  GlobalDescriptorsLength());
        str.append(QString("count: %1\n").arg(desc.size()));
        for (auto & i : desc)
        {
            str.append(QString("    %1\n")
                       .arg(MPEGDescriptor(i).toString()));
        }
    }
    return str;
}

QString MasterGuideTable::toStringXML(uint indent_level) const
{
    QString indent_0 = StringUtil::indentSpaces(indent_level);
    QString indent_1 = StringUtil::indentSpaces(indent_level + 1);
    QString indent_2 = StringUtil::indentSpaces(indent_level + 2);

    QString str =
        QString("%1<MasterGuideSection table_count=\"%2\" "
                "global_descriptors_length=\"%3\"\n%4%5>\n")
        .arg(indent_0)
        .arg(TableCount())
        .arg(GlobalDescriptorsLength())
        .arg(indent_1,
             PSIPTable::XMLValues(indent_level + 1));

    std::vector<const unsigned char*> gdesc =
        MPEGDescriptor::Parse(GlobalDescriptors(), GlobalDescriptorsLength());
    for (auto & i : gdesc)
    {
        str += MPEGDescriptor(i, 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    if (m_ptrs.size() < TableCount())
        LOG(VB_GENERAL, LOG_ERR, "MasterGuideTable::toStringXML(): Table count mismatch");

    for (uint i = 0; i < TableCount() && i < m_ptrs.size(); i++)
    {
        str += QString(
            "%1<Table pid=\"0x%2\" version=\"%3\""
            "\n%4type=\"0x%5\" type_desc=\"%6\""
            "\n%7number_bytes=\"%8\" table_descriptors_length=\"%9\"")
            .arg(indent_1)
            .arg(TablePID(i),4,16,QChar('0'))
            .arg(TableVersion(i))
            .arg(indent_1)
            .arg(TableType(i),4,16,QChar('0'))
            .arg(TableClassString(i),
                 indent_2)
            .arg(TableDescriptorsBytes(i))
            .arg(TableDescriptorsLength(i));

        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(
                TableDescriptors(i), TableDescriptorsLength(i));
        str += (desc.empty()) ? " />\n" : ">\n";
        for (auto & j : desc)
        {
            str += MPEGDescriptor(j, 300)
                .toStringXML(indent_level + 2) + "\n";
        }

        if (!desc.empty())
            str += indent_1 + "</Table>\n";
    }

    return str + "</MasterGuideSection>";
}

QString VirtualChannelTable::ModulationModeString(uint i) const
{
    static const std::array<const std::string,6>s_modnames
    {
        "[Reserved]",   "Analog",      "SCTE mode 1",
        "SCTE mode 2",  "ATSC 8-VSB",  "ATSC 16-VSB",
    };
    uint mode = ModulationMode(i);
    if (mode >= (sizeof(s_modnames) / sizeof(char*)))
        return QString("Unknown 0x%1").arg(mode,2,16,QChar('0'));
    return QString::fromStdString(s_modnames[mode]);
}

QString VirtualChannelTable::ServiceTypeString(uint i) const
{
    static const std::array<const std::string,5> s_servicenames
    {
        "[Reserved]", "Analog", "ATSC TV", "ATSC Audio", "ATSC Data",
    };
    uint type = ServiceType(i);
    if (type >= s_servicenames.size())
        return QString("Unknown 0x%1").arg(type,2,16,QChar('0'));
    return QString::fromStdString(s_servicenames[type]);
}

QString VirtualChannelTable::toString(void) const
{
    QString str;
    str.append(QString("%1 Virtual Channel Section\n%2"
                       "      channel_count(%3) tsid(0x%4)")
               .arg((TableID::TVCT == TableID()) ? "Terrestrial":"Cable",
                    PSIPTable::toString())
               .arg(ChannelCount())
               .arg(TransportStreamID(),4,16,QChar('0')));

    if (TableID::CVCT == TableID())
    {
        uint sctemapid = (pesdata()[3]<<8) | pesdata()[4];
        str.append(QString(" mapid(0x%1)").arg(sctemapid,0,16));
    }

    str.append("\n");

    for (uint i = 0; i < ChannelCount(); i++)
        str.append(ChannelString(i));

    if (0 != GlobalDescriptorsLength())
    {
        str.append(QString("global descriptors length(%1) ")
                   .arg(GlobalDescriptorsLength()));
        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(GlobalDescriptors(),
                                  GlobalDescriptorsLength());
        str.append(QString("count: %1\n").arg(desc.size()));
        for (auto & i : desc)
        {
            str.append(QString(" %1\n")
                       .arg(MPEGDescriptor(i).toString()));
        }
    }

    return str;
}

QString VirtualChannelTable::toStringXML(uint indent_level) const
{
    QString indent_0 = StringUtil::indentSpaces(indent_level);
    QString indent_1 = StringUtil::indentSpaces(indent_level + 1);

    QString section_name = QString("%1VirtualChannelSection")
        .arg((TableID::TVCT == TableID()) ? "Terrestrial" : "Cable");

    QString mapid;
    if (TableID::CVCT == TableID())
    {
        uint sctemapid = (pesdata()[3]<<8) | pesdata()[4];
        mapid = QString(" mapid=\"0x%1\"").arg(sctemapid,4,16,QChar('0'));
    }

    QString str =
        QString("%1<%2 tsid=\"0x%3\" channel_count=\"%4\""
                "\n%5global_descriptors_length=\"%6\"%7"
                "\n%8%9>\n")
        .arg(indent_0,
             section_name)
        .arg(TransportStreamID(),4,16,QChar('0'))
        .arg(ChannelCount())
        .arg(indent_1)
        .arg(GlobalDescriptorsLength())
        .arg(mapid,
             indent_1,
             PSIPTable::XMLValues(indent_level + 1));

    std::vector<const unsigned char*> gdesc =
        MPEGDescriptor::Parse(GlobalDescriptors(), GlobalDescriptorsLength());
    for (auto & i : gdesc)
    {
        str += MPEGDescriptor(i, 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    for (uint i = 0; i < ChannelCount(); i++)
        str += ChannelStringXML(indent_level + 1, i) + "\n";

    return str + indent_0 + QString("</%1>").arg(section_name);
}

QString VirtualChannelTable::ChannelStringXML(
    uint indent_level, uint chan) const
{
    QString indent_0 = StringUtil::indentSpaces(indent_level);
    QString indent_1 = StringUtil::indentSpaces(indent_level + 1);
    QString str = QString("%1<Channel %2\n%3descriptors_length=\"%4\">\n")
        .arg(indent_0,
             XMLChannelValues(indent_level + 1, chan),
             indent_1)
        .arg(DescriptorsLength(chan));

    std::vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(chan), DescriptorsLength(chan));
    for (auto & i : desc)
    {
        str += MPEGDescriptor(i, 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    return str + indent_0 + "</Channel>";
}

QString VirtualChannelTable::XMLChannelValues(
    uint indent_level, uint chan) const
{
    QString indent_0 = StringUtil::indentSpaces(indent_level);
    QString str;

    str += QString("short_channel_name=\"%1\" ").arg(ShortChannelName(chan));
    str += "\n" + indent_0;

    str += QString(R"(modulation="0x%1" modulation_desc="%2" )")
        .arg(ModulationMode(chan),2,16,QChar('0'))
        .arg(ModulationModeString(chan));
    str += QString("channel_tsid=\"0x%1\"")
        .arg(ChannelTransportStreamID(chan),4,16,QChar('0'));
    str += "\n" + indent_0;

    str += QString("program_number=\"%1\" ").arg(ProgramNumber(chan));
    str += QString("etm_location=\"%1\" ").arg(ETMlocation(chan));
    str += QString("access_controlled=\"%1\"")
        .arg(StringUtil::bool_to_string(IsAccessControlled(chan)));
    str += "\n" + indent_0;

    str += QString("hidden=\"%1\" ")
        .arg(StringUtil::bool_to_string(IsHidden(chan)));
    str += QString("hide_guide=\"%1\"")
        .arg(StringUtil::bool_to_string(IsHiddenInGuide(chan)));
    str += "\n" + indent_0;

    str += QString(R"(service_type="0x%1" service_type_desc="%2")")
        .arg(ServiceType(chan),2,16,QChar('0'))
        .arg(ServiceTypeString(chan));
    str += "\n" + indent_0;

    str += QString("source_id=\"0x%1\"")
        .arg(SourceID(chan),4,16,QChar('0'));

    return str;
}

QString TerrestrialVirtualChannelTable::XMLChannelValues(
    uint indent_level, uint chan) const
{
    return QString(R"(major_channel="%1" minor_channel="%2" )")
        .arg(MajorChannel(chan)).arg(MinorChannel(chan)) +
        VirtualChannelTable::XMLChannelValues(indent_level, chan);
}

QString TerrestrialVirtualChannelTable::ChannelString(uint chan) const
{
    QString str;
    str.append(QString("  Channel #%1 ").arg(chan));
    str.append(QString("name(%1) %2-%3 ").arg(ShortChannelName(chan))
               .arg(MajorChannel(chan)).arg(MinorChannel(chan)));
    str.append(QString("mod(%1) ").arg(ModulationModeString(chan)));
    str.append(QString("cTSID(0x%1)\n")
               .arg(ChannelTransportStreamID(chan),4,16,QChar('0')));

    str.append(QString("    pnum(%1) ").arg(ProgramNumber(chan)));
    str.append(QString("ETM_loc(%1) ").arg(ETMlocation(chan)));
    str.append(QString("access_ctrl(%1) ").arg(static_cast<int>(IsAccessControlled(chan))));
    str.append(QString("hidden(%1) ").arg(static_cast<int>(IsHidden(chan))));
    str.append(QString("hide_guide(%1) ").arg(static_cast<int>(IsHiddenInGuide(chan))));
    str.append(QString("service_type(%1)\n").arg(ServiceTypeString(chan)));

    str.append(QString("    source_id(%1)\n").arg(SourceID(chan)));
    if (0 != DescriptorsLength(chan))
    {
        str.append(QString("    descriptors length(%1) ")
                   .arg(DescriptorsLength(chan)));
        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(Descriptors(chan), DescriptorsLength(chan));
        str.append(QString("count:%1\n").arg(desc.size()));
        for (auto & i : desc)
        {
            str.append(QString("    %1\n")
                       .arg(MPEGDescriptor(i).toString()));
        }
    }
    return str;
}

QString CableVirtualChannelTable::XMLChannelValues(
    uint indent_level, uint chan) const
{
    QString channel_info = SCTEIsChannelNumberTwoPart(chan) ?
        QString(R"(major_channel="%1" minor_channel="%2" )")
        .arg(MajorChannel(chan)).arg(MinorChannel(chan)) :
        QString("channel_number=\"%1\" ")
        .arg(SCTEOnePartChannel(chan));

    return channel_info +
        VirtualChannelTable::XMLChannelValues(indent_level, chan) +
        QString(R"( path_select="%1" out_of_band="%2")")
        .arg(StringUtil::bool_to_string(IsPathSelect(chan)),
             StringUtil::bool_to_string(IsOutOfBand(chan)));
}

QString CableVirtualChannelTable::ChannelString(uint chan) const
{
    QString str;
    str.append(QString("  Channel #%1 ").arg(chan));
    str.append(QString("name(%1)").arg(ShortChannelName(chan)));

    if (SCTEIsChannelNumberTwoPart(chan))
    {
        str.append(QString(" %1-%2 ")
                   .arg(MajorChannel(chan)).arg(MinorChannel(chan)));
    }
    else
    {
        str.append(QString(" %1 ").arg(SCTEOnePartChannel(chan)));
    }

    str.append(QString("mod(%1) ").arg(ModulationModeString(chan)));
    str.append(QString("cTSID(0x%1)\n")
               .arg(ChannelTransportStreamID(chan),4,16,QChar('0')));

    str.append(QString("    pnum(%1) ").arg(ProgramNumber(chan)));
    str.append(QString("ETM_loc(%1) ").arg(ETMlocation(chan)));
    str.append(QString("access_ctrl(%1) ").arg(static_cast<int>(IsAccessControlled(chan))));
    str.append(QString("hidden(%1) ").arg(static_cast<int>(IsHidden(chan))));
    str.append(QString("hide_guide(%1) ").arg(static_cast<int>(IsHiddenInGuide(chan))));
    str.append(QString("service_type(%1)\n").arg(ServiceTypeString(chan)));

    str.append(QString("    source_id(%1) ").arg(SourceID(chan)));
    str.append(QString("path_select(%1) ").arg(static_cast<int>(IsPathSelect(chan))));
    str.append(QString("out_of_band(%1)\n").arg(static_cast<int>(IsOutOfBand(chan))));

    if (0 != DescriptorsLength(chan))
    {
        str.append(QString("    descriptors length(%1) ")
                   .arg(DescriptorsLength(chan)));
        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(Descriptors(chan), DescriptorsLength(chan));
        str.append(QString("count:%1\n").arg(desc.size()));
        for (auto & i : desc)
            str.append(QString("    %1\n")
                       .arg(MPEGDescriptor(i).toString()));
    }
    return str;
}

QString EventInformationTable::toString(void) const
{
    QString str;
    str.append(QString("Event Information Table\n"));
    str.append(static_cast<const PSIPTable*>(this)->toString());
    str.append(QString("      pid(0x%1) sourceID(%2) eventCount(%3)\n")
               .arg(tsheader()->PID()).arg(SourceID()).arg(EventCount()));
    for (uint i = 0; i < EventCount(); i++)
    {
        str.append(QString(" Event #%1 ID(%2) start_time(%3) length(%4 sec)\n")
                   .arg(i,2,10).arg(EventID(i))
                   .arg(StartTimeGPS(i).toString(Qt::ISODate))
                   .arg(LengthInSeconds(i)));
        str.append(QString("           ETM_loc(%1) Title(%2)\n").
                   arg(ETMLocation(i)).arg(title(i).toString()));
        if (0 != DescriptorsLength(i))
        {
            std::vector<const unsigned char*> desc =
                MPEGDescriptor::Parse(Descriptors(i), DescriptorsLength(i));
            for (auto & j : desc)
                str.append(QString("%1\n")
                           .arg(MPEGDescriptor(j).toString()));
        }
    }
    return str;
}

QString ExtendedTextTable::toString(void) const
{
    QString str =
        QString("Extended Text Table -- sourceID(%1) eventID(%2) "
                "ettID(%3) isChannelETM(%4) isEventETM(%5)\n%6")
        .arg(SourceID()).arg(EventID()).arg(ExtendedTextTableID())
        .arg(static_cast<int>(IsChannelETM())).arg(static_cast<int>(IsEventETM()))
        .arg(ExtendedTextMessage().toString());
    return str;
}

int VirtualChannelTable::Find(int major, int minor) const
{
    if (major>0)
    {
        for (uint i = 0; i < ChannelCount(); i++)
        {
            if ((MajorChannel(i) == (uint)major) &&
                (MinorChannel(i) == (uint)minor))
                return (int)i;
        }
    }
    else if (minor>0)
    {
        for (uint i = 0; i < ChannelCount(); i++)
        {
            if (MinorChannel(i) == (uint)minor)
                return (int)i;
        }
    }
    return -1;
}

QString VirtualChannelTable::GetExtendedChannelName(uint i) const
{
    if ((i >= ChannelCount()) || DescriptorsLength(i) == 0)
        return {};

    std::vector<const unsigned char*> parsed =
        MPEGDescriptor::Parse(Descriptors(i), DescriptorsLength(i));

    const unsigned char* desc =
        MPEGDescriptor::Find(parsed, DescriptorID::extended_channel_name);

    if (!desc)
        return {};

    return ExtendedChannelNameDescriptor(desc).LongChannelNameString();
}

QString SystemTimeTable::toString(void) const
{
    QString str =
        QString("System Time Section GPSTime(%1) GPS2UTC_Offset(%2) ")
        .arg(SystemTimeGPS().toString(Qt::ISODate)).arg(GPSOffset());
    str.append(QString("DS(%3) Day(%4) Hour(%5)\n")
               .arg(static_cast<int>(InDaylightSavingsTime()))
               .arg(DayDaylightSavingsStarts())
               .arg(HourDaylightSavingsStarts()));
    return str;
}

QString SystemTimeTable::toStringXML(uint indent_level) const
{
    QString indent_0 = StringUtil::indentSpaces(indent_level);
    QString indent_1 = StringUtil::indentSpaces(indent_level + 1);

    return QString(
        "%1<SystemTimeSection system_time=\"%2\" system_time_iso=\"%3\""
        "\n%4in_dst=\"%5\" dst_start_day=\"%6\" dst_start_hour=\"%7\""
        "\n%8%9 />")
        .arg(indent_0,
             QString::number(GPSRaw()),
             SystemTimeGPS().toString(Qt::ISODate),
             indent_1,
             StringUtil::bool_to_string(InDaylightSavingsTime()),
             QString::number(DayDaylightSavingsStarts()), /* day-of-month */
             QString::number(HourDaylightSavingsStarts()),
             indent_1,
             PSIPTable::XMLValues(indent_level + 1));
}
