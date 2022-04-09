// -*- Mode: c++ -*-
/**
 *   ANSI/SCTE 35 splice descriptor implementation
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef SPLICE_DESCRIPTOR_H
#define SPLICE_DESCRIPTOR_H

#include "splicedescriptors.h"
#include "stringutil.h"

desc_list_t SpliceDescriptor::Parse(
    const unsigned char *data, uint len)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        tmp.push_back(data+off);
        SpliceDescriptor desc(data+off, len-off);
        if (!desc.IsValid())
        {
            tmp.pop_back();
            break;
        }
        off += desc.size();
    }
    return tmp;
}

desc_list_t SpliceDescriptor::ParseAndExclude(
    const unsigned char *data, uint len, int excluded_descid)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        if ((data+off)[0] != excluded_descid)
            tmp.push_back(data+off);
        SpliceDescriptor desc(data+off, len-off);
        if (!desc.IsValid())
        {
            if ((data+off)[0] != excluded_descid)
                tmp.pop_back();
            break;
        }
        off += desc.size();
    }
    return tmp;
}

desc_list_t SpliceDescriptor::ParseOnlyInclude(
    const unsigned char *data, uint len, int excluded_descid)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        if ((data+off)[0] == excluded_descid)
            tmp.push_back(data+off);
        SpliceDescriptor desc(data+off, len-off);
        if (!desc.IsValid())
        {
            if ((data+off)[0] == excluded_descid)
                tmp.pop_back();
            break;
        }
        off += desc.size();
    }
    return tmp;
}

const unsigned char *SpliceDescriptor::Find(
    const desc_list_t &parsed, uint desc_tag)
{
    auto sametag = [desc_tag](const auto *item){ return item[0] == desc_tag; };
    auto it = std::find_if(parsed.cbegin(), parsed.cend(), sametag);
    return (it != parsed.cend()) ? *it : nullptr;
}

desc_list_t SpliceDescriptor::FindAll(const desc_list_t &parsed, uint desc_tag)
{
    desc_list_t tmp;
    auto sametag = [desc_tag](const auto *item) { return item[0] == desc_tag; };
    std::copy_if(parsed.cbegin(), parsed.cend(), std::back_inserter(tmp), sametag);
    return tmp;
}

QString SpliceDescriptor::DescriptorTagString(void) const
{
    switch (DescriptorTag())
    {
        case SpliceDescriptorID::avail:
            return QString("Avail");
        case SpliceDescriptorID::dtmf:
            return QString("DTMF");
        case SpliceDescriptorID::segmentation:
            return QString("Segmentation");
        default:
            return QString("Unknown(%1)").arg(DescriptorTag());
    }
}

QString SpliceDescriptor::toString(void) const
{
    QString str;

    if (!IsValid())
        return "Invalid Splice Descriptor";
    if (SpliceDescriptorID::avail == DescriptorTag())
    {
        auto desc = AvailDescriptor(m_data);
        if (desc.IsValid())
            str = desc.toString();
    }
    else if (SpliceDescriptorID::dtmf == DescriptorTag())
    {
        auto desc = DTMFDescriptor(m_data);
        if (desc.IsValid())
            str = desc.toString();
    }
    else if (SpliceDescriptorID::segmentation == DescriptorTag())
    {
        auto desc = SegmentationDescriptor(m_data);
        if (desc.IsValid())
            str = desc.toString();
    }
    else
    {
        str.append(QString("%1 Splice Descriptor (0x%2)")
                   .arg(DescriptorTagString())
                   .arg(int(DescriptorTag()), 0, 16));
        str.append(QString(" length(%1)").arg(int(DescriptorLength())));
        for (uint i=0; i<DescriptorLength(); i++)
            str.append(QString(" 0x%1").arg(int(m_data[i+2]), 0, 16));
    }

    return str.isEmpty() ? "Invalid Splice Descriptor" : str;
}

/// Returns XML representation of string the TS Reader XML format.
/// When possible matching http://www.tsreader.com/tsreader/text-export.html
QString SpliceDescriptor::toStringXML(uint level) const
{
    QString indent_0 = StringUtil::indentSpaces(level);
    QString indent_1 = StringUtil::indentSpaces(level+1);
    QString str;

    str += indent_0 + "<DESCRIPTOR namespace=\"splice\">\n";
    str += indent_1 + QString("<TAG>0x%1</TAG>\n")
        .arg(DescriptorTag(),2,16,QChar('0'));
    str += indent_1 + QString("<DESCRIPTION>%1</DESCRIPTION>\n")
        .arg(DescriptorTagString());

    str += indent_1 + "<DATA>";
    for (uint i = 0; i < DescriptorLength(); i++)
        str += QString("0x%1 ").arg(m_data[i+2],2,16,QChar('0'));
    str = str.trimmed();
    str += "</DATA>\n";

    str += indent_1 + "<DECODED>" + toString() + "</DECODED>";

    str += indent_0 + "</DESCRIPTOR>";

    return str;
}

bool DTMFDescriptor::IsParsible(const unsigned char *data, uint safe_bytes)
{
    if (safe_bytes < 8)
        return false;
    if (data[0] != SpliceDescriptorID::dtmf)
        return false;
    uint len = data[1];
    if (len + 2 > safe_bytes)
        return false;
    if (data[2] != 'C' || data[3] != 'U' ||
        data[4] != 'E' || data[5] != 'I')
        return false;
    return len == (6 + uint(data[7]>>5));
}

bool SegmentationDescriptor::Parse(void)
{
    _ptrs[0] = m_data + (IsProgramSegmentation() ? 12 : 13 + ComponentCount() * 6);
    _ptrs[1] = _ptrs[0] + (HasSegmentationDuration() ? 5 : 0);
    _ptrs[2] = _ptrs[1] + 2 + SegmentationUPIDLength();
    return true;
}

QString SegmentationDescriptor::toString() const
{
    // TODO
    return QString("Segmentation: id(%1)").arg(SegmentationEventIdString());
}

#endif // SPLICE_DESCRIPTOR_H
