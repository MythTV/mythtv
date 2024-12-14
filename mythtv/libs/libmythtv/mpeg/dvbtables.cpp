// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <cmath>

#include <QTimeZone>

#include "dvbtables.h"
#include "dvbdescriptors.h"

static uint GetPrivateDataSpecifier(const unsigned char *desc, uint priv_dsid)
{
    if (desc[0] == DescriptorID::private_data_specifier)
    {
        PrivateDataSpecifierDescriptor pd(desc);
        if (pd.IsValid())
            priv_dsid = pd.PrivateDataSpecifier();
    }
    return priv_dsid;
}

void NetworkInformationTable::Parse(void) const
{
    m_tscPtr = pesdata() + 10 + NetworkDescriptorsLength();

    m_ptrs.clear();
    m_ptrs.push_back(m_tscPtr + 2);
    for (uint i=0; m_ptrs[i] + 6 <= m_ptrs[0] + TransportStreamDataLength(); i++)
        m_ptrs.push_back(m_ptrs[i] + 6 + TransportDescriptorsLength(i));
}

QString NetworkInformationTable::toString(void) const
{
    QString str = QString("NIT: NetID(%1) transports(%2)\n")
        .arg(NetworkID()).arg(TransportStreamCount());
    str.append(QString("Section (%1) Last Section (%2) IsCurrent (%3)\n")
        .arg(Section()).arg(LastSection()).arg(static_cast<int>(IsCurrent())));

    if (0 != NetworkDescriptorsLength())
    {
        str.append(QString("Network descriptors length: %1\n")
                   .arg(NetworkDescriptorsLength()));
        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(NetworkDescriptors(),
                                  NetworkDescriptorsLength());
        uint priv_dsid = 0;
        for (auto & i : desc)
        {
            priv_dsid = GetPrivateDataSpecifier(i, priv_dsid);
            str.append(QString("  %1\n")
                       .arg(MPEGDescriptor(i).toStringPD(priv_dsid)));
        }
    }

    for (uint i = 0; i < TransportStreamCount(); i++)
    {
        str.append(QString("  Transport #%1 TSID(0x%2) ")
                   .arg(i, 2, 10).arg(TSID(i), 0, 16));
        str.append(QString("original_network_id(0x%2) desc_len(%3)\n")
                   .arg(OriginalNetworkID(i), 0, 16)
                   .arg(TransportDescriptorsLength(i)));

        if (0 != TransportDescriptorsLength(i))
        {
            str.append(QString("  Transport descriptors length: %1\n")
                       .arg(TransportDescriptorsLength(i)));
            std::vector<const unsigned char*> desc =
                MPEGDescriptor::Parse(TransportDescriptors(i),
                                      TransportDescriptorsLength(i));
            uint priv_dsid = 0;
            for (auto & j : desc)
            {
                priv_dsid = GetPrivateDataSpecifier(j, priv_dsid);
                str.append(QString("    %1\n")
                           .arg(MPEGDescriptor(j).toStringPD(priv_dsid)));
            }
        }
    }
    return str;
}

QString NetworkInformationTable::NetworkName() const
{
    if (m_cachedNetworkName.isEmpty())
    {
        desc_list_t parsed =
            MPEGDescriptor::Parse(NetworkDescriptors(),
                                  NetworkDescriptorsLength());

        const unsigned char *desc =
            MPEGDescriptor::Find(parsed, DescriptorID::network_name);

        if (desc)
        {
            auto nndesc = NetworkNameDescriptor(desc);
            if (nndesc.IsValid())
                m_cachedNetworkName = nndesc.Name();
        }
        if (m_cachedNetworkName.isEmpty())
            m_cachedNetworkName = QString("Net ID 0x%1")
                .arg(NetworkID(), 0, 16);
    }
    return m_cachedNetworkName;
}

bool NetworkInformationTable::Mutate(void)
{
    if (VerifyCRC())
    {
        SetTableID((TableID() == TableID::NITo) ? TableID::NIT : TableID::NITo);
        SetCRC(CalcCRC());
        return true;
    }
    return false;
}

void ServiceDescriptionTable::Parse(void) const
{
    m_ptrs.clear();
    m_ptrs.push_back(pesdata() + 11);
    uint i = 0;
    while ((m_ptrs[i] + 5) < (pesdata() + Length()))
    {
        m_ptrs.push_back(m_ptrs[i] + 5 + ServiceDescriptorsLength(i));
        i++;
    }
}

