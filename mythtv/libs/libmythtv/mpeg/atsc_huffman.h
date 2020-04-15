#ifndef ATSC_HUFFMAN_H
#define ATSC_HUFFMAN_H

// POSIX header
#include <unistd.h>

// Qt header
#include <QString>

QString atsc_huffman1_to_string(const unsigned char *compressed,
                                uint size, uint table);

QString atsc_huffman2_to_string(const unsigned char *compressed,
                                uint length, uint table);


#endif //ATSC_HUFFMAN_H
