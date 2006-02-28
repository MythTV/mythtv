// -*- Mode: c++ -*-

#include "dishdescriptors.h"
#include "atsc_huffman.h"

QString DishEventNameDescriptor::Name(uint compression_type) const
{
    if (!HasName())
        return QString::null;

    return atsc_huffman2_to_string(
        _data + 3, DescriptorLength() - 1, compression_type);
}

const unsigned char *DishEventDescriptionDescriptor::DescriptionRaw(void) const
{
    if (DescriptorLength() <= 2)
        return NULL;

    bool offset = (_data[3] & 0xf8) == 0x80;
    return _data + ((offset) ? 4 : 3);
}

uint DishEventDescriptionDescriptor::DescriptionRawLength(void) const
{
    if (DescriptorLength() <= 2)
        return 0;

    bool offset = (_data[3] & 0xf8) == 0x80;
    return DescriptorLength() - ((offset) ? 2 : 1);
}

QString DishEventDescriptionDescriptor::Description(
    uint compression_type) const
{
    const unsigned char *raw = DescriptionRaw();
    uint len = DescriptionRawLength();

    if (raw && len)
        return atsc_huffman2_to_string(raw, len, compression_type);

    return QString::null;
}
