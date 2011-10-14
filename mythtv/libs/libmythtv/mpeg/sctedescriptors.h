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

#ifndef _SCTE_TABLES_H_
#define _SCTE_TABLES_H_

// Qt headers
#include <QString>

// MythTV
#include "mpegdescriptors.h"

/** This descriptor is used to identify streams with SpliceInformationTable
 *  data in them.
 *
 * \note While specified by SCTE 35 this descriptor is used worldwide.
 */
class CueIdentifierDescriptor : public MPEGDescriptor
{
  public:
    CueIdentifierDescriptor(const unsigned char *data, uint len) :
    MPEGDescriptor(data, len, DescriptorID::scte_cue_identifier, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x8A
    // descriptor_length        8   1.0       0x01
    // cue_stream_type          8   2.0
    enum // Table 6.3 Cue Stream Types
    {
        kLimited        = 0x0, ///< Only splice null, insert, and schedule
        kAllCommands    = 0x1, ///< Carries all commands.
        kSegmentation   = 0x2, ///< Carries time signal w/ segmentation desc.
        kTieredSplicing = 0x3, ///< outside scope of SCTE 35
        kTieredSegmentation = 0x4, ///< outside scope of SCTE 35
        // 0x05-0x7f are reserved for future use
        // 0x80-0xff user defined range
    };
    uint CueStreamType(void) const { return _data[2]; }
    QString CueStreamTypeString(void) const;
    QString toString(void) const;
};

#endif // _SCTE_TABLES_H_
