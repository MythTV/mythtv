// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson
#include "dvbtables.h"
#include "dvbdescriptors.h"
#include "qstring.h"

void NetworkInformationTable::Parse(void) const
{
    _ptrs.clear();
    _tsc_ptr = pesdata() + 11 + NetworkDescriptorsLength();
    _ptrs.push_back(_tsc_ptr + 2);
    for (uint i = 0; i < TransportStreamCount(); i++)
        _ptrs.push_back(_ptrs[i] + 6 + TransportDescriptorsLength(i));

}

QString NetworkInformationTable::toString(void) const
{
    QString str = QString("NIT: NetID(%1) tranports(%2)\n")
        .arg(NetworkID()).arg(TransportStreamCount());

    if (0 != NetworkDescriptorsLength())
    {
        str.append(QString("Network descriptors length: %1\n")
                   .arg(NetworkDescriptorsLength()));
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(NetworkDescriptors(),
                                  NetworkDescriptorsLength());
        for (uint i = 0; i < desc.size(); i++)
            str.append(QString("  %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
    }

    for (uint i = 0; i < TransportStreamCount(); i++)
    {
        str.append(QString("  Transport #%1 TSID(0x%1) ")
                   .arg(i, 2, 10).arg(TSID(i), 0, 16));
        str.append(QString("original_network_id(0x%2) desc_len(%3)\n")
                   .arg(OriginalNetworkID(i), 0, 16)
                   .arg(TransportDescriptorsLength(i)));

        if (0 != TransportDescriptorsLength(i))
        {
            str.append(QString("  Transport descriptors length: %1\n")
                       .arg(TransportDescriptorsLength(i)));
            vector<const unsigned char*> desc = 
                MPEGDescriptor::Parse(TransportDescriptors(i),
                                      TransportDescriptorsLength(i));
            for (uint i = 0; i < desc.size(); i++)
                str.append(QString("    %1\n")
                           .arg(MPEGDescriptor(desc[i]).toString()));
        }
    }
    return str;
}

QString NetworkInformationTable::NetworkName() const
{
    if (_cached_network_name == QString::null)
    {
        desc_list_t parsed =
            MPEGDescriptor::Parse(NetworkDescriptors(),
                                  NetworkDescriptorsLength());

        const unsigned char *desc =
            MPEGDescriptor::Find(parsed, DescriptorID::network_name);

        if (desc)
            _cached_network_name = NetworkNameDescriptor(desc).Name();
        else 
            _cached_network_name = QString("Net ID 0x%1")
                .arg(NetworkID(), 0, 16);
    }
    return _cached_network_name;
}


void ServiceDescriptionTable::Parse(void) const
{
    _ptrs.clear();
    _ptrs.push_back(pesdata() + 12);
    uint i = 0;
    while ((_ptrs[i] + 5) < (pesdata() + Length()))
    {
        _ptrs.push_back(_ptrs[i] + 5 + ServiceDescriptorsLength(i));
        i++;
    }
}

QString ServiceDescriptionTable::toString(void) const
{
    QString str =
        QString("SDT: TSID(0x%1) original_network_id(0x%2) services(%3)\n")
        .arg(OriginalNetworkID(), 0, 16).arg(TSID(), 0, 16)
        .arg(ServiceCount());
    
    for (uint i = 0; i < ServiceCount(); i++)
    {
        str.append(QString("  Service #%1 SID(0x%2) ")
                   .arg(i, 2, 10).arg(ServiceID(i), 0, 16));
        str.append(QString("eit_schd(%1) eit_pf(%2) free_ca(%3)\n")
                   .arg(HasEITSchedule(i) ? "t" : "f")
                   .arg(HasEITPresentFollowing(i) ? "t" : "f")
                   .arg(HasFreeCA(i) ? "t" : "f"));

        if (0 != ServiceDescriptorsLength(i))
        {
            str.append(QString("  Service descriptors length: %1\n")
                       .arg(ServiceDescriptorsLength(i)));
            vector<const unsigned char*> desc = 
                MPEGDescriptor::Parse(ServiceDescriptors(i),
                                      ServiceDescriptorsLength(i));
            for (uint i = 0; i < desc.size(); i++)
                str.append(QString("    %1\n")
                           .arg(MPEGDescriptor(desc[i]).toString()));
        }
    }
    return str;
}

ServiceDescriptor *ServiceDescriptionTable::GetServiceDescriptor(uint i) const
{
    desc_list_t parsed =
        MPEGDescriptor::Parse(ServiceDescriptors(i),
                              ServiceDescriptorsLength(i));

    const unsigned char *desc =
        MPEGDescriptor::Find(parsed, DescriptorID::service);

    if (desc)
        return new ServiceDescriptor(desc);

    return NULL;
}
