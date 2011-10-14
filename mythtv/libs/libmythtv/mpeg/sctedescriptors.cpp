// -*- Mode: c++ -*-
/**
 *   SCTE Descriptors.
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Author: Daniel Kristjansson
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sctedescriptors.h"

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
