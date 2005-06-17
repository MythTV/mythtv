// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "atsctables.h"
#include "atscdescriptors.h"
#include "qstring.h"

QString MasterGuideTable::TableClassString(unsigned int i) const 
{
    static const QString tts[] = {
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
    return tts[tt];
}

int MasterGuideTable::TableClass(unsigned int i) const 
{
    const int tt = TableType(i);
    if (tt < 6)
    {
        return tt;
    }
    else if (tt < 0x300)
    {
        if (tt < 0x200) return TableClass::EIT;
        return TableClass::ETTe;
    }
    else if (tt >= 0x1400 && tt < 0x1500)
        return TableClass::DCCT;
    else if (tt < 0x400)
        return TableClass::RRT;
    return TableClass::UNKNOWN;
}

void MasterGuideTable::Parse(void) const 
{
    _ptrs.clear();
    _ptrs.push_back(const_cast<unsigned char*>(pesdata()) + 12);
    for (unsigned int i = 0; i < TableCount(); i++)
        _ptrs.push_back(_ptrs[i] + 11 + TableDescriptorsLength(i));
}


void VirtualChannelTable::Parse(void) const 
{
    _ptrs.clear();
    _ptrs.push_back(const_cast<unsigned char*>(pesdata()) + 11);
    for (unsigned int i = 0; i < ChannelCount(); i++)
        _ptrs.push_back(_ptrs[i] + 32 + DescriptorsLength(i));
}

void EventInformationTable::Parse(void) const 
{
    _ptrs.clear();
    _ptrs.push_back(const_cast<unsigned char*>(pesdata()) + 11);
    for (unsigned int i = 0; i < EventCount(); i++)
        _ptrs.push_back(_ptrs[i] + 12 + TitleLength(i) + DescriptorsLength(i));
}

QString MasterGuideTable::toString(void) const 
{
    QString str;
    str.append(QString("MGT: tables(%1)\n").arg(TableCount()));
    for (unsigned int i = 0; i < TableCount(); i++)
    {
        str.append(QString("Table #%1 ").arg(i, 2, 10));
        str.append(QString("pid(0x%1) ver(%2) ")
                   .arg(TablePID(i), 0, 16).arg(TableVersion(i), 2, 10));
        str.append(QString("size(%1) desc_len(%2) type: %4 %3 \n")
                   .arg(TableDescriptorsBytes(i), 4, 10)
                   .arg(TableDescriptorsLength(i))
                   .arg(TableClassString(i)).arg(TableType(i)));
        if (0 != TableDescriptorsLength(i))
        {
            vector<const unsigned char*> desc = 
                MPEGDescriptor::Parse(TableDescriptors(i),
                                      TableDescriptorsLength(i));
            for (uint i = 0; i < desc.size(); i++)
                str.append(QString("  %1\n")
                           .arg(MPEGDescriptor(desc[i]).toString()));
        }
    }
    if (0 != GlobalDescriptorsLength())
    {
        str.append(QString("Global descriptors length: %1\n")
                   .arg(GlobalDescriptorsLength()));
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(GlobalDescriptors(),
                                  GlobalDescriptorsLength());
        for (uint i = 0; i < desc.size(); i++)
            str.append(QString(" %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
    }
    return str;
}

QString TerrestrialVirtualChannelTable::toString(int chan) const 
{
    static QString modnames[6] =
    {
        QObject::tr("[Reserved]"),  QObject::tr("Analog"),
        QObject::tr("SCTE mode 1"), QObject::tr("SCTE mode 2"),
        QObject::tr("ATSC 8-VSB"),  QObject::tr("ATSC 16-VSB"),
    };

    QString str;
    str.append(QString("Channel #%1 ").arg(chan));
    str.append(QString("name(%1) %2-%3 ").arg(ShortChannelName(chan))
               .arg(MajorChannel(chan)).arg(MinorChannel(chan)));

    if (ModulationMode(chan) > 5)
        str.append(QString("mod(UNKNOWN %1) ").arg(ModulationMode(chan)));
    else
        str.append(QString("mod(%1) ")
                   .arg(modnames[ModulationMode(chan)]));

    str.append(QString("cTSID(0x%1)\n")
               .arg(ChannelTransportStreamID(chan), 0, 16));
    str.append(QString(" pnum(%1) ").arg(ProgramNumber(chan)));
    str.append(QString("ETM_loc(%1) ").arg(ETMlocation(chan)));
    str.append(QString("access_ctrl(%1) ").arg(IsAccessControlled(chan)));
    str.append(QString("hidden(%1) ").arg(IsHidden(chan)));
    str.append(QString("hide_guide(%1) ").arg(IsHiddenInGuide(chan)));
    str.append(QString("service_type(%1) ").arg(ServiceType(chan)));
    str.append(QString("source_id(%1)\n").arg(SourceID(chan)));
    if (0!=DescriptorsLength(chan))
    {
        str.append(QString(" descriptors length(%1) ")
                   .arg(DescriptorsLength(chan)));
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(Descriptors(chan), DescriptorsLength(chan));
        str.append(QString("count(%1)\n").arg(desc.size()));
        for (uint i = 0; i < desc.size(); i++)
            str.append(QString("  %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
    }
    return str;
}

QString TerrestrialVirtualChannelTable::toString(void) const 
{
    QString str;
    str.append(QString("VCT Terra: channels(%1) tsid(0x%2) ")
               .arg(ChannelCount()).arg(TransportStreamID(), 0, 16));
    str.append(QString("seclength(%3)\n").arg(Length()));
    for (unsigned int i = 0; i < ChannelCount(); i++)
    {
        str.append(toString(i)).append("\n");
    }
    if (0!=GlobalDescriptorsLength())
    {
        str.append(QString("global descriptors length: %1\n")
                   .arg(GlobalDescriptorsLength()));
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(GlobalDescriptors(),
                                  GlobalDescriptorsLength());
        str.append(QString("global descriptors count: %1\n").arg(desc.size()));
        for (uint i = 0; i < desc.size(); i++)
            str.append(QString(" %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
    }
    return str;
}

QString CableVirtualChannelTable::toString(int chan) const 
{
    static QString modnames[6] =
    {
        QObject::tr("[Reserved]"),  QObject::tr("Analog"),
        QObject::tr("SCTE mode 1"), QObject::tr("SCTE mode 2"),
        QObject::tr("ATSC 8-VSB"),  QObject::tr("ATSC 16-VSB"),
    };

    QString str;
    str.append(QString("Channel #%1 ").arg(chan));
    str.append(QString("name(%1) %2-%3 ").arg(ShortChannelName(chan))
               .arg(MajorChannel(chan)).arg(MinorChannel(chan)));

    if (ModulationMode(chan) > 5)
        str.append(QString("mod(UNKNOWN %1) ").arg(ModulationMode(chan)));
    else
        str.append(QString("mod(%1) ").arg(modnames[ModulationMode(chan)]));

    str.append(QString("cTSID(0x%1)\n")
               .arg(ChannelTransportStreamID(chan), 0, 16));
    str.append(QString(" pnum(%1) ").arg(ProgramNumber(chan)));
    str.append(QString("ETM_loc(%1) ").arg(ETMlocation(chan)));
    str.append(QString("access_ctrl(%1) ").arg(IsAccessControlled(chan)));
    str.append(QString("hidden(%1)\n").arg(IsHidden(chan)));

    str.append(QString("path_select(%1) ").arg(IsPathSelect(chan)));
    str.append(QString("out_of_band(%1) ").arg(IsOutOfBand(chan)));

    str.append(QString("hide_guide(%1) ").arg(IsHiddenInGuide(chan)));
    str.append(QString("service_type(%1) ").arg(ServiceType(chan)));
    str.append(QString("source_id(%1)\n").arg(SourceID(chan)));
    if (0 != DescriptorsLength(chan))
    {
        str.append(QString(" descriptors length(%1) ")
                   .arg(DescriptorsLength(chan)));
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(Descriptors(chan), DescriptorsLength(chan));
        str.append(QString("count(%1)\n").arg(desc.size()));
        for (uint i = 0; i < desc.size(); i++)
            str.append(QString("  %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
    }
    return str;
}

QString CableVirtualChannelTable::toString(void) const 
{
    QString str;
    str.append(QString("VCT Cable: channels(%1) tsid(0x%2) ")
               .arg(ChannelCount()).arg(TransportStreamID(), 0, 16));
    str.append(QString("seclength(%3)\n").arg(Length()));
    for (unsigned int i = 0; i < ChannelCount(); i++)
    {
        str.append(toString(i)).append("\n");
    }
    if (0 != GlobalDescriptorsLength())
    {
        str.append(QString("global descriptors length: %1\n")
                   .arg(GlobalDescriptorsLength()));
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(GlobalDescriptors(),
                                  GlobalDescriptorsLength());
        str.append(QString("global descriptors count: %1\n").arg(desc.size()));
        for (uint i = 0; i < desc.size(); i++)
        {
            str.append(QString(" %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
        }
    }
    return str;
}

QString EventInformationTable::toString(void) const 
{
    QString str;
    str.append(QString("Event Information Table\n"));
    str.append(((PSIPTable*)(this))->toString());
    str.append(QString("      pid(0x%1) sourceID(%2) eventCount(%3)\n")
               .arg(tsheader()->PID()).arg(SourceID()).arg(EventCount()));
    for (uint i = 0; i < EventCount(); i++)
    {
        str.append(QString(" Event #%1 ID(%2) start_time(%3) length(%4 sec)\n")
                   .arg(i,2,10).arg(EventID(i))
                   .arg(StartTimeGPS(i).toString(Qt::LocalDate))
                   .arg(LengthInSeconds(i)));
        str.append(QString("           ETM_loc(%1) Title(%2)\n").
                   arg(ETMLocation(i)).arg(title(i).toString()));
        if (0 != DescriptorsLength(i))
        {
            vector<const unsigned char*> desc = 
                MPEGDescriptor::Parse(Descriptors(i), DescriptorsLength(i));
            for (uint j=0; j<desc.size(); j++)
                str.append(QString("%1\n")
                           .arg(MPEGDescriptor(desc[j]).toString()));
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
        .arg(IsChannelETM()).arg(IsEventETM())
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