QString ServiceDescriptionTable::toString(void) const
{
    QString str =
        QString("SDT: TSID(0x%1) original_network_id(0x%2) services(%3)\n")
        .arg(TSID(), 0, 16).arg(OriginalNetworkID(), 0, 16)
        .arg(ServiceCount());
    str.append(QString("Section (%1) Last Section (%2) IsCurrent (%3)\n")
        .arg(Section()).arg(LastSection()).arg(static_cast<int>(IsCurrent())));

    for (uint i = 0; i < ServiceCount(); i++)
    {
        str.append(QString("  Service #%1 SID(0x%2) ")
                   .arg(i, 2, 10).arg(ServiceID(i), 0, 16));
        str.append(QString("eit_schd(%1) eit_pf(%2) encrypted(%3)\n")
                   .arg(HasEITSchedule(i) ? "t" : "f",
                        HasEITPresentFollowing(i) ? "t" : "f",
                        IsEncrypted(i) ? "t" : "f"));

        if (0 != ServiceDescriptorsLength(i))
        {
            str.append(QString("  Service descriptors length: %1\n")
                       .arg(ServiceDescriptorsLength(i)));
            std::vector<const unsigned char*> desc =
                MPEGDescriptor::Parse(ServiceDescriptors(i),
                                      ServiceDescriptorsLength(i));
            uint priv_dsid = 0;
            for (auto & j : desc)
            {
                priv_dsid = GetPrivateDataSpecifier(j, priv_dsid);
                str.append(QString("    %1\n")
                            .arg(MPEGDescriptor(j).toStringPD(priv_dsid)));
            }
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

    return nullptr;
}

ServiceRelocatedDescriptor *ServiceDescriptionTable::GetServiceRelocatedDescriptor(uint i) const
{
    desc_list_t parsed =
        MPEGDescriptor::Parse(ServiceDescriptors(i),
                              ServiceDescriptorsLength(i));

    const unsigned char *desc =
        MPEGDescriptor::FindExtension(parsed, DescriptorID::service_relocated);

    if (desc)
        return new ServiceRelocatedDescriptor(desc);

    return nullptr;
}

bool ServiceDescriptionTable::Mutate(void)
{
    if (VerifyCRC())
    {
        SetTableID((TableID() == TableID::SDTo) ? TableID::SDT : TableID::SDTo);
        SetCRC(CalcCRC());
        return true;
    }
    return false;
}

void BouquetAssociationTable::Parse(void) const
{
    m_tscPtr = pesdata() + 10 + BouquetDescriptorsLength();

    m_ptrs.clear();
    m_ptrs.push_back(m_tscPtr + 2);
    for (uint i=0; m_ptrs[i] + 6 <= m_ptrs[0] + TransportStreamDataLength(); i++)
        m_ptrs.push_back(m_ptrs[i] + 6 + TransportDescriptorsLength(i));
}

QString BouquetAssociationTable::toString(void) const
{
    QString str =
        QString("BAT: BouquetID(0x%1) transports(%2)\n")
        .arg(BouquetID(), 0, 16).arg(TransportStreamCount());

    str.append(QString("Section (%1) Last Section (%2) IsCurrent (%3)\n")
        .arg(Section()).arg(LastSection()).arg(static_cast<int>(IsCurrent())));

    if (0 != BouquetDescriptorsLength())
    {
        str.append(QString("Bouquet descriptors length: %1\n")
                   .arg(BouquetDescriptorsLength()));
        std::vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(BouquetDescriptors(),
                                  BouquetDescriptorsLength());
        uint priv_dsid = 0;
        for (auto & i : desc)
        {
            priv_dsid = GetPrivateDataSpecifier(i, priv_dsid);
            str.append(QString("  %1\n")
                       .arg(MPEGDescriptor(i).toStringPD(priv_dsid)));
        }
    }

    for (uint i = 0; i < TransportStreamCount(); i++)
    {
        str.append(QString("  Transport #%1 TSID(0x%2) ")
                   .arg(i, 2, 10).arg(TSID(i), 0, 16));
        str.append(QString("original_network_id(0x%2) desc_len(%3)\n")
                   .arg(OriginalNetworkID(i), 0, 16)
                   .arg(TransportDescriptorsLength(i)));

        if (0 != TransportDescriptorsLength(i))
        {
            str.append(QString("  Transport descriptors length: %1\n")
                       .arg(TransportDescriptorsLength(i)));
            std::vector<const unsigned char*> desc =
                MPEGDescriptor::Parse(TransportDescriptors(i),
                                      TransportDescriptorsLength(i));
            uint priv_dsid = 0;
            for (auto & j : desc)
            {
                priv_dsid = GetPrivateDataSpecifier(j, priv_dsid);
                str.append(QString("    %1\n")
                           .arg(MPEGDescriptor(j).toStringPD(priv_dsid)));
            }
        }
    }
    return str;
}


void DVBEventInformationTable::Parse(void) const
{
    m_ptrs.clear();
    m_ptrs.push_back(psipdata() + 6);
    uint i = 0;
    while ((m_ptrs[i] + 12) < (pesdata() + Length()))
    {
        m_ptrs.push_back(m_ptrs[i] + 12 + DescriptorsLength(i));
        i++;
    }
}

bool DVBEventInformationTable::IsEIT(uint table_id)
{
    bool is_eit = false;

    // Standard Now/Next Event Information Tables for this transport
    is_eit |= TableID::PF_EIT  == table_id;
    // Standard Now/Next Event Information Tables for other transport
    is_eit |= TableID::PF_EITo == table_id;
    // Standard Future Event Information Tables for this transport
    is_eit |= (TableID::SC_EITbeg  <= table_id &&
               TableID::SC_EITend  >= table_id);
    // Standard Future Event Information Tables for other transports
    is_eit |= (TableID::SC_EITbego <= table_id &&
               TableID::SC_EITendo >= table_id);
    // Dish Network Long Term Future Event Information for all transports
    is_eit |= (TableID::DN_EITbego <= table_id &&
               TableID::DN_EITendo >= table_id);

    return is_eit;
}

/** \fn dvbdate2qt(const unsigned char*)
 *  \return UTC time as QDateTime
 */
QDateTime dvbdate2qt(const unsigned char *buf)
{
    /* FIXME what's wrong with QDate::fromJulianDay? */
    uint mjd = (buf[0] << 8) | buf[1];
    if (mjd >= 40587)
    {
        // Modified Julian date as number of days since 17th November 1858.
        // 1st Jan 1970 was date 40587.
        uint secsSince1970 = (mjd - 40587)   * 86400;
        secsSince1970 += byteBCD2int(buf[2]) * 3600;
        secsSince1970 += byteBCD2int(buf[3]) * 60;
        secsSince1970 += byteBCD2int(buf[4]);
        return MythDate::fromSecsSinceEpoch(secsSince1970);
    }

    // Original function taken from dvbdate.c in linuxtv-apps code
    // Use the routine specified in ETSI EN 300 468 V1.4.1,
    // "Specification for Service Information in Digital Video Broadcasting"
    // to convert from Modified Julian Date to Year, Month, Day.

    const auto tmpA = (float)(1.0 / 365.25);
    const auto tmpB = (float)(1.0 / 30.6001);

    float mjdf = mjd;
    int year  = (int) truncf((mjdf - 15078.2F) * tmpA);
    int month = (int) truncf(
        (mjdf - 14956.1F - truncf(year * 365.25F)) * tmpB);
    int day   = (int) truncf(
        (mjdf - 14956.0F - truncf(year * 365.25F) - truncf(month * 30.6001F)));
    int i     = (month == 14 || month == 15) ? 1 : 0;

    QDate date(1900 + year + i, month - 1 - (i * 12), day);
    QTime time(byteBCD2int(buf[2]), byteBCD2int(buf[3]),
               byteBCD2int(buf[4]));

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    return {date, time, Qt::UTC};
#else
    return {date, time, QTimeZone(QTimeZone::UTC)};
#endif
}

/** \fn dvbdate2unix(const unsigned char*)
 *  \return UTC time as time_t
 */
time_t dvbdate2unix(const unsigned char *buf)
{
    // Modified Julian date as number of days since 17th November 1858.
    // The unix epoch, 1st Jan 1970, was day 40587.
    uint mjd = (buf[0] << 8) | buf[1];
    if (mjd < 40587)
        return 0; // we don't handle pre-unix dates..

    uint secsSince1970 = (mjd - 40587)   * 86400;
    secsSince1970 += byteBCD2int(buf[2]) * 3600;
    secsSince1970 += byteBCD2int(buf[3]) * 60;
    secsSince1970 += byteBCD2int(buf[4]);
    return secsSince1970;
}

/** \fn dvbdate2key(const unsigned char*)
 *  \return UTC time as 32 bit key
 */
uint32_t dvbdate2key(const unsigned char *buf)
{
    uint dt = (((uint)buf[0]) << 24) | (((uint)buf[1]) << 16); // 16 bits
    uint tm = ((byteBCD2int(buf[2]) * 3600) +
               (byteBCD2int(buf[3]) * 60) +
               (byteBCD2int(buf[4]))); // 17 bits
    return (dt | (tm>>1)) ^ ((tm & 1)<<31);
}
