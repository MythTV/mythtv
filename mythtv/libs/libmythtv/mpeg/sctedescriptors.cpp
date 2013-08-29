// -*- Mode: c++ -*-
/**
 *   SCTE Descriptors.
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Authors: Daniel Kristjansson, Sergey Staroletov
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

#include "sctedescriptors.h"
#include "dvbdescriptors.h" // for dvb_decode_text()

QString SCTEComponentNameDescriptor::NameString(uint i) const
{
    return dvb_decode_text(&_data[loc(i) + 4], StringLength(i));
}

QString SCTEComponentNameDescriptor::toString(void) const
{
    QString ret =  QString("ComponentNameDescriptor: StringCount(%1)")
        .arg(StringCount());
    for (uint i = 0; i < StringCount(); ++i)
    {
        ret += QString(" Language(%1) Name(%2)")
            .arg(LanguageString(i)).arg(NameString(i));
    }
    return ret;
}

QString CueIdentifierDescriptor::CueStreamTypeString(void) const
{
    switch (CueStreamType())
    {
        case kLimited:
            return "Limited";
        case kAllCommands:
            return "AllCommands";
        case kSegmentation:
            return "Segmentation";
        case kTieredSplicing:
            return "TieredSplicing";
        case kTieredSegmentation:
            return "TieredSegmentation";
        default:
            if (CueStreamType() <= 0x7f)
                return QString("Reserved(0x%1)").arg(CueStreamType(),0,16);
            else
                return QString("User(0x%1)").arg(CueStreamType(),0,16);
    }
}

QString CueIdentifierDescriptor::toString(void) const
{
    return QString("Cue Identifier Descriptor (0x8A): StreamType(%1)")
        .arg(CueStreamTypeString());
}

QString RevisionDetectionDescriptor::toString(void) const
{
    return QString("Revision Detection Descriptor (0x93): "
                   "Version(%1) Section(%2) LastSection(%3)")
        .arg(TableVersionNumber())
        .arg(SectionNumber())
        .arg(LastSectionNumber());
}
