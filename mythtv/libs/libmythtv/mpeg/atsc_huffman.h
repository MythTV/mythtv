#ifndef ATSC_HUFFMAN_H
#define ATSC_HUFFMAN_H

// C++ headers
#include <array>
#include <vector>

// Qt header
#include <QString>

// MythTV
#include "libmythtv/mythtvexp.h"

MTV_PUBLIC
QString atsc_huffman1_to_string(const unsigned char *compressed,
                                uint size, uint table);

MTV_PUBLIC
QString atsc_huffman2_to_string(const unsigned char *compressed,
                                uint length, uint table);


#endif //ATSC_HUFFMAN_H
