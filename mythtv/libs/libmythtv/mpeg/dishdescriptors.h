// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _DISH_DESCRIPTORS_H_
#define _DISH_DESCRIPTORS_H_

#include <cassert>

using namespace std;
#include <qstring.h>

#include "mythcontext.h"
#include "atscdescriptors.h"

class DishEventNameDescriptor : public MPEGDescriptor
{
  public:
    DishEventNameDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x91
        assert(DescriptorID::dish_event_name == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // unknown                  8   2.0
    // event_name            dlen-1 3.0
    bool HasName(void) const { return DescriptorLength() > 1; }
    QString Name(uint) const;
};

class DishEventDescriptionDescriptor : public MPEGDescriptor
{
  public:
    DishEventDescriptionDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x92
        assert(DescriptorID::dish_event_description == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // unknown                 8/16 2.0
    // event_name            dlen-2 3.0/4.0
    const unsigned char *DescriptionRaw(void) const;
    uint DescriptionRawLength(void) const;
    bool HasDescription(void) const { return DescriptionRawLength(); }
    QString Description(uint) const;
};

#endif // _DISH_DESCRIPTORS_H_
